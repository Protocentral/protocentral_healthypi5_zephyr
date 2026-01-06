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
#include "hw_module.h"

LOG_MODULE_REGISTER(sampling_module, CONFIG_SENSOR_LOG_LEVEL);

extern const struct device *const afe4400_dev;
extern const struct device *const max30205_dev;

#define UNIFIED_SAMPLING_INTERVAL_MS 7  // 128 SPS = 7.8ms per sample

static uint32_t ppg_sample_counter = 0;
#define PPG_READ_INTERVAL 2  // Read PPG every 2nd cycle

// Unified data point queue - 384 entries = 3 seconds buffer at 128 SPS (balanced RAM/stability)
K_MSGQ_DEFINE(q_hpi_data_sample, sizeof(struct hpi_sensor_data_point_t), 384, 1);

// Dedicated work queue for sensor sampling to avoid overloading system workqueue
// Priority 5 with 5KB stack (needs space for SPI/RTIO operations + safety margin)
K_THREAD_STACK_DEFINE(sampling_workq_stack, 5120);
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

static bool sensors_ready = false;

void work_sample_handler(struct k_work *work)
{
    // Update heartbeat for software watchdog
    heartbeat_sampling_workq = k_uptime_get_32();

    if (!sensors_ready) {
        return;
    }

    static uint32_t consecutive_errors = 0;
    // Buffer for max30001_encoded_data structure (~280 bytes)
    uint8_t ecg_bioz_buf[288];
    // Buffer for afe4400_encoded_data structure (~16 bytes)
    uint8_t ppg_buf[24];

    int ret = sensor_read(&max30001_iodev, &max30001_read_rtio_poll_ctx, ecg_bioz_buf, sizeof(ecg_bioz_buf));
    if (ret < 0)
    {
        consecutive_errors++;
        if (consecutive_errors <= 3 || (consecutive_errors % 1000) == 0) {
            LOG_ERR("MAX30001 read error %d (count=%u)", ret, consecutive_errors);
        }
        return;
    }
    consecutive_errors = 0;

    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)ecg_bioz_buf;
    uint8_t n_samples_ecg = edata->num_samples_ecg;
    uint8_t n_samples_bioz = edata->num_samples_bioz;

    // Read PPG at reduced rate (every 2nd cycle)
    if (++ppg_sample_counter >= PPG_READ_INTERVAL) {
        ppg_sample_counter = 0;

        ret = sensor_read(&afe4400_iodev, &afe4400_read_rtio_poll_ctx, ppg_buf, sizeof(ppg_buf));
        if (ret == 0) {
            sensor_ppg_decode(ppg_buf, sizeof(ppg_buf));
        }
    }

    if (n_samples_ecg > 0) {
        // BioZ runs at 64 SPS (half of ECG's 128 SPS), interleave samples
        int bioz_idx = 0;

        for (int i = 0; i < n_samples_ecg; i++) {
            hpi_sensor_data_point.ecg_sample = edata->ecg_samples[i];

            // Distribute BioZ samples across ECG samples (2:1 ratio)
            if (n_samples_bioz > 0) {
                bioz_idx = (i * n_samples_bioz) / n_samples_ecg;
                if (bioz_idx >= n_samples_bioz) {
                    bioz_idx = n_samples_bioz - 1;
                }
                hpi_sensor_data_point.bioz_sample = edata->bioz_samples[bioz_idx];
            }

            if (i == 0) {
                hpi_sensor_data_point.hr = edata->hr;
                hpi_sensor_data_point.rtor = edata->rri;
                hpi_sensor_data_point.ecg_lead_off = edata->ecg_lead_off;
                hpi_sensor_data_point.bioz_lead_off = edata->bioz_lead_off;
            }

            k_msgq_put(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT);
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
    k_sem_take(&sem_ecg_bioz_thread_start, K_FOREVER);

    k_work_queue_init(&sampling_workq);
    k_work_queue_start(&sampling_workq, sampling_workq_stack,
                       K_THREAD_STACK_SIZEOF(sampling_workq_stack),
                       5, NULL);

    sensors_ready = true;
    k_timer_start(&tmr_sensor_sample_all, K_MSEC(UNIFIED_SAMPLING_INTERVAL_MS), K_MSEC(UNIFIED_SAMPLING_INTERVAL_MS));

    for (;;) {
        k_msleep(1000);
    }
}

#define UNIFIED_SAMPLING_THREAD_PRIORITY 7

K_THREAD_DEFINE(hpi_sensor_read_all_thread_id, 3072, hpi_sensor_read_all_thread, NULL, NULL, NULL, UNIFIED_SAMPLING_THREAD_PRIORITY, 0, 1000);
