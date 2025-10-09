/*
 * Temperature Detail Screen Header
 */

#pragma once

#include "display_module.h"

/**
 * @brief Create and display the Temperature detail screen
 * 
 * @param m_scroll_dir Animation direction for screen transition
 */
void draw_scr_temp(enum scroll_dir m_scroll_dir);

/**
 * @brief Update Temperature screen with current values
 * 
 * Should be called periodically (e.g., every 1 second) to refresh display
 */
void update_scr_temp(void);

/**
 * @brief Clean up Temperature screen resources
 */
void scr_temp_delete(void);
