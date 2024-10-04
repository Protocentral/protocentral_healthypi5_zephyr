#pragma once

#define IR_RED_BUFFER_SIZE      500 // 4 seconds of data at 125 Hz
#define RESP_RREST_BUFF_SIZE    2048    // 32 seconds at 64 sps

struct hpi_computed_data_t {
    int32_t hr;
    uint32_t rr;
    int32_t spo2;
    uint8_t hr_valid;
    uint8_t spo2_valid;
};