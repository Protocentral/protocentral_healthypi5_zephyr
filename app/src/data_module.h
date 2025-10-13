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


#pragma once

#include "hpi_common_types.h"

#define LOG_SAMPLE_RATE_SPS 125
#define LOG_WRITE_INTERVAL 10      // Write to file every 10 seconds
#define LOG_BUFFER_LENGTH 1250 + 1 // 125Hz * 10 seconds

#define ECG_DATA 0
#define PPG_DATA 1
#define RESP_DATA 2
#define ALL_DATA 3

struct hpi_computed_data_t {
    int32_t hr;
    uint32_t rr;
    int32_t spo2;
    uint8_t hr_valid;
    uint8_t spo2_valid;
};

enum hpi_stream_modes {
    HPI_STREAM_MODE_BLE,
    HPI_STREAM_MODE_USB,
    HPI_STREAM_MODE_RPI_UART,
    HPI_STREAM_MODE_PLOT,
};

// HR source selection functions
void hpi_data_set_hr_source(enum hpi_hr_source source);
enum hpi_hr_source hpi_data_get_hr_source(void);

void flush_current_session_logs(void);