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

lv_obj_t *scr_home;

extern lv_style_t style_sub;

extern lv_style_t style_number_big;
extern lv_style_t style_number_medium;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_pr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;

static lv_obj_t *label_temp_f;
static lv_obj_t *label_temp_c;

void draw_scr_home(enum scroll_dir m_scroll_dir)
{
    scr_home = lv_obj_create(NULL);
    draw_header(scr_home, true);
    
    static lv_coord_t col_dsc[] = {220, 220, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {120, 120, LV_GRID_TEMPLATE_LAST};

    /*Create a container with grid*/
    lv_obj_t *cont_home = lv_obj_create(scr_home);
    lv_obj_set_style_grid_column_dsc_array(cont_home, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont_home, row_dsc, 0);
    lv_obj_set_size(cont_home, 480, 310);
    lv_obj_set_pos(cont_home, 0, 25);
    lv_obj_set_layout(cont_home, LV_LAYOUT_GRID);
    lv_obj_set_style_bg_color(cont_home, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *obj_hr_card = lv_obj_create(cont_home);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_style_bg_color(obj_hr_card, lv_palette_darken(LV_PALETTE_ORANGE, 4), LV_PART_MAIN);

    lv_obj_t *obj_spo2_card = lv_obj_create(cont_home);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_style_bg_color(obj_spo2_card, lv_palette_darken(LV_PALETTE_BLUE, 4), LV_PART_MAIN);

    lv_obj_t *obj_rr_card = lv_obj_create(cont_home);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_style_bg_color(obj_rr_card, lv_palette_darken(LV_PALETTE_GREEN, 4), LV_PART_MAIN);

    lv_obj_t *obj_temp_card = lv_obj_create(cont_home);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_style_bg_color(obj_temp_card, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

    lv_obj_set_grid_cell(obj_hr_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(obj_spo2_card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(obj_rr_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(obj_temp_card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

    // HR Number label
    label_hr = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr, "---");
    //lv_obj_align(label_hr, LV_ALIGN_LEFT_MID, 50, -50);
    lv_obj_center(label_hr);
    lv_obj_add_style(label_hr, &style_number_big, LV_STATE_DEFAULT);

    // HR Title label
    lv_obj_t *label_hr_title = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_title, "Heart Rate");
    lv_obj_align_to(label_hr_title, label_hr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_hr_title, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    lv_obj_t *label_hr_sub = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_sub, "bpm");
    lv_obj_align_to(label_hr_sub, label_hr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_hr_sub, &style_sub, LV_STATE_DEFAULT);

    // SPO2 Number label
    label_spo2 = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2, "---");
    //lv_obj_align_to(label_spo2, label_hr, LV_ALIGN_OUT_RIGHT_TOP, 100, 0);
    lv_obj_center(label_spo2);
    lv_obj_add_style(label_spo2, &style_number_big, LV_STATE_DEFAULT);

    // SpO2 Title label
    lv_obj_t *label_spo2_title = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2_title, "SpO2");
    lv_obj_align_to(label_spo2_title, label_spo2, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_spo2_title, &style_sub, LV_STATE_DEFAULT);

    // SpO2 % label
    lv_obj_t *label_spo2_sub = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2_sub, "%");
    lv_obj_align_to(label_spo2_sub, label_spo2, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_spo2_sub, &style_sub, LV_STATE_DEFAULT);

    // RR Number label
    label_rr = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr, "---");
    lv_obj_center(label_rr);
    lv_obj_add_style(label_rr, &style_number_big, LV_STATE_DEFAULT);

    // RR Title label
    lv_obj_t *label_rr_title = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr_title, "Resp Rate");
    lv_obj_align_to(label_rr_title, label_rr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_rr_title, &style_sub, LV_STATE_DEFAULT);

    // RR Sub BPM label
    lv_obj_t *label_rr_sub = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr_sub, "bpm");
    lv_obj_align_to(label_rr_sub, label_rr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_rr_sub, &style_sub, LV_STATE_DEFAULT);

    // Temp Number label
    label_temp_f = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_f, "---");
    lv_obj_center(label_temp_f);
    lv_obj_add_style(label_temp_f, &style_number_big, LV_STATE_DEFAULT);

    label_temp_c = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_c, "---");
    lv_obj_align_to(label_temp_c, label_temp_f, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_add_style(label_temp_c, &style_number_medium, LV_STATE_DEFAULT);

    // Temp label
    lv_obj_t *label_temp_title = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_title, "Temp");
    lv_obj_align_to(label_temp_title, label_temp_f, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_temp_sub = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_sub, "°F");
    lv_obj_align_to(label_temp_sub, label_temp_f, LV_ALIGN_OUT_RIGHT_MID, 45, 0);
    lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub_c = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_sub_c, "°C");
    lv_obj_align_to(label_temp_sub_c, label_temp_c, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_add_style(label_temp_sub_c, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_menu = lv_label_create(scr_home);
    lv_label_set_text(label_menu, "Use the Display mode firmware to view live charts");
    lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -5);

    hpi_disp_set_curr_screen(SCR_HOME);

    hpi_show_screen(scr_home, m_scroll_dir);
}

void hpi_scr_home_update_temp(float temp_f, float temp_c)
{
    if (label_temp_f == NULL)
        return;

    if (temp_c <= 0)
    {
        lv_label_set_text(label_temp_f, "---");
        return;
    }

    char buf[32];
    // double temp_d = (double)(temp_f);
    sprintf(buf, "%.1f", temp_f);
    lv_label_set_text(label_temp_f, buf);
    sprintf(buf, "%.1f", temp_c);
    lv_label_set_text(label_temp_c, buf);
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

void hpi_scr_home_update_pr(int pr)
{
    if (label_pr == NULL)
        return;

    if (pr < 0)
    {
        lv_label_set_text(label_pr, "---");
        return;
    }

    char buf[32];
    sprintf(buf, "%d", pr);
    lv_label_set_text(label_pr, buf);
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