#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "algos.h"

int RESP_Second_Prev_Sample = 0;
int RESP_Prev_Sample = 0;
int RESP_Current_Sample = 0;
int RESP_Next_Sample = 0;
int RESP_Second_Next_Sample = 0;
uint8_t Respiration_Rate = 0;
int32_t RESP_WorkingBuff[2 * FILTERORDER];
int32_t Pvev_DC_Sample = 0, Pvev_Sample = 0;

static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];

const uint8_t uch_spo2_table[184] = {95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
                                     99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                     100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
                                     97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
                                     90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
                                     80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
                                     66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
                                     49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
                                     28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
                                     3, 2, 1};

int32_t RespCoeffBuf[FILTERORDER] = { 120,    124,    126,    127,    127,    125,    122,    118,    113,  /* Coeff for lowpass Fc=2Hz @ 125 SPS*/
                                      106,     97,     88,     77,     65,     52,     38,     24,      8,
                                       -8,    -25,    -42,    -59,    -76,    -93,   -110,   -126,   -142,
                                     -156,   -170,   -183,   -194,   -203,   -211,   -217,   -221,   -223,
                                     -223,   -220,   -215,   -208,   -198,   -185,   -170,   -152,   -132,
                                     -108,    -83,    -55,    -24,      8,     43,     80,    119,    159,
                                      201,    244,    288,    333,    378,    424,    470,    516,    561,
                                      606,    650,    693,    734,    773,    811,    847,    880,    911,
                                      939,    964,    986,   1005,   1020,   1033,   1041,   1047,   1049,
                                     1047,   1041,   1033,   1020,   1005,    986,    964,    939,    911,
                                      880,    847,    811,    773,    734,    693,    650,    606,    561,
                                      516,    470,    424,    378,    333,    288,    244,    201,    159,
                                      119,     80,     43,      8,    -24,    -55,    -83,   -108,   -132,
                                     -152,   -170,   -185,   -198,   -208,   -215,   -220,   -223,   -223,
                                     -221,   -217,   -211,   -203,   -194,   -183,   -170,   -156,   -142,
                                     -126,   -110,    -93,    -76,    -59,    -42,    -25,     -8,      8,
                                       24,     38,     52,     65,     77,     88,     97,    106,    113,
                                      118,    122,    125,    127,    127,    126,    124,    120       };

