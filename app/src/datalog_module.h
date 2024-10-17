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
    uint16_t session_size;
    struct healthypi_time_t session_start_time;
};

void hpi_datalog_start_session(in_pkt_buf);
void hpi_session_fetch(uint16_t session_id);
uint16_t hpi_get_session_count(void);
void hpi_log_session_write_file();
