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
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pwm.h>

#include <stdio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usbd.h>

#include "usbd_init.h"

#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/reboot.h>

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
static uint32_t usb_buffer_writes = 0;
static uint32_t usb_buffer_drops = 0;       // dropped because ring was full
static uint32_t usb_dtr_off_drops = 0;      // dropped because host port closed (DTR=0)
static uint32_t last_usb_log_time = 0;

// Temperature sensor availability
static bool temp_sensor_available = false;  // Set at boot, never changes

// Thread heartbeat tracking for software watchdog
volatile uint32_t heartbeat_data_thread = 0;
volatile uint32_t heartbeat_display_thread = 0;
volatile uint32_t heartbeat_sampling_workq = 0;

// Hardware watchdog
static const struct device *const wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int wdt_channel_id = -1;

static const struct device *const gpio_keys_dev = DEVICE_DT_GET_ANY(gpio_keys);
static const struct device *const longpress_dev = DEVICE_DT_GET(DT_NODELABEL(longpress));
uint8_t m_key_pressed = GPIO_KEYPAD_KEY_NONE;

static uint32_t last_tx_activity = 0;
static bool usb_dtr_state = false;  // DTR line status (host port open)

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

// Get USB buffer utilization as percentage (0-100)
uint8_t get_usb_buffer_utilization(void)
{
    uint32_t used = ring_buf_size_get(&ringbuf_usb_cdc);
    uint32_t capacity = ring_buf_capacity_get(&ringbuf_usb_cdc);
    
    if (capacity == 0) {
        return 0;
    }
    
    return (uint8_t)((used * 100) / capacity);
}

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
    /* Gate writes on the host-side DTR signal. USBD-next delivers DTR
     * transitions reliably through hpi_usbd_msg_cb, so usb_dtr_state is
     * authoritative. When the host stack drops DTR (port closed, USB
     * suspend, hub renegotiation, etc.), we drop at the producer instead
     * of filling the ring. This is critical on RP2040: if the ring fills
     * while the host is not draining, the udc_rpi_pico bulk-IN endpoint
     * ends up with an in-flight transfer descriptor the host never polls,
     * and the endpoint gets wedged in a state it cannot recover from when
     * DTR returns. Gating at the producer avoids the wedge entirely.
     */
    if (!usb_dtr_state) {
        usb_dtr_off_drops++;
        return;
    }

    uint32_t space = ring_buf_space_get(&ringbuf_usb_cdc);

    if (space < len) {
        /* Ring full despite DTR being up — host is reading slower than
         * we're producing. Drop packet (rather than partial-write, which
         * would corrupt the framed protocol).
         */
        usb_buffer_drops++;

        static uint32_t last_drop_log = 0;
        if (usb_buffer_drops == 1 || usb_buffer_drops - last_drop_log >= 5000) {
            LOG_WRN("USB buffer full, drops: %u", usb_buffer_drops);
            last_drop_log = usb_buffer_drops;
        }
        return;
    }
    
    int rb_len = ring_buf_put(&ringbuf_usb_cdc, buf, len);
    usb_buffer_writes++;

    /* Periodic USB buffer health log: every 120 s, but only if drops have
     * accumulated since the last log (no news = good news, less log spam).
     */
    uint32_t current_time = k_uptime_get_32();
    if (current_time - last_usb_log_time >= 120000) {
        if (usb_buffer_drops > 0 || usb_dtr_off_drops > 0) {
            uint32_t ring_used = ring_buf_size_get(&ringbuf_usb_cdc);
            uint32_t capacity  = ring_buf_capacity_get(&ringbuf_usb_cdc);
            uint32_t total     = usb_buffer_writes + usb_buffer_drops;
            uint32_t drop_rate = total ? (usb_buffer_drops * 100) / total : 0;
            LOG_INF("USB: %u/%u bytes, buf_drops=%u (%u%%) dtr_off_drops=%u",
                    ring_used, capacity, usb_buffer_drops, drop_rate,
                    usb_dtr_off_drops);
        }
        last_usb_log_time = current_time;
    }

    if (rb_len > 0) {
        uart_irq_tx_enable(usb_dev);
    }
}

