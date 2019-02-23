/*
 * rpitx_alsa module
 * Author: Kevin "felixzero" Guilloy, F4VQG
 * 
 * This file handles the variable files /sys/devices/rpitx/.
 * It defines the following:
 *  /sys/devices/rpitx/frequency --> rpitx center frequency in Hz
 * /sys/devices/rpitx/harmonic --> harmonic to use (default: 1)
 * 
 * This file is licensed under GNU GPL v3.
 */

#ifndef SYSFS_VARIABLE
#define SYSFS_VARIABLE

/* To be called at initialization of the module to init the /sys node. */
int rpitx_init_sysfs_variables(void);

/* to be called at the release of the module to free resources. */
void rpitx_unregister_sysfs_variables(void);

#endif


