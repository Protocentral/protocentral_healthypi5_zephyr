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
#include <zephyr/drivers/dac.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>
#include <app_version.h>

#include "hw_module.h"
#include "display_module.h"
#include "hpi_common_types.h"
#include "data_module.h"
#include "vital_stats.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_module, LOG_LEVEL_DBG);

#define HPI_DISP_BATT_REFR_INT 1000

#define HPI_DISP_TEMP_REFR_INT 1000
#define HPI_DISP_HR_REFR_INT 1000
#define HPI_DISP_SPO2_REFR_INT 1000

#define HPI_DISP_RR_REFR_INT 4000

// Downsampling ratio for waveform plots
// ECG: 128 Hz → 64 Hz (2:1 downsample)
// PPG: ~100 Hz → 50 Hz (2:1 downsample)  
// Resp: 128 Hz → 64 Hz (2:1 downsample)
// Balance between performance and visual smoothness
#define PLOT_DOWNSAMPLE_RATIO 2

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

extern uint8_t m_key_pressed;

const struct device *display_dev;

// Default to BASIC operation mode at runtime. Build scripts no longer override this.
static enum hpi_disp_op_mode m_op_mode = OP_MODE_BASIC;

// LVGL Screens
lv_obj_t *scr_menu;
lv_obj_t *scr_charts_all;
lv_obj_t *scr_charts_single;

lv_style_t style_sub;

lv_style_t style_number_big;
lv_style_t style_number_medium;

lv_style_t style_header_black;
lv_style_t style_header_red;
lv_style_t style_header_green;
lv_style_t style_scr_back;

lv_style_t style_batt_sym;

lv_style_t style_h1;
lv_style_t style_h2;
lv_style_t style_info;
lv_style_t style_icon;

// Font styles for detail screens
lv_style_t style_text_14;     // montserrat_14 - chart titles, stats, small text
lv_style_t style_text_16;     // montserrat_16 - units, regular text
lv_style_t style_text_20;     // montserrat_20 - medium values
lv_style_t style_text_24;     // montserrat_24 - icons
lv_style_t style_text_28;     // montserrat_28 - large values
lv_style_t style_text_42;     // montserrat_42 - extra large values

// static lv_obj_t *roller_session_select;
// static lv_obj_t *label_current_mode;

// Label declarations moved to centralized section below (line ~130)
// to avoid duplicates and ensure proper screen transition handling

static uint8_t m_disp_batt_level = 0;
static bool m_disp_batt_charging = false;
static int last_batt_refresh = 0;

uint16_t m_disp_hr = 0;  // Non-static - accessed by detail screens
static int last_hr_refresh = 0;

float m_disp_temp_f = 0;  // Non-static - accessed by detail screens
float m_disp_temp_c = 0;  // Non-static - accessed by detail screens

static int last_temp_refresh = 0;

uint8_t m_disp_spo2 = 0;  // Non-static - accessed by detail screens
static int last_spo2_refresh = 0;

uint8_t m_disp_rr = 0;  // Non-static - accessed by detail screens
static int last_rr_refresh = 0;

K_MSGQ_DEFINE(q_plot_ecg_bioz, sizeof(struct hpi_ecg_bioz_sensor_data_t), 64, 4);
K_MSGQ_DEFINE(q_plot_ppg, sizeof(struct hpi_ppg_sensor_data_t), 64, 4);

K_MSGQ_DEFINE(q_hpi_plot_all_sample, sizeof(struct hpi_sensor_data_point_t), 64, 1);

K_MUTEX_DEFINE(mutex_curr_screen);
K_SEM_DEFINE(sem_disp_inited, 0, 1);

// Async screen change pattern (from healthypi-move-fw)
static int g_screen = SCR_HOME;
static enum scroll_dir g_scroll_dir = SCROLL_DOWN;
static uint32_t g_arg1 = 0;
static uint32_t g_arg2 = 0;
static uint32_t g_arg3 = 0;
static uint32_t g_arg4 = 0;
K_SEM_DEFINE(sem_change_screen, 0, 1);

// Screen transition state flag to prevent race conditions
static bool screen_transitioning = false;

bool display_inited = false;
static uint8_t curr_screen = SCR_HR;

