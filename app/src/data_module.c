#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <stdio.h>

#include "max30001.h"

#include "data_module.h"
#include "hw_module.h"
#include "cmd_module.h"
#include "sampling_module.h"

LOG_MODULE_REGISTER(data_module, CONFIG_SENSOR_LOG_LEVEL);

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
#include "display_module.h"`
#endif

#include "fs_module.h"
#include "ble_module.h"

#include "arm_math.h"

#include "spo2_process.h"
#include "resp_process.h"
#include "datalog_module.h"

// ProtoCentral data formats
#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 22

#define SAMPLING_FREQ 104 // in Hz.

#define LOG_SAMPLE_RATE_SPS 125
#define LOG_WRITE_INTERVAL 10      // Write to file every 10 seconds
#define LOG_BUFFER_LENGTH 1250 + 1 // 125Hz * 10 seconds

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

char DataPacket[DATA_LEN];

const uint8_t DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};
const uint8_t DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};

#define HPI_OV3_DATA_LEN 62
#define HPI_OV3_DATA_ECG_LEN 8
#define HPI_OV3_DATA_BIOZ_LEN 4
#define HPI_OV3_DATA_RED_LEN 8
#define HPI_OV3_DATA_IR_LEN 8

const uint8_t hpi_ov3_packet_header[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};
const uint8_t hpi_ov3_packet_footer[2] = {0, CES_CMDIF_PKT_STOP};

uint8_t hpi_ov3_data[HPI_OV3_DATA_LEN];

static bool settings_send_usb_enabled = true;
static bool settings_send_ble_enabled = true;
static bool settings_send_rpi_uart_enabled = false;
static bool settings_plot_enabled = true;

extern bool settings_log_data_enabled; // true;
extern struct fs_mount_t *mp_sd;
extern struct hpi_log_session_header_t hpi_log_session_header;
static int settings_data_format = DATA_FMT_OPENVIEW; // DATA_FMT_PLAIN_TEXT;

// struct hpi_sensor_data_t log_buffer[LOG_BUFFER_LENGTH];
struct hpi_sensor_logging_data_t log_buffer[LOG_BUFFER_LENGTH];

uint16_t current_session_ecg_counter = 0;
uint16_t current_session_bioz_counter = 0;
uint16_t current_session_log_id = 0;
char session_id_str[15];

volatile uint8_t globalRespirationRate = 0;
int16_t resWaveBuff, respFilterout;
long timeElapsed = 0;

int32_t ecg_sample_buffer[64];
int sample_buffer_count = 0;

int16_t ppg_sample_buffer[64];
int ppg_sample_buffer_count = 0;

int32_t resp_sample_buffer[64];
int resp_sample_buffer_count = 0;

// Externs
// extern struct k_msgq q_sample;

extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;

extern struct k_msgq q_ecg_bioz_sample;
extern struct k_msgq q_ppg_sample;

extern struct k_msgq q_plot_ecg_bioz;
extern struct k_msgq q_plot_ppg;

#define NUM_TAPS 10  /* Number of taps in the FIR filter (length of the moving average window) */
#define BLOCK_SIZE 4 /* Number of samples processed per block */

float firCoeffs[NUM_TAPS] = {0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990, 0.990};

arm_fir_instance_f32 sFIR;
float firState[NUM_TAPS + BLOCK_SIZE - 1];

void send_data_ov3_format(int16_t ecg_samples[8], int16_t bioz_samples[4], int16_t raw_ir[8], int16_t raw_red[8], int16_t temp, uint8_t hr, uint8_t rr, uint8_t spo2, bool _bioZSkipSample)
{
    for (int i = 0; i < HPI_OV3_DATA_ECG_LEN; i++)
    {
        hpi_ov3_data[i * 2] = (uint8_t)ecg_samples[i];
        hpi_ov3_data[i * 2 + 1] = (uint8_t)(ecg_samples[i] >> 8);
    }

    for (int i = 0; i < HPI_OV3_DATA_BIOZ_LEN; i++)
    {
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + i * 2] = (uint8_t)bioz_samples[i];
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + i * 2 + 1] = (uint8_t)(bioz_samples[i] >> 8);
    }

    hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + HPI_OV3_DATA_BIOZ_LEN * 2] = (uint8_t)_bioZSkipSample;

    for (int i = 0; i < HPI_OV3_DATA_RED_LEN; i++)
    {
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + HPI_OV3_DATA_BIOZ_LEN * 2 + 1 + i * 2] = (uint8_t)raw_red[i];
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + HPI_OV3_DATA_BIOZ_LEN * 2 + 1 + i * 2 + 1] = (uint8_t)(raw_red[i] >> 8);
    }

    for (int i = 0; i < HPI_OV3_DATA_IR_LEN; i++)
    {
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + HPI_OV3_DATA_BIOZ_LEN * 2 + 1 + HPI_OV3_DATA_RED_LEN * 2 + i * 2] = (uint8_t)raw_ir[i];
        hpi_ov3_data[HPI_OV3_DATA_ECG_LEN * 2 + HPI_OV3_DATA_BIOZ_LEN * 2 + 1 + HPI_OV3_DATA_RED_LEN * 2 + i * 2 + 1] = (uint8_t)(raw_ir[i] >> 8);
    }

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(hpi_ov3_packet_header, 5);
        send_usb_cdc(hpi_ov3_data, HPI_OV3_DATA_LEN);
        send_usb_cdc(hpi_ov3_packet_footer, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        // send_rpi_uart(DataPacketHeader, 5);
        // send_rpi_uart(DataPacket, DATA_LEN);
        // send_rpi_uart(DataPacketFooter, 2);
    }
}

