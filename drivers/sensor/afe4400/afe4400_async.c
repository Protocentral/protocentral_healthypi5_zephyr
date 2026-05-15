#define DT_DRV_COMPAT ti_afe4400

#include <zephyr/drivers/sensor.h>

#include "afe4400.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(SENSOR_AFE4400_ASYNC, CONFIG_SENSOR_LOG_LEVEL);

#define AFE4400_READ_BLOCK_SIZE 1

static int afe4400_async_sample_fetch(const struct device *dev, int32_t *raw_ir_sample, int32_t *raw_red_sample)
{
    static int32_t ambiant_light_buf[10];
    static int sample_count = 0;

    //every 5th samples, read ambiant light and put it in buffer
    if (sample_count % 5 == 0)
    {
        //Read ambient light value for LED1
        _afe4400_reg_write(dev, CONTROL0, 0x000001);
        uint32_t aledval1 = _afe4400_read_reg(dev, ALED1VAL);
        aledval1 = (uint32_t)(aledval1 << 10);
        int32_t aledval1_signed = (int32_t)aledval1 >> 10;

        //Read ambient light value for LED2
        _afe4400_reg_write(dev, CONTROL0, 0x000001);
        uint32_t aledval2 = _afe4400_read_reg(dev, ALED2VAL);
        aledval2 = (uint32_t)(aledval2 << 10);
        int32_t aledval2_signed = (int32_t)aledval2 >> 10;

        ambiant_light_buf[sample_count / 5] = (aledval1_signed + aledval2_signed) / 2;
    }

    //every 50th sample, compute average light, and use PI controller to adjust LED cancelation current
    if (sample_count==45)
    {
        int32_t ambiant_light_avg = 0;
        for (int i = 0; i < 10; i++)
        {
            ambiant_light_avg += ambiant_light_buf[i];
        }
        ambiant_light_avg /= 10;

        //simple PI controller
        int32_t error = ambiant_light_avg - 1000; // target ambient light value
        static int32_t integral = 0;
        integral += error;
        int32_t cancel_current = (error * 10 + integral * 1) / 1000; // Kp=0.01, Ki=0.001

        //limit cancel current to valid range
        if (cancel_current < 0)
            cancel_current = 0;
        if (cancel_current > 10)
            cancel_current = 10;

        //cancel current is bit 19 to 16 of TIA_AMB_GAIN register
        cancel_current <<= 16;
        _afe4400_reg_write(dev, TIA_AMB_GAIN, cancel_current | rf_cf_config);

        LOG_INF("Ambient light avg: %d, error: %d, cancel current: %d", ambiant_light_avg, error, cancel_current  >> 16);

        //Read LED1 and LED2 values to check saturation
        uint32_t led1_debug = _afe4400_read_reg(dev, LED1VAL);
        led1_debug = (uint32_t)(led1_debug << 10);
        int32_t led1s_debug = (int32_t)led1_debug >> 10;

        uint32_t led2_debug = _afe4400_read_reg(dev, LED2VAL);
        led2_debug = (uint32_t)(led2_debug << 10);
        int32_t led2s_debug = (int32_t)led2_debug >> 10;

        if (led1s_debug > 2000000 || led2s_debug > 2000000) {
            LOG_WRN("Sample saturation detected! LED1=%d, LED2=%d", led1s_debug, led2s_debug);
        }
    }

    _afe4400_reg_write(dev, CONTROL0, 0x000001);
    uint32_t led1val = _afe4400_read_reg(dev, LED1ABSVAL);
    /* Sign-extend AFE4400's signed 22-bit sample from 24-bit register payload. */
    led1val = (uint32_t)(led1val << 10);
    int32_t led1val_signed = (int32_t)led1val;
    *raw_ir_sample = (int32_t)led1val_signed >> 10;

    _afe4400_reg_write(dev, CONTROL0, 0x000001);
    uint32_t led2val = _afe4400_read_reg(dev, LED2ABSVAL);
    /* Sign-extend AFE4400's signed 22-bit sample from 24-bit register payload. */
    led2val = (uint32_t)(led2val << 10);
    int32_t led2val_signed = (int32_t)led2val;
    *raw_red_sample = (int32_t)led2val_signed >> 10;

    sample_count++;
    if (sample_count >= 46) sample_count = 0;

    return 0;
}

int afe4400_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe)
{
    uint32_t m_min_buf_len = sizeof(struct afe4400_encoded_data);

    uint8_t *buf;
    uint32_t buf_len;

    struct afe4400_encoded_data *m_edata;

    int ret = 0;

    ret = rtio_sqe_rx_buf(iodev_sqe, m_min_buf_len, m_min_buf_len, &buf, &buf_len);
    if (ret != 0)
    {
        LOG_ERR("Failed to get a read buffer of size %u bytes", m_min_buf_len);
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    m_edata = (struct afe4400_encoded_data *)buf;
    m_edata->header.timestamp = k_ticks_to_ns_floor64(k_uptime_ticks());
    ret = afe4400_async_sample_fetch(dev, &m_edata->raw_sample_ir, &m_edata->raw_sample_red);

    if (ret != 0)
    {
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    rtio_iodev_sqe_ok(iodev_sqe, 0);

    return 0;
}