void hpi_estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{

  uint32_t un_ir_mean;
  int32_t k, n_i_ratio_count;
  int32_t i, n_exact_ir_valley_locs_count, n_middle_idx;
  int32_t n_th1, n_npks;
  int32_t an_ir_valley_locs[15];
  int32_t n_peak_interval_sum;
  int32_t n_y_ac, n_x_ac;
  int32_t n_spo2_calc;
  int32_t n_y_dc_max, n_x_dc_max;
  int32_t n_y_dc_max_idx = 0;
  int32_t n_x_dc_max_idx = 0;
  int32_t an_ratio[5], n_ratio_average;
  int32_t n_nume, n_denom;

  // calculates DC mean and subtract DC from ir
  un_ir_mean = 0;

  for (k = 0; k < n_ir_buffer_length; k++)
  {
    un_ir_mean += pun_ir_buffer[k];
  }

  un_ir_mean = un_ir_mean / n_ir_buffer_length;

  // remove DC and invert signal so that we can use peak detector as valley detector
  for (k = 0; k < n_ir_buffer_length; k++)
  {
    an_x[k] = -1 * (pun_ir_buffer[k] - un_ir_mean);
  }

  // 4 pt Moving Average
  for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++)
  {
    an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int)4;
  }

  // calculate threshold
  n_th1 = 0;

  for (k = 0; k < BUFFER_SIZE; k++)
  {
    n_th1 += an_x[k];
  }

  n_th1 = n_th1 / (BUFFER_SIZE);

  if (n_th1 < 30)
  {
    n_th1 = 30; // min allowed
  }

  if (n_th1 > 60)
  {
    n_th1 = 60; // max allowed
  }

  for (k = 0; k < 15; k++)
  {
    an_ir_valley_locs[k] = 0;
  }

  // since we flipped signal, we use peak detector as valley detector
  hpi_find_peak(an_ir_valley_locs, &n_npks, an_x, BUFFER_SIZE, n_th1, 4, 15); // peak_height, peak_distance, max_num_peaks
  n_peak_interval_sum = 0;

  if (n_npks >= 2)
  {

    for (k = 1; k < n_npks; k++)
    {
      n_peak_interval_sum += (an_ir_valley_locs[k] - an_ir_valley_locs[k - 1]);
    }

    n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
    // Serial.println(n_npks);

    *pn_heart_rate = (int32_t)((SF_spo2 * 60) / n_peak_interval_sum);
    *pch_hr_valid = 1;
  }
  else
  {
    *pn_heart_rate = -999; // unable to calculate because # of peaks are too small
    *pch_hr_valid = 0;
  }

  //  load raw value again for SPO2 calculation : RED(=y) and IR(=X)
  for (k = 0; k < n_ir_buffer_length; k++)
  {
    an_x[k] = pun_ir_buffer[k];
    an_y[k] = pun_red_buffer[k];
  }

  // find precise min near an_ir_valley_locs
  n_exact_ir_valley_locs_count = n_npks;

  // using exact_ir_valley_locs , find ir-red DC andir-red AC for SPO2 calibration an_ratio
  // finding AC/DC maximum of raw

  n_ratio_average = 0;
  n_i_ratio_count = 0;

  for (k = 0; k < 5; k++)
  {
    an_ratio[k] = 0;
  }

  for (k = 0; k < n_exact_ir_valley_locs_count; k++)
  {

    if (an_ir_valley_locs[k] > BUFFER_SIZE)
    {
      *pn_spo2 = -999; // do not use SPO2 since valley loc is out of range
      *pch_spo2_valid = 0;
      return;
    }
  }
  // find max between two valley locations
  // and use an_ratio betwen AC compoent of Ir & Red and DC compoent of Ir & Red for SPO2
  for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++)
  {
    n_y_dc_max = -16777216;
    n_x_dc_max = -16777216;

    if (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k] > 3)
    {

      for (i = an_ir_valley_locs[k]; i < an_ir_valley_locs[k + 1]; i++)
      {

        if (an_x[i] > n_x_dc_max)
        {
          n_x_dc_max = an_x[i];
          n_x_dc_max_idx = i;
        }

        if (an_y[i] > n_y_dc_max)
        {
          n_y_dc_max = an_y[i];
          n_y_dc_max_idx = i;
        }
      }
      n_y_ac = (an_y[an_ir_valley_locs[k + 1]] - an_y[an_ir_valley_locs[k]]) * (n_y_dc_max_idx - an_ir_valley_locs[k]); // red
      n_y_ac = an_y[an_ir_valley_locs[k]] + n_y_ac / (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k]);
      n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;                                                                           // subracting linear DC compoenents from raw
      n_x_ac = (an_x[an_ir_valley_locs[k + 1]] - an_x[an_ir_valley_locs[k]]) * (n_x_dc_max_idx - an_ir_valley_locs[k]); // ir
      n_x_ac = an_x[an_ir_valley_locs[k]] + n_x_ac / (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k]);
      n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac; // subracting linear DC compoenents from raw
      n_nume = (n_y_ac * n_x_dc_max) >> 7;    // prepare X100 to preserve floating value
      n_denom = (n_x_ac * n_y_dc_max) >> 7;

      if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0)
      {
        an_ratio[n_i_ratio_count] = (n_nume * 100) / n_denom; // formular is ( n_y_ac *n_x_dc_max) / ( n_x_ac *n_y_dc_max) ;
        n_i_ratio_count++;
      }
    }
  }
  // choose median value since PPG signal may varies from beat to beat
  hpi_sort_ascend(an_ratio, n_i_ratio_count);
  n_middle_idx = n_i_ratio_count / 2;

  if (n_middle_idx > 1)
  {
    n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2; // use median
  }
  else
  {
    n_ratio_average = an_ratio[n_middle_idx];
  }

  if (n_ratio_average > 2 && n_ratio_average < 184)
  {
    n_spo2_calc = uch_spo2_table[n_ratio_average];
    *pn_spo2 = n_spo2_calc;
    *pch_spo2_valid = 1; //  float_SPO2 =  -45.060*n_ratio_average* n_ratio_average/10000 + 30.354 *n_ratio_average/100 + 94.845 ;  // for comparison with table
  }
  else
  {
    *pn_spo2 = -999; // do not use SPO2 since signal an_ratio is out of range
    *pch_spo2_valid = 0;
  }
}