void sendData(int32_t ecg_sample, int32_t bioz_samples, int32_t raw_red, int32_t raw_ir, int32_t temp, uint8_t hr,
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
}

void send_data_text(int32_t ecg_sample, int32_t bioz_samples, int32_t raw_red)
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
}


// Start a new session log
void flush_current_session_logs(bool write_to_file)
{
    // if data is pending in the log Buffer

    if ((current_session_ecg_counter > 0) && (write_to_file))
    {
        //printk("Log Buffer pending at %d \n", k_uptime_get_32());
        hpi_log_session_write_file(current_session_ecg_counter, log_buffer);
    }

    //current_session_log_id = 0;
    for (int i = 0; i < LOG_BUFFER_LENGTH; i++)
    {
        log_buffer[i].log_ecg_sample = 0;
    }

    current_session_ecg_counter = 0;
    hpi_log_session_header.session_id = 0;
    hpi_log_session_header.session_start_time.day = 0;
    hpi_log_session_header.session_start_time.hour = 0;
    hpi_log_session_header.session_start_time.minute = 0;
    hpi_log_session_header.session_start_time.month = 0;
    hpi_log_session_header.session_start_time.second = 0;
    hpi_log_session_header.session_start_time.year = 0;

    hpi_log_session_header.session_id = 0;
    hpi_log_session_header.session_size = 0;
}

// Add a log point to the current session log
void record_session_add_point(int32_t *ecg_samples,uint8_t ecg_len)
{
    if (current_session_ecg_counter < LOG_BUFFER_LENGTH)
    {
        for (int i = 0; i < ecg_len; i++)
        {
            
            log_buffer[current_session_ecg_counter].log_ecg_sample = ecg_samples[i];

            current_session_ecg_counter++;
        }
    }
    else
    {
        //printk("Log Buffer Full at %d \n", k_uptime_get_32());
        struct fs_statvfs sbuf;

        int rc = fs_statvfs(mp_sd->mnt_point, &sbuf);
        if (rc < 0)
        {
            printk("FAILED to return stats");
        }

        //printk("free: %lu, available : %f\n", sbuf.f_bfree, (0.25 * sbuf.f_blocks));

        if (sbuf.f_bfree < (0.25 * sbuf.f_blocks))
        {
            settings_log_data_enabled = false;
        }
        else
        {
            hpi_log_session_write_file(current_session_ecg_counter, log_buffer);
            current_session_ecg_counter = 0;
            for (int i = 0; i < ecg_len; i++)
            {

                log_buffer[current_session_ecg_counter].log_ecg_sample = ecg_samples[i];
                current_session_ecg_counter++;
            }
        }
    }
}

