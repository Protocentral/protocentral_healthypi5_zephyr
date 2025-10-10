#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <stdio.h>

#include <zephyr/zbus/zbus.h>

#include "max30001.h"

#include "data_module.h"
#include "datalog_module.h"
#include "hw_module.h"
#include "cmd_module.h"
#include "hpi_common_types.h"
#include "display_module.h"

LOG_MODULE_REGISTER(data_module, LOG_LEVEL_DBG);

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
#include "display_module.h"
#endif

#include "fs_module.h"
#include "ble_module.h"

#include "arm_math.h"

#include "spo2_process.h"
#include "resp_process.h"
#include "datalog_module.h"
#include "hw_module.h"
#include "hpi_common_types.h"
#include "settings_module.h"

// ProtoCentral data formats
#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_ECG_BIOZ_DATA 0x03
#define CES_CMDIF_TYPE_PPG_DATA 0x04
#define CES_CMDIF_PKT_STOP 0x0B

#define SAMPLING_FREQ 104 // in Hz.
#define TEMP_CALC_BUFFER_LENGTH 125
#define RESP_CALC_BUFFER_LENGTH 125

#define SAMPLE_BUFF_WATERMARK 4

K_MSGQ_DEFINE(q_computed_val, sizeof(struct hpi_computed_data_t), 100, 1);

enum hpi5_data_format
{
    DATA_FMT_OPENVIEW,
    DATA_FMT_PLAIN_TEXT,
    DATA_FMT_HPI5_OV3,

} hpi5_data_format_t;

#define HPI_OV3_DATA_ECG_BIOZ_LEN 50
#define HPI_OV3_DATA_PPG_LEN 19
#define HPI_OV3_DATA_ECG_LEN 8
#define HPI_OV3_DATA_BIOZ_LEN 4
#define HPI_OV3_DATA_RED_LEN 8
#define HPI_OV3_DATA_IR_LEN 8

const uint8_t hpi_ov3_ecg_bioz_packet_header[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, HPI_OV3_DATA_ECG_BIOZ_LEN, 0, CES_CMDIF_TYPE_ECG_BIOZ_DATA};
const uint8_t hpi_ov3_ppg_packet_header[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, HPI_OV3_DATA_PPG_LEN, 0, CES_CMDIF_TYPE_PPG_DATA};
const uint8_t hpi_ov3_packet_footer[2] = {0, CES_CMDIF_PKT_STOP};

#define DATA_LEN 22
uint8_t DataPacket[DATA_LEN];
const char DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};

uint8_t hpi_ov3_ecg_bioz_data[HPI_OV3_DATA_ECG_BIOZ_LEN];
uint8_t hpi_ov3_ppg_data[HPI_OV3_DATA_PPG_LEN];

// NOTE: OP mode is now selected at runtime via m_op_mode; compile-time flag removed
/*static bool settings_send_usb_enabled = false;
static bool settings_send_ble_enabled = false;
static bool settings_plot_enabled = true;
#else
*/
static bool settings_send_usb_enabled = true;
static bool settings_send_ble_enabled = true;
static bool settings_plot_enabled = true;
// #endif

static bool settings_send_rpi_uart_enabled = false;

// struct hpi_sensor_data_t log_buffer[LOG_BUFFER_LENGTH];
struct hpi_sensor_logging_data_t log_buffer[LOG_BUFFER_LENGTH];

uint16_t current_session_ecg_counter = 0;
uint16_t serial_ecg_counter = 0;
uint16_t serial_bioz_counter = 0;
uint16_t serial_ppg_counter = 0;
uint16_t current_session_bioz_counter = 0;
uint16_t current_session_ppg_counter = 0;
uint16_t current_session_log_id = 0;
char session_id_str[15];

static volatile uint16_t m_resp_rate = 0;

int32_t ecg_serial_streaming[8];
int sample_buffer_count = 0;

int16_t ppg_serial_streaming[8];
int ppg_sample_buffer_count = 0;

int32_t resp_serial_streaming[4];
int resp_sample_buffer_count = 0;

// Externs
// extern struct k_msgq q_sample;

extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;

extern struct k_msgq q_ecg_bioz_sample;
extern struct k_msgq q_ppg_sample;

extern struct k_msgq q_plot_ecg_bioz;
extern struct k_msgq q_plot_ppg;

