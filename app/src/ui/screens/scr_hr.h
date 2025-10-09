/*
 * Heart Rate Detail Screen Header
 */

#pragma once

#include "display_module.h"

/**
 * @brief Create and display the HR detail screen
 * 
 * @param m_scroll_dir Animation direction for screen transition
 */
void draw_scr_hr(enum scroll_dir m_scroll_dir);

/**
 * @brief Update HR screen with current values
 * 
 * Should be called periodically (e.g., every 1 second) to refresh display
 */
void update_scr_hr(void);

/**
 * @brief Clean up HR screen resources
 */
void scr_hr_delete(void);
