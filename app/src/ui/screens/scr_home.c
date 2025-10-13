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
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <lvgl.h>
#include <stdio.h>
#include <app_version.h>

#include "hw_module.h"
#include "display_module.h"
#include "hpi_common_types.h"
#include "data_module.h"

lv_obj_t *scr_home;

extern lv_style_t style_sub;
extern lv_style_t style_number_big;
extern lv_style_t style_number_medium;

// GUI Labels - managed by display_module.c
extern lv_obj_t *g_label_home_hr;
extern lv_obj_t *g_label_home_hr_source;  // HR source indicator
extern lv_obj_t *g_label_home_spo2;
extern lv_obj_t *g_label_home_rr;
extern lv_obj_t *g_label_home_temp_f;
extern lv_obj_t *g_label_home_temp_c;

// Lead-off warning icons
static lv_obj_t *icon_hr_warn = NULL;
static lv_obj_t *icon_spo2_warn = NULL;

void draw_scr_home(enum scroll_dir m_scroll_dir)
{
    scr_home = lv_obj_create(NULL);
    draw_header(scr_home, true);
    
    // Optimized 2x2 grid layout for 480x320 - larger cards with better spacing
    static lv_coord_t col_dsc[] = {235, 235, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {130, 130, LV_GRID_TEMPLATE_LAST};

    /*Create main container with grid*/
    lv_obj_t *cont_home = lv_obj_create(scr_home);
    lv_obj_set_style_grid_column_dsc_array(cont_home, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont_home, row_dsc, 0);
    lv_obj_set_size(cont_home, 480, 270);
    lv_obj_set_pos(cont_home, 0, 30);
    lv_obj_set_layout(cont_home, LV_LAYOUT_GRID);
    lv_obj_set_style_bg_color(cont_home, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_home, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont_home, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cont_home, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(cont_home, 5, LV_PART_MAIN);
    lv_obj_clear_flag(cont_home, LV_OBJ_FLAG_SCROLLABLE);

    // Card 1: Heart Rate (top-left)
    lv_obj_t *obj_hr_card = lv_obj_create(cont_home);
    lv_obj_set_style_bg_color(obj_hr_card, lv_palette_darken(LV_PALETTE_CYAN, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj_hr_card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj_hr_card, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN);
    lv_obj_set_style_radius(obj_hr_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(obj_hr_card, LV_OBJ_FLAG_SCROLLABLE);

    // Card 2: SpO2 (top-right)
    lv_obj_t *obj_spo2_card = lv_obj_create(cont_home);
    lv_obj_set_style_bg_color(obj_spo2_card, lv_palette_darken(LV_PALETTE_BLUE, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj_spo2_card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj_spo2_card, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_radius(obj_spo2_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(obj_spo2_card, LV_OBJ_FLAG_SCROLLABLE);

    // Card 3: Respiration Rate (bottom-left)
    lv_obj_t *obj_rr_card = lv_obj_create(cont_home);
    lv_obj_set_style_bg_color(obj_rr_card, lv_palette_darken(LV_PALETTE_GREEN, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj_rr_card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj_rr_card, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_radius(obj_rr_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(obj_rr_card, LV_OBJ_FLAG_SCROLLABLE);

    // Card 4: Temperature (bottom-right)
    lv_obj_t *obj_temp_card = lv_obj_create(cont_home);
    lv_obj_set_style_bg_color(obj_temp_card, lv_palette_darken(LV_PALETTE_RED, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj_temp_card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj_temp_card, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_radius(obj_temp_card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(obj_temp_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_grid_cell(obj_hr_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(obj_spo2_card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(obj_rr_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(obj_temp_card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    // HR Icon - using heart image (35px for consistency)
    lv_obj_t *img_hr = lv_img_create(obj_hr_card);
    lv_img_set_src(img_hr, &img_heart_35);
    lv_obj_align(img_hr, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_img_recolor(img_hr, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_hr, LV_OPA_COVER, LV_PART_MAIN);

    // HR Title
    lv_obj_t *label_hr_title = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_title, "Heart Rate");
    lv_obj_align(label_hr_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(label_hr_title, &style_sub, LV_STATE_DEFAULT);

    // HR Value
    g_label_home_hr = lv_label_create(obj_hr_card);
    lv_label_set_text(g_label_home_hr, "--");
    lv_obj_align(g_label_home_hr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(g_label_home_hr, &style_number_big, LV_STATE_DEFAULT);

    // HR Unit
    lv_obj_t *label_hr_unit = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_unit, "bpm");
    lv_obj_align(label_hr_unit, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_style(label_hr_unit, &style_sub, LV_STATE_DEFAULT);

    // HR Warning icon (lead-off indicator) - initially hidden
    icon_hr_warn = lv_label_create(obj_hr_card);
    lv_label_set_text(icon_hr_warn, LV_SYMBOL_WARNING);
    lv_obj_align(icon_hr_warn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_text_font(icon_hr_warn, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(icon_hr_warn, lv_color_make(255, 160, 0), LV_STATE_DEFAULT);
    lv_obj_add_flag(icon_hr_warn, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // SpO2 Icon - using SpO2 image (35px for consistency)
    lv_obj_t *img_spo2 = lv_img_create(obj_spo2_card);
    lv_img_set_src(img_spo2, &icon_spo2_35);
    lv_obj_align(img_spo2, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_img_recolor(img_spo2, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_spo2, LV_OPA_COVER, LV_PART_MAIN);

    // SpO2 Title
    lv_obj_t *label_spo2_title = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2_title, "SpO2");
    lv_obj_align(label_spo2_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(label_spo2_title, &style_sub, LV_STATE_DEFAULT);

    // SpO2 Value
    g_label_home_spo2 = lv_label_create(obj_spo2_card);
    lv_label_set_text(g_label_home_spo2, "--");
    lv_obj_align(g_label_home_spo2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(g_label_home_spo2, &style_number_big, LV_STATE_DEFAULT);

    // SpO2 Unit
    lv_obj_t *label_spo2_unit = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_align(label_spo2_unit, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_style(label_spo2_unit, &style_sub, LV_STATE_DEFAULT);

    // SpO2 Warning icon (lead-off indicator) - initially hidden
    icon_spo2_warn = lv_label_create(obj_spo2_card);
    lv_label_set_text(icon_spo2_warn, LV_SYMBOL_WARNING);
    lv_obj_align(icon_spo2_warn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_text_font(icon_spo2_warn, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(icon_spo2_warn, lv_color_make(255, 160, 0), LV_STATE_DEFAULT);
    lv_obj_add_flag(icon_spo2_warn, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // RR Icon - using respiration image (35px for consistency)
    lv_obj_t *img_rr = lv_img_create(obj_rr_card);
    lv_img_set_src(img_rr, &img_resp_35);
    lv_obj_align(img_rr, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_img_recolor(img_rr, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_rr, LV_OPA_COVER, LV_PART_MAIN);

    // RR Title
    lv_obj_t *label_rr_title = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr_title, "Resp Rate");
    lv_obj_align(label_rr_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(label_rr_title, &style_sub, LV_STATE_DEFAULT);

    // RR Value
    g_label_home_rr = lv_label_create(obj_rr_card);
    lv_label_set_text(g_label_home_rr, "--");
    lv_obj_align(g_label_home_rr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(g_label_home_rr, &style_number_big, LV_STATE_DEFAULT);

    // RR Unit
    lv_obj_t *label_rr_unit = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr_unit, "bpm");
    lv_obj_align(label_rr_unit, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_style(label_rr_unit, &style_sub, LV_STATE_DEFAULT);

    // Temp Icon - using 35px temperature image for consistency
    lv_obj_t *img_temp = lv_img_create(obj_temp_card);
    lv_img_set_src(img_temp, &img_temp_45);
    lv_obj_align(img_temp, LV_ALIGN_TOP_LEFT, 8, 4);
    lv_obj_set_style_img_recolor(img_temp, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_temp, LV_OPA_COVER, LV_PART_MAIN);

    // Temp Title
    lv_obj_t *label_temp_title = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_title, "Temp");
    lv_obj_align(label_temp_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    // Temperature Fahrenheit container - centered with more space from title
    lv_obj_t *cont_temp_f = lv_obj_create(obj_temp_card);
    lv_obj_set_size(cont_temp_f, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(cont_temp_f, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_flex_flow(cont_temp_f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_temp_f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont_temp_f, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_temp_f, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont_temp_f, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(cont_temp_f, 4, LV_PART_MAIN);

    // Temp Value (°F) - main value
    g_label_home_temp_f = lv_label_create(cont_temp_f);
    lv_label_set_text(g_label_home_temp_f, "--");
    lv_obj_add_style(g_label_home_temp_f, &style_number_big, LV_STATE_DEFAULT);

    // Temp °F Unit
    lv_obj_t *label_temp_unit_f = lv_label_create(cont_temp_f);
    lv_label_set_text(label_temp_unit_f, "°F");
    lv_obj_add_style(label_temp_unit_f, &style_sub, LV_STATE_DEFAULT);

    // Temperature Celsius container - bottom
    lv_obj_t *cont_temp_c = lv_obj_create(obj_temp_card);
    lv_obj_set_size(cont_temp_c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(cont_temp_c, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_flex_flow(cont_temp_c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_temp_c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont_temp_c, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_temp_c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont_temp_c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(cont_temp_c, 3, LV_PART_MAIN);

    // Temp Value (°C) - smaller below
    g_label_home_temp_c = lv_label_create(cont_temp_c);
    lv_label_set_text(g_label_home_temp_c, "--");
    lv_obj_add_style(g_label_home_temp_c, &style_sub, LV_STATE_DEFAULT);

    // Temp °C Unit
    lv_obj_t *label_temp_unit_c = lv_label_create(cont_temp_c);
    lv_label_set_text(label_temp_unit_c, "°C");
    lv_obj_add_style(label_temp_unit_c, &style_sub, LV_STATE_DEFAULT);

    // Bottom navigation hint - with proper spacing from cards
    lv_obj_t *label_menu = lv_label_create(scr_home);
    lv_label_set_text(label_menu, "Use UP/DOWN to navigate screens");
    lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(label_menu, lv_color_hex(0xA0A0A0), LV_STATE_DEFAULT);  // Slightly brighter for visibility
    lv_obj_set_style_text_font(label_menu, &lv_font_montserrat_12, LV_STATE_DEFAULT);

    hpi_disp_set_curr_screen(SCR_HOME);

    hpi_show_screen(scr_home, m_scroll_dir);
}

void hpi_scr_home_update_temp(float temp_f, float temp_c)
{
    if (g_label_home_temp_f == NULL || g_label_home_temp_c == NULL)
        return;

    if (temp_c <= 0)
    {
        lv_label_set_text(g_label_home_temp_f, "--");
        lv_label_set_text(g_label_home_temp_c, "--");
        return;
    }

    // Manual float formatting (no FP printf support)
    char buf[16];
    int temp_f_int = (int)temp_f;
    int temp_f_dec = (int)((temp_f - temp_f_int) * 10);
    sprintf(buf, "%d.%d", temp_f_int, temp_f_dec);
    lv_label_set_text(g_label_home_temp_f, buf);
    
    int temp_c_int = (int)temp_c;
    int temp_c_dec = (int)((temp_c - temp_c_int) * 10);
    sprintf(buf, "%d.%d", temp_c_int, temp_c_dec);
    lv_label_set_text(g_label_home_temp_c, buf);
}

void hpi_scr_home_update_hr(int hr)
{
    if (g_label_home_hr == NULL)
        return;

    char buf[16];
    sprintf(buf, "%d", hr);
    lv_label_set_text(g_label_home_hr, buf);

    // Update HR source indicator
    if (g_label_home_hr_source != NULL)
    {
        enum hpi_hr_source source = hpi_data_get_hr_source();
        if (source == HR_SOURCE_ECG)
        {
            lv_label_set_text(g_label_home_hr_source, "Src:ECG");
            lv_obj_set_style_text_color(g_label_home_hr_source, lv_palette_main(LV_PALETTE_ORANGE), LV_STATE_DEFAULT);
        }
        else  // HR_SOURCE_PPG
        {
            lv_label_set_text(g_label_home_hr_source, "Src:PPG");
            lv_obj_set_style_text_color(g_label_home_hr_source, lv_palette_main(LV_PALETTE_CYAN), LV_STATE_DEFAULT);
        }
    }
}

void hpi_scr_home_update_spo2(int spo2)
{
    if (g_label_home_spo2 == NULL)
        return;

    // Display "--" for invalid readings (< 0) or zero (no valid data)
    if (spo2 <= 0)
    {
        lv_label_set_text(g_label_home_spo2, "--");
        return;
    }

    char buf[16];
    sprintf(buf, "%d", spo2);
    lv_label_set_text(g_label_home_spo2, buf);
}

void hpi_scr_home_update_lead_off(bool ecg_lead_off, bool ppg_lead_off)
{
    if (scr_home == NULL) return;
    
    // Update HR warning icon (ECG lead-off)
    if (icon_hr_warn != NULL) {
        if (ecg_lead_off) {
            lv_obj_clear_flag(icon_hr_warn, LV_OBJ_FLAG_HIDDEN);
            // Also show "--" for HR value
            if (g_label_home_hr != NULL) {
                lv_label_set_text(g_label_home_hr, "--");
            }
        } else {
            lv_obj_add_flag(icon_hr_warn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Update SpO2 warning icon (PPG lead-off - software detection based on signal quality)
    if (icon_spo2_warn != NULL) {
        if (ppg_lead_off) {
            lv_obj_clear_flag(icon_spo2_warn, LV_OBJ_FLAG_HIDDEN);
            // Also show "--" for SpO2 value
            if (g_label_home_spo2 != NULL) {
                lv_label_set_text(g_label_home_spo2, "--");
            }
        } else {
            lv_obj_add_flag(icon_spo2_warn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void hpi_scr_home_update_pr(int pr)
{
    // Pulse rate not shown on home screen in minimalist design
    // Function kept for compatibility
}

void hpi_scr_home_update_rr(int rr)
{
    if (g_label_home_rr == NULL)
        return;

    if (rr < 0)
    {
        lv_label_set_text(g_label_home_rr, "--");
        return;
    }

    char buf[16];
    sprintf(buf, "%d", rr);
    lv_label_set_text(g_label_home_rr, buf);
}