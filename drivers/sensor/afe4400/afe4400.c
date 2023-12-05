// ProtoCentral Electronics (info@protocentral.com)
// SPDX-License-Identifier: Apache-2.0

#define DT_DRV_COMPAT ti_afe4400

#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#include "afe4400.h"

LOG_MODULE_REGISTER(SENSOR_AFE4400, CONFIG_SENSOR_LOG_LEVEL);

#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 0
#warning "AFE4400 driver enabled without any devices"
#endif

#define AFE4400_SPI_OPERATION (SPI_WORD_SET(8) | SPI_TRANSFER_MSB | \
                               SPI_MODE_CPOL | SPI_MODE_CPHA)

static int _afe4400_reg_write(const struct device *dev, uint8_t reg, uint32_t val)
{
    const struct afe4400_config *config = dev->config;
    uint8_t cmd[] = {(reg), (uint8_t)(val >> 16), (uint8_t)(val >> 8), (uint8_t)val};

    const struct spi_buf tx_buf = {.buf = cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    int ret;

    ret = spi_write_dt(&config->spi, &tx);
    if (ret)
    {
        LOG_DBG("spi_write FAIL %d\n", ret);
        return ret;
    }
    return 0;
}

static uint32_t _afe4400_read_reg(const struct device *dev, uint8_t reg)
{
    const struct afe4400_config *config = dev->config;
    uint8_t spiTxCommand = reg;
    ;

    uint8_t buf[3];

    const struct spi_buf tx_buf[1] = {{.buf = &spiTxCommand, .len = 1}};
    const struct spi_buf_set tx = {.buffers = tx_buf, .count = 1};
    struct spi_buf rx_buf[2] = {{.buf = NULL, .len = 1}, {.buf = buf, .len = 3}}; // 24 bit register + 1 dummy byte
    const struct spi_buf_set rx = {.buffers = rx_buf, .count = 2};

    spi_transceive_dt(&config->spi, &tx, &rx);

    return (uint32_t)(buf[0] << 16) | (buf[1] << 8) | buf[2];
}

static int afe4400_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct afe4400_data *drv_data = dev->data;

    _afe4400_reg_write(dev, CONTROL0, 0x000001);
    uint32_t led1val = _afe4400_read_reg(dev, LED1VAL);
    led1val = (uint32_t) (led1val << 8);
    int32_t led1val_signed = (int32_t) led1val;
    drv_data->raw_sample_ir = led1val_signed;

    _afe4400_reg_write(dev, CONTROL0, 0x000001);
    uint32_t led2val = _afe4400_read_reg(dev, LED2VAL);
    led2val = (uint32_t) (led2val << 8);
    int32_t led2val_signed = (int32_t) led2val;
    drv_data->raw_sample_red = led2val_signed;

    return 0;
}

static int afe4400_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct afe4400_data *drv_data = dev->data;

    switch (chan)
    {
    case SENSOR_CHAN_RED:
        val->val1 = drv_data->raw_sample_red;
        val->val2 = 0;
        break;
    case SENSOR_CHAN_IR:
        val->val1 = drv_data->raw_sample_ir;
        val->val2 = 0;
        break;
    default:
        return -ENOTSUP;
    }

    return 0;
}

static const struct sensor_driver_api afe4400_api_funcs = {
    .sample_fetch = afe4400_sample_fetch,
    .channel_get = afe4400_channel_get,
};