// Function table types for screen management
typedef void (*screen_draw_func_t)(enum scroll_dir);
typedef void (*screen_gesture_down_func_t)(void);

typedef struct
{
    screen_draw_func_t draw;
    screen_gesture_down_func_t gesture_down;
} screen_func_table_entry_t;

// GUI Labels - Centralized label management for thread safety
// Header labels (used on all screens)
static lv_obj_t *label_batt_level;
static lv_obj_t *label_batt_level_val;

// Home screen labels (used only on home screen)
lv_obj_t *g_label_home_hr = NULL;
lv_obj_t *g_label_home_hr_source = NULL;  // HR source indicator (ECG/PPG)
lv_obj_t *g_label_home_spo2 = NULL;
lv_obj_t *g_label_home_rr = NULL;
lv_obj_t *g_label_home_temp_f = NULL;
lv_obj_t *g_label_home_temp_c = NULL;

// Externs

extern struct k_sem sem_hw_inited;
extern struct k_sem sem_ok_key_pressed;
extern struct k_sem sem_ok_key_longpress;  // Separate semaphore for long press
extern struct k_sem sem_up_key_pressed;
extern struct k_sem sem_down_key_pressed;
extern struct k_msgq q_computed_val;

// Software debounce for UP/DOWN buttons to prevent rapid screen changes
#define BUTTON_DEBOUNCE_MS 1200  // 1.2 seconds between UP/DOWN button presses
static uint32_t last_up_press_time = 0;
static uint32_t last_down_press_time = 0;

// Flag to skip lv_task_handler on next iteration after screen change
static bool skip_next_lvgl_handler = false;

int hpi_disp_get_op_mode()
{
    return m_op_mode;
}

bool hpi_disp_is_plot_screen_active(void)
{
    int screen = hpi_disp_get_curr_screen();
    return false;  // No plot screens anymore, only detail screens with charts
}

void display_init_styles()
{
    // Subscript (Unit) label style
    lv_style_init(&style_sub);
    lv_style_set_text_color(&style_sub, lv_color_white());
    lv_style_set_text_font(&style_sub, &lv_font_montserrat_16);

    lv_style_init(&style_number_big);
    lv_style_set_text_color(&style_number_big, lv_color_white());
    lv_style_set_text_font(&style_number_big, &lv_font_montserrat_42);

    lv_style_init(&style_number_medium);
    lv_style_set_text_color(&style_number_medium, lv_color_white());
    lv_style_set_text_font(&style_number_medium, &lv_font_montserrat_34);

    // Icon welcome screen style
    lv_style_init(&style_icon);
    lv_style_set_text_color(&style_icon, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_font(&style_icon, &lv_font_montserrat_34);

    // H1 welcome screen style
    lv_style_init(&style_h1);
    lv_style_set_text_color(&style_h1, lv_color_white());
    lv_style_set_text_font(&style_h1, &lv_font_montserrat_34);

    lv_style_init(&style_header_black);
    lv_style_set_text_color(&style_header_black, lv_color_black());
    lv_style_set_text_font(&style_header_black, &lv_font_montserrat_14);

    lv_style_init(&style_header_red);
    lv_style_set_text_color(&style_header_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_font(&style_header_red, &lv_font_montserrat_16);

    lv_style_init(&style_header_green);
    lv_style_set_text_color(&style_header_green, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_font(&style_header_green, &lv_font_montserrat_16);

    // H2 welcome screen style
    lv_style_init(&style_h2);
    lv_style_set_text_color(&style_h2, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_text_font(&style_h2, &lv_font_montserrat_28);

    // Info welcome screen style
    lv_style_init(&style_info);
    lv_style_set_text_color(&style_info, lv_color_white());
    lv_style_set_text_font(&style_info, &lv_font_montserrat_16);

    // Font styles for detail screens - neutral white color
    lv_style_init(&style_text_14);
    lv_style_set_text_color(&style_text_14, lv_color_white());
    lv_style_set_text_font(&style_text_14, &lv_font_montserrat_14);

    lv_style_init(&style_text_16);
    lv_style_set_text_color(&style_text_16, lv_color_white());
    lv_style_set_text_font(&style_text_16, &lv_font_montserrat_16);

    lv_style_init(&style_text_20);
    lv_style_set_text_color(&style_text_20, lv_color_white());
    lv_style_set_text_font(&style_text_20, &lv_font_montserrat_20);

    lv_style_init(&style_text_24);
    lv_style_set_text_color(&style_text_24, lv_color_white());
    lv_style_set_text_font(&style_text_24, &lv_font_montserrat_24);

    lv_style_init(&style_text_28);
    lv_style_set_text_color(&style_text_28, lv_color_white());
    lv_style_set_text_font(&style_text_28, &lv_font_montserrat_28);

    lv_style_init(&style_text_42);
    lv_style_set_text_color(&style_text_42, lv_color_white());
    lv_style_set_text_font(&style_text_42, &lv_font_montserrat_42);

    // lv_style_set_bg_grad(&style_scr_back, &grad);
}

static void hpi_disp_update_temp(float temp_f, float temp_c)
{
    // Only update home screen - detail screens update themselves
    if (curr_screen == SCR_HOME)
    {
        hpi_scr_home_update_temp(temp_f, temp_c);
    }
}

void hpi_scr_update_hr(int hr)
{
    // Only update home screen - detail screens update themselves
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_hr(hr);
    }
}

void hpi_scr_update_spo2(int spo2)
{
    // Only update home screen - detail screens update themselves
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_spo2(spo2);
    }
}

void hpi_scr_update_pr(int pr)
{
    // Only update home screen - detail screens update themselves
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_pr(pr);
    }
}

void hpi_scr_update_rr(int rr)
{
    // Only update home screen - detail screens update themselves
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_rr(rr);
    }
}

