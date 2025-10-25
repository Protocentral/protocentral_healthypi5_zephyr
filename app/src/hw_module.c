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
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pwm.h>

#include <stdio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/zbus/zbus.h>

#include "max30001.h"
#include "hw_module.h"
#include "fs_module.h"
#include "hpi_common_types.h"

#ifdef CONFIG_DISPLAY
#include "display_module.h"
#endif

#include "ble_module.h"

LOG_MODULE_REGISTER(hw_module, LOG_LEVEL_INF);

ZBUS_CHAN_DECLARE(temp_chan, batt_chan);
K_SEM_DEFINE(sem_hw_inited, 0, 1);

// GPIO LEDs
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(ledgreen), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(ledblue), gpios);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct device *usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

// USB buffer monitoring
static uint32_t usb_buffer_drops = 0;
static uint32_t usb_buffer_writes = 0;
static uint32_t last_usb_log_time = 0;

static const struct device *const gpio_keys_dev = DEVICE_DT_GET_ANY(gpio_keys);
static const struct device *const longpress_dev = DEVICE_DT_GET(DT_NODELABEL(longpress));
uint8_t m_key_pressed = GPIO_KEYPAD_KEY_NONE;

static bool rx_throttled;

K_SEM_DEFINE(sem_up_key_pressed, 0, 1);
K_SEM_DEFINE(sem_down_key_pressed, 0, 1);
K_SEM_DEFINE(sem_ok_key_pressed, 0, 1);
K_SEM_DEFINE(sem_ok_key_longpress, 0, 1);  // Separate semaphore for long press

K_SEM_DEFINE(sem_ecg_bioz_thread_start, 0, 1);

// USB CDC UART
// 6KB buffer - balanced between memory usage and USB throughput
// At 125Hz with ~29 bytes/packet = ~3.6KB/sec, 6KB provides ~1.7 second buffer
// This handles USB enumeration delays and brief host-side pauses
#define RING_BUF_SIZE 6144
uint8_t ring_buffer[RING_BUF_SIZE];
struct ring_buf ringbuf_usb_cdc;

// Peripheral Device Pointers
const struct device *fg_dev;
const struct device *const max30001_dev = DEVICE_DT_GET_ANY(maxim_max30001);
const struct device *const afe4400_dev = DEVICE_DT_GET_ANY(ti_afe4400);
const struct device *const max30205_dev = DEVICE_DT_GET_ANY(maxim_max30205);
const struct device *fg_dev;

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
static const struct pwm_dt_spec bl_led_pwm = PWM_DT_SPEC_GET(DT_ALIAS(bl_led_pwm));
#endif

uint8_t global_batt_level = 0;

/*******EXTERNS******/
extern struct k_msgq q_session_cmd_msg;

/*bool settings_send_usb_enabled = true;
bool settings_send_ble_enabled = true;
bool settings_send_display_enabled = false;*/

static void leds_init()
{
    int ret;
    // Setup LED devices
    if (!device_is_ready(led_blue.port))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return;
    }

    if (!device_is_ready(led_green.port))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        return;
    }

    gpio_pin_set_dt(&led_green, 0);
}

void send_usb_cdc(const char *buf, size_t len)
{
    // Check if buffer has enough space before attempting write
    uint32_t space = ring_buf_space_get(&ringbuf_usb_cdc);
    
    if (space < len) {
        // Buffer full - drop packet silently to avoid USB stalls
        // Don't log here as logging during overflow makes it worse
        usb_buffer_drops++;
        return;
    }
    
    int rb_len = ring_buf_put(&ringbuf_usb_cdc, buf, len);
    usb_buffer_writes++;
    
    // Periodic USB buffer health logging (every 30 seconds)
    uint32_t current_time = k_uptime_get_32();
    if (current_time - last_usb_log_time >= 30000) {
        uint32_t used = ring_buf_size_get(&ringbuf_usb_cdc);
        uint32_t capacity = ring_buf_capacity_get(&ringbuf_usb_cdc);
        uint32_t drop_rate = (usb_buffer_drops * 100) / (usb_buffer_writes + usb_buffer_drops);
        
        LOG_INF("USB CDC: %u/%u bytes, %u drops (%u%%)",
                used, capacity, usb_buffer_drops, drop_rate);
        
        last_usb_log_time = current_time;
    }
    
    // Only enable TX interrupt if data was successfully written
    if (rb_len > 0) {
        uart_irq_tx_enable(usb_dev);
    }
}