static int afe4400_chip_init(const struct device *dev)
{
    const struct afe4400_config *config = dev->config;
    int err;

    err = spi_is_ready_dt(&config->spi);
    if (err < 0)
    {
        LOG_DBG("bus check failed: %d", err);
        return err;
    }

    if (!device_is_ready(config->pwdn_gpio.port))
    {
        LOG_ERR("GPIO device not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->pwdn_gpio, GPIO_OUTPUT);

    gpio_pin_set_dt(&config->pwdn_gpio, 1);
    k_sleep(K_MSEC(100));
    gpio_pin_set_dt(&config->pwdn_gpio, 0);
    k_sleep(K_MSEC(100));

    _afe4400_reg_write(dev, CONTROL0, 0x000000);
    _afe4400_reg_write(dev, CONTROL0, 0x000008);
    _afe4400_reg_write(dev, TIAGAIN, 0x000000); // CF = 5pF, RF = 500kR
    _afe4400_reg_write(dev, TIA_AMB_GAIN, 0x000001);
    _afe4400_reg_write(dev, LEDCNTRL, 0x001414);
    _afe4400_reg_write(dev, CONTROL2, 0x000000); // LED_RANGE=100mA, LED=50mA
    _afe4400_reg_write(dev, CONTROL1, 0x010707); // Timers ON, average 3 samples
    _afe4400_reg_write(dev, PRPCOUNT, 0X001F3F);
    _afe4400_reg_write(dev, LED2STC, 0X001770);
    _afe4400_reg_write(dev, LED2ENDC, 0X001F3E);
    _afe4400_reg_write(dev, LED2LEDSTC, 0X001770);
    _afe4400_reg_write(dev, LED2LEDENDC, 0X001F3F);
    _afe4400_reg_write(dev, ALED2STC, 0X000000);
    _afe4400_reg_write(dev, ALED2ENDC, 0X0007CE);
    _afe4400_reg_write(dev, LED2CONVST, 0X000002);
    _afe4400_reg_write(dev, LED2CONVEND, 0X0007CF);
    _afe4400_reg_write(dev, ALED2CONVST, 0X0007D2);
    _afe4400_reg_write(dev, ALED2CONVEND, 0X000F9F);
    _afe4400_reg_write(dev, LED1STC, 0X0007D0);
    _afe4400_reg_write(dev, LED1ENDC, 0X000F9E);
    _afe4400_reg_write(dev, LED1LEDSTC, 0X0007D0);
    _afe4400_reg_write(dev, LED1LEDENDC, 0X000F9F);
    _afe4400_reg_write(dev, ALED1STC, 0X000FA0);
    _afe4400_reg_write(dev, ALED1ENDC, 0X00176E);
    _afe4400_reg_write(dev, LED1CONVST, 0X000FA2);
    _afe4400_reg_write(dev, LED1CONVEND, 0X00176F);
    _afe4400_reg_write(dev, ALED1CONVST, 0X001772);
    _afe4400_reg_write(dev, ALED1CONVEND, 0X001F3F);
    _afe4400_reg_write(dev, ADCRSTCNT0, 0X000000);
    _afe4400_reg_write(dev, ADCRSTENDCT0, 0X000000);
    _afe4400_reg_write(dev, ADCRSTCNT1, 0X0007D0);
    _afe4400_reg_write(dev, ADCRSTENDCT1, 0X0007D0);
    _afe4400_reg_write(dev, ADCRSTCNT2, 0X000FA0);
    _afe4400_reg_write(dev, ADCRSTENDCT2, 0X000FA0);
    _afe4400_reg_write(dev, ADCRSTCNT3, 0X001770);
    _afe4400_reg_write(dev, ADCRSTENDCT3, 0X001770);

    //printk("\nafe4400_chip_init\n");

    /* Wait for the sensor to be ready */
    k_sleep(K_MSEC(1));

    LOG_DBG("\"%s\" OK", dev->name);
    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int afe4400_pm_action(const struct device *dev,
                             enum pm_device_action action)
{
    int ret = 0;

    switch (action)
    {
    case PM_DEVICE_ACTION_RESUME:
        /* Re-initialize the chip */
        // ret = bme280_chip_init(dev);
        break;
    case PM_DEVICE_ACTION_SUSPEND:
        /* Put the chip into sleep mode */
        /*ret = bme280_reg_write(dev,
            BME280_REG_CTRL_MEAS,
            BME280_CTRL_MEAS_OFF_VAL);
            */

        if (ret < 0)
        {
            LOG_DBG("CTRL_MEAS write failed: %d", ret);
        }
        break;
    default:
        return -ENOTSUP;
    }

    return ret;
}
#endif /* CONFIG_PM_DEVICE */

/*
 * Main instantiation macro, which selects the correct bus-specific
 * instantiation macros for the instance.
 */

#define AFE4400_DEFINE(inst)                                             \
    static struct afe4400_data afe4400_data_##inst;                      \
    static const struct afe4400_config afe4400_config_##inst =           \
        {                                                                \
            .spi = SPI_DT_SPEC_INST_GET(                                 \
                inst, AFE4400_SPI_OPERATION, 0),                         \
            .pwdn_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, pwdn_gpios, {0}) \
                                                                         \
    };                                                                   \
    PM_DEVICE_DT_INST_DEFINE(inst, afe4400_pm_action);                   \
                                                                         \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                                   \
                                 afe4400_chip_init,                      \
                                 PM_DEVICE_DT_INST_GET(inst),            \
                                 &afe4400_data_##inst,                   \
                                 &afe4400_config_##inst,                 \
                                 POST_KERNEL,                            \
                                 CONFIG_SENSOR_INIT_PRIORITY,            \
                                 &afe4400_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(AFE4400_DEFINE)