void hpi_disp_change_event(enum hpi_scr_event evt)
{
    if (evt == HPI_SCR_EVENT_DOWN)
    {
        printf("DOWN at %d\n", hpi_disp_get_curr_screen());
        
        int curr_screen = hpi_disp_get_curr_screen();
        
        // Handle special splash screen - wrap to first list screen
        if (curr_screen == SCR_SPLASH)
        {
            printk("From splash screen to first list screen\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
        }
        else if ((curr_screen + 1) == SCR_LIST_END)
        {
            printk("End of list, wrap to beginning\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
        }
        else
        {
            printk("Loading screen %d\n", curr_screen + 1);
            hpi_load_screen(curr_screen + 1, SCROLL_LEFT);
        }
    }
    else if (evt == HPI_SCR_EVENT_UP)
    {
        printf("UP at %d\n", hpi_disp_get_curr_screen());
        
        int curr_screen = hpi_disp_get_curr_screen();
        
        // Handle special splash screen - wrap to last list screen
        if (curr_screen == SCR_SPLASH)
        {
            printk("From splash screen to last list screen\n");
            hpi_load_screen(SCR_LIST_END - 1, SCROLL_RIGHT);
        }
        else if ((curr_screen - 1) == SCR_LIST_START)
        {
            printk("Start of list, wrap to end\n");
            hpi_load_screen(SCR_LIST_END - 1, SCROLL_RIGHT);
        }
        else
        {
            printk("Loading screen %d\n", curr_screen - 1);
            hpi_load_screen(curr_screen - 1, SCROLL_RIGHT);
        }
    }
}

void draw_header(lv_obj_t *parent, bool showFWVersion)
{
    // Draw Header bar

    // ProtoCentral logo
    /*LV_IMG_DECLARE(logo_oneline);
    lv_obj_t *img1 = lv_img_create(parent);
    lv_img_set_src(img1, &logo_oneline);
    lv_obj_align(img1, LV_ALIGN_TOP_LEFT, 10, 7);
    lv_obj_set_size(img1, 104, 10);
    */

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&style_scr_back);
    lv_style_set_bg_color(&style_scr_back, lv_color_black());
    lv_obj_add_style(parent, &style_scr_back, 0);

    //lv_disp_set_bg_color(NULL, lv_color_black());

    lv_obj_t *header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, 480, 30);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, lv_color_white(), LV_STATE_DEFAULT);

    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    if (showFWVersion)
    {
        // HealthyPi label
        lv_obj_t *label_hpi = lv_label_create(parent);
        char fw_version[40];
        sprintf(fw_version, " HealthyPi 5 (FW v%d.%d.%d)", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
        lv_label_set_text(label_hpi, fw_version);
        lv_obj_add_style(label_hpi, &style_header_black, LV_STATE_DEFAULT);
        // lv_style_set_text_color(label_hpi, lv_color_black());
        lv_obj_align(label_hpi, LV_ALIGN_TOP_LEFT, 3, 5);
    }
    // Label for Symbols

    label_batt_level = lv_label_create(parent);
    lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align(label_batt_level, LV_ALIGN_TOP_RIGHT, -50, 5);

    label_batt_level_val = lv_label_create(parent);
    lv_label_set_text(label_batt_level_val, "--%");
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align_to(label_batt_level_val, label_batt_level, LV_ALIGN_OUT_RIGHT_MID, 7, 0);
    lv_obj_add_style(label_batt_level_val, &style_header_black, LV_STATE_DEFAULT);

    lv_obj_t *lbl_conn_status = lv_label_create(parent);
    if (m_op_mode == OP_MODE_DISPLAY)
    {

        lv_label_set_text(lbl_conn_status, LV_SYMBOL_BLUETOOTH "  " LV_SYMBOL_USB);
        lv_obj_add_style(lbl_conn_status, &style_header_red, LV_STATE_DEFAULT);
        lv_obj_align_to(lbl_conn_status, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -15, 0);
    }
    else
    {

        lv_label_set_text(lbl_conn_status, LV_SYMBOL_BLUETOOTH "  " LV_SYMBOL_USB);
        lv_obj_add_style(lbl_conn_status, &style_header_green, LV_STATE_DEFAULT);
        lv_obj_align_to(lbl_conn_status, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -15, 0);
    }
    /*
     */
    // lv_obj_set_text_color(lbl_no_ble, LV_PART_MAIN, LV_STATE_DEFAULT, lv_palette_main(LV_PALETTE_RED));

    // label_sym_ble = lv_label_create(parent);
    // lv_label_set_text(label_sym_ble, LV_SYMBOL_BLUETOOTH);
    // lv_obj_add_style(label_sym_ble, &style_header_black, LV_STATE_DEFAULT);
    // lv_obj_align_to(label_sym_ble, label_batt_level_val, LV_ALIGN_OUT_LEFT_MID, -5, 1);
}

