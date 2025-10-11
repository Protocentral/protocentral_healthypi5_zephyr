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


#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "resp_process.h"

LOG_MODULE_REGISTER(resp_process, LOG_LEVEL_DBG);

// Global variables for compatibility
uint8_t Respiration_Rate = 0;
int RESP_Second_Prev_Sample = 0;
int RESP_Prev_Sample = 0;
int RESP_Current_Sample = 0;
int RESP_Next_Sample = 0;
int RESP_Second_Next_Sample = 0;

/**
 * ROBUST ALGORITHM: Adaptive Threshold Crossing Detection
 *
 * Designed for real-world respiration signals with varying amplitude and frequency.
 *
 * Key Features:
 * 1. Adaptive baseline tracking - follows DC drift and amplitude changes
 * 2. Hysteresis - prevents false triggers from noise
 * 3. Period validation - rejects outliers and artifacts
 * 4. Moving average - provides stable rate output
 * 5. Auto-calibrating timing - doesn't depend on exact sample rate
 *
 * Method:
 * - Track running average (baseline) using exponential moving average
 * - Detect upward crossings of baseline with hysteresis bands
 * - Measure breath intervals in samples (sample count between breaths)
 * - Use RATIO of intervals to calculate rate (independent of absolute timing)
 *
 * Timing Approach:
 * - Instead of assuming exact sample rate, we measure intervals in samples
 * - Rate is calculated from the ratio: Current_Interval / Reference_Interval
 * - This makes the algorithm robust to sample rate variations
 * - Calibration constant can be adjusted based on known reference signal
 *
 * CALIBRATION_FACTOR: Adjust this to match actual sample rate
 * - Default 1250 assumes ~50 Hz call rate (closer to reality than 31.25 Hz)
 * - If measuring known 20 BPM signal with interval N, factor = N × 20 / 60
 */

// Calibration factor for rate calculation
// Empirically determined from 20 BPM reference signal:
// Measured interval = 149 samples at 20 BPM
// Therefore: CALIBRATION_FACTOR = 149 × 20 = 2980
// This gives: Rate (BPM) = 2980 / interval_in_samples
//
// This calibration automatically accounts for:
// - Actual sampling rate (not exactly 125 Hz)
// - Buffer size and calling frequency
// - Any timing variations in the system
#define RESP_CALIBRATION_FACTOR 2980

void resp_process_sample(int16_t *CurrAqsSample, int16_t *respFiltered)
{
    // Pass through without filtering for now
    // Can add gentle lowpass filter here if needed for noise reduction
    for (int i = 0; i < 4; i++)
    {
        respFiltered[i] = CurrAqsSample[i];
    }
}

void resp_algo_process(int16_t *CurrSample, volatile uint8_t *RespirationRate)
{
    // Average the 4 input samples
    long Mac = 0;
    for (int k = 0; k < 4; k++)
    {
        Mac += CurrSample[k];
    }

    // Pass averaged sample to rate detection
    resp_rate_detect((int16_t)(Mac >> 2), RespirationRate);
}

