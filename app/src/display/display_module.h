#define pragma once
#include <lvgl.h>

void draw_scr_home(void);
void draw_scr_menu(char* session_names);

void draw_plotppg(float data_ppg);
void draw_plotresp(float data_resp);
void draw_plotECG(float data_ecg);

void draw_scr_chart_single(uint8_t m_data_type);
void draw_chart_single_scr(uint8_t m_data_type, lv_obj_t *scr_obj);

void hpi_disp_draw_plot(float plot_data);
void hpi_disp_update_batt_level(int batt_level);
void hpi_disp_update_temp(int temp);

//void draw_scr_chart_resp(void);
void draw_header(lv_obj_t *parent, bool showFWVersion);
void draw_footer(lv_obj_t *parent);
void hpi_disp_switch_screen(void);