void down_key_event_handler()
{
    printk("Down key pressed\n");
    // hpi_load_screen();
}

void hpi_load_screen(int m_screen, enum scroll_dir m_scroll_dir)
{
    // Async screen change pattern - signal request, don't execute directly
    // Allow both the paged screens (between SCR_LIST_START and SCR_LIST_END)
    // as well as special screens like SCR_SPLASH and SCR_HOME which live
    // outside that contiguous range.
    bool is_valid = (m_screen >= SCR_LIST_START && m_screen < SCR_LIST_END) ||
                    (m_screen == SCR_HOME) ||
                    (m_screen == SCR_SPLASH);

    if (is_valid)
    {
        LOG_DBG("Requesting screen change to %d", m_screen);
        g_screen = m_screen;
        g_scroll_dir = m_scroll_dir;
        g_arg1 = 0;
        g_arg2 = 0;
        g_arg3 = 0;
        g_arg4 = 0;

        k_sem_give(&sem_change_screen);
    }
    else
    {
        LOG_ERR("Invalid screen ID: %d", m_screen);
    }
}

void disp_screen_event(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Left at %d\n", hpi_disp_get_curr_screen());
        
        int curr_screen = hpi_disp_get_curr_screen();
        
        // Handle special splash screen - wrap to first list screen
        if (curr_screen == SCR_SPLASH)
        {
            printk("From splash screen to first list screen\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
        }
        else if ((curr_screen + 1) == SCR_LIST_END)
        {
            printk("End of list, wrap to beginning\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
        }
        else
        {
            printk("Loading screen %d\n", curr_screen + 1);
            hpi_load_screen(curr_screen + 1, SCROLL_LEFT);
        }
    }

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Right at %d\n", hpi_disp_get_curr_screen());
        
        int curr_screen = hpi_disp_get_curr_screen();
        
        // Handle special splash screen - wrap to last list screen
        if (curr_screen == SCR_SPLASH)
        {
            printk("From splash screen to last list screen\n");
            hpi_load_screen(SCR_LIST_END - 1, SCROLL_RIGHT);
        }
        else if ((curr_screen - 1) == SCR_LIST_START)
        {
            printk("Start of list, wrap to end\n");
            /* Wrap to last valid list screen */
            hpi_load_screen(SCR_LIST_END - 1, SCROLL_RIGHT);
        }
        else
        {
            printk("Loading screen %d\n", curr_screen - 1);
            hpi_load_screen(curr_screen - 1, SCROLL_RIGHT);
        }
    }
}