static void interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
        if (!rx_throttled && uart_irq_rx_ready(dev))
        {
            int recv_len, rb_len;
            uint8_t buffer[128];  // Increased from 64 to 128 bytes
            size_t len = MIN(ring_buf_space_get(&ringbuf_usb_cdc),
                             sizeof(buffer));

            if (len == 0)
            {
                /* Throttle because ring buffer is full */
                uart_irq_rx_disable(dev);
                rx_throttled = true;
                continue;
            }

            recv_len = uart_fifo_read(dev, buffer, len);
            if (recv_len < 0)
            {
                LOG_ERR("Failed to read UART FIFO");
                recv_len = 0;
            };

            rb_len = ring_buf_put(&ringbuf_usb_cdc, buffer, recv_len);
            if (rb_len < recv_len)
            {
                LOG_ERR("Drop %u bytes", recv_len - rb_len);
            }

            LOG_DBG("tty fifo -> ringbuf %d bytes", rb_len);
            if (rb_len)
            {
                uart_irq_tx_enable(dev);
            }
        }

        if (uart_irq_tx_ready(dev))
        {
            uint8_t buffer[128];  // Increased from 64 to 128 bytes
            int rb_len, send_len;

            rb_len = ring_buf_get(&ringbuf_usb_cdc, buffer, sizeof(buffer));
            if (!rb_len)
            {
                LOG_DBG("Ring buffer empty, disable TX IRQ");
                uart_irq_tx_disable(dev);
                continue;
            }

            if (rx_throttled)
            {
                uart_irq_rx_enable(dev);
                rx_throttled = false;
            }

            send_len = uart_fifo_fill(dev, buffer, rb_len);
            if (send_len < rb_len)
            {
                LOG_ERR("Drop %d bytes", rb_len - send_len);
            }

            LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
        }
    }
}

void update_battery_level(void)
{
}

static void usb_init()
{
    int ret = 0;

#ifdef CONFIG_HEALTHYPI_USB_CDC_ENABLED

    if (!device_is_ready(usb_dev))
    {
        LOG_ERR("CDC ACM device not ready");
        // return;
    }

    ret = usb_enable(NULL);
    if (ret != 0)
    {
        LOG_ERR("Failed to enable USB");
        // return;
    }

    /* Enabled USB CDC interrupts */

    ring_buf_init(&ringbuf_usb_cdc, sizeof(ring_buffer), ring_buffer);
    k_msleep(100);
    uart_irq_callback_set(usb_dev, interrupt_handler);
    uart_irq_rx_enable(usb_dev);

#endif

#ifdef CONFIG_HEALTHYPI_USB_MSC_ENABLED

#endif

    LOG_INF("USB Init complete");
}

int hpi_hw_read_temp(float* temp_f, float* temp_c)
{
    struct sensor_value temp_sample;
    sensor_sample_fetch(max30205_dev);
    sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);

    if (temp_sample.val1 < 0)
        return 0;

    // Convert to degree F

    *temp_c = (double)temp_sample.val1 / 1000;
    *temp_f = (*temp_c * 1.8) + 32.0;

    return 0;
}

uint8_t hpi_hw_read_batt(void)
{
    int ret = 0;
    uint8_t batt_level = 0;

    fuel_gauge_prop_t props[] = {
        FUEL_GAUGE_RUNTIME_TO_EMPTY,
        FUEL_GAUGE_RUNTIME_TO_FULL,
        FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
        FUEL_GAUGE_VOLTAGE,
    };
    union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];

    ret = fuel_gauge_get_props(fg_dev, props, vals, ARRAY_SIZE(props));

    if (ret < 0)
    {
        LOG_ERR("Error: cannot get properties\n");
    }
    else
    {
        // LOG_DBG("Charge %d%% TTE: %d Voltage: %d \n", vals[2].relative_state_of_charge, vals[0].runtime_to_empty, (vals[3].voltage));
        batt_level = vals[2].relative_state_of_charge;
    }

    return batt_level;
}

