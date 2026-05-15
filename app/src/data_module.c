/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/zbus/zbus.h>

#include "max30001.h"

#include "data_module.h"
#include "datalog_module.h"
#include "hw_module.h"
#include "cmd_module.h"
#include "hpi_common_types.h"

LOG_MODULE_REGISTER(data_module, LOG_LEVEL_INF);  // Changed from DBG to reduce log spam

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
#include "display_module.h"
#endif

#include "fs_module.h"
#include "ble_module.h"

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

#define SAMPLE_BUFF_WATERMARK 4

enum hpi5_data_format
{
    DATA_FMT_OPENVIEW,
    DATA_FMT_PLAIN_TEXT,
    DATA_FMT_HPI5_OV3,

} hpi5_data_format_t;

// Data packet OpenView original format (ECG , Bioz and PPG in the same packet)
#define DATA_LEN 22
uint8_t DataPacket[DATA_LEN];
const char DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};

bool settings_send_usb_enabled = true;
bool settings_send_ble_enabled = true;
bool settings_plot_enabled = false;
bool settings_send_rpi_uart_enabled = false; //set to true for UART data streaming

// struct hpi_sensor_data_t log_buffer[LOG_BUFFER_LENGTH];
struct hpi_sensor_logging_data_t log_buffer[LOG_BUFFER_LENGTH];

uint16_t current_session_ecg_counter = 0;
uint16_t current_session_bioz_counter = 0;
uint16_t current_session_ppg_counter = 0;
uint16_t current_session_log_id = 0;
char session_id_str[15];

static volatile uint16_t m_resp_rate = 0;

// Externs
// extern struct k_msgq q_sample;

extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;

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

// DC blocker for streamed PPG (y[n] = x[n] - x[n-1] + a*y[n-1])
// a close to 1.0 removes slow baseline drift while preserving pulsatile AC.
#define PPG_TX_DC_BLOCK_ALPHA 0.995f

int16_t spo2_serial;
int16_t hr_serial;
int16_t rr_serial;
uint16_t temp_serial;

struct ppg_dc_block_filter_t {
    float x_prev;
    float y_prev;
    bool initialized;
};

static struct ppg_dc_block_filter_t ppg_red_tx_filter;
static struct ppg_dc_block_filter_t ppg_ir_tx_filter;

static int32_t ppg_dc_block_filter_apply(struct ppg_dc_block_filter_t *filter, int32_t sample)
{
    if (!filter->initialized) {
        filter->x_prev = (float)sample;
        filter->y_prev = 0.0f;
        filter->initialized = true;
        return 0;
    }

    float x = (float)sample;
    float y = (x - filter->x_prev)*(1+PPG_TX_DC_BLOCK_ALPHA)/2 + PPG_TX_DC_BLOCK_ALPHA * filter->y_prev; // Gain compensation to preserve AC amplitude
    filter->x_prev = x;
    filter->y_prev = y;

    return (int32_t)y;
}

ZBUS_CHAN_DECLARE(hr_chan);
ZBUS_CHAN_DECLARE(spo2_chan);
ZBUS_CHAN_DECLARE(resp_rate_chan);
ZBUS_CHAN_DECLARE(lead_off_chan);  // Lead-off state channel
ZBUS_CHAN_DECLARE(temp_chan);

// New vars

static enum hpi_stream_modes m_stream_mode = HPI_STREAM_MODE_USB;
K_MUTEX_DEFINE(mutex_stream_mode);

// HR source selection (ECG vs PPG)
static enum hpi_hr_source m_hr_source = HR_SOURCE_PPG;
K_MUTEX_DEFINE(mutex_hr_source);

// ============================================================================
// Lead-Off Detection State Management (UI Layer Only)
// ============================================================================
// Time-based debouncing for UI updates only - all detection logic is in spo2_process.c

#define ECG_LEADOFF_DEBOUNCE_MS    500    // ECG lead-off is stable (hardware detection)
#define PPG_LEADOFF_DEBOUNCE_MS    1500   // PPG UI debounce for removal (balanced with AC hysteresis)