void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir)
{
    lv_obj_add_event_cb(parent, disp_screen_event, LV_EVENT_GESTURE, NULL);

    // Get reference to old screen before loading new one
    lv_obj_t *old_screen = lv_scr_act();
    
    // Load new screen with auto-delete enabled
    if (m_scroll_dir == SCROLL_LEFT)
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    else
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    
    // Explicitly clean up old screen if it still exists and isn't the default
    if (old_screen != NULL && old_screen != parent && lv_obj_is_valid(old_screen)) {
        lv_obj_del(old_screen);
    }
}

void hpi_disp_update_batt_level(int batt_level)
{
    if (label_batt_level == NULL)
        return;

    char buf[32];
    sprintf(buf, "%d %% ", batt_level);
    lv_label_set_text(label_batt_level_val, buf);

    if (batt_level > 75)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    }
    else if (batt_level > 50)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_3);
    }
    else if (batt_level > 25)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_2);
    }
    else if (batt_level > 10)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_1);
    }
    else
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_EMPTY);
    }
}

// Forward declarations for screen draw functions
void draw_scr_home(enum scroll_dir m_scroll_dir);
// void draw_scr_all_trends(enum scroll_dir m_scroll_dir);  // Disabled
void draw_scr_hr(enum scroll_dir m_scroll_dir);
void draw_scr_spo2(enum scroll_dir m_scroll_dir);
void draw_scr_rr(enum scroll_dir m_scroll_dir);
void draw_scr_temp(enum scroll_dir m_scroll_dir);

void hpi_disp_set_curr_screen(int screen)
{
    k_mutex_lock(&mutex_curr_screen, K_FOREVER);
    curr_screen = screen;
    k_mutex_unlock(&mutex_curr_screen);
}

int hpi_disp_get_curr_screen(void)
{
    k_mutex_lock(&mutex_curr_screen, K_FOREVER);
    int screen = curr_screen;
    k_mutex_unlock(&mutex_curr_screen);
    return screen;
}

bool hpi_disp_is_screen_transitioning(void)
{
    return screen_transitioning;
}

// Function table mapping screen IDs to draw/gesture functions
static const screen_func_table_entry_t screen_func_table[] = {
    [SCR_HOME] = {draw_scr_home, NULL},
    // [SCR_ALL_TRENDS] = {draw_scr_all_trends, NULL},  // Disabled
    [SCR_HR] = {draw_scr_hr, NULL},
    [SCR_SPO2] = {draw_scr_spo2, NULL},
    [SCR_RR] = {draw_scr_rr, NULL},
    [SCR_TEMP] = {draw_scr_temp, NULL},
};

