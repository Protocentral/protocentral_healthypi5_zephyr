#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <arm_math.h>
#include <math.h>

#include "data_module.h"

arm_fir_instance_f32 firf32;

//static int16_t resp_signal_buffer[RESP_RREST_BUFF_SIZE];

void hpi_rrest_detrend(int32_t *resp_signal, int32_t *detrended_signal)
{
    int32_t length, mean_signal, mean_time, cov_st = 0, var_t = 0, intercept, slope;
    length = sizeof(resp_signal) / sizeof(int32_t);
    int32_t trend[length];

    float *time = (float *)malloc(length * sizeof(float)); // Allocate memory for the array
    for (int i = 0; i < length; i++)
    {
        time[i] = (float)i; // Populate array with sequential floats
    }

    for (int i = 0; i < length; i++)
    {
        mean_time += time[i];
        mean_signal += resp_signal[i];
    }

    for (int k = 0; k < length; k++)
    {
        cov_st += (time[k] - mean_time) * (resp_signal[k] - mean_signal);
        var_t += pow((time[k] - mean_time), 2);
    }

    slope = cov_st / var_t;
    intercept = mean_signal - slope * mean_time;

    // Generate the trend (best fit line)
    for (int i = 0; i < length; i++)
    {
        trend[i] = slope * time[i] + intercept;
        detrended_signal[i] = resp_signal[i] - trend[i];
    }
}

int32_t linear_interpolate(int32_t x, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
    if (x2 == x1)
        return y1; // Prevent division by zero
    return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

//    int32_t* interp_values = (int32_t*) malloc(length * sizeof(int32_t));  // Allocate memory for the array
void cmsis_interp1d(int32_t *x_data, int32_t *y_data, int32_t *interp_points, int32_t *interp_values)
{
    int32_t length = sizeof(interp_points) / sizeof(int32_t);
    for (int i = 0; i < length; i++)
    {
        interp_values[i] = 0; // Populate array with sequential floats
    }

    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < (sizeof(x_data) / sizeof(int32_t)) - 1; j++)
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

void hpi_rrest_tukey_window(int32_t length, int32_t alpha, int32_t *tukey_win)
{
    int32_t middle_region_start, middle_region_end;

    middle_region_start = (int)alpha * (length - 1) / 2;
    middle_region_end = length - middle_region_start;

    // Apply cosine taper in the first region
    for (int n = 0; n < middle_region_start; n++)
    {
        tukey_win[n] = 0.5 * (1 + cos(PI * ((2 * n) / (alpha * (length - 1)) - 1)));
    }

    // Middle region is flat
    for (int n = middle_region_start; n < middle_region_end; n++)
    {
        tukey_win[n] = 1;
    }

    // Apply cosine taper in the last region
    for (int n = middle_region_end; n < length; n++)
    {
        tukey_win[n] = 0.5 * (1 + cos(PI * ((2 * n) / (alpha * (length - 1)) - 2 / alpha + 1)));
    }
}

void lp_filter_signal_to_remove_freqs_above_resp(int32_t *s, int32_t s_filt)
{
    int32_t blockSize = sizeof(s) / sizeof(int32_t);
    int32_t numTaps = 10;
    int32_t stateLength = numTaps + blockSize - 1;

    float *coefficients = (float *)malloc(numTaps * sizeof(float)); // Allocate memory for coefficients
    float *state = (float *)malloc(stateLength * sizeof(float));    // Allocate memory for coefficients

    // Fill the coefficients array
    for (int i = 0; i < numTaps; i++)
    {
        coefficients[i] = 1.0 / numTaps; // Equivalent to np.ones(numTaps) / numTaps
    }

    for (int i = 0; i < stateLength; i++)
    {
        state[i] = 0; // Equivalent to np.ones(numTaps) / numTaps
    }

    arm_fir_init_f32(&firf32, numTaps, coefficients, state, blockSize);

    arm_fir_f32(&firf32, s, s_filt, blockSize);
}

void lpf_to_exclude_resp(int32_t *sig, int32_t wins_t_start, float *lpf_sig, int32_t lpf_sig_fs)
{
    int32_t duration_of_signal = sizeof(sig) / sizeof(int32_t);
    float prop_of_win_in_outer_regions = 2 * 2 / duration_of_signal;
    float *tukey_win = (float *)malloc(sizeof(sig) * sizeof(float));     // Allocate memory for the array
    int32_t *d_s_win = (int32_t *)malloc(sizeof(sig) * sizeof(int32_t)); // Allocate memory for the array
    float *s_filt = (float *)malloc(sizeof(sig) * sizeof(float));

    for (int i = 0; i < duration_of_signal; i++)
    {
        tukey_win[i] = 0; // Populate array with sequential floats
    }

    hpi_rrest_tukey_window(duration_of_signal, prop_of_win_in_outer_regions, &tukey_win);
    hpi_rrest_detrend(sig, d_s_win);
    lp_filter_signal_to_remove_freqs_above_resp(d_s_win, s_filt);
}

/*void ref_cto_mod(struct bioz_signal_parameters bioz_signal)
{
    bioz_signal.v
}*/