extern struct k_msgq q_hpi_data_sample;
extern struct k_msgq q_hpi_plot_all_sample;

extern bool settings_log_data_enabled; // true;
extern bool sd_card_present;
extern struct fs_mount_t *mp_sd;
extern struct hpi_log_session_header_t hpi_log_session_header;

extern struct k_sem sem_ble_connected;
extern struct k_sem sem_ble_disconnected;

#define NUM_TAPS 10  /* Number of taps in the FIR filter (length of the moving average window) */
#define BLOCK_SIZE 4 /* Number of samples processed per block */

float firCoeffs[NUM_TAPS] = {0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990};

arm_fir_instance_f32 sFIR;
float firState[NUM_TAPS + BLOCK_SIZE - 1];

int16_t spo2_serial;
int16_t hr_serial;
int16_t rr_serial;
int16_t temp_serial;

ZBUS_CHAN_DECLARE(hr_chan);
ZBUS_CHAN_DECLARE(spo2_chan);
ZBUS_CHAN_DECLARE(resp_rate_chan);

// New vars

static enum hpi_stream_modes m_stream_mode = HPI_STREAM_MODE_USB;
K_MUTEX_DEFINE(mutex_stream_mode);

// HR source selection (ECG vs PPG)
static enum hpi_hr_source m_hr_source = HR_SOURCE_ECG;
K_MUTEX_DEFINE(mutex_hr_source);

ZBUS_CHAN_DECLARE(resp_rate_chan);

void hpi_data_set_stream_mode(enum hpi_stream_modes mode)
{
    k_mutex_lock(&mutex_stream_mode, K_FOREVER);
    m_stream_mode = mode;
    k_mutex_unlock(&mutex_stream_mode);
}

void hpi_data_set_hr_source(enum hpi_hr_source source)
{
    k_mutex_lock(&mutex_hr_source, K_FOREVER);
    m_hr_source = source;
    k_mutex_unlock(&mutex_hr_source);
    
    LOG_INF("HR source changed to: %s", source == HR_SOURCE_ECG ? "ECG" : "PPG");
    
    // Save to filesystem
    settings_save_hr_source(source);
}

enum hpi_hr_source hpi_data_get_hr_source(void)
{
    enum hpi_hr_source source;
    k_mutex_lock(&mutex_hr_source, K_FOREVER);
    source = m_hr_source;
    k_mutex_unlock(&mutex_hr_source);
    return source;
}

void send_ppg_data_ov3_format()
{
    uint8_t pkt_ppg_pos_counter = 0;

    for (int i = 0; i < HPI_OV3_DATA_IR_LEN; i++)
    {
        hpi_ov3_ppg_data[pkt_ppg_pos_counter++] = (uint8_t)ppg_serial_streaming[i];
        hpi_ov3_ppg_data[pkt_ppg_pos_counter++] = (uint8_t)(ppg_serial_streaming[i] >> 8);
    }

    hpi_ov3_ppg_data[pkt_ppg_pos_counter++] = (uint8_t)spo2_serial;

    hpi_ov3_ppg_data[pkt_ppg_pos_counter++] = (uint8_t)(temp_serial >> 8);
    hpi_ov3_ppg_data[pkt_ppg_pos_counter++] = (uint8_t)(temp_serial >> 8);

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(hpi_ov3_ppg_packet_header, 5);
        send_usb_cdc(hpi_ov3_ppg_data, pkt_ppg_pos_counter);
        send_usb_cdc(hpi_ov3_packet_footer, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        // send_rpi_uart(DataPacketHeader, 5);
        // send_rpi_uart(DataPacket, DATA_LEN);
        // send_rpi_uart(DataPacketFooter, 2);
    }
}

void send_ecg_bioz_data_ov3_format(int32_t *ecg_data, int32_t ecg_sample_count, int32_t *bioz_samples, int32_t bioz_sample_count, uint8_t hr, uint8_t rr)
{
    uint8_t pkt_ecg_bioz_pos_counter = 0;

    for (int i = 0; i < ecg_sample_count; i++)
    {
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)ecg_data[i];
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(ecg_data[i] >> 8);
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(ecg_data[i] >> 16);
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(ecg_data[i] >> 24);
    }

    for (int i = 0; i < bioz_sample_count; i++)
    {
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)bioz_samples[i];
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(bioz_samples[i] >> 8);
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(bioz_samples[i] >> 16);
        hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = (uint8_t)(bioz_samples[i] >> 24);
    }

    hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = hr_serial;
    hpi_ov3_ecg_bioz_data[pkt_ecg_bioz_pos_counter++] = rr_serial;

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(hpi_ov3_ecg_bioz_packet_header, 5);
        send_usb_cdc(hpi_ov3_ecg_bioz_data, pkt_ecg_bioz_pos_counter);
        send_usb_cdc(hpi_ov3_packet_footer, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        // send_rpi_uart(DataPacketHeader, 5);
        // send_rpi_uart(DataPacket, DATA_LEN);
        // send_rpi_uart(DataPacketFooter, 2);
    }
}