void display_screens_thread(void)
{
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;
    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    struct hpi_sensor_data_point_t sensor_all_data_point;

    k_sem_take(&sem_hw_inited, K_FOREVER);

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev))
    {
        LOG_ERR("Display not available. Stopping display thread");
        return;
    }
    // Init all styles globally
    display_init_styles();

    // Setup LVGL Input Device
    /*lv_indev_drv_init(&m_keypad_drv);
    m_keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
    m_keypad_drv.read_cb = keypad_read;
    m_keypad_indev = lv_indev_drv_register(&m_keypad_drv);
    */

    display_blanking_off(display_dev);

    LOG_INF("Display screens inited");

    // Initialize vital stats tracking module
    vital_stats_init();

    // draw_scr_ecg(SCROLL_DOWN);
    // draw_scr_resp(SCROLL_DOWN);
    // draw_scr_ppg(SCROLL_DOWN);

    // draw_scr_welcome();
    if (m_op_mode == OP_MODE_BASIC)
    {
        hpi_load_screen(SCR_HOME, SCROLL_DOWN);
    }
    else
    {
        hpi_load_screen(SCR_HR, SCROLL_DOWN);  // Load HR detail screen in display mode
        //draw_scr_welcome();
    }

    while (1)
    {

        /*if (k_msgq_get(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {
            if (hpi_disp_get_curr_screen() == SCR_ECG)
            {
                hpi_ecg_disp_draw_plot_ecg(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples, ecg_bioz_sensor_sample.ecg_lead_off);
            }
            else if (hpi_disp_get_curr_screen() == SCR_RESP)
            {
                hpi_resp_disp_draw_plot_resp(ecg_bioz_sensor_sample.bioz_samples, ecg_bioz_sensor_sample.bioz_num_samples, ecg_bioz_sensor_sample.bioz_lead_off);
            }
        }

        if (k_msgq_get(&q_plot_ppg, &ppg_sensor_sample, K_NO_WAIT) == 0)
        {
            if (hpi_disp_get_curr_screen() == SCR_PPG)
            {
                hpi_ppg_disp_draw_plot_ppg(ppg_sensor_sample.ppg_red_sample, ppg_sensor_sample.ppg_ir_sample, ppg_sensor_sample.ppg_lead_off);
            }
        }*/

        // CRITICAL: Process lv_task_handler() FIRST to handle screen changes immediately
        // BUT skip immediately after screen change to let new screen stabilize
        if (!screen_transitioning && !skip_next_lvgl_handler) {
            lv_task_handler();
        }
        
        // Clear skip flag after one iteration
        if (skip_next_lvgl_handler) {
            skip_next_lvgl_handler = false;
        }

        /*
         * REAL-TIME PLOTTING DISABLED
         * RP2040 + SPI display cannot handle real-time waveform rendering at 128 Hz.
         * Hardware limitations:
         * - M0+ CPU @ 133 MHz (no GPU acceleration)
         * - SPI display bandwidth ~10-20 MHz
         * - LVGL software rendering takes ~50ms per chart update
         * - Maximum achievable: ~20 Hz, Required: 128 Hz
         * 
         * For waveform viewing, use USB streaming to PC-based display.
         * Display shows vital signs only (HR, SpO2, RR, Temp).
         */
        
        // Drain plot queue ONLY when on plot screens to prevent overflow
        // This ensures zero interference with USB/BLE streaming when on Home screen
        if (hpi_disp_is_plot_screen_active()) {
            struct hpi_sensor_data_point_t sensor_all_data_point;
            for (int i = 0; i < 64; i++) {
                if (k_msgq_get(&q_hpi_plot_all_sample, &sensor_all_data_point, K_NO_WAIT) != 0) {
                    break;  // Queue empty
                }
                // Sample discarded - plotting disabled
            }
        }
        
        /*
        if (k_uptime_get_32() - last_batt_refresh > HPI_DISP_BATT_REFR_INT)
        {
            hpi_disp_update_batt_level(m_disp_batt_level);
            last_batt_refresh = k_uptime_get_32();
        }
        */

        // Process async screen change requests FIRST (from healthypi-move-fw pattern)
        // CRITICAL: Must be before periodic updates to prevent use-after-free
        if (k_sem_take(&sem_change_screen, K_NO_WAIT) == 0)
        {
            LOG_DBG("Processing screen change request: %d", g_screen);
            
            // Log LVGL memory status before screen change
            lv_mem_monitor_t mem_mon;
            lv_mem_monitor(&mem_mon);
            LOG_DBG("LVGL Memory before change: %d%% used, %u bytes free", 
                    mem_mon.used_pct, mem_mon.free_size);
            
            screen_transitioning = true;  // Block periodic updates during transition
            
            // Invalidate ALL label pointers IMMEDIATELY before screen destruction
            // This prevents ANY updates during screen transition/destruction
            // Header labels (recreated by draw_header)
            label_batt_level = NULL;
            label_batt_level_val = NULL;
            
            // Home screen labels (recreated by draw_scr_home)
            g_label_home_hr = NULL;
            g_label_home_spo2 = NULL;
            g_label_home_rr = NULL;
            g_label_home_temp_f = NULL;
            g_label_home_temp_c = NULL;
            
            bool valid_change = (g_screen >= SCR_LIST_START && g_screen < SCR_LIST_END) ||
                                (g_screen == SCR_HOME) ||
                                (g_screen == SCR_SPLASH);

            if (g_screen == SCR_SPLASH)
            {
                /* Special-case splash/welcome screen */
                draw_scr_welcome();
            }
            else if (valid_change && screen_func_table[g_screen].draw)
            {
                screen_func_table[g_screen].draw(g_scroll_dir);
                
                // Force LVGL to process all pending operations (layouts, animations)
                // This ensures the screen is fully rendered before resuming updates
                lv_task_handler();
                
                // Small safety margin to ensure screen fully initialized
                // Prevents race condition where label pointers are set but objects not committed
                k_msleep(10);
                
                // Log memory after screen change
                lv_mem_monitor(&mem_mon);
                LOG_DBG("LVGL Memory after change: %d%% used, %u bytes free", 
                        mem_mon.used_pct, mem_mon.free_size);
                
                screen_transitioning = false;  // Re-enable periodic updates
                skip_next_lvgl_handler = true;  // Skip lv_task_handler on next iteration
                
                // Skip periodic updates this iteration - let new screen settle
                // This prevents trying to update labels that were just created
                continue;
            }
            else
            {
                LOG_ERR("Invalid screen in change request: %d", g_screen);
                screen_transitioning = false;
            }
        }

        // Periodic vital signs updates (only when not transitioning screens)
        if (!screen_transitioning)
        {
            if (k_uptime_get_32() - last_temp_refresh > HPI_DISP_TEMP_REFR_INT)
            {
                hpi_disp_update_temp(m_disp_temp_f, m_disp_temp_c);
                last_temp_refresh = k_uptime_get_32();
                
                // Update detail screens when active
                int curr = hpi_disp_get_curr_screen();
                if (curr == SCR_TEMP) {
                    update_scr_temp();
                } // else if (curr == SCR_ALL_TRENDS) {
                  //     update_scr_all_trends();
                  // }
            }

            if (k_uptime_get_32() - last_hr_refresh > HPI_DISP_HR_REFR_INT)
            {
                hpi_scr_update_hr(m_disp_hr);
                last_hr_refresh = k_uptime_get_32();
                
                // Update detail screens when active
                int curr = hpi_disp_get_curr_screen();
                if (curr == SCR_HR) {
                    update_scr_hr();
                } // else if (curr == SCR_ALL_TRENDS) {
                  //     update_scr_all_trends();
                  // }
            }

            if (k_uptime_get_32() - last_spo2_refresh > HPI_DISP_SPO2_REFR_INT)
            {
                hpi_scr_update_spo2(m_disp_spo2);
                last_spo2_refresh = k_uptime_get_32();
                
                // Update detail screens when active
                int curr = hpi_disp_get_curr_screen();
                if (curr == SCR_SPO2) {
                    update_scr_spo2();
                } // else if (curr == SCR_ALL_TRENDS) {
                  //     update_scr_all_trends();
                  // }
            }

            if (k_uptime_get_32() - last_rr_refresh > HPI_DISP_RR_REFR_INT)
            {
                hpi_scr_update_rr(m_disp_rr);
                last_rr_refresh = k_uptime_get_32();
                
                // Update detail screens when active
                int curr = hpi_disp_get_curr_screen();
                if (curr == SCR_RR) {
                    update_scr_rr();
                } // else if (curr == SCR_ALL_TRENDS) {
                  //     update_scr_all_trends();
                  // }
            }
        }

        // Check for key presses with software debouncing for UP/DOWN
        if (k_sem_take(&sem_down_key_pressed, K_NO_WAIT) == 0)
        {
            uint32_t current_time = k_uptime_get_32();
            if ((current_time - last_down_press_time) >= BUTTON_DEBOUNCE_MS) {
                last_down_press_time = current_time;
                hpi_disp_change_event(HPI_SCR_EVENT_DOWN);
                LOG_DBG("DOWN button accepted (debounced)");
            } else {
                LOG_DBG("DOWN button ignored (debounce: %u ms elapsed)", 
                        current_time - last_down_press_time);
            }
        }

        if (k_sem_take(&sem_up_key_pressed, K_NO_WAIT) == 0)
        {
            uint32_t current_time = k_uptime_get_32();
            if ((current_time - last_up_press_time) >= BUTTON_DEBOUNCE_MS) {
                last_up_press_time = current_time;
                hpi_disp_change_event(HPI_SCR_EVENT_UP);
                LOG_DBG("UP button accepted (debounced)");
            } else {
                LOG_DBG("UP button ignored (debounce: %u ms elapsed)", 
                        current_time - last_up_press_time);
            }
        }

        // OK button long-press detection using Zephyr's input-longpress driver
        if (k_sem_take(&sem_ok_key_longpress, K_NO_WAIT) == 0)
        {
            if (hpi_disp_get_curr_screen() == SCR_HR) {
                LOG_INF("Long-press OK detected on HR screen - toggling source");
                scr_hr_toggle_source();
            } else {
                LOG_DBG("Long-press OK on non-HR screen - no action");
            }
        }

        // lv_task_handler() now called at TOP of loop for better responsiveness

        // Sleep for responsiveness without wasting CPU (plotting disabled)
        k_sleep(K_MSEC(20));  // Moderate sleep - responsive to button presses and vital sign updates

    }
}

