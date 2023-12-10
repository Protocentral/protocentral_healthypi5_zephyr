#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/drivers/uart.h>

#include <stdio.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/sys/ring_buffer.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include "max30001.h"

#include "sys_sm_module.h"
#include "hw_module.h"
#include "fs_module.h"
#include "display_module.h"

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
const struct device *max30205_dev;
const struct device *max32664_dev;

uint8_t global_batt_level = 0;

// GPIO LEDs
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(ledgreen), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(ledblue), gpios);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

// const struct device *usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

enum button_evt
{
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
};

/// @brief GPIO Button Support
static struct gpio_dt_spec button_ok = GPIO_DT_SPEC_GET_OR(DT_ALIAS(keyok), gpios, {0});
static struct gpio_dt_spec button_up = GPIO_DT_SPEC_GET_OR(DT_ALIAS(keyup), gpios, {0});
static struct gpio_dt_spec button_down = GPIO_DT_SPEC_GET_OR(DT_ALIAS(keydown), gpios, {0});

static struct gpio_callback button_ok_cb;
static struct gpio_callback button_up_cb;
static struct gpio_callback button_down_cb;

// USB CDC UART
#define RING_BUF_SIZE 1024
uint8_t ring_buffer[RING_BUF_SIZE];
struct ring_buf ringbuf_usb_cdc;

#ifdef CONFIG_SENSOR_MAX30001
#define MAX30001_DEVICE_NODE DT_ALIAS(max30001)
const struct device *const max30001_dev = DEVICE_DT_GET(MAX30001_DEVICE_NODE);
#endif

int32_t global_temp;

// #ifdef CONFIG_SENSOR_MAX32664
// #define MAX32664_DEVICE_NODE DT_ALIAS(max32664)
// const struct device *const max32664_dev = DEVICE_DT_GET(MAX32664_DEVICE_NODE);
// #endif

#define GPIO_DEBOUNCE_TIME K_MSEC(30)

uint8_t m_key_pressed = GPIO_KEYPAD_KEY_NONE;

static void cooldown_expired_isr_ok(struct k_work *work)
{
    ARG_UNUSED(work);

    // int val = gpio_pin_get_dt(&button_ok);
    // enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    m_key_pressed = GPIO_KEYPAD_KEY_OK;
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work_ok, cooldown_expired_isr_ok);

static void cooldown_expired_isr_up(struct k_work *work)
{
    ARG_UNUSED(work);

    // int val = gpio_pin_get_dt(&button_ok);
    // enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    m_key_pressed = GPIO_KEYPAD_KEY_UP;
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work_up, cooldown_expired_isr_up);

static void cooldown_expired_isr_down(struct k_work *work)
{
    ARG_UNUSED(work);

    // int val = gpio_pin_get_dt(&button_ok);
    // enum button_evt evt = val ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
    m_key_pressed = GPIO_KEYPAD_KEY_DOWN;
}

static K_WORK_DELAYABLE_DEFINE(cooldown_work_down, cooldown_expired_isr_down);

static void button_isr_ok(const struct device *port,
                          struct gpio_callback *cb,
                          uint32_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(&cooldown_work_ok, GPIO_DEBOUNCE_TIME);
}

static void button_isr_up(const struct device *port,
                          struct gpio_callback *cb,
                          uint32_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(&cooldown_work_up, GPIO_DEBOUNCE_TIME);
}

static void button_isr_down(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(&cooldown_work_down, GPIO_DEBOUNCE_TIME);
}

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

static void buttons_init(void)
{
    int ret;

    // Configure buttons GPIO
    printk("Initing GPIO\n");

    if (!device_is_ready(button_ok.port))
    {
        return;
    }

    ret = gpio_pin_configure_dt(&button_ok, GPIO_INPUT);
    ret = gpio_pin_configure_dt(&button_up, GPIO_INPUT);
    ret = gpio_pin_configure_dt(&button_down, GPIO_INPUT);

    gpio_init_callback(&button_ok_cb, button_isr_ok, BIT(button_ok.pin));
    gpio_init_callback(&button_up_cb, button_isr_up, BIT(button_up.pin));
    gpio_init_callback(&button_down_cb, button_isr_down, BIT(button_down.pin));

    ret = gpio_add_callback(button_ok.port, &button_ok_cb);
    ret = gpio_add_callback(button_up.port, &button_up_cb);
    ret = gpio_add_callback(button_down.port, &button_down_cb);

    ret = gpio_pin_interrupt_configure_dt(&button_ok, GPIO_INT_EDGE_TO_ACTIVE);
    ret = gpio_pin_interrupt_configure_dt(&button_up, GPIO_INT_EDGE_TO_ACTIVE);
    ret = gpio_pin_interrupt_configure_dt(&button_down, GPIO_INT_EDGE_TO_ACTIVE);
}

void send_usb_cdc(const char *buf, size_t len)
{
    int rb_len;
    // rb_len = ring_buf_put(&ringbuf_usb_cdc, buf, len);
    // uart_irq_tx_enable(usb_dev);
}

// TODO: implement RPI UART
void send_rpi_uart(const char *buf, size_t len)
{
    // int rb_len;
    // rb_len = ring_buf_put(&ringbuf_uart, buf, len);
    // uart_irq_tx_enable(uart_dev);
}

static void interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
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

            send_len = uart_fifo_fill(dev, buffer, rb_len);
            if (send_len < rb_len)
            {
                LOG_ERR("Drop %d bytes", rb_len - send_len);
            }

            LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
        }
    }
}

