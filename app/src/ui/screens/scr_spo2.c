/*
 * SpO2 Detail Screen
 * 
 * Minimalist design with:
 * - Large current SpO2 value
 * - Min/Max/Avg statistics (60s window)
 * - Trend indicator
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

LOG_MODULE_REGISTER(scr_spo2, LOG_LEVEL_DBG);

lv_obj_t *scr_spo2;

// GUI Components
static lv_obj_t *label_spo2_current;
static lv_obj_t *label_spo2_unit;
static lv_obj_t *label_stats_text;
static lv_obj_t *label_trend;
static lv_obj_t *label_time_info;
static lv_obj_t *chart_spo2_trend;
static lv_chart_series_t *ser_spo2;

// Chart configuration
#define SPO2_CHART_POINTS 120  // 2 minutes at 1 Hz
#define SPO2_CHART_MIN 85
#define SPO2_CHART_MAX 100

// Styles
extern lv_style_t style_sub;
extern lv_style_t style_number_big;

// External variables
extern uint8_t m_disp_spo2;

void draw_scr_spo2(enum scroll_dir m_scroll_dir)
{
    scr_spo2 = lv_obj_create(NULL);
    draw_header(scr_spo2, true);

    // Main container - black background for OLED-friendly high contrast
    lv_obj_t *main_container = lv_obj_create(scr_spo2);
    lv_obj_set_size(main_container, 480, 290);
    lv_obj_set_pos(main_container, 0, 30);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // Left panel - Current value and stats (200px wide)
    lv_obj_t *left_panel = lv_obj_create(main_container);
    lv_obj_set_size(left_panel, 200, 290);
    lv_obj_set_pos(left_panel, 0, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(left_panel, 15, LV_PART_MAIN);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Right panel - Trend chart (280px wide)
    lv_obj_t *right_panel = lv_obj_create(main_container);
    lv_obj_set_size(right_panel, 280, 290);
    lv_obj_set_pos(right_panel, 200, 0);
    lv_obj_set_style_bg_color(right_panel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(right_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(right_panel, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
    lv_obj_set_style_border_side(right_panel, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_panel, 10, LV_PART_MAIN);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // === LEFT PANEL CONTENT ===

    // O2 icon
    lv_obj_t *icon_o2 = lv_label_create(left_panel);
    lv_label_set_text(icon_o2, "O2");
    lv_obj_align(icon_o2, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(icon_o2, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT);
    lv_obj_add_style(icon_o2, &style_text_24, LV_STATE_DEFAULT);

    // Current SpO2 value - large and centered
    label_spo2_current = lv_label_create(left_panel);
    lv_label_set_text(label_spo2_current, "--");
    lv_obj_align(label_spo2_current, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_text_color(label_spo2_current, lv_palette_lighten(LV_PALETTE_BLUE, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_spo2_current, &style_text_42, LV_STATE_DEFAULT);

    // Unit label
    label_spo2_unit = lv_label_create(left_panel);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_align_to(label_spo2_unit, label_spo2_current, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_text_color(label_spo2_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_spo2_unit, &style_text_16, LV_STATE_DEFAULT);

    // Divider line
    lv_obj_t *line_div = lv_obj_create(left_panel);
    lv_obj_set_size(line_div, 170, 2);
    lv_obj_align(line_div, LV_ALIGN_TOP_LEFT, 0, 140);
    lv_obj_set_style_bg_color(line_div, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_MAIN);
    lv_obj_set_style_border_width(line_div, 0, LV_PART_MAIN);

    // Statistics label (multiline)
    label_stats_text = lv_label_create(left_panel);
    lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    lv_obj_align(label_stats_text, LV_ALIGN_TOP_LEFT, 0, 155);
    lv_obj_set_style_text_color(label_stats_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(label_stats_text, &style_text_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(label_stats_text, 4, LV_STATE_DEFAULT);

    // Trend indicator
    label_trend = lv_label_create(left_panel);
    lv_label_set_text(label_trend, LV_SYMBOL_UP " Rising");
    lv_obj_align(label_trend, LV_ALIGN_BOTTOM_LEFT, 0, -25);
    lv_obj_set_style_text_color(label_trend, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_DEFAULT);
    lv_obj_add_style(label_trend, &style_text_14, LV_STATE_DEFAULT);

    // Time info (last update / uptime)
    label_time_info = lv_label_create(left_panel);
    lv_label_set_text(label_time_info, "Updated: --");
    lv_obj_align(label_time_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_color(label_time_info, lv_color_make(140, 140, 140), LV_STATE_DEFAULT);
    lv_obj_add_style(label_time_info, &style_text_14, LV_STATE_DEFAULT);

    // === RIGHT PANEL CONTENT - TREND CHART ===

    // Chart title
    lv_obj_t *chart_title = lv_label_create(right_panel);
    lv_label_set_text(chart_title, "TREND (2 MIN)");
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_BLUE, 1), LV_STATE_DEFAULT);
    lv_obj_add_style(chart_title, &style_text_14, LV_STATE_DEFAULT);

    // Create chart
    chart_spo2_trend = lv_chart_create(right_panel);
    lv_obj_set_size(chart_spo2_trend, 260, 240);
    lv_obj_align(chart_spo2_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling - minimalist
    lv_obj_set_style_bg_color(chart_spo2_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_spo2_trend, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_spo2_trend, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_spo2_trend, 8, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_spo2_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_spo2_trend, SPO2_CHART_POINTS);
    lv_chart_set_range(chart_spo2_trend, LV_CHART_AXIS_PRIMARY_Y, SPO2_CHART_MIN, SPO2_CHART_MAX);
    lv_chart_set_div_line_count(chart_spo2_trend, 5, 8);
    lv_chart_set_update_mode(chart_spo2_trend, LV_CHART_UPDATE_MODE_SHIFT);  // Shift mode for scrolling
    
    // Grid styling
    lv_obj_set_style_line_color(chart_spo2_trend, lv_color_make(30, 30, 30), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_spo2_trend, 1, LV_PART_MAIN);
    
    // Add series
    ser_spo2 = lv_chart_add_series(chart_spo2_trend, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    
    // Series styling
    lv_obj_set_style_line_color(chart_spo2_trend, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_spo2_trend, 2, LV_PART_ITEMS);
    
    // Initialize with no data
    for (int i = 0; i < SPO2_CHART_POINTS; i++) {
        lv_chart_set_next_value(chart_spo2_trend, ser_spo2, LV_CHART_POINT_NONE);
    }

    // Y-axis labels (min/max)
    lv_obj_t *label_y_max = lv_label_create(right_panel);
    lv_label_set_text_fmt(label_y_max, "%d", SPO2_CHART_MAX);
    lv_obj_align(label_y_max, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_text_color(label_y_max, lv_color_make(120, 120, 120), LV_STATE_DEFAULT);
    lv_obj_add_style(label_y_max, &style_text_14, LV_STATE_DEFAULT);

    lv_obj_t *label_y_min = lv_label_create(right_panel);
    lv_label_set_text_fmt(label_y_min, "%d", SPO2_CHART_MIN);
    lv_obj_align(label_y_min, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_obj_set_style_text_color(label_y_min, lv_color_make(120, 120, 120), LV_STATE_DEFAULT);
    lv_obj_add_style(label_y_min, &style_text_14, LV_STATE_DEFAULT);

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_SPO2);
    hpi_show_screen(scr_spo2, m_scroll_dir);
    
    // Initial update
    update_scr_spo2();
}

void update_scr_spo2(void)
{
    if (scr_spo2 == NULL || label_spo2_current == NULL) {
        return;
    }

    // Update current SpO2 value
    uint8_t current_spo2 = m_disp_spo2;
    if (current_spo2 > 0 && current_spo2 <= 100) {
        lv_label_set_text_fmt(label_spo2_current, "%d", current_spo2);
        
        // Add to trend chart
        lv_chart_set_next_value(chart_spo2_trend, ser_spo2, current_spo2);
        lv_chart_refresh(chart_spo2_trend);
    } else {
        lv_label_set_text(label_spo2_current, "--");
    }

    // Update statistics (60-second window)
    uint8_t spo2_min = vital_stats_get_spo2_min();
    uint8_t spo2_max = vital_stats_get_spo2_max();
    uint8_t spo2_avg = vital_stats_get_spo2_avg();

    if (spo2_min > 0) {
        lv_label_set_text_fmt(label_stats_text, 
                             "Min: %d%%\nMax: %d%%\nAvg: %d%%",
                             spo2_min, spo2_max, spo2_avg);
    } else {
        lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    }

    // Update trend indicator (using HR trend as proxy - could implement SpO2 trend later)
    int8_t trend = vital_stats_get_hr_trend();  // TODO: Add spo2 trend to vital_stats
    if (trend > 0) {
        lv_label_set_text(label_trend, LV_SYMBOL_UP " Rising");
        lv_obj_set_style_text_color(label_trend, lv_palette_main(LV_PALETTE_GREEN), LV_STATE_DEFAULT);
    } else if (trend < 0) {
        lv_label_set_text(label_trend, LV_SYMBOL_DOWN " Falling");
        lv_obj_set_style_text_color(label_trend, lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(label_trend, LV_SYMBOL_MINUS " Stable");
        lv_obj_set_style_text_color(label_trend, lv_palette_main(LV_PALETTE_GREY), LV_STATE_DEFAULT);
    }

    // Update time info
    uint32_t time_since_update = vital_stats_get_spo2_time_since_update();
    if (time_since_update < 5) {
        lv_label_set_text(label_time_info, "Just now");
    } else if (time_since_update < 60) {
        lv_label_set_text_fmt(label_time_info, "%ds ago", time_since_update);
    } else if (time_since_update != 0xFFFFFFFF) {
        lv_label_set_text_fmt(label_time_info, "%dm ago", time_since_update / 60);
    } else {
        lv_label_set_text(label_time_info, "No data");
    }
}

void scr_spo2_delete(void)
{
    if (scr_spo2 != NULL) {
        lv_obj_del(scr_spo2);
        scr_spo2 = NULL;
        label_spo2_current = NULL;
        label_spo2_unit = NULL;
        label_stats_text = NULL;
        label_trend = NULL;
        label_time_info = NULL;
        chart_spo2_trend = NULL;
        ser_spo2 = NULL;
    }
}
