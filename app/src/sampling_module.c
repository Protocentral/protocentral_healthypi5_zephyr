#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "max30001.h"
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

RTIO_DEFINE_WITH_MEMPOOL(max30001_read_rtio_ctx,
                         32,  /* submission queue size */
                         32,  /* completion queue size */
                         128, /* number of memory blocks */
                         64,  /* size of each memory block */
                         4    /* memory alignment */
);

K_MSGQ_DEFINE(q_sample, sizeof(struct hpi_sensor_data_t), 100, 1);

K_MSGQ_DEFINE(q_ecg_bioz_sample, sizeof(struct hpi_ecg_bioz_sensor_data_t), 100, 1);

static void sampling_thread(void)
{
    printk("Sampling Thread starting\n");

    int sample_count = 0;

    int32_t last_read_temp_value = 0;

    for (;;)
    {
        k_sleep(K_MSEC(SAMPLING_INTERVAL_MS));

        /*struct k_timer next_val_timer;
        k_timer_init(&next_val_timer, NULL, NULL);
        k_timer_start(&next_val_timer, K_USEC(time_between_samples_us), K_NO_WAIT);
        */

        struct sensor_value ecg_sample;
        struct sensor_value bioz_sample;

        struct sensor_value red_sample;
        struct sensor_value ir_sample;

        struct sensor_value rtor_sample;
        struct sensor_value hr_sample;

        /* sensor_sample_fetch(max30001_dev);
         sensor_channel_get(max30001_dev, SENSOR_CHAN_ECG_UV, &ecg_sample);
         sensor_channel_get(max30001_dev, SENSOR_CHAN_BIOZ_UV, &bioz_sample);
         sensor_channel_get(max30001_dev, SENSOR_CHAN_RTOR, &rtor_sample);
         sensor_channel_get(max30001_dev, SENSOR_CHAN_HR, &hr_sample);
         */

        sensor_sample_fetch(afe4400_dev);
        sensor_channel_get(afe4400_dev, SENSOR_CHAN_RED, &red_sample);
        sensor_channel_get(afe4400_dev, SENSOR_CHAN_IR, &ir_sample);

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

        struct hpi_sensor_data_t sensor_sample;

        sensor_sample.ecg_sample = ecg_sample.val1;
        sensor_sample.bioz_sample = bioz_sample.val1;
        sensor_sample.raw_red = red_sample.val1;
        sensor_sample.raw_ir = ir_sample.val1;
        sensor_sample.temp = last_read_temp_value;
        sensor_sample._bioZSkipSample = false;
        sensor_sample.rtor = rtor_sample.val1;
        sensor_sample.hr = hr_sample.val1;

        // printk("%d ", sensor_sample.ecg_sample);

        k_msgq_put(&q_sample, &sensor_sample, K_NO_WAIT);

        // busy loop until next value should be grabbed
        // while (k_timer_status_get(&next_val_timer) <= 0);
    }
}

static void sensor_ecg_processing_callback(int result, uint8_t *buf,
                                           uint32_t buf_len, void *userdata)
{
    const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)buf;
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;

    //printk("ECG NS: %d ", edata->num_samples_ecg);

    if (edata->num_samples_ecg > 0)
    {
        ecg_bioz_sensor_sample.ecg_num_samples = edata->num_samples_ecg;
        for (int i = 0; i < edata->num_samples_ecg; i++)
        {
            ecg_bioz_sensor_sample.ecg_samples[i] = edata->ecg_samples[i];
        }
        ecg_bioz_sensor_sample.hr = edata->hr;

        k_msgq_put(&q_ecg_bioz_sample, &ecg_bioz_sensor_sample, K_MSEC(1));
    }
}

void ecg_sampling_trigger_thread(void)
{
    LOG_INF("ECG/ BioZ Sampling Trigger Thread starting\n");

    for (;;)
    {
        // k_sem_take(&sem_ecg_intb_recd, K_FOREVER);

        sensor_read(&max30001_iodev, &max30001_read_rtio_ctx, NULL);
        sensor_processing_with_callback(&max30001_read_rtio_ctx, sensor_ecg_processing_callback);

        k_sleep(K_MSEC(ECG_SAMPLING_INTERVAL_MS));
    }
}

#define SAMPLING_THREAD_STACKSIZE 2048
#define SAMPLING_THREAD_PRIORITY 7

K_THREAD_DEFINE(ecg_sampling_trigger_thread_id, 2048, ecg_sampling_trigger_thread, NULL, NULL, NULL, SAMPLING_THREAD_PRIORITY, 0, 1000);

//K_THREAD_DEFINE(sampling_thread_id, SAMPLING_THREAD_STACKSIZE, sampling_thread, NULL, NULL, NULL, SAMPLING_THREAD_PRIORITY, 0, 1000);
