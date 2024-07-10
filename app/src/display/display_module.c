#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/smf.h>
#include <app_version.h>

#include "sys_sm_module.h"
#include "hw_module.h"

#include "display_module.h"
#include "sampling_module.h"
#include "data_module.h"
#include "sys_sm_module.h"

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

extern uint8_t m_key_pressed;

const struct device *display_dev;

// GUI Charts
static lv_obj_t *chart1;

static lv_chart_series_t *ser1;

// LVGL Screens

lv_obj_t *scr_menu;
lv_obj_t *scr_charts_all;
lv_obj_t *scr_charts_single;

lv_style_t style_sub;
lv_style_t style_hr;
lv_style_t style_spo2;
lv_style_t style_rr;
lv_style_t style_temp;

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
static lv_obj_t *label_sym_ble;

bool chart2_update = false;
bool chart3_update = false;

float y2_max = 0;
float y2_min = 10000;

float y3_max = 0;
float y3_min = 10000;

static float gx = 0;

int curr_mode = MODE_STANDBY;

extern struct k_sem sem_hw_inited;
K_SEM_DEFINE(sem_disp_inited, 0, 1);

bool display_inited = false;

extern struct k_sem sem_ok_key_pressed;
extern struct k_sem sem_up_key_pressed;
extern struct k_sem sem_down_key_pressed;

K_MSGQ_DEFINE(q_plot, sizeof(struct hpi_sensor_data_t), 100, 1);
extern struct k_msgq q_computed_val;

uint8_t curr_screen = SCR_POINCARE;

lv_obj_t *scr_chart_single;

lv_obj_t *scr_chart_single_resp;
lv_obj_t *scr_chart_single_ppg;

/*static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static int call_count = 0;

    switch (m_key_pressed)
    {
    case GPIO_KEYPAD_KEY_OK:
        printk("K OK");
        data->key = LV_KEY_ENTER;
        break;
    case GPIO_KEYPAD_KEY_UP:
        printk("K UP");
        data->key = LV_KEY_UP;
        break;
    case GPIO_KEYPAD_KEY_DOWN:
        printk("K DOWN");
        data->key = LV_KEY_DOWN;
        break;
    default:
        break;
    }

    if (m_key_pressed != GPIO_KEYPAD_KEY_NONE)
    {
        if (call_count == 0)
        {
            data->state = LV_INDEV_STATE_PR;
            call_count = 1;
        }
        else if (call_count == 1)
        {
            call_count = 2;
            data->state = LV_INDEV_STATE_REL;
        }
    }

    if ((m_key_pressed != GPIO_KEYPAD_KEY_NONE))
    {
        call_count = 0;
        m_key_pressed = GPIO_KEYPAD_KEY_NONE;
        // m_press_type = UNKNOWN;
    }
}*/

