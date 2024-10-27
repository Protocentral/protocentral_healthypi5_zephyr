#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "resp_process.h"

int RESP_Second_Prev_Sample = 0;
int RESP_Prev_Sample = 0;
int RESP_Current_Sample = 0;
int RESP_Next_Sample = 0;
int RESP_Second_Next_Sample = 0;
uint8_t Respiration_Rate = 0;

#define FILTERORDER 161 /* DC Removal Numerator Coeff*/
#define NRCOEFF (0.992)

int16_t RESP_WorkingBuff[2 * FILTERORDER];
int16_t Pvev_DC_Sample = 0, Pvev_Sample = 0;

int16_t RespCoeffBuf[FILTERORDER] = {120, 124, 126, 127, 127, 125, 122, 118, 113, /* Coeff for lowpass Fc=2Hz @ 125 SPS*/
                                     106, 97, 88, 77, 65, 52, 38, 24, 8,
                                     -8, -25, -42, -59, -76, -93, -110, -126, -142,
                                     -156, -170, -183, -194, -203, -211, -217, -221, -223,
                                     -223, -220, -215, -208, -198, -185, -170, -152, -132,
                                     -108, -83, -55, -24, 8, 43, 80, 119, 159,
                                     201, 244, 288, 333, 378, 424, 470, 516, 561,
                                     606, 650, 693, 734, 773, 811, 847, 880, 911,
                                     939, 964, 986, 1005, 1020, 1033, 1041, 1047, 1049,
                                     1047, 1041, 1033, 1020, 1005, 986, 964, 939, 911,
                                     880, 847, 811, 773, 734, 693, 650, 606, 561,
                                     516, 470, 424, 378, 333, 288, 244, 201, 159,
                                     119, 80, 43, 8, -24, -55, -83, -108, -132,
                                     -152, -170, -185, -198, -208, -215, -220, -223, -223,
                                     -221, -217, -211, -203, -194, -183, -170, -156, -142,
                                     -126, -110, -93, -76, -59, -42, -25, -8, 8,
                                     24, 38, 52, 65, 77, 88, 97, 106, 113,
                                     118, 122, 125, 127, 127, 126, 124, 120};

void resp_filt(int16_t *RESP_WorkingBuff, int16_t *CoeffBuf, int16_t *FilterOut)
{
    int32_t acc = 0; // accumulator for MACs
    int k;
    // perform the multiply-accumulate
    for (k = 0; k < 161; k++)
    {
        acc += (int32_t)(*CoeffBuf++) * (int32_t)(*RESP_WorkingBuff--);
    }
    // saturate the result
    if (acc > 0x3fffffff)
    {
        acc = 0x3fffffff;
    }

    else if (acc < -0x40000000)
    {
        acc = -0x40000000;
    }

    // convert from Q30 to Q15
    *FilterOut = (int16_t)(acc >> 15);
}

void resp_process_sample(int16_t *CurrAqsSample, int16_t *respFiltered)
{
    static uint16_t bufStart = 0, bufCur = FILTERORDER - 1;
    int16_t temp1; //, RESPData;
    int16_t RESPData;
    int16_t FiltOut;

    for (int i = 0; i < 4; i++)
    {
        temp1 = NRCOEFF * Pvev_DC_Sample;
        Pvev_DC_Sample = (CurrAqsSample[i] - Pvev_Sample) + temp1;
        Pvev_Sample = CurrAqsSample[i];
        RESPData = (int16_t)Pvev_DC_Sample;

        /* Store the DC removed value in RESP_WorkingBuff buffer in millivolts range*/
        RESP_WorkingBuff[bufCur] = RESPData;
        resp_filt(&RESP_WorkingBuff[bufCur], RespCoeffBuf, (int16_t *)&FiltOut);
        /* Store the DC removed value in Working buffer in millivolts range*/
        RESP_WorkingBuff[bufStart] = RESPData;
        respFiltered[i] = FiltOut;
        /* Store the filtered out sample to the LeadInfo buffer*/
        bufCur++;
        bufStart++;

        if (bufStart >= (FILTERORDER - 1))
        {
            bufStart = 0;
            bufCur = FILTERORDER - 1;
        }
    }
}

