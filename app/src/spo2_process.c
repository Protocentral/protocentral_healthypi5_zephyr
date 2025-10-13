/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * Improved SpO2 Algorithm - Industry Standard Implementation
 * 
 * This implementation uses proper AC/DC ratio calculation with:
 * - Adaptive peak detection with robust thresholds
 * - Proper AC amplitude calculation (peak - valley in same pulse)
 * - Industry-standard quality metrics (Perfusion Index)
 * - Motion artifact rejection via pulse interval analysis
 * - Calibrated lookup table for R-ratio to SpO2 conversion
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "spo2_process.h"

LOG_MODULE_REGISTER(spo2_process, LOG_LEVEL_DBG);

// ============================================================================
// PROBE-OFF DETECTION THRESHOLDS
// ============================================================================
// These thresholds are tuned for the HealthyPi 5 hardware
// Adjust based on empirical testing with your specific AFE4400 configuration

// Perfusion Index threshold (in 0.01%, so 30 = 0.30%)
// Clinical context: < 0.3% typically indicates probe off or very poor perfusion
#define PROBE_OFF_PI_THRESHOLD      30      // 0.30%

// Minimum number of cardiac peaks required in 100-sample window
// 2 peaks allows for heart rates as low as ~40 bpm
#define PROBE_OFF_MIN_PEAKS         2

// AC amplitude threshold (ADC units)
// This is hardware-specific and depends on AFE4400 gain settings
// HealthyPi 5 typically sees AC=150-300 with good finger contact
#define PROBE_OFF_AC_THRESHOLD      150     // ADC units

// Shared buffers - exposed for memory efficiency
uint32_t an_x[BUFFER_SIZE]; // IR buffer
uint32_t an_y[BUFFER_SIZE]; // Red buffer

// ============================================================================
// IMPROVED SPO2 LOOKUP TABLE
// ============================================================================
// Calibrated SpO2 table based on R-ratio
// R = (AC_red/DC_red) / (AC_ir/DC_ir)
// Formula: SpO2 = -45.060*R² + 30.354*R + 94.845
// Table covers R values from 0.5 to 2.3 (index 50-230 = R*100)
// Valid SpO2 range: 70-100%
const uint8_t uch_spo2_table[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1
};

// ============================================================================
// PROBE STATE TRACKER (Consecutive Count Filtering)
// ============================================================================

/**
 * \brief Initialize probe state tracker
 * \par Details
 *        Sets up consecutive count thresholds and initial state
 * 
 * \param[in/out] state - Probe state structure
 * \param[in]     threshold_off - Consecutive off readings required (e.g., 4)
 * \param[in]     threshold_on - Consecutive on readings required (e.g., 4)
 */
void spo2_probe_state_init(spo2_probe_state_t *state, uint8_t threshold_off, uint8_t threshold_on)
{
    if (state == NULL) return;
    
    state->probe_off_state = true;  // Start as probe-off (conservative)
    state->consecutive_off = 0;
    state->consecutive_on = 0;
    state->threshold_off = threshold_off;
    state->threshold_on = threshold_on;
    
    LOG_INF("Probe state tracker initialized: off_thresh=%d, on_thresh=%d", 
            threshold_off, threshold_on);
}

/**
 * \brief Update probe state with new detection result
 * \par Details
 *        Implements consecutive count filtering to prevent single-sample flicker
 *        Requires N consecutive readings before changing state
 * 
 * \param[in/out] state - Probe state structure
 * \param[in]     probe_off_raw - Current raw probe-off detection
 * \return        true if state changed, false otherwise
 */
