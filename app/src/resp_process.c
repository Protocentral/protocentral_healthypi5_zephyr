#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <arm_math.h>
#include <math.h>


#include "resp_process.h"

int RESP_Second_Prev_Sample = 0;
int RESP_Prev_Sample = 0;
int RESP_Current_Sample = 0;
int RESP_Next_Sample = 0;
int RESP_Second_Next_Sample = 0;

uint8_t Respiration_Rate = 0;

#define FILTERORDER 161 /* DC Removal Numerator Coeff*/
#define NRCOEFF (0.992)
struct bioz_signal_parameters bioz_signal;
arm_fir_instance_f32 firf32;


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

void cmsis_detrend(int32_t *resp_signal,int32_t *detrended_signal)
{
    int32_t length,mean_signal,mean_time,cov_st=0,var_t=0,intercept,slope;
    length = sizeof(resp_signal)/sizeof(int32_t);
    int32_t trend[length];

    float* time = (float*) malloc(length * sizeof(float));  // Allocate memory for the array    
    for (int i = 0; i < length; i++) {
        time[i] = (float)i;  // Populate array with sequential floats
    }

    for (int i=0;i<length;i++)
    {
        mean_time += time[i];
        mean_signal += resp_signal[i];
    }


    for (int k=0;k<length;k++)
    {
        cov_st += (time[k] - mean_time) * (resp_signal[k] - mean_signal);
        var_t += pow((time[k] - mean_time),2);
    }

    slope = cov_st / var_t;
    intercept = mean_signal - slope * mean_time;

    // Generate the trend (best fit line)
    for (int i=0;i<length;i++)
    {
        trend[i] = slope * time[i] + intercept;
        detrended_signal[i] = resp_signal[i] - trend[i];
    }
}

int32_t linear_interpolate (int32_t x, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
    if (x2 == x1)
        return y1;  //Prevent division by zero
    return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}


//    int32_t* interp_values = (int32_t*) malloc(length * sizeof(int32_t));  // Allocate memory for the array    
void cmsis_interp1d(int32_t *x_data,int32_t *y_data,int32_t *interp_points, int32_t *interp_values)
{
    int32_t length = sizeof(interp_points)/sizeof(int32_t);
    for (int i = 0; i < length; i++) {
        interp_values[i] = 0;  // Populate array with sequential floats
    }

    for (int i=0;i<length;i++)
    {
        for (int j=0;j<(sizeof(x_data)/sizeof(int32_t))-1;j++)
        {
            if (x_data[j] <= interp_points[i] && interp_points[i] <= x_data[j + 1])
            {
                interp_values[i] = linear_interpolate(interp_points[i], x_data[j], x_data[j + 1], y_data[j], y_data[j + 1]);
                break;
            }
            else
            {
                if (interp_points[i] < x_data[0])
                    interp_values[i] = linear_interpolate(interp_points[i], x_data[0], x_data[1], y_data[0], y_data[1]);
                else if (interp_points[i] > x_data[-1])                    
                    interp_values[i] = linear_interpolate(interp_points[i], x_data[-2], x_data[-1], y_data[-2], y_data[-1]);
            }
        }
    }
}

void cmsis_tukey_window (int32_t length,int32_t alpha, int32_t *tukey_win)
{
    int32_t middle_region_start,middle_region_end;

    middle_region_start = (int)alpha * (length - 1) / 2;
    middle_region_end = length - middle_region_start;

    //Apply cosine taper in the first region
    for (int n=0;n<middle_region_start;n++)
    {
        tukey_win[n] = 0.5 * (1 + cos(PI * ((2 * n) / (alpha * (length - 1)) - 1)));
    }

    // Middle region is flat
    for (int n=middle_region_start;n<middle_region_end;n++)
    {
        tukey_win[n] = 1;
    }

    // Apply cosine taper in the last region
    for (int n=middle_region_end;n<length;n++)
    {
        tukey_win[n] = 0.5 * (1 + cos(PI * ((2 * n) / (alpha * (length - 1)) - 2 / alpha + 1)));

    }
}

void lp_filter_signal_to_remove_freqs_above_resp(int32_t *s,int32_t s_filt)
{
    int32_t blockSize = sizeof(s)/sizeof(int32_t);
    int32_t numTaps = 10;
    int32_t stateLength = numTaps + blockSize - 1;

    float *coefficients = (float*) malloc(numTaps * sizeof(float));  // Allocate memory for coefficients
    float *state = (float*) malloc(stateLength * sizeof(float));  // Allocate memory for coefficients

    // Fill the coefficients array
    for (int i = 0; i < numTaps; i++) {
        coefficients[i] = 1.0 / numTaps;  // Equivalent to np.ones(numTaps) / numTaps
    }

    for (int i = 0; i < stateLength; i++) {
        state[i] = 0;  // Equivalent to np.ones(numTaps) / numTaps
    }

    arm_fir_init_f32(&firf32, numTaps, coefficients, state,blockSize);

    arm_fir_f32(&firf32,s,s_filt,blockSize);
}

void lpf_to_exclude_resp(int32_t *sig, int32_t wins_t_start,float *lpf_sig,int32_t lpf_sig_fs)
{
    int32_t duration_of_signal = sizeof(sig)/sizeof(int32_t);
    float prop_of_win_in_outer_regions = 2 * 2 / duration_of_signal;
    float* tukey_win = (float*) malloc(sizeof(sig) * sizeof(float));  // Allocate memory for the array    
    int32_t* d_s_win = (int32_t*) malloc(sizeof(sig) * sizeof(int32_t));  // Allocate memory for the array    
    float *s_filt = (float*) malloc(sizeof(sig) * sizeof(float));


    for (int i = 0; i < duration_of_signal; i++) {
        tukey_win[i] = 0;  // Populate array with sequential floats
    }

    cmsis_tukey_window(duration_of_signal, prop_of_win_in_outer_regions,&tukey_win);
    cmsis_detrend(sig,d_s_win);
    lp_filter_signal_to_remove_freqs_above_resp(d_s_win,s_filt);
}

/*void ref_cto_mod(struct bioz_signal_parameters bioz_signal)
{
    bioz_signal.v
}*/