void sendData(int32_t ecg_sample, int32_t bioz_sample, int32_t raw_red, int32_t raw_ir, int32_t temp, uint8_t hr,
              uint8_t rr, uint8_t spo2, bool _bioZSkipSample)
{

    DataPacket[0] = ecg_sample;
    DataPacket[1] = ecg_sample >> 8;
    DataPacket[2] = ecg_sample >> 16;
    DataPacket[3] = ecg_sample >> 24;

    DataPacket[4] = bioz_sample;
    DataPacket[5] = bioz_sample >> 8;
    DataPacket[6] = bioz_sample >> 16;
    DataPacket[7] = bioz_sample >> 24;

    if (_bioZSkipSample == false)
    {
        DataPacket[8] = 0x00;
    }
    else
    {
        DataPacket[8] = 0xFF;
    }

    DataPacket[9] = raw_red;
    DataPacket[10] = raw_red >> 8;
    DataPacket[11] = raw_red >> 16;
    DataPacket[12] = raw_red >> 24;

    DataPacket[13] = raw_ir;
    DataPacket[14] = raw_ir >> 8;
    DataPacket[15] = raw_ir >> 16;
    DataPacket[16] = raw_ir >> 24;

    DataPacket[17] = temp;
    DataPacket[18] = temp >> 8;

    DataPacket[19] = spo2;
    DataPacket[20] = hr;
    DataPacket[21] = rr;

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(DataPacketHeader, 5);
        send_usb_cdc(DataPacket, DATA_LEN);
        send_usb_cdc(DataPacketFooter, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        // send_rpi_uart(DataPacketHeader, 5);
        // send_rpi_uart(DataPacket, DATA_LEN);
        // send_rpi_uart(DataPacketFooter, 2);
    }
}

/*void sendData(int32_t ecg_sample, int32_t bioz_samples, int32_t raw_red, int32_t raw_ir, int32_t temp, uint8_t hr,
              uint8_t rr, uint8_t spo2, bool _bioZSkipSample)
{

    DataPacket[0] = ecg_sample;
    DataPacket[1] = ecg_sample >> 8;
    DataPacket[2] = ecg_sample >> 16;
    DataPacket[3] = ecg_sample >> 24;

    DataPacket[4] = bioz_samples;
    DataPacket[5] = bioz_samples >> 8;
    DataPacket[6] = bioz_samples >> 16;
    DataPacket[7] = bioz_samples >> 24;

    if (_bioZSkipSample == false)
    {
        DataPacket[8] = 0x00;
    }
    else
    {
        DataPacket[8] = 0xFF;
    }

    DataPacket[9] = raw_red;
    DataPacket[10] = raw_red >> 8;
    DataPacket[11] = raw_red >> 16;
    DataPacket[12] = raw_red >> 24;

    DataPacket[13] = raw_ir;
    DataPacket[14] = raw_ir >> 8;
    DataPacket[15] = raw_ir >> 16;
    DataPacket[16] = raw_ir >> 24;

    DataPacket[17] = temp;
    DataPacket[18] = temp >> 8;

    DataPacket[19] = spo2;
    DataPacket[20] = hr;
    DataPacket[21] = rr;

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(DataPacketHeader, 5);
        send_usb_cdc(DataPacket, DATA_LEN);
        send_usb_cdc(DataPacketFooter, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        // send_rpi_uart(DataPacketHeader, 5);
        // send_rpi_uart(DataPacket, DATA_LEN);
        // send_rpi_uart(DataPacketFooter, 2);
    }
}*/

