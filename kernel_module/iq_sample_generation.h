/*
 * rpitx_alsa module
 * Author: Kevin "felixzero" Guilloy, F4VQG
 * 
 * This file converts real sample (USB) to complex I-Q
 * using a finite impulse response filter as an approximation
 * of the Hilbert transform to generate the Q samples.
 * 
 * This induces an extra latency of one buffer.
 * 
 * This file is licensed under GNU GPL v3.
 */

#ifndef IQ_SAMPLE_GENERATION_H
#define IQ_SAMPLE_GENERATION_H

#include "alsa_handling.h"

/* Reset the saved buffers to a zero-ed state */
void clear_iq_sample_generation(void);

/* Compute the Hilbert transform of one sample.
 * in_buffer is assumed to be a real buffer of S16_LE samples.
 * out_buffer is assumed to be a complex buffer of S16_LE * 2 samples.
 * in_buffer must be exactly PERIOD_BYTES and out_buffer double. */
void process_iq_period(char __user *out_buffer, const char *in_buffer);

#endif



