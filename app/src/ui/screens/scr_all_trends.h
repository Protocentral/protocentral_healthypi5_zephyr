/*
 * All Trends Overview Screen - Header
 */

#ifndef SCR_ALL_TRENDS_H
#define SCR_ALL_TRENDS_H

#include "display_module.h"

/**
 * @brief Draw the all trends screen showing all 4 vitals in column layout
 * @param m_scroll_dir Scroll direction for screen transition animation
 */
void draw_scr_all_trends(enum scroll_dir m_scroll_dir);

/**
 * @brief Update all vital signs values and mini charts on the all trends screen
 */
void update_scr_all_trends(void);

/**
 * @brief Delete the all trends screen and free resources
 */
void scr_all_trends_delete(void);

#endif // SCR_ALL_TRENDS_H
