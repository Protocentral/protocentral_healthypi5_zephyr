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
static lv_obj_t *label_temp_f;
static lv_obj_t *label_temp_c;
static lv_obj_t *label_unit_f;
static lv_obj_t *label_unit_c;
static lv_obj_t *label_stats_text;
static lv_obj_t *label_time_info;
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

    // Main container - black background for OLED-friendly high contrast
    lv_obj_t *main_container = lv_obj_create(scr_temp);
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
    lv_obj_set_style_border_color(right_panel, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
    lv_obj_set_style_border_side(right_panel, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_panel, 10, LV_PART_MAIN);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // === LEFT PANEL CONTENT ===

    // Temperature icon
    lv_obj_t *icon_temp = lv_label_create(left_panel);
    lv_label_set_text(icon_temp, LV_SYMBOL_CHARGE);  // Using charge symbol as thermometer
    lv_obj_align(icon_temp, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(icon_temp, lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
    lv_obj_add_style(icon_temp, &style_text_24, LV_STATE_DEFAULT);

    // Current Temp Fahrenheit - large
    label_temp_f = lv_label_create(left_panel);
    lv_label_set_text(label_temp_f, "--");
    lv_obj_align(label_temp_f, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_text_color(label_temp_f, lv_palette_lighten(LV_PALETTE_RED, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_temp_f, &style_text_28, LV_STATE_DEFAULT);

    // Unit label F
    label_unit_f = lv_label_create(left_panel);
    lv_label_set_text(label_unit_f, "°F");
    lv_obj_align_to(label_unit_f, label_temp_f, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    lv_obj_set_style_text_color(label_unit_f, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_unit_f, &style_text_14, LV_STATE_DEFAULT);

    // Current Temp Celsius - medium
    label_temp_c = lv_label_create(left_panel);
    lv_label_set_text(label_temp_c, "--");
    lv_obj_align(label_temp_c, LV_ALIGN_TOP_MID, 0, 115);
    lv_obj_set_style_text_color(label_temp_c, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(label_temp_c, &style_text_20, LV_STATE_DEFAULT);

    // Unit label C
    label_unit_c = lv_label_create(left_panel);
    lv_label_set_text(label_unit_c, "°C");
    lv_obj_align_to(label_unit_c, label_temp_c, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    lv_obj_set_style_text_color(label_unit_c, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_unit_c, &style_text_14, LV_STATE_DEFAULT);

    // Divider line
    lv_obj_t *line_div = lv_obj_create(left_panel);
    lv_obj_set_size(line_div, 170, 2);
    lv_obj_align(line_div, LV_ALIGN_TOP_LEFT, 0, 165);
    lv_obj_set_style_bg_color(line_div, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN);
    lv_obj_set_style_border_width(line_div, 0, LV_PART_MAIN);

    // Statistics label (multiline) - in Fahrenheit
    label_stats_text = lv_label_create(left_panel);
    lv_label_set_text(label_stats_text, "Min: --\nMax: --\nAvg: --");
    lv_obj_align(label_stats_text, LV_ALIGN_TOP_LEFT, 0, 180);
    lv_obj_set_style_text_color(label_stats_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(label_stats_text, &style_text_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(label_stats_text, 3, LV_STATE_DEFAULT);

    // Time info (last update)
    label_time_info = lv_label_create(left_panel);
    lv_label_set_text(label_time_info, "Updated: --");
    lv_obj_align(label_time_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_color(label_time_info, lv_color_make(140, 140, 140), LV_STATE_DEFAULT);
    lv_obj_add_style(label_time_info, &style_text_14, LV_STATE_DEFAULT);

    // === RIGHT PANEL CONTENT - TREND CHART ===

    // Chart title
    lv_obj_t *chart_title = lv_label_create(right_panel);
    lv_label_set_text(chart_title, "TREND (2 MIN) °F");
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_RED, 1), LV_STATE_DEFAULT);
    lv_obj_add_style(chart_title, &style_text_14, LV_STATE_DEFAULT);

    // Create chart
    chart_temp_trend = lv_chart_create(right_panel);
    lv_obj_set_size(chart_temp_trend, 260, 240);
    lv_obj_align(chart_temp_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling - minimalist
    lv_obj_set_style_bg_color(chart_temp_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_temp_trend, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_temp_trend, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_temp_trend, 8, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_temp_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_temp_trend, TEMP_CHART_POINTS);
    lv_chart_set_range(chart_temp_trend, LV_CHART_AXIS_PRIMARY_Y, TEMP_CHART_MIN_F, TEMP_CHART_MAX_F);
    lv_chart_set_div_line_count(chart_temp_trend, 5, 8);
    lv_chart_set_update_mode(chart_temp_trend, LV_CHART_UPDATE_MODE_SHIFT);  // Shift mode for scrolling
    
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

    // Y-axis labels (min/max)
    lv_obj_t *label_y_max = lv_label_create(right_panel);
    lv_label_set_text_fmt(label_y_max, "%d", TEMP_CHART_MAX_F);
    lv_obj_align(label_y_max, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_text_color(label_y_max, lv_color_make(120, 120, 120), LV_STATE_DEFAULT);
    lv_obj_add_style(label_y_max, &style_text_14, LV_STATE_DEFAULT);

    lv_obj_t *label_y_min = lv_label_create(right_panel);
    lv_label_set_text_fmt(label_y_min, "%d", TEMP_CHART_MIN_F);
    lv_obj_align(label_y_min, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_obj_set_style_text_color(label_y_min, lv_color_make(120, 120, 120), LV_STATE_DEFAULT);
    lv_obj_add_style(label_y_min, &style_text_14, LV_STATE_DEFAULT);

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_TEMP);
    hpi_show_screen(scr_temp, m_scroll_dir);
    
    // Initial update
    update_scr_temp();
}

void update_scr_temp(void)
{
    if (scr_temp == NULL || label_temp_f == NULL) {
        return;
    }

    // Update current temperature values
    float current_temp_f = m_disp_temp_f;
    float current_temp_c = m_disp_temp_c;
    
    if (current_temp_f > 0 && current_temp_f < 120) {
        // Manual float formatting (no FP printf support)
        int temp_f_int = (int)current_temp_f;
        int temp_f_dec = (int)((current_temp_f - temp_f_int) * 10);
        int temp_c_int = (int)current_temp_c;
        int temp_c_dec = (int)((current_temp_c - temp_c_int) * 10);
        
        lv_label_set_text_fmt(label_temp_f, "%d.%d", temp_f_int, temp_f_dec);
        lv_label_set_text_fmt(label_temp_c, "%d.%d", temp_c_int, temp_c_dec);
        
        // Add to trend chart (Fahrenheit) - only if chart is valid
        if (chart_temp_trend != NULL && ser_temp != NULL) {
            lv_chart_set_next_value(chart_temp_trend, ser_temp, (int16_t)current_temp_f);
            lv_chart_refresh(chart_temp_trend);
            LOG_DBG("Added temp to chart: %d F", (int)current_temp_f);
        }
    } else {
        lv_label_set_text(label_temp_f, "--");
        lv_label_set_text(label_temp_c, "--");
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

    // Update time info
    uint32_t time_since_update = vital_stats_get_temp_time_since_update();
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

void scr_temp_delete(void)
{
    if (scr_temp != NULL) {
        lv_obj_del(scr_temp);
        scr_temp = NULL;
        label_temp_f = NULL;
        label_temp_c = NULL;
        label_unit_f = NULL;
        label_unit_c = NULL;
        label_stats_text = NULL;
        label_time_info = NULL;
        chart_temp_trend = NULL;
        ser_temp = NULL;
    }
}
