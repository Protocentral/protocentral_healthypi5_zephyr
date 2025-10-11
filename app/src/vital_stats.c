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
 * Vital Signs Statistics Module Implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>

#include "vital_stats.h"

LOG_MODULE_REGISTER(vital_stats, LOG_LEVEL_DBG);

// Heart Rate history
static struct {
    uint16_t values[VITAL_STATS_WINDOW_SIZE];
    uint8_t  index;
    uint16_t min;
    uint16_t max;
    uint32_t sum;
    uint16_t valid_count;
    uint32_t last_update_time;
    bool     initialized;
} hr_history;

// SpO2 history
static struct {
    uint8_t  values[VITAL_STATS_WINDOW_SIZE];
    uint8_t  index;
    uint8_t  min;
    uint8_t  max;
    uint32_t sum;
    uint16_t valid_count;
    uint32_t last_update_time;
    bool     initialized;
} spo2_history;

// Respiration Rate history
static struct {
    uint8_t  values[VITAL_STATS_WINDOW_SIZE];
    uint8_t  index;
    uint8_t  min;
    uint8_t  max;
    uint32_t sum;
    uint16_t valid_count;
    uint32_t last_update_time;
    bool     initialized;
} rr_history;

// Temperature history
static struct {
    float    values[VITAL_STATS_WINDOW_SIZE];
    uint8_t  index;
    float    min;
    float    max;
    float    sum;
    uint16_t valid_count;
    uint32_t last_update_time;
    bool     initialized;
} temp_history;

void vital_stats_init(void)
{
    memset(&hr_history, 0, sizeof(hr_history));
    memset(&spo2_history, 0, sizeof(spo2_history));
    memset(&rr_history, 0, sizeof(rr_history));
    memset(&temp_history, 0, sizeof(temp_history));
    
    // Initialize min/max to extreme values
    hr_history.min = 0xFFFF;
    hr_history.max = 0;
    spo2_history.min = 0xFF;
    spo2_history.max = 0;
    rr_history.min = 0xFF;
    rr_history.max = 0;
    temp_history.min = 999.0f;
    temp_history.max = -999.0f;
    
    LOG_INF("Vital stats module initialized");
}

void vital_stats_update_hr(uint16_t hr)
{
    if (hr == 0) {
        return;  // Invalid data, skip
    }
    
    uint32_t now = k_uptime_get_32();
    
    //LOG_DBG("HR update: new=%d, valid_count=%d, index=%d", hr, hr_history.valid_count, hr_history.index);
    
    // Remove old value from sum if buffer is full
    if (hr_history.valid_count == VITAL_STATS_WINDOW_SIZE) {
        hr_history.sum -= hr_history.values[hr_history.index];
    } else {
        hr_history.valid_count++;
    }
    
    // Add new value
    hr_history.values[hr_history.index] = hr;
    hr_history.sum += hr;
    hr_history.last_update_time = now;
    hr_history.initialized = true;
    
    // Update circular buffer index
    hr_history.index = (hr_history.index + 1) % VITAL_STATS_WINDOW_SIZE;
    
    // Recalculate min/max over entire window
    hr_history.min = 0xFFFF;
    hr_history.max = 0;
    for (int i = 0; i < hr_history.valid_count; i++) {
        uint16_t val = hr_history.values[i];
        if (val > 0 && val < hr_history.min) hr_history.min = val;
        if (val > hr_history.max) hr_history.max = val;
    }
    
    //LOG_DBG("HR stats calculated: min=%d, max=%d, avg=%d", hr_history.min, hr_history.max, 
    //        (hr_history.valid_count > 0) ? (hr_history.sum / hr_history.valid_count) : 0);
}

