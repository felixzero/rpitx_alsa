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

#include "sysfs_variable.h"

#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/device.h>

#define FOLDER_NAME "rpitx"

static struct device *sys_dev;
static struct kobject *root_folder;

static unsigned int frequency = 14000000;
static unsigned int harmonic = 1;

static ssize_t frequency_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", frequency);
}

static ssize_t frequency_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%du", &frequency);
    return count;
}

static ssize_t harmonic_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", harmonic);
}

static ssize_t harmonic_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%du", &harmonic);
    return count;
}

static struct kobj_attribute frequency_attr = __ATTR(frequency, 0664, frequency_show, frequency_store);
static struct kobj_attribute harmonic_attr  = __ATTR(harmonic,  0664, harmonic_show,  harmonic_store);

int rpitx_init_sysfs_variables(void)
{
    int err;
    
    sys_dev = root_device_register(FOLDER_NAME);
    root_folder = &sys_dev->kobj;

    err = sysfs_create_file(root_folder, &frequency_attr.attr);
    if (err < 0)
        return err;
    err = sysfs_create_file(root_folder, &harmonic_attr.attr);
    if (err < 0)
        return err;
    
    return 0;
}

void rpitx_unregister_sysfs_variables(void)
{
    root_device_unregister(sys_dev);
}