static void disp_batt_status_listener(const struct zbus_channel *chan)
{
    const struct hpi_batt_status_t *batt_s = zbus_chan_const_msg(chan);

    // LOG_DBG("Ch Batt: %d, Charge: %d", batt_s->batt_level, batt_s->batt_charging);
    m_disp_batt_level = batt_s->batt_level;
    m_disp_batt_charging = batt_s->batt_charging;
}
ZBUS_LISTENER_DEFINE(disp_batt_lis, disp_batt_status_listener);

static void disp_hr_listener(const struct zbus_channel *chan)
{
    const struct hpi_hr_t *hpi_hr = zbus_chan_const_msg(chan);
    m_disp_hr = hpi_hr->hr;
    
    // Update vital stats history - throttle to once per second like other vitals
    // HR updates come at ~83 Hz (PPG sample rate), but stats should update at 1 Hz
    // to maintain a meaningful 60-second window instead of 720ms window
    static uint32_t last_stats_update = 0;
    uint32_t now = k_uptime_get_32();
    if (now - last_stats_update >= 1000) {  // Update stats once per second
        vital_stats_update_hr(hpi_hr->hr);
        last_stats_update = now;
    }
}
ZBUS_LISTENER_DEFINE(disp_hr_lis, disp_hr_listener);

