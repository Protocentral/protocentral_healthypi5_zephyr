#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>
#include <app_version.h>

#include "hw_module.h"
#include "display_module.h"
#include "hpi_common_types.h"
#include "data_module.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_module, LOG_LEVEL_DBG);

#define HPI_DISP_BATT_REFR_INT 1000

#define HPI_DISP_TEMP_REFR_INT 1000
#define HPI_DISP_HR_REFR_INT 1000
#define HPI_DISP_SPO2_REFR_INT 1000

#define HPI_DISP_RR_REFR_INT 4000

lv_obj_t *btn_start_session;
lv_obj_t *btn_return;

extern uint8_t m_key_pressed;

const struct device *display_dev;

#ifdef CONFIG_HEALTHYPI_OP_MODE_DISPLAY
static enum hpi_disp_op_mode m_op_mode = OP_MODE_DISPLAY;
#else
static enum hpi_disp_op_mode m_op_mode =  OP_MODE_BASIC;
#endif

// LVGL Screens
lv_obj_t *scr_menu;
lv_obj_t *scr_charts_all;
lv_obj_t *scr_charts_single;

lv_style_t style_sub;

lv_style_t style_number_big;
lv_style_t style_number_medium;

lv_style_t style_header_black;
lv_style_t style_header_red;
lv_style_t style_header_green;
lv_style_t style_scr_back;


lv_style_t style_batt_sym;

lv_style_t style_h1;
lv_style_t style_h2;
lv_style_t style_info;
lv_style_t style_icon;

// static lv_obj_t *roller_session_select;
// static lv_obj_t *label_current_mode;

static lv_obj_t *label_batt_level;
static lv_obj_t *label_batt_level_val;
// static lv_obj_t *label_sym_ble;

static uint8_t m_disp_batt_level = 0;
static bool m_disp_batt_charging = false;
static int last_batt_refresh = 0;

static uint16_t m_disp_hr = 0;
static int last_hr_refresh = 0;

static float m_disp_temp_f = 0;
static float m_disp_temp_c = 0;

static int last_temp_refresh = 0;

static uint8_t m_disp_spo2 = 0;
static int last_spo2_refresh = 0;

static uint8_t m_disp_rr = 0;
static int last_rr_refresh = 0;

K_MSGQ_DEFINE(q_plot_ecg_bioz, sizeof(struct hpi_ecg_bioz_sensor_data_t), 64, 4);
K_MSGQ_DEFINE(q_plot_ppg, sizeof(struct hpi_ppg_sensor_data_t), 64, 4);

K_MSGQ_DEFINE(q_hpi_plot_all_sample, sizeof(struct hpi_sensor_data_point_t), 64, 1);

K_MUTEX_DEFINE(mutex_curr_screen);
K_SEM_DEFINE(sem_disp_inited, 0, 1);

bool display_inited = false;
static uint8_t curr_screen = SCR_ECG;

// GUI Labels
static lv_obj_t *label_hr;
static lv_obj_t *label_pr;
static lv_obj_t *label_spo2;
static lv_obj_t *label_rr;

static lv_obj_t *label_temp_f;
static lv_obj_t *label_temp_c;

// Externs

extern struct k_sem sem_hw_inited;
extern struct k_sem sem_ok_key_pressed;
extern struct k_sem sem_up_key_pressed;
extern struct k_sem sem_down_key_pressed;
extern struct k_msgq q_computed_val;

int hpi_disp_get_op_mode()
{
    return m_op_mode;
}

