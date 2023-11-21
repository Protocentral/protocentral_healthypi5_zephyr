#pragma once

#define SF_spo2 25 // sampling frequency
#define BUFFER_SIZE (SF_spo2 * 4)
#define MA4_SIZE 4 // DONOT CHANGE
#define min(x, y) ((x) < (y) ? (x) : (y))

#define FILTERORDER 161 /* DC Removal Numerator Coeff*/
#define NRCOEFF (0.992)

void hpi_estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
void hpi_find_peak(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num);
void hpi_find_peak_above(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height);
void hpi_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance);
void hpi_sort_ascend(int32_t *pn_x, int32_t n_size);
void hpi_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);

void Resp_FilterProcess(int32_t * RESP_WorkingBuff, int32_t * CoeffBuf, int32_t* FilterOut);
int32_t Resp_ProcessCurrSample(int32_t CurrAqsSample);
void RESP_Algorithm_Interface(int32_t CurrSample,volatile uint32_t *RespirationRate);
void Respiration_Rate_Detection(int32_t Resp_wave,volatile uint32_t *RespirationRate);
