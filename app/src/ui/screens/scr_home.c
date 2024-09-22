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

static lv_obj_t *scr_home;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;
static lv_obj_t *label_temp;

// GUI Charts
static lv_obj_t *chart1;
static lv_chart_series_t *ser1;

static lv_obj_t *chart2;
static lv_chart_series_t *ser2;

static bool chart1_update = true;
static bool chart2_update = true;

static float y1_max = 0;
static float y1_min = 10000;

static float y2_max = 0;
static float y2_min = 10000;

static float x1 = 0;
static float x2 = 0;

// GUI Styles
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_sub;

extern int curr_screen;

void draw_scr_home_footer(lv_obj_t *parent)
{
    /*static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_obj_add_style(parent, &style, 0);
    */

    // HR Number label
    label_hr = lv_label_create(parent);
    lv_label_set_text(label_hr, "---");
    lv_obj_align(label_hr, LV_ALIGN_LEFT_MID, 20, 100);
    lv_obj_add_style(label_hr, &style_hr, LV_STATE_DEFAULT);

    // HR Title label
    lv_obj_t *label_hr_title = lv_label_create(parent);
    lv_label_set_text(label_hr_title, "HR");
    lv_obj_align_to(label_hr_title, label_hr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_hr_title, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    lv_obj_t *label_hr_sub = lv_label_create(parent);
    lv_label_set_text(label_hr_sub, "bpm");
    lv_obj_align_to(label_hr_sub, label_hr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_hr_sub, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    lv_obj_t *label_hr_status = lv_label_create(parent);
    lv_label_set_text(label_hr_status, "ON");
    lv_obj_align_to(label_hr_status, label_hr_sub, LV_ALIGN_BOTTOM_MID, 0, 17);
    // lv_obj_add_style(label_hr_status, &style_sub, LV_STATE_DEFAULT);

    // SPO2 Number label
    label_spo2 = lv_label_create(parent);
    lv_label_set_text(label_spo2, "---");
    lv_obj_align_to(label_spo2, label_hr, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
    lv_obj_add_style(label_spo2, &style_spo2, LV_STATE_DEFAULT);

    // SpO2 Title label
    lv_obj_t *label_spo2_title = lv_label_create(parent);
    lv_label_set_text(label_spo2_title, "SpO2");
    lv_obj_align_to(label_spo2_title, label_spo2, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_spo2_title, &style_sub, LV_STATE_DEFAULT);

    // SpO2 % label
    lv_obj_t *label_spo2_sub = lv_label_create(parent);
    lv_label_set_text(label_spo2_sub, "%");
    lv_obj_align_to(label_spo2_sub, label_spo2, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_spo2_sub, &style_sub, LV_STATE_DEFAULT);

    // RR Number label
    label_rr = lv_label_create(parent);
    lv_label_set_text(label_rr, "---");
    lv_obj_align_to(label_rr, label_spo2, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
    lv_obj_add_style(label_rr, &style_rr, LV_STATE_DEFAULT);

    // RR Title label
    lv_obj_t *label_rr_title = lv_label_create(parent);
    lv_label_set_text(label_rr_title, "Resp Rate");
    lv_obj_align_to(label_rr_title, label_rr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_rr_title, &style_sub, LV_STATE_DEFAULT);

    // RR Sub BPM label
    lv_obj_t *label_rr_sub = lv_label_create(parent);
    lv_label_set_text(label_rr_sub, "bpm");
    lv_obj_align_to(label_rr_sub, label_rr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_rr_sub, &style_sub, LV_STATE_DEFAULT);

    // Temp Number label
    label_temp = lv_label_create(parent);
    lv_label_set_text(label_temp, "---");
    lv_obj_align_to(label_temp, label_rr, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
    lv_obj_add_style(label_temp, &style_temp, LV_STATE_DEFAULT);

    // Temp label
    lv_obj_t *label_temp_title = lv_label_create(parent);
    lv_label_set_text(label_temp_title, "Temperature");
    lv_obj_align_to(label_temp_title, label_temp, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub = lv_label_create(parent);
    lv_label_set_text(label_temp_sub, "°C");
    lv_obj_align_to(label_temp_sub, label_temp, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_menu = lv_label_create(parent);
    lv_label_set_text(label_menu, "Press side wheel DOWN for more charts");
    lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void draw_scr_home(enum scroll_dir m_scroll_dir)
{

    scr_home = lv_obj_create(NULL);
    draw_scr_home_footer(scr_home);
    draw_header(scr_home, true);

    // lv_obj_add_style(scr_home, &style_welcome_scr_bg, 0);

    // lv_group_t *g1 = lv_group_create();

    // Create Chart 1
    chart1 = lv_chart_create(scr_home);
    lv_obj_set_pos(chart1, 10, 25);

    lv_obj_set_size(chart1, 400, 100);
    lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart1, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart1, DISP_WINDOW_SIZE);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_div_line_count(chart1, 0, 0);
    lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_CIRCULAR);

    ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    chart2 = lv_chart_create(scr_home);
    lv_obj_set_pos(chart2, 10, 150);
    lv_obj_set_size(chart2, 460, 100);
    lv_obj_set_style_bg_color(chart2, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart2, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart2, DISP_WINDOW_SIZE);
    lv_chart_set_range(chart2, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_div_line_count(chart2, 0, 0);
    lv_chart_set_update_mode(chart2, LV_CHART_UPDATE_MODE_CIRCULAR);

    ser2 = lv_chart_add_series(chart2, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    //lv_obj_t *label_chart_title = lv_label_create(scr_home);

    //lv_obj_align_to(label_chart_title, chart1, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    //lv_label_set_text(label_chart_title, "Showing ECG");

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
   
    curr_screen = SCR_HOME;

    hpi_show_screen(scr_home, m_scroll_dir);
}

void hpi_chart1_set_scale()
{
    if (x1 >= DISP_WINDOW_SIZE)
    {
        if (chart1_update == true)
            lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, y1_min, y1_max);
        
        x1 = 0;

        y1_max = -100000;
        y1_min = 100000;
    }
}

void hpi_chart2_set_scale()
{
    if (x2 >= DISP_WINDOW_SIZE)
    {
        if (chart2_update == true)
            lv_chart_set_range(chart2, LV_CHART_AXIS_PRIMARY_Y, y2_min, y2_max);

        x2 = 0;

        y2_max = -100000;
        y2_min = 100000;
    }
}

void scr_home_plot_ecg(float plot_data)
{
    if (chart1_update == true)
    {

        if (plot_data < y1_min)
        {
            y1_min = plot_data;
        }

        if (plot_data > y1_max)
        {
            y1_max = plot_data;
        }

        // printk("E");
        lv_chart_set_next_value(chart1, ser1, plot_data);
        x1+=1;
        
        hpi_chart1_set_scale();
    }
}

void scr_home_plot_ppg(float plot_data)
{
    if (chart2_update == true)
    {

        if (plot_data < y2_min)
        {
            y2_min = plot_data;
        }

        if (plot_data > y2_max)
        {
            y2_max = plot_data;
        }

        // printk("E");
        lv_chart_set_next_value(chart2, ser2, plot_data);
        x2+=1;
        
        hpi_chart2_set_scale();
    }
}

void hpi_scr_home_update_temp(int temp)
{
    if (label_temp == NULL)
        return;

    if (temp <= 0)
    {
        lv_label_set_text(label_temp, "---");
        return;
    }

    char buf[32];
    double temp_d = (double)(temp / 1000.00);
    sprintf(buf, "%.2f", temp_d);
    lv_label_set_text(label_temp, buf);
}

void hpi_scr_home_update_hr(int hr)
{
    if (label_hr == NULL)
        return;

    char buf[32];
    sprintf(buf, "%d", hr);
    lv_label_set_text(label_hr, buf);
}

void hpi_scr_home_update_spo2(int spo2)
{
    if (label_spo2 == NULL)
        return;

    if (spo2 < 0)
    {
        lv_label_set_text(label_spo2, "---");
        return;
    }

    char buf[32];
    sprintf(buf, "%d", spo2);
    lv_label_set_text(label_spo2, buf);
}

void hpi_scr_home_update_rr(int rr)
{
    if (label_rr == NULL)
        return;

    if (rr < 0)
    {
        lv_label_set_text(label_rr, "---");
        return;
    }

    char buf[32];
    sprintf(buf, "%d", rr);
    lv_label_set_text(label_rr, buf);
}