/*void send_data_text(int32_t ecg_sample, int32_t bioz_samples, int32_t raw_red)
{
    char data[100];
    float f_ecg_sample = (float)ecg_sample / 1000;
    float f_bioz_sample = (float)bioz_samples / 1000;
    float f_raw_red = (float)raw_red / 1000;

    sprintf(data, "%.3f\t%.3f\t%.3f\r\n", (double)f_ecg_sample, (double)f_bioz_sample, (double)f_raw_red);

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(data, strlen(data));
    }

    if (settings_send_ble_enabled)
    {
        cmdif_send_ble_data(data, strlen(data));
    }
}

void send_data_text_1(int32_t in_sample)
{
    char data[100];
    float f_in_sample = (float)in_sample / 1000;

    sprintf(data, "%.3f\r\n", (double)f_in_sample);
    send_usb_cdc(data, strlen(data));
}*/

// Start a new session log
void flush_current_session_logs()
{
    // if data is pending in the log Buffer

    /*if ((current_session_ecg_counter > 0) && (write_to_file))
    {
        hpi_log_session_write_file(ECG_DATA);
    }

    if ((current_session_ppg_counter > 0) && (write_to_file))
    {
        hpi_log_session_write_file(PPG_DATA);
    }*/

    // current_session_log_id = 0;
    for (int i = 0; i < LOG_BUFFER_LENGTH; i++)
    {
        log_buffer[i].log_ecg_sample = 0;
        log_buffer[i].log_ppg_sample = 0;
        log_buffer[i].log_bioz_sample = 0;
    }

    current_session_ecg_counter = 0;
    current_session_ppg_counter = 0;
    current_session_bioz_counter = 0;
    hpi_log_session_header.session_start_time.day = 0;
    hpi_log_session_header.session_start_time.hour = 0;
    hpi_log_session_header.session_start_time.minute = 0;
    hpi_log_session_header.session_start_time.month = 0;
    hpi_log_session_header.session_start_time.second = 0;
    hpi_log_session_header.session_start_time.year = 0;

    hpi_log_session_header.session_id = 0;
    hpi_log_session_header.session_size = 0;
    hpi_log_session_header.file_no = 0;
}

void record_session_add_ppg_point(int16_t ppg_sample)
{
    if (current_session_ppg_counter < LOG_BUFFER_LENGTH)
    {
        log_buffer[current_session_ppg_counter++].log_ppg_sample = ppg_sample;
    }
    else
    {
        hpi_log_session_write_file(PPG_DATA);
        current_session_ppg_counter = 0;
        log_buffer[current_session_ppg_counter++].log_ppg_sample = ppg_sample;
    }
}

// Add a log point to the current session log
void record_session_add_ecg_point(int32_t *ecg_samples, uint8_t ecg_len, int32_t *bioz_samples, uint8_t bioz_len)
{
    if (current_session_ecg_counter < LOG_BUFFER_LENGTH)
    {
        // printk("Writing dataa to the file\n");
        for (int i = 0; i < ecg_len; i++)
        {
            // k_sem_give(&log_sem);
            log_buffer[current_session_ecg_counter++].log_ecg_sample = ecg_samples[i];
        }

        for (int i = 0; i < bioz_len; i++)
        {
            // k_sem_give(&log_sem);
            log_buffer[current_session_bioz_counter++].log_bioz_sample = bioz_samples[i];
        }
    }
    else
    {

        hpi_log_session_write_file(ECG_DATA);
        current_session_ecg_counter = 0;
        current_session_bioz_counter = 0;
        for (int i = 0; i < ecg_len; i++)
        {
            log_buffer[current_session_ecg_counter++].log_ecg_sample = ecg_samples[i];
        }

        for (int i = 0; i < bioz_len; i++)
        {
            log_buffer[current_session_bioz_counter++].log_bioz_sample = bioz_samples[i];
        }
    }
}

void ppg_buff_for_pkt(int16_t ppg_data_in)
{
    if (serial_ppg_counter < HPI_OV3_DATA_IR_LEN)
    {
        ppg_serial_streaming[serial_ppg_counter++] = ppg_data_in;
    }
    else
    {
        send_ppg_data_ov3_format();
        serial_ppg_counter = 0;
        memset(ppg_serial_streaming, 0, sizeof(ppg_serial_streaming));
        ppg_serial_streaming[serial_ppg_counter++] = ppg_data_in;
    }
}