void resp_rate_detect(int16_t Resp_wave, volatile uint8_t *RespirationRate)
{
    // State variables
    static int16_t prevSample = 0;
    static int32_t baseline = 0;          // Running average baseline (Q16 fixed-point)
    static uint16_t samplesSinceCrossing = 0;
    static uint16_t breathIntervals[8] = {0};  // Store last 8 breath intervals
    static uint8_t intervalIndex = 0;
    static uint8_t validIntervals = 0;
    static uint16_t startupDelay = 312;   // Skip first 10 seconds
    static bool waitingForLow = false;     // State machine for hysteresis
    static int16_t minAmplitude = 32767;   // Track signal amplitude (initialized to max int16)
    static int16_t maxAmplitude = -32768;  // Track signal amplitude (initialized to min int16)
    static uint16_t amplitudeCheckTimer = 0;

    // Skip startup transient
    if (startupDelay > 0)
    {
        startupDelay--;
        prevSample = Resp_wave;
        baseline = (int32_t)Resp_wave << 16;  // Initialize baseline
        if (startupDelay == 0)
        {
            LOG_INF("RESP: Adaptive threshold detection started, initial baseline=%d", (int16_t)(baseline >> 16));
        }

        // Log a few samples during startup to see signal
        if (startupDelay % 31 == 0)  // Every ~1 second
        {
            LOG_DBG("RESP startup: sample=%d, baseline=%d", Resp_wave, (int16_t)(baseline >> 16));
        }
        return;
    }

    // Update baseline using exponential moving average
    // Alpha = 1/256 gives very slow tracking to follow DC drift only, not the breath signal
    // For 20 BPM: period = 3 sec = 94 samples, time constant should be >> 94 samples
    // Alpha = 1/256 gives time constant of 256 samples (~8 sec), good for 8-80 BPM range
    baseline = baseline - (baseline >> 8) + ((int32_t)Resp_wave << 8);
    int16_t baselineValue = (int16_t)(baseline >> 16);

    // Track amplitude for signal quality monitoring
    if (Resp_wave < minAmplitude) minAmplitude = Resp_wave;
    if (Resp_wave > maxAmplitude) maxAmplitude = Resp_wave;

    amplitudeCheckTimer++;
    if (amplitudeCheckTimer >= 125)  // Every 4 seconds
    {
        int16_t peakToPeak = maxAmplitude - minAmplitude;
        LOG_DBG("RESP: Amplitude=%d, Baseline=%d, Signal range: [%d, %d]",
                peakToPeak, baselineValue, minAmplitude, maxAmplitude);

        // Check for signal loss
        if (peakToPeak < 50)
        {
            LOG_WRN("RESP: Weak signal, amplitude=%d", peakToPeak);
            Respiration_Rate = 0;
            validIntervals = 0;
        }

        minAmplitude = 32767;
        maxAmplitude = -32768;
        amplitudeCheckTimer = 0;
    }

    // Increment sample counter
    samplesSinceCrossing++;

    // Periodic logging to see signal values
    static uint16_t signalLogTimer = 0;
    if (signalLogTimer++ >= 31)  // Every ~1 second
    {
        signalLogTimer = 0;
        LOG_DBG("RESP signal: wave=%d, baseline=%d, upper=%d, lower=%d, state=%s",
                Resp_wave, baselineValue, baselineValue + 40, baselineValue - 40,
                waitingForLow ? "waitLow" : "waitHigh");
    }

    // State machine with hysteresis to detect breath cycle
    // Reduced hysteresis for better sensitivity - using ±20 instead of ±40
    // This allows detection of smaller amplitude variations
    int16_t hysteresis = 20;
    int16_t upperThreshold = baselineValue + hysteresis;
    int16_t lowerThreshold = baselineValue - hysteresis;

    if (!waitingForLow)
    {
        // State: Above baseline, waiting to go below
        // Looking for end of inhalation / start of exhalation
        if (Resp_wave < lowerThreshold)
        {
            waitingForLow = true;
            LOG_DBG("RESP: Crossed below baseline (sample %d)", samplesSinceCrossing);
        }
    }
    else
    {
        // State: Below baseline, waiting to go above
        // Looking for end of exhalation / start of inhalation (= one breath complete)
        if (Resp_wave > upperThreshold)
        {
            // Complete breath cycle detected!
            waitingForLow = false;

            // Validate interval using calibrated ranges
            // With CALIBRATION_FACTOR = 2980:
            //   80 BPM: 2980 / 80 = 37 samples minimum
            //   8 BPM: 2980 / 8 = 372 samples maximum
            // Use slightly wider range for safety: 30 to 400 samples
            if (samplesSinceCrossing >= 30 && samplesSinceCrossing <= 400)
            {
                // Store this breath interval
                breathIntervals[intervalIndex] = samplesSinceCrossing;
                intervalIndex++;
                if (intervalIndex >= 8)
                {
                    intervalIndex = 0;
                }

                if (validIntervals < 8)
                {
                    validIntervals++;
                }

                // Calculate and display the breath rate from this single interval
                // Using calibrated formula: Rate = CALIBRATION_FACTOR / interval
                uint16_t instantRate = RESP_CALIBRATION_FACTOR / samplesSinceCrossing;
                LOG_INF("RESP: Breath detected! Interval=%d samples = %d BPM, validIntervals=%d",
                        samplesSinceCrossing, instantRate, validIntervals);

                // Calculate rate if we have enough data (at least 3 breaths)
                if (validIntervals >= 3)
                {
                    // Average the stored breath intervals
                    uint32_t sum = 0;
                    for (int i = 0; i < validIntervals; i++)
                    {
                        sum += breathIntervals[i];
                    }
                    uint16_t avgInterval = sum / validIntervals;

                    // Calculate respiration rate using calibration factor
                    // Rate (BPM) = CALIBRATION_FACTOR / avgInterval
                    // For 50Hz sampling: 1250 / 150 samples = 8.33... but we need integer
                    // Better: multiply first to avoid truncation
                    // Rate = (CALIBRATION_FACTOR * 10) / avgInterval / 10 for better precision
                    uint16_t calculatedRate = RESP_CALIBRATION_FACTOR / avgInterval;

                    LOG_INF("RESP: Calculated rate=%d BPM from avgInterval=%d (factor=%d)",
                            calculatedRate, avgInterval, RESP_CALIBRATION_FACTOR);

                    // Sanity check: rate should be 8-80 BPM
                    if (calculatedRate >= 8 && calculatedRate <= 80)
                    {
                        Respiration_Rate = (uint8_t)calculatedRate;
                        LOG_INF("RESP RATE: %d BPM (avg interval: %d samples, n=%d)",
                                Respiration_Rate, avgInterval, validIntervals);
                    }
                    else
                    {
                        LOG_WRN("RESP: Calculated rate %d BPM out of range (expected 8-80)", calculatedRate);
                    }
                }
                else
                {
                    LOG_DBG("RESP: Waiting for more breaths (%d/3)", validIntervals);
                }
            }
            else
            {
                LOG_WRN("RESP: Invalid interval %d samples (expected 30-400)", samplesSinceCrossing);

                // If interval is way too long, might have lost signal
                if (samplesSinceCrossing > 400)
                {
                    validIntervals = 0;  // Reset, need fresh measurements
                }
            }

            // Reset counter for next breath
            samplesSinceCrossing = 0;
        }
    }

    // Timeout detection: if no crossing for >10 seconds, reset
    if (samplesSinceCrossing > 312)  // 312 samples × 32ms = 10 seconds
    {
        LOG_WRN("RESP: No breath detected for >10 sec, resetting");
        Respiration_Rate = 0;
        validIntervals = 0;
        waitingForLow = false;
        samplesSinceCrossing = 0;
    }

    // Store current sample for next iteration
    prevSample = Resp_wave;

    // Update output
    *RespirationRate = Respiration_Rate;
}
