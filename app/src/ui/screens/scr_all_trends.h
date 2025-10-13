/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


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