void buffer_ecg_data_for_serial(int32_t *ecg_data_in, int ecg_len, int32_t *bioz_data_in, int bioz_len)
{
    if (serial_ecg_counter < HPI_OV3_DATA_ECG_LEN)
    {
        for (int i = 0; i < ecg_len; i++)
        {
            ecg_serial_streaming[serial_ecg_counter++] = ecg_data_in[i];
        }

        for (int i = 0; i < bioz_len; i++)
        {
            resp_serial_streaming[serial_bioz_counter++] = bioz_data_in[i];
        }
    }
    else
    {
        // send_data_ov3_format();
        serial_ecg_counter = 0;
        serial_bioz_counter = 0;
        // memset(ecg_serial_streaming,0,sizeof(ecg_serial_streaming));
        // memset(resp_serial_streaming, 0, sizeof(resp_serial_streaming));

        for (int i = 0; i < ecg_len; i++)
        {
            ecg_serial_streaming[serial_ecg_counter++] = ecg_data_in[i];
        }

        for (int i = 0; i < bioz_len; i++)
        {
            resp_serial_streaming[serial_bioz_counter++] = bioz_data_in[i];
        }
    }
}

void data_thread(void)
{
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;
    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    struct hpi_sensor_data_point_t hpi_sensor_data_point;

    // record_init_session_log();

    // Phase 1 Optimization: Use external buffers from spo2_process.c to avoid 4KB duplication
    // CRITICAL FIX: Changed to uint32_t to match PPG sensor data type (was causing algorithm errors!)
    // Original code had duplicate uint32_t irBuffer[500] and redBuffer[500] here (4KB wasted!)
    // Now we use the an_x and an_y buffers directly - NO CAST NEEDED (same type)
    // Memory savings: 4KB (2KB per buffer)
    extern uint32_t an_x[BUFFER_SIZE]; // Declared in spo2_process.c
    extern uint32_t an_y[BUFFER_SIZE];
    uint32_t *irBuffer = an_x;   // Direct pointer assignment, no cast needed
    uint32_t *redBuffer = an_y;  // Direct pointer assignment, no cast needed

    int32_t bufferLength;  // data length
    int32_t m_spo2;        // SPO2 value
    int8_t validSPO2;      // indicator to show if the SPO2 calculation is valid
    int32_t m_hr;          // heart rate value
    int8_t validHeartRate; // indicator to show if the heart rate calculation is valid

    // Phase 1: Add quality metrics
    spo2_quality_metrics_t quality_metrics = {0};

    // HR Smoothing Filter - Moving average to reduce fluctuations
    #define HR_FILTER_SIZE 5  // Average over 5 readings (10 seconds at 2-second updates)
    static int32_t hr_history[HR_FILTER_SIZE] = {0};
    static uint8_t hr_history_idx = 0;
    static uint8_t hr_history_count = 0;
    static int32_t hr_filtered = 0;

    // SpO2 Smoothing Filter - Moving average (SpO2 changes very slowly)
    #define SPO2_FILTER_SIZE 8  // Average over 8 readings (16 seconds) - SpO2 changes slowly
    static int32_t spo2_history[SPO2_FILTER_SIZE] = {0};
    static uint8_t spo2_history_idx = 0;
    static uint8_t spo2_history_count = 0;
    static int32_t spo2_filtered = 0;

    uint32_t spo2_time_count = 0;
    
    // Initialize buffers
    bufferLength = BUFFER_SIZE;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        irBuffer[i] = 0;
        redBuffer[i] = 0;
    }

    // Manual float formatting (no FP printf support)
    int buffer_seconds = BUFFER_SIZE / FreqS;
    int buffer_dec = ((BUFFER_SIZE % FreqS) * 10) / FreqS;
    LOG_INF("Data thread starting - SpO2 buffer size: %d samples (%d.%d seconds), Memory saved: 4KB",
            BUFFER_SIZE, buffer_seconds, buffer_dec);

    /* Initialize the FIR filter */
    arm_fir_init_f32(&sFIR, NUM_TAPS, firCoeffs, firState, BLOCK_SIZE);

// BLE buffer size: 8 samples = 62ms at 128 Hz (matches typical BLE connection intervals)
#define BLE_ECG_BUFFER_SIZE 8

