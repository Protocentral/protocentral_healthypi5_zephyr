#ifndef fs_module_h
#define fs_module_h

#include "sampling_module.h"

void fs_module_init(void);
void init_settings(void);
//void record_write_to_file(int ecg_ppg_counter, struct hpi_sensor_logging_data_t *current_session_log_points);
static int lsdir(const char *path);
static int littlefs_flash_erase(unsigned int id);
void set_current_session_log_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year);
void write_header_to_new_file();

#endif