void display_init_styles()
{
    // Subscript (Unit) label style
    lv_style_init(&style_sub);
    lv_style_set_text_color(&style_sub, lv_color_white());
    lv_style_set_text_font(&style_sub, &lv_font_montserrat_16);

    // HR Number label style
    lv_style_init(&style_hr);
    lv_style_set_text_color(&style_hr, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_font(&style_hr, &lv_font_montserrat_42);

    // SpO2 label style
    lv_style_init(&style_spo2);
    lv_style_set_text_color(&style_spo2, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_font(&style_spo2, &lv_font_montserrat_42);

    // RR label style
    lv_style_init(&style_rr);
    lv_style_set_text_color(&style_rr, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_font(&style_rr, &lv_font_montserrat_42);

    // Temp label style
    lv_style_init(&style_temp);
    lv_style_set_text_color(&style_temp, lv_palette_main(LV_PALETTE_LIME));
    lv_style_set_text_font(&style_temp, &lv_font_montserrat_42);

    // Icon welcome screen style
    lv_style_init(&style_icon);
    lv_style_set_text_color(&style_icon, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_text_font(&style_icon, &lv_font_montserrat_42);

    // H1 welcome screen style
    lv_style_init(&style_h1);
    lv_style_set_text_color(&style_h1, lv_color_white());
    lv_style_set_text_font(&style_h1, &lv_font_montserrat_34);

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

void down_key_event_handler()
{
    printk("Down key pressed\n");
    // hpi_load_screen();
}

void hpi_load_screen(enum hpi_disp_screens m_screen, enum scroll_dir m_scroll_dir)
{
    switch (m_screen)
    {
    case SCR_HOME:
        draw_scr_home(SCROLL_DOWN);
        break;
    /*case HPI_DISP_SCR_ECG:
        draw_chart_single_scr(HPI_SENSOR_DATA_PPG, scr_chart_single_ppg);
        break;
    case HPI_DISP_SCR_PPG:
        draw_chart_single_scr(HPI_SENSOR_DATA_RESP, scr_chart_single_resp);
        break;
    case HPI_DISP_SCR_RESP:
        draw_chart_single_scr(HPI_SENSOR_DATA_ECG, scr_chart_single_ecg);
        break;*/
    default:
        break;
    }
}

void disp_screen_event(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Left at %d\n", curr_screen);

        if ((curr_screen + 1) == SCR_LIST_END)
        {
            printk("End of list\n");
            return;
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
            return;
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
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_MOVE_LEFT, SCREEN_TRANS_TIME, 0, true);
    else
        lv_scr_load_anim(parent, LV_SCR_LOAD_ANIM_MOVE_RIGHT, SCREEN_TRANS_TIME, 0, true);
}

void draw_header(lv_obj_t *parent, bool showFWVersion)
{
    /*static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_obj_add_style(parent, &style, 0);
    */

    // Draw Header bar
    // ProtoCentral logo
    /*LV_IMG_DECLARE(logo_oneline);
    lv_obj_t *img1 = lv_img_create(parent);
    lv_img_set_src(img1, &logo_oneline);
    lv_obj_align(img1, LV_ALIGN_TOP_LEFT, 10, 7);
    lv_obj_set_size(img1, 104, 10);
    */

    if (showFWVersion)
    {
        // HealthyPi label
        lv_obj_t *label_hpi = lv_label_create(parent);
        char fw_version[40];
        sprintf(fw_version, " HealthyPi 5 (FW v%d.%d.%d)", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
        lv_label_set_text(label_hpi, fw_version);
        lv_obj_align(label_hpi, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    // Label for Symbols

    label_batt_level = lv_label_create(parent);
    lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(label_batt_level, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align(label_batt_level, LV_ALIGN_TOP_RIGHT, -15, 0);

    label_batt_level_val = lv_label_create(parent);
    lv_label_set_text(label_batt_level_val, "--%");
    // lv_obj_add_style(label_batt_level, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align_to(label_batt_level_val, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -7, 0);

    label_sym_ble = lv_label_create(parent);
    lv_label_set_text(label_sym_ble, LV_SYMBOL_BLUETOOTH);
    lv_obj_add_style(label_sym_ble, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align_to(label_sym_ble, label_batt_level_val, LV_ALIGN_OUT_LEFT_MID, -5, 0);
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
    k_sem_take(&sem_hw_inited, K_FOREVER);

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev))
    {
        // LOG_ERR("Device not ready, aborting test");
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
    // k_sem_give(&sem_disp_inited);
    //  draw_scr_menu("A\nB\n");
    struct hpi_sensor_data_t sensor_sample;
    struct hpi_computed_data_t computed_data;
    struct hpi_computed_hrv_t hrv_calculated;

    // draw_scr_chart_single(HPI_SENSOR_DATA_PPG);
    // draw_chart_single_scr(HPI_SENSOR_DATA_ECG, scr_chart_single_ecg);

    draw_scr_poincare(SCROLL_DOWN);

    // draw_scr_welcome();

    int sample_count = 0;
    while (1)
    {
        if (k_msgq_get(&q_plot, &sensor_sample, K_NO_WAIT) == 0)
        {
            if (curr_screen == SCR_HOME)
            {
                scr_home_plot_ecg((float)((sensor_sample.ecg_sample) / 100.0000));
                scr_home_plot_ppg((float)((sensor_sample.raw_ir) / 1000.0000));
            }
            else if (curr_screen == SCR_ECG)
            {
                scr_ecg_plot_ecg((float)((sensor_sample.ecg_sample) / 100.0000));
            }
            else if (curr_screen == SCR_POINCARE)
            {
                //scr_poincare_plot_rr(lv_rand(600, 1200), lv_rand(600, 1200));
                scr_poincare_plot_rr((float)((sensor_sample.ecg_sample) / 100.0000),(float)((sensor_sample.raw_ir) / 1000.0000));
            }

            /*else if (curr_screen == HPI_DISP_SCR_PPG)
            {
                hpi_disp_draw_plot((sensor_sample.raw_ir) / 1000.0000);
            }

            else if (curr_screen == HPI_DISP_SCR_RESP)
            {
                hpi_disp_draw_plot((sensor_sample.bioz_sample) / 100.0000);
            }*/
        }

        if (sample_count >= TEMP_SAMPLING_INTERVAL_COUNT)
        {
            sample_count = 0;
            hpi_scr_home_update_temp(sensor_sample.temp);
            hpi_scr_home_update_hr(sensor_sample.hr);
        }
        else
        {
            sample_count++;
        }

        if (k_msgq_get(&q_computed_val, &computed_data, K_NO_WAIT) == 0)
        {
            printk("Got computed data");
            printk("SpO2: %d", computed_data.spo2);
            printk("HR: %d", computed_data.hr);
            printk("RR: %d\n", computed_data.rr);

            // hpi_scr_home_update_hr(computed_data.hr);
            hpi_scr_home_update_spo2(computed_data.spo2);
            hpi_scr_home_update_rr(computed_data.rr);
        }

        
        if (k_sem_take(&sem_down_key_pressed, K_NO_WAIT) == 0)
        {
            down_key_event_handler();
        }

        lv_task_handler();
        k_sleep(K_MSEC(4));
    }
}

#define DISPLAY_SCREENS_THREAD_STACKSIZE 4096
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