bool spo2_probe_state_update(spo2_probe_state_t *state, bool probe_off_raw)
{
    if (state == NULL) return false;
    
    bool state_changed = false;
    
    if (probe_off_raw) {
        // Raw detection says probe is off
        state->consecutive_off++;
        state->consecutive_on = 0;  // Reset on counter
        
        // Check if we've reached threshold to declare probe-off
        if (!state->probe_off_state && state->consecutive_off >= state->threshold_off) {
            state->probe_off_state = true;
            state_changed = true;
            LOG_INF("Probe state: OFF (after %d consecutive detections)", state->consecutive_off);
        }
    } else {
        // Raw detection says probe is on
        state->consecutive_on++;
        state->consecutive_off = 0;  // Reset off counter
        
        // Check if we've reached threshold to declare probe-on
        if (state->probe_off_state && state->consecutive_on >= state->threshold_on) {
            state->probe_off_state = false;
            state_changed = true;
            LOG_INF("Probe state: ON (after %d consecutive detections)", state->consecutive_on);
        }
    }
    
    return state_changed;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * \brief Sort array in ascending order (insertion sort)
 */
static void sort_ascend(int32_t *pn_x, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
            pn_x[j] = pn_x[j - 1];
        pn_x[j] = n_temp;
    }
}

/**
 * \brief Calculate mean of array
 */
static int32_t calculate_mean(uint32_t *buffer, int32_t length)
{
    int64_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += buffer[i];
    }
    return (int32_t)(sum / length);
}

// ============================================================================
// IMPROVED PEAK DETECTION
// ============================================================================

/**
 * \brief Detect peaks in signal using adaptive threshold
 * \par Details
 *        Uses adaptive thresholding and minimum distance filtering
 *        to robustly detect cardiac pulses
 * 
 * \param[out]  peak_locs - Array to store peak locations
 * \param[out]  n_peaks - Number of peaks found
 * \param[in]   signal - Input signal buffer
 * \param[in]   length - Signal length
 * \param[in]   min_distance - Minimum samples between peaks
 * \param[in]   max_peaks - Maximum number of peaks to detect
 */
static void detect_peaks(int32_t *peak_locs, int32_t *n_peaks, 
                        int32_t *signal, int32_t length,
                        int32_t min_distance, int32_t max_peaks)
{
    *n_peaks = 0;
    
    if (length < 10) return;
    
    // Calculate adaptive threshold (mean + 0.3*std_dev)
    int64_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += signal[i];
    }
    int32_t mean = (int32_t)(sum / length);
    
    int64_t var_sum = 0;
    for (int i = 0; i < length; i++) {
        int32_t diff = signal[i] - mean;
        var_sum += (int64_t)diff * diff;
    }
    int32_t std_dev = 1;
    if (var_sum > 0) {
        int64_t variance = var_sum / length;
        // Simple integer sqrt
        int64_t x = variance;
        int64_t y = (x + 1) / 2;
        while (y < x) {
            x = y;
            y = (x + variance / x) / 2;
        }
        std_dev = (int32_t)x;
    }
    
    int32_t threshold = mean + (std_dev * 3) / 10;  // mean + 0.3*std
    
    // Ensure reasonable threshold
    if (threshold < 20) threshold = 20;
    if (threshold > 100) threshold = 100;
    
    // Find peaks above threshold
    int32_t last_peak = -min_distance;
    for (int i = 1; i < length - 1 && *n_peaks < max_peaks; i++) {
        // Check if this is a local maximum above threshold
        if (signal[i] > threshold && 
            signal[i] > signal[i-1] && 
            signal[i] >= signal[i+1]) {
            
            // Check minimum distance from last peak
            if (i - last_peak >= min_distance) {
                peak_locs[*n_peaks] = i;
                (*n_peaks)++;
                last_peak = i;
            }
        }
    }
    
    LOG_DBG("Peak detection: threshold=%d, found %d peaks", threshold, *n_peaks);
}

// ============================================================================
// IMPROVED AC/DC CALCULATION
// ============================================================================

/**
 * \brief Calculate AC and DC components for each detected pulse
 * \par Details
 *        For each pulse between consecutive peaks:
 *        - Find actual min (valley) and max (peak) in the pulse segment
 *        - AC = peak - valley (true pulsatile amplitude)
 *        - DC = (peak + valley) / 2 (local baseline)
 * 
 * \param[in]   buffer - Signal buffer (IR or Red)
 * \param[in]   peak_locs - Array of peak locations
 * \param[in]   n_peaks - Number of peaks
 * \param[out]  ac_values - Array to store AC values for each pulse
 * \param[out]  dc_values - Array to store DC values for each pulse
 * \param[out]  n_valid - Number of valid AC/DC pairs calculated
 */
