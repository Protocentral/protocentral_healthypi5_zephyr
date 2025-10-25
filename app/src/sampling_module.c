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

#include "max30001.h"
#include "afe4400.h"

#include "hpi_common_types.h"

// #include "display_screens.h"
LOG_MODULE_REGISTER(sampling_module, CONFIG_SENSOR_LOG_LEVEL);

// Sample count tracking for diagnostics
static uint32_t work_handler_calls = 0;

// extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;
extern const struct device *const max30205_dev;

#define SAMPLING_INTERVAL_MS 6           // Time between samples in milliseconds
#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

#define PPG_SAMPLING_INTERVAL_MS 8
#define ECG_SAMPLING_INTERVAL_MS 50

#define UNIFIED_SAMPLING_INTERVAL_MS 8  // 8ms = 125Hz reads, matches 128 SPS sensor rate

// Unified data point queue - 128 entries = 1 second buffer at 128 SPS
K_MSGQ_DEFINE(q_hpi_data_sample, sizeof(struct hpi_sensor_data_point_t), 128, 1);

// Dedicated work queue for sensor sampling to avoid overloading system workqueue
// Priority 6 (same as data thread) with 4KB stack (needs space for SPI/RTIO operations)
K_THREAD_STACK_DEFINE(sampling_workq_stack, 4096);
static struct k_work_q sampling_workq;

RTIO_DEFINE(max30001_read_rtio_poll_ctx, 16, 16);
RTIO_DEFINE(afe4400_read_rtio_poll_ctx, 16, 16);

SENSOR_DT_READ_IODEV(max30001_iodev, DT_ALIAS(max30001), {SENSOR_CHAN_VOLTAGE});
SENSOR_DT_READ_IODEV(afe4400_iodev, DT_ALIAS(afe4400), {SENSOR_CHAN_RED});

extern struct k_sem sem_ecg_bioz_thread_start;

static volatile int hpi_sampling_ppg_sample_count = 0;

static struct hpi_sensor_data_point_t hpi_sensor_data_point;

/*static void sensor_ppg_decode(uint8_t *buf, uint32_t buf_len)
{
    const struct afe4400_encoded_data *edata = (const struct afe4400_encoded_data *)buf;

    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    ppg_sensor_sample.ppg_red_sample = edata->raw_sample_red;
    ppg_sensor_sample.ppg_ir_sample = edata->raw_sample_ir;

    if(k_msgq_put(&q_ppg_sample, &ppg_sensor_sample, K_MSEC(1))!=0)
    {
        LOG_ERR("PPG sample queue error");
        //k_msgq_purge(&q_ppg_sample);
    }
}*/

static void sensor_ppg_decode(uint8_t *buf, uint32_t buf_len)
{
    const struct afe4400_encoded_data *edata = (const struct afe4400_encoded_data *)buf;

    hpi_sensor_data_point.ppg_sample_red = edata->raw_sample_red;
    hpi_sensor_data_point.ppg_sample_ir = edata->raw_sample_ir;
}

static bool sensors_ready = false;  // Flag to prevent timer callbacks before sensors are initialized

