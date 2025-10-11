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
HealthyPi specific common data types
*/

#pragma once

#define PPG_POINTS_PER_SAMPLE 8

#define ECG_POINTS_PER_SAMPLE   8
#define BIOZ_POINTS_PER_SAMPLE  4

// HR source selection
enum hpi_hr_source {
    HR_SOURCE_ECG = 0,    // From MAX30001 R-R interval
    HR_SOURCE_PPG = 1,    // From AFE4400 SpO2 algorithm
};

struct hpi_hr_trend_point_t
{
    uint16_t hr;
    uint32_t timestamp;
};

struct hpi_ecg_bioz_sensor_data_t
{
    int32_t ecg_samples[ECG_POINTS_PER_SAMPLE];
    int32_t bioz_samples[BIOZ_POINTS_PER_SAMPLE];

    uint8_t ecg_num_samples;
    uint8_t bioz_num_samples;

    uint16_t rtor;
    uint16_t hr;
    uint8_t ecg_lead_off;
    uint8_t bioz_lead_off;
    bool _bioZSkipSample;
};

struct hpi_sensor_data_point_t
{
    int32_t ecg_sample;
    int32_t bioz_sample;

    uint8_t hr;
    uint8_t rtor;
    uint8_t ecg_lead_off;
    uint8_t bioz_lead_off;

    bool _bioZSkipSample;

    int32_t ppg_sample_red;
    int32_t ppg_sample_ir;
};

struct hpi_ppg_sensor_data_t
{
    int32_t ppg_red_sample;
    int32_t ppg_ir_sample;
    uint8_t ppg_lead_off;
    uint8_t spo2;
};

struct hpi_computed_hrv_t
{
    int32_t hrv_max;
    int32_t hrv_min;
    float mean;
    float sdnn;
    float pnn;
    float rmssd;
    bool hrv_ready_flag;
};

struct hpi_hr_t
{
    uint16_t hr;
};

struct hpi_steps_t
{
    uint32_t steps_run;
    uint32_t steps_walk;
};

struct hpi_temp_t
{
    double temp_f;
    double temp_c;
};

struct hpi_spo2_t
{
    uint8_t spo2;
};

struct hpi_resp_rate_t
{
    uint16_t resp_rate;
};

struct hpi_batt_status_t
{
    uint8_t batt_level;
    bool batt_charging;
};
