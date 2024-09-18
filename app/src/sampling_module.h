#pragma once

struct hpi_sensor_data_t {
    int32_t ecg_sample;
    int32_t bioz_sample;
    int32_t raw_red;
    int32_t raw_ir;
    int32_t temp;
    bool _bioZSkipSample;
    uint16_t rtor;
    uint16_t hr;
};

#define ECG_POINTS_PER_SAMPLE   8
#define BIOZ_POINTS_PER_SAMPLE  8

struct hpi_ecg_bioz_sensor_data_t
{
    int32_t ecg_samples[ECG_POINTS_PER_SAMPLE];
    int32_t bioz_sample[BIOZ_POINTS_PER_SAMPLE];
    uint8_t ecg_num_samples;
    uint8_t bioz_num_samples;
    uint16_t rtor;
    uint16_t hr;
    uint8_t ecg_lead_off;
    uint8_t bioz_lead_off;
    bool _bioZSkipSample;
};

 