void hpi_find_peak(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
{
  hpi_find_peak_above(pn_locs, n_npks, pn_x, n_size, n_min_height);
  hpi_remove_close_peaks(pn_locs, n_npks, pn_x, n_min_distance);
  *n_npks = min(*n_npks, n_max_num);
}

void hpi_find_peak_above(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height)
{
  int32_t i = 1, n_width;
  *n_npks = 0;

  while (i < n_size - 1)
  {

    if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1])
    { // find left edge of potential peaks
      n_width = 1;

      while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width]) // find flat peaks
      {
        n_width++;
      }

      if (pn_x[i] > pn_x[i + n_width] && (*n_npks) < 15)
      { // find right edge of peaks
        pn_locs[(*n_npks)++] = i;
        // for flat peaks, peak location is left edge
        i += n_width + 1;
      }
      else
      {
        i += n_width;
      }
    }
    else
    {
      i++;
    }
  }
}

void hpi_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
{
  int32_t i, j, n_old_npks, n_dist; /* Order peaks from large to small */
  hpi_sort_indices_descend(pn_x, pn_locs, *pn_npks);

  for (i = -1; i < *pn_npks; i++)
  {
    n_old_npks = *pn_npks;
    *pn_npks = i + 1;

    for (j = i + 1; j < n_old_npks; j++)
    {
      n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]); // lag-zero peak of autocorr is at index -1

      if (n_dist > n_min_distance || n_dist < -n_min_distance)
      {
        pn_locs[(*pn_npks)++] = pn_locs[j];
      }
    }
  }
  // Resort indices int32_to ascending order
  hpi_sort_ascend(pn_locs, *pn_npks);
}

void hpi_sort_ascend(int32_t *pn_x, int32_t n_size)
{
  int32_t i, j, n_temp;

  for (i = 1; i < n_size; i++)
  {
    n_temp = pn_x[i];

    for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
    {
      pn_x[j] = pn_x[j - 1];
    }
    pn_x[j] = n_temp;
  }
}

void hpi_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
  int32_t i, j, n_temp;

  for (i = 1; i < n_size; i++)
  {
    n_temp = pn_indx[i];

    for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
    {
      pn_indx[j] = pn_indx[j - 1];
    }
    pn_indx[j] = n_temp;
  }
}

void Resp_FilterProcess(int32_t *RESP_WorkingBuff, int32_t *CoeffBuf, int32_t *FilterOut)
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
  *FilterOut = (int32_t)(acc >> 15);
}

