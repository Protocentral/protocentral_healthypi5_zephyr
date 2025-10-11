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
 * Vital Signs Statistics Module
 * 
 * Tracks historical data for vital signs to provide:
 * - Min/Max values over time windows
 * - Trends and averages
 * - Time-since-update tracking
 * 
 * Uses circular buffers to maintain 60-second history for each vital sign.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// History window size (60 seconds at 1 Hz update rate)
#define VITAL_STATS_WINDOW_SIZE 60

/**
 * @brief Initialize the vital stats module
 * 
 * Must be called once at system startup before any other vital_stats_* functions.
 */
void vital_stats_init(void);

/**
 * @brief Update heart rate history
 * 
 * @param hr Current heart rate in bpm (0 = invalid/no data)
 */
void vital_stats_update_hr(uint16_t hr);

/**
 * @brief Update SpO2 history
 * 
 * @param spo2 Current SpO2 percentage (0 = invalid/no data)
 */
void vital_stats_update_spo2(uint8_t spo2);

/**
 * @brief Update respiration rate history
 * 
 * @param rr Current respiration rate in breaths/min (0 = invalid/no data)
 */
void vital_stats_update_rr(uint8_t rr);

/**
 * @brief Update temperature history
 * 
 * @param temp_f Current temperature in Fahrenheit
 */
void vital_stats_update_temp(float temp_f);

/**
 * @brief Get minimum heart rate over the last 60 seconds
 * 
 * @return Minimum HR in bpm, or 0 if no valid data
 */
uint16_t vital_stats_get_hr_min(void);

/**
 * @brief Get maximum heart rate over the last 60 seconds
 * 
 * @return Maximum HR in bpm, or 0 if no valid data
 */
uint16_t vital_stats_get_hr_max(void);

/**
 * @brief Get average heart rate over the last 60 seconds
 * 
 * @return Average HR in bpm, or 0 if no valid data
 */
uint16_t vital_stats_get_hr_avg(void);

/**
 * @brief Get time since last HR update
 * 
 * @return Seconds since last update, or 0xFFFFFFFF if never updated
 */
uint32_t vital_stats_get_hr_time_since_update(void);

/**
 * @brief Get minimum SpO2 over the last 60 seconds
 * 
 * @return Minimum SpO2 percentage, or 0 if no valid data
 */
uint8_t vital_stats_get_spo2_min(void);

/**
 * @brief Get maximum SpO2 over the last 60 seconds
 * 
 * @return Maximum SpO2 percentage, or 0 if no valid data
 */
uint8_t vital_stats_get_spo2_max(void);

/**
 * @brief Get average SpO2 over the last 60 seconds
 * 
 * @return Average SpO2 percentage, or 0 if no valid data
 */
uint8_t vital_stats_get_spo2_avg(void);

/**
 * @brief Get time since last SpO2 update
 * 
 * @return Seconds since last update, or 0xFFFFFFFF if never updated
 */
uint32_t vital_stats_get_spo2_time_since_update(void);

/**
 * @brief Get minimum respiration rate over the last 60 seconds
 * 
 * @return Minimum RR in breaths/min, or 0 if no valid data
 */
uint8_t vital_stats_get_rr_min(void);

/**
 * @brief Get maximum respiration rate over the last 60 seconds
 * 
 * @return Maximum RR in breaths/min, or 0 if no valid data
 */
uint8_t vital_stats_get_rr_max(void);

/**
 * @brief Get average respiration rate over the last 60 seconds
 * 
 * @return Average RR in breaths/min, or 0 if no valid data
 */
uint8_t vital_stats_get_rr_avg(void);

/**
 * @brief Get time since last RR update
 * 
 * @return Seconds since last update, or 0xFFFFFFFF if never updated
 */
uint32_t vital_stats_get_rr_time_since_update(void);

/**
 * @brief Get minimum temperature over the last 60 seconds
 * 
 * @return Minimum temperature in °F
 */
float vital_stats_get_temp_min(void);

/**
 * @brief Get maximum temperature over the last 60 seconds
 * 
 * @return Maximum temperature in °F
 */
float vital_stats_get_temp_max(void);

/**
 * @brief Get average temperature over the last 60 seconds
 * 
 * @return Average temperature in °F
 */
float vital_stats_get_temp_avg(void);

/**
 * @brief Get time since last temperature update
 * 
 * @return Seconds since last update, or 0xFFFFFFFF if never updated
 */
uint32_t vital_stats_get_temp_time_since_update(void);

/**
 * @brief Get HR trend indicator
 * 
 * Compares current HR to 10-second-ago average.
 * 
 * @return 1 = increasing, 0 = stable, -1 = decreasing
 */
int8_t vital_stats_get_hr_trend(void);

/**
 * @brief Get system uptime for display
 * 
 * @param hours Output: hours since boot
 * @param minutes Output: minutes since boot (0-59)
 * @param seconds Output: seconds since boot (0-59)
 */
void vital_stats_get_uptime(uint32_t *hours, uint32_t *minutes, uint32_t *seconds);
