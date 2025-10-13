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
 * Heart Rate Detail Screen
 * 
 * Minimalist horizontal design:
 * - Top section: Icon + Current value + Stats (compact)
 * - Bottom section: Wide horizontal trend chart
 * - Optimized for 480x320 display
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

LOG_MODULE_REGISTER(scr_hr, LOG_LEVEL_DBG);

lv_obj_t *scr_hr;

// GUI Components
static lv_obj_t *label_hr_current;
static lv_obj_t *label_stats_text;  // Combined stats label (Min/Max/Avg)
static lv_obj_t *label_hr_source;  // HR source indicator (ECG/PPG)
static lv_obj_t *chart_hr_trend;
static lv_chart_series_t *ser_hr;
static lv_obj_t *lead_off_overlay = NULL;  // Lead-off warning overlay

// Chart configuration
#define HR_CHART_POINTS 120  // Reduced from 240 for better performance (still 3.7px per point)
#define HR_CHART_MIN 40
#define HR_CHART_MAX 180
#define HR_CHART_UPDATE_INTERVAL 2  // Update chart every N updates (throttling)

// Update counter for chart throttling
static uint8_t chart_update_counter = 0;

// Styles
extern lv_style_t style_sub;
extern lv_style_t style_number_big;

// External variables
extern uint16_t m_disp_hr;

