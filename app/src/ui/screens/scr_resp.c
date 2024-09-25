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

static lv_obj_t *scr_resp;

// GUI Charts
static lv_obj_t *chart_resp;
static lv_chart_series_t *ser_resp;

static bool chart_resp_update = true;

// Chart Variables
static float y_max_resp = 0;
static float y_min_resp = 10000;
static float gx = 0;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;

extern int curr_screen;

void draw_scr_resp(enum scroll_dir m_scroll_dir)
{
    scr_resp = lv_obj_create(NULL);
    draw_scr_home_footer(scr_resp);
    draw_header(scr_resp, true);

    chart_resp = lv_chart_create(scr_resp);
    lv_obj_set_pos(chart_resp, 10, 25);

    lv_obj_set_size(chart_resp, 450, 190);
    lv_obj_set_style_bg_color(chart_resp, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart_resp, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart_resp, RESP_DISP_WINDOW_SIZE);
    lv_chart_set_range(chart_resp, LV_CHART_AXIS_PRIMARY_Y, -1000, 250);
    lv_chart_set_div_line_count(chart_resp, 8, 0);
    lv_chart_set_update_mode(chart_resp, LV_CHART_UPDATE_MODE_CIRCULAR);

    ser_resp = lv_chart_add_series(chart_resp, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    curr_screen = SCR_RESP;

    hpi_show_screen(scr_resp, m_scroll_dir);
}

static void hpi_resp_disp_add_samples(int num_samples)
{
    gx += num_samples;
}

static void hpi_resp_disp_do_set_scale(int disp_window_size)
{
    if (gx >= (disp_window_size))
    {
        if (chart_resp_update == true)
        {
            lv_chart_set_range(chart_resp, LV_CHART_AXIS_PRIMARY_Y, y_min_resp, y_max_resp);
        }

        gx = 0;

        y_max_resp = -900000;
        y_min_resp = 900000;
    }
}

void hpi_resp_disp_draw_plot_resp(int32_t *data_resp, int num_samples, bool resp_lead_off)
{
    if (chart_resp_update == true) // && resp_lead_off == false)
    {
        for (int i = 0; i < num_samples; i++)
        {
            int32_t data_resp_i = ((data_resp[i]));///5242880)*10;// in mV// (data_resp[i]);

            if (data_resp_i < y_min_resp)
            {
                y_min_resp = data_resp_i;
            }

            if (data_resp_i > y_max_resp)
            {
                y_max_resp = data_resp_i;
            }

            lv_chart_set_next_value(chart_resp, ser_resp, data_resp_i);
            hpi_resp_disp_add_samples(1);
            hpi_resp_disp_do_set_scale(RESP_DISP_WINDOW_SIZE);
        }
    }

    if (resp_lead_off == true)
    {
        // printk("resp Lead Off\n");
    }
}
