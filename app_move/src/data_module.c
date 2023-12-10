#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/random/rand32.h>

#include "max30001.h"

#include "data_module.h"
#include "hw_module.h"
#include "sampling_module.h"
#include "fs_module.h"

// ProtoCentral data formats
#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 22

char DataPacket[DATA_LEN];
const char DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};

extern const struct device *const max30001_dev;
extern const struct device *const max32664_dev;

static bool settings_send_usb_enabled = true;
static bool settings_send_ble_enabled = false;
static bool settings_send_rpi_uart_enabled = false;

static bool settings_log_data_enabled = false;//true;
static int settings_data_format = DATA_FMT_PLAIN_TEXT;

extern struct k_msgq q_sample;
extern struct k_msgq q_plot;

#define SAMPLING_FREQ 104 // in Hz.

#define LOG_SAMPLE_RATE_SPS 125 
#define LOG_WRITE_INTERVAL  10  // Write to file every 10 seconds
#define LOG_BUFFER_LENGTH   1250+1 // 125Hz * 10 seconds

struct hpi_sensor_data_t log_buffer[LOG_BUFFER_LENGTH];

uint16_t current_session_log_counter = 0;
uint16_t current_session_log_id = 0;
char session_id_str[5];

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
    DataPacket[20] = rr;
    DataPacket[21] = hr;

    if (settings_send_usb_enabled)
    {
        send_usb_cdc(DataPacketHeader, 5);
        send_usb_cdc(DataPacket, DATA_LEN);
        send_usb_cdc(DataPacketFooter, 2);
    }

    if (settings_send_ble_enabled)
    {
        // cmdif_send_ble_data(DataPacketHeader, 5);
        cmdif_send_ble_data(DataPacket, DATA_LEN);
        // cmdif_send_ble_data(DataPacketFooter, 2);
    }

    if (settings_send_rpi_uart_enabled)
    {
        send_rpi_uart(DataPacketHeader, 5);
        send_rpi_uart(DataPacket, DATA_LEN);
        send_rpi_uart(DataPacketFooter, 2);
    }
}

void send_data_text(int32_t ecg_sample, int32_t bioz_sample, int32_t raw_red)
{
    char data[100];
    float f_ecg_sample = (float)ecg_sample / 1000;
    float f_bioz_sample = (float)bioz_sample / 1000;
    float f_raw_red = (float)raw_red / 1000;

    sprintf(data, "%.3f\t%.3f\t%.3f\r\n", f_ecg_sample, f_bioz_sample, f_raw_red);

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

    sprintf(data, "%.3f\r\n", f_in_sample);
    send_usb_cdc(data, strlen(data));
}

// Start a new session log
void record_init_session_log()
{
    // current_session_log.session_id = 0;
    current_session_log_id = 0;
    // strcpy(current_session_log.session_header, "Session Header");
    for(int i = 0; i < LOG_BUFFER_LENGTH; i++)
    {
        log_buffer[i].ecg_sample = 0;
        log_buffer[i].bioz_sample = 0;
        log_buffer[i].raw_red = 0;
        log_buffer[i].raw_ir = 0;
        log_buffer[i].temp = 0;
        log_buffer[i]._bioZSkipSample = false;
    }

    current_session_log_counter = 0;
    current_session_log_id = (uint16_t) sys_rand32_get(); // Create random session ID

    //printk("Init Session ID %s \n", log_get_current_session_id_str());
}

char *log_get_current_session_id_str(void)
{
    sprintf(session_id_str, "%d", current_session_log_id);
    return session_id_str;
}


// Add a log point to the current session log
void record_session_add_point(int32_t ecg_val, int32_t bioz_val, int32_t raw_ir_val, int32_t raw_red_val, int16_t temp)
{
    

    if((current_session_log_counter-1) < LOG_BUFFER_LENGTH)
    {
        current_session_log_counter++;
    }
    else
    {
        printk("Log Buffer Full at %d \n", k_uptime_get_32());
        record_write_to_file(current_session_log_id, current_session_log_counter, log_buffer);
        current_session_log_counter = 0;
        
    }

    //log_buffer[current_session_log_counter].ecg_sample = time;
    log_buffer[current_session_log_counter].ecg_sample = ecg_val;
    log_buffer[current_session_log_counter].bioz_sample = bioz_val;
    log_buffer[current_session_log_counter].raw_ir = raw_ir_val;
    log_buffer[current_session_log_counter].raw_red = raw_red_val;
    log_buffer[current_session_log_counter].temp = temp;

}

void data_thread(void)
{
    printk("Data Thread starting\n");

    struct hpi_sensor_data_t sensor_sample;

    record_init_session_log();

    for (;;)
    {
        k_msgq_get(&q_sample, &sensor_sample, K_FOREVER);

        // Data is now available in sensor_sample

        // Send, store or process data here



        /***** Send to USB if enabled *****/

        if (settings_send_usb_enabled)
        {
            if (settings_data_format == DATA_FMT_OPENVIEW)
            {
                sendData(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red, sensor_sample.raw_ir,
                         sensor_sample.temp, 0, 0, 0, sensor_sample._bioZSkipSample);
            }
            else if (settings_data_format == DATA_FMT_PLAIN_TEXT)
            {
                send_data_text(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red);
                //printk("ECG: %d, BIOZ: %d, RED: %d\n", sensor_sample.ecg_sample, sensor_sample.bioz_sample,
                //       sensor_sample.raw_red);
            }
        }

        k_msgq_put(&q_plot, &sensor_sample, K_NO_WAIT);

        if (settings_log_data_enabled)
        {
            // log_data(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red, sensor_sample.raw_ir,
            //          sensor_sample.temp, 0, 0, 0, sensor_sample._bioZSkipSample);
            record_session_add_point(sensor_sample.ecg_sample, sensor_sample.bioz_sample, sensor_sample.raw_red,
                                     sensor_sample.raw_ir, sensor_sample.temp);

        }
    }
}

#define DATA_THREAD_STACKSIZE 1024
#define DATA_THREAD_PRIORITY 7

K_THREAD_DEFINE(data_thread_id, DATA_THREAD_STACKSIZE, data_thread, NULL, NULL, NULL, DATA_THREAD_PRIORITY, 0, 1000);
