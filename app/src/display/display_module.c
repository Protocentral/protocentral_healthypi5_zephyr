#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/smf.h>
#include <app_version.h>

#include "hw_module.h"
#include "display_module.h"
#include "sampling_module.h"
#include "data_module.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_module, CONFIG_SENSOR_LOG_LEVEL);

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

extern uint8_t m_key_pressed;

const struct device *display_dev;

// LVGL Screens

lv_obj_t *scr_menu;
lv_obj_t *scr_charts_all;
lv_obj_t *scr_charts_single;

lv_style_t style_sub;
lv_style_t style_hr;
lv_style_t style_spo2;
lv_style_t style_rr;
lv_style_t style_temp;

lv_style_t style_header_black;

lv_style_t style_welcome_scr_bg;
lv_style_t style_batt_sym;

lv_style_t style_h1;
lv_style_t style_h2;
lv_style_t style_info;
lv_style_t style_icon;

// static lv_obj_t *roller_session_select;
// static lv_obj_t *label_current_mode;

static lv_obj_t *label_batt_level;
static lv_obj_t *label_batt_level_val;
//static lv_obj_t *label_sym_ble;

extern struct k_sem sem_hw_inited;
K_SEM_DEFINE(sem_disp_inited, 0, 1);

bool display_inited = false;

extern struct k_sem sem_ok_key_pressed;
extern struct k_sem sem_up_key_pressed;
extern struct k_sem sem_down_key_pressed;

K_MSGQ_DEFINE(q_plot_ecg_bioz, sizeof(struct hpi_ecg_bioz_sensor_data_t), 100, 1);
K_MSGQ_DEFINE(q_plot_ppg, sizeof(struct hpi_ppg_sensor_data_t), 100, 1);

extern struct k_msgq q_computed_val;

uint8_t curr_screen = SCR_ECG;

lv_obj_t *scr_chart_single;

lv_obj_t *scr_chart_single_resp;
lv_obj_t *scr_chart_single_ppg;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_pr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;
static lv_obj_t *label_temp;