#define RESP_FILT_BUFFER_SIZE 4

    int32_t ble_ecg_buffer[BLE_ECG_BUFFER_SIZE];
    int32_t ble_bioz_buffer[BLE_ECG_BUFFER_SIZE];

    uint8_t ecg_buffer_count = 0;
    uint8_t bioz_buffer_count = 0;

    int16_t resp_i16_buf[RESP_FILT_BUFFER_SIZE];
    int16_t resp_i16_filt_out[RESP_FILT_BUFFER_SIZE];
    int16_t resp_filt_buffer_count = 0;

    LOG_INF("Data Thread starting");

    // Load HR source setting from filesystem
    m_hr_source = settings_load_hr_source();
    LOG_INF("Initialized HR source: %s", m_hr_source == HR_SOURCE_ECG ? "ECG" : "PPG");

    for (;;)
    {
        // Process ALL available samples before sleeping (critical for performance)
        int samples_processed = 0;
        
        while (k_msgq_get(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT) == 0)
        {
            samples_processed++;
            
            // Buffer PPG data for SPO2/HR calculation
            if (spo2_time_count < FreqS)
            {
                // CRITICAL: AFE4400 outputs SIGNED int32_t (two's complement)
                // Maxim algorithm expects UNSIGNED uint32_t
                // Negative values indicate signal issues, but we clamp to 0 for algorithm stability
                int32_t ir_signed = hpi_sensor_data_point.ppg_sample_ir;
                int32_t red_signed = hpi_sensor_data_point.ppg_sample_red;
                
                // Debug logging disabled to prevent terminal flooding
                // if (spo2_time_count == 0 && samples_processed == 1) {
                //     LOG_DBG("PPG Raw: IR=%d, Red=%d", ir_signed, red_signed);
                // }
                
                // Convert signed to unsigned: negative values become 0
                irBuffer[BUFFER_SIZE - FreqS + spo2_time_count] = (ir_signed < 0) ? 0 : (uint32_t)ir_signed;
                redBuffer[BUFFER_SIZE - FreqS + spo2_time_count] = (red_signed < 0) ? 0 : (uint32_t)red_signed;
                spo2_time_count++;
            }
            else
            {
                // Buffer is full, calculate SPO2 and HR with quality metrics (Phase 1)
                spo2_time_count = 0;
                maxim_heart_rate_and_oxygen_saturation_with_quality(irBuffer, bufferLength, redBuffer, 
                    &m_spo2, &validSPO2, &m_hr, &validHeartRate, &quality_metrics);
                
                // Log quality metrics for debugging
                if (validSPO2 || validHeartRate) {
                    LOG_DBG("SpO2: %d%% (valid:%d), HR: %d bpm (valid:%d), PI: %d.%d%%, Conf: %d%%, Valid: %d",
                            m_spo2, validSPO2, m_hr, validHeartRate,
                            quality_metrics.perfusion_ir / 100, quality_metrics.perfusion_ir % 100,
                            quality_metrics.confidence, quality_metrics.valid);
                }
                
                // Reset SpO2 filter if perfusion is lost (probe removed/poor contact)
                // This allows the filter to quickly adapt to new readings when probe is reapplied
                static uint8_t low_perfusion_counter = 0;
                if (quality_metrics.perfusion_ir < 50) {  // PI < 0.5%
                    low_perfusion_counter++;
                    if (low_perfusion_counter >= 3) {  // 3 consecutive low readings (~1.5 seconds)
                        if (spo2_history_count > 0) {
                            LOG_INF("SpO2 filter reset due to low perfusion (PI=%d.%02d%%)", 
                                    quality_metrics.perfusion_ir / 100, quality_metrics.perfusion_ir % 100);
                            spo2_history_count = 0;
                            spo2_history_idx = 0;
                            spo2_filtered = 0;
                        }
                        low_perfusion_counter = 0;  // Reset counter
                    }
                } else {
                    low_perfusion_counter = 0;  // Reset counter on good perfusion
                }
                
                // Publish SpO2 with enhanced validation and smoothing filter
                if (validSPO2 && m_spo2 > 0 && m_spo2 <= 100 && 
                    quality_metrics.confidence >= 50) // Minimum confidence threshold
                {
                    // Outlier rejection: SpO2 changes slowly, reject >5% jumps
                    bool is_spo2_outlier = false;
                    if (spo2_history_count > 0 && spo2_filtered > 0) {
                        int32_t spo2_delta = (m_spo2 > spo2_filtered) ? (m_spo2 - spo2_filtered) : (spo2_filtered - m_spo2);
                        if (spo2_delta > 5) {  // SpO2 shouldn't jump >5% normally
                            is_spo2_outlier = true;
                            LOG_WRN("SpO2 outlier detected: %d%% (filtered: %d%%, delta: %d)", 
                                    m_spo2, spo2_filtered, spo2_delta);
                        }
                    }
                    
                    // Only add to history if not an extreme outlier
                    if (!is_spo2_outlier || spo2_history_count == 0) {
                        // Add to history buffer for moving average filter
                        spo2_history[spo2_history_idx] = m_spo2;
                        spo2_history_idx = (spo2_history_idx + 1) % SPO2_FILTER_SIZE;
                        if (spo2_history_count < SPO2_FILTER_SIZE) {
                            spo2_history_count++;
                        }
                        
                        // Calculate filtered SpO2 (moving average)
                        int32_t spo2_sum = 0;
                        for (int i = 0; i < spo2_history_count; i++) {
                            spo2_sum += spo2_history[i];
                        }
                        spo2_filtered = spo2_sum / spo2_history_count;
                    }
                    // If outlier, keep using previous filtered value
                    
                    // Use filtered value for serial and display
                    spo2_serial = spo2_filtered;
                    struct hpi_spo2_t spo2_chan_value = {.spo2 = spo2_filtered};
                    zbus_chan_pub(&spo2_chan, &spo2_chan_value, K_NO_WAIT);
                }
                
                // Publish HR with enhanced validation and smoothing filter
                // Enhanced quality gating: Reject readings with poor signal quality
                // Testing showed PI=0% readings give wildly inaccurate HR (36-101 bpm vs 67 bpm actual)
                // Primary filter is perfusion index (PI ≥ 1%) as it correlates strongly with accuracy
                if (validHeartRate && m_hr > 30 && m_hr < 220 &&
                    quality_metrics.perfusion_ir >= 100)  // Require PI ≥ 1.0% (primary quality gate)
                {
                    // Outlier rejection: If HR changes by more than 30 bpm from filtered value, 
                    // treat it as suspicious and reduce its weight
                    bool is_outlier = false;
                    if (hr_history_count > 0 && hr_filtered > 0) {
                        int32_t hr_delta = (m_hr > hr_filtered) ? (m_hr - hr_filtered) : (hr_filtered - m_hr);
                        if (hr_delta > 30) {
                            is_outlier = true;
                            LOG_WRN("HR outlier detected: %d bpm (filtered: %d bpm, delta: %d)", 
                                    m_hr, hr_filtered, hr_delta);
                        }
                    }
                    
                    // Only add to history if not an extreme outlier
                    if (!is_outlier || hr_history_count == 0) {
                        // Add to history buffer for moving average filter
                        hr_history[hr_history_idx] = m_hr;
                        hr_history_idx = (hr_history_idx + 1) % HR_FILTER_SIZE;
                        if (hr_history_count < HR_FILTER_SIZE) {
                            hr_history_count++;
                        }
                        
                        // Calculate filtered HR (moving average)
                        int32_t hr_sum = 0;
                        for (int i = 0; i < hr_history_count; i++) {
                            hr_sum += hr_history[i];
                        }
                        hr_filtered = hr_sum / hr_history_count;
                    }
                    // If outlier, keep using previous filtered value
                    
                    // Publish PPG HR only if PPG source is selected
                    if (hpi_data_get_hr_source() == HR_SOURCE_PPG) {
                        hr_serial = hr_filtered;
                        struct hpi_hr_t hr_chan_value = {.hr = hr_filtered};
                        zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
                    }
                }
                else if (validHeartRate && quality_metrics.perfusion_ir < 100) {
                    LOG_DBG("HR rejected: %d bpm (PI=%d.%02d%%, low perfusion)",
                            m_hr, quality_metrics.perfusion_ir / 100, quality_metrics.perfusion_ir % 100);
                }
                
                // Shift buffer for next calculation
                for (int i = FreqS; i < BUFFER_SIZE; i++)
                {
                    redBuffer[i - FreqS] = redBuffer[i];
                    irBuffer[i - FreqS] = irBuffer[i];
                }
            }
            
            if (resp_filt_buffer_count < RESP_FILT_BUFFER_SIZE)
            {
                resp_i16_buf[resp_filt_buffer_count++] = (int16_t)(hpi_sensor_data_point.bioz_sample >> 4);
            }
            else
            {
                // if (hpi_sensor_data_point.bioz_sample != 0)
                resp_process_sample(resp_i16_buf, resp_i16_filt_out);
                resp_algo_process(resp_i16_filt_out, &m_resp_rate);

                struct hpi_resp_rate_t resp_rate_chan_value = {
                    .resp_rate = m_resp_rate};
                // Use K_NO_WAIT to prevent blocking data thread and causing USB stalling
                zbus_chan_pub(&resp_rate_chan, &resp_rate_chan_value, K_NO_WAIT);

                resp_filt_buffer_count = 0;
            }

            // Publish ECG HR if ECG source is selected
            // ECG HR comes from MAX30001 R-R interval detection
            if (hpi_data_get_hr_source() == HR_SOURCE_ECG && 
                hpi_sensor_data_point.hr > 0 && hpi_sensor_data_point.hr < 255) 
            {
                hr_serial = hpi_sensor_data_point.hr;
                struct hpi_hr_t hr_chan_value = {.hr = hpi_sensor_data_point.hr};
                zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
            }

            /*for (int i = 0; i < 4; i++)
            {
                resp_i16_buf[i] = (int16_t)(ecg_bioz_sensor_sample.bioz_samples[i] >> 4);
            }*/

            if (m_stream_mode == HPI_STREAM_MODE_USB)
            {
                sendData(hpi_sensor_data_point.ecg_sample, hpi_sensor_data_point.bioz_sample, hpi_sensor_data_point.ppg_sample_red,
                         hpi_sensor_data_point.ppg_sample_ir, 0, 0, 0, 0, 0);
            }
            else if (m_stream_mode == HPI_STREAM_MODE_BLE)
            {
                if (ecg_buffer_count < BLE_ECG_BUFFER_SIZE)
                {
                    ble_ecg_buffer[ecg_buffer_count++] = hpi_sensor_data_point.ecg_sample;
                }
                else
                {
                    ble_ecg_notify(ble_ecg_buffer, BLE_ECG_BUFFER_SIZE);
                    ecg_buffer_count = 0;
                }

                if (bioz_buffer_count < BLE_ECG_BUFFER_SIZE)
                {
                    ble_bioz_buffer[bioz_buffer_count++] = hpi_sensor_data_point.bioz_sample;
                }
                else
                {
                    ble_bioz_notify(ble_bioz_buffer, BLE_ECG_BUFFER_SIZE);
                    bioz_buffer_count = 0;
                }

                // Move HR notify to ZBus
                // ble_hrs_notify(ecg_bioz_sensor_sample.hr);
                // ble_resp_rate_notify(globalRespirationRate);
            }
            else if (m_stream_mode == HPI_STREAM_MODE_RPI_UART)
            {
                // printk("RPI UART");
            }
            else if (m_stream_mode == HPI_STREAM_MODE_PLOT)
            {
                // Plot mode - no USB/BLE streaming, only display plotting
                // Plot data is handled below by automatic screen detection
            }

            // Automatic plot updates: Always send to plot queue when display enabled 
            // and on a waveform screen, regardless of streaming mode (USB/BLE/Plot)
            // This implements Phase 3 Option A: plots auto-pause/resume based on screen
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
            if (settings_plot_enabled && hpi_disp_is_plot_screen_active())
            {
                k_msgq_put(&q_hpi_plot_all_sample, &hpi_sensor_data_point, K_NO_WAIT);
            }
#endif
        }

        if (k_sem_take(&sem_ble_connected, K_NO_WAIT) == 0)
        {
            hpi_data_set_stream_mode(HPI_STREAM_MODE_BLE);
        }

        if (k_sem_take(&sem_ble_disconnected, K_NO_WAIT) == 0)
        {
            hpi_data_set_stream_mode(HPI_STREAM_MODE_USB);
        }

        // Only sleep if no samples were processed (queue was empty)
        // This ensures we process samples as fast as they arrive
        if (samples_processed == 0) {
            k_sleep(K_USEC(500));  // 500us sleep when idle (was 1ms)
        } else {
            k_yield();  // Just yield to other threads, don't sleep
        }
    }
}

#define DATA_THREAD_STACKSIZE 6144
#define DATA_THREAD_PRIORITY 6

K_THREAD_DEFINE(data_thread_id, DATA_THREAD_STACKSIZE, data_thread, NULL, NULL, NULL, DATA_THREAD_PRIORITY, 0, 0);