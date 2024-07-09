#pragma once

struct hpi_computed_data_t {
    int32_t hr;
    uint32_t rr;
    int32_t spo2;
    uint8_t hr_valid;
    uint8_t spo2_valid;
};

struct hpi_computed_hrv_t {
    int32_t hrv_max;
    int32_t hrv_min;
    float mean;
    float sdnn;
    float pnn;
    float rmssd; 
    bool hrv_ready_flag;
};