void data_thread(void)
{
    printk("Data Thread starting\n");

    struct hpi_sensor_data_t sensor_sample;
    struct hpi_computed_data_t computed_data;

    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;
    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    //record_init_session_log();

    int m_temp_sample_counter = 0;

    uint32_t irBuffer[500];  // infrared LED sensor data
    uint32_t redBuffer[500]; // red LED sensor data

    float ecg_filt_in[8];
    float ecg_filt_out[8];

    int32_t bufferLength;  // data length
    int32_t m_spo2;        // SPO2 value
    int8_t validSPO2;      // indicator to show if the SPO2 calculation is valid
    int32_t m_hr;          // heart rate value
    int8_t validHeartRate; // indicator to show if the heart rate calculation is valid

    uint32_t ppg_buffer_count = 0;

    uint32_t spo2_time_count = 0;

    /* Initialize the FIR filter */
    arm_fir_init_f32(&sFIR, NUM_TAPS, firCoeffs, firState, BLOCK_SIZE);

    int32_t init_buffer_count = 0;

    bufferLength = BUFFER_SIZE;

    // Initialize red and IR buffers with first 100 samples
    while (init_buffer_count < BUFFER_SIZE)
    {
        if (k_msgq_get(&q_ppg_sample, &ppg_sensor_sample, K_FOREVER) == 0)
        {
            //printk("PPG %d\n",ppg_sensor_sample.ppg_num_samples);
            for (int i = 0; i < ppg_sensor_sample.ppg_num_samples; i++)
            {
                irBuffer[init_buffer_count] = ppg_sensor_sample.ppg_ir_samples[i];
                redBuffer[init_buffer_count] = ppg_sensor_sample.ppg_red_samples[i];
                init_buffer_count++;
            }
        }
    }

    for (;;)
    {
        k_sleep(K_MSEC(1));

        // Get Sample from ECG / BioZ sampling queue
        if (k_msgq_get(&q_ecg_bioz_sample, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {
            // printk("S: %d", ecg_bioz_sensor_sample.ecg_num_samples);

            /*for (int i = 0; i < ecg_bioz_sensor_sample.ecg_num_samples; i++)
            {
                ecg_filt_in[i] = (float)(ecg_bioz_sensor_sample.bioz_samples[i]/1000.0000 );
            }

            arm_fir_f32(&sFIR, ecg_filt_in, ecg_filt_out, BLOCK_SIZE);

            for (int i = 0; i < ecg_bioz_sensor_sample.ecg_num_samples; i++)
            {
                ecg_bioz_sensor_sample.bioz_samples[i] = (int32_t)(ecg_filt_out[i] * 1000.0000);
            }*/

            int16_t resp_i16_buf[4];
            int16_t resp_i16_filt_out[4];

            for (int i = 0; i < ecg_bioz_sensor_sample.bioz_num_samples; i++)
            {
                //printk("BIOZ %d\n",ecg_bioz_sensor_sample.bioz_num_samples);
                resp_i16_buf[i] = (int16_t)(ecg_bioz_sensor_sample.bioz_samples[i] >> 4);
            }

            resp_process_sample(resp_i16_buf, resp_i16_filt_out);
            resp_algo_process(resp_i16_filt_out, &globalRespirationRate);

//#ifdef CONFIG_BT
            if (settings_send_ble_enabled)
            {
                ble_resp_rate_notify(globalRespirationRate);
                ble_ecg_notify(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples);
                ble_bioz_notify(ecg_bioz_sensor_sample.bioz_samples, ecg_bioz_sensor_sample.bioz_num_samples);
                ble_hrs_notify(ecg_bioz_sensor_sample.hr);
            }

            if (settings_log_data_enabled)
            {
                record_session_add_point(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples);
            }
//#endif

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
            if (settings_plot_enabled)
            {
                k_msgq_put(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT);
            }
#endif
        }

        /* Get Sample from PPG sampling queue */
        if (k_msgq_get(&q_ppg_sample, &ppg_sensor_sample, K_NO_WAIT) == 0)
        {

//#ifdef CONFIG_BT
            if (settings_send_ble_enabled)
            {
                ble_ppg_notify(ppg_sensor_sample.ppg_red_samples, PPG_POINTS_PER_SAMPLE);
            }
//#endif

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
            if (settings_plot_enabled)
            {
                k_msgq_put(&q_plot_ppg, &ppg_sensor_sample, K_NO_WAIT);
            }
#endif
            if (spo2_time_count < FreqS)
            {
                irBuffer[BUFFER_SIZE - FreqS + spo2_time_count] = ppg_sensor_sample.ppg_ir_samples[0];
                redBuffer[BUFFER_SIZE - FreqS + spo2_time_count] = ppg_sensor_sample.ppg_red_samples[0];

                spo2_time_count++;
            }
            else
            {
                spo2_time_count = 0;
                maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &m_spo2, &validSPO2, &m_hr, &validHeartRate);
                LOG_DBG("SPO2: %d, Valid: %d, HR: %d, Valid: %d\n", m_spo2, validSPO2, m_hr, validHeartRate);
                if (validSPO2)
                {
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
                    hpi_scr_home_update_spo2(m_spo2);
#endif
//#ifdef CONFIG_BT
                    ble_spo2_notify(m_spo2);
//#endif
                }

                if (validHeartRate)
                {
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
                    hpi_scr_home_update_pr(m_hr);
#endif
                }

                for (int i = FreqS; i < BUFFER_SIZE; i++)
                {
                    redBuffer[i - FreqS] = redBuffer[i];
                    irBuffer[i - FreqS] = irBuffer[i];
                }
            }
        }

        /***** Send to USB if enabled *****/
        if (settings_send_usb_enabled)
        {
            if (settings_data_format == DATA_FMT_OPENVIEW)
            {
                //sendData(sensor_sample.ecg_sample, sensor_sample.bioz_samples, sensor_sample.raw_red, sensor_sample.raw_ir,
                //         (double)(sensor_sample.temp / 10.00), computed_data.hr, computed_data.rr, computed_data.spo2, sensor_sample._bioZSkipSample);
            }
            else if (settings_data_format == DATA_FMT_PLAIN_TEXT)
            {
                send_data_text(sensor_sample.ecg_sample, sensor_sample.bioz_samples, sensor_sample.raw_red);
            }
            else if (settings_data_format == DATA_FMT_HPI5_OV3)
            {
                send_data_ov3_format(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.bioz_samples, ecg_bioz_sensor_sample.ecg_samples,
                                     ecg_bioz_sensor_sample.ecg_samples, sensor_sample.temp, computed_data.hr, computed_data.rr, computed_data.spo2, sensor_sample._bioZSkipSample);
            }
            // #endif

            /****** Send to log queue if enabled ******/
        }
    }
}

#define DATA_THREAD_STACKSIZE 8192
#define DATA_THREAD_PRIORITY 7

K_THREAD_DEFINE(data_thread_id, DATA_THREAD_STACKSIZE, data_thread, NULL, NULL, NULL, DATA_THREAD_PRIORITY, 0, 1000);