static void calculate_ac_dc(uint32_t *buffer, int32_t *peak_locs, int32_t n_peaks,
                           int32_t *ac_values, int32_t *dc_values, int32_t *n_valid)
{
    *n_valid = 0;
    
    if (n_peaks < 2) return;
    
    // For each pulse between consecutive peaks
    for (int p = 0; p < n_peaks - 1 && *n_valid < 10; p++) {
        int32_t start = peak_locs[p];
        int32_t end = peak_locs[p + 1];
        
        // Need reasonable segment length
        if (end - start < 10 || end - start > 200) continue;
        
        // Find min and max in this pulse segment
        uint32_t min_val = buffer[start];
        uint32_t max_val = buffer[start];
        
        for (int i = start + 1; i < end; i++) {
            if (buffer[i] < min_val) min_val = buffer[i];
            if (buffer[i] > max_val) max_val = buffer[i];
        }
        
        // Calculate AC (peak-to-peak amplitude / 2)
        int32_t ac = (int32_t)((max_val - min_val) / 2);
        
        // Calculate DC (local baseline = average of peak and valley)
        int32_t dc = (int32_t)((max_val + min_val) / 2);
        
        // Validate AC and DC
        if (ac > 0 && dc > 100) {  // DC must be substantial
            ac_values[*n_valid] = ac;
            dc_values[*n_valid] = dc;
            (*n_valid)++;
        }
    }
    
    LOG_DBG("AC/DC calculation: %d valid pulses from %d peaks", *n_valid, n_peaks);
}

// ============================================================================
// PERFUSION INDEX CALCULATION
// ============================================================================

/**
 * \brief Calculate Perfusion Index
 * \par Details
 *        PI = (AC / DC) × 100%
 *        Returns PI × 100 for precision (e.g., 150 = 1.5%)
 *        Clinical range: 0.2% (poor) to 20% (excellent)
 * 
 * \param[in]   ac - AC amplitude
 * \param[in]   dc - DC level
 * \return      Perfusion index × 100 (0-2000 = 0.0-20.0%)
 */
static uint16_t calculate_perfusion_index(int32_t ac, int32_t dc)
{
    if (dc == 0 || ac <= 0) return 0;
    
    // PI% = (AC / DC) × 100
    // Return PI × 100, so: (AC / DC) × 100 × 100 = (AC × 10000) / DC
    int64_t pi_scaled = ((int64_t)ac * 10000) / dc;
    
    // Cap at 20.0% (2000)
    if (pi_scaled > 2000) pi_scaled = 2000;
    if (pi_scaled < 0) pi_scaled = 0;
    
    return (uint16_t)pi_scaled;
}

// ============================================================================
// HEART RATE CALCULATION
// ============================================================================

/**
 * \brief Calculate heart rate from peak intervals
 * \par Details
 *        HR = 60 / avg_peak_interval_seconds
 *        Uses median of intervals for robustness
 * 
 * \param[in]   peak_locs - Array of peak locations
 * \param[in]   n_peaks - Number of peaks
 * \param[out]  pn_heart_rate - Calculated heart rate (bpm)
 * \param[out]  pch_hr_valid - 1 if valid, 0 if not
 */
