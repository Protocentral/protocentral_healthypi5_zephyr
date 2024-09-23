#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "max30001.h"

#include "data_module.h"
#include "hw_module.h"
#include "cmd_module.h"
#include "sampling_module.h"
#include "algos.h"

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
#include "display_module.h"
#endif

#include "fs_module.h"
#include "ble_module.h"

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
const uint8_t DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};
const uint8_t DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};

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

static bool settings_log_data_enabled = false;       // true;
static int settings_data_format = DATA_FMT_OPENVIEW; // DATA_FMT_PLAIN_TEXT;

struct hpi_sensor_data_t log_buffer[LOG_BUFFER_LENGTH];

uint16_t current_session_log_counter = 0;
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
extern struct k_msgq q_sample;
extern struct k_msgq q_plot_ecg_bioz;
extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;

extern struct k_msgq q_ecg_bioz_sample;

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

void send_data_text(int32_t ecg_sample, int32_t bioz_sample, int32_t raw_red)
{
    char data[100];
    float f_ecg_sample = (float)ecg_sample / 1000;
    float f_bioz_sample = (float)bioz_sample / 1000;
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
void record_init_session_log()
{
    // current_session_log.session_id = 0;
    current_session_log_id = 0;
    // strcpy(current_session_log.session_header, "Session Header");
    for (int i = 0; i < LOG_BUFFER_LENGTH; i++)
    {
        // log_buffer[i].ecg_sample = 0;
        // log_buffer[i].bioz_sample = 0;
        log_buffer[i].raw_red = 0;
        log_buffer[i].raw_ir = 0;
        log_buffer[i].temp = 0;
        log_buffer[i]._bioZSkipSample = false;
    }

    current_session_log_counter = 0;
    // current_session_log_id = (uint16_t)sys_rand32_get(); // Create random session ID

    // printk("Init Session ID %s \n", log_get_current_session_id_str());
}

char *log_get_current_session_id_str(void)
{
    sprintf(session_id_str, "%d", current_session_log_id);
    return session_id_str;
}

// Add a log point to the current session log
void record_session_add_point(int32_t ecg_val, int32_t bioz_val, int32_t raw_ir_val, int32_t raw_red_val, int16_t temp)
{
    if ((current_session_log_counter - 1) < LOG_BUFFER_LENGTH)
    {
        current_session_log_counter++;
    }
    else
    {
        printk("Log Buffer Full at %d \n", k_uptime_get_32());
        record_write_to_file(current_session_log_id, current_session_log_counter, log_buffer);
        current_session_log_counter = 0;
    }

    // log_buffer[current_session_log_counter].ecg_sample = time;
    // log_buffer[current_session_log_counter].ecg_sample = ecg_val;
    // log_buffer[current_session_log_counter].bioz_sample = bioz_val;
    log_buffer[current_session_log_counter].raw_ir = raw_ir_val;
    log_buffer[current_session_log_counter].raw_red = raw_red_val;
    log_buffer[current_session_log_counter].temp = temp;
}

void data_thread(void)
{
    printk("Data Thread starting\n");

    struct hpi_sensor_data_t sensor_sample;
    struct hpi_computed_data_t computed_data;

    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;

    record_init_session_log();

    int m_temp_sample_counter = 0;

    int32_t n_spo2;       // SPO2 value
    int32_t n_heart_rate; // heart rate value

    uint16_t aun_ir_buffer[100];  // infrared LED sensor data
    uint16_t aun_red_buffer[100]; // red LED sensor data
    uint16_t power_ir_buffer[32];
    static uint16_t power_ir_average = 0;

    int8_t ch_spo2_valid; // indicator to show if the SPO2 calculation is valid
    int8_t ch_hr_valid;   // indicator to show if the heart rate calculation is valid

    int dec = 0;
    volatile int8_t n_buffer_count;        // data length
    volatile int8_t power_ir_buffer_count; // data length

    bool power_up_data_ready = false;
    for (;;)
    {
        k_sleep(K_MSEC(10));

        if (k_msgq_get(&q_ecg_bioz_sample, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {

            if (settings_send_ble_enabled)
            {
                //ble_ecg_notify(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples);
                //ble_bioz_notify(ecg_bioz_sensor_sample.bioz_sample, ecg_bioz_sensor_sample.bioz_num_samples);
                // b_notify(ecg_bioz_sensor_sample.bioz_sample);

                /*resp_sample_buffer[resp_sample_buffer_count++] = ecg_bioz_sensor_sample.bioz_sample;
                if (resp_sample_buffer_count >= SAMPLE_BUFF_WATERMARK)
                {
                    ble_bioz_notify(resp_sample_buffer, resp_sample_buffer_count);
                    resp_sample_buffer_count = 0;

                }*/
            }

            /***** Send to USB if enabled *****/
            if (settings_send_usb_enabled)
            {
                /*if (settings_data_format == DATA_FMT_OPENVIEW)
                {
                    sendData(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red, sensor_sample.raw_ir,
                             (double)(sensor_sample.temp / 10.00), computed_data.hr, computed_data.rr, computed_data.spo2, sensor_sample._bioZSkipSample);
                }
                else if (settings_data_format == DATA_FMT_PLAIN_TEXT)
                {
                    send_data_text(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red);
                }
                else if (settings_data_format == DATA_FMT_HPI5_OV3)
                {
                    send_data_ov3_format(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.bioz_sample, ecg_bioz_sensor_sample.ecg_samples,
                                         ecg_bioz_sensor_sample.ecg_samples, sensor_sample.temp, computed_data.hr, computed_data.rr, computed_data.spo2, sensor_sample._bioZSkipSample);
                }*/
            }

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
            if (settings_plot_enabled)
            {
                k_msgq_put(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT);
            }
#endif
        }

        /*k_msgq_get(&q_sample, &sensor_sample, K_FOREVER);
        // Data is now available in sensor_sample, Send, store or process data here

        m_temp_sample_counter++;

        if (m_temp_sample_counter > TEMP_CALC_BUFFER_LENGTH)
        {
            m_temp_sample_counter = 0;
#ifdef CONFIG_BT
            ble_temp_notify((int16_t)sensor_sample.temp);
#endif
        }

        if (dec == 20)
        {
            aun_ir_buffer[n_buffer_count] = (uint16_t)sensor_sample.raw_ir;   //((afe44xx_raw_data->IR_data) >> 4);
            aun_red_buffer[n_buffer_count] = (uint16_t)sensor_sample.raw_red; //((afe44xx_raw_data->RED_data) >> 4);

            n_buffer_count++;
            dec = 0;
        }

        if (power_ir_buffer_count < 32)
        {
            power_ir_buffer[power_ir_buffer_count] = (uint16_t)sensor_sample.raw_ir;
            power_ir_buffer_count++;
        }
        else
        {
            if (power_up_data_ready == false)
            {
                for (int i = 0; i < 32; i++)
                {
                    power_ir_average += power_ir_buffer[i];
                }
                power_ir_average = power_ir_average / 32;
                power_up_data_ready = true;
            }
        }

        dec++;
        */

        /*resWaveBuff = (int16_t)(sensor_sample.bioz_sample >> 4);
        respFilterout = Resp_ProcessCurrSample(resWaveBuff);
        RESP_Algorithm_Interface(respFilterout, &globalRespirationRate);
        computed_data.rr = (uint32_t)globalRespirationRate;

        if (n_buffer_count > 99)
        {
            // n_buffer_count = 75;
            n_buffer_count = 0;

            // printf("Calculating SPO2...\n");
            hpi_estimate_spo2(aun_ir_buffer, 100, aun_red_buffer, power_ir_average, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
            // printk("SPO2: %d, SPO2 Valid: %d, HR: %d\n", n_spo2, ch_spo2_valid, n_heart_rate);

            computed_data.hr = sensor_sample.hr; // HR from MAX30001 RtoR detection algorithm
            // computed_data.rr = -999;
            computed_data.spo2_valid = ch_spo2_valid;
            computed_data.spo2 = n_spo2;
            computed_data.hr_valid = ch_hr_valid;

#ifdef CONFIG_BT
            ble_spo2_notify(n_spo2);
            ble_hrs_notify(computed_data.hr);
            ble_resp_rate_notify(computed_data.rr);

#endif

            k_msgq_put(&q_computed_val, &computed_data, K_NO_WAIT);
        }
        *

        /***** Send to USB if enabled *****/
        /*if (settings_send_usb_enabled)
        {
            if (settings_data_format == DATA_FMT_OPENVIEW)
            {
                sendData(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red, sensor_sample.raw_ir,
                         (double)(sensor_sample.temp / 10.00), computed_data.hr, computed_data.rr, computed_data.spo2, sensor_sample._bioZSkipSample);
            }
            else if (settings_data_format == DATA_FMT_PLAIN_TEXT)
            {
                send_data_text(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red);
            }
        }*/

        /*#ifdef CONFIG_BT
                if (settings_send_ble_enabled)
                {
                    ecg_sample_buffer[sample_buffer_count++] = sensor_sample.ecg_sample;
                    if (sample_buffer_count >= SAMPLE_BUFF_WATERMARK)
                    {
                        ble_ecg_notify(ecg_sample_buffer, sample_buffer_count);
                        sample_buffer_count = 0;
                    }

                    ppg_sample_buffer[ppg_sample_buffer_count++] = (int16_t)((sensor_sample.raw_ir / 1000));
                    if (ppg_sample_buffer_count >= SAMPLE_BUFF_WATERMARK)
                    {
                        ble_ppg_notify(ppg_sample_buffer, ppg_sample_buffer_count);
                        ppg_sample_buffer_count = 0;
                    }

                    resp_sample_buffer[resp_sample_buffer_count++] = sensor_sample.bioz_sample;
                    if (resp_sample_buffer_count >= SAMPLE_BUFF_WATERMARK)
                    {
                        ble_bioz_notify(resp_sample_buffer, resp_sample_buffer_count);
                        resp_sample_buffer_count = 0;
                    }
                }
        #endif
        */
        /****** Send to log queue if enabled ******/

        if (settings_log_data_enabled)
        {
            // log_data(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red, sensor_sample.raw_ir,
            //          sensor_sample.temp, 0, 0, 0, sensor_sample._bioZSkipSample);
            // record_session_add_point(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red,
            //                         sensor_sample.raw_ir, sensor_sample.temp);
        }
    }
}

#define DATA_THREAD_STACKSIZE 4096
#define DATA_THREAD_PRIORITY 7

K_THREAD_DEFINE(data_thread_id, DATA_THREAD_STACKSIZE, data_thread, NULL, NULL, NULL, DATA_THREAD_PRIORITY, 0, 1000);
