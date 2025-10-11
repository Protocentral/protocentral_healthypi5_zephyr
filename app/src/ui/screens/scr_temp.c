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


/*
 * Temperature Detail Screen
 * 
 * Minimalist design with:
 * - Large current temperature value (F and C)
 * - Min/Max/Avg statistics (60s window)
 * - Historical line chart (since boot)
 * 
 * Full 480x320 screen utilization with clean layout
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
#include "vital_stats.h"

LOG_MODULE_REGISTER(scr_temp, LOG_LEVEL_DBG);

lv_obj_t *scr_temp;

// GUI Components
static lv_obj_t *label_temp_current;  // Main temperature display
static lv_obj_t *label_temp_unit;
static lv_obj_t *label_stats_text;
static lv_obj_t *chart_temp_trend;
static lv_chart_series_t *ser_temp;

// Chart configuration
#define TEMP_CHART_POINTS 120  // 2 minutes at 1 Hz
#define TEMP_CHART_MIN_F 95
#define TEMP_CHART_MAX_F 105

// Styles
extern lv_style_t style_sub;
extern lv_style_t style_number_big;

// External variables
extern float m_disp_temp_f;
extern float m_disp_temp_c;

void draw_scr_temp(enum scroll_dir m_scroll_dir)
{
    scr_temp = lv_obj_create(NULL);
    draw_header(scr_temp, true);

    // Main container
    lv_obj_t *main_container = lv_obj_create(scr_temp);
    lv_obj_set_size(main_container, 480, 290);
    lv_obj_set_pos(main_container, 0, 30);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // Top info panel - horizontal layout with icon, value, and stats
    lv_obj_t *info_panel = lv_obj_create(main_container);
    lv_obj_set_size(info_panel, 460, 105);
    lv_obj_set_pos(info_panel, 0, 0);
    lv_obj_set_style_bg_color(info_panel, lv_palette_darken(LV_PALETTE_RED, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_panel, 12, LV_PART_MAIN);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature icon (70px)
    lv_obj_t *img_temp = lv_img_create(info_panel);
    lv_img_set_src(img_temp, &img_temp_70);
    lv_obj_align(img_temp, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_img_recolor(img_temp, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_temp, LV_OPA_COVER, LV_PART_MAIN);

    // Current temperature value container (left-center position)
    lv_obj_t *value_cont = lv_obj_create(info_panel);
    lv_obj_set_size(value_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(value_cont, LV_ALIGN_CENTER, -20, 0);
    lv_obj_set_flex_flow(value_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(value_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(value_cont, 2, LV_PART_MAIN);

    // Current temperature value (Fahrenheit)
    label_temp_current = lv_label_create(value_cont);
    lv_label_set_text(label_temp_current, "--");
    lv_obj_add_style(label_temp_current, &style_number_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_temp_current, lv_color_white(), LV_STATE_DEFAULT);

    // Unit label
    label_temp_unit = lv_label_create(value_cont);
    lv_label_set_text(label_temp_unit, "°F");
    lv_obj_add_style(label_temp_unit, &style_sub, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_temp_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);

    // Stats container (right side) - VERTICAL layout
    lv_obj_t *stats_cont = lv_obj_create(info_panel);
    lv_obj_set_size(stats_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(stats_cont, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stats_cont, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(stats_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(stats_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stats_cont, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_row(stats_cont, 5, LV_PART_MAIN);

    // Stats labels - using multiline label for compatibility
    label_stats_text = lv_label_create(stats_cont);
    lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    lv_obj_set_style_text_font(label_stats_text, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_stats_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(label_stats_text, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label_stats_text, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);

    // Chart panel - wide horizontal chart at bottom
    lv_obj_t *chart_panel = lv_obj_create(main_container);
    lv_obj_set_size(chart_panel, 460, 155);
    lv_obj_set_pos(chart_panel, 0, 115);
    lv_obj_set_style_bg_color(chart_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_panel, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_radius(chart_panel, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_panel, 8, LV_PART_MAIN);
    lv_obj_clear_flag(chart_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Chart title
    lv_obj_t *chart_title = lv_label_create(chart_panel);
    lv_label_set_text(chart_title, "TREND");
    lv_obj_align(chart_title, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_RED, 1), LV_STATE_DEFAULT);
    lv_obj_add_style(chart_title, &style_sub, LV_STATE_DEFAULT);

    // Create wide horizontal chart
    chart_temp_trend = lv_chart_create(chart_panel);
    lv_obj_set_size(chart_temp_trend, 444, 120);
    lv_obj_align(chart_temp_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling
    lv_obj_set_style_bg_color(chart_temp_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_temp_trend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_temp_trend, 5, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_temp_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_temp_trend, TEMP_CHART_POINTS);
    lv_chart_set_range(chart_temp_trend, LV_CHART_AXIS_PRIMARY_Y, TEMP_CHART_MIN_F, TEMP_CHART_MAX_F);
    lv_chart_set_div_line_count(chart_temp_trend, 4, 10);
    lv_chart_set_update_mode(chart_temp_trend, LV_CHART_UPDATE_MODE_SHIFT);
    
    // Grid styling
    lv_obj_set_style_line_color(chart_temp_trend, lv_color_make(30, 30, 30), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_temp_trend, 1, LV_PART_MAIN);
    
    // Add series
    ser_temp = lv_chart_add_series(chart_temp_trend, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    // Series styling
    lv_obj_set_style_line_color(chart_temp_trend, lv_palette_main(LV_PALETTE_RED), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_temp_trend, 2, LV_PART_ITEMS);
    
    // Initialize with no data
    for (int i = 0; i < TEMP_CHART_POINTS; i++) {
        lv_chart_set_next_value(chart_temp_trend, ser_temp, LV_CHART_POINT_NONE);
    }

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_TEMP);
    hpi_show_screen(scr_temp, m_scroll_dir);
    
    // Initial update
    update_scr_temp();
}

void update_scr_temp(void)
{
    if (scr_temp == NULL || label_temp_current == NULL) {
        return;
    }

    // Update current temperature values
    float current_temp_f = m_disp_temp_f;
    
    if (current_temp_f > 0 && current_temp_f < 120) {
        // Manual float formatting (no FP printf support)
        int temp_f_int = (int)current_temp_f;
        int temp_f_dec = (int)((current_temp_f - temp_f_int) * 10);
        
        lv_label_set_text_fmt(label_temp_current, "%d.%d", temp_f_int, temp_f_dec);
        
        // Add to trend chart (Fahrenheit) - only if chart is valid
        if (chart_temp_trend != NULL && ser_temp != NULL) {
            lv_chart_set_next_value(chart_temp_trend, ser_temp, (int16_t)current_temp_f);
            lv_chart_refresh(chart_temp_trend);
        }
    } else {
        lv_label_set_text(label_temp_current, "--");
    }

    // Update statistics (60-second window) - in Fahrenheit
    float temp_min = vital_stats_get_temp_min();
    float temp_max = vital_stats_get_temp_max();
    float temp_avg = vital_stats_get_temp_avg();

    if (temp_min > 0) {
        // Manual float formatting for statistics
        int min_int = (int)temp_min;
        int min_dec = (int)((temp_min - min_int) * 10);
        int max_int = (int)temp_max;
        int max_dec = (int)((temp_max - max_int) * 10);
        int avg_int = (int)temp_avg;
        int avg_dec = (int)((temp_avg - avg_int) * 10);
        
        lv_label_set_text_fmt(label_stats_text, 
                             "Min: %d.%d°F\nMax: %d.%d°F\nAvg: %d.%d°F",
                             min_int, min_dec, max_int, max_dec, avg_int, avg_dec);
    } else {
        lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    }

    // Note: Time info removed in new layout
}

void scr_temp_delete(void)
{
    if (scr_temp != NULL) {
        lv_obj_del(scr_temp);
        scr_temp = NULL;
        label_temp_current = NULL;
        label_temp_unit = NULL;
        label_stats_text = NULL;
        chart_temp_trend = NULL;
        ser_temp = NULL;
    }
}