void vital_stats_update_spo2(uint8_t spo2)
{
    if (spo2 == 0) {
        return;  // Invalid data, skip
    }
    
    uint32_t now = k_uptime_get_32();
    
    // Remove old value from sum if buffer is full
    if (spo2_history.valid_count == VITAL_STATS_WINDOW_SIZE) {
        spo2_history.sum -= spo2_history.values[spo2_history.index];
    } else {
        spo2_history.valid_count++;
    }
    
    // Add new value
    spo2_history.values[spo2_history.index] = spo2;
    spo2_history.sum += spo2;
    spo2_history.last_update_time = now;
    spo2_history.initialized = true;
    
    // Update circular buffer index
    spo2_history.index = (spo2_history.index + 1) % VITAL_STATS_WINDOW_SIZE;
    
    // Recalculate min/max over entire window
    spo2_history.min = 0xFF;
    spo2_history.max = 0;
    for (int i = 0; i < spo2_history.valid_count; i++) {
        uint8_t val = spo2_history.values[i];
        if (val > 0 && val < spo2_history.min) spo2_history.min = val;
        if (val > spo2_history.max) spo2_history.max = val;
    }
}

void vital_stats_update_rr(uint8_t rr)
{
    if (rr == 0) {
        return;  // Invalid data, skip (though 0 could indicate apnea - handle separately if needed)
    }
    
    uint32_t now = k_uptime_get_32();
    
    // Remove old value from sum if buffer is full
    if (rr_history.valid_count == VITAL_STATS_WINDOW_SIZE) {
        rr_history.sum -= rr_history.values[rr_history.index];
    } else {
        rr_history.valid_count++;
    }
    
    // Add new value
    rr_history.values[rr_history.index] = rr;
    rr_history.sum += rr;
    rr_history.last_update_time = now;
    rr_history.initialized = true;
    
    // Update circular buffer index
    rr_history.index = (rr_history.index + 1) % VITAL_STATS_WINDOW_SIZE;
    
    // Recalculate min/max over entire window
    rr_history.min = 0xFF;
    rr_history.max = 0;
    for (int i = 0; i < rr_history.valid_count; i++) {
        uint8_t val = rr_history.values[i];
        if (val > 0 && val < rr_history.min) rr_history.min = val;
        if (val > rr_history.max) rr_history.max = val;
    }
}

void vital_stats_update_temp(float temp_f)
{
    if (temp_f < -50.0f || temp_f > 150.0f) {
        return;  // Invalid data, skip
    }
    
    uint32_t now = k_uptime_get_32();
    
    // Remove old value from sum if buffer is full
    if (temp_history.valid_count == VITAL_STATS_WINDOW_SIZE) {
        temp_history.sum -= temp_history.values[temp_history.index];
    } else {
        temp_history.valid_count++;
    }
    
    // Add new value
    temp_history.values[temp_history.index] = temp_f;
    temp_history.sum += temp_f;
    temp_history.last_update_time = now;
    temp_history.initialized = true;
    
    // Update circular buffer index
    temp_history.index = (temp_history.index + 1) % VITAL_STATS_WINDOW_SIZE;
    
    // Recalculate min/max over entire window
    temp_history.min = 999.0f;
    temp_history.max = -999.0f;
    for (int i = 0; i < temp_history.valid_count; i++) {
        float val = temp_history.values[i];
        if (val < temp_history.min) temp_history.min = val;
        if (val > temp_history.max) temp_history.max = val;
    }
}

uint16_t vital_stats_get_hr_min(void)
{
    if (!hr_history.initialized || hr_history.valid_count == 0) return 0;
    return (hr_history.min == 0xFFFF) ? 0 : hr_history.min;
}

uint16_t vital_stats_get_hr_max(void)
{
    if (!hr_history.initialized || hr_history.valid_count == 0) return 0;
    return hr_history.max;
}

uint16_t vital_stats_get_hr_avg(void)
{
    if (!hr_history.initialized || hr_history.valid_count == 0) return 0;
    return (uint16_t)(hr_history.sum / hr_history.valid_count);
}

uint32_t vital_stats_get_hr_time_since_update(void)
{
    if (!hr_history.initialized) return 0xFFFFFFFF;
    return (k_uptime_get_32() - hr_history.last_update_time) / 1000;  // Convert to seconds
}

uint8_t vital_stats_get_spo2_min(void)
{
    if (!spo2_history.initialized || spo2_history.valid_count == 0) return 0;
    return (spo2_history.min == 0xFF) ? 0 : spo2_history.min;
}

uint8_t vital_stats_get_spo2_max(void)
{
    if (!spo2_history.initialized || spo2_history.valid_count == 0) return 0;
    return spo2_history.max;
}

