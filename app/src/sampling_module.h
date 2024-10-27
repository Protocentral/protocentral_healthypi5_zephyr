#pragma once

struct hpi_sensor_data_t {
    int32_t ecg_sample;
    int32_t bioz_samples;
    int32_t raw_red;
    int32_t raw_ir;
    int32_t temp;
    bool _bioZSkipSample;
    uint16_t rtor;
    uint16_t hr;
};

#define ECG_POINTS_PER_SAMPLE   8
#define BIOZ_POINTS_PER_SAMPLE  4
#define PPG_POINTS_PER_SAMPLE   1

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

struct hpi_ppg_sensor_data_t
{
    int32_t ppg_red_samples[PPG_POINTS_PER_SAMPLE];
    int32_t ppg_ir_samples[PPG_POINTS_PER_SAMPLE];
    uint8_t ppg_num_samples;
    uint8_t ppg_lead_off;
};

 



