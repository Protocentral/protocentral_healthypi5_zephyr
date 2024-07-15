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

static lv_obj_t *scr_poincare;

// GUI Charts
static lv_obj_t *chart1;
static lv_chart_series_t *ser1;

static bool chart1_update = true;

extern int curr_screen;

void draw_scr_poincare(enum scroll_dir m_scroll_dir)
{
    scr_poincare = lv_obj_create(NULL);
    draw_header(scr_poincare, true);

    chart1 = lv_chart_create(scr_poincare);
    lv_obj_set_pos(chart1, 50, 35);
    lv_obj_set_size(chart1, 400, 230);
    lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(chart1, 0, LV_PART_ITEMS);   /*Remove the lines*/
    lv_chart_set_type(chart1, LV_CHART_TYPE_SCATTER);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_X, 600, 1100);
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, 800, 1200);
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_X, 5, 0, 6, 5, true, 40);
    lv_chart_set_axis_tick(chart1, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 5, 5, true, 40);
    lv_chart_set_point_count(chart1, 50);
    lv_chart_set_div_line_count(chart1, 0, 0);
    lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_CIRCULAR);

    lv_obj_t *label_chart_title = lv_label_create(scr_poincare);
    lv_obj_align_to(label_chart_title, chart1, LV_ALIGN_OUT_TOP_LEFT, 120, 0);
    lv_label_set_text(label_chart_title, "HRV Poincare Plot");

    lv_obj_t *label_x_axis_label = lv_label_create(scr_poincare);
    lv_obj_align_to(label_x_axis_label, chart1, LV_ALIGN_OUT_BOTTOM_LEFT, 120, 25);
    lv_label_set_text(label_x_axis_label, "RR Interval (ms)");
     
    /*Need to set rotation to y label
    lv_obj_t *label_y_axis_label = lv_label_create(scr_poincare);
    lv_obj_align_to(label_y_axis_label, chart1, LV_ALIGN_OUT_LEFT_MID, 0, 0);
    //lv_obj_set_style_transform_rotation(label_y_axis_label, 150, 0); 
    lv_obj_set_style_transform_angle(label_y_axis_label, 10, 0);
    lv_label_set_text(label_y_axis_label, "Next RR Interval (ms)");*/

    ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    curr_screen = SCR_POINCARE;

    hpi_show_screen(scr_poincare, m_scroll_dir);
}

void scr_poincare_plot_rr(float rr1, float rr2)
{
    if (chart1_update == true)
    {
        lv_chart_set_next_value2(chart1, ser1, lv_rand(600, 1200), lv_rand(600, 1200));        
    }
}
