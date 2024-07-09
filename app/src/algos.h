#pragma once

#define SF_spo2 25 // sampling frequency
#define BUFFER_SIZE (SF_spo2 * 4)
#define MA4_SIZE 4 // DONOT CHANGE
#define min(x, y) ((x) < (y) ? (x) : (y))

#define FILTERORDER 161 /* DC Removal Numerator Coeff*/
#define NRCOEFF (0.992)
#define HRV_LIMIT 10

//void hpi_estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
void hpi_estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, uint16_t power_ir_average,int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
void hpi_find_peak(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num);
void hpi_find_peak_above(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height);
void hpi_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance);
void hpi_sort_ascend(int32_t *pn_x, int32_t n_size);
void hpi_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);

void Resp_FilterProcess(int16_t * RESP_WorkingBuff, int16_t * CoeffBuf, int16_t* FilterOut);
int16_t Resp_ProcessCurrSample(int16_t CurrAqsSample);
void RESP_Algorithm_Interface(int16_t CurrSample,volatile uint8_t *RespirationRate);
void Respiration_Rate_Detection(int16_t Resp_wave,volatile uint8_t *RespirationRate);

void calculate_pnn_rmssd(unsigned int array[], float *pnn50, float *rmssd);
float calculate_sdnn(unsigned int array[]);
float calculate_mean(unsigned int array[]);
int calculate_hrvmin(unsigned int array[]);
int calculate_hrvmax(unsigned int array[]);
void calculate_hrv (int32_t heart_rate, int32_t *hrv_max, int32_t *hrv_min, float *mean, float *sdnn, float *pnn, float *rmssd, bool *hrv_ready_flag);
