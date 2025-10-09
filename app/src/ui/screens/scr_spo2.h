/*
 * SpO2 Detail Screen Header
 */

#pragma once

#include "display_module.h"

/**
 * @brief Create and display the SpO2 detail screen
 * 
 * @param m_scroll_dir Animation direction for screen transition
 */
void draw_scr_spo2(enum scroll_dir m_scroll_dir);

/**
 * @brief Update SpO2 screen with current values
 * 
 * Should be called periodically (e.g., every 1 second) to refresh display
 */
void update_scr_spo2(void);

/**
 * @brief Clean up SpO2 screen resources
 */
void scr_spo2_delete(void);