void display_init_styles()
{
    // Subscript (Unit) label style
    lv_style_init(&style_sub);
    lv_style_set_text_color(&style_sub, lv_color_white());
    lv_style_set_text_font(&style_sub, &lv_font_montserrat_16);

    // HR Number label style
    lv_style_init(&style_hr);
    lv_style_set_text_color(&style_hr, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_font(&style_hr, &lv_font_montserrat_34);

    // SpO2 label style
    lv_style_init(&style_spo2);
    lv_style_set_text_color(&style_spo2, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_font(&style_spo2, &lv_font_montserrat_34);

    // RR label style
    lv_style_init(&style_rr);
    lv_style_set_text_color(&style_rr, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_font(&style_rr, &lv_font_montserrat_34);

    // Temp label style
    lv_style_init(&style_temp);
    lv_style_set_text_color(&style_temp, lv_palette_main(LV_PALETTE_LIME));
    lv_style_set_text_font(&style_temp, &lv_font_montserrat_34);

    // Icon welcome screen style
    lv_style_init(&style_icon);
    lv_style_set_text_color(&style_icon, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_font(&style_icon, &lv_font_montserrat_34);

    // H1 welcome screen style
    lv_style_init(&style_h1);
    lv_style_set_text_color(&style_h1, lv_color_white());
    lv_style_set_text_font(&style_h1, &lv_font_montserrat_34);

    lv_style_init(&style_header_black);
    lv_style_set_text_color(&style_header_black, lv_color_black());
    lv_style_set_text_font(&style_header_black, &lv_font_montserrat_14);

    // H2 welcome screen style
    lv_style_init(&style_h2);
    lv_style_set_text_color(&style_h2, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_text_font(&style_h2, &lv_font_montserrat_28);

    // Info welcome screen style
    lv_style_init(&style_info);
    lv_style_set_text_color(&style_info, lv_color_white());
    lv_style_set_text_font(&style_info, &lv_font_montserrat_16);

    // Screen background style
    lv_style_init(&style_welcome_scr_bg);
    // lv_style_set_radius(&style, 5);

    /*Make a gradient*/
    lv_style_set_bg_opa(&style_welcome_scr_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_welcome_scr_bg, 0);

    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_HOR;
    grad.stops_count = 2;
    grad.stops[0].color = lv_color_hex(0x003a57); // lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[1].color = lv_color_black();       // lv_palette_main(LV_PALETTE_BLUE);

    /*Shift the gradient to the bottom*/
    grad.stops[0].frac = 128;
    grad.stops[1].frac = 192;

    lv_style_set_bg_color(&style_welcome_scr_bg, lv_color_black());
    // lv_style_set_bg_grad(&style_scr_back, &grad);
}

void hpi_disp_update_temp(int32_t temp)
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

void hpi_disp_change_event(enum hpi_scr_event evt)
{
    if (evt == HPI_SCR_EVENT_DOWN)
    {
        printf("DOWN at %d\n", curr_screen);

        if ((curr_screen + 1) == SCR_LIST_END)
        {
            printk("End of list\n");
            //return;
            hpi_load_screen(SCR_LIST_START+1, SCROLL_LEFT);
        }
        else
        {
            printk("Loading screen %d\n", curr_screen + 1);
            hpi_load_screen(curr_screen + 1, SCROLL_LEFT);
        }
    }
    else if (evt == HPI_SCR_EVENT_UP)
    {
        printf("UP at %d\n", curr_screen);

        if ((curr_screen - 1) == SCR_LIST_START)
        {
            printk("Start of list\n");
            hpi_load_screen(SCR_LIST_END-1, SCROLL_RIGHT);
            //return;
        }
        else
        {
            printk("Loading screen %d\n", curr_screen - 1);
            hpi_load_screen(curr_screen - 1, SCROLL_RIGHT);
        }
    }
}

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
    lv_obj_align(label_hr, LV_ALIGN_LEFT_MID, 20, 120);
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
    // lv_obj_t *label_hr_status = lv_label_create(parent);
    // lv_label_set_text(label_hr_status, "ON");
    // lv_obj_align_to(label_hr_status, label_hr_sub, LV_ALIGN_BOTTOM_MID, 0, 17);
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

    // Pulse Rate Number label
    label_pr = lv_label_create(parent);
    lv_label_set_text(label_pr, "---");
    lv_obj_align_to(label_pr, label_spo2, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
    lv_obj_add_style(label_pr, &style_hr, LV_STATE_DEFAULT);

    // Pulse Rate Title label
    lv_obj_t *label_pr_title = lv_label_create(parent);
    lv_label_set_text(label_pr_title, "Pulse");
    lv_obj_align_to(label_pr_title, label_pr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_pr_title, &style_sub, LV_STATE_DEFAULT);

    // Pulse Rate Sub BPM label
    lv_obj_t *label_pr_sub = lv_label_create(parent);
    lv_label_set_text(label_pr_sub, "bpm");
    lv_obj_align_to(label_pr_sub, label_pr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_pr_sub, &style_sub, LV_STATE_DEFAULT);

    // RR Number label
    label_rr = lv_label_create(parent);
    lv_label_set_text(label_rr, "---");
    lv_obj_align_to(label_rr, label_pr, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
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
    lv_obj_align_to(label_temp, label_rr, LV_ALIGN_OUT_RIGHT_TOP, 50, 0);
    lv_obj_add_style(label_temp, &style_temp, LV_STATE_DEFAULT);

    // Temp label
    lv_obj_t *label_temp_title = lv_label_create(parent);
    lv_label_set_text(label_temp_title, "Temp");
    lv_obj_align_to(label_temp_title, label_temp, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub = lv_label_create(parent);
    lv_label_set_text(label_temp_sub, "°C");
    lv_obj_align_to(label_temp_sub, label_temp, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);

    // lv_obj_t *label_menu = lv_label_create(parent);
    // lv_label_set_text(label_menu, "Press side wheel DOWN for more charts");
    // lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void down_key_event_handler()
{
    printk("Down key pressed\n");
    // hpi_load_screen();
}

void hpi_load_screen(enum hpi_disp_screens m_screen, enum scroll_dir m_scroll_dir)
{
    switch (m_screen)
    {
    case SCR_ECG:
        draw_scr_ecg(SCROLL_DOWN);
        break;
    case SCR_RESP:
        draw_scr_resp(SCROLL_DOWN);
        break;
    case SCR_PPG:
        draw_scr_ppg(SCROLL_DOWN);
        break;

    default:
        break;
    }
}

void disp_screen_event(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    
    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Left at %d\n", curr_screen);

        if ((curr_screen + 1) == SCR_LIST_END)
        {
            printk("End of list\n");
            hpi_load_screen(SCR_LIST_START+1, SCROLL_LEFT);
            //return;
        }
        else
        {
            printk("Loading screen %d\n", curr_screen + 1);
            hpi_load_screen(curr_screen + 1, SCROLL_LEFT);
        }
    }

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Right at %d\n", curr_screen);
        if ((curr_screen - 1) == SCR_LIST_START)
        {
            printk("Start of list\n");
            hpi_load_screen(SCR_LIST_END, SCROLL_RIGHT);
            //return;
        }
        else
        {

            printk("Loading screen %d\n", curr_screen - 1);
            hpi_load_screen(curr_screen - 1, SCROLL_RIGHT);
        }
    }
}

void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir)
{
    lv_obj_add_event_cb(parent, disp_screen_event, LV_EVENT_GESTURE, NULL);

    if (m_scroll_dir == SCROLL_LEFT)
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    else
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    // lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_MOVE_RIGHT, SCREEN_TRANS_TIME, 0, true);
}

void draw_header(lv_obj_t *parent, bool showFWVersion)
{
    // Draw Header bar
    // ProtoCentral logo
    /*LV_IMG_DECLARE(logo_oneline);
    lv_obj_t *img1 = lv_img_create(parent);
    lv_img_set_src(img1, &logo_oneline);
    lv_obj_align(img1, LV_ALIGN_TOP_LEFT, 10, 7);
    lv_obj_set_size(img1, 104, 10);
    */

    lv_obj_t *header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, 480, 25);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, lv_color_white(), LV_STATE_DEFAULT);

    if (showFWVersion)
    {
        // HealthyPi label
        lv_obj_t *label_hpi = lv_label_create(parent);
        char fw_version[40];
        sprintf(fw_version, " HealthyPi 5 (FW v%d.%d.%d)", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
        lv_label_set_text(label_hpi, fw_version);
        lv_obj_add_style(label_hpi, &style_header_black, LV_STATE_DEFAULT);
        // lv_style_set_text_color(label_hpi, lv_color_black());
        lv_obj_align(label_hpi, LV_ALIGN_TOP_LEFT, 3, 1);
    }
    // Label for Symbols

    label_batt_level = lv_label_create(parent);
    lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align(label_batt_level, LV_ALIGN_TOP_RIGHT, -15, 1);

    label_batt_level_val = lv_label_create(parent);
    lv_label_set_text(label_batt_level_val, "--%");
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align_to(label_batt_level_val, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -25, 1);
    lv_obj_add_style(label_batt_level_val, &style_header_black, LV_STATE_DEFAULT);

    // label_sym_ble = lv_label_create(parent);
    // lv_label_set_text(label_sym_ble, LV_SYMBOL_BLUETOOTH);
    // lv_obj_add_style(label_sym_ble, &style_header_black, LV_STATE_DEFAULT);
    // lv_obj_align_to(label_sym_ble, label_batt_level_val, LV_ALIGN_OUT_LEFT_MID, -5, 1);
}

void hpi_disp_update_batt_level(int batt_level)
{
    if (label_batt_level == NULL)
        return;

    char buf[32];
    sprintf(buf, "%d %% ", batt_level);
    lv_label_set_text(label_batt_level_val, buf);

    if (batt_level > 75)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    }
    else if (batt_level > 50)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_3);
    }
    else if (batt_level > 25)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_2);
    }
    else if (batt_level > 10)
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_1);
    }
    else
    {
        lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_EMPTY);
    }
}

