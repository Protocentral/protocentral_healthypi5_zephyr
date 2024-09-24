#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "max30001.h"
#include "afe4400.h"

#include "sampling_module.h"
#include "sys_sm_module.h"

// #include "display_screens.h"
LOG_MODULE_REGISTER(sampling_module, CONFIG_SENSOR_LOG_LEVEL);

extern const struct device *const max30001_dev;
extern const struct device *const afe4400_dev;
extern const struct device *const max30205_dev;

#define SAMPLING_INTERVAL_MS 6           // Time between samples in milliseconds
#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

#define PPG_SAMPLING_INTERVAL_MS 1
#define ECG_SAMPLING_INTERVAL_MS 60

SENSOR_DT_READ_IODEV(max30001_iodev, DT_ALIAS(max30001), SENSOR_CHAN_VOLTAGE);
SENSOR_DT_READ_IODEV(afe4400_iodev, DT_ALIAS(afe4400), SENSOR_CHAN_RED);

RTIO_DEFINE_WITH_MEMPOOL(max30001_read_rtio_ctx,
                         32,  /* submission queue size */
                         32,  /* completion queue size */
                         64, /* number of memory blocks */
                         64,  /* size of each memory block */
                         4    /* memory alignment */
);

RTIO_DEFINE_WITH_MEMPOOL(afe4400_read_rtio_ctx,
                         32,  /* submission queue size */
                         32,  /* completion queue size */
                         64, /* number of memory blocks */
                         64,  /* size of each memory block */
                         4    /* memory alignment */
);

K_MSGQ_DEFINE(q_ecg_bioz_sample, sizeof(struct hpi_ecg_bioz_sensor_data_t), 64, 1);
K_MSGQ_DEFINE(q_ppg_sample, sizeof(struct hpi_ppg_sensor_data_t), 64, 1);

SENSOR_DT_READ_IODEV(max30001_iodev, DT_ALIAS(max30001), SENSOR_CHAN_VOLTAGE);
SENSOR_DT_READ_IODEV(afe4400_iodev, DT_ALIAS(afe4400), SENSOR_CHAN_RED);

RTIO_DEFINE_WITH_MEMPOOL(max30001_read_rtio_ctx,
                         32,  /* submission queue size */
                         32,  /* completion queue size */
                         64, /* number of memory blocks */
                         128,  /* size of each memory block */
                         4    /* memory alignment */
);

RTIO_DEFINE_WITH_MEMPOOL(afe4400_read_rtio_ctx,
                         32,  /* submission queue size */
                         32,  /* completion queue size */
                         64, /* number of memory blocks */
                         128,  /* size of each memory block */
                         4    /* memory alignment */
);

K_MSGQ_DEFINE(q_ecg_bioz_sample, sizeof(struct hpi_ecg_bioz_sensor_data_t), 64, 1);
K_MSGQ_DEFINE(q_ppg_sample, sizeof(struct hpi_ppg_sensor_data_t), 64, 1);

/*
static void sampling_thread(void)

{
    printk("Sampling Thread starting\n");

    int sample_count = 0;

    int32_t last_read_temp_value = 0;

    for (;;)
    {
        k_sleep(K_MSEC(SAMPLING_INTERVAL_MS));

        //struct sensor_value red_sample;
        //struct sensor_value ir_sample;

        
        sensor_sample_fetch(afe4400_dev);
        sensor_channel_get(afe4400_dev, SENSOR_CHAN_RED, &red_sample);
        sensor_channel_get(afe4400_dev, SENSOR_CHAN_IR, &ir_sample);

        struct hpi_sensor_data_t sensor_sample;


        sensor_sample.raw_red = red_sample.val1;
        sensor_sample.raw_ir = ir_sample.val1;
        sensor_sample.temp = last_read_temp_value;
        sensor_sample._bioZSkipSample = false;
        sensor_sample.rtor = rtor_sample.val1;
        sensor_sample.hr = hr_sample.val1;
        

        if (sample_count >= TEMP_SAMPLING_INTERVAL_COUNT)
        {
            sample_count = 0;
            sensor_sample_fetch(max30205_dev);
            struct sensor_value temp_sample;
            sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);
            // Convert to degree F
            if (temp_sample.val1 > 0)
            {
                last_read_temp_value = temp_sample.val1; //(temp_sample.val1 * 9 / 5) + 32000;
            }
            // printk("Temp: %d\n", last_read_temp_value);
        }
        else
        {
            sample_count++;
        }

        // printk("%d ", sensor_sample.ecg_sample);

        // k_msgq_put(&q_sample, &sensor_sample, K_NO_WAIT);
    }
}
*/