void resp_algo_process(int16_t *CurrSample, volatile uint8_t *RespirationRate)
{

    static int16_t prev_data[64] = {0};
    long Mac = 0;

    for (int k = 0; k < 4; k++)
    {
        prev_data[3 - k] = CurrSample[k];
    }

    for (int i = 63; i > 0; i--)
    {
        Mac += prev_data[i];
        prev_data[i] = prev_data[i - 1];
    }

    Mac += CurrSample[0];

    resp_rate_detect((int16_t)Mac >> 1, RespirationRate);
}

void resp_rate_detect(int16_t Resp_wave, volatile uint8_t *RespirationRate)
{
    static uint16_t skipCount = 0, SampleCount = 0, TimeCnt = 0, SampleCountNtve = 0, PtiveCnt = 0;
    static int16_t MinThreshold = 0x7FFF, MaxThreshold = 0x8000, PrevSample = 0, PrevPrevSample = 0, PrevPrevPrevSample = 0;
    static int16_t MinThresholdNew = 0x7FFF, MaxThresholdNew = 0x8000, AvgThreshold = 0;
    static unsigned char startCalc = 0, PtiveEdgeDetected = 0, peakCount = 0;
    static uint16_t PeakCount[10];
    SampleCount += 4;
    TimeCnt += 4;

    if (Resp_wave < MinThresholdNew)
    {
        MinThresholdNew = Resp_wave;
    }

    if (Resp_wave > MaxThresholdNew)
    {
        MaxThresholdNew = Resp_wave;
    }

    if (SampleCount > 1000)
    {
        SampleCount = 0;
    }
    if (SampleCountNtve > 1000)
    {
        SampleCountNtve = 0;
    }

    if (startCalc == 1)
    {

        if (TimeCnt >= 500)
        {
            TimeCnt = 0;

            if ((MaxThresholdNew - MinThresholdNew) > 400)
            {
                MaxThreshold = MaxThresholdNew;
                MinThreshold = MinThresholdNew;
                AvgThreshold = MaxThreshold + MinThreshold;
                AvgThreshold = AvgThreshold >> 1;
            }
            else
            {

                startCalc = 0;
                Respiration_Rate = 0;
            }
        }
        PrevPrevPrevSample = PrevPrevSample;
        PrevPrevSample = PrevSample;
        PrevSample = Resp_wave;

        if (skipCount == 0)
        {

            if (PrevPrevPrevSample < AvgThreshold && Resp_wave > AvgThreshold)
            {
                PtiveEdgeDetected = 1;
                PtiveCnt = SampleCount;
                skipCount = 4;
                SampleCount = 0;
            }

            if (PtiveEdgeDetected == 1)
            {
                PtiveEdgeDetected = 0;

                PeakCount[peakCount++] = PtiveCnt;

                if (peakCount == 10)
                {
                    peakCount = 0;
                    PtiveCnt = PeakCount[0] + PeakCount[1] + PeakCount[2] + PeakCount[3] +
                               PeakCount[4] + PeakCount[5] + PeakCount[6] + PeakCount[7] + PeakCount[8] + PeakCount[9];
                    PtiveCnt = PtiveCnt >> 3;

                    peakCount = 9;
                    for (int k = 0; k < 9; k++)
                    {
                        PeakCount[k] = PeakCount[k + 1];
                    }

                    Respiration_Rate = 6000 / PtiveCnt; // 60 * 125/SampleCount;
                }
            }
        }
        else
        {
            skipCount--;
        }
    }
    else
    {

        if (TimeCnt >= 500)
        {
            TimeCnt = 0;

            if ((MaxThresholdNew - MinThresholdNew) > 400)
            {
                startCalc = 1;
                MaxThreshold = MaxThresholdNew;
                MinThreshold = MinThresholdNew;
                AvgThreshold = MaxThreshold + MinThreshold;
                AvgThreshold = AvgThreshold >> 1;
                PrevPrevPrevSample = Resp_wave;
                PrevPrevSample = Resp_wave;
                PrevSample = Resp_wave;
            }
        }
    }

    *RespirationRate = (uint8_t)Respiration_Rate;
}
