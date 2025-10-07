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

lv_obj_t *scr_ecg;

// GUI Labels for detailed view
static lv_obj_t *label_hr_big;
static lv_obj_t *label_hr_unit;
static lv_obj_t *label_hr_range;
static lv_obj_t *label_hr_trend;
static lv_obj_t *label_hr_updated;
static lv_obj_t *label_uptime;
static lv_obj_t *label_streaming_hint;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;
extern lv_style_t style_number_big;

// External variables from display_module.c
extern uint16_t m_disp_hr;

void draw_scr_ecg(enum scroll_dir m_scroll_dir)
{
    scr_ecg = lv_obj_create(NULL);
    draw_header(scr_ecg, true);

    // Create styled container for detail view - black background with orange accents for high contrast
    lv_obj_t *detail_container = lv_obj_create(scr_ecg);
    lv_obj_set_size(detail_container, 470, 285);
    lv_obj_set_pos(detail_container, 5, 30);
    lv_obj_set_style_bg_color(detail_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(detail_container, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(detail_container, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_radius(detail_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(detail_container, 15, LV_PART_MAIN);
    lv_obj_clear_flag(detail_container, LV_OBJ_FLAG_SCROLLABLE);

    // Title label - bright orange for visibility
    lv_obj_t *icon_label = lv_label_create(detail_container);
    lv_label_set_text(icon_label, "HEART RATE");
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(icon_label, lv_palette_lighten(LV_PALETTE_ORANGE, 1), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_20, LV_STATE_DEFAULT);

    // Large HR value - bright orange
    label_hr_big = lv_label_create(detail_container);
    lv_label_set_text(label_hr_big, "--");
    lv_obj_align(label_hr_big, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_text_color(label_hr_big, lv_palette_lighten(LV_PALETTE_ORANGE, 2), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_hr_big, &lv_font_montserrat_42, LV_STATE_DEFAULT);

    // Unit label - white for contrast
    label_hr_unit = lv_label_create(detail_container);
    lv_label_set_text(label_hr_unit, "bpm");
    lv_obj_align_to(label_hr_unit, label_hr_big, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_text_color(label_hr_unit, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_hr_unit, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Min/Max range label - bright white
    label_hr_range = lv_label_create(detail_container);
    lv_label_set_text(label_hr_range, "Min: --  Max: --  (1 min)");
    lv_obj_align(label_hr_range, LV_ALIGN_TOP_LEFT, 5, 140);
    lv_obj_set_style_text_color(label_hr_range, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_hr_range, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Trend indicator - dynamic color set in update function
    label_hr_trend = lv_label_create(detail_container);
    lv_label_set_text(label_hr_trend, "Trend: --");
    lv_obj_align(label_hr_trend, LV_ALIGN_TOP_LEFT, 5, 170);
    lv_obj_set_style_text_color(label_hr_trend, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_hr_trend, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Last update time - light gray for secondary info
    label_hr_updated = lv_label_create(detail_container);
    lv_label_set_text(label_hr_updated, "Updated: --");
    lv_obj_align(label_hr_updated, LV_ALIGN_TOP_LEFT, 5, 200);
    lv_obj_set_style_text_color(label_hr_updated, lv_color_make(160, 160, 160), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_hr_updated, &lv_font_montserrat_14, LV_STATE_DEFAULT);

    // Uptime display - light gray
    label_uptime = lv_label_create(detail_container);
    lv_label_set_text(label_uptime, "Uptime: 00:00:00");
    lv_obj_align(label_uptime, LV_ALIGN_BOTTOM_LEFT, 5, -25);
    lv_obj_set_style_text_color(label_uptime, lv_color_make(160, 160, 160), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_uptime, &lv_font_montserrat_14, LV_STATE_DEFAULT);

    // Streaming hint - medium gray
    label_streaming_hint = lv_label_create(detail_container);
    lv_label_set_text(label_streaming_hint, "Use USB/BLE for waveforms");
    lv_obj_align(label_streaming_hint, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_color(label_streaming_hint, lv_color_make(140, 140, 140), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label_streaming_hint, &lv_font_montserrat_12, LV_STATE_DEFAULT);

    hpi_disp_set_curr_screen(SCR_ECG);
    hpi_show_screen(scr_ecg, m_scroll_dir);
    
    // Initial update
    hpi_scr_ecg_update();
}

void hpi_scr_ecg_update(void)
{
    if (label_hr_big == NULL) return;  // Screen not initialized
    
    // Get current values from vital_stats module
    uint16_t hr_min = vital_stats_get_hr_min();
    uint16_t hr_max = vital_stats_get_hr_max();
    uint32_t time_since = vital_stats_get_hr_time_since_update();
    int8_t trend = vital_stats_get_hr_trend();
    
    // Update main HR value (from module variable - need to access display_module.c variable)
    if (m_disp_hr > 0) {
        lv_label_set_text_fmt(label_hr_big, "%d", m_disp_hr);
    } else {
        lv_label_set_text(label_hr_big, "--");
    }
    
    // Update min/max range
    if (hr_min > 0 && hr_max > 0) {
        lv_label_set_text_fmt(label_hr_range, "Min: %d  Max: %d  (1 min)", hr_min, hr_max);
    } else {
        lv_label_set_text(label_hr_range, "Min: --  Max: --  (1 min)");
    }
    
    // Update trend indicator with bright colors for visibility on black background
    if (trend > 0) {
        lv_label_set_text(label_hr_trend, LV_SYMBOL_UP " Increasing");
        lv_obj_set_style_text_color(label_hr_trend, lv_palette_lighten(LV_PALETTE_RED, 1), LV_STATE_DEFAULT);
    } else if (trend < 0) {
        lv_label_set_text(label_hr_trend, LV_SYMBOL_DOWN " Decreasing");
        lv_obj_set_style_text_color(label_hr_trend, lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 2), LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(label_hr_trend, LV_SYMBOL_RIGHT " Stable");
        lv_obj_set_style_text_color(label_hr_trend, lv_palette_lighten(LV_PALETTE_GREEN, 2), LV_STATE_DEFAULT);
    }
    
    // Update last update time
    if (time_since < 60) {
        lv_label_set_text_fmt(label_hr_updated, "Updated: %us ago", time_since);
    } else if (time_since < 0xFFFFFFFF) {
        lv_label_set_text_fmt(label_hr_updated, "Updated: %um ago", time_since / 60);
    } else {
        lv_label_set_text(label_hr_updated, "Updated: --");
    }
    
    // Update uptime
    uint32_t hours, minutes, seconds;
    vital_stats_get_uptime(&hours, &minutes, &seconds);
    lv_label_set_text_fmt(label_uptime, "Uptime: %02u:%02u:%02u", hours, minutes, seconds);
}