static void interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
        if (uart_irq_rx_ready(dev))
        {
            /* Drain host RX into a local scratch and discard. The shared
             * ringbuf_usb_cdc is TX-only; mixing RX bytes into it caused
             * stream corruption and races with send_usb_cdc(). If RX needs
             * to be consumed, do it via a dedicated RX path/queue.
             */
            uint8_t scratch[64];
            (void)uart_fifo_read(dev, scratch, sizeof(scratch));
        }

        if (uart_irq_tx_ready(dev))
        {
            /* Single-producer drain: claim a contiguous span from the ring
             * buffer, hand it to uart_fifo_fill(), and only finish() the
             * bytes that were actually accepted. This avoids ring_buf_put()
             * from ISR context, which would race with send_usb_cdc() in
             * thread context (ring_buf is SPSC-safe only).
             */
            uint8_t *claim_ptr = NULL;
            uint32_t claim_len = ring_buf_get_claim(&ringbuf_usb_cdc,
                                                    &claim_ptr, 128);
            if (claim_len == 0)
            {
                (void)ring_buf_get_finish(&ringbuf_usb_cdc, 0);
                uart_irq_tx_disable(dev);
                continue;
            }

            int send_len = uart_fifo_fill(dev, claim_ptr, claim_len);
            if (send_len < 0)
            {
                send_len = 0;
            }

            (void)ring_buf_get_finish(&ringbuf_usb_cdc, send_len);

            if (send_len > 0)
            {
                last_tx_activity = k_uptime_get_32();
            }
        }
    }
}

// USB TX watchdog - re-enables TX if it appears stuck
static void usb_tx_watchdog_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(usb_tx_watchdog_work, usb_tx_watchdog_work_handler);

static uint32_t tx_watchdog_triggers = 0;

static void usb_tx_watchdog_work_handler(struct k_work *work)
{
    static uint32_t watchdog_runs = 0;
    uint32_t ring_used = ring_buf_size_get(&ringbuf_usb_cdc);
    uint32_t now = k_uptime_get_32();

    watchdog_runs++;

    /* DTR is tracked authoritatively by hpi_usbd_msg_cb under USBD-next, so
     * polling uart_line_ctrl_get is no longer needed here. We only kick TX
     * when the host has actually opened the port AND there's data pending
     * AND TX hasn't fired recently — otherwise we'd just be re-enabling a
     * stalled endpoint nobody is draining.
     */
    if ((watchdog_runs % 240) == 0) {  /* 240 * 250 ms = 60 s */
        LOG_INF("USB WD: runs=%u ring=%u triggers=%u DTR=%d buf_drops=%u dtr_off_drops=%u",
                watchdog_runs, ring_used, tx_watchdog_triggers,
                (int)usb_dtr_state, usb_buffer_drops, usb_dtr_off_drops);
    }

    if (usb_dtr_state && ring_used > 0 && (now - last_tx_activity) > 500) {
        tx_watchdog_triggers++;
        if (tx_watchdog_triggers <= 5 || (tx_watchdog_triggers % 100) == 0) {
            LOG_WRN("USB TX watchdog: ring=%u, triggers=%u", ring_used, tx_watchdog_triggers);
        }
        uart_irq_tx_enable(usb_dev);
        last_tx_activity = now;
    }

    k_work_schedule(&usb_tx_watchdog_work, K_MSEC(250));
}

void update_battery_level(void)
{
}

/* ---------------------------------------------------------------------------
 * Software watchdog + diagnostic dump (ISR-driven)
 *
 * Earlier this lived in a priority-8 thread, alongside hw_thread (priority
 * 9) which fed the 8 s HW watchdog. Both threads sit *below* every producer
 * in this app (display/sampling workq at 5, data_thread at 6, cmd/sensor at
 * 7). When USB-CDC backpressure pushes those producers into busy loops, the
 * priority-9 hw_thread misses wdt_feed() → HW WDT silently resets → the
 * priority-8 SW WDT never gets to run either, so no diagnostic dump is
 * emitted. The next boot reports "POR" because RP2040 hwinfo conflates
 * watchdog resets with power-on.
 *
 * The fix is to run the watchdog from a k_timer callback (system tick ISR
 * context). ISRs always preempt threads, so the check + HW WDT feed cannot
 * be starved. The HW WDT now only ever fires if the system clock itself
 * stops — a real catastrophic failure rather than scheduling pressure.
 * --------------------------------------------------------------------------- */

#define SW_WDT_CHECK_INTERVAL_MS  1000
#define SW_WDT_STALL_THRESHOLD_MS 6000
#define SW_WDT_GRACE_PERIOD_MS    15000

