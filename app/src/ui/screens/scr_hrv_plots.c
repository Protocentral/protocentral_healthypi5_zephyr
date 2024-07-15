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

static lv_obj_t *scr_hrv_plots;

// GUI Charts
static lv_obj_t *chart1;
static lv_chart_series_t *ser1;

static lv_obj_t *chart2;
static lv_chart_series_t *ser2;

static bool chart1_update = true;
static bool chart2_update = true;

extern int curr_screen;

void draw_scr_hrv_plots(enum scroll_dir m_scroll_dir)
{
    scr_hrv_plots = lv_obj_create(NULL);
    draw_header(scr_hrv_plots, true);

    lv_obj_t * chart1 = lv_chart_create(scr_hrv_plots);
    lv_obj_set_pos(chart1, 30, 25);
    lv_obj_set_size(chart1, 410, 110);
    lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);
    lv_chart_set_type(chart1, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_Y, 2, 0, 3, 5, true, 40);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_X, 600, 1100);
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_X, 2, 0, 10, 5, true, 20);
    //lv_obj_set_style_pad_column(chart1, 2, 0);
    lv_chart_set_point_count(chart1, 10);
    lv_chart_set_div_line_count(chart1, 0, 0);

    //add x axis ticks (650 to 1100)

    lv_obj_t *label_chart1_title = lv_label_create(scr_hrv_plots);
    lv_obj_align_to(label_chart1_title, chart1, LV_ALIGN_OUT_TOP_LEFT, 120, 0);
    lv_label_set_text(label_chart1_title, "HRV Histogram Plot");

    lv_obj_t * chart2 = lv_chart_create(scr_hrv_plots);
    lv_obj_set_size (chart2, 410, 110);
    lv_obj_align_to(chart2, chart1,LV_ALIGN_OUT_BOTTOM_LEFT,0,40);
    lv_obj_set_style_bg_color(chart2, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_size(chart2, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart2, 1100);
    lv_chart_set_type(chart2, LV_CHART_TYPE_LINE);    
    lv_chart_set_div_line_count(chart2, 0, 0);

    lv_chart_set_range(chart2, LV_CHART_AXIS_PRIMARY_Y, 0, 1100);
    lv_chart_set_axis_tick(chart2, LV_CHART_AXIS_PRIMARY_Y, 2, 0, 3, 5, true, 40);
    lv_chart_set_axis_tick(chart2, LV_CHART_AXIS_PRIMARY_X, 2, 0, 10, 5, true, 20);
    lv_chart_set_range(chart2, LV_CHART_AXIS_PRIMARY_X, 0, 1100);

    lv_obj_t *label_chart2_title = lv_label_create(scr_hrv_plots);
    lv_obj_align_to(label_chart2_title, chart2, LV_ALIGN_OUT_TOP_LEFT, 120, 0);
    lv_label_set_text(label_chart2_title, "HRV Tachogram Plot");

    ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser2 = lv_chart_add_series(chart2, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    uint32_t i;
    for(i = 0; i < 10; i++) {
        lv_chart_set_next_value(chart1, ser1, lv_rand(0, 90));
    } 

    //lv_chart_set_next_value(chart2, ser2, lv_rand(0, 1100));

    curr_screen =  SCR_HRV_PLOTS;
    hpi_show_screen(scr_hrv_plots, m_scroll_dir);

}

void scr_hrv_plots_tach()
{
    /*if (chart2_update == true)
    {
        lv_chart_set_next_value(chart2, ser2, lv_rand(0, 1100));
    }*/


}

void scr_hrv_plots_hist()
{
    //need to add updating logic
    /*if (chart1_update == true)
    {
        uint32_t i;
        for(i = 0; i < 10; i++) {
            lv_chart_set_next_value(chart1, ser1, lv_rand(0, 90));
        }   
    }*/
}


