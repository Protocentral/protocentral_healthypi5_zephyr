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

#include "sys_sm_module.h"

#define SAMPLE_RATE 125
#define DISP_WINDOW_SIZE 500      // SAMPLE_RATE * 4

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

// LVGL Common Objects
static lv_indev_drv_t m_keypad_drv;
static lv_indev_t *m_keypad_indev = NULL;
extern uint8_t m_key_pressed;

const struct device *display_dev;

// GUI Charts
static lv_obj_t *chart1;
static lv_obj_t *chart2;
static lv_obj_t *chart3;

static lv_chart_series_t *ser1;
static lv_chart_series_t *ser2;
static lv_chart_series_t *ser3;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;
static lv_obj_t *label_temp;

// LVGL Screens
lv_obj_t *scr_home;
lv_obj_t *scr_menu;
lv_obj_t *scr_charts_all;
lv_obj_t *scr_charts_single;
lv_obj_t *scr_clock;

lv_obj_t *scr_chart_single;

static lv_style_t style_sub;
static lv_style_t style_hr;
static lv_style_t style_spo2;
static lv_style_t style_rr;
static lv_style_t style_temp;
static lv_style_t style_scr_back;
static lv_style_t style_batt_sym;
static lv_style_t style_h1;
static lv_style_t style_h2;
static lv_style_t style_info;
static lv_style_t style_icon;

static lv_obj_t *roller_session_select;

static lv_obj_t *label_current_mode;
static lv_style_t style_scr_back;

bool chart1_update = true;
bool chart2_update = false;
bool chart3_update = false;

float y1_max = 0;
float y1_min = 10000;

float y2_max = 0;
float y2_min = 10000;

float y3_max = 0;
float y3_min = 10000;

static float gx = 0;

int curr_mode = MODE_STANDBY;

extern struct k_sem sem_hw_inited;
K_SEM_DEFINE(sem_disp_inited, 0, 1);

bool display_inited = false;

static lv_obj_t *label_batt_level;
static lv_obj_t *label_batt_level_val;
static lv_obj_t *label_sym_ble;

enum hpi_sensor_data_type
{
    HPI_SENSOR_DATA_ECG = 0x01,
    HPI_SENSOR_DATA_PPG,
    HPI_SENSOR_DATA_RESP,
    HPI_SENSOR_DATA_TEMP
};

enum hpi_disp_screens
{
    HPI_DISP_SCREEN_HOME = 0x01,
    HPI_DISP_SCR_ECG,
    HPI_DISP_SCR_PPG,
    HPI_DISP_SCR_RESP,
};

K_MSGQ_DEFINE(q_plot, sizeof(struct hpi_sensor_data_t), 100, 1);

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
    lv_style_set_text_font(&style_temp, &lv_font_montserrat_24);

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
    lv_style_init(&style_scr_back);
    // lv_style_set_radius(&style, 5);

    /*Make a gradient*/
    lv_style_set_bg_opa(&style_scr_back, LV_OPA_COVER);
    lv_style_set_border_width(&style_scr_back, 0);

    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_HOR;
    grad.stops_count = 2;
    grad.stops[0].color = lv_color_hex(0x003a57); // lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[1].color = lv_color_black();       // lv_palette_main(LV_PALETTE_BLUE);

    /*Shift the gradient to the bottom*/
    grad.stops[0].frac = 128;
    grad.stops[1].frac = 192;

    //lv_style_set_bg_color(&style_scr_back, lv_color_black());
    lv_style_set_bg_grad(&style_scr_back, &grad);
}
static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
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

    /* key press */
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

    /* reset the keys */
    if ((m_key_pressed != GPIO_KEYPAD_KEY_NONE))
    {
        call_count = 0;
        m_key_pressed = GPIO_KEYPAD_KEY_NONE;
        // m_press_type = UNKNOWN;
    }
}

void menu_roller_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *roller = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint16_t sel = lv_roller_get_selected(roller);
        printk("Roller changed: %d\n", sel);
        switch (sel)
        {
        
            break;
        }

        // display_load_session_preview(sel);
    }
}

