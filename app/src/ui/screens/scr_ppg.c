#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <lvgl.h>
#include <stdio.h>
#include <app_version.h>

#include "hw_module.h"
#include "display_module.h"
#include "sampling_module.h"
#include "data_module.h"

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

extern uint8_t curr_screen;

void draw_scr_ppg(enum scroll_dir m_scroll_dir)
{
    scr_ppg = lv_obj_create(NULL);
    draw_scr_home_footer(scr_ppg);
    draw_header(scr_ppg, true);

    chart_ppg = lv_chart_create(scr_ppg);
    lv_obj_set_pos(chart_ppg, 5, 30);

    lv_obj_set_size(chart_ppg, 475, 200);
    lv_obj_set_style_bg_color(chart_ppg, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart_ppg, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart_ppg, PPG_DISP_WINDOW_SIZE);
    lv_chart_set_range(chart_ppg, LV_CHART_AXIS_PRIMARY_Y, -200, 250);
    lv_chart_set_div_line_count(chart_ppg, 0, 0);
    lv_chart_set_update_mode(chart_ppg, LV_CHART_UPDATE_MODE_CIRCULAR);

    ser_ppg = lv_chart_add_series(chart_ppg, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_t *lbl_sig_type = lv_label_create(scr_ppg);
    lv_label_set_text(lbl_sig_type, "Showing PPG");
    // lv_obj_add_style(lbl_sig_type, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align(lbl_sig_type, LV_ALIGN_TOP_MID, 0, 25);

    curr_screen = SCR_PPG;

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

void hpi_ppg_disp_draw_plot_ppg(int32_t *data_ppg_red, int32_t *data_ppg_ir, int num_samples, bool ppg_lead_off)
{
    if (chart_ppg_update == true) // && ppg_lead_off == false)
    {
        for (int i = 0; i < num_samples; i++)
        {
            int32_t data_ppg_i = ((data_ppg_red[i]));

            // printk("PPG: %d\n", data_ppg_i);

            if (data_ppg_i < y_min_ppg)
            {
                y_min_ppg = data_ppg_i;
            }

            if (data_ppg_i > y_max_ppg)
            {
                y_max_ppg = data_ppg_i;
            }

            lv_chart_set_next_value(chart_ppg, ser_ppg, data_ppg_i);
            hpi_ppg_disp_add_samples(1);
        }
        // hpi_ppg_disp_do_set_scale(PPG_DISP_WINDOW_SIZE);
    }

    if (ppg_lead_off == true)
    {
        // printk("PPG Lead Off\n");
    }
}
