#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <lvgl.h>
#include <stdio.h>
#include <app_version.h>

#include "sys_sm_module.h"
#include "hw_module.h"

#include "display_module.h"
#include "sampling_module.h"
#include "data_module.h"

static lv_obj_t *scr_ecg;

// GUI Charts
static lv_obj_t *chart_ecg;
static lv_chart_series_t *ser_ecg;

static bool chart_ecg_update = true;

// Chart Variables
static float y_max_ecg = 0;
static float y_min_ecg = 10000;
static float gx = 0;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;

extern int curr_screen;

#define DISP_WINDOW_SIZE_ECG 390

void draw_scr_ecg(enum scroll_dir m_scroll_dir)
{
    scr_ecg = lv_obj_create(NULL);
    draw_scr_home_footer(scr_ecg);
    draw_header(scr_ecg, true);

    chart_ecg = lv_chart_create(scr_ecg);
    lv_obj_set_pos(chart_ecg, 10, 25);

    lv_obj_set_size(chart_ecg, 450, 190);
    lv_obj_set_style_bg_color(chart_ecg, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart_ecg, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart_ecg, DISP_WINDOW_SIZE);
    lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_div_line_count(chart_ecg, 0, 0);
    lv_chart_set_update_mode(chart_ecg, LV_CHART_UPDATE_MODE_CIRCULAR);

    ser_ecg = lv_chart_add_series(chart_ecg, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    curr_screen = SCR_ECG;

    hpi_show_screen(scr_ecg, m_scroll_dir);
}

static void hpi_ecg_disp_add_samples(int num_samples)
{
    gx += num_samples;
}

static void hpi_ecg_disp_do_set_scale(int disp_window_size)
{
    if (gx >= (disp_window_size))
    {
        if (chart_ecg_update == true)
        {
            lv_chart_set_range(chart_ecg, LV_CHART_AXIS_PRIMARY_Y, y_min_ecg, y_max_ecg);
        }

        gx = 0;

        y_max_ecg = -900000;
        y_min_ecg = 900000;
    }
}

void hpi_ecg_disp_draw_plotECG(int32_t *data_ecg, int num_samples, bool ecg_lead_off)
{
    if (chart_ecg_update == true) // && ecg_lead_off == false)
    {
        for (int i = 0; i < num_samples; i++)
        {
            int32_t data_ecg_i = (data_ecg[i] / 1000);

            if (data_ecg_i < y_min_ecg)
            {
                y_min_ecg = data_ecg_i;
            }

            if (data_ecg_i > y_max_ecg)
            {
                y_max_ecg = data_ecg_i;
            }

            /*if (ecg_plot_hidden == true)
            {
                lv_obj_add_flag(label_ecg_lead_off, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(chart_ecg, LV_OBJ_FLAG_HIDDEN);
                ecg_plot_hidden = false;
            }*/

            // printk("E");

            lv_chart_set_next_value(chart_ecg, ser_ecg, data_ecg_i);
            hpi_ecg_disp_add_samples(1);
            hpi_ecg_disp_do_set_scale(DISP_WINDOW_SIZE_ECG);
        }
        // lv_chart_set_next_value(chart_ecg, ser_ecg, data_ecg);
        // hpi_ecg_disp_add_samples(1);
        // hpi_ecg_disp_do_set_scale(DISP_WINDOW_SIZE_ECG);
    }

    if (ecg_lead_off == true)
    {
        // printk("ECG Lead Off\n");
    }
}