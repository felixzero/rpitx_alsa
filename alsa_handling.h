/*
 * 
 */

#ifndef ALSA_HANDLING_H
#define ALSA_HANDLING_H

int rpitx_init_alsa_system(void);
void rpitx_unregister_alsa(void);
ssize_t rpitx_read_bytes_from_alsa_buffer(char *buffer, size_t len);

#endif