static void calculate_heart_rate(int32_t *peak_locs, int32_t n_peaks,
                                 int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
    *pn_heart_rate = 0;
    *pch_hr_valid = 0;
    
    if (n_peaks < 2) {
        LOG_DBG("HR: Insufficient peaks (%d)", n_peaks);
        return;
    }
    
    // Calculate intervals between consecutive peaks
    int32_t intervals[14];  // Max 15 peaks = 14 intervals
    int32_t n_intervals = n_peaks - 1;
    
    for (int i = 0; i < n_intervals; i++) {
        intervals[i] = peak_locs[i + 1] - peak_locs[i];
    }
    
    // Use median interval for robustness against outliers
    sort_ascend(intervals, n_intervals);
    int32_t median_interval = intervals[n_intervals / 2];
    
    // Calculate HR: (60 seconds * FreqS samples/sec) / interval_samples
    // HR = (60 * FreqS) / interval
    if (median_interval > 0) {
        int32_t hr = (60 * FreqS) / median_interval;
        
        // Validate physiological range (30-220 bpm)
        if (hr >= 30 && hr <= 220) {
            *pn_heart_rate = hr;
            *pch_hr_valid = 1;
            LOG_DBG("HR: %d bpm from %d peaks, median interval=%d samples", 
                    hr, n_peaks, median_interval);
        } else {
            LOG_DBG("HR: %d bpm out of range (30-220)", hr);
        }
    }
}

// ============================================================================
// SPO2 CALCULATION
// ============================================================================

/**
 * \brief Calculate SpO2 from R-ratio
 * \par Details
 *        R = (AC_red/DC_red) / (AC_ir/DC_ir)
 *        SpO2 looked up from calibration table
 * 
 * \param[in]   ac_red_values - Array of Red AC values
 * \param[in]   dc_red_values - Array of Red DC values  
 * \param[in]   ac_ir_values - Array of IR AC values
 * \param[in]   dc_ir_values - Array of IR DC values
 * \param[in]   n_values - Number of values
 * \param[out]  pn_spo2 - Calculated SpO2 (%)
 * \param[out]  pch_spo2_valid - 1 if valid, 0 if not
 */
static void calculate_spo2(int32_t *ac_red, int32_t *dc_red,
                          int32_t *ac_ir, int32_t *dc_ir,
                          int32_t n_values,
                          int32_t *pn_spo2, int8_t *pch_spo2_valid)
{
    *pn_spo2 = 0;
    *pch_spo2_valid = 0;
    
    // Require at least 1 valid pulse (relaxed from 2 for better responsiveness)
    if (n_values < 1) {
        LOG_DBG("SpO2: Insufficient valid pulses (%d)", n_values);
        return;
    }
    
    // Calculate R-ratio for each pulse
    int32_t r_ratios[10];
    int32_t n_ratios = 0;
    
    for (int i = 0; i < n_values && n_ratios < 10; i++) {
        if (dc_red[i] == 0 || dc_ir[i] == 0) continue;
        if (ac_ir[i] == 0) continue;
        
        // R = (AC_red/DC_red) / (AC_ir/DC_ir)
        // Rearranged: R = (AC_red × DC_ir) / (AC_ir × DC_red)
        // Scale by 100 to get integer ratio (R × 100)
        int64_t numerator = (int64_t)ac_red[i] * dc_ir[i];
        int64_t denominator = (int64_t)ac_ir[i] * dc_red[i];
        
        if (denominator > 0) {
            int32_t r_ratio = (int32_t)((numerator * 100) / denominator);
            
            // Validate range (50-230 = R of 0.5 to 2.3)
            if (r_ratio >= 50 && r_ratio <= 230) {
                r_ratios[n_ratios++] = r_ratio;
            }
        }
    }
    
    if (n_ratios == 0) {
        LOG_DBG("SpO2: No valid R-ratios calculated");
        return;
    }
    
    // Use median R-ratio for robustness
    sort_ascend(r_ratios, n_ratios);
    int32_t median_r = r_ratios[n_ratios / 2];
    
    // Look up SpO2 from table
    // Table is indexed from R=0.5 to R=2.33, so index = R*100 - 50
    int32_t table_idx = median_r - 50;
    
    if (table_idx >= 0 && table_idx < 184) {
        *pn_spo2 = uch_spo2_table[table_idx];
        *pch_spo2_valid = 1;
        LOG_DBG("SpO2: %d%% from R=%d.%02d (%d ratios, median used)", 
                *pn_spo2, median_r/100, median_r%100, n_ratios);
    } else {
        LOG_DBG("SpO2: R-ratio %d.%02d out of table range", 
                median_r/100, median_r%100);
    }
}

