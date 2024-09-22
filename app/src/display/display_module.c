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
#ifdef CONFIG_HEALTHYPI_DISPLAY_ENABLED
#include "display_module.h"
#endif
#include "sampling_module.h"
#include "data_module.h"
#include "sys_sm_module.h"

#define SAMPLE_RATE 125
#define DISP_WINDOW_SIZE 625 // SAMPLE_RATE * 4

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

extern uint8_t m_key_pressed;

const struct device *display_dev;

// GUI Charts
static lv_obj_t *chart1;

static lv_chart_series_t *ser1;

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

// static lv_obj_t *roller_session_select;
// static lv_obj_t *label_current_mode;

static lv_obj_t *label_batt_level;
static lv_obj_t *label_batt_level_val;
static lv_obj_t *label_sym_ble;

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

extern struct k_sem sem_ok_key_pressed;
extern struct k_sem sem_up_key_pressed;
extern struct k_sem sem_down_key_pressed;

K_MSGQ_DEFINE(q_plot_ecg_bioz, sizeof(struct hpi_ecg_bioz_sensor_data_t), 100, 1);
extern struct k_msgq q_computed_val;

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

uint8_t hpi_disp_curr_screen = HPI_DISP_SCR_ECG;

lv_obj_t *scr_chart_single;
lv_obj_t *scr_chart_single_ecg;
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

    lv_style_set_bg_color(&style_scr_back, lv_color_black());
    // lv_style_set_bg_grad(&style_scr_back, &grad);
}

void down_key_event_handler()
{
    printk("Down key pressed\n");
    hpi_disp_switch_screen();
}

void hpi_disp_switch_screen(void)
{
    switch (hpi_disp_curr_screen)
    {
    case HPI_DISP_SCR_ECG:
        draw_chart_single_scr(HPI_SENSOR_DATA_PPG, scr_chart_single_ppg);
        break;
    case HPI_DISP_SCR_PPG:
        draw_chart_single_scr(HPI_SENSOR_DATA_RESP, scr_chart_single_resp);
        break;
    case HPI_DISP_SCR_RESP:
        draw_chart_single_scr(HPI_SENSOR_DATA_ECG, scr_chart_single_ecg);
        break;
    default:
        break;
    }
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

void draw_footer(lv_obj_t *parent)
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

void hpi_disp_update_temp(int temp)
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

void hpi_disp_update_hr(int hr)
{
    if (label_hr == NULL)
        return;

    char buf[32];
    sprintf(buf, "%d", hr);
    lv_label_set_text(label_hr, buf);
}

void hpi_disp_update_spo2(int spo2)
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

void hpi_disp_update_rr(int rr)
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

void hpi_disp_draw_plot(float plot_data)
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
        hpi_disp_add_samples(1);
        hpi_disp_do_set_scale();
    }
}

void draw_chart_single_scr(uint8_t m_data_type, lv_obj_t *scr_obj)
{
    // lv_obj_clean(scr_obj);

    if (scr_obj == NULL)
    {
        scr_obj = lv_obj_create(NULL);
        draw_footer(scr_obj);
        draw_header(scr_obj, true);

        lv_obj_add_style(scr_obj, &style_scr_back, 0);

        // lv_group_t *g1 = lv_group_create();

        // Create Chart 1
        chart1 = lv_chart_create(scr_obj);
        lv_obj_set_size(chart1, 460, 180);
        lv_obj_set_style_bg_color(chart1, lv_color_black(), LV_STATE_DEFAULT);

        lv_obj_set_style_size(chart1, 0, LV_PART_INDICATOR);
        lv_chart_set_point_count(chart1, DISP_WINDOW_SIZE);
        lv_chart_set_range(chart1, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
        lv_chart_set_div_line_count(chart1, 0, 0);
        lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_CIRCULAR);

        lv_obj_set_pos(chart1, 10, 25);

        lv_obj_t *label_chart_title = lv_label_create(scr_obj);

        if (m_data_type == HPI_SENSOR_DATA_ECG)
        {
            ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
            hpi_disp_curr_screen = HPI_DISP_SCR_ECG;
            lv_label_set_text(label_chart_title, "Showing ECG");
        }
        else if (m_data_type == HPI_SENSOR_DATA_PPG)
        {
            ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
            hpi_disp_curr_screen = HPI_DISP_SCR_PPG;
            lv_label_set_text(label_chart_title, "Showing PPG");
        }
        else if (m_data_type == HPI_SENSOR_DATA_RESP)
        {
            ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
            hpi_disp_curr_screen = HPI_DISP_SCR_RESP;
            lv_label_set_text(label_chart_title, "Showing RESP");
        }

        lv_obj_align_to(label_chart_title, chart1, LV_ALIGN_OUT_TOP_MID, 0, 20);
    }

    lv_scr_load_anim(scr_obj, LV_SCR_LOAD_ANIM_OUT_BOTTOM, 100, 0, true);
}

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
    lv_style_set_bg_grad(&style_scr_back, &grad);

    lv_obj_t *scr_welcome = lv_obj_create(NULL);
    lv_obj_add_style(scr_welcome, &style_scr_back, 0);

    draw_header(scr_welcome, false);

    /*Make a gradient*/
    lv_style_set_bg_opa(&style_scr_back, LV_OPA_COVER);
    lv_style_set_border_width(&style_scr_back, 0);

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

#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

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
    //struct hpi_sensor_data_t sensor_sample;
    struct hpi_computed_data_t computed_data;
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;

    // draw_scr_chart_single(HPI_SENSOR_DATA_PPG);
    draw_chart_single_scr(HPI_SENSOR_DATA_ECG, scr_chart_single_ecg);

    // draw_scr_welcome();

    int sample_count = 0;
    while (1)
    {
        if (k_msgq_get(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {

            if (hpi_disp_curr_screen == HPI_DISP_SCR_ECG)
            {
                hpi_disp_draw_plot((float)((sensor_sample.ecg_sample) / 100.0000));
            }
            else if (hpi_disp_curr_screen == HPI_DISP_SCR_PPG)
            {
                hpi_disp_draw_plot((sensor_sample.raw_ir) / 1000.0000);
            }

            else if (hpi_disp_curr_screen == HPI_DISP_SCR_RESP)
            {
                hpi_disp_draw_plot((sensor_sample.bioz_sample) / 100.0000);
            }

            if (sample_count >= TEMP_SAMPLING_INTERVAL_COUNT)
            {
                sample_count = 0;
                hpi_disp_update_temp(sensor_sample.temp);
                hpi_disp_update_hr(sensor_sample.hr);
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

                // hpi_disp_update_hr(computed_data.hr);
                hpi_disp_update_spo2(computed_data.spo2);
                hpi_disp_update_rr(computed_data.rr);
            }

            lv_task_handler();
            if (k_sem_take(&sem_down_key_pressed, K_NO_WAIT) == 0)
            {
                down_key_event_handler();
            }
            k_sleep(K_MSEC(4));
        }
    }
}

#define DISPLAY_SCREENS_THREAD_STACKSIZE 4096
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
