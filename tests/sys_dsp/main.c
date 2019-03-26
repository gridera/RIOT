/*
 * Copyright (C) 2019 Unwired Devices [info@unwds.com]
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */
/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Short test application for Q15 fixed point FFT
 *
 * @author      Alexander Ugorelov <alex_u@unwds.com>
 *
 * @}
 */



/*
  Short test program to accompany fix_fft.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>


#define ENABLE_DEBUG (0)
#include "debug.h"

#include "dsp/int_fft_q15.h"

#ifndef M_PI
#   define M_PI 3.14159265359
#endif

void int_to_float_str(char *buf, int decimal, uint8_t precision) {  
    uint32_t i = 0;
    int divider = 1;
    char format[10] = { };
    char digits[3];
    
    buf[0] = 0;
    if (decimal < 0) {
        strcat(format, "-");
    }
    strcat(format, "%d.%0");
    
    for (i = 0; i<precision; i++) {
        divider *= 10;
    }

    snprintf(digits, 3, "%" PRIu32 "d", i);
    strcat(format, digits);
    
    snprintf(buf, 50, format, abs(decimal/divider), abs(decimal%divider));
}


int main(void) {

    uint32_t i;
    const uint32_t freq_sample = 400;
    const uint32_t fsine = 50;
    int16_t fx[FFT_SAMPLE * 2]; 

    for (i = FFT_SAMPLE; i < FFT_SAMPLE * 2; i++) {
        fx[i] = 0;
    }

    for (i = 0; i < FFT_SAMPLE; i++) {
        fx[i] = (32767 * sin(i * 2 * M_PI * fsine / freq_sample));
        fx[i] += (32767 / 3 * sin(i * 2 * M_PI * fsine * 3 / freq_sample));
    }

    hanning_window_q15(fx, FFT_SAMPLE);
    
    if (fft_q15(fx, fx + FFT_SAMPLE, log_base_2(FFT_SAMPLE), 0) < 0)
        return 1;

    uint16_t mag[FFT_SAMPLE];
    fft_magnitude_q15(fx, FFT_SAMPLE, mag);
    

    


    uint32_t fft_bin  = freq_to_fft_bin(freq_sample/2, freq_sample, FFT_SAMPLE);
    printf("fft bin for %ldHz is %ld\n", freq_sample/2, fft_bin);
    


    for(i = 0; i < fft_bin + 1 ; ++i) {
        uint8_t freq[10] = {0x00};
        float freq_cur = (float)(i * (float)(freq_sample / 2) / (float)(FFT_SAMPLE / 2));
        
        int_to_float_str((char *)freq, freq_cur, 3);

        printf("[%ld]\t%s[Hz]\t%hu\n", i, freq, mag[i]);
    }
       
    printf("Maximum magnitude: ");
    uint16_t max_mag;
    uint32_t fft_bin_max =  fft_bin_max_q15(mag, FFT_SAMPLE, &max_mag);
    printf("%ld[Hz]\t%hu\n", (uint32_t)fft_bin_to_freq(fft_bin_max, freq_sample, FFT_SAMPLE), max_mag);
    return 0;
}