void display_init_styles()
{
    // Subscript (Unit) label style
    lv_style_init(&style_sub);
    lv_style_set_text_color(&style_sub, lv_color_white());
    lv_style_set_text_font(&style_sub, &lv_font_montserrat_16);

    lv_style_init(&style_number_big);
    lv_style_set_text_color(&style_number_big, lv_color_white());
    lv_style_set_text_font(&style_number_big, &lv_font_montserrat_42);

    lv_style_init(&style_number_medium);
    lv_style_set_text_color(&style_number_medium, lv_color_white());
    lv_style_set_text_font(&style_number_medium, &lv_font_montserrat_34);

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

    lv_style_init(&style_header_red);
    lv_style_set_text_color(&style_header_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_font(&style_header_red, &lv_font_montserrat_16);

    lv_style_init(&style_header_green);
    lv_style_set_text_color(&style_header_green, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_font(&style_header_green, &lv_font_montserrat_16);

    // H2 welcome screen style
    lv_style_init(&style_h2);
    lv_style_set_text_color(&style_h2, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_text_font(&style_h2, &lv_font_montserrat_28);

    // Info welcome screen style
    lv_style_init(&style_info);
    lv_style_set_text_color(&style_info, lv_color_white());
    lv_style_set_text_font(&style_info, &lv_font_montserrat_16);

    // lv_style_set_bg_grad(&style_scr_back, &grad);
}

static void hpi_disp_update_temp(float temp_f, float temp_c)
{
    if (curr_screen == SCR_HOME)
    {
        hpi_scr_home_update_temp(temp_f, temp_c);
    }
    else
    {
        if (label_temp_f == NULL)
            return;

        if (temp_f <= 0)
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
}

void hpi_scr_update_hr(int hr)
{
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_hr(hr);
    }
    else
    {
        if (label_hr == NULL)
            return;

        char buf[32];
        sprintf(buf, "%d", hr);
        lv_label_set_text(label_hr, buf);
    }
}

void hpi_scr_update_spo2(int spo2)
{
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_spo2(spo2);
    }
    else
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
}

void hpi_scr_update_pr(int pr)
{
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_pr(pr);
    }
    else
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
}

void hpi_scr_update_rr(int rr)
{
    if (hpi_disp_get_curr_screen() == SCR_HOME)
    {
        hpi_scr_home_update_rr(rr);
    }
    else
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
}

void hpi_disp_change_event(enum hpi_scr_event evt)
{
    if (evt == HPI_SCR_EVENT_DOWN)
    {
        printf("DOWN at %d\n", hpi_disp_get_curr_screen());

        if ((hpi_disp_get_curr_screen() + 1) == SCR_LIST_END)
        {
            printk("End of list\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
        }
        else
        {
            printk("Loading screen %d\n", hpi_disp_get_curr_screen() + 1);
            hpi_load_screen(hpi_disp_get_curr_screen() + 1, SCROLL_LEFT);
        }
    }
    else if (evt == HPI_SCR_EVENT_UP)
    {
        printf("UP at %d\n", hpi_disp_get_curr_screen());

        if ((hpi_disp_get_curr_screen() - 1) == SCR_LIST_START)
        {
            printk("Start of list\n");
            hpi_load_screen(SCR_LIST_END - 1, SCROLL_RIGHT);
            // return;
        }
        else
        {
            printk("Loading screen %d\n", hpi_disp_get_curr_screen() - 1);
            hpi_load_screen(hpi_disp_get_curr_screen() - 1, SCROLL_RIGHT);
        }
    }
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

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&style_scr_back);
    lv_style_set_bg_color(&style_scr_back, lv_color_black());
    lv_obj_add_style(parent, &style_scr_back, 0);

    //lv_disp_set_bg_color(NULL, lv_color_black());

    lv_obj_t *header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, 480, 30);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, lv_color_white(), LV_STATE_DEFAULT);

    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    if (showFWVersion)
    {
        // HealthyPi label
        lv_obj_t *label_hpi = lv_label_create(parent);
        char fw_version[40];
        sprintf(fw_version, " HealthyPi 5 (FW v%d.%d.%d)", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
        lv_label_set_text(label_hpi, fw_version);
        lv_obj_add_style(label_hpi, &style_header_black, LV_STATE_DEFAULT);
        // lv_style_set_text_color(label_hpi, lv_color_black());
        lv_obj_align(label_hpi, LV_ALIGN_TOP_LEFT, 3, 5);
    }
    // Label for Symbols

    label_batt_level = lv_label_create(parent);
    lv_label_set_text(label_batt_level, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align(label_batt_level, LV_ALIGN_TOP_RIGHT, -50, 5);

    label_batt_level_val = lv_label_create(parent);
    lv_label_set_text(label_batt_level_val, "--%");
    lv_obj_add_style(label_batt_level, &style_header_black, LV_STATE_DEFAULT);
    lv_obj_align_to(label_batt_level_val, label_batt_level, LV_ALIGN_OUT_RIGHT_MID, 7, 0);
    lv_obj_add_style(label_batt_level_val, &style_header_black, LV_STATE_DEFAULT);

    lv_obj_t *lbl_conn_status = lv_label_create(parent);
    if (m_op_mode == OP_MODE_DISPLAY)
    {

        lv_label_set_text(lbl_conn_status, LV_SYMBOL_BLUETOOTH "  " LV_SYMBOL_USB);
        lv_obj_add_style(lbl_conn_status, &style_header_red, LV_STATE_DEFAULT);
        lv_obj_align_to(lbl_conn_status, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -15, 0);
    }
    else
    {

        lv_label_set_text(lbl_conn_status, LV_SYMBOL_BLUETOOTH "  " LV_SYMBOL_USB);
        lv_obj_add_style(lbl_conn_status, &style_header_green, LV_STATE_DEFAULT);
        lv_obj_align_to(lbl_conn_status, label_batt_level, LV_ALIGN_OUT_LEFT_MID, -15, 0);
    }
    /*
     */
    // lv_obj_set_text_color(lbl_no_ble, LV_PART_MAIN, LV_STATE_DEFAULT, lv_palette_main(LV_PALETTE_RED));

    // label_sym_ble = lv_label_create(parent);
    // lv_label_set_text(label_sym_ble, LV_SYMBOL_BLUETOOTH);
    // lv_obj_add_style(label_sym_ble, &style_header_black, LV_STATE_DEFAULT);
    // lv_obj_align_to(label_sym_ble, label_batt_level_val, LV_ALIGN_OUT_LEFT_MID, -5, 1);
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

        /*Create a container with ROW flex direction*/
    lv_obj_t *cont_row = lv_obj_create(parent);
    lv_obj_set_size(cont_row, 480, 78);
    lv_obj_set_pos(cont_row,10,225);
    lv_obj_set_flex_flow(cont_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(cont_row, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont_row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(cont_row, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *obj_hr_card = lv_obj_create(cont_row);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_size(obj_hr_card, 100, LV_PCT(100));
    lv_obj_set_style_bg_color(obj_hr_card, lv_palette_darken(LV_PALETTE_ORANGE, 4), LV_PART_MAIN);
    lv_obj_clear_flag(obj_hr_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj_hr_card, 0, LV_PART_MAIN);

    lv_obj_t *obj_spo2_card = lv_obj_create(cont_row);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_size(obj_spo2_card, 100, LV_PCT(100));
    lv_obj_set_style_bg_color(obj_spo2_card, lv_palette_darken(LV_PALETTE_BLUE, 4), LV_PART_MAIN);
    lv_obj_clear_flag(obj_spo2_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj_spo2_card, 0, LV_PART_MAIN);

    lv_obj_t *obj_rr_card = lv_obj_create(cont_row);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_size(obj_rr_card, 110, LV_PCT(100));
    lv_obj_set_style_bg_color(obj_rr_card, lv_palette_darken(LV_PALETTE_GREEN, 4), LV_PART_MAIN);
    lv_obj_clear_flag(obj_rr_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj_rr_card, 0, LV_PART_MAIN);

    lv_obj_t *obj_temp_card = lv_obj_create(cont_row);
    // lv_obj_add_style(obj_hr_card, &style, 0);
    lv_obj_set_size(obj_temp_card,120, LV_PCT(100));
    lv_obj_set_style_bg_color(obj_temp_card, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_clear_flag(obj_temp_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj_temp_card, 0, LV_PART_MAIN);

    // HR Number label
    label_hr = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr, "---");
    lv_obj_center(label_hr);
    lv_obj_add_style(label_hr, &style_number_medium, LV_STATE_DEFAULT);

    // HR Title label
    lv_obj_t *label_hr_title = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_title, "HR");
    lv_obj_align_to(label_hr_title, label_hr, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_hr_title, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    lv_obj_t *label_hr_sub = lv_label_create(obj_hr_card);
    lv_label_set_text(label_hr_sub, "bpm");
    lv_obj_align_to(label_hr_sub, label_hr, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_style(label_hr_sub, &style_sub, LV_STATE_DEFAULT);

    // HR BPM Subscript label
    // lv_obj_t *label_hr_status = lv_label_create(parent);
    // lv_label_set_text(label_hr_status, "ON");
    // lv_obj_align_to(label_hr_status, label_hr_sub, LV_ALIGN_BOTTOM_MID, 0, 17);
    // lv_obj_add_style(label_hr_status, &style_sub, LV_STATE_DEFAULT);

    // SPO2 Number label
    label_spo2 = lv_label_create(obj_spo2_card);
    lv_label_set_text(label_spo2, "---");
    //lv_obj_align_to(label_spo2, label_hr, LV_ALIGN_OUT_RIGHT_TOP, 60, 0);
    lv_obj_center(label_spo2);
    lv_obj_add_style(label_spo2, &style_number_medium, LV_STATE_DEFAULT);

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

    /*
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
    */

    // RR Number label
    label_rr = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr, "---");
    lv_obj_center(label_rr);
    lv_obj_add_style(label_rr, &style_number_medium, LV_STATE_DEFAULT);

    // RR Title label
    lv_obj_t *label_rr_title = lv_label_create(obj_rr_card);
    lv_label_set_text(label_rr_title, "Resp Rate");
    lv_obj_align_to(label_rr_title, label_rr, LV_ALIGN_TOP_MID, -5, -15);
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
    lv_obj_add_style(label_temp_f, &style_number_medium, LV_STATE_DEFAULT);

    label_temp_c = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_c, "---");
    lv_obj_align_to(label_temp_c, label_temp_f, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_add_style(label_temp_c, &style_sub, LV_STATE_DEFAULT);

    // Temp label
    lv_obj_t *label_temp_title = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_title, "Temp");
    lv_obj_align_to(label_temp_title, label_temp_f, LV_ALIGN_TOP_MID, 0, -15);
    lv_obj_add_style(label_temp_title, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_temp_sub = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_sub, "°F");
    lv_obj_align_to(label_temp_sub, label_temp_f, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_style(label_temp_sub, &style_sub, LV_STATE_DEFAULT);

    // Temp Sub deg C label
    lv_obj_t *label_temp_sub_c = lv_label_create(obj_temp_card);
    lv_label_set_text(label_temp_sub_c, "°C");
    lv_obj_align_to(label_temp_sub_c, label_temp_c, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_style(label_temp_sub_c, &style_sub, LV_STATE_DEFAULT);

    lv_obj_t *label_menu = lv_label_create(parent);
    lv_label_set_text(label_menu, "Press side wheel UP/DOWN for other charts");
    lv_obj_align(label_menu, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void down_key_event_handler()
{
    printk("Down key pressed\n");
    // hpi_load_screen();
}

void hpi_load_screen(int m_screen, enum scroll_dir m_scroll_dir)
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
    case SCR_HOME:
        draw_scr_home(SCROLL_DOWN);
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
        printf("Left at %d\n", hpi_disp_get_curr_screen());

        if ((hpi_disp_get_curr_screen() + 1) == SCR_LIST_END)
        {
            printk("End of list\n");
            hpi_load_screen(SCR_LIST_START + 1, SCROLL_LEFT);
            // return;
        }
        else
        {
            printk("Loading screen %d\n", hpi_disp_get_curr_screen() + 1);
            hpi_load_screen(hpi_disp_get_curr_screen() + 1, SCROLL_LEFT);
        }
    }

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
    {
        lv_indev_wait_release(lv_indev_get_act());
        printf("Right at %d\n", hpi_disp_get_curr_screen());
        if ((hpi_disp_get_curr_screen() - 1) == SCR_LIST_START)
        {
            printk("Start of list\n");
            hpi_load_screen(SCR_LIST_END, SCROLL_RIGHT);
            // return;
        }
        else
        {

            printk("Loading screen %d\n", curr_screen - 1);
            hpi_load_screen(hpi_disp_get_curr_screen() - 1, SCROLL_RIGHT);
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

void hpi_disp_set_curr_screen(int screen)
{
    k_mutex_lock(&mutex_curr_screen, K_FOREVER);
    curr_screen = screen;
    k_mutex_unlock(&mutex_curr_screen);
}

int hpi_disp_get_curr_screen(void)
{
    k_mutex_lock(&mutex_curr_screen, K_FOREVER);
    int screen = curr_screen;
    k_mutex_unlock(&mutex_curr_screen);
    return screen;
}

void display_screens_thread(void)
{
    struct hpi_ecg_bioz_sensor_data_t ecg_bioz_sensor_sample;
    struct hpi_ppg_sensor_data_t ppg_sensor_sample;

    struct hpi_sensor_data_point_t sensor_all_data_point;

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

    LOG_INF("Display screens inited");

    // draw_scr_ecg(SCROLL_DOWN);
    // draw_scr_resp(SCROLL_DOWN);
    // draw_scr_ppg(SCROLL_DOWN);

    // draw_scr_welcome();
    if (m_op_mode == OP_MODE_BASIC)
    {
        hpi_load_screen(SCR_HOME, SCROLL_DOWN);
    }
    else
    {
        hpi_load_screen(SCR_PPG, SCROLL_DOWN);
        //draw_scr_welcome();
    }

    while (1)
    {

        /*if (k_msgq_get(&q_plot_ecg_bioz, &ecg_bioz_sensor_sample, K_NO_WAIT) == 0)
        {
            if (hpi_disp_get_curr_screen() == SCR_ECG)
            {
                hpi_ecg_disp_draw_plot_ecg(ecg_bioz_sensor_sample.ecg_samples, ecg_bioz_sensor_sample.ecg_num_samples, ecg_bioz_sensor_sample.ecg_lead_off);
            }
            else if (hpi_disp_get_curr_screen() == SCR_RESP)
            {
                hpi_resp_disp_draw_plot_resp(ecg_bioz_sensor_sample.bioz_samples, ecg_bioz_sensor_sample.bioz_num_samples, ecg_bioz_sensor_sample.bioz_lead_off);
            }
        }

        if (k_msgq_get(&q_plot_ppg, &ppg_sensor_sample, K_NO_WAIT) == 0)
        {
            if (hpi_disp_get_curr_screen() == SCR_PPG)
            {
                hpi_ppg_disp_draw_plot_ppg(ppg_sensor_sample.ppg_red_sample, ppg_sensor_sample.ppg_ir_sample, ppg_sensor_sample.ppg_lead_off);
            }
        }*/

        if(k_msgq_get(&q_hpi_plot_all_sample, &sensor_all_data_point, K_NO_WAIT) == 0)
        {
            //LOG_DBG("All sens");
            printk("PP ");
            hpi_ppg_disp_draw_plot_ppg(sensor_all_data_point.ppg_sample_red, sensor_all_data_point.ppg_sample_ir, 0);
            /*if (hpi_disp_get_curr_screen() == SCR_HOME)
            {
                hpi_scr_home_update_hr(sensor_all_data_point.hr);
                hpi_scr_home_update_pr(sensor_all_data_point.pr);
                hpi_scr_home_update_spo2(sensor_all_data_point.spo2);
                hpi_scr_home_update_rr(sensor_all_data_point.rr);
                hpi_scr_home_update_temp(sensor_all_data_point.temp_f, sensor_all_data_point.temp_c);
                
            }*/
        }
        
        /*
        if (k_uptime_get_32() - last_batt_refresh > HPI_DISP_BATT_REFR_INT)
        {
            hpi_disp_update_batt_level(m_disp_batt_level);
            last_batt_refresh = k_uptime_get_32();
        }
        */

        
        /*
        if (k_uptime_get_32() - last_temp_refresh > HPI_DISP_TEMP_REFR_INT)
        {
            hpi_disp_update_temp(m_disp_temp_f, m_disp_temp_c);
            last_temp_refresh = k_uptime_get_32();
        }

        /*
        if (k_uptime_get_32() - last_hr_refresh > HPI_DISP_HR_REFR_INT)
        {
            hpi_scr_update_hr(m_disp_hr);
            last_hr_refresh = k_uptime_get_32();
        }

        if (k_uptime_get_32() - last_spo2_refresh > HPI_DISP_SPO2_REFR_INT)
        {
            hpi_scr_update_spo2(m_disp_spo2);
            last_spo2_refresh = k_uptime_get_32();
        }*/

        if (k_uptime_get_32() - last_rr_refresh > HPI_DISP_RR_REFR_INT)
        {
            hpi_scr_update_rr(m_disp_rr);
            last_rr_refresh = k_uptime_get_32();
        }

        // Check for key presses
        if (k_sem_take(&sem_down_key_pressed, K_NO_WAIT) == 0)
        {
            hpi_disp_change_event(HPI_SCR_EVENT_DOWN);
        }

        if (k_sem_take(&sem_up_key_pressed, K_NO_WAIT) == 0)
        {
            hpi_disp_change_event(HPI_SCR_EVENT_UP);
        }

        lv_task_handler();

        if (m_op_mode == OP_MODE_BASIC)
        {
            //k_sleep(K_MSEC(100));
        }
        else
        {
            //k_sleep(K_MSEC(30));
        }
        //k_msleep(30);
        k_sleep(K_MSEC(100));

    }
}

static void disp_batt_status_listener(const struct zbus_channel *chan)
{
    const struct hpi_batt_status_t *batt_s = zbus_chan_const_msg(chan);

    // LOG_DBG("Ch Batt: %d, Charge: %d", batt_s->batt_level, batt_s->batt_charging);
    m_disp_batt_level = batt_s->batt_level;
    m_disp_batt_charging = batt_s->batt_charging;
}
ZBUS_LISTENER_DEFINE(disp_batt_lis, disp_batt_status_listener);

static void disp_hr_listener(const struct zbus_channel *chan)
{
    const struct hpi_hr_t *hpi_hr = zbus_chan_const_msg(chan);
    m_disp_hr = hpi_hr->hr;
}
ZBUS_LISTENER_DEFINE(disp_hr_lis, disp_hr_listener);

static void disp_temp_listener(const struct zbus_channel *chan)
{
    const struct hpi_temp_t *hpi_temp = zbus_chan_const_msg(chan);
    m_disp_temp_f = hpi_temp->temp_f;
    m_disp_temp_c = hpi_temp->temp_c;
}
ZBUS_LISTENER_DEFINE(disp_temp_lis, disp_temp_listener);

static void disp_spo2_listener(const struct zbus_channel *chan)
{
    const struct hpi_spo2_t *hpi_spo2 = zbus_chan_const_msg(chan);
    m_disp_spo2 = hpi_spo2->spo2;
    // hpi_scr_update_spo2(hpi_spo2->spo2);
}
ZBUS_LISTENER_DEFINE(disp_spo2_lis, disp_spo2_listener);

static void disp_resp_rate_listener(const struct zbus_channel *chan)
{
    const struct hpi_resp_rate_t *hpi_resp_rate = zbus_chan_const_msg(chan);
    m_disp_rr = hpi_resp_rate->resp_rate;
    // hpi_scr_update_rr(hpi_resp_rate->resp_rate);
}
ZBUS_LISTENER_DEFINE(disp_resp_rate_lis, disp_resp_rate_listener);

#define DISPLAY_SCREENS_THREAD_STACKSIZE 5000
#define DISPLAY_SCREENS_THREAD_PRIORITY 5

K_THREAD_DEFINE(display_screens_thread_id, DISPLAY_SCREENS_THREAD_STACKSIZE, display_screens_thread, NULL, NULL, NULL, DISPLAY_SCREENS_THREAD_PRIORITY, 0, 0);
