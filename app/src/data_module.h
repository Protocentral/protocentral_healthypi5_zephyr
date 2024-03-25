#pragma once

enum hpi5_data_format
{
    DATA_FMT_OPENVIEW,
    DATA_FMT_PLAIN_TEXT,
} hpi5_data_format_t;

struct hpi_computed_data_t {
    int32_t hr;
    uint32_t rr;
    int32_t spo2;
    uint8_t hr_valid;
    uint8_t spo2_valid;
};