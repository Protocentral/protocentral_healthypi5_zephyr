#pragma once

#define FILE_TRANSFER_BLE_PACKET_SIZE 64

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_PKT_STOP_1 0x00
#define CES_CMDIF_PKT_STOP_2 0x0B


#define CMD_LOGGING_MEMORY_FREE 0x32 //tested
#define CMD_LOGGING_MEMORY_NOT_AVAILABLE 0x31
#define CMD_LOGGING_END 0x56 //tested
#define CMD_LOGGING_START 0x55 //tested
#define CMD_LOG_GET_COUNT 0x54  //tested
#define CMD_FETCH_LOG_FILE_DATA 0x51 //
#define CMD_LOG_FILE_HEADER 0x50
#define CMG_LOG_FILE_DELETE 0x52 //tested
#define CMD_LOG_WIPE_ALL 0x53 //tested

void cmdif_send_ble_progress(uint8_t m_stage, uint16_t m_total_time, uint16_t m_curr_time, uint16_t m_current, uint16_t m_imped);
void cmdif_send_ble_command(uint8_t m_cmd);
void cmdif_send_ble_device_status_response(void);

//void cmdif_send_ble_data(const char *buf, size_t len);
void cmdif_send_ble_file_data(int8_t *m_data, uint8_t m_data_len);
//void cmdif_send_ble_file_data(uint8_t *m_data, uint32_t number_writes,uint8_t m_data_len);

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
    uint16_t session_id;
    uint16_t session_size;
    struct healthypi_time_t session_start_time;

};

struct hpi_sensor_logging_data_t {
    int32_t log_ecg_sample;
    //int16_t raw_ir;
};

struct healthypi_session_t {
    uint16_t session_id;
};

