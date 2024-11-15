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

lv_obj_t *scr_home;

extern uint8_t curr_screen;

void draw_scr_home(enum scroll_dir m_scroll_dir)
{
    scr_home = lv_obj_create(NULL);
    draw_header(scr_home, true);

    lv_obj_t *lbl_screen_title = lv_label_create(scr_home);
    lv_label_set_text(lbl_screen_title, "Home Screen");
    lv_obj_align(lbl_screen_title, LV_ALIGN_CENTER, 0, 0);

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

    curr_screen = SCR_HOME;

    hpi_show_screen(scr_home, m_scroll_dir);
}