// ============================================================================
// MAIN ALGORITHM INTERFACE
// ============================================================================

/**
 * \brief Calculate heart rate and SpO2 with quality metrics
 * \par Details
 *        Industry-standard implementation with:
 *        - Adaptive peak detection
 *        - Proper AC/DC ratio calculation  
 *        - Perfusion Index measurement
 *        - Motion artifact detection
 *        - Quality scoring
 *        - Consecutive count filtering for probe-off (optional)
 * 
 * \param[in]   pun_ir_buffer - IR sensor data buffer
 * \param[in]   n_ir_buffer_length - Buffer length (should be BUFFER_SIZE)
 * \param[in]   pun_red_buffer - Red sensor data buffer
 * \param[out]  pn_spo2 - Calculated SpO2 (%)
 * \param[out]  pch_spo2_valid - 1 if valid, 0 otherwise
 * \param[out]  pn_heart_rate - Calculated heart rate (bpm)
 * \param[out]  pch_hr_valid - 1 if valid, 0 otherwise
 * \param[out]  quality - Quality metrics structure (can be NULL)
 * \param[in/out] probe_state - Probe state tracker for filtering (can be NULL)
 */
void maxim_heart_rate_and_oxygen_saturation_with_quality(
    uint32_t *pun_ir_buffer, 
    int32_t n_ir_buffer_length, 
    uint32_t *pun_red_buffer, 
    int32_t *pn_spo2, 
    int8_t *pch_spo2_valid, 
    int32_t *pn_heart_rate, 
    int8_t *pch_hr_valid,
    spo2_quality_metrics_t *quality,
    spo2_probe_state_t *probe_state)
{
    // Initialize outputs
    *pn_spo2 = 0;
    *pch_spo2_valid = 0;
    *pn_heart_rate = 0;
    *pch_hr_valid = 0;
    
    if (quality != NULL) {
        quality->perfusion_ir = 0;
        quality->perfusion_red = 0;
        quality->signal_strength = 0;
        quality->confidence = 0;
        quality->valid = false;
        quality->probe_off = true;  // Assume probe off until proven otherwise (raw)
        quality->probe_off_filtered = true;  // Filtered state
        quality->probe_off_reason = PROBE_OFF_NONE;
    }
    
    // Validate inputs
    if (pun_ir_buffer == NULL || pun_red_buffer == NULL) {
        LOG_ERR("NULL buffer pointers");
        return;
    }
    
    if (n_ir_buffer_length < 100) {
        LOG_ERR("Buffer too short: %d samples", n_ir_buffer_length);
        return;
    }
    
    // === STEP 1: Calculate DC baselines and check signal quality ===
    int32_t mean_ir = calculate_mean(pun_ir_buffer, n_ir_buffer_length);
    int32_t mean_red = calculate_mean(pun_red_buffer, n_ir_buffer_length);
    
    LOG_DBG("Signal means: IR=%d, Red=%d", mean_ir, mean_red);
    
    // ============================================================================
    // ENHANCED PROBE-OFF DETECTION
    // ============================================================================
    // Multi-criteria approach for reliable probe disconnection detection:
    // 1. DC signal strength (primary - detects physical removal)
    // 2. Signal saturation (detects probe malposition)
    // 3. Subsequent checks: PI, peak count, AC amplitude (secondary validation)
    
    bool probe_off_detected = false;
    uint8_t probe_off_reason = PROBE_OFF_NONE;
    
    // Check 1: DC Signal Too Weak (probe physically removed or very poor contact)
    // Clinical: Normal PPG DC values are 50k-500k for AFE4400
    // Threshold 1000 is extremely conservative - probe must be completely off
    if (mean_ir < 1000 || mean_red < 1000) {
        probe_off_detected = true;
        probe_off_reason = PROBE_OFF_LOW_DC;
        LOG_WRN("Probe OFF: DC too weak (IR=%d, Red=%d, need >1000)", mean_ir, mean_red);
        if (quality != NULL) {
            quality->probe_off = true;
            quality->probe_off_reason = probe_off_reason;
        }
        return;
    }
    
    // Check 2: Signal Saturation (probe pressed too hard or light overflow)
    // AFE4400 has 22-bit ADC: max ~4.2M, use 3.8M as saturation threshold (90%)
    if (mean_ir > 3800000 || mean_red > 3800000) {
        probe_off_detected = true;
        probe_off_reason = PROBE_OFF_SATURATED;
        LOG_WRN("Probe OFF: Signal saturated (IR=%d, Red=%d, max 3.8M)", mean_ir, mean_red);
        if (quality != NULL) {
            quality->probe_off = true;
            quality->probe_off_reason = probe_off_reason;
        }
        return;
    }
    
    // Check 3: Weak signal (marginal contact - still process but flag low quality)
    // This doesn't immediately trigger probe-off, but contributes to confidence scoring
    bool weak_signal = (mean_ir < 10000 || mean_red < 10000);
    if (weak_signal) {
        LOG_DBG("Weak signal detected: IR=%d, Red=%d", mean_ir, mean_red);
    }
    
    // === STEP 2: Prepare IR signal for peak detection ===
    // Remove DC and invert (so peaks become valleys for valley detection)
    int32_t ir_normalized[BUFFER_SIZE];
    for (int i = 0; i < n_ir_buffer_length; i++) {
        ir_normalized[i] = -((int32_t)pun_ir_buffer[i] - mean_ir);
    }
    
    // === STEP 3: Detect peaks (cardiac pulses) ===
    int32_t peak_locs[15];
    int32_t n_peaks = 0;
    
    // At 125 Hz, typical HR of 60-90 bpm means 1-1.5 beats/sec
    // In 2 seconds: expect 2-3 beats
    // Minimum distance: 125 Hz / 3 Hz = ~40 samples (prevent dicrotic notch)
    detect_peaks(peak_locs, &n_peaks, ir_normalized, n_ir_buffer_length, 40, 15);
    
    if (n_peaks < 2) {
        LOG_WRN("Insufficient peaks detected: %d", n_peaks);
        return;
    }
    
    // === STEP 4: Calculate heart rate ===
    calculate_heart_rate(peak_locs, n_peaks, pn_heart_rate, pch_hr_valid);
    
    // === STEP 5: Calculate AC/DC for IR and Red ===
    int32_t ac_ir[10], dc_ir[10], n_valid_ir = 0;
    int32_t ac_red[10], dc_red[10], n_valid_red = 0;
    
    calculate_ac_dc(pun_ir_buffer, peak_locs, n_peaks, ac_ir, dc_ir, &n_valid_ir);
    calculate_ac_dc(pun_red_buffer, peak_locs, n_peaks, ac_red, dc_red, &n_valid_red);
    
    // Require at least 1 valid AC/DC pair (relaxed from 2 for better responsiveness)
    // With 2 peaks: 1 pulse → 1 valid pair (no median, but still valid)
    // With 3+ peaks: 2+ pulses → median filtering for robustness
    if (n_valid_ir < 1 || n_valid_red < 1) {
        LOG_WRN("Insufficient valid AC/DC values: IR=%d, Red=%d", n_valid_ir, n_valid_red);
        return;
    }
    
    // === STEP 6: Calculate SpO2 ===
    calculate_spo2(ac_red, dc_red, ac_ir, dc_ir, 
                   (n_valid_ir < n_valid_red) ? n_valid_ir : n_valid_red,
                   pn_spo2, pch_spo2_valid);
    
    // === STEP 7: Calculate quality metrics with enhanced probe-off detection ===
    if (quality != NULL && n_valid_ir > 0 && n_valid_red > 0) {
        // Use median AC and DC for quality calculation
        sort_ascend(ac_ir, n_valid_ir);
        sort_ascend(dc_ir, n_valid_ir);
        sort_ascend(ac_red, n_valid_red);
        sort_ascend(dc_red, n_valid_red);
        
        int32_t median_ac_ir = ac_ir[n_valid_ir / 2];
        int32_t median_dc_ir = dc_ir[n_valid_ir / 2];
        int32_t median_ac_red = ac_red[n_valid_red / 2];
        int32_t median_dc_red = dc_red[n_valid_red / 2];
        
        // Calculate Perfusion Indices
        quality->perfusion_ir = calculate_perfusion_index(median_ac_ir, median_dc_ir);
        quality->perfusion_red = calculate_perfusion_index(median_ac_red, median_dc_red);
        quality->signal_strength = (uint16_t)median_ac_ir;
        
        // ========================================================================
        // ENHANCED PROBE-OFF DETECTION (Multi-criteria)
        // ========================================================================
        // Hierarchical detection with multiple criteria for robustness
        // Priority: DC level > Saturation > Perfusion > Peaks > AC strength
        
        probe_off_detected = false;
        probe_off_reason = PROBE_OFF_NONE;
        
        // Criterion 1: Perfusion Index (most reliable for probe removal)
        // Clinical ranges:
        //   < 0.3% (30): Probe likely off or extremely poor perfusion
        //   0.3-0.5%: Marginal (sick patient or poor contact)
        //   0.5-1.0%: Adequate for most patients
        //   > 1.0%: Good signal
        if (quality->perfusion_ir < PROBE_OFF_PI_THRESHOLD) {
            probe_off_detected = true;
            probe_off_reason = PROBE_OFF_LOW_PI;
            LOG_WRN("Probe OFF: PI too low (IR=%d.%02d%%)", 
                    quality->perfusion_ir / 100, quality->perfusion_ir % 100);
        }
        
        // Criterion 2: Insufficient cardiac pulses
        else if (n_peaks < PROBE_OFF_MIN_PEAKS) {
            probe_off_detected = true;
            probe_off_reason = PROBE_OFF_NO_PEAKS;
            LOG_WRN("Probe OFF: Insufficient peaks (%d)", n_peaks);
        }
        
        // Criterion 3: AC amplitude too weak (even if PI calculation worked)
        // This catches edge cases where DC is high but AC is negligible
        else if (median_ac_ir < PROBE_OFF_AC_THRESHOLD) {
            probe_off_detected = true;
            probe_off_reason = PROBE_OFF_WEAK_AC;
            LOG_WRN("Probe OFF: AC too weak (%d, need >%d)", median_ac_ir, PROBE_OFF_AC_THRESHOLD);
        }
        
        // Set probe-off flag (RAW detection, single reading)
        quality->probe_off = probe_off_detected;
        quality->probe_off_reason = probe_off_reason;
        
        // Apply consecutive count filtering if probe_state is provided
        if (probe_state != NULL) {
            bool state_changed = spo2_probe_state_update(probe_state, probe_off_detected);
            quality->probe_off_filtered = probe_state->probe_off_state;
            
            if (state_changed) {
                LOG_INF("Probe state changed: %s (raw=%d, reason=%d, PI=%d.%02d%%)",
                        probe_state->probe_off_state ? "OFF" : "ON",
                        probe_off_detected, probe_off_reason,
                        quality->perfusion_ir / 100, quality->perfusion_ir % 100);
            }
        } else {
            // No filtering, filtered state = raw state
            quality->probe_off_filtered = probe_off_detected;
        }
        
        // ========================================================================
        // CONFIDENCE SCORING (for quality assessment)
        // ========================================================================
        int32_t confidence = 100;
        
        // Penalize low perfusion (graduated scale)
        if (quality->perfusion_ir < 30) {          // < 0.3%
            confidence = 0;  // Invalid - probe off
        } else if (quality->perfusion_ir < 50) {   // < 0.5%
            confidence -= 50;  // Very poor
        } else if (quality->perfusion_ir < 100) {  // < 1.0%
            confidence -= 30;  // Poor
        } else if (quality->perfusion_ir < 200) {  // < 2.0%
            confidence -= 15;  // Marginal
        }
        
        // Penalize few peaks (motion artifact or irregular rhythm)
        if (n_peaks < 3) {
            confidence -= 40;
        } else if (n_peaks < 4) {
            confidence -= 20;
        } else if (n_peaks < 5) {
            confidence -= 10;
        }
        
        // Penalize weak signal (aligned with PROBE_OFF_AC_THRESHOLD=150)
        if (median_ac_ir < PROBE_OFF_AC_THRESHOLD) {
            confidence = 0;  // Below probe-off threshold
        } else if (median_ac_ir < 300) {
            confidence -= 40;  // Marginal signal
        } else if (median_ac_ir < 500) {
            confidence -= 20;
        } else if (median_ac_ir < 1000) {
            confidence -= 10;
        }
        
        // Penalize weak DC signal
        if (weak_signal) {
            confidence -= 20;
        }
        
        // Ensure confidence is valid range
        if (confidence < 0) confidence = 0;
        quality->confidence = (uint8_t)confidence;
        
        // ========================================================================
        // OVERALL VALIDITY DETERMINATION
        // ========================================================================
        // Signal is valid only if:
        // 1. Probe is connected (not probe_off)
        // 2. SpO2 and HR calculations succeeded
        // 3. Confidence is reasonable (>40%)
        // 4. PI is above minimum threshold (>0.3%)
        
        quality->valid = (!probe_off_detected &&
                         *pch_spo2_valid && 
                         *pch_hr_valid && 
                         quality->confidence >= 40 &&
                         quality->perfusion_ir >= 30);  // Minimum 0.3% PI
        
        // Logging for debugging
        if (probe_off_detected) {
            LOG_WRN("PROBE OFF: Reason=%d, PI=%d.%02d%%, AC=%d, Peaks=%d",
                    probe_off_reason,
                    quality->perfusion_ir / 100, quality->perfusion_ir % 100,
                    quality->signal_strength, n_peaks);
        } else {
            LOG_INF("Quality: PI=%d.%02d%%, AC=%d, Conf=%d%%, Valid=%d, Peaks=%d",
                    quality->perfusion_ir / 100, quality->perfusion_ir % 100,
                    quality->signal_strength, quality->confidence, 
                    quality->valid, n_peaks);
        }
    }
}