/* Tick-gap tracking: distinguishes "thread starved by scheduling" from
 * "entire system frozen with interrupts disabled" (e.g., during a multi-
 * sector flash erase on RP2040, where the Pico SDK's flash_safe_execute()
 * masks IRQs on the calling core for the duration of the op).
 *
 * If `last_tick_gap_ms` and `max_tick_gap_ms` are far larger than
 * SW_WDT_CHECK_INTERVAL_MS (~1000 ms) when the SW WDT trips, the timer ISR
 * itself was held off — i.e., something disabled interrupts. That points at
 * a flash op (LittleFS GC, settings save, MCUboot operation) rather than
 * application code spinning.
 */
static volatile uint32_t last_tick_uptime  = 0;
static volatile uint32_t last_tick_gap_ms  = 0;
static volatile uint32_t max_tick_gap_ms   = 0;
#define SW_WDT_GAP_WARN_MS 1500  /* warn if a tick was delayed > 1.5 s */

/* Signed age in ms between `now` and a heartbeat timestamp.
 *
 * Heartbeats are bumped from other threads while we read them, so a heartbeat
 * captured just *after* `now` can appear "in the future" by a few ms. Casting
 * the unsigned subtraction to int32_t makes that case a small negative number
 * (interpreted as "fresh"), instead of underflowing to ~UINT32_MAX and
 * falsely tripping the stall threshold.
 */
static inline int32_t hb_age_ms(uint32_t now, uint32_t hb)
{
    return (int32_t)(now - hb);
}

/* Called from k_timer ISR context. LOG_PANIC switches the logger to
 * synchronous flush, so subsequent LOG_ERR writes go straight to the UART
 * backend without depending on a thread to drain the deferred queue.
 * k_busy_wait (not k_msleep) is used because we may be in ISR context.
 */
static void sw_wdt_dump_and_reboot(const char *reason)
{
    uint32_t now = k_uptime_get_32();
    uint32_t hb_data = heartbeat_data_thread;
    uint32_t hb_disp = heartbeat_display_thread;
    uint32_t hb_samp = heartbeat_sampling_workq;
    uint32_t tx_act  = last_tx_activity;

    LOG_PANIC();
    LOG_ERR("========================================");
    LOG_ERR("SW WATCHDOG TRIPPED: %s", reason);
    LOG_ERR("uptime=%u ms", now);
    LOG_ERR("heartbeats: data=%u (age=%d) display=%u (age=%d) sampling=%u (age=%d)",
            hb_data, hb_age_ms(now, hb_data),
            hb_disp, hb_age_ms(now, hb_disp),
            hb_samp, hb_age_ms(now, hb_samp));
    LOG_ERR("usb: ring_used=%u drops=%u writes=%u tx_wd_triggers=%u dtr=%d",
            ring_buf_size_get(&ringbuf_usb_cdc),
            usb_buffer_drops, usb_buffer_writes,
            tx_watchdog_triggers, (int)usb_dtr_state);
    LOG_ERR("last_tx_activity=%u (age=%d)", tx_act, hb_age_ms(now, tx_act));
    /* If last_tick_gap_ms ≫ SW_WDT_CHECK_INTERVAL_MS (~1000 ms), the timer
     * ISR itself was held off — i.e., interrupts were disabled (most likely
     * during a long flash erase/program). If it's near 1000 ms, threads
     * were starved while ISRs kept running (scheduling problem).
     */
    LOG_ERR("tick_gap: last=%u ms max=%u ms (expected ~%u ms)",
            last_tick_gap_ms, max_tick_gap_ms, SW_WDT_CHECK_INTERVAL_MS);
    LOG_ERR("========================================");

    /* Give the synchronous-mode log backend a moment to push the last lines
     * out the UART before the SoC reset cuts the bus.
     */
    k_busy_wait(200000);
    sys_reboot(SYS_REBOOT_COLD);
}

/* k_timer expiry callback. Runs in system tick ISR context. Feeds the HW
 * watchdog every tick (it can only ever miss a feed if the timer ISR itself
 * stops) and trips a stall dump if any monitored thread heartbeat is older
 * than SW_WDT_STALL_THRESHOLD_MS.
 */
