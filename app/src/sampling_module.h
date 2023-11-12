#pragma once

struct hpi_sensor_data_t {
    int32_t ecg_sample;
    int32_t bioz_sample;
    int32_t raw_red;
    int32_t raw_ir;
    int32_t temp;
    bool _bioZSkipSample;
};



