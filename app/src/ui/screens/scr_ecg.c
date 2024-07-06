#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/smf.h>
#include <app_version.h>

#include "sys_sm_module.h"
#include "hw_module.h"

#include "display_module.h"
#include "sampling_module.h"
#include "data_module.h"

static lv_obj_t *scr_ecg;

// GUI Charts
static lv_obj_t *chart1;
static lv_chart_series_t *ser1;

static bool chart1_update = true;

static float y1_max = 0;
static float y1_min = 10000;

static float gx = 0;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;

extern int curr_screen;

void draw_scr_ecg(enum scroll_dir m_scroll_dir)
{

    scr_ecg = lv_obj_create(NULL);
    draw_scr_ecg_footer(scr_ecg);
    draw_header(scr_ecg, true);

    // lv_obj_add_style(scr_ecg, &style_scr_back, 0);

    // lv_group_t *g1 = lv_group_create();

    // Create Chart 1
    chart1 = lv_chart_create(scr_ecg);
    lv_obj_set_size(chart1, 460, 180);
    lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart1, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart1, DISP_WINDOW_SIZE);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_div_line_count(chart1, 0, 0);
    lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_CIRCULAR);

    lv_obj_set_pos(chart1, 10, 25);

    lv_obj_t *label_chart_title = lv_label_create(scr_ecg);

    lv_obj_align_to(label_chart_title, chart1, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_label_set_text(label_chart_title, "Showing ECG");

    /*}
    else if (m_data_type == HPI_SENSOR_DATA_PPG)
    {
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
        curr_screen = HPI_DISP_SCR_PPG;
        lv_label_set_text(label_chart_title, "Showing PPG");
    }
    else if (m_data_type == HPI_SENSOR_DATA_RESP)
    {
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        curr_screen = HPI_DISP_SCR_RESP;
        lv_label_set_text(label_chart_title, "Showing RESP");
    }*/
    ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    curr_screen = SCR_ECG;

    hpi_show_screen(scr_ecg, m_scroll_dir);
}