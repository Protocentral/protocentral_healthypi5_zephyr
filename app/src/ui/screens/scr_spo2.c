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

    // Main container
    lv_obj_t *main_container = lv_obj_create(scr_spo2);
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
    lv_obj_set_style_bg_color(info_panel, lv_palette_darken(LV_PALETTE_BLUE, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_panel, 12, LV_PART_MAIN);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // SpO2 icon (70px)
    lv_obj_t *img_spo2 = lv_img_create(info_panel);
    lv_img_set_src(img_spo2, &icon_spo2_70);
    lv_obj_align(img_spo2, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_img_recolor(img_spo2, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_spo2, LV_OPA_COVER, LV_PART_MAIN);

    // Current SpO2 value container (left-center position)
    lv_obj_t *value_cont = lv_obj_create(info_panel);
    lv_obj_set_size(value_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(value_cont, LV_ALIGN_CENTER, -20, 0);
    lv_obj_set_flex_flow(value_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(value_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(value_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(value_cont, 2, LV_PART_MAIN);

    // Current SpO2 value
    label_spo2_current = lv_label_create(value_cont);
    lv_label_set_text(label_spo2_current, "--");
    lv_obj_add_style(label_spo2_current, &style_number_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_spo2_current, lv_color_white(), LV_STATE_DEFAULT);

    // Unit label
    label_spo2_unit = lv_label_create(value_cont);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_add_style(label_spo2_unit, &style_sub, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_spo2_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);

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
    lv_obj_set_style_text_color(chart_title, lv_palette_lighten(LV_PALETTE_BLUE, 1), LV_STATE_DEFAULT);
    lv_obj_add_style(chart_title, &style_sub, LV_STATE_DEFAULT);

    // Create wide horizontal chart
    chart_spo2_trend = lv_chart_create(chart_panel);
    lv_obj_set_size(chart_spo2_trend, 444, 120);
    lv_obj_align(chart_spo2_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Chart styling
    lv_obj_set_style_bg_color(chart_spo2_trend, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_spo2_trend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_spo2_trend, 5, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart_spo2_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_spo2_trend, SPO2_CHART_POINTS);
    lv_chart_set_range(chart_spo2_trend, LV_CHART_AXIS_PRIMARY_Y, SPO2_CHART_MIN, SPO2_CHART_MAX);
    lv_chart_set_div_line_count(chart_spo2_trend, 4, 10);
    lv_chart_set_update_mode(chart_spo2_trend, LV_CHART_UPDATE_MODE_SHIFT);
    
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

    // Note: Trend and time info removed in new layout
}

void scr_spo2_delete(void)
{
    if (scr_spo2 != NULL) {
        lv_obj_del(scr_spo2);
        scr_spo2 = NULL;
        label_spo2_current = NULL;
        label_spo2_unit = NULL;
        label_stats_text = NULL;
        chart_spo2_trend = NULL;
        ser_spo2 = NULL;
    }
}
