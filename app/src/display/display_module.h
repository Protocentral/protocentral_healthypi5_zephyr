#define pragma once

#include <lvgl.h>

enum scroll_dir
{
    SCROLL_UP,
    SCROLL_DOWN,
    SCROLL_LEFT,
    SCROLL_RIGHT,
};

enum hpi_scr_event
{
    HPI_SCR_EVENT_UP,
    HPI_SCR_EVENT_DOWN,
    HPI_SCR_EVENT_OK,
};

enum hpi_disp_screens
{
    SCR_LIST_START,

    //SCR_HOME,
    SCR_ECG,
    SCR_PPG,
    SCR_RESP,

    SCR_LIST_END
};

#define SCREEN_TRANS_TIME 100

#define SAMPLE_RATE 128

#define ECG_DISP_WINDOW_SIZE 512 // SAMPLE_RATE * 4
#define RESP_DISP_WINDOW_SIZE 512 // SAMPLE_RATE * 4
#define PPG_DISP_WINDOW_SIZE 256 // SAMPLE_RATE * 4

#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

// Home Screen functions
void draw_scr_home(enum scroll_dir m_scroll_dir);
void scr_home_plot_ecg(float plot_data);
void scr_home_plot_ppg(float plot_data);

void hpi_disp_update_temp(int32_t temp);

// Display helper functions
void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir);
void hpi_load_screen(enum hpi_disp_screens m_screen, enum scroll_dir m_scroll_dir);
void hpi_disp_update_batt_level(int batt_level);
void hpi_disp_change_event(enum hpi_scr_event);

void draw_scr_chart_single(uint8_t m_data_type);
void draw_chart_single_scr(uint8_t m_data_type, lv_obj_t *scr_obj);

// ECG Screen functions
void draw_scr_ecg(enum scroll_dir m_scroll_dir);
void hpi_ecg_disp_draw_plot_ecg(int32_t *data_ecg, int num_samples, bool ecg_lead_off);

// Resp Screen functions
void draw_scr_resp(enum scroll_dir m_scroll_dir);
void hpi_resp_disp_draw_plot_resp(int32_t *data_resp, int num_samples, bool resp_lead_off);

// PPG Screen functions
void draw_scr_ppg(enum scroll_dir m_scroll_dir);
void hpi_ppg_disp_draw_plot_ppg(int32_t data_ppg_red, int32_t data_ppg_ir, bool ppg_lead_off);
void hpi_scr_home_update_pr(int pr);

//void draw_scr_chart_resp(void);
void draw_header(lv_obj_t *parent, bool showFWVersion);
void draw_footer(lv_obj_t *parent);
