/*
 * rpitx_alsa module
 * Author: Kevin "felixzero" Guilloy, F4VQG
 * 
 * This file handles the ALSA PCM interface. It declares one device with 2 PCMs:
 *  - number 0 is stereo only and takes I-Q samples
 *  - number 1 is mono only and takes (already pre-filtered) USB samples
 * 
 * This file is licensed under GNU GPL v3.
 */

#ifndef ALSA_HANDLING_H
#define ALSA_HANDLING_H

#define PERIOD_BYTES 256

/* To be called at initialization of the module to init the sound devices. */
int rpitx_init_alsa_system(void);

/* to be called at the release of the module to free resources. */
void rpitx_unregister_alsa(void);

/* 
 * Read ALSA's buffer.
 * buffer is the destination.
 * len is the size to read. It need to be at least PERIOD_BYTE long to
 * trigger read. Otherwises does nothing.
 * 
 * Will copy exactly PERIOD_BYTE bytes, if available.
 * Will return the number of byte read.
 */
ssize_t rpitx_read_bytes_from_alsa_buffer(char *buffer, size_t len);

#endif



