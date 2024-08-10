#pragma once

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_PKT_STOP_1 0x00
#define CES_CMDIF_PKT_STOP_2 0x0B

#define CMD_SESSION_START_TES 0x09
#define CMD_SESSION_START_TDCS 0x10
#define CMD_SESSION_START_TPCS 0x11
#define CMD_SESSION_START_TACS 0x12

#define CMD_SESSION_PAUSE 0x13
#define CMD_SESSION_ABORT 0x14
#define CMD_SESSION_RESUME 0x15

#define CMD_PROGRAM_SAVE_TDCS 0x20
#define CMD_PROGRAM_SAVE_TPCS 0x21
#define CMD_PROGRAM_SAVE_TACS 0x22

#define CMD_LOGGING_START 0x55
#define CMD_LOGGING_MEMORY_FREE 0x32
#define CMD_LOGGING_MEMORY_NOT_AVAILABLE 0x31
#define CMD_LOG_GET_COUNT 0x54 //0x06 and file count
#define CMD_LOG_FILE_NAMES 0x50



void cmdif_send_ble_progress(uint8_t m_stage, uint16_t m_total_time, uint16_t m_curr_time, uint16_t m_current, uint16_t m_imped);
void cmdif_send_ble_command(uint8_t m_cmd);
void cmdif_send_ble_device_status_response(void);

void cmdif_send_ble_data(const char *buf, size_t len);

enum cmdsm_state
{
    CMD_SM_STATE_INIT = 0,
    CMD_SM_STATE_SOF1_FOUND,
    CMD_SM_STATE_SOF2_FOUND,
    CMD_SM_STATE_PKTLEN_FOUND,
};

enum cmdsm_index
{
    CES_CMDIF_IND_LEN = 2,
    CES_CMDIF_IND_LEN_MSB,
    CES_CMDIF_IND_PKTTYPE,
    CES_CMDIF_PKT_OVERHEAD,
};

enum hpi_cmds
{
    HPI_CMD_GET_DEVICE_STATUS = 0x40,
    HPI_CMD_RESET = 0x41,
};

enum wiser_device_state
{
    HPI_STATUS_IDLE = 0x20,
    HPI_STATUS_BUSY = 0x21,
};

enum cmdif_pkt_type
{
    CES_CMDIF_TYPE_CMD = 0x01,
    CES_CMDIF_TYPE_DATA = 0x02,
    CES_CMDIF_TYPE_STATUS = 0x03,
    CES_CMDIF_TYPE_PROGRESS = 0x04,
    CES_CMDIF_TYPE_LOG_IDX = 0x05,
    CES_CMDIF_TYPE_CMD_RSP = 0x06,
};

enum ble_status
{
    BLE_STATUS_CONNECTED,
    BLE_STATUS_DISCONNECTED,
    BLE_STATUS_CONNECTING,
};

#define MAX_MSG_SIZE 32

struct hpi_cmd_data_obj_t
{
    uint8_t pkt_type;
    uint8_t data_len;
    uint8_t data[MAX_MSG_SIZE];
};

struct healthypi_time_t
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct healthypi_session_log_header_t
{
    // Log ID (0-9999)
    uint16_t log_id;
    uint16_t log_session_length;
    //Session start time
    struct healthypi_time_t log_start_time;

};

