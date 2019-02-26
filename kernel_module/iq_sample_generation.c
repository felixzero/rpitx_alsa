#include "iq_sample_generation.h"

#include <linux/types.h>
#include <sound/pcm.h>

#define NUMBER_OF_SAMPLES (PERIOD_BYTES / 2)

/* The FIR approximation of the Hilber transform is defined as:
 * out = h conv. in
 * 
 * With h(n) = 2 / (n*pi) with n odd
 * and h(n) = 0 with n even
*/
#define TWO_OVER_PI 41722 /* = 2 / pi * 2^16 */
#define FIR_HILBERT(i, x) \
    ( ((NUMBER_OF_SAMPLES - i) % 2 == 0) ? \
        0 \
        : (TWO_OVER_PI * (int32_t)x / (i - NUMBER_OF_SAMPLES)))

/* This assumes the current implementation is *little endian*. */
static int16_t period1[NUMBER_OF_SAMPLES], period2[NUMBER_OF_SAMPLES];

void clear_iq_sample_generation(void)
{
    memset(period1, 0, PERIOD_BYTES);
    memset(period2, 0, PERIOD_BYTES);
}

void process_iq_period(char __user *out_buffer, const char *in_buffer)
{
    int i, j;
    int32_t q_sample;
    
    const int16_t *period3 = (int16_t*)in_buffer;
    int16_t __user *iq_data = (int16_t*)out_buffer;
    
    for (i = 0; i < NUMBER_OF_SAMPLES; i++) {
        q_sample = 0;
        for (j = i; j < NUMBER_OF_SAMPLES; j++)
            q_sample += FIR_HILBERT(j - i, period1[j]);
        for (j = 0; j < NUMBER_OF_SAMPLES; j++)
            q_sample += FIR_HILBERT(j - i + NUMBER_OF_SAMPLES, period2[j]);
        for (j = 0; j < i; j++)
            q_sample += FIR_HILBERT(j - i + 2 * NUMBER_OF_SAMPLES, period3[j]);
        
        put_user(period2[i], iq_data + 2 * i);
        put_user((int16_t)((-q_sample) >> 16), iq_data + 2 * i + 1);
    }
    
    for (i = 0; i < NUMBER_OF_SAMPLES; i++) {
        period1[i] = period2[i];
        period2[i] = period3[i];
    }
}



