#define pragma once

#include <lvgl.h>

enum scroll_dir
{
    SCROLL_UP,
    SCROLL_DOWN,
    SCROLL_LEFT,
    SCROLL_RIGHT,
};

enum hpi_disp_screens
{
    SCR_LIST_START,

    SCR_HOME,
    SCR_ECG,
    SCR_PPG,
    SCR_RESP,

    SCR_LIST_END
};


#define SCREEN_TRANS_TIME 100
#define SAMPLE_RATE 125
#define DISP_WINDOW_SIZE 625 // SAMPLE_RATE * 4

#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

void draw_scr_home(enum scroll_dir m_scroll_dir);

// Display helper functions
void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir);
void hpi_load_screen(enum hpi_disp_screens m_screen, enum scroll_dir m_scroll_dir);

void draw_scr_menu(char* session_names);

void draw_plotppg(float data_ppg);
void draw_plotresp(float data_resp);
void draw_plotECG(float data_ecg);

void draw_scr_chart_single(uint8_t m_data_type);
void draw_chart_single_scr(uint8_t m_data_type, lv_obj_t *scr_obj);

void scr_home_draw_plot_ecg(float plot_data);
void hpi_disp_update_batt_level(int batt_level);
void hpi_scr_home_update_temp(int temp);

//void draw_scr_chart_resp(void);
void draw_header(lv_obj_t *parent, bool showFWVersion);
void draw_footer(lv_obj_t *parent);