uint8_t read_battery_level(void)
{
    int ret = 0;
    //uint8_t batt_level = 0;

    /*struct fuel_gauge_get_property props[] = {
        {
            .property_type = FUEL_GAUGE_RUNTIME_TO_EMPTY,
        },
        {
            .property_type = FUEL_GAUGE_RUNTIME_TO_FULL,
        },
        {
            .property_type = FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
        },
        {
            .property_type = FUEL_GAUGE_VOLTAGE,
        }};

    ret = fuel_gauge_get_prop(fg_dev, props, ARRAY_SIZE(props));
    if (ret < 0)
    {
        printk("Error: cannot get properties\n");
    }
    else
    {
        if (ret != 0)
        {
            printk("Warning: Some properties failed\n");
        }

        if (props[0].status == 0)
        {
            // printk("Time to empty %d\n", props[0].value.runtime_to_empty);
        }
        else
        {
            printk(
                "Property FUEL_GAUGE_RUNTIME_TO_EMPTY failed with error %d\n",
                props[0].status);
        }

        if (props[1].status == 0)
        {
            // printk("Time to full %d\n", props[1].value.runtime_to_full);
        }
        else
        {
            printk(
                "Property FUEL_GAUGE_RUNTIME_TO_FULL failed with error %d\n",
                props[1].status);
        }

        if (props[2].status == 0)
        {
            printk("Charge %d%%\n", props[2].value.relative_state_of_charge);
        }
        else
        {
            printk(
                "Property FUEL_GAUGE_STATE_OF_CHARGE failed with error %d\n",
                props[2].status);
        }

        if (props[3].status == 0)
        {
            printk("Voltage %d\n", props[3].value.voltage);
        }
        else
        {
            printk(
                "Property FUEL_GAUGE_VOLTAGE failed with error %d\n",
                props[3].status);
        }
    }*/



    return ret;
}

static void usb_init()
{
    int ret = 0;

#ifdef CONFIG_HEALTHYPI_USB_CDC_ENABLED

    /*if (!device_is_ready(usb_dev))
    {
        LOG_ERR("CDC ACM device not ready");
        // return;
    }
    */

    ret = usb_enable(NULL);
    if (ret != 0)
    {
        LOG_ERR("Failed to enable USB");
        // return;
    }

    /* Enabled USB CDC interrupts */

    ring_buf_init(&ringbuf_usb_cdc, sizeof(ring_buffer), ring_buffer);

    // uart_irq_callback_set(usb_dev, interrupt_handler);
    // uart_irq_rx_enable(usb_dev);

#endif

#ifdef CONFIG_HEALTHYPI_USB_MSC_ENABLED

#endif

    printk("\nUSB Init complete\n\n");
}

static inline float out_ev(struct sensor_value *val)
{
    return (val->val1 + (float)val->val2 / 1000000);
}

static void fetch_and_display(const struct device *dev)
{
    struct sensor_value x, y, z;
    static int trig_cnt;

    trig_cnt++;

    /* lsm6dso accel */
    sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

    printf("accel x:%f ms/2 y:%f ms/2 z:%f ms/2\n",
           (double)out_ev(&x), (double)out_ev(&y), (double)out_ev(&z));

    /* lsm6dso gyro */
    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    printf("gyro x:%f rad/s y:%f rad/s z:%f rad/s\n",
           (double)out_ev(&x), (double)out_ev(&y), (double)out_ev(&z));

    printf("trig_cnt:%d\n\n", trig_cnt);
}

static int set_sampling_freq(const struct device *dev)
{
    int ret = 0;
    struct sensor_value odr_attr;

    /* set accel/gyro sampling frequency to 12.5 Hz */
    odr_attr.val1 = 12.5;
    odr_attr.val2 = 0;

    ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
    if (ret != 0)
    {
        printf("Cannot set sampling frequency for accelerometer.\n");
        return ret;
    }

    ret = sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
    if (ret != 0)
    {
        printf("Cannot set sampling frequency for gyro.\n");
        return ret;
    }

    return 0;
}

int32_t read_temp(void)
{
    sensor_sample_fetch(max30205_dev);
    struct sensor_value temp_sample;
    sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);
    // last_read_temp_value = temp_sample.val1;
    //printk("Temp: %d\n", temp_sample.val1);
    return temp_sample.val1;
}

void hw_thread(void)
{
    // int ret = 0;

#ifdef CONFIG_SENSOR_MAX30001
    if (!device_is_ready(max30001_dev))
    {
        printk("MAX30001 device not found!");
        //return;
    }
#endif

    /*max32664_dev = DEVICE_DT_GET_ANY(maxim_max32664);
    if (!device_is_ready(max32664_dev))
    {
        printk("MAX32664 device not found!");
        //return;
    }*/

    max30205_dev = DEVICE_DT_GET_ANY(maxim_max30205);
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

    /*const struct device *const acc_dev = DEVICE_DT_GET_ONE(st_lsm6dso);

    if (!device_is_ready(acc_dev))
    {
        printk("%s: device not ready.\n", acc_dev->name);
        // return 0;
    }*/

    /*if (set_sampling_freq(acc_dev) != 0)
    {
        return;
    }*/

    leds_init();
    buttons_init();

    fs_module_init();

    // init_settings();

    printk("HW Thread started\n");
    // printk("Initing...\n");

    k_sem_give(&sem_hw_inited);

    usb_init();

    //read_temp();

    for (;;)
    {
        //global_batt_level = read_battery_level();
        // fetch_and_display(acc_dev);
        global_temp = read_temp();
        hpi_disp_update_temp(global_temp);
        global_batt_level = read_battery_level();
        hpi_disp_update_batt_level(global_batt_level);
        k_sleep(K_MSEC(3000));
    }
}

K_THREAD_DEFINE(hw_thread_id, HW_THREAD_STACKSIZE, hw_thread, NULL, NULL, NULL, HW_THREAD_PRIORITY, 0, 0);