void menu_roller_remove_event(void)
{
    lv_obj_remove_event_cb(roller_session_select, menu_roller_event_handler);
}

void draw_header(lv_obj_t *parent, bool showFWVersion)
{
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_obj_add_style(parent, &style, 0);

    // Draw Header bar
    // ProtoCentral logo
    LV_IMG_DECLARE(logo_oneline);
    lv_obj_t *img1 = lv_img_create(parent);
    lv_img_set_src(img1, &logo_oneline);
    lv_obj_align(img1, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_set_size(img1, 104, 10);
    
    if (showFWVersion)
    {
        // HealthyPi label
        lv_obj_t *label_hpi = lv_label_create(parent);
        char fw_version[40];
        sprintf(fw_version, " HealthyPi 5 (FW v%d.%d.%d Z)", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
        lv_label_set_text(label_hpi, fw_version);
        lv_obj_align(label_hpi, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    // Label for Symbols

    label_batt_level = lv_label_create(parent);
    lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(label_batt_level, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align(label_batt_level, LV_ALIGN_TOP_MID, 15, 25);

    label_batt_level_val = lv_label_create(parent);
    lv_label_set_text(label_batt_level_val, "--%");
    // lv_obj_add_style(label_batt_level, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align_to(label_batt_level_val, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -7, 0);

    label_sym_ble = lv_label_create(parent);
    lv_label_set_text(label_sym_ble, LV_SYMBOL_BLUETOOTH);
    lv_obj_add_style(label_sym_ble, &style_batt_sym, LV_STATE_DEFAULT);
    lv_obj_align_to(label_sym_ble, label_batt_level_val, LV_ALIGN_OUT_LEFT_MID, -5, 0);
}

void hpi_disp_add_samples(int num_samples)
{
    gx += num_samples;
}

void hpi_disp_do_set_scale()
{
    if (gx >= DISP_WINDOW_SIZE)
    {
        if (chart1_update == true)
            lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, y1_min, y1_max);

        gx = 0;
        y1_max = -100000;
        y1_min = 100000;

        y2_max = -100000;
        y2_min = 100000;

        y3_max = -100000;
        y3_min = 100000;
    }
}

void hpi_disp_draw_plotECG(float data_ecg)
{
    if (chart1_update == true)
    {
        if (data_ecg < y1_min)
        {
            y1_min = data_ecg;
        }

        if (data_ecg > y1_max)
        {
            y1_max = data_ecg;
        }

        // printk("E");
        lv_chart_set_next_value(chart1, ser1, data_ecg);
        hpi_disp_add_samples(1);
        hpi_disp_do_set_scale();
    }
}

void hpi_disp_update_temp(int temp)
{
    if (label_temp == NULL)
        return;

    char buf[32];
    double temp_d = (double)(temp / 1000.00);
    sprintf(buf, "%.1f", temp_d);
    lv_label_set_text(label_temp, buf);
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

void draw_scr_home(void)
{
    scr_home = lv_obj_create(NULL);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 1);

    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_border_width(&style, 0);

    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_HOR;
    grad.stops_count = 2;
    grad.stops[0].color = lv_color_hex(0x003a57); // lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[1].color = lv_color_black();       // lv_palette_main(LV_PALETTE_BLUE);

    grad.stops[0].frac = 128;
    grad.stops[1].frac = 192;


    lv_style_set_bg_grad(&style, &grad);
    //lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_obj_add_style(scr_home, &style, 0);

    LV_IMG_DECLARE(logo_round_50x50);
    lv_obj_t *img1 = lv_img_create(scr_home);
    lv_img_set_src(img1, &logo_round_50x50);
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(img1, 50, 50);

    lv_obj_t *label_hpi = lv_label_create(scr_home);
    lv_label_set_text(label_hpi, "Welcome to HealthyPi Move");
    //lv_obj_add_style(label_hpi, &style_h1, 0);
    //lv_obj_center(label_hpi);
    lv_obj_align(label_hpi, LV_ALIGN_TOP_MID, 0, 20);


    lv_obj_t *lbl_mode_disp = lv_label_create(scr_home);
    lv_label_set_text(lbl_mode_disp, "Current Mode");
    lv_obj_align(lbl_mode_disp, LV_ALIGN_CENTER, 0, 75);

    label_current_mode = lv_label_create(scr_home);
    lv_label_set_text(label_current_mode, "Standby");
    lv_obj_align(label_current_mode, LV_ALIGN_CENTER, 0, 95);
    lv_obj_add_style(label_current_mode, &style_h2, 0);

    //draw_footer_header(scr_home);

    lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_NONE, 0, 500, true);
}

void draw_scr_chart_single(uint8_t m_data_type)
{
    scr_chart_single = lv_obj_create(NULL);
    draw_header(scr_chart_single, false);

    lv_obj_add_style(scr_chart_single, &style_scr_back, 0);

    // Create Chart 1
    chart1 = lv_chart_create(scr_chart_single);
    lv_obj_set_size(chart1, 220, 100);
    lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_set_style_size(chart1, 0, LV_PART_INDICATOR);
    lv_chart_set_point_count(chart1, DISP_WINDOW_SIZE);
    // lv_chart_set_type(chart1, LV_CHART_TYPE_LINE);   /*Show lines and points too*
    lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    // lv_chart_set_range(chart1, LV_CHART_AXIS_SECONDARY_Y, 0, 1000);
    lv_chart_set_div_line_count(chart1, 0, 0);
    lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_CIRCULAR);

    //lv_obj_set_pos(chart1, 10, 25);
    lv_obj_center(chart1);

    lv_obj_t *label_chart_title = lv_label_create(scr_chart_single);

    if (m_data_type == HPI_SENSOR_DATA_ECG)
        lv_label_set_text(label_chart_title, "Showing ECG");
    else if (m_data_type == HPI_SENSOR_DATA_PPG)
        lv_label_set_text(label_chart_title, "Showing PPG");
    else if (m_data_type == HPI_SENSOR_DATA_RESP)
        lv_label_set_text(label_chart_title, "Showing RESP");
    else if (m_data_type == HPI_SENSOR_DATA_TEMP)
        lv_label_set_text(label_chart_title, "Showing TEMP");

    lv_obj_align_to(label_chart_title, chart1, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Temp Number label
    label_temp = lv_label_create(scr_chart_single);
    lv_label_set_text(label_temp, "--");
    lv_obj_align_to(label_temp, chart1, LV_ALIGN_OUT_BOTTOM_MID, -25, 45);
    //lv_obj_add_style(label_temp, &style_temp, LV_STATE_DEFAULT);

    // Temp label
    
    /*lv_obj_t *label_temp_title = lv_label_create(scr_chart_single);
    lv_label_set_text(label_temp_title, "Temp: ");
    lv_obj_align_to(label_temp_title, label_temp, LV_ALIGN_OUT_LEFT_MID, 0, 0);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);
    */

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub = lv_label_create(scr_chart_single);
    lv_label_set_text(label_temp_sub, "°F");
    lv_obj_align_to(label_temp_sub, label_temp, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    //lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);
    // lv_obj_add_style(label_rr_sub, &style_sub, LV_STATE_DEFAULT);

    /* Data Series for main plot*/
    if (m_data_type == HPI_SENSOR_DATA_ECG)
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    else if (m_data_type == HPI_SENSOR_DATA_PPG)
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
    else if (m_data_type == HPI_SENSOR_DATA_RESP)
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    else if (m_data_type == HPI_SENSOR_DATA_TEMP)
        ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_LIME), LV_CHART_AXIS_PRIMARY_Y);

    lv_scr_load_anim(scr_chart_single, LV_SCR_LOAD_ANIM_OUT_BOTTOM, 100, 0, true);
    // lv_scr_load(scr_chart_single);
}

static lv_obj_t *meter;

static void set_value(void *indic, int32_t v) {
  lv_meter_set_indicator_end_value(meter, (lv_meter_indicator_t *)indic, v);
}

void draw_scr_clockface(void)
{   
    scr_clock = lv_obj_create(NULL);
    meter = lv_meter_create(scr_clock);
    lv_obj_set_size(meter, 220, 220);
    lv_obj_center(meter);
    // lv_obj_set_style_bg_color(meter, LV_COLOR_MAKE(22, 83, 105), LV_STATE_DEFAULT);

    /*Create a scale for the minutes*/
    /*61 ticks in a 360 degrees range (the last and the first line overlaps)*/
    lv_meter_scale_t *scale_min = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale_min, 61, 1, 10, lv_color_make(22, 83, 105));
    lv_meter_set_scale_range(meter, scale_min, 0, 60, 360, 270);

    /*Create another scale for the hours. It's only visual and contains only major ticks*/
    lv_meter_scale_t *scale_hour = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale_hour, 12, 0, 0, lv_color_make(22, 83, 105));           /*12 ticks*/
    lv_meter_set_scale_major_ticks(meter, scale_hour, 1, 2, 20, lv_color_make(22, 83, 105), 10); /*Every tick is major*/
    lv_meter_set_scale_range(meter, scale_hour, 1, 12, 330, 300);                                /*[1..12] values in an almost full circle*/

    LV_IMG_DECLARE(img_hand);

    /*Add a the hands from images*/
    lv_meter_indicator_t *indic_min = lv_meter_add_needle_img(meter, scale_min, &img_hand, 5, 5);
    lv_meter_indicator_t *indic_hour = lv_meter_add_needle_img(meter, scale_min, &img_hand, 5, 5);

    /*Create an animation to set the value*/
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);
    lv_anim_set_values(&a, 0, 60);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_time(&a, 2000); /*2 sec for 1 turn of the minute hand (1 hour)*/
    lv_anim_set_var(&a, indic_min);
    lv_anim_start(&a);

    lv_anim_set_var(&a, indic_hour);
    lv_anim_set_time(&a, 24000); /*24 sec for 1 turn of the hour hand*/
    lv_anim_set_values(&a, 0, 60);
    lv_anim_start(&a);

    // ProtoCentral logo
    LV_IMG_DECLARE(logo_oneline);
    lv_obj_t *img1 = lv_img_create(scr_clock);
    lv_img_set_src(img1, &logo_oneline);
    lv_obj_align_to(img1, meter, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_size(img1, 150, 10);

    // ProtoCentral round logo
    LV_IMG_DECLARE(logo_round_50x50);
    lv_obj_t *img2 = lv_img_create(scr_clock);
    lv_img_set_src(img2, &logo_round_50x50);
    lv_obj_align_to(img2, meter, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_size(img2, 50, 50);
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
    lv_indev_drv_init(&m_keypad_drv);
    m_keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
    m_keypad_drv.read_cb = keypad_read;
    m_keypad_indev = lv_indev_drv_register(&m_keypad_drv);

    display_blanking_off(display_dev);

    int ret=display_set_brightness(display_dev,0);
	
    printk("Display screens inited");
    // k_sem_give(&sem_disp_inited);
    //  draw_scr_menu("A\nB\n");
    //draw_scr_home();
    //draw_scr_clockface();

    struct hpi_sensor_data_t sensor_sample;
    //struct hpi_computed_data_t computed_data;
    
    draw_scr_chart_single(HPI_SENSOR_DATA_ECG);
    while (1)

    {
        k_msgq_get(&q_plot, &sensor_sample, K_FOREVER);

        //printk("P");
        
        hpi_disp_draw_plotECG((float)((sensor_sample.ecg_sample) / 1000000.0000));

        lv_task_handler();
        k_sleep(K_MSEC(4));
    }
}

#define DISPLAY_SCREENS_THREAD_STACKSIZE 8192
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
