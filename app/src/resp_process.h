#pragma once

#define RESP_FREQ 64
#define RESP_ALGO_WINDOW_SIZE 2048
#define RESP_DOWN_SAMPLE_SIZE 160
#define PI 3.14159

void resp_filt(int16_t * RESP_WorkingBuff, int16_t * CoeffBuf, int16_t* FilterOut);
void resp_process_sample(int16_t *CurrAqsSample, int16_t *respFiltered);
void resp_algo_process(int16_t *CurrSample,volatile uint8_t *RespirationRate);
void resp_rate_detect(int16_t Resp_wave,volatile uint8_t *RespirationRate);

struct bioz_signal_parameters
{
    int32_t filtered_signal[RESP_DOWN_SAMPLE_SIZE];
    int32_t bioz_signal[RESP_ALGO_WINDOW_SIZE];
};