static void sw_wdt_tick(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    uint32_t now = k_uptime_get_32();

    /* Track the gap between consecutive timer fires. Under normal operation
     * this should be ~SW_WDT_CHECK_INTERVAL_MS. A much larger value means
     * the timer ISR was held off (interrupts disabled or system frozen).
     */
    if (last_tick_uptime != 0) {
        uint32_t gap = now - last_tick_uptime;
        last_tick_gap_ms = gap;
        if (gap > max_tick_gap_ms) {
            max_tick_gap_ms = gap;
        }
        if (gap > SW_WDT_GAP_WARN_MS) {
            LOG_WRN("SW WDT tick gap=%u ms (expected ~%u) -- IRQs were masked",
                    gap, SW_WDT_CHECK_INTERVAL_MS);
        }
    }
    last_tick_uptime = now;

    /* Grace period for bring-up and USB enumeration. */
    if (now < SW_WDT_GRACE_PERIOD_MS) {
        if (wdt_channel_id >= 0) {
            wdt_feed(wdt_dev, wdt_channel_id);
        }
        return;
    }

    uint32_t hb_data = heartbeat_data_thread;
    uint32_t hb_samp = heartbeat_sampling_workq;

    if (hb_data != 0 && hb_age_ms(now, hb_data) > SW_WDT_STALL_THRESHOLD_MS) {
        sw_wdt_dump_and_reboot("data_thread stalled");
    }

    if (hb_samp != 0 && hb_age_ms(now, hb_samp) > SW_WDT_STALL_THRESHOLD_MS) {
        sw_wdt_dump_and_reboot("sampling workq stalled");
    }

#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
    uint32_t hb_disp = heartbeat_display_thread;
    if (hb_disp != 0 && hb_age_ms(now, hb_disp) > SW_WDT_STALL_THRESHOLD_MS) {
        sw_wdt_dump_and_reboot("display_thread stalled");
    }
#endif

    /* All heartbeats fresh — keep the HW WDT happy. */
    if (wdt_channel_id >= 0) {
        wdt_feed(wdt_dev, wdt_channel_id);
    }
}

K_TIMER_DEFINE(sw_wdt_timer, sw_wdt_tick, NULL);

static void log_reset_cause(void)
{
    uint32_t cause = 0;
    int ret = hwinfo_get_reset_cause(&cause);
    if (ret != 0) {
        LOG_INF("reset cause: unavailable (%d)", ret);
        return;
    }

    LOG_INF("reset cause: 0x%08x%s%s%s%s%s%s", cause,
            (cause & RESET_PIN)        ? " PIN"        : "",
            (cause & RESET_SOFTWARE)   ? " SOFTWARE"   : "",
            (cause & RESET_BROWNOUT)   ? " BROWNOUT"   : "",
            (cause & RESET_POR)        ? " POR"        : "",
            (cause & RESET_WATCHDOG)   ? " WATCHDOG"   : "",
            (cause & RESET_DEBUG)      ? " DEBUG"      : "");

    (void)hwinfo_clear_reset_cause();
}

/* USBD-next lifecycle callback.
 *
 * Driven by `usbd_msg_register_cb`. Tracks the host DTR line and emits a log
 * line on baud-rate changes. The legacy code polled DTR from
 * `usb_tx_watchdog_work_handler`; that polling is kept as a backstop, but the
 * callback path is the authoritative source of DTR state under USBD-next.
 */
static void hpi_usbd_msg_cb(struct usbd_context *const ctx,
                            const struct usbd_msg *msg)
{
    ARG_UNUSED(ctx);

    if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
        uint32_t dtr = 0;
        uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &dtr);
        bool new_dtr = (dtr != 0);
        if (new_dtr != usb_dtr_state) {
            LOG_INF("USB DTR: %s -> %s",
                    usb_dtr_state ? "ON" : "OFF",
                    new_dtr ? "ON" : "OFF");
            usb_dtr_state = new_dtr;
            /* Clear any stale bytes left in the ring on a transition so a
             * fresh host reader doesn't see a torn pre-disconnect packet
             * spliced onto the start of the new stream.
             */
            ring_buf_reset(&ringbuf_usb_cdc);
            last_tx_activity = k_uptime_get_32();
        }
        return;
    }

    if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
        uint32_t baudrate = 0;
        if (uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &baudrate) == 0) {
            LOG_INF("USB CDC baudrate=%u", baudrate);
        }
        return;
    }
}

static void usb_init(void)
{
#ifdef CONFIG_HEALTHYPI_USB_CDC_ENABLED

    if (!device_is_ready(usb_dev))
    {
        LOG_ERR("CDC ACM device not ready");
    }

    /* Initialize the TX ring buffer before bringing USB up, so any early
     * `send_usb_cdc()` from other threads finds a valid buffer.
     */
    ring_buf_init(&ringbuf_usb_cdc, sizeof(ring_buffer), ring_buffer);

    if (hpi_usbd_init(hpi_usbd_msg_cb) == NULL) {
        LOG_ERR("USBD-next init failed");
        /* Fall through; CDC UART still works as a no-op sink. */
    }

    /* Let the host complete enumeration + interface bring-up before we start
     * pushing into the CDC UART. The legacy stack used the same 100 ms idiom.
     */
    k_msleep(100);

    uart_irq_callback_set(usb_dev, interrupt_handler);
    uart_irq_rx_enable(usb_dev);

    /* Backstop TX watchdog: delayed 5 s past enumeration noise. */
    last_tx_activity = k_uptime_get_32();
    k_work_schedule(&usb_tx_watchdog_work, K_MSEC(5000));

#endif

    LOG_INF("USB Init complete");
}

