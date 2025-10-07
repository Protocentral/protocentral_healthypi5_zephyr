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

lv_obj_t *scr_ppg;

// GUI Charts
static lv_obj_t *chart_ppg;
static lv_chart_series_t *ser_ppg;

static bool chart_ppg_update = true;

// Chart Variables
static float y_max_ppg = 0;
static float y_min_ppg = 10000;
static float gx = 0;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;

void draw_scr_ppg(enum scroll_dir m_scroll_dir)
{
    scr_ppg = lv_obj_create(NULL);
    draw_header(scr_ppg, true);

    // Create styled container for chart with yellow theme (high visibility for PPG)
    lv_obj_t *chart_container = lv_obj_create(scr_ppg);
    lv_obj_set_size(chart_container, 470, 285);
    lv_obj_set_pos(chart_container, 5, 30);
    lv_obj_set_style_bg_color(chart_container, lv_palette_darken(LV_PALETTE_YELLOW, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_container, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
    lv_obj_set_style_radius(chart_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_container, 8, LV_PART_MAIN);
    lv_obj_clear_flag(chart_container, LV_OBJ_FLAG_SCROLLABLE);

    // Title label with icon
    lv_obj_t *icon_label = lv_label_create(chart_container);
    lv_label_set_text(icon_label, "O2 PPG");
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Chart widget
    chart_ppg = lv_chart_create(chart_container);
    lv_obj_set_size(chart_ppg, 450, 240);
    lv_obj_align(chart_ppg, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(chart_ppg, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(chart_ppg, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_ppg, lv_palette_darken(LV_PALETTE_YELLOW, 2), LV_PART_MAIN);
    lv_obj_set_style_radius(chart_ppg, 5, LV_PART_MAIN);

    lv_obj_set_style_size(chart_ppg, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart_ppg, PPG_DISP_WINDOW_SIZE);
    lv_chart_set_range(chart_ppg, LV_CHART_AXIS_PRIMARY_Y, -200, 250);
    lv_chart_set_div_line_count(chart_ppg, 0, 0);
    lv_chart_set_update_mode(chart_ppg, LV_CHART_UPDATE_MODE_SHIFT);

    ser_ppg = lv_chart_add_series(chart_ppg, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(chart_ppg, 2, LV_PART_ITEMS);

    hpi_disp_set_curr_screen(SCR_PPG);
    hpi_show_screen(scr_ppg, m_scroll_dir);
}

static void hpi_ppg_disp_add_samples(int num_samples)
{
    gx += num_samples;
}

static void hpi_ppg_disp_do_set_scale(int disp_window_size)
{
    if (gx >= (disp_window_size))
    {
        if (chart_ppg_update == true)
        {
            lv_chart_set_range(chart_ppg, LV_CHART_AXIS_PRIMARY_Y, y_min_ppg, y_max_ppg);
        }

        gx = 0;

        y_max_ppg = -900000;
        y_min_ppg = 900000;
    }
}

void hpi_ppg_disp_draw_plot_ppg(int32_t data_ppg_red, int32_t data_ppg_ir, bool ppg_lead_off)
{
    if (chart_ppg_update == true) // && ppg_lead_off == false)
    {
        if (data_ppg_red < y_min_ppg)
        {
            y_min_ppg = (int16_t)data_ppg_red;
        }

        if (data_ppg_red > y_max_ppg)
        {
            y_max_ppg = (int16_t)data_ppg_red;
        }

        // Add sample - LVGL updates automatically
        lv_chart_set_next_value(chart_ppg, ser_ppg, (int16_t)data_ppg_red);
        hpi_ppg_disp_add_samples(1);

        //hpi_ppg_disp_do_set_scale(PPG_DISP_WINDOW_SIZE);
    }

    if (ppg_lead_off == true)
    {
        // printk("PPG Lead Off\n");
    }
}

// Stub update function for SpO2 detail screen
// TODO: Implement detailed SpO2 screen similar to ECG screen
void hpi_scr_ppg_update(void)
{
    // Placeholder - will be implemented with detailed SpO2 view
}
