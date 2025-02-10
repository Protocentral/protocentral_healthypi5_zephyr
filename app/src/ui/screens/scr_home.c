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

extern uint8_t curr_screen;

extern lv_style_t style_sub;
extern lv_style_t style_hr;
extern lv_style_t style_spo2;
extern lv_style_t style_rr;
extern lv_style_t style_temp;
extern lv_style_t style_pr;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_pr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;
static lv_obj_t *label_temp;

void draw_scr_home(enum scroll_dir m_scroll_dir)
{
    scr_home = lv_obj_create(NULL);
    draw_header(scr_home, true);

    // HR Number label
    label_hr = lv_label_create(scr_home);
    lv_label_set_text(label_hr, "---");
    lv_obj_align(label_hr, LV_ALIGN_LEFT_MID, 50, -50);
    lv_obj_add_style(label_hr, &style_hr, LV_STATE_DEFAULT);

    // HR Title label
    lv_obj_t *label_hr_title = lv_label_create(scr_home);
    lv_label_set_text(label_hr_title, "Heart Rate");
    lv_obj_align_to(label_hr_title, label_hr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_hr_title, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    lv_obj_t *label_hr_sub = lv_label_create(scr_home);
    lv_label_set_text(label_hr_sub, "bpm");
    lv_obj_align_to(label_hr_sub, label_hr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_hr_sub, &style_sub, LV_STATE_DEFAULT);

    // SPO2 Number label
    label_spo2 = lv_label_create(scr_home);
    lv_label_set_text(label_spo2, "---");
    lv_obj_align_to(label_spo2, label_hr, LV_ALIGN_OUT_RIGHT_TOP, 100, 0);
    lv_obj_add_style(label_spo2, &style_spo2, LV_STATE_DEFAULT);

    // SpO2 Title label
    lv_obj_t *label_spo2_title = lv_label_create(scr_home);
    lv_label_set_text(label_spo2_title, "SpO2");
    lv_obj_align_to(label_spo2_title, label_spo2, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_spo2_title, &style_sub, LV_STATE_DEFAULT);

    // SpO2 % label
    lv_obj_t *label_spo2_sub = lv_label_create(scr_home);
    lv_label_set_text(label_spo2_sub, "%");
    lv_obj_align_to(label_spo2_sub, label_spo2, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_spo2_sub, &style_sub, LV_STATE_DEFAULT);

    // RR Number label
    label_rr = lv_label_create(scr_home);
    lv_label_set_text(label_rr, "---");
    lv_obj_align_to(label_rr, label_spo2, LV_ALIGN_OUT_RIGHT_TOP, 100, 0);
    lv_obj_add_style(label_rr, &style_rr, LV_STATE_DEFAULT);

    // RR Title label
    lv_obj_t *label_rr_title = lv_label_create(scr_home);
    lv_label_set_text(label_rr_title, "Resp Rate");
    lv_obj_align_to(label_rr_title, label_rr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_rr_title, &style_sub, LV_STATE_DEFAULT);

    // RR Sub BPM label
    lv_obj_t *label_rr_sub = lv_label_create(scr_home);
    lv_label_set_text(label_rr_sub, "bpm");
    lv_obj_align_to(label_rr_sub, label_rr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_rr_sub, &style_sub, LV_STATE_DEFAULT);

    /*
    // Pulse Rate Number label
    label_pr = lv_label_create(scr_home);
    lv_label_set_text(label_pr, "---");
    lv_obj_align_to(label_pr, label_hr, LV_ALIGN_OUT_BOTTOM_MID, 60, 100);
    lv_obj_add_style(label_pr, &style_pr, LV_STATE_DEFAULT);

    // Pulse Rate Title label
    lv_obj_t *label_pr_title = lv_label_create(scr_home);
    lv_label_set_text(label_pr_title, "Pulse");
    lv_obj_align_to(label_pr_title, label_pr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_pr_title, &style_sub, LV_STATE_DEFAULT);

    // Pulse Rate Sub BPM label
    lv_obj_t *label_pr_sub = lv_label_create(scr_home);
    lv_label_set_text(label_pr_sub, "bpm");
    lv_obj_align_to(label_pr_sub, label_pr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_pr_sub, &style_sub, LV_STATE_DEFAULT);
    */

    // Temp Number label
    label_temp = lv_label_create(scr_home);
    lv_label_set_text(label_temp, "---");
    lv_obj_align_to(label_temp, label_spo2, LV_ALIGN_OUT_BOTTOM_MID, 0, 100);
    lv_obj_add_style(label_temp, &style_temp, LV_STATE_DEFAULT);

    // Temp label
    lv_obj_t *label_temp_title = lv_label_create(scr_home);
    lv_label_set_text(label_temp_title, "Temp");
    lv_obj_align_to(label_temp_title, label_temp, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub = lv_label_create(scr_home);
    lv_label_set_text(label_temp_sub, "°C");
    lv_obj_align_to(label_temp_sub, label_temp, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_menu = lv_label_create(scr_home);
    lv_label_set_text(label_menu, "Use the Display mode firmware to view live charts");
    lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -5);

    curr_screen = SCR_HOME;

    hpi_show_screen(scr_home, m_scroll_dir);
}

void hpi_scr_home_update_temp(int32_t temp)
{
    if (label_temp == NULL)
        return;

    if (temp <= 0)
    {
        lv_label_set_text(label_temp, "---");
        return;
    }

    char buf[32];
    double temp_d = (double)(temp / 100.00);
    sprintf(buf, "%.1f", temp_d);
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