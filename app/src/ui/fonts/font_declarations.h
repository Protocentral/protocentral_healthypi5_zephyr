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


/* Modern Google Fonts for HealthyPi Move - LVGL Font Declarations */
/* Generated fonts for AMOLED circular display optimization */

// Phase 1: Core System Fonts (Essential)
LV_FONT_DECLARE(inter_semibold_24);       // General UI text - Primary font (upgraded from regular to semibold)
LV_FONT_DECLARE(inter_semibold_18);       // Legacy 18px font - kept for compatibility but styles now enforce 24px minimum for readability
LV_FONT_DECLARE(inter_semibold_80_time);  // Large minimalist time display (80px, digits only)
LV_FONT_DECLARE(jetbrains_mono_regular_16); // Time display, sensor readings

// Phase 2: Additional Essential Sizes  
LV_FONT_DECLARE(inter_regular_16);        // Secondary size for specific contexts
LV_FONT_DECLARE(inter_semibold_24); // Large time display, main clock