void draw_scr_hr(enum scroll_dir m_scroll_dir)
{
    scr_hr = lv_obj_create(NULL);
    draw_header(scr_hr, true);

    // Main container
    lv_obj_t *main_container = lv_obj_create(scr_hr);
    lv_obj_set_size(main_container, 480, 290);
    lv_obj_set_pos(main_container, 0, 30);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 10, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // Top info panel - horizontal layout with icon, value, and stats
    lv_obj_t *info_panel = lv_obj_create(main_container);
    lv_obj_set_size(info_panel, 460, 105);  // Increased from 90 to 105 for source label
    lv_obj_set_pos(info_panel, 0, 0);
    lv_obj_set_style_bg_color(info_panel, lv_palette_darken(LV_PALETTE_CYAN, 4), LV_PART_MAIN);  // Changed to cyan for better visibility
    lv_obj_set_style_border_width(info_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN);  // Cyan border
    lv_obj_set_style_radius(info_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_panel, 12, LV_PART_MAIN);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Heart icon (70px)
    lv_obj_t *img_heart = lv_img_create(info_panel);
    lv_img_set_src(img_heart, &img_heart_70);
    lv_obj_align(img_heart, LV_ALIGN_LEFT_MID, 0, -8);  // Shifted up slightly
    lv_obj_set_style_img_recolor(img_heart, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN);  // Cyan icon
    lv_obj_set_style_img_recolor_opa(img_heart, LV_OPA_COVER, LV_PART_MAIN);

    // HR Source label (below icon) - shows "Source: ECG" or "Source: PPG"
    label_hr_source = lv_label_create(info_panel);
    lv_label_set_text(label_hr_source, "Source: ECG");
    lv_obj_align_to(label_hr_source, img_heart, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);  // Increased spacing
    lv_obj_set_style_text_font(label_hr_source, &lv_font_montserrat_16, LV_STATE_DEFAULT);  // Even larger font for visibility
    lv_obj_add_style(label_hr_source, &style_sub, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_hr_source, lv_color_white(), LV_STATE_DEFAULT);  // White for maximum contrast

    // Current HR value container (left-center position to avoid overlap with stats)
    lv_obj_t *value_cont = lv_obj_create(info_panel);
    lv_obj_set_size(value_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(value_cont, LV_ALIGN_CENTER, -20, 0);  // Adjusted positioning
    lv_obj_set_flex_flow(value_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(value_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(value_cont, 2, LV_PART_MAIN);

    // Current HR value
    label_hr_current = lv_label_create(value_cont);
    lv_label_set_text(label_hr_current, "--");
    lv_obj_add_style(label_hr_current, &style_number_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_hr_current, lv_color_white(), LV_STATE_DEFAULT);

    // Unit label
    lv_obj_t *label_unit = lv_label_create(value_cont);
    lv_label_set_text(label_unit, "bpm");
    lv_obj_add_style(label_unit, &style_sub, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);  // Brighter gray

    // Lead-off warning overlay (positioned over value, initially hidden)
    lead_off_overlay = lv_obj_create(info_panel);
    lv_obj_set_size(lead_off_overlay, 280, 80);
    lv_obj_align(lead_off_overlay, LV_ALIGN_CENTER, 20, 0);
    lv_obj_set_style_bg_color(lead_off_overlay, lv_color_make(60, 20, 20), LV_PART_MAIN);
    lv_obj_set_style_border_width(lead_off_overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(lead_off_overlay, lv_color_make(200, 50, 50), LV_PART_MAIN);
    lv_obj_set_style_radius(lead_off_overlay, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lead_off_overlay, 8, LV_PART_MAIN);
    lv_obj_add_flag(lead_off_overlay, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Warning icon
    lv_obj_t *icon_lead_off = lv_label_create(lead_off_overlay);
    lv_label_set_text(icon_lead_off, LV_SYMBOL_WARNING);
    lv_obj_align(icon_lead_off, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_font(icon_lead_off, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(icon_lead_off, lv_color_make(255, 180, 0), LV_STATE_DEFAULT);
    
    // Warning text
    lv_obj_t *label_lead_off_warning = lv_label_create(lead_off_overlay);
    lv_label_set_text(label_lead_off_warning, "ELECTRODES\nDISCONNECTED");
    lv_obj_align(label_lead_off_warning, LV_ALIGN_CENTER, 25, 0);
    lv_obj_set_style_text_font(label_lead_off_warning, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_lead_off_warning, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label_lead_off_warning, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    // Stats container (right side) - VERTICAL layout for better readability
    lv_obj_t *stats_cont = lv_obj_create(info_panel);
    lv_obj_set_size(stats_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(stats_cont, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stats_cont, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(stats_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(stats_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stats_cont, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_row(stats_cont, 5, LV_PART_MAIN);

    // Combined stats label (multiline for Min/Max/Avg)
    label_stats_text = lv_label_create(stats_cont);
    lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    lv_obj_set_style_text_font(label_stats_text, &lv_font_montserrat_16, LV_STATE_DEFAULT);  // 16pt font
    lv_obj_set_style_text_color(label_stats_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label_stats_text, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(label_stats_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_stats_text, LV_SIZE_CONTENT);

    // Chart panel - wide horizontal chart at bottom
    lv_obj_t *chart_panel = lv_obj_create(main_container);
    lv_obj_set_size(chart_panel, 460, 155);  // Reduced from 170 to 155
    lv_obj_set_pos(chart_panel, 0, 115);  // Moved down from 100 to 115
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
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_CYAN, 1), LV_STATE_DEFAULT);  // Cyan title
    lv_obj_add_style(chart_title, &style_sub, LV_STATE_DEFAULT);

    // Create wide horizontal chart
    chart_hr_trend = lv_chart_create(chart_panel);
    lv_obj_set_size(chart_hr_trend, 444, 120);  // Reduced from 135 to 120
    lv_obj_align(chart_hr_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling
    lv_obj_set_style_bg_color(chart_hr_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_hr_trend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_hr_trend, 5, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_hr_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_hr_trend, HR_CHART_POINTS);
    lv_chart_set_range(chart_hr_trend, LV_CHART_AXIS_PRIMARY_Y, HR_CHART_MIN, HR_CHART_MAX);
    lv_chart_set_div_line_count(chart_hr_trend, 4, 10);  // 4 horizontal, 10 vertical divisions
    lv_chart_set_update_mode(chart_hr_trend, LV_CHART_UPDATE_MODE_SHIFT);
    
    // Grid styling
    lv_obj_set_style_line_color(chart_hr_trend, lv_color_make(30, 30, 30), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_hr_trend, 1, LV_PART_MAIN);
    
    // Add series
    ser_hr = lv_chart_add_series(chart_hr_trend, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);  // Cyan for better visibility
    
    // Series styling
    lv_obj_set_style_line_color(chart_hr_trend, lv_palette_main(LV_PALETTE_CYAN), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_hr_trend, 2, LV_PART_ITEMS);
    
    // Initialize chart with no data - yield every 20 points to prevent blocking data thread
    for (int i = 0; i < HR_CHART_POINTS; i++) {
        lv_chart_set_next_value(chart_hr_trend, ser_hr, LV_CHART_POINT_NONE);
        if ((i % 20) == 0) {
            k_yield();  // Allow data thread to run
        }
    }

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_HR);
    hpi_show_screen(scr_hr, m_scroll_dir);
    
    // Initial update
    update_scr_hr();
}

void update_scr_hr(void)
{
    if (scr_hr == NULL || label_hr_current == NULL) {
        return;
    }

    // Additional validation: Check if screen is valid before accessing children
    if (!lv_obj_is_valid(scr_hr)) {
        LOG_DBG("HR screen not yet valid, skipping update");
        return;
    }

    // Update HR source label
    if (label_hr_source != NULL && lv_obj_is_valid(label_hr_source)) {
        enum hpi_hr_source current_source = hpi_data_get_hr_source();
        lv_label_set_text(label_hr_source, current_source == HR_SOURCE_ECG ? "Source: ECG" : "Source: PPG");
        
        // Change color based on source: Cyan for ECG, Green for PPG
        lv_color_t source_color = current_source == HR_SOURCE_ECG ? 
                                  lv_palette_main(LV_PALETTE_CYAN) : 
                                  lv_palette_main(LV_PALETTE_GREEN);
        lv_obj_set_style_text_color(label_hr_source, source_color, LV_STATE_DEFAULT);
    }

    // Update current HR value
    uint16_t current_hr = m_disp_hr;
    if (current_hr > 0 && current_hr < 255) {
        if (label_hr_current != NULL && lv_obj_is_valid(label_hr_current)) {
            lv_label_set_text_fmt(label_hr_current, "%d", current_hr);
        }
        
        // Throttled chart update - only update every N calls for better performance
        chart_update_counter++;
        if (chart_update_counter >= HR_CHART_UPDATE_INTERVAL) {
            chart_update_counter = 0;
            if (chart_hr_trend != NULL && lv_obj_is_valid(chart_hr_trend) && ser_hr != NULL) {
                lv_chart_set_next_value(chart_hr_trend, ser_hr, current_hr);
                lv_chart_refresh(chart_hr_trend);
            }
        }
    } else {
        if (label_hr_current != NULL && lv_obj_is_valid(label_hr_current)) {
            lv_label_set_text(label_hr_current, "--");
        }
    }

    // Update statistics from vital_stats module - using multiline label like other screens
    if (label_stats_text != NULL && lv_obj_is_valid(label_stats_text)) {
        uint16_t hr_min = vital_stats_get_hr_min();
        uint16_t hr_max = vital_stats_get_hr_max();
        uint16_t hr_avg = vital_stats_get_hr_avg();

        if (hr_min > 0) {
            char stats_buf[64];
            snprintf(stats_buf, sizeof(stats_buf), "Min: %d\nMax: %d\nAvg: %d", hr_min, hr_max, hr_avg);
            lv_label_set_text(label_stats_text, stats_buf);
            LOG_INF("HR Stats displayed - Min: %d, Max: %d, Avg: %d", hr_min, hr_max, hr_avg);
        } else {
            lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
        }
    }
}

/**
 * Update lead-off status on HR screen
 */
void update_scr_hr_lead_off(bool ecg_lead_off)
{
    // CRITICAL: Only update if we're actually on the HR screen
    if (hpi_disp_get_curr_screen() != SCR_HR) {
        return;
    }
    
    if (scr_hr == NULL || lead_off_overlay == NULL) return;
    
    // Additional validation: Check if objects are still valid in LVGL
    if (!lv_obj_is_valid(scr_hr) || !lv_obj_is_valid(lead_off_overlay)) {
        LOG_DBG("HR screen objects not yet valid, skipping lead-off update");
        return;
    }
    
    if (ecg_lead_off) {
        // Show warning overlay, hide normal value
        if (lv_obj_is_valid(lead_off_overlay)) {
            lv_obj_clear_flag(lead_off_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (label_hr_current != NULL && lv_obj_is_valid(label_hr_current)) {
            lv_obj_add_flag(label_hr_current, LV_OBJ_FLAG_HIDDEN);
        }
        LOG_WRN("HR Screen: ECG lead-off warning displayed");
    } else {
        // Hide warning, show normal value
        // Re-validate before each operation (object could become invalid between checks)
        if (lead_off_overlay != NULL && lv_obj_is_valid(lead_off_overlay)) {
            lv_obj_add_flag(lead_off_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (label_hr_current != NULL && lv_obj_is_valid(label_hr_current)) {
            lv_obj_clear_flag(label_hr_current, LV_OBJ_FLAG_HIDDEN);
        }
        LOG_INF("HR Screen: ECG lead-off cleared");
    }
}

/**
 * Toggle HR source between ECG and PPG
 * Called when user long-presses OK button on HR screen
 */
void scr_hr_toggle_source(void)
{
    enum hpi_hr_source current = hpi_data_get_hr_source();
    enum hpi_hr_source new_source = (current == HR_SOURCE_ECG) ? HR_SOURCE_PPG : HR_SOURCE_ECG;
    
    hpi_data_set_hr_source(new_source);
    
    LOG_INF("HR source toggled to: %s", new_source == HR_SOURCE_ECG ? "ECG" : "PPG");
    
    // Force immediate update
    update_scr_hr();
    
    // TODO: Show toast notification "Switched to PPG HR" or "Switched to ECG HR"
}

void scr_hr_delete(void)
{
    if (scr_hr != NULL) {
        lv_obj_del(scr_hr);
        scr_hr = NULL;
        label_hr_current = NULL;
        label_stats_text = NULL;
        label_hr_source = NULL;
        chart_hr_trend = NULL;
        ser_hr = NULL;
        lead_off_overlay = NULL;   // FIX: NULL out overlay pointer
        chart_update_counter = 0;  // Reset throttle counter
    }
}
