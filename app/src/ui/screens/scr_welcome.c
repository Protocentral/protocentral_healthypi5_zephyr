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

static lv_obj_t *scr_welcome;

extern lv_style_t style_welcome_scr_bg;
extern lv_style_t style_h1;
extern lv_style_t style_h2;
extern lv_style_t style_info;
extern lv_style_t style_icon;

extern int curr_screen;

static void anim_x_cb(void *var, int32_t v)
{
    lv_obj_set_x(var, v);
}

void draw_scr_welcome(void)
{
    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_VER;
    grad.stops_count = 2;
    grad.stops[1].color = lv_color_hex(0x165369); // lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[0].color = lv_color_black();       // lv_palette_main(LV_PALETTE_BLUE);

    /*Shift the gradient to the bottom*/
    grad.stops[0].frac = 128;
    grad.stops[1].frac = 255;

    // lv_style_set_bg_color(&style_scr_back, lv_color_black());
    lv_style_set_bg_grad(&style_welcome_scr_bg, &grad);

    lv_obj_t *scr_welcome = lv_obj_create(NULL);
    lv_obj_add_style(scr_welcome, &style_welcome_scr_bg, 0);

    draw_header(scr_welcome, false);

    /*Make a gradient*/
    lv_style_set_bg_opa(&style_welcome_scr_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_welcome_scr_bg, 0);

    lv_obj_t *label_hpi = lv_label_create(scr_welcome);
    lv_label_set_text(label_hpi, "Welcome to HealthyPi 5 !");
    lv_obj_add_style(label_hpi, &style_h1, 0);
    // lv_obj_center(label_hpi);
    lv_obj_align(label_hpi, LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_t *label_icon1 = lv_label_create(scr_welcome);
    lv_label_set_text(label_icon1, LV_SYMBOL_DIRECTORY);
    lv_obj_add_style(label_icon1, &style_icon, 0);
    // lv_obj_align(label_icon, LV_ALIGN_CENTER, 0, -20);
    lv_obj_align(label_icon1, LV_ALIGN_LEFT_MID, 80, -50);

    lv_obj_t *label_icon = lv_label_create(scr_welcome);
    lv_label_set_text(label_icon, LV_SYMBOL_COPY);
    lv_obj_add_style(label_icon, &style_icon, 0);
    // lv_obj_align(label_icon, LV_ALIGN_CENTER, 0, -20);
    lv_obj_align_to(label_icon, label_icon1, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    lv_obj_t *label_icon2 = lv_label_create(scr_welcome);
    lv_label_set_text(label_icon2, LV_SYMBOL_DOWNLOAD);
    lv_obj_add_style(label_icon2, &style_icon, 0);
    // lv_obj_align(label_icon, LV_ALIGN_CENTER, 0, -20);
    lv_obj_align_to(label_icon2, label_icon1, LV_ALIGN_OUT_RIGHT_MID, 250, 0);

    lv_obj_t *label_info1 = lv_label_create(scr_welcome);
    lv_label_set_text(label_info1, "HealthyPi is designed to adapt to your application.\n\n Download and \"drop\" your preferred firmware \nfrom the following website");
    lv_obj_set_style_text_align(label_info1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_style(label_info1, &style_info, 0);
    lv_obj_align(label_info1, LV_ALIGN_CENTER, 0, 30);
    // lv_obj_align_to(label_info1, label_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t *label_info2 = lv_label_create(scr_welcome);
    lv_label_set_text(label_info2, "protocentral.com/healthypi");
    lv_obj_add_style(label_info2, &style_h2, 0);
    // lv_obj_center(label_hpi);
    // lv_obj_align(label_info2, LV_ALIGN_CENTER, 0, 90);
    lv_obj_align_to(label_info2, label_info1, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_icon);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 4000);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_playback_time(&a, 300);
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    // lv_anim_set_exec_cb(&a, anim_size_cb);
    // lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, 120, 340);
    lv_anim_start(&a);

    lv_scr_load_anim(scr_welcome, LV_SCR_LOAD_ANIM_OUT_BOTTOM, 100, 0, true);
}