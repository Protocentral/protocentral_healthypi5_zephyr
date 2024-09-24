#define pragma once
#include <lvgl.h>

void draw_scr_home(void);
void draw_scr_menu(char* session_names);

<<<<<<< Updated upstream
void draw_plotppg(float data_ppg);
void draw_plotresp(float data_resp);
void draw_plotECG(float data_ecg);
=======
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
#define SAMPLE_RATE 128
#define ECG_DISP_WINDOW_SIZE 512 // SAMPLE_RATE * 4

#define TEMP_SAMPLING_INTERVAL_COUNT 125 // Number of counts of SAMPLING_INTERVAL_MS to wait before sampling temperature

// Home Screen functions
void draw_scr_home(enum scroll_dir m_scroll_dir);
void scr_home_plot_ecg(float plot_data);
void scr_home_plot_ppg(float plot_data);

void hpi_scr_home_update_temp(int temp);

// Display helper functions
void hpi_show_screen(lv_obj_t *parent, enum scroll_dir m_scroll_dir);
void hpi_load_screen(enum hpi_disp_screens m_screen, enum scroll_dir m_scroll_dir);
void hpi_disp_update_batt_level(int batt_level);
>>>>>>> Stashed changes

void draw_scr_chart_single(uint8_t m_data_type);
void draw_chart_single_scr(uint8_t m_data_type, lv_obj_t *scr_obj);

void hpi_disp_draw_plot(float plot_data);
void hpi_disp_update_batt_level(int batt_level);
void hpi_disp_update_temp(int temp);

//void draw_scr_chart_resp(void);
void draw_header(lv_obj_t *parent, bool showFWVersion);
void draw_footer(lv_obj_t *parent);
void hpi_disp_switch_screen(void);