void work_sample_handler(struct k_work *work)
{
    // CRITICAL: Don't process if sensors aren't ready yet (prevents boot crash)
    if (!sensors_ready) {
        return;
    }
    
    work_handler_calls++;
    
    uint8_t ecg_bioz_buf[512];
    uint8_t ppg_buf[64];

    int ret = sensor_read(&max30001_iodev, &max30001_read_rtio_poll_ctx, ecg_bioz_buf, sizeof(ecg_bioz_buf));
    if (ret < 0)
    {
        LOG_ERR("Error reading from MAX30001");
        return;
    }

    // First, decode ECG/BioZ which updates hpi_sensor_data_point
    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)ecg_bioz_buf;
    uint8_t n_samples_ecg = edata->num_samples_ecg;
    uint8_t n_samples_bioz = edata->num_samples_bioz;
    
    // Debug logging (reduced frequency)
    static uint32_t debug_count = 0;
    if (++debug_count % 2500 == 0) {  // Every ~10 seconds at 250Hz
        LOG_INF("ECG/BioZ: n_ecg=%d, n_bioz=%d, calls=%u",
                n_samples_ecg, n_samples_bioz, work_handler_calls);
    }

    ret = sensor_read(&afe4400_iodev, &afe4400_read_rtio_poll_ctx, ppg_buf, sizeof(ppg_buf));
    if (ret < 0)
    {
        LOG_ERR("Error reading from AFE4400");
        return;
    }
    sensor_ppg_decode(ppg_buf, sizeof(ppg_buf));
    
    // MAX30001 now configured for 128 SPS
    // At 8ms reads (125Hz), expect ~1 sample per read
    // Process all samples to prevent any FIFO accumulation
    
    int samples_to_process = (n_samples_ecg > 2) ? 2 : n_samples_ecg;
    
    for (int i = 0; i < samples_to_process; i++) {
        hpi_sensor_data_point.ecg_sample = edata->ecg_samples[i];
        
        // BioZ only on first sample (64 SPS rate)
        if (i == 0 && n_samples_bioz > 0) {
            hpi_sensor_data_point.bioz_sample = edata->bioz_samples[0];
        }
        
        // Metadata only on first sample
        if (i == 0) {
            hpi_sensor_data_point.hr = edata->hr;
            hpi_sensor_data_point.rtor = edata->rri;
            hpi_sensor_data_point.ecg_lead_off = edata->ecg_lead_off;
            hpi_sensor_data_point.bioz_lead_off = edata->bioz_lead_off;
        }
        
        // K_NO_WAIT: at 128 SPS with 128-entry queue, drops indicate processing issue
        if (k_msgq_put(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT) != 0)
        {
            static uint32_t drop_count = 0;
            if (++drop_count % 50 == 1) {
                LOG_ERR("Queue full at 128 SPS, drops: %u", drop_count);
            }
        }
    }
}

K_WORK_DEFINE(work_sample, work_sample_handler);

void sample_all_handler(struct k_timer *dummy)
{
    // Submit to dedicated sampling workqueue instead of system workqueue
    // This prevents overwhelming the system workqueue
    k_work_submit_to_queue(&sampling_workq, &work_sample);
}

K_TIMER_DEFINE(tmr_sensor_sample_all, sample_all_handler, NULL);

void hpi_sensor_read_all_thread(void)
{
    int ret;
    //uint8_t ecg_bioz_buf[512];
    //uint8_t ppg_buf[64];

    k_sem_take(&sem_ecg_bioz_thread_start, K_FOREVER);
    LOG_INF("Sensor Read All Thread starting");

    // Initialize dedicated work queue for sensor sampling
    k_work_queue_init(&sampling_workq);
    k_work_queue_start(&sampling_workq, sampling_workq_stack,
                       K_THREAD_STACK_SIZEOF(sampling_workq_stack), 
                       6, NULL);  // Priority 6 (same as data thread)
    
    LOG_INF("Sampling work queue initialized");

    // Mark sensors as ready BEFORE starting timer to prevent null pointer crashes
    sensors_ready = true;
    
    k_timer_start(&tmr_sensor_sample_all, K_MSEC(UNIFIED_SAMPLING_INTERVAL_MS), K_MSEC(UNIFIED_SAMPLING_INTERVAL_MS));

    for (;;)
    {
        /*ret = sensor_read(&max30001_iodev, &max30001_read_rtio_poll_ctx, ecg_bioz_buf, sizeof(ecg_bioz_buf));
        if (ret < 0)
        {
            LOG_ERR("Error reading from MAX30001");
            continue;
        }

        sensor_ecg_bioz_decode(ecg_bioz_buf, sizeof(ecg_bioz_buf));

        ret = sensor_read(&afe4400_iodev, &afe4400_read_rtio_poll_ctx, ppg_buf, sizeof(ppg_buf));
        if (ret < 0)
        {
            LOG_ERR("Error reading from AFE4400");
            continue;
        }
        sensor_ppg_decode(ppg_buf, sizeof(ppg_buf));

        if (k_msgq_put(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT) != 0)
        {
            LOG_ERR("Unified sample queue error");
            // k_msgq_purge(&q_ecg_bioz_sample);
        }*/


        k_msleep(1000);
    }
}

#define UNIFIED_SAMPLING_THREAD_PRIORITY 7

K_THREAD_DEFINE(hpi_sensor_read_all_thread_id, 3072, hpi_sensor_read_all_thread, NULL, NULL, NULL, UNIFIED_SAMPLING_THREAD_PRIORITY, 0, 1000);

// K_THREAD_DEFINE(ecg_bioz_sample_trigger_thread_id, 4096, ecg_bioz_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);
// K_THREAD_DEFINE(ppg_sample_trigger_thread_id, 1024, ppg_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);
