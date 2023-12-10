/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max32664

#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/gpio.h>

#include "max32664.h"

LOG_MODULE_REGISTER(MAX32664, CONFIG_SENSOR_LOG_LEVEL);

#define MAX32664_CMD_DELAY 45

static int max32664_sample_fetch(const struct device *dev,
								 enum sensor_channel chan)
{
	// struct max32664_data *data = dev->data;
	// const struct max32664_config *config = dev->config;

	return 0;
}

static int max32664_channel_get(const struct device *dev,
								enum sensor_channel chan,
								struct sensor_value *val)
{
	/*struct max32664_data *data = dev->data;
	enum max32664_led_channel led_chan;
	int fifo_chan;

	switch (chan)
	{
	case SENSOR_CHAN_RED:
		led_chan = MAX30101_LED_CHANNEL_RED;
		break;

	case SENSOR_CHAN_IR:
		led_chan = MAX30101_LED_CHANNEL_IR;
		break;

	case SENSOR_CHAN_GREEN:
		led_chan = MAX30101_LED_CHANNEL_GREEN;
		break;

	default:
		LOG_ERR("Unsupported sensor channel");
		return -ENOTSUP;
	}

	/* Check if the led channel is active by looking up the associated fifo
	 * channel. If the fifo channel isn't valid, then the led channel
	 * isn't active.
	 */

	return 0;
}

static const struct sensor_driver_api max32664_driver_api = {
	.sample_fetch = max32664_sample_fetch,
	.channel_get = max32664_channel_get,
};

static int m_read_op_mode(const struct device *dev)
{
	// struct max32664_data *data = dev->data;
	const struct max32664_config *config = dev->config;
	uint8_t rd_buf[2]={0x00,0x00};

	uint8_t wr_buf[2] = {0x02, 0x00};

	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));

	k_sleep(K_MSEC(45));

	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(45));
	gpio_pin_set_dt(&config->mfio_gpio, 1);

	printk("Op mode = %x\n", rd_buf[0]);

	return 0;
}


static int m_get_ver(const struct device *dev)
{
	const struct max32664_config *config = dev->config;
	uint8_t rd_buf[4]={0x00,0x00,0x00,0x00};

	uint8_t wr_buf[2] = {0xFF, 0x03};

	//gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, sizeof(wr_buf));
	//gpio_pin_set_dt(&config->mfio_gpio, 1);

	k_sleep(K_MSEC(MAX32664_CMD_DELAY));

	//gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_read_dt(&config->i2c, rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(MAX32664_CMD_DELAY));
	//gpio_pin_set_dt(&config->mfio_gpio, 1);

	//printk("Response = %x\n", rd_buf[0]);

	printk("Version = %x %x %x %x\n", rd_buf[0], rd_buf[1], rd_buf[2], rd_buf[3]);

	return 0;
}

static int m_i2c_write_cmd_4(const struct device *dev, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
	const struct max32664_config *config = dev->config;
	uint8_t buffer[4];
	uint8_t rd_buf[1];

	buffer[0] = byte1;
	buffer[1] = byte2;
	buffer[2] = byte3;
	buffer[3] = byte4;

	//gpio_pin_set_dt(&config->mfio_gpio, 0);
	//k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, buffer, sizeof(buffer));
	//gpio_pin_set_dt(&config->mfio_gpio, 1);

	k_sleep(K_MSEC(MAX32664_CMD_DELAY));

	//gpio_pin_set_dt(&config->mfio_gpio, 0);
	//k_sleep(K_USEC(300));
	i2c_read_dt(&config->i2c, rd_buf, 1);
	//k_sleep(K_MSEC(MAX32664_CMD_DELAY));
	//gpio_pin_set_dt(&config->mfio_gpio, 1);

	printk("Response = %x\n", rd_buf[0]);

	return 0;
}

static int m_i2c_write_cmd_3(const struct device *dev, uint8_t byte1, uint8_t byte2, uint8_t byte3)
{
	const struct max32664_config *config = dev->config;
	uint8_t wr_reg[3];

	uint8_t rd_buf[2]={0x00,0x00};

	wr_reg[0] = byte1;
	wr_reg[1] = byte2;
	wr_reg[2] = byte3;

	
	/*gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_reg, 3);
	gpio_pin_set_dt(&config->mfio_gpio, 1);

	k_sleep(K_MSEC(2));

	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_read_dt(&config->i2c, rd_buf, 1);
	k_sleep(K_MSEC(2));
	gpio_pin_set_dt(&config->mfio_gpio, 1);
	*/
	
	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_write_read_dt(&config->i2c, wr_reg, sizeof(wr_reg), rd_buf, sizeof(rd_buf));
	k_sleep(K_MSEC(2));
	gpio_pin_set_dt(&config->mfio_gpio, 1);
	

	printk("Response = %x %x\n", rd_buf[0], rd_buf[1]);

	k_sleep(K_MSEC(45));

	return 0;
}

