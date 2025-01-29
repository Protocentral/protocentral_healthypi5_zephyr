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

#include "max30001.h"
#include "hw_module.h"
#include "fs_module.h"
#include "sampling_module.h"

#ifdef CONFIG_DISPLAY
#include "display_module.h"
#endif

#include "ble_module.h"

LOG_MODULE_REGISTER(hw_module);
char curr_string[40];

/*******EXTERNS******/
extern struct k_msgq q_session_cmd_msg;

K_SEM_DEFINE(sem_hw_inited, 0, 1);

/****END EXTERNS****/

#define HW_THREAD_STACKSIZE 4096
#define HW_THREAD_PRIORITY 7

// Peripheral Device Pointers
const struct device *fg_dev;

// GPIO LEDs
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(ledgreen), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(ledblue), gpios);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct device *usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

static const struct device *const gpio_keys_dev = DEVICE_DT_GET_ANY(gpio_keys);
uint8_t m_key_pressed = GPIO_KEYPAD_KEY_NONE;

static bool rx_throttled;

K_SEM_DEFINE(sem_up_key_pressed, 0, 1);
K_SEM_DEFINE(sem_down_key_pressed, 0, 1);
K_SEM_DEFINE(sem_ok_key_pressed, 0, 1);

// USB CDC UART
#define RING_BUF_SIZE 1024
uint8_t ring_buffer[RING_BUF_SIZE];
struct ring_buf ringbuf_usb_cdc;

const struct device *const max30001_dev = DEVICE_DT_GET_ANY(maxim_max30001);
const struct device *const afe4400_dev = DEVICE_DT_GET_ANY(ti_afe4400);
const struct device *const max30205_dev = DEVICE_DT_GET_ANY(maxim_max30205);
const struct device *fg_dev;

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
static const struct pwm_dt_spec bl_led_pwm = PWM_DT_SPEC_GET(DT_ALIAS(bl_led_pwm));
#endif

uint8_t global_batt_level = 0;
static int32_t global_temp_val = 0;

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
    int rb_len;
    rb_len = ring_buf_put(&ringbuf_usb_cdc, buf, len);
    uart_irq_tx_enable(usb_dev);
}

static void interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
        if (!rx_throttled && uart_irq_rx_ready(dev))
        {
            int recv_len, rb_len;
            uint8_t buffer[64];
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
            uint8_t buffer[64];
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

    printk("\nUSB Init complete\n\n");
}

int16_t hpi_get_global_temp(void)
{
    return global_temp_val;
}

int16_t hpi_hw_read_temp(void)
{
    int32_t i32_temp_val = 0;
    int16_t temp_val = 0;

    struct sensor_value temp_sample;
    sensor_sample_fetch(max30205_dev);
    sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);
    // Convert to degree F
    if (temp_sample.val1 > 0)
    {
        i32_temp_val = (temp_sample.val1 * 9 / 5) + 32000;
        temp_val = (int16_t) (i32_temp_val / 10);
    }

    return temp_val;
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
        printk("Error: cannot get properties\n");
    }
    else
    {
        // printk("Charge %d%% TTE: %d Voltage: %d \n", vals[2].relative_state_of_charge, vals[0].runtime_to_empty, (vals[3].voltage));
        batt_level = vals[2].relative_state_of_charge;
    }

    return batt_level;
}

static void gpio_keys_cb_handler(struct input_event *evt)
{
    printk("GPIO_KEY %s pressed, zephyr_code=%u, value=%d\n",evt->dev->name, evt->code, evt->value);
    /*settings_send_usb_enabled = false;
    settings_send_ble_enabled = false;
    settings_send_display_enabled = true;*/

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    if (evt->value == 1)
    {
        switch (evt->code)
        {
        case INPUT_KEY_ENTER:
            LOG_INF("OK Key Pressed");
            //hpi_disp_change_event(HPI_SCR_EVENT_OK);
            k_sem_give(&sem_ok_key_pressed);
            break;
        case INPUT_KEY_UP:
            LOG_INF("UP Key Pressed");
            //hpi_disp_change_event(HPI_SCR_EVENT_UP);
            k_sem_give(&sem_up_key_pressed);
            break;
        case INPUT_KEY_DOWN:
            LOG_INF("DOWN Key Pressed");
            //hpi_disp_change_event(HPI_SCR_EVENT_DOWN);
            k_sem_give(&sem_down_key_pressed);
            break;
        default:
            break;
        }
    }
#endif
}
//INPUT_CALLBACK_DEFINE(gpio_keys_dev, gpio_keys_cb_handler);

void hw_thread(void)
{
    if (!device_is_ready(max30001_dev))
    {
        printk("MAX30001 device not found! Rebooting !");
        // sys_reboot(SYS_REBOOT_COLD);
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
        printk("AFE4400 device not found!");
        // return;
    }

    if (!device_is_ready(max30205_dev))
    {
        printk("MAX30205 device not found!");
        // return;
    }

    fg_dev = DEVICE_DT_GET_ANY(maxim_max17048);
    if (!device_is_ready(fg_dev))
    {
        printk("No device found...\n");
    }

#ifdef CONFIG_BT
    ble_module_init();
#endif

    leds_init();
    fs_module_init();

    // init_settings();

    printk("HW Thread started\n");

    k_sem_give(&sem_hw_inited);

    usb_init();

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    // PWM for LCD Backlight
    if (!pwm_is_ready_dt(&bl_led_pwm))
    {
        printk("Error: PWM device %s is not ready\n",
               bl_led_pwm.dev->name);
        // return 0;
    }

    int ret = pwm_set_pulse_dt(&bl_led_pwm, 3000);
    if (ret)
    {
        printk("Error %d: failed to set pulse width\n", ret);
        // return 0;
    }
#endif

    for (;;)
    {
        // Sample slow changing sensors
        global_batt_level = hpi_hw_read_batt();

        global_temp_val = hpi_hw_read_temp();

#ifdef CONFIG_DISPLAY
        //if (settings_send_display_enabled)
        //{
        hpi_disp_update_batt_level(global_batt_level);
        hpi_disp_update_temp(global_temp_val);
        //}
#endif

#ifdef CONFIG_BT
        //if (settings_send_ble_enabled)
        //{
        ble_bas_notify(global_batt_level);
        ble_temp_notify(global_temp_val);
        //}
#endif

        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(hw_thread_id, HW_THREAD_STACKSIZE, hw_thread, NULL, NULL, NULL, HW_THREAD_PRIORITY, 0, 0);
