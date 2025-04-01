#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "max30001.h"
#include "afe4400.h"

#include "hpi_common_types.h"

// #include "display_screens.h"
LOG_MODULE_REGISTER(sampling_module, CONFIG_SENSOR_LOG_LEVEL);

// extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;
extern const struct device *const max30205_dev;

#define SAMPLING_INTERVAL_MS 6           // Time between samples in milliseconds
#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

#define PPG_SAMPLING_INTERVAL_MS 8
#define ECG_SAMPLING_INTERVAL_MS 50

#define UNIFIED_SAMPLING_INTERVAL_MS 4

// Unified data point queue
K_MSGQ_DEFINE(q_hpi_data_sample, sizeof(struct hpi_sensor_data_point_t), 256, 1);

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

static void sensor_ecg_bioz_decode(uint8_t *buf, uint32_t buf_len)
{
    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)buf;

    // printk("ECG NS: %d | B: %d ", edata->num_samples_ecg, edata->num_samples_bioz);
    uint8_t n_samples_ecg = edata->num_samples_ecg;
    uint8_t n_samples_bioz = edata->num_samples_bioz;

    if (n_samples_ecg > 1)
    {
        n_samples_ecg = 1;
    }

    if (n_samples_bioz > 1)
    {
        n_samples_bioz = 1;
    }

    if ((n_samples_ecg > 0) || (n_samples_bioz > 0))
    {
        hpi_sensor_data_point.ecg_sample = edata->ecg_samples[0];
        hpi_sensor_data_point.bioz_sample = edata->bioz_samples[0];
    }
}

/*static void sensor_ecg_bioz_decode(uint8_t *buf, uint32_t buf_len)
{
    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)buf;
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;

    //printk("ECG NS: %d | B: %d ", edata->num_samples_ecg, edata->num_samples_bioz);
    uint8_t n_samples_ecg = edata->num_samples_ecg;
    uint8_t n_samples_bioz = edata->num_samples_bioz;

    if (n_samples_ecg > 8)
    {
        n_samples_ecg = 8;
    }

    if (n_samples_bioz > 4)
    {
        n_samples_bioz = 4;
    }

    if ((n_samples_ecg > 0) || (n_samples_bioz > 0))
    {
        ecg_bioz_sensor_sample.ecg_num_samples = n_samples_ecg;
        ecg_bioz_sensor_sample.bioz_num_samples = n_samples_bioz;

        for (int i = 0; i < n_samples_ecg; i++)
        {
            ecg_bioz_sensor_sample.ecg_samples[i] = edata->ecg_samples[i];
        }

        for (int i = 0; i < n_samples_bioz; i++)
        {
            ecg_bioz_sensor_sample.bioz_samples[i] = edata->bioz_samples[i];
        }

        ecg_bioz_sensor_sample.hr = edata->hr;

        if(k_msgq_put(&q_ecg_bioz_sample, &ecg_bioz_sensor_sample, K_MSEC(1))!=0)
        {
            LOG_ERR("ECG/BioZ sample queue error");
            //k_msgq_purge(&q_ecg_bioz_sample);
        }
    }
}*/


void work_sample_handler(struct k_work *work)
{
    uint8_t ecg_bioz_buf[512];
    uint8_t ppg_buf[64];

    int ret = sensor_read(&max30001_iodev, &max30001_read_rtio_poll_ctx, ecg_bioz_buf, sizeof(ecg_bioz_buf));
    if (ret < 0)
    {
        LOG_ERR("Error reading from MAX30001");
        //continue;
    }

    sensor_ecg_bioz_decode(ecg_bioz_buf, sizeof(ecg_bioz_buf));

    ret = sensor_read(&afe4400_iodev, &afe4400_read_rtio_poll_ctx, ppg_buf, sizeof(ppg_buf));
    if (ret < 0)
    {
        LOG_ERR("Error reading from AFE4400");
        //continue;
    }
    sensor_ppg_decode(ppg_buf, sizeof(ppg_buf));

    if (k_msgq_put(&q_hpi_data_sample, &hpi_sensor_data_point, K_NO_WAIT) != 0)
    {
        LOG_ERR("Unified sample queue error");
        // k_msgq_purge(&q_ecg_bioz_sample);
    }
}

K_WORK_DEFINE(work_sample, work_sample_handler);

void sample_all_handler(struct k_timer *dummy)
{
    k_work_submit(&work_sample);
}

K_TIMER_DEFINE(tmr_sensor_sample_all, sample_all_handler, NULL);

void hpi_sensor_read_all_thread(void)
{
    int ret;
    //uint8_t ecg_bioz_buf[512];
    //uint8_t ppg_buf[64];

    k_sem_take(&sem_ecg_bioz_thread_start, K_FOREVER);
    LOG_INF("Sensor Read All Thread starting");

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

K_THREAD_DEFINE(hpi_sensor_read_all_thread_id, 4096, hpi_sensor_read_all_thread, NULL, NULL, NULL, UNIFIED_SAMPLING_THREAD_PRIORITY, 0, 1000);

// K_THREAD_DEFINE(ecg_bioz_sample_trigger_thread_id, 4096, ecg_bioz_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);
// K_THREAD_DEFINE(ppg_sample_trigger_thread_id, 1024, ppg_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);
