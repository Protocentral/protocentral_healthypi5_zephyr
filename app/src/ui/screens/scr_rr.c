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
#include "vital_stats.h"

LOG_MODULE_REGISTER(scr_rr, LOG_LEVEL_WRN);

lv_obj_t *scr_rr;

// GUI Components
static lv_obj_t *label_rr_current = NULL;
static lv_obj_t *label_rr_unit = NULL;
static lv_obj_t *label_stats_text = NULL;
static lv_obj_t *chart_rr_trend = NULL;
static lv_chart_series_t *ser_rr = NULL;
static lv_obj_t *lead_off_overlay = NULL;  // Lead-off warning overlay

// Chart configuration
#define RR_CHART_POINTS 120  // 2 minutes at 1 Hz
#define RR_CHART_MIN 5
#define RR_CHART_MAX 35

// Styles
extern lv_style_t style_sub;
extern lv_style_t style_number_big;

void draw_scr_rr(enum scroll_dir m_scroll_dir)
{
    scr_rr = lv_obj_create(NULL);
    draw_header(scr_rr, true);

    // Main container
    lv_obj_t *main_container = lv_obj_create(scr_rr);
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
    lv_obj_set_style_bg_color(info_panel, lv_palette_darken(LV_PALETTE_GREEN, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_panel, 12, LV_PART_MAIN);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Respiration icon (70px)
    lv_obj_t *img_resp = lv_img_create(info_panel);
    lv_img_set_src(img_resp, &img_resp_70);
    lv_obj_align(img_resp, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_img_recolor(img_resp, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_resp, LV_OPA_COVER, LV_PART_MAIN);

    // Current RR value container (left-center position)
    lv_obj_t *value_cont = lv_obj_create(info_panel);
    lv_obj_set_size(value_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(value_cont, LV_ALIGN_CENTER, -20, 0);
    lv_obj_set_flex_flow(value_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(value_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(value_cont, 2, LV_PART_MAIN);

    // Current RR value
    label_rr_current = lv_label_create(value_cont);
    lv_label_set_text(label_rr_current, "--");
    lv_obj_add_style(label_rr_current, &style_number_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_rr_current, lv_color_white(), LV_STATE_DEFAULT);

    // Unit label
    label_rr_unit = lv_label_create(value_cont);
    lv_label_set_text(label_rr_unit, "br/min");
    lv_obj_add_style(label_rr_unit, &style_sub, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_rr_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);

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
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_GREEN, 1), LV_STATE_DEFAULT);
    lv_obj_add_style(chart_title, &style_sub, LV_STATE_DEFAULT);

    // Create wide horizontal chart
    chart_rr_trend = lv_chart_create(chart_panel);
    lv_obj_set_size(chart_rr_trend, 444, 120);
    lv_obj_align(chart_rr_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling
    lv_obj_set_style_bg_color(chart_rr_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_rr_trend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_rr_trend, 5, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_rr_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_rr_trend, RR_CHART_POINTS);
    lv_chart_set_range(chart_rr_trend, LV_CHART_AXIS_PRIMARY_Y, RR_CHART_MIN, RR_CHART_MAX);
    lv_chart_set_div_line_count(chart_rr_trend, 4, 10);
    lv_chart_set_update_mode(chart_rr_trend, LV_CHART_UPDATE_MODE_SHIFT);
    
    // Grid styling
    lv_obj_set_style_line_color(chart_rr_trend, lv_color_make(30, 30, 30), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_rr_trend, 1, LV_PART_MAIN);
    
    // Add series
    ser_rr = lv_chart_add_series(chart_rr_trend, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    
    // Series styling
    lv_obj_set_style_line_color(chart_rr_trend, lv_palette_main(LV_PALETTE_GREEN), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_rr_trend, 2, LV_PART_ITEMS);
    
    // Chart is initialized with default values by lv_chart_set_point_count()
    // No need for manual initialization which can block the system

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_RR);
    hpi_show_screen(scr_rr, m_scroll_dir);
    
    // No initial update - will be called by display_module with actual values
}

void update_scr_rr(uint8_t rr_value, bool ecg_lead_off)
{
    // CRITICAL: Validate screen exists first before touching any objects
    // This prevents crashes when screen is deleted during/after transition
    if (scr_rr == NULL || !lv_obj_is_valid(scr_rr)) {
        return;  // Screen doesn't exist or was deleted, skip update
    }
    
    // Verify we're still on the RR screen - could have changed during delayed updates
    if (hpi_disp_get_curr_screen() != SCR_RR) {
        return;
    }
    
    if (label_rr_current == NULL) {
        return;
    }

    // Handle lead-off overlay visibility
    if (ecg_lead_off) {
        // Show warning overlay, hide normal value
        if (lead_off_overlay != NULL && lv_obj_is_valid(lead_off_overlay)) {
            lv_obj_clear_flag(lead_off_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (label_rr_current != NULL && lv_obj_is_valid(label_rr_current)) {
            lv_obj_add_flag(label_rr_current, LV_OBJ_FLAG_HIDDEN);
        }
        // Don't update chart or stats during lead-off
        return;
    } else {
        // Hide warning overlay, show normal value
        if (lead_off_overlay != NULL && lv_obj_is_valid(lead_off_overlay)) {
            lv_obj_add_flag(lead_off_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (label_rr_current != NULL && lv_obj_is_valid(label_rr_current)) {
            lv_obj_clear_flag(label_rr_current, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update current RR value - show "--" for invalid values
    if (rr_value == 0 || rr_value >= 60) {
        if (label_rr_current != NULL && lv_obj_is_valid(label_rr_current)) {
            lv_label_set_text(label_rr_current, "--");
        }
        // Don't update chart or stats for invalid values
        return;
    }
    
    // Valid RR value - update display
    if (label_rr_current != NULL && lv_obj_is_valid(label_rr_current)) {
        lv_label_set_text_fmt(label_rr_current, "%d", rr_value);
    }
    
    // Add to trend chart - validate before use
    if (chart_rr_trend != NULL && lv_obj_is_valid(chart_rr_trend) && ser_rr != NULL) {
        lv_chart_set_next_value(chart_rr_trend, ser_rr, rr_value);
        lv_chart_refresh(chart_rr_trend);
    }

    // Update statistics (60-second window)
    uint8_t rr_min = vital_stats_get_rr_min();
    uint8_t rr_max = vital_stats_get_rr_max();
    uint8_t rr_avg = vital_stats_get_rr_avg();

    if (label_stats_text != NULL && lv_obj_is_valid(label_stats_text)) {
        if (rr_min > 0) {
            lv_label_set_text_fmt(label_stats_text, 
                                 "Min: %d\nMax: %d\nAvg: %d",
                                 rr_min, rr_max, rr_avg);
        } else {
            lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
        }
    }

    // Note: Trend and time info removed in new layout
}

void scr_rr_delete(void)
{
    if (scr_rr != NULL) {
        lv_obj_del(scr_rr);
        scr_rr = NULL;
        label_rr_current = NULL;
        label_rr_unit = NULL;
        label_stats_text = NULL;
        chart_rr_trend = NULL;
        ser_rr = NULL;
        lead_off_overlay = NULL;
    }
}
