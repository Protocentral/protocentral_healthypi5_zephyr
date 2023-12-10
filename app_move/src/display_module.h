#define pragma once

void draw_scr_home_menu(void);
void draw_scr_charts_all(void);
void draw_scr_chart_single(uint8_t m_data_type);
void draw_scr_menu(char* session_names);

void draw_plotppg(float data_ppg);
void draw_plotresp(float data_resp);
void draw_plotECG(float data_ecg);

void hpi_disp_update_batt_level(int batt_level);
void hpi_disp_update_temp(int temp);