// Lead-off state tracking (UI layer)
static bool ecg_lead_off_state = false;   // Current UI state for ECG
static bool ppg_lead_off_state = true;    // Current UI state for PPG (start as lead-off)
static int64_t ecg_leadoff_timer = 0;     // Timestamp for ECG debouncing
static int64_t ppg_leadoff_timer = 0;     // Timestamp for PPG debouncing
static bool ecg_leadoff_raw = false;      // Raw ECG hardware status
static bool ppg_leadoff_prev = true;      // Previous filtered state from algorithm

// Last valid values for lead-off state change publications
static uint8_t last_valid_ecg_hr = 0;     // Last valid ECG HR (for lead-off publications)
static uint8_t last_valid_rr = 0;          // Last valid respiration rate

// SpO2 algorithm probe state tracker (initialized once)
static spo2_probe_state_t spo2_probe_state;
static bool spo2_probe_state_initialized = false;

static void data_temp_listener(const struct zbus_channel *chan)
{
    const struct hpi_temp_t *temp_data = zbus_chan_const_msg(chan);
    float temp_c = temp_data->temp_c; // Store latest temperature in Celsius
    temp_serial = (uint16_t)(temp_c * 100); // Convert to fixed-point representation
}

ZBUS_LISTENER_DEFINE(serial_temp_listener, data_temp_listener);

void hpi_data_set_stream_mode(enum hpi_stream_modes mode)
{
    k_mutex_lock(&mutex_stream_mode, K_FOREVER);
    if (m_stream_mode != mode) {
        LOG_INF("Stream mode: %d -> %d", m_stream_mode, mode);
    }
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

void sendData(int32_t ecg_sample, int32_t bioz_sample, int32_t raw_red, int32_t raw_ir, int16_t temp, uint8_t hr,
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
        // Consolidate into single buffer to reduce ring buffer fragmentation
        // and interrupt overhead (was 3 separate calls, now 1)
        static uint8_t consolidated_packet[5 + DATA_LEN + 2];
        memcpy(consolidated_packet, DataPacketHeader, 5);
        memcpy(consolidated_packet + 5, DataPacket, DATA_LEN);
        memcpy(consolidated_packet + 5 + DATA_LEN, DataPacketFooter, 2);
        send_usb_cdc(consolidated_packet, sizeof(consolidated_packet));
    }

    if (settings_send_rpi_uart_enabled)
    {
        send_rpi_uart(DataPacketHeader, 5);
        send_rpi_uart(DataPacket, DATA_LEN);
        send_rpi_uart(DataPacketFooter, 2);
    }
}

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

