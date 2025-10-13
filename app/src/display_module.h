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


#pragma once

#include <lvgl.h>

enum scroll_dir
{
    SCROLL_UP,
    SCROLL_DOWN,
    SCROLL_LEFT,
    SCROLL_RIGHT,
};

enum hpi_scr_event
{
    HPI_SCR_EVENT_UP,
    HPI_SCR_EVENT_DOWN,
    HPI_SCR_EVENT_OK,
};

enum hpi_disp_screens
{
    SCR_LIST_START,

    SCR_HOME,
    // SCR_ALL_TRENDS,  // Disabled for now
    SCR_HR,
    SCR_SPO2,
    SCR_RR,
    SCR_TEMP,

    SCR_LIST_END
};

enum hpi_disp_screens_spl
{
    SCR_SPLASH=51
};

enum hpi_disp_op_mode
{
    OP_MODE_BASIC,
    OP_MODE_DISPLAY,
};

#define SCREEN_TRANS_TIME 100

#define SAMPLE_RATE 128

#define ECG_DISP_WINDOW_SIZE 256 // ~2 seconds at 128 Hz sample rate (optimized for performance)
#define RESP_DISP_WINDOW_SIZE 256 // ~2 seconds at 128 Hz sample rate
#define PPG_DISP_WINDOW_SIZE 100 // ~1 second at 100 Hz sample rate

#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

// Image declarations - LVGL images converted from PNG
// Standard sizes: 35px (small), 45px (medium), 70px (large), 100/120px (source)
LV_IMG_DECLARE(img_heart_35);
LV_IMG_DECLARE(img_heart_45);
LV_IMG_DECLARE(img_heart_70);
LV_IMG_DECLARE(img_heart_120);
LV_IMG_DECLARE(icon_spo2_35);
LV_IMG_DECLARE(icon_spo2_45);
LV_IMG_DECLARE(icon_spo2_70);
LV_IMG_DECLARE(icon_spo2_120);
LV_IMG_DECLARE(ecg_35);
LV_IMG_DECLARE(ecg_45);
LV_IMG_DECLARE(ecg_70);
LV_IMG_DECLARE(ecg_120);
LV_IMG_DECLARE(img_temp_45);
LV_IMG_DECLARE(img_temp_70);
LV_IMG_DECLARE(img_temp_100);
LV_IMG_DECLARE(img_resp_35);
LV_IMG_DECLARE(img_resp_45);
LV_IMG_DECLARE(img_resp_70);
LV_IMG_DECLARE(img_resp);
LV_IMG_DECLARE(img_failed_80);
LV_IMG_DECLARE(low_batt_100);

// Home Screen functions
void draw_scr_home(enum scroll_dir m_scroll_dir);
void hpi_scr_home_update_spo2(int spo2);
void hpi_scr_home_update_pr(int pr);
void hpi_scr_home_update_rr(int rr);
void hpi_scr_home_update_temp(float temp_f, float temp_c);
void hpi_scr_home_update_hr(int hr);
void hpi_scr_home_update_lead_off(bool ecg_lead_off, bool ppg_lead_off);

int hpi_disp_get_op_mode();
bool hpi_disp_is_plot_screen_active(void);

// HR Screen functions
void draw_scr_hr(enum scroll_dir m_scroll_dir);
void update_scr_hr(void);
void scr_hr_toggle_source(void);  // Toggle HR source (ECG/PPG)
void update_scr_hr_lead_off(bool ecg_lead_off);

// SpO2 Screen functions
void draw_scr_spo2(enum scroll_dir m_scroll_dir);
void update_scr_spo2(void);
void update_scr_spo2_lead_off(bool ppg_lead_off);  // Uses debounced state like home screen
void draw_scr_spo2(enum scroll_dir m_scroll_dir);
void update_scr_spo2(void);

// RR Screen functions
void draw_scr_rr(enum scroll_dir m_scroll_dir);
void update_scr_rr(void);

// Temperature Screen functions
void draw_scr_temp(enum scroll_dir m_scroll_dir);
void update_scr_temp(void);

// All Trends Screen functions
void draw_scr_all_trends(enum scroll_dir m_scroll_dir);
void update_scr_all_trends(void);

// Display helper functions
void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir);
void hpi_load_screen(int m_screen, enum scroll_dir m_scroll_dir);
void hpi_disp_update_batt_level(int batt_level);
void hpi_disp_change_event(enum hpi_scr_event);


void draw_scr_home_footer(lv_obj_t *parent);
void hpi_scr_update_spo2(int spo2);

void hpi_disp_set_curr_screen(int screen);
int hpi_disp_get_curr_screen(void);
bool hpi_disp_is_screen_transitioning(void);  // Check if screen change in progress

void draw_header(lv_obj_t *parent, bool showFWVersion);
void draw_footer(lv_obj_t *parent);

void draw_scr_welcome(void);

// Style declarations - extern for use in screen files
extern lv_style_t style_sub;
extern lv_style_t style_number_big;
extern lv_style_t style_number_medium;
extern lv_style_t style_header_black;
extern lv_style_t style_header_red;
extern lv_style_t style_header_green;
extern lv_style_t style_h1;
extern lv_style_t style_h2;
extern lv_style_t style_info;
extern lv_style_t style_icon;

// Font styles for detail screens
extern lv_style_t style_text_14;     // montserrat_14 - chart titles, stats, small text
extern lv_style_t style_text_16;     // montserrat_16 - units, regular text
extern lv_style_t style_text_20;     // montserrat_20 - medium values
extern lv_style_t style_text_24;     // montserrat_24 - icons
extern lv_style_t style_text_28;     // montserrat_28 - large values
extern lv_style_t style_text_42;     // montserrat_42 - extra large values