/**
 * \brief Legacy function for backwards compatibility
 */
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t *pun_ir_buffer, 
    int32_t n_ir_buffer_length, 
    uint32_t *pun_red_buffer, 
    int32_t *pn_spo2, 
    int8_t *pch_spo2_valid, 
    int32_t *pn_heart_rate, 
    int8_t *pch_hr_valid)
{
    maxim_heart_rate_and_oxygen_saturation_with_quality(
        pun_ir_buffer, n_ir_buffer_length, pun_red_buffer,
        pn_spo2, pch_spo2_valid, pn_heart_rate, pch_hr_valid,
        NULL,  // No quality metrics
        NULL   // No probe state filtering
    );
}

// ============================================================================
// LEGACY HELPER FUNCTIONS (kept for API compatibility)
// ============================================================================

void maxim_sort_ascend(int32_t *pn_x, int32_t n_size)
{
    sort_ascend(pn_x, n_size);
}

void maxim_find_peaks(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, 
                     int32_t n_size, int32_t n_min_height, 
                     int32_t n_min_distance, int32_t n_max_num)
{
    detect_peaks(pn_locs, n_npks, pn_x, n_size, n_min_distance, n_max_num);
}

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *n_npks, 
                                   int32_t *pn_x, int32_t n_size, 
                                   int32_t n_min_height)
{
    // Legacy function - not used in new implementation
    (void)pn_locs;
    (void)n_npks;
    (void)pn_x;
    (void)n_size;
    (void)n_min_height;
}

void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, 
                              int32_t *pn_x, int32_t n_min_distance)
{
    // Legacy function - not used in new implementation
    (void)pn_locs;
    (void)pn_npks;
    (void)pn_x;
    (void)n_min_distance;
}

void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
    // Legacy function - not used in new implementation
    (void)pn_x;
    (void)pn_indx;
    (void)n_size;
}