static volatile int hpi_sampling_ppg_sample_count = 0;

static void sensor_ppg_process_cb(int result, uint8_t *buf, uint32_t buf_len, void *userdata)
{
    const struct afe4400_encoded_data *edata = (const struct afe4400_encoded_data *)buf;

    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    if (hpi_sampling_ppg_sample_count < (PPG_POINTS_PER_SAMPLE))
    {
        ppg_sensor_sample.ppg_red_samples[hpi_sampling_ppg_sample_count] = edata->raw_sample_red;
        ppg_sensor_sample.ppg_ir_samples[hpi_sampling_ppg_sample_count] = edata->raw_sample_ir;

        hpi_sampling_ppg_sample_count++;
    }
    else
    {
        k_msgq_put(&q_ppg_sample, &ppg_sensor_sample, K_MSEC(1));
        hpi_sampling_ppg_sample_count = 0;

        ppg_sensor_sample.ppg_red_samples[hpi_sampling_ppg_sample_count++] = edata->raw_sample_red;
        ppg_sensor_sample.ppg_ir_samples[hpi_sampling_ppg_sample_count++] = edata->raw_sample_ir;
    }
}

static void sensor_ecg_bioz_processing_cb(int result, uint8_t *buf,
                                          uint32_t buf_len, void *userdata)
{
    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)buf;
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;

    //printk("ECG NS: %d | B: %d ", edata->num_samples_ecg, edata->num_samples_bioz);

    if ((edata->num_samples_ecg > 0) || (edata->num_samples_bioz > 0))
    {
        ecg_bioz_sensor_sample.ecg_num_samples = edata->num_samples_ecg;
        ecg_bioz_sensor_sample.bioz_num_samples = edata->num_samples_bioz;

        for (int i = 0; i < edata->num_samples_ecg; i++)
        {
            ecg_bioz_sensor_sample.ecg_samples[i] = edata->ecg_samples[i];
        }

        for (int i = 0; i < edata->num_samples_bioz; i++)
        {
            ecg_bioz_sensor_sample.bioz_sample[i] = edata->bioz_samples[i];
        }

        ecg_bioz_sensor_sample.hr = edata->hr;

        k_msgq_put(&q_ecg_bioz_sample, &ecg_bioz_sensor_sample, K_MSEC(1));
    }
}

void ppg_sample_trigger_thread(void)
{
    LOG_INF("PPG Sampling Trigger Thread starting\n");

    for (;;)
    {
        sensor_read(&afe4400_iodev, &afe4400_read_rtio_ctx, NULL);
        sensor_processing_with_callback(&afe4400_read_rtio_ctx, sensor_ppg_process_cb);

        k_sleep(K_MSEC(PPG_SAMPLING_INTERVAL_MS));
    }
}

void ecg_bioz_sample_trigger_thread(void)
{
    LOG_INF("ECG/ BioZ Sampling Trigger Thread starting\n");

    for (;;)
    {
        sensor_read(&max30001_iodev, &max30001_read_rtio_ctx, NULL);
        sensor_processing_with_callback(&max30001_read_rtio_ctx, sensor_ecg_bioz_processing_cb);

        k_sleep(K_MSEC(ECG_SAMPLING_INTERVAL_MS));
    }
}

#define ECG_SAMPLING_THREAD_PRIORITY 3


K_THREAD_DEFINE(ecg_bioz_sample_trigger_thread_id, 2048, ecg_bioz_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);
//K_THREAD_DEFINE(ppg_sample_trigger_thread_id, 2048, ppg_sample_trigger_thread, NULL, NULL, NULL, ECG_SAMPLING_THREAD_PRIORITY, 0, 1000);