static int m_read_accel_whoami(const struct device *dev)
{
	// struct max32664_data *data = dev->data;
	const struct max32664_config *config = dev->config;
	uint8_t rd_buf[2] = {0x00, 0x00};

	uint8_t wr_buf[3] = {0x41, 0x04, 0x0F};

	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_write_dt(&config->i2c, wr_buf, 3);
	gpio_pin_set_dt(&config->mfio_gpio, 1);

	k_sleep(K_MSEC(2));

	gpio_pin_set_dt(&config->mfio_gpio, 0);
	k_sleep(K_USEC(300));
	i2c_read_dt(&config->i2c, rd_buf, 2);
	k_sleep(K_MSEC(2));
	gpio_pin_set_dt(&config->mfio_gpio, 1);

	printk("Accel WHOAMI: %x %x\n", rd_buf[0], rd_buf[1]);

	return 0;
}

static int max32664_chip_init(const struct device *dev)
{
	const struct max32664_config *config = dev->config;
	// struct max32664_data *data = dev->data;

	if (!device_is_ready(config->i2c.bus))
	{
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT);
	gpio_pin_configure_dt(&config->mfio_gpio, GPIO_OUTPUT);

	// Enter application mdoe
	gpio_pin_set_dt(&config->reset_gpio, 0);
	gpio_pin_set_dt(&config->mfio_gpio, 1);
	k_sleep(K_MSEC(10));
	gpio_pin_set_dt(&config->reset_gpio, 1);
	k_sleep(K_MSEC(1500));
	
	gpio_pin_configure_dt(&config->mfio_gpio, GPIO_INPUT);

	m_read_op_mode(dev);
	m_get_ver(dev);

	// uint8_t wr_buf[4]={0xAA,0x10,0x00,0x01};
	// i2c_write_dt(&config->i2c, wr_buf, 4);

	// Read raw sensor data
	//m_i2c_write_cmd_3(dev, 0x10, 0x00, 0x01);
	//k_sleep(K_MSEC(10));

	// Set interrupt threshold
	//m_i2c_write_cmd_3(dev, 0x10, 0x01, 0x0F);
	k_sleep(K_MSEC(10));

	// Enable Accel
	// m_i2c_write_cmd_4(dev, 0x44, 0x04, 0x01, 0x00);
	// k_sleep(K_MSEC(10));

	// Enable AFE
	//m_i2c_write_cmd_3(dev, 0x44, 0x03, 0x01);
	k_sleep(K_MSEC(10));

	//m_i2c_write_cmd_3(dev, 0x52, 0x04, 0x02);
	k_sleep(K_MSEC(10));

	//m_i2c_write_cmd_3(dev, 0x52, 0x00, 0x00);
	k_sleep(K_MSEC(200));

	//m_i2c_write_cmd_4(dev, 0x40, 0x03, 0x0C, 0x7F);
	k_sleep(K_MSEC(200));
	//m_i2c_write_cmd_4(dev, 0x40, 0x03, 0x0D,0x7F);
	k_sleep(K_MSEC(200));
	// m_read_accel_whoami(dev);

	/*m_i2c_write_cmd_4(dev, 0x40, 0x00, 0x12, 0x18);
	k_sleep(K_MSEC(10));

	m_i2c_write_cmd_4(dev, 0x40, 0x00, 0x23, 0x7F);
	k_sleep(K_MSEC(10));

	m_i2c_write_cmd_4(dev, 0x40, 0x00, 0x24, 0x7F);
	k_sleep(K_MSEC(50));

	m_i2c_write_cmd_4(dev, 0x40, 0x00, 0x25, 0x7F);
	k_sleep(K_MSEC(50));
	*/

	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int max32664_pm_action(const struct device *dev,
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

/*static struct max32664_config max32664_config = {
	.i2c = I2C_DT_SPEC_INST_GET(0),
	.reset_gpio = GPIO_DT_SPEC_INST_GET(0, reset_gpios),
	.mfio_gpio = GPIO_DT_SPEC_INST_GET(0, mfio_gpios),
};
*/

// static struct max32664_data max32664_data;

/*
 * Main instantiation macro, which selects the correct bus-specific
 * instantiation macros for the instance.
 */
#define MAX32664_DEFINE(inst)                                    \
	static struct max32664_data max32664_data_##inst;            \
	static const struct max32664_config max32664_config_##inst = \
		{                                                        \
			.i2c = I2C_DT_SPEC_INST_GET(0),                      \
			.reset_gpio = GPIO_DT_SPEC_INST_GET(0, reset_gpios), \
			.mfio_gpio = GPIO_DT_SPEC_INST_GET(0, mfio_gpios),   \
	};                                                           \
	PM_DEVICE_DT_INST_DEFINE(inst, max32664_pm_action);          \
	SENSOR_DEVICE_DT_INST_DEFINE(inst,                           \
								 max32664_chip_init,             \
								 PM_DEVICE_DT_INST_GET(inst),    \
								 &max32664_data_##inst,          \
								 &max32664_config_##inst,        \
								 POST_KERNEL,                    \
								 CONFIG_SENSOR_INIT_PRIORITY,    \
								 &max32664_driver_api)

DT_INST_FOREACH_STATUS_OKAY(MAX32664_DEFINE)
