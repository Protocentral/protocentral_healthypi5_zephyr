#pragma once

struct hpi_log_session_header_t
{
    uint16_t session_id;
    uint16_t session_size;
    struct healthypi_time_t session_start_time;
};

void hpi_datalog_start_session(void);
void hpi_session_fetch(uint16_t session_id);
uint16_t hpi_get_session_count(void);