void data_thread(void)
{
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
    
    // Heartbeat tracking
    uint32_t samples_processed = 0;
    uint32_t last_heartbeat_time = 0;

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

// BLE buffer size: 8 samples = 64ms at 125 Hz (matches typical BLE connection intervals)
#define BLE_ECG_BUFFER_SIZE 8

#define RESP_FILT_BUFFER_SIZE 4

    int32_t ble_ecg_buffer[BLE_ECG_BUFFER_SIZE];
    int32_t ble_bioz_buffer[BLE_ECG_BUFFER_SIZE];
    int32_t ble_ppg_buffer[BLE_ECG_BUFFER_SIZE];

    uint8_t ecg_buffer_count = 0;
    uint8_t bioz_buffer_count = 0;
    uint8_t ppg_buffer_count = 0;

    int16_t resp_i16_buf[RESP_FILT_BUFFER_SIZE];
    int16_t resp_i16_filt_out[RESP_FILT_BUFFER_SIZE];
    int16_t resp_filt_buffer_count = 0;

    LOG_INF("Data Thread starting");
    
    // Initialize heartbeat tracking
    last_heartbeat_time = k_uptime_get_32();

    // Load HR source setting from filesystem
    //m_hr_source = settings_load_hr_source();
    //LOG_INF("Initialized HR source: %s", m_hr_source == HR_SOURCE_ECG ? "ECG" : "PPG");

    // Initialize lead-off timers
    ecg_leadoff_timer = k_uptime_get();
    ppg_leadoff_timer = k_uptime_get();
    
    // Initialize SpO2 probe state tracker (consecutive count filtering in algorithm)
    // With severe AC alternation (110→260→110→260), use very forgiving consecutive filtering
    // Real-world testing shows valid SpO2=99% even with AC=111-130, so accept marginal signals
    // threshold_off=4: need 4 consecutive probe-off detections (~2 seconds at 0.5s/reading)
    //   - Quick removal detection when AC < 80
    //   - Strict: truly poor contact, not just marginal
    // threshold_on=3: need 3 consecutive probe-on detections (~1.5 seconds)
    //   - Very fast initial detection when AC > 130 (allows alternating 111→260 to pass)
    //   - Asymmetric hysteresis: easy to detect, moderate to lose
    spo2_probe_state_init(&spo2_probe_state, 4, 3);
    spo2_probe_state_initialized = true;

    for (;;)
    {
        // Update heartbeat for software watchdog
        heartbeat_data_thread = k_uptime_get_32();

        // Process ALL available samples before sleeping (critical for performance)
        int loop_samples_processed = 0;
        static uint32_t last_data_log_time = 0;

        while (k_msgq_get(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT) == 0)
        {
            samples_processed++;
            loop_samples_processed++;

            // Log data thread activity every 30 seconds
            static uint32_t usb_send_count = 0;
            static uint32_t ble_send_count = 0;
            uint32_t now = k_uptime_get_32();
            if (now - last_data_log_time >= 30000) {
                LOG_INF("Data: %u samples, mode=%d, USB=%u, BLE=%u, queue=%u",
                        samples_processed, m_stream_mode, usb_send_count, ble_send_count,
                        k_msgq_num_used_get(&q_hpi_data_sample));
                last_data_log_time = now;
            }
            
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
                    &m_spo2, &validSPO2, &m_hr, &validHeartRate, &quality_metrics, &spo2_probe_state);
                
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
                // Confidence threshold removed - probe-off detection handles validity
                if (validSPO2 && m_spo2 > 0 && m_spo2 <= 100)
                {
                    // Outlier rejection: Only reject sudden DROPS >5%, allow gradual increases
                    // SpO2 can legitimately increase from low readings when signal improves
                    bool is_spo2_outlier = false;
                    if (spo2_history_count > 0 && spo2_filtered > 0) {
                        // Only check for drops, not increases
                        if (m_spo2 < spo2_filtered) {
                            int32_t spo2_drop = spo2_filtered - m_spo2;
                            if (spo2_drop > 5) {  // Reject sudden drops >5%
                                is_spo2_outlier = true;
                                LOG_WRN("SpO2 outlier detected: %d%% (filtered: %d%%, drop: %d)", 
                                        m_spo2, spo2_filtered, spo2_drop);
                            }
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
                    // Include PPG lead-off status - display should show "--" if probe off
                    spo2_serial = spo2_filtered;
                    struct hpi_spo2_t spo2_chan_value = {
                        .spo2 = spo2_filtered,
                        .lead_off = ppg_lead_off_state
                    };
                    spo2_serial = spo2_filtered;
                    zbus_chan_pub(&spo2_chan, &spo2_chan_value, K_NO_WAIT);
                }
                
                // Publish HR with enhanced validation and smoothing filter
                // Enhanced quality gating: Reject readings with poor signal quality
                // Testing showed PI=0% readings give wildly inaccurate HR (36-101 bpm vs 67 bpm actual)
                // Primary filter is perfusion index (PI ≥ 1%) as it correlates strongly with accuracy
                if (validHeartRate && m_hr > 30 && m_hr < 220 &&
                    quality_metrics.perfusion_ir >= 100)  // Require PI ≥ 1.0% (primary quality gate)
                {
                    // Outlier rejection: Only reject sudden DROPS or JUMPS >30 bpm
                    // HR can legitimately vary but sudden extreme changes indicate noise
                    bool is_outlier = false;
                    if (hr_history_count > 0 && hr_filtered > 0) {
                        // Check both increases and decreases for HR (unlike SpO2)
                        // HR can jump up suddenly (exercise) or drop (relaxation)
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
                    // Include PPG lead-off status - HR invalid if probe off
                    if (hpi_data_get_hr_source() == HR_SOURCE_PPG) {
                        hr_serial = hr_filtered;
                        struct hpi_hr_t hr_chan_value = {
                            .hr = hr_filtered,
                            .lead_off = ppg_lead_off_state
                        };
                        hr_serial = hr_filtered;
                        zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
                    }
                }
                else if (validHeartRate && quality_metrics.perfusion_ir < 100) {
                    LOG_DBG("HR rejected: %d bpm (PI=%d.%02d%%, low perfusion)",
                            m_hr, quality_metrics.perfusion_ir / 100, quality_metrics.perfusion_ir % 100);
                }
                
                // ============================================================================
                // PPG Lead-Off Detection - UI Debouncing Only
                // ============================================================================
                // All detection logic (DC level, PI, peaks, consecutive filtering) is in spo2_process.c
                // Here we only apply final time-based debounce for UI stability
                
                bool probe_off_filtered = quality_metrics.probe_off_filtered;
                
                // Check if filtered state changed from algorithm
                if (probe_off_filtered != ppg_leadoff_prev) {
                    // State change detected, restart timer
                    ppg_leadoff_timer = k_uptime_get();
                    ppg_leadoff_prev = probe_off_filtered;
                    
                    LOG_INF("PPG filtered state changed: %s (reason=%d, PI=%d.%02d%%)",
                            probe_off_filtered ? "PROBE-OFF" : "PROBE-ON",
                            quality_metrics.probe_off_reason,
                            quality_metrics.perfusion_ir / 100, 
                            quality_metrics.perfusion_ir % 100);
                }
                
                // Apply asymmetric debouncing: fast response when finger placed, slow when removed
                int64_t elapsed_ms = k_uptime_get() - ppg_leadoff_timer;
                
                if (probe_off_filtered != ppg_lead_off_state) {
                    // Asymmetric debounce thresholds (reduced with lower consecutive counts):
                    // - PROBE-ON (finger placed): 300ms - fast initial detection (4 consecutive in algorithm)
                    // - PROBE-OFF (finger removed): 1500ms - prevent flickering (6 consecutive + time buffer)
                    int64_t required_debounce = probe_off_filtered ? PPG_LEADOFF_DEBOUNCE_MS : 300;
                    
                    if (elapsed_ms >= required_debounce) {
                        ppg_lead_off_state = probe_off_filtered;
                        LOG_INF("PPG UI state updated: %s (after %lld ms)",
                                ppg_lead_off_state ? "LEAD-OFF" : "CONNECTED", elapsed_ms);
                        
                        // Immediately publish lead-off state change for both SpO2 and HR (PPG source)
                        // This ensures display updates to show "--" even if no valid readings
                        struct hpi_spo2_t spo2_chan_value = {
                            .spo2 = spo2_filtered,  // Keep last valid value
                            .lead_off = ppg_lead_off_state
                        };
                        spo2_serial = spo2_filtered;
                        zbus_chan_pub(&spo2_chan, &spo2_chan_value, K_NO_WAIT);
                        
                        // Also update PPG HR if it's the active source
                        if (hpi_data_get_hr_source() == HR_SOURCE_PPG) {
                            struct hpi_hr_t hr_chan_value = {
                                .hr = hr_filtered,  // Keep last valid value
                                .lead_off = ppg_lead_off_state
                            };
                            hr_serial = hr_filtered;
                            zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
                        }
                    }
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
                // DEBUG: Log the scaling issue
                static uint16_t dbg_cnt = 0;
                if (dbg_cnt++ > 2000) {
                    dbg_cnt = 0;
                    LOG_DBG("BioZ: Original=%d, Scaled>>4=%d, USB sends Original",
                            hpi_sensor_data_point.bioz_sample,
                            (int16_t)(hpi_sensor_data_point.bioz_sample >> 4));
                }

                // TEMPORARY FIX: Remove >> 4 scaling for sine wave testing
                // Original: resp_i16_buf[resp_filt_buffer_count++] = (int16_t)(hpi_sensor_data_point.bioz_sample >> 4);
                resp_i16_buf[resp_filt_buffer_count++] = (int16_t)(hpi_sensor_data_point.bioz_sample);
            }
            else
            {
                // if (hpi_sensor_data_point.bioz_sample != 0)
                resp_process_sample(resp_i16_buf, resp_i16_filt_out);
                resp_algo_process(resp_i16_filt_out, &m_resp_rate);

                // Publish respiration rate with lead-off status
                // BioZ signal requires ECG electrodes for proper measurement
                last_valid_rr = m_resp_rate;  // Remember for lead-off state changes
                struct hpi_resp_rate_t resp_rate_chan_value = {
                    .resp_rate = m_resp_rate,
                    .lead_off = ecg_lead_off_state
                };
                // Use K_NO_WAIT to prevent blocking data thread and causing USB stalling
                rr_serial = m_resp_rate;
                zbus_chan_pub(&resp_rate_chan, &resp_rate_chan_value, K_NO_WAIT);

                resp_filt_buffer_count = 0;
            }

            // Publish ECG HR if ECG source is selected
            // ECG HR comes from MAX30001 R-R interval detection
            // Include lead-off status - HR invalid if electrodes disconnected
            if (hpi_data_get_hr_source() == HR_SOURCE_ECG && 
                hpi_sensor_data_point.hr > 0 && hpi_sensor_data_point.hr < 255)
            {
                hr_serial = hpi_sensor_data_point.hr;
                last_valid_ecg_hr = hpi_sensor_data_point.hr;  // Remember for lead-off state changes
                struct hpi_hr_t hr_chan_value = {
                    .hr = hpi_sensor_data_point.hr,
                    .lead_off = ecg_lead_off_state
                };
                hr_serial = hpi_sensor_data_point.hr;
                zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
            }

            // ============================================================================
            // ECG Lead-Off Detection (Hardware-based with debouncing)
            // ============================================================================
            // MAX30001 provides hardware DC lead-off detection
            // ecg_lead_off: 0 = electrodes connected, 1 = electrodes disconnected
            
            bool ecg_leadoff_raw_new = (hpi_sensor_data_point.ecg_lead_off != 0);
            
            if (ecg_leadoff_raw_new != ecg_leadoff_raw) {
                // State change detected, restart timer
                ecg_leadoff_raw = ecg_leadoff_raw_new;
                ecg_leadoff_timer = k_uptime_get();
                LOG_DBG("ECG lead-off raw state change: %s", 
                        ecg_leadoff_raw ? "DISCONNECTED" : "CONNECTED");
            }
            else {
                // Check if debounce period has elapsed
                int64_t elapsed_ms = k_uptime_get() - ecg_leadoff_timer;
                
                if (ecg_leadoff_raw && !ecg_lead_off_state && elapsed_ms >= ECG_LEADOFF_DEBOUNCE_MS) {
                    // Transitioning to lead-off
                    ecg_lead_off_state = true;
                    LOG_INF("ECG Lead-Off DETECTED (electrodes disconnected for %lld ms)", elapsed_ms);
                    
                    // Immediately publish lead-off state change for ECG HR and RR
                    // This ensures display updates to show "--" even if no valid readings
                    if (hpi_data_get_hr_source() == HR_SOURCE_ECG) {
                        struct hpi_hr_t hr_chan_value = {
                            .hr = last_valid_ecg_hr,  // Keep last valid value
                            .lead_off = ecg_lead_off_state
                        };
                        hr_serial = last_valid_ecg_hr;
                        zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
                    }
                    
                    struct hpi_resp_rate_t rr_chan_value = {
                        .resp_rate = last_valid_rr,  // Keep last valid value
                        .lead_off = ecg_lead_off_state
                    };
                    rr_serial = last_valid_rr;  // Update serial with last valid value for consistency
                    zbus_chan_pub(&resp_rate_chan, &rr_chan_value, K_NO_WAIT);
                }
                else if (!ecg_leadoff_raw && ecg_lead_off_state && elapsed_ms >= ECG_LEADOFF_DEBOUNCE_MS) {
                    // Transitioning to connected
                    ecg_lead_off_state = false;
                    LOG_INF("ECG Lead-On DETECTED (electrodes connected for %lld ms)", elapsed_ms);
                    
                    // Immediately publish lead-on state change for ECG HR and RR
                    if (hpi_data_get_hr_source() == HR_SOURCE_ECG) {
                        struct hpi_hr_t hr_chan_value = {
                            .hr = last_valid_ecg_hr,  // Keep last valid value
                            .lead_off = ecg_lead_off_state
                        };
                        hr_serial = last_valid_ecg_hr;
                        zbus_chan_pub(&hr_chan, &hr_chan_value, K_NO_WAIT);
                    }
                    
                    struct hpi_resp_rate_t rr_chan_value = {
                        .resp_rate = last_valid_rr,  // Keep last valid value
                        .lead_off = ecg_lead_off_state
                    };
                    rr_serial = last_valid_rr;
                    zbus_chan_pub(&resp_rate_chan, &rr_chan_value, K_NO_WAIT);
                }
            }

            if (m_stream_mode == HPI_STREAM_MODE_USB)
            {
                usb_send_count++;
                int32_t ppg_red_tx = ppg_dc_block_filter_apply(&ppg_red_tx_filter, hpi_sensor_data_point.ppg_sample_red);
                int32_t ppg_ir_tx = ppg_dc_block_filter_apply(&ppg_ir_tx_filter, hpi_sensor_data_point.ppg_sample_ir);

                sendData(hpi_sensor_data_point.ecg_sample, hpi_sensor_data_point.bioz_sample, ppg_red_tx,
                         ppg_ir_tx, temp_serial, hr_serial, rr_serial, spo2_serial, 0);
            }
            else if (m_stream_mode == HPI_STREAM_MODE_BLE)
            {
                ble_send_count++;
                int32_t ppg_red_tx = ppg_dc_block_filter_apply(&ppg_red_tx_filter, hpi_sensor_data_point.ppg_sample_red);

                if (ppg_buffer_count < BLE_ECG_BUFFER_SIZE)
                {
                    ble_ppg_buffer[ppg_buffer_count++] = ppg_red_tx;
                }
                else
                {
                    ble_ppg_notify(ble_ppg_buffer, BLE_ECG_BUFFER_SIZE);
                    ppg_buffer_count = 0;
                    ble_ppg_buffer[ppg_buffer_count++] = ppg_red_tx;
                }

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
            LOG_INF("BLE connected - switching to BLE stream mode");
            hpi_data_set_stream_mode(HPI_STREAM_MODE_BLE);
        }

        if (k_sem_take(&sem_ble_disconnected, K_NO_WAIT) == 0)
        {
            LOG_INF("BLE disconnected - switching to USB stream mode");
            hpi_data_set_stream_mode(HPI_STREAM_MODE_USB);
        }

        // ============================================================================
        // Publish Lead-Off State Updates via Zbus (10Hz throttled)
        // ============================================================================
        // CRITICAL: Never call LVGL functions from data thread!
        // Lead-off status is now embedded in vital sign messages (hr_chan, spo2_chan, resp_rate_chan)
        // No need for separate lead-off channel publication
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
        // Placeholder for future display-only updates
#endif

        if (loop_samples_processed == 0) {
            k_sleep(K_USEC(500));  // 500us sleep when idle (no samples)
        } else {
            // Check USB buffer health for backpressure
            uint8_t usb_util = get_usb_buffer_utilization();
            if (usb_util > 90) {
                // USB buffer critically full (>90%) - apply backpressure
                // This happens when USB host stops reading or disconnects
                k_sleep(K_MSEC(10));  // 10ms backpressure sleep to prevent buffer overflow
            } else {
                // Normal: Small sleep to prevent CPU starvation
                // At 256 SPS, we process samples in bursts - need to yield CPU between bursts
                k_sleep(K_USEC(100));  // 100us sleep (was 10us, caused CPU starvation)
            }
        }
    }
}

#define DATA_THREAD_STACKSIZE 5120
#define DATA_THREAD_PRIORITY 6  // Lower priority than sampling workqueue (5) - sampling must be timely

K_THREAD_DEFINE(data_thread_id, DATA_THREAD_STACKSIZE, data_thread, NULL, NULL, NULL, DATA_THREAD_PRIORITY, 0, 0);