uint8_t vital_stats_get_spo2_avg(void)
{
    if (!spo2_history.initialized || spo2_history.valid_count == 0) return 0;
    return (uint8_t)(spo2_history.sum / spo2_history.valid_count);
}

uint32_t vital_stats_get_spo2_time_since_update(void)
{
    if (!spo2_history.initialized) return 0xFFFFFFFF;
    return (k_uptime_get_32() - spo2_history.last_update_time) / 1000;
}

uint8_t vital_stats_get_rr_min(void)
{
    if (!rr_history.initialized || rr_history.valid_count == 0) return 0;
    return (rr_history.min == 0xFF) ? 0 : rr_history.min;
}

uint8_t vital_stats_get_rr_max(void)
{
    if (!rr_history.initialized || rr_history.valid_count == 0) return 0;
    return rr_history.max;
}

uint8_t vital_stats_get_rr_avg(void)
{
    if (!rr_history.initialized || rr_history.valid_count == 0) return 0;
    return (uint8_t)(rr_history.sum / rr_history.valid_count);
}

uint32_t vital_stats_get_rr_time_since_update(void)
{
    if (!rr_history.initialized) return 0xFFFFFFFF;
    return (k_uptime_get_32() - rr_history.last_update_time) / 1000;
}

float vital_stats_get_temp_min(void)
{
    if (!temp_history.initialized || temp_history.valid_count == 0) return 0.0f;
    return (temp_history.min > 500.0f) ? 0.0f : temp_history.min;
}

float vital_stats_get_temp_max(void)
{
    if (!temp_history.initialized || temp_history.valid_count == 0) return 0.0f;
    return (temp_history.max < -500.0f) ? 0.0f : temp_history.max;
}

float vital_stats_get_temp_avg(void)
{
    if (!temp_history.initialized || temp_history.valid_count == 0) return 0.0f;
    return temp_history.sum / temp_history.valid_count;
}

uint32_t vital_stats_get_temp_time_since_update(void)
{
    if (!temp_history.initialized) return 0xFFFFFFFF;
    return (k_uptime_get_32() - temp_history.last_update_time) / 1000;
}

int8_t vital_stats_get_hr_trend(void)
{
    if (!hr_history.initialized || hr_history.valid_count < 15) {
        return 0;  // Not enough data for trend
    }
    
    // Compare most recent value to 10-second-ago average
    // Assuming 1 Hz update rate: 10 seconds ago = 10 samples back
    uint32_t recent_sum = 0;
    uint8_t recent_count = 0;
    
    // Average of last 3 samples (most recent)
    for (int i = 0; i < 3 && i < hr_history.valid_count; i++) {
        int idx = (hr_history.index - 1 - i + VITAL_STATS_WINDOW_SIZE) % VITAL_STATS_WINDOW_SIZE;
        if (hr_history.values[idx] > 0) {
            recent_sum += hr_history.values[idx];
            recent_count++;
        }
    }
    
    // Average of samples 10-13 seconds ago
    uint32_t past_sum = 0;
    uint8_t past_count = 0;
    for (int i = 10; i < 13 && i < hr_history.valid_count; i++) {
        int idx = (hr_history.index - 1 - i + VITAL_STATS_WINDOW_SIZE) % VITAL_STATS_WINDOW_SIZE;
        if (hr_history.values[idx] > 0) {
            past_sum += hr_history.values[idx];
            past_count++;
        }
    }
    
    if (recent_count == 0 || past_count == 0) {
        return 0;  // Not enough valid data
    }
    
    uint16_t recent_avg = recent_sum / recent_count;
    uint16_t past_avg = past_sum / past_count;
    
    // Threshold of 3 bpm to avoid noise
    if (recent_avg > past_avg + 3) {
        return 1;  // Increasing
    } else if (recent_avg < past_avg - 3) {
        return -1;  // Decreasing
    } else {
        return 0;  // Stable
    }
}

void vital_stats_get_uptime(uint32_t *hours, uint32_t *minutes, uint32_t *seconds)
{
    uint64_t uptime_ms = k_uptime_get();
    uint32_t total_seconds = uptime_ms / 1000;
    
    *seconds = total_seconds % 60;
    *minutes = (total_seconds / 60) % 60;
    *hours = total_seconds / 3600;
}
