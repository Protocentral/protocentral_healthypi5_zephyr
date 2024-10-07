#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <arm_math.h>
#include <math.h>

#include "data_module.h"
#include "resp_process.h"


arm_fir_instance_f32 firf32;
volatile uint8_t rr_Cto_RespirationRate = 0;
struct bioz_signal_parameters bioz_signal;

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

void cmsis_interp1d(int32_t *x_data, int32_t *y_data, int32_t *interp_points, float *interp_values)
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

void lpf_to_exclude_resp(int32_t *sig, int32_t *wins_t_start, float *lpf_sig, int32_t lpf_sig_fs)
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

// Function to compare two doubles for sorting
int compare(const void *a, const void *b) {
    return (*(double *)a > *(double *)b) - (*(double *)a < *(double *)b);
}

// Function to calculate the q-th quantile
int32_t calculate_quantile(int32_t *data, float q) {
    int32_t n = sizeof(data)/sizeof(int32_t);

    // Create a sorted copy of the data
    int32_t *sorted = malloc(n * sizeof(int32_t));

    for (size_t i = 0; i < n; i++) {
        sorted[i] = data[i];
    }

    // Sort the array
    qsort(sorted, n, sizeof(int32_t), compare);

    // Calculate the index for the quantile
    double index = (n - 1) * q;
    size_t lower_index = (size_t)index;
    double fractional_part = index - lower_index;

    int32_t quantile_value = sorted[lower_index];
    if (lower_index + 1 < n) {
        quantile_value += fractional_part * (sorted[lower_index + 1] - sorted[lower_index]);
    }

    free(sorted); // Free the sorted array
    return quantile_value;
}


void ref_cto_mod(int32_t *v,float *t)
{
    int peaks_count=0,rel_peaks_count=0,troughs_count=0,rel_troughs_count=0;
    int32_t peaks[] = {};
    int32_t peaks_value[] = {};
    int32_t troughs[] ={};
    int32_t troughs_value[]={};
    int32_t rel_peaks[]={};
    int32_t rel_troughs[]= {};

    // Identify relevant peaks and troughs
    int32_t length = sizeof(v)/sizeof(int32_t);
    int32_t* d_s_win = (int32_t*) malloc(sizeof(v) * sizeof(int32_t));  // Allocate memory for the array    
    cmsis_detrend(v, d_s_win);
    
    for (int i=0;i<length;i++)
    {
        v[i] = -1 * d_s_win[i];
    }

    double *diffs = malloc((length - 1) * sizeof(double));
    for (size_t i = 1; i < length; i++) {
        diffs[i - 1] = v[i] - v[i - 1];
    }

    for (int i = 1; i < length - 1; i++) {
        if (diffs[i - 1] > 0 && diffs[i] < 0) {
            peaks[peaks_count] = i; // Store the index of the peak
            peaks_value[peaks_count++] = v[i];
        }
        else if (diffs[i - 1] < 0 && diffs[i] > 0) {
            troughs[troughs_count] = i; // Store the index of the trough
            troughs_value[troughs_count++] = v[i];
        }
    }

    free(diffs);

    // Define peaks threshold
    int32_t q2 = calculate_quantile(peaks_value, 0.75);
    float thresh_1 = 0.2 * q2;
    for (int i=0;i<=peaks_count; i++)
    {
        if (v[peaks[i]] > thresh_1)
            rel_peaks[i] = peaks[rel_peaks_count++];
    }

    // Define troughs threshold
    int32_t q3 = calculate_quantile(troughs_value, 0.25);
    float thresh_2 = 0.2 * q3;
    for (int i=0;i<=troughs_count; i++)
    {
        if (v[troughs[i]] < thresh_2)
            rel_troughs[i] = troughs[rel_troughs_count++];
    }

    //Find valid breathing cycles
    find_valid_cycles_and_rr(v, rel_peaks, rel_peaks_count, rel_troughs, rel_troughs_count,t,&rr_Cto_RespirationRate);
}