int hpi_hw_read_temp(float* temp_f, float* temp_c)
{
    // If sensor was not detected at boot, don't try to read it
    if (!temp_sensor_available) {
        return -ENODEV;
    }

    static uint32_t consecutive_failures = 0;
    static uint32_t last_retry_time = 0;

    // Skip temperature reading if sensor is persistently failing
    // Back off quickly (after 3 failures) to reduce I2C bus noise
    if (consecutive_failures >= 3) {
        // Try again every 30 seconds
        if (k_uptime_get_32() - last_retry_time < 30000) {
            return -EIO;  // Sensor unavailable
        }
        last_retry_time = k_uptime_get_32();
    }

    struct sensor_value temp_sample;
    int ret = sensor_sample_fetch(max30205_dev);
    if (ret != 0) {
        consecutive_failures++;
        // Silently fail - sensor may not be connected
        return ret;
    }
    
    ret = sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_sample);
    if (ret != 0) {
        consecutive_failures++;
        return ret;
    }

    if (temp_sample.val1 < 0)
        return 0;

    // Successful read - reset failure counter
    consecutive_failures = 0;

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
    log_reset_cause();

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
        temp_sensor_available = false;
    }
    else
    {
        // Test sensor by attempting a read
        struct sensor_value test_sample;
        int ret = sensor_sample_fetch(max30205_dev);
        if (ret == 0) {
            ret = sensor_channel_get(max30205_dev, SENSOR_CHAN_AMBIENT_TEMP, &test_sample);
        }
        
        if (ret == 0) {
            temp_sensor_available = true;
            LOG_INF("MAX30205 temperature sensor detected and working");
        } else {
            temp_sensor_available = false;
            LOG_WRN("MAX30205 present but not responding (error: %d) - disabling", ret);
        }
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

    // Initialize hardware watchdog - 8 second timeout
    // If hw_thread hangs or system crashes, watchdog will reset the device
    if (device_is_ready(wdt_dev)) {
        struct wdt_timeout_cfg wdt_config = {
            .window.min = 0,
            .window.max = 8000,  // 8 second timeout (max for RP2040 is ~8.3s)
            .callback = NULL,   // No callback, just reset
            .flags = WDT_FLAG_RESET_SOC,
        };

        wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
        if (wdt_channel_id >= 0) {
            int ret = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
            if (ret == 0) {
                LOG_INF("Hardware watchdog enabled (8s timeout)");
            } else {
                LOG_ERR("Failed to setup watchdog: %d", ret);
                wdt_channel_id = -1;
            }
        } else {
            LOG_ERR("Failed to install watchdog timeout: %d", wdt_channel_id);
        }
    } else {
        LOG_WRN("Hardware watchdog not available");
    }

    /* Start the ISR-driven software watchdog. From this point on, the HW
     * WDT is fed from the k_timer ISR (never from a thread), so a busy
     * priority-5/6 thread can no longer cause a silent HW reset.
     */
    k_timer_start(&sw_wdt_timer,
                  K_MSEC(SW_WDT_CHECK_INTERVAL_MS),
                  K_MSEC(SW_WDT_CHECK_INTERVAL_MS));
    LOG_INF("Software watchdog armed (threshold=%u ms, HW WDT=8000 ms, ISR-driven)",
            SW_WDT_STALL_THRESHOLD_MS);

    float m_temp_f = 0;
    float m_temp_c = 0;

    for (;;)
    {
        static uint32_t hw_loop_count = 0;
        hw_loop_count++;

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

        /* HW watchdog is fed from the k_timer ISR (sw_wdt_tick) so it can't
         * be starved by higher-priority threads. Nothing to do here.
         */

        gpio_pin_toggle_dt(&led_blue);
        k_sleep(K_MSEC(1000));
    }
}

#define HW_THREAD_STACKSIZE 3072
#define HW_THREAD_PRIORITY 9

K_THREAD_DEFINE(hw_thread_id, HW_THREAD_STACKSIZE, hw_thread, NULL, NULL, NULL, HW_THREAD_PRIORITY, 0, 0);