int32_t Resp_ProcessCurrSample(int32_t CurrAqsSample)
{
  static uint32_t bufStart = 0, bufCur = FILTERORDER - 1;
  int32_t temp1, temp2; //, RESPData;
  int32_t RESPData;
  /* Count variable*/
  int32_t FiltOut;
  temp1 = NRCOEFF * Pvev_DC_Sample;
  Pvev_DC_Sample = (CurrAqsSample - Pvev_Sample) + temp1;
  Pvev_Sample = CurrAqsSample;
  temp2 = Pvev_DC_Sample;
  RESPData = (int32_t)temp2;
  RESPData = CurrAqsSample;
  /* Store the DC removed value in RESP_WorkingBuff buffer in millivolts range*/
  RESP_WorkingBuff[bufCur] = RESPData;
  Resp_FilterProcess(&RESP_WorkingBuff[bufCur], RespCoeffBuf, (int32_t *)&FiltOut);
  /* Store the DC removed value in Working buffer in millivolts range*/
  RESP_WorkingBuff[bufStart] = RESPData;
  /* Store the filtered out sample to the LeadInfo buffer*/
  bufCur++;
  bufStart++;

  if (bufStart >= (FILTERORDER - 1))
  {
    bufStart = 0;
    bufCur = FILTERORDER - 1;
  }

  return FiltOut;
}

void RESP_Algorithm_Interface(int32_t CurrSample, volatile uint32_t *RespirationRate)
{
  static int32_t prev_data[64] = {0};
  char i;
  long Mac = 0;
  prev_data[0] = CurrSample;

  for (i = 63; i > 0; i--)
  {
    Mac += prev_data[i];
    prev_data[i] = prev_data[i - 1];
  }

  Mac += CurrSample;
  CurrSample = (int32_t)Mac >> 1;
  RESP_Second_Prev_Sample = RESP_Prev_Sample;
  RESP_Prev_Sample = RESP_Current_Sample;
  RESP_Current_Sample = RESP_Next_Sample;
  RESP_Next_Sample = RESP_Second_Next_Sample;
  RESP_Second_Next_Sample = CurrSample; // << 3 ;
  Respiration_Rate_Detection(RESP_Second_Next_Sample, RespirationRate);
}

void Respiration_Rate_Detection(int32_t Resp_wave, volatile uint32_t *RespirationRate)
{
  static uint32_t skipCount = 0, SampleCount = 0, TimeCnt = 0, SampleCountNtve = 0, PtiveCnt = 0, NtiveCnt = 0;
  static int32_t MinThreshold = 0x7FFF, MaxThreshold = 0x8000, PrevSample = 0, PrevPrevSample = 0, PrevPrevPrevSample = 0;
  static int32_t MinThresholdNew = 0x7FFF, MaxThresholdNew = 0x8000, AvgThreshold = 0;
  static unsigned char startCalc = 0, PtiveEdgeDetected = 0, NtiveEdgeDetected = 0, peakCount = 0;
  static uint32_t PeakCount[8];
  SampleCount++;
  SampleCountNtve++;
  TimeCnt++;

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

        if (SampleCount > 40 && SampleCount < 700)
        {
          PtiveEdgeDetected = 1;
          PtiveCnt = SampleCount;
          skipCount = 4;
        }

        SampleCount = 0;
      }

      if (PrevPrevPrevSample < AvgThreshold && Resp_wave > AvgThreshold)
      {

        if (SampleCountNtve > 40 && SampleCountNtve < 700)
        {
          NtiveEdgeDetected = 1;
          NtiveCnt = SampleCountNtve;
          skipCount = 4;
        }

        SampleCountNtve = 0;
      }

      if (PtiveEdgeDetected == 1 && NtiveEdgeDetected == 1)
      {
        PtiveEdgeDetected = 0;
        NtiveEdgeDetected = 0;

        if (abs(PtiveCnt - NtiveCnt) < 5)
        {
          PeakCount[peakCount++] = PtiveCnt;
          PeakCount[peakCount++] = NtiveCnt;

          if (peakCount == 8)
          {
            peakCount = 0;
            PtiveCnt = PeakCount[0] + PeakCount[1] + PeakCount[2] + PeakCount[3] +
                       PeakCount[4] + PeakCount[5] + PeakCount[6] + PeakCount[7];
            PtiveCnt = PtiveCnt >> 3;
            Respiration_Rate = 6000 / PtiveCnt; // 60 * 125/SampleCount;
          }
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
    TimeCnt++;

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

  *RespirationRate=(uint32_t)Respiration_Rate;
}
