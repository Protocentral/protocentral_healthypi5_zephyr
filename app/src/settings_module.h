#ifndef settings_module_h
#define settings_module_h

#include "hpi_common_types.h"

// Settings persistence functions
void settings_save_hr_source(enum hpi_hr_source source);
enum hpi_hr_source settings_load_hr_source(void);

#endif