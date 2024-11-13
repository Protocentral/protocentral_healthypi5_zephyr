#ifndef fs_module_h
#define fs_module_h

#include "sampling_module.h"

void fs_module_init(void);
void init_settings(void);
//static int lsdir(const char *path);
void set_current_session_log_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year);
void write_header_to_new_file();

#endif