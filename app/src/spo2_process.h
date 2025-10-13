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


/*******************************************************************************
* Copyright (C) 2015 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/
#ifndef SPO2_ALGORITHM_H_
#define SPO2_ALGORITHM_H_

#include <stdint.h>
#include <stdbool.h>

// Phase 1 Optimization: Reduced buffer size for memory efficiency
// Original: 500 samples (4 seconds) = 4KB
// Optimized: 250 samples (2 seconds) = 2KB  
// Memory savings: 50% (2KB)
#define FreqS 125    //sampling frequency
#define BUFFER_SIZE (FreqS * 2)  // Changed from 4 to 2 seconds (500 -> 250)
#define MA4_SIZE 4 // DONOT CHANGE

// Quality metrics structure for Phase 1
typedef struct {
    uint16_t perfusion_ir;      // IR perfusion index × 100 (0-2000 = 0.0-20.0%)
    uint16_t perfusion_red;     // Red perfusion index × 100
    uint16_t signal_strength;   // RMS amplitude of AC component
    uint8_t confidence;         // Overall confidence score (0-100)
    bool valid;                 // Result is valid and reliable
    bool probe_off;             // Reliable probe-off detection flag (RAW, single reading)
    bool probe_off_filtered;    // Filtered probe-off (after consecutive count filtering)
    uint8_t probe_off_reason;   // Reason code for probe-off (see below)
} spo2_quality_metrics_t;

// Probe-off reason codes (for debugging/logging)
#define PROBE_OFF_NONE           0  // Probe is connected
#define PROBE_OFF_LOW_DC         1  // DC signal too weak
#define PROBE_OFF_SATURATED      2  // Signal saturated
#define PROBE_OFF_LOW_PI         3  // Perfusion index too low
#define PROBE_OFF_NO_PEAKS       4  // No cardiac pulses detected
#define PROBE_OFF_WEAK_AC        5  // AC amplitude too weak
#define PROBE_OFF_LOW_CONFIDENCE 6  // Overall confidence too low

// Probe-off detection state tracker
// This handles consecutive count filtering within the algorithm layer
typedef struct {
    bool probe_off_state;       // Current filtered probe-off state
    uint8_t consecutive_off;    // Count of consecutive probe-off detections
    uint8_t consecutive_on;     // Count of consecutive probe-on detections
    uint8_t threshold_off;      // Threshold for declaring probe-off
    uint8_t threshold_on;       // Threshold for declaring probe-on
} spo2_probe_state_t;

// Initialize probe state tracker (call once at startup)
void spo2_probe_state_init(spo2_probe_state_t *state, uint8_t threshold_off, uint8_t threshold_on);

// Update probe state with new detection result (call each SpO2 calculation)
// Returns true if state changed
bool spo2_probe_state_update(spo2_probe_state_t *state, bool probe_off_raw);

// Expose internal buffers to avoid duplication (Phase 1 optimization)
// CRITICAL FIX: Must be uint32_t to match PPG sensor data type
extern uint32_t an_x[BUFFER_SIZE]; // IR buffer
extern uint32_t an_y[BUFFER_SIZE]; // Red buffer

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);

// Phase 1: Enhanced function with quality metrics
void maxim_heart_rate_and_oxygen_saturation_with_quality(
    uint32_t *pun_ir_buffer, 
    int32_t n_ir_buffer_length, 
    uint32_t *pun_red_buffer, 
    int32_t *pn_spo2, 
    int8_t *pch_spo2_valid, 
    int32_t *pn_heart_rate, 
    int8_t *pch_hr_valid,
    spo2_quality_metrics_t *quality,
    spo2_probe_state_t *probe_state);  // Optional: pass NULL to disable filtering

void maxim_find_peaks(int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num);
void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height);
void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance);
void maxim_sort_ascend(int32_t  *pn_x, int32_t n_size);
void maxim_sort_indices_descend(int32_t  *pn_x, int32_t *pn_indx, int32_t n_size);

#endif /* ALGORITHM_H_ */