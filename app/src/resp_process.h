#pragma once

void resp_filt(int16_t * RESP_WorkingBuff, int16_t * CoeffBuf, int16_t* FilterOut);
void resp_process_sample(int16_t *CurrAqsSample, int16_t *respFiltered);
void resp_algo_process(int16_t *CurrSample,volatile uint8_t *RespirationRate);
void resp_rate_detect(int16_t Resp_wave,volatile uint8_t *RespirationRate);
//void resp_remove_dc_component(int16_t *CurrAqsSample, int16_t *respFiltered);
int16_t resp_remove_dc_component(int16_t current_sample);

