/*
 * All Trends Overview Screen
 * 
 * Column-based layout showing all 4 vital signs with mini trend charts:
 * - Heart Rate (Orange)
 * - SpO2 (Blue)
 *    // SpO2 Icon - using SpO2 image
    lv_obj_t *img_spo2 = lv_img_create(spo2_row);
    lv_img_set_src(img_spo2, &icon_spo2_35);
    lv_obj_set_pos(img_spo2, 5, 17);espiration Rate (Green)
 * - Temperature (Red)
 * 
 * Each row: [Icon] [Current Value] [Mini Chart]
 * Full 480x320 screen with 4 rows of ~70px each
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

LOG_MODULE_REGISTER(scr_all_trends, LOG_LEVEL_DBG);

lv_obj_t *scr_all_trends;

// GUI Components - HR Row
static lv_obj_t *label_hr_value;
static lv_obj_t *chart_hr_mini;
static lv_chart_series_t *ser_hr_mini;

// GUI Components - SpO2 Row
static lv_obj_t *label_spo2_value;
static lv_obj_t *chart_spo2_mini;
static lv_chart_series_t *ser_spo2_mini;

// GUI Components - RR Row
static lv_obj_t *label_rr_value;
static lv_obj_t *chart_rr_mini;
static lv_chart_series_t *ser_rr_mini;

// GUI Components - Temp Row
static lv_obj_t *label_temp_value;
static lv_obj_t *chart_temp_mini;
static lv_chart_series_t *ser_temp_mini;

// Chart configuration
#define MINI_CHART_POINTS 60  // 1 minute of data
#define ROW_HEIGHT 70
#define VALUE_WIDTH 140
#define CHART_WIDTH 330

// External variables
extern uint16_t m_disp_hr;
extern uint8_t m_disp_spo2;
extern uint8_t m_disp_rr;
extern float m_disp_temp_f;
extern float m_disp_temp_c;

// Helper function to create a mini chart
static lv_obj_t* create_mini_chart(lv_obj_t *parent, int y_pos, int y_min, int y_max, 
                                   lv_color_t color, lv_chart_series_t **series)
{
    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_set_size(chart, CHART_WIDTH, ROW_HEIGHT - 10);
    lv_obj_set_pos(chart, VALUE_WIDTH, y_pos + 5);
    
    // Chart styling - minimalist
    lv_obj_set_style_bg_color(chart, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart, lv_color_make(50, 50, 50), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 2, LV_PART_MAIN);
    
    // Chart configuration
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, MINI_CHART_POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    lv_chart_set_div_line_count(chart, 3, 6);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    
    // Grid styling
    lv_obj_set_style_line_color(chart, lv_color_make(20, 20, 20), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    
    // Add series
    *series = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
    
    // Series styling
    lv_obj_set_style_line_color(chart, color, LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    
    // Initialize with no data
    for (int i = 0; i < MINI_CHART_POINTS; i++) {
        lv_chart_set_next_value(chart, *series, LV_CHART_POINT_NONE);
    }
    
    return chart;
}

void draw_scr_all_trends(enum scroll_dir m_scroll_dir)
{
    scr_all_trends = lv_obj_create(NULL);
    draw_header(scr_all_trends, true);

    // Main container - black background
    lv_obj_t *main_container = lv_obj_create(scr_all_trends);
    lv_obj_set_size(main_container, 480, 290);
    lv_obj_set_pos(main_container, 0, 30);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 5, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // === ROW 1: HEART RATE (Orange) ===
    lv_obj_t *hr_row = lv_obj_create(main_container);
    lv_obj_set_size(hr_row, 470, ROW_HEIGHT);
    lv_obj_set_pos(hr_row, 0, 0);
    lv_obj_set_style_bg_color(hr_row, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(hr_row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(hr_row, lv_palette_darken(LV_PALETTE_ORANGE, 3), LV_PART_MAIN);
    lv_obj_set_style_pad_all(hr_row, 5, LV_PART_MAIN);
    lv_obj_clear_flag(hr_row, LV_OBJ_FLAG_SCROLLABLE);

    // HR Icon - using image
    lv_obj_t *img_hr = lv_img_create(hr_row);
    lv_img_set_src(img_hr, &img_heart_35);
    lv_obj_set_pos(img_hr, 5, 5);
    lv_obj_set_style_img_recolor(img_hr, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_hr, LV_OPA_COVER, LV_PART_MAIN);

    // HR Value
    label_hr_value = lv_label_create(hr_row);
    lv_label_set_text(label_hr_value, "-- bpm");
    lv_obj_set_pos(label_hr_value, 45, 22);
    lv_obj_set_style_text_color(label_hr_value, lv_palette_lighten(LV_PALETTE_CYAN, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_hr_value, &style_text_16, LV_STATE_DEFAULT);

    // HR Mini Chart
    chart_hr_mini = create_mini_chart(hr_row, 0, 40, 180, lv_palette_main(LV_PALETTE_ORANGE), &ser_hr_mini);

    // === ROW 2: SPO2 (Blue) ===
    lv_obj_t *spo2_row = lv_obj_create(main_container);
    lv_obj_set_size(spo2_row, 470, ROW_HEIGHT);
    lv_obj_set_pos(spo2_row, 0, ROW_HEIGHT + 5);
    lv_obj_set_style_bg_color(spo2_row, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(spo2_row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(spo2_row, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_MAIN);
    lv_obj_set_style_pad_all(spo2_row, 5, LV_PART_MAIN);
    lv_obj_clear_flag(spo2_row, LV_OBJ_FLAG_SCROLLABLE);

    // SpO2 Icon - using properly sized 35px image  
    lv_obj_t *img_spo2 = lv_img_create(spo2_row);
    lv_img_set_src(img_spo2, &icon_spo2_35);
    lv_obj_set_pos(img_spo2, 5, 17);
    lv_obj_set_style_img_recolor(img_spo2, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_spo2, LV_OPA_COVER, LV_PART_MAIN);

    // SpO2 Value
    label_spo2_value = lv_label_create(spo2_row);
    lv_label_set_text(label_spo2_value, "-- %");
    lv_obj_set_pos(label_spo2_value, 45, 22);
    lv_obj_set_style_text_color(label_spo2_value, lv_palette_lighten(LV_PALETTE_BLUE, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_spo2_value, &style_text_16, LV_STATE_DEFAULT);

    // SpO2 Mini Chart
    chart_spo2_mini = create_mini_chart(spo2_row, 0, 85, 100, lv_palette_main(LV_PALETTE_BLUE), &ser_spo2_mini);

    // === ROW 3: RESPIRATION RATE (Green) ===
    lv_obj_t *rr_row = lv_obj_create(main_container);
    lv_obj_set_size(rr_row, 470, ROW_HEIGHT);
    lv_obj_set_pos(rr_row, 0, (ROW_HEIGHT + 5) * 2);
    lv_obj_set_style_bg_color(rr_row, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(rr_row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(rr_row, lv_palette_darken(LV_PALETTE_GREEN, 3), LV_PART_MAIN);
    lv_obj_set_style_pad_all(rr_row, 5, LV_PART_MAIN);
    lv_obj_clear_flag(rr_row, LV_OBJ_FLAG_SCROLLABLE);

    // RR Icon - using ECG icon for respiration
    lv_obj_t *img_rr = lv_img_create(rr_row);
    lv_img_set_src(img_rr, &ecg_45);
    lv_obj_set_pos(img_rr, 5, 5);
    lv_obj_set_style_img_recolor(img_rr, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_rr, LV_OPA_COVER, LV_PART_MAIN);

    // RR Value
    label_rr_value = lv_label_create(rr_row);
    lv_label_set_text(label_rr_value, "-- br/m");
    lv_obj_set_pos(label_rr_value, 55, 22);
    lv_obj_set_style_text_color(label_rr_value, lv_palette_lighten(LV_PALETTE_GREEN, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_rr_value, &style_text_16, LV_STATE_DEFAULT);

    // RR Mini Chart
    chart_rr_mini = create_mini_chart(rr_row, 0, 5, 35, lv_palette_main(LV_PALETTE_GREEN), &ser_rr_mini);

    // === ROW 4: TEMPERATURE (Red) ===
    lv_obj_t *temp_row = lv_obj_create(main_container);
    lv_obj_set_size(temp_row, 470, ROW_HEIGHT);
    lv_obj_set_pos(temp_row, 0, (ROW_HEIGHT + 5) * 3);
    lv_obj_set_style_bg_color(temp_row, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(temp_row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(temp_row, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN);
    lv_obj_set_style_pad_all(temp_row, 5, LV_PART_MAIN);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature Icon - using properly sized 45px image
    lv_obj_t *img_temp = lv_img_create(temp_row);
    lv_img_set_src(img_temp, &img_temp_45);
    lv_obj_set_pos(img_temp, 5, 12);
    lv_obj_set_style_img_recolor(img_temp, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img_temp, LV_OPA_COVER, LV_PART_MAIN);

    // Temperature Value
    label_temp_value = lv_label_create(temp_row);
    lv_label_set_text(label_temp_value, "--.- °F");
    lv_obj_set_pos(label_temp_value, 50, 22);
    lv_obj_set_style_text_color(label_temp_value, lv_palette_lighten(LV_PALETTE_RED, 2), LV_STATE_DEFAULT);
    lv_obj_add_style(label_temp_value, &style_text_16, LV_STATE_DEFAULT);

    // Temp Mini Chart
    chart_temp_mini = create_mini_chart(temp_row, 0, 95, 105, lv_palette_main(LV_PALETTE_RED), &ser_temp_mini);

    // Set as current screen and show
    hpi_disp_set_curr_screen(SCR_ALL_TRENDS);
    hpi_show_screen(scr_all_trends, m_scroll_dir);
    
    // Initial update
    update_scr_all_trends();
}

void update_scr_all_trends(void)
{
    if (scr_all_trends == NULL) {
        return;
    }

    // Update HR
    uint16_t current_hr = m_disp_hr;
    if (current_hr > 0 && current_hr < 255) {
        lv_label_set_text_fmt(label_hr_value, "%d bpm", current_hr);
        if (chart_hr_mini != NULL && ser_hr_mini != NULL) {
            lv_chart_set_next_value(chart_hr_mini, ser_hr_mini, current_hr);
            lv_chart_refresh(chart_hr_mini);
        }
    } else {
        lv_label_set_text(label_hr_value, "-- bpm");
    }

    // Update SpO2
    uint8_t current_spo2 = m_disp_spo2;
    if (current_spo2 > 0 && current_spo2 <= 100) {
        lv_label_set_text_fmt(label_spo2_value, "%d %%", current_spo2);
        if (chart_spo2_mini != NULL && ser_spo2_mini != NULL) {
            lv_chart_set_next_value(chart_spo2_mini, ser_spo2_mini, current_spo2);
            lv_chart_refresh(chart_spo2_mini);
        }
    } else {
        lv_label_set_text(label_spo2_value, "-- %");
    }

    // Update RR
    uint8_t current_rr = m_disp_rr;
    if (current_rr > 0 && current_rr < 60) {
        lv_label_set_text_fmt(label_rr_value, "%d br/m", current_rr);
        if (chart_rr_mini != NULL && ser_rr_mini != NULL) {
            lv_chart_set_next_value(chart_rr_mini, ser_rr_mini, current_rr);
            lv_chart_refresh(chart_rr_mini);
        }
    } else {
        lv_label_set_text(label_rr_value, "-- br/m");
    }

    // Update Temperature
    float current_temp_f = m_disp_temp_f;
    if (current_temp_f > 0 && current_temp_f < 120) {
        // Manual float formatting (no FP printf support)
        int temp_f_int = (int)current_temp_f;
        int temp_f_dec = (int)((current_temp_f - temp_f_int) * 10);
        lv_label_set_text_fmt(label_temp_value, "%d.%d °F", temp_f_int, temp_f_dec);
        
        if (chart_temp_mini != NULL && ser_temp_mini != NULL) {
            lv_chart_set_next_value(chart_temp_mini, ser_temp_mini, (int16_t)current_temp_f);
            lv_chart_refresh(chart_temp_mini);
        }
    } else {
        lv_label_set_text(label_temp_value, "-- °F");
    }
}

void scr_all_trends_delete(void)
{
    if (scr_all_trends != NULL) {
        lv_obj_del(scr_all_trends);
        scr_all_trends = NULL;
        
        // Reset all component pointers
        label_hr_value = NULL;
        chart_hr_mini = NULL;
        ser_hr_mini = NULL;
        
        label_spo2_value = NULL;
        chart_spo2_mini = NULL;
        ser_spo2_mini = NULL;
        
        label_rr_value = NULL;
        chart_rr_mini = NULL;
        ser_rr_mini = NULL;
        
        label_temp_value = NULL;
        chart_temp_mini = NULL;
        ser_temp_mini = NULL;
    }
}