void find_valid_cycles_and_rr(float *v, int *rel_peaks, int rel_peak_count, int *rel_troughs, int rel_trough_count, float *t, float *rr_cto)
{
    bool *invalid_peaks = (bool *)malloc(rel_peak_count * sizeof(bool));
    for (int i = 0; i < rel_peak_count; i++) {
        invalid_peaks[i] = false;
    }

    // Loop over pairs of troughs and find peaks in between
    for (int trough_pair_no = 0; trough_pair_no < rel_trough_count - 1; trough_pair_no++) {
        int first_trough = rel_troughs[trough_pair_no];
        int second_trough = rel_troughs[trough_pair_no + 1];

        // Find peaks between the two troughs
        int cycle_peak_indices[rel_peak_count];
        int cycle_peak_count = 0;

        for (int i = 0; i < rel_peak_count; i++) {
            if (rel_peaks[i] > first_trough && rel_peaks[i] < second_trough) {
                cycle_peak_indices[cycle_peak_count++] = i;
            }
        }

        // If more than one peak, mark the unnecessary peaks as invalid
        if (cycle_peak_count > 1) {
            int max_peak_index = cycle_peak_indices[0];
            for (int i = 1; i < cycle_peak_count; i++) {
                if (v[rel_peaks[cycle_peak_indices[i]]] > v[rel_peaks[max_peak_index]]) {
                    max_peak_index = cycle_peak_indices[i];
                }
            }

            // Mark all peaks other than the max peak as invalid
            for (int i = 0; i < cycle_peak_count; i++) {
                if (cycle_peak_indices[i] != max_peak_index) {
                    invalid_peaks[cycle_peak_indices[i]] = true;
                }
            }
        }
    }

    // Filter out the invalid peaks
    int valid_peak_count = 0;
    int valid_peaks[rel_peak_count];
    for (int i = 0; i < rel_peak_count; i++) {
        if (!invalid_peaks[i]) {
            valid_peaks[valid_peak_count++] = rel_peaks[i];
        }
    }

    // Handle initial peaks (ensure valid cycles start with a peak)
    if (valid_peaks[0] < rel_troughs[0] && valid_peak_count > 1) {
        int max_initial_peak = 0;
        for (int i = 1; i < valid_peak_count; i++) {
            if (v[valid_peaks[i]] > v[valid_peaks[max_initial_peak]]) {
                max_initial_peak = i;
            }
        }

        valid_peaks[0] = valid_peaks[max_initial_peak];
    }

    // Calculate valid cycle durations
    int valid_cycle_count = 0;
    float cycle_durations[valid_peak_count - 1];
    for (int i = 1; i < valid_peak_count; i++) {
        // Ensure there's a trough between two peaks
        for (int j = 0; j < rel_trough_count; j++) {
            if (rel_troughs[j] > valid_peaks[i - 1] && rel_troughs[j] < valid_peaks[i]) {
                cycle_durations[valid_cycle_count++] = t[valid_peaks[i]] - t[valid_peaks[i - 1]];
                break;
            }
        }
    }

    // Calculate RR from valid cycle durations
    if (valid_cycle_count != 0) {
        float ave_breath_duration = average(cycle_durations, valid_cycle_count);
        *rr_cto = 60.0 / ave_breath_duration;
    }

    // Clean up memory
    free(invalid_peaks);
}

int *arange(int start, int end, int step) {
    // Calculate the size of the resulting array
    int len = (end - start) / step + 1;
    
    // Allocate memory for the array
    int *arr = (int *)malloc(len * sizeof(int));
    
    // Fill the array with values from start to end (inclusive)
    for (int i = 0; i < len; i++) {
        arr[i] = start + i * step;
    }
    
    return arr;
}

// Function to calculate the median of an array excluding NaNs
float calculate_median(float* data, int size) {
    // Copy valid elements (non-NaN) to a temporary array
    double* valid_data = (double*)malloc(size * sizeof(double));
    int valid_count = 0;
    
    for (int i = 0; i < size; i++) {
        if (!isnan(data[i])) {
            valid_data[valid_count++] = data[i];
        }
    }
    
    // Sort the valid data
    for (int i = 0; i < valid_count - 1; i++) {
        for (int j = i + 1; j < valid_count; j++) {
            if (valid_data[i] > valid_data[j]) {
                double temp = valid_data[i];
                valid_data[i] = valid_data[j];
                valid_data[j] = temp;
            }
        }
    }
    
    // Calculate median
    double median;
    if (valid_count % 2 == 0) {
        median = (valid_data[valid_count/2 - 1] + valid_data[valid_count/2]) / 2.0;
    } else {
        median = valid_data[valid_count/2];
    }
    
    free(valid_data);
    return median;
}

void replace_nan_with_median(double* data, int size) {
    double median = calculate_median(data, size);
    
    for (int i = 0; i < size; i++) {
        if (isnan(data[i])) {
            data[i] = median;
        }
    }
}