static void disp_temp_listener(const struct zbus_channel *chan)
{
    const struct hpi_temp_t *hpi_temp = zbus_chan_const_msg(chan);
    m_disp_temp_f = hpi_temp->temp_f;
    m_disp_temp_c = hpi_temp->temp_c;
    
    // Update vital stats history
    vital_stats_update_temp((float)hpi_temp->temp_f);
}
ZBUS_LISTENER_DEFINE(disp_temp_lis, disp_temp_listener);

static void disp_spo2_listener(const struct zbus_channel *chan)
{
    const struct hpi_spo2_t *hpi_spo2 = zbus_chan_const_msg(chan);
    m_disp_spo2 = hpi_spo2->spo2;
    
    LOG_DBG("SpO2 ZBUS update: %d", hpi_spo2->spo2);
    
    // Update vital stats history
    vital_stats_update_spo2(hpi_spo2->spo2);
    // hpi_scr_update_spo2(hpi_spo2->spo2);
}
ZBUS_LISTENER_DEFINE(disp_spo2_lis, disp_spo2_listener);

static void disp_resp_rate_listener(const struct zbus_channel *chan)
{
    const struct hpi_resp_rate_t *hpi_resp_rate = zbus_chan_const_msg(chan);
    m_disp_rr = hpi_resp_rate->resp_rate;
    
    // Update vital stats history
    vital_stats_update_rr((uint8_t)hpi_resp_rate->resp_rate);
    // hpi_scr_update_rr(hpi_resp_rate->resp_rate);
}
ZBUS_LISTENER_DEFINE(disp_resp_rate_lis, disp_resp_rate_listener);

#define DISPLAY_SCREENS_THREAD_STACKSIZE 5000
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
