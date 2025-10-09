/*
 * Respiration Rate Detail Screen Header
 */

#pragma once

#include "display_module.h"

/**
 * @brief Create and display the RR detail screen
 * 
 * @param m_scroll_dir Animation direction for screen transition
 */
void draw_scr_rr(enum scroll_dir m_scroll_dir);

/**
 * @brief Update RR screen with current values
 * 
 * Should be called periodically (e.g., every 4 seconds) to refresh display
 */
void update_scr_rr(void);

/**
 * @brief Clean up RR screen resources
 */
void scr_rr_delete(void);
