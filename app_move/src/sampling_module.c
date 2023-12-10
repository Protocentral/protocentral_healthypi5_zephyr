#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "max30001.h"

#include "sampling_module.h"
#include "sys_sm_module.h"

#include "display_module.h"

extern const struct device *const max30001_dev;
extern const struct device *const max32664_dev;

#define SAMPLING_INTERVAL_MS 8

K_MSGQ_DEFINE(q_sample, sizeof(struct hpi_sensor_data_t), 100, 1);

void sampling_thread(void)
{
    printk("Sampling Thread starting\n");

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

#ifdef CONFIG_SENSOR_MAX30001
        sensor_sample_fetch(max30001_dev);
        sensor_channel_get(max30001_dev, SENSOR_CHAN_ECG_UV, &ecg_sample);
        sensor_channel_get(max30001_dev, SENSOR_CHAN_BIOZ_UV, &bioz_sample);
#endif 

/*
#ifdef CONFIG_SENSOR_MAX32664
        sensor_sample_fetch(max32664_dev);
        sensor_channel_get(max32664_dev, SENSOR_CHAN_RED, &red_sample);
        sensor_channel_get(max32664_dev, SENSOR_CHAN_IR, &ir_sample);
        
#endif
*/
        struct hpi_sensor_data_t sensor_sample;

        sensor_sample.ecg_sample = ecg_sample.val1;
        sensor_sample.bioz_sample = bioz_sample.val1;
        sensor_sample.raw_red = 0;//red_sample.val1;
        sensor_sample.raw_ir = 0;//ir_sample.val1;
        sensor_sample.temp = 0;
        sensor_sample._bioZSkipSample = false;

        k_msgq_put(&q_sample, &sensor_sample, K_NO_WAIT);

        // busy loop until next value should be grabbed
        // while (k_timer_status_get(&next_val_timer) <= 0);
    }
}

#define SAMPLING_THREAD_STACKSIZE 2048
#define SAMPLING_THREAD_PRIORITY 7

K_THREAD_DEFINE(sampling_thread_id, SAMPLING_THREAD_STACKSIZE, sampling_thread, NULL, NULL, NULL, SAMPLING_THREAD_PRIORITY, 0, 1000);
