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

struct healthypi_time_t
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct hpi_log_session_header_t
{
    uint16_t session_id;
    uint32_t session_size;
    uint8_t file_no;
    struct healthypi_time_t session_start_time;
};

void hpi_datalog_start_session(uint8_t *in_pkt_buf);
void hpi_session_fetch(uint16_t session_id,uint8_t file_no);
void hpi_get_session_count(void);
void hpi_log_session_write_file();
void hpi_datalog_delete_session(uint16_t session_id,uint8_t file_no);
void hpi_datalog_delete_all(void);
void hpi_get_session_index(void);



