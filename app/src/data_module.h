#pragma once

struct hpi_computed_data_t {
    int32_t hr;
    uint8_t rr;
    int32_t spo2;
    uint8_t hr_valid;
    uint8_t spo2_valid;
};