static void gpio_keys_cb_handler(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    if (evt->value == 1)  // Button pressed
    {
        switch (evt->code)
        {
        case INPUT_KEY_ENTER:
            LOG_INF("OK Key Pressed (short)");
            k_sem_give(&sem_ok_key_pressed);
            break;
        case INPUT_BTN_0:  // Long press sends BTN_0
            LOG_INF("OK Key Long-Pressed (via longpress driver)");
            k_sem_give(&sem_ok_key_longpress);
            break;
        default:
            LOG_DBG("Unknown input code from longpress: %d, value: %d", evt->code, evt->value);
            break;
        }
    }
#endif
}

// Separate callback for UP/DOWN buttons (direct from gpio_keys)
static void gpio_updown_cb_handler(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    if (evt->value == 1)  // Button pressed
    {
        switch (evt->code)
        {
        case INPUT_KEY_UP:
            LOG_INF("UP Key Pressed");
            k_sem_give(&sem_up_key_pressed);
            break;
        case INPUT_KEY_DOWN:
            LOG_INF("DOWN Key Pressed");
            k_sem_give(&sem_down_key_pressed);
            break;
        default:
            // Ignore other keys (ENTER is handled by longpress driver)
            break;
        }
    }
#endif
}

// Register callback on longpress device output (for OK button short/long press)
INPUT_CALLBACK_DEFINE(longpress_dev, gpio_keys_cb_handler, NULL);
// Register callback on gpio_keys device (for UP/DOWN buttons)
INPUT_CALLBACK_DEFINE(gpio_keys_dev, gpio_updown_cb_handler, NULL);

void hw_thread(void)
{
    if (!device_is_ready(max30001_dev))
    {
        LOG_ERR("MAX30001 device not found! Rebooting !");
    }
    else
    {
        struct sensor_value ecg_mode_set;

        ecg_mode_set.val1 = 1;
        sensor_attr_set(max30001_dev, SENSOR_CHAN_ALL, MAX30001_ATTR_ECG_ENABLED, &ecg_mode_set);
        sensor_attr_set(max30001_dev, SENSOR_CHAN_ALL, MAX30001_ATTR_BIOZ_ENABLED, &ecg_mode_set);
    }

    if (!device_is_ready(afe4400_dev))
    {
        LOG_ERR("AFE4400 device not found!");
        // return;
    }

    if (!device_is_ready(max30205_dev))
    {
        LOG_ERR("MAX30205 device not found!");
        // return;
    }

    fg_dev = DEVICE_DT_GET_ANY(maxim_max17048);
    if (!device_is_ready(fg_dev))
    {
        LOG_ERR("Fuel Gauge device not found!");
    }

#ifdef CONFIG_BT
    ble_module_init();
#endif

    leds_init();
    fs_module_init();

    // init_settings();

    LOG_INF("HW Thread started");

    k_sem_give(&sem_hw_inited);

    usb_init();

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    // PWM for LCD Backlight
    if (!pwm_is_ready_dt(&bl_led_pwm))
    {
        LOG_ERR("Error: PWM device %s is not ready\n",
                bl_led_pwm.dev->name);
        // return 0;
    }

    int ret = pwm_set_pulse_dt(&bl_led_pwm, 6000);
    if (ret)
    {
        LOG_ERR("Error %d: failed to set pulse width\n", ret);
        // return 0;
    }
#endif

    k_sem_give(&sem_ecg_bioz_thread_start);

    float m_temp_f = 0;
    float m_temp_c = 0;

    for (;;)
    {
        // Sample slow changing sensors
        global_batt_level = hpi_hw_read_batt();

        struct hpi_batt_status_t batt_s = {
            .batt_level = (uint8_t) hpi_hw_read_batt(),
            .batt_charging = 0,
        };
        // Use K_NO_WAIT to prevent blocking threads
        zbus_chan_pub(&batt_chan, &batt_s, K_NO_WAIT);

        // Read and publish temperature
        hpi_hw_read_temp(&m_temp_f, &m_temp_c);
        struct hpi_temp_t temp = {
            .temp_f = m_temp_f,
            .temp_c = m_temp_c,
        };
        // Use K_NO_WAIT to prevent blocking threads
        zbus_chan_pub(&temp_chan, &temp, K_NO_WAIT);

        gpio_pin_toggle_dt(&led_blue);
        k_sleep(K_MSEC(1000));
    }
}

#define HW_THREAD_STACKSIZE 3072
#define HW_THREAD_PRIORITY 9

K_THREAD_DEFINE(hw_thread_id, HW_THREAD_STACKSIZE, hw_thread, NULL, NULL, NULL, HW_THREAD_PRIORITY, 0, 0);