void display_screens_thread(void)
{
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;
    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    k_sem_take(&sem_hw_inited, K_FOREVER);

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev))
    {
        LOG_ERR("Display not available. Stopping display thread");
        return;
    }
    // Init all styles globally
    display_init_styles();

    // Setup LVGL Input Device
    /*lv_indev_drv_init(&m_keypad_drv);
    m_keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
    m_keypad_drv.read_cb = keypad_read;
    m_keypad_indev = lv_indev_drv_register(&m_keypad_drv);
    */

    display_blanking_off(display_dev);

    printk("Display screens inited");

    // draw_scr_ecg(SCROLL_DOWN);
    //   draw_scr_resp(SCROLL_DOWN);
    // draw_scr_ppg(SCROLL_DOWN);

    // draw_scr_welcome();
    hpi_load_screen(SCR_ECG, SCROLL_DOWN);

    int sample_count = 0;
    while (1)
    {
        if (k_msgq_get(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {
            if (curr_screen == SCR_ECG)
            {
                hpi_ecg_disp_draw_plot_ecg(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples, ecg_bioz_sensor_sample.ecg_lead_off);
                hpi_scr_home_update_hr(ecg_bioz_sensor_sample.hr);
                // hpi_ecg_disp_update_hr(ecg_bioz_sensor_sample.hr);
            }
            else if (curr_screen == SCR_RESP)
            {
                hpi_resp_disp_draw_plot_resp(ecg_bioz_sensor_sample.bioz_samples, ecg_bioz_sensor_sample.bioz_num_samples, ecg_bioz_sensor_sample.bioz_lead_off);
                // hpi_scr_home_update_rr(ecg_bioz_sensor_sample.rr);
            }
        }

        if (k_msgq_get(&q_plot_ppg, &ppg_sensor_sample, K_NO_WAIT) == 0)
        {
            if (curr_screen == SCR_PPG)
            {
                hpi_ppg_disp_draw_plot_ppg(ppg_sensor_sample.ppg_red_sample, ppg_sensor_sample.ppg_ir_sample, ppg_sensor_sample.ppg_lead_off);
            }
        }

        if (sample_count >= TEMP_SAMPLING_INTERVAL_COUNT)
        {
            sample_count = 0;
            // hpi_scr_home_update_temp(sensor_sample.temp);
            // hpi_scr_home_update_hr(sensor_sample.hr);
        }
        else
        {
            sample_count++;
        }

        if (k_sem_take(&sem_down_key_pressed, K_NO_WAIT) == 0)
        {
            hpi_disp_change_event(HPI_SCR_EVENT_DOWN);
        }

        if (k_sem_take(&sem_up_key_pressed, K_NO_WAIT) == 0)
        {
            hpi_disp_change_event(HPI_SCR_EVENT_UP);
        }

        lv_task_handler();
        k_sleep(K_MSEC(1));
    }
}

#define DISPLAY_SCREENS_THREAD_STACKSIZE 8192
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
