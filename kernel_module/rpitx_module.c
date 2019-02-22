/*
 * rpitx_alsa module
 * Author: Kevin "felixzero" Guilloy, F4VQG
 * 
 * This file declares the module to the kernel.
 * It also handles the /dev/rpitxin device (its output).
 * 
 * This file is licensed under GNU GPL v3.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include "alsa_handling.h"

/* Name definition */
#define CHARDEV_NAME "rpitxin"
#define CLASS_NAME "rpitx"

/* Module definition */
MODULE_AUTHOR("Kevin Guilloy");
MODULE_DESCRIPTION("ALSA module for rpitx");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{ALSA,raspberrypi}}");

static int major_number;
static struct class *chardev_class = NULL;
static struct device *chardev = NULL;

static int dev_open(struct inode *inodep, struct file *filep)
{
    /* Nothing to do */
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    return rpitx_read_bytes_from_alsa_buffer(buffer, len);
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    /* Write not allowed */
    return -EINVAL;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    /* Nothing to do */
    return 0;
}

static struct file_operations chardev_ops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

static int init_char_device(void)
{    
    major_number = register_chrdev(0, CHARDEV_NAME, &chardev_ops);
    if (major_number < 0)
        return major_number;
    
    chardev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chardev_class))
        goto __remove_chrdev;
    
    chardev = device_create(chardev_class, NULL, MKDEV(major_number, 0), NULL, CHARDEV_NAME);
    if (IS_ERR(chardev))
        goto __remove_class;
    
    return 0;

__remove_class:
    class_destroy(chardev_class);
__remove_chrdev:
    unregister_chrdev(major_number, CHARDEV_NAME);
    return -1;
}

static void unregister_char_device(void)
{
    device_destroy(chardev_class, MKDEV(major_number, 0));
    class_unregister(chardev_class);
    class_destroy(chardev_class);
    unregister_chrdev(major_number, CHARDEV_NAME);
}

static int __init alsa_card_rpitx_init(void)
{
    int err;
    
    err = rpitx_init_alsa_system();
    if (err < 0)
        return err;

    err = init_char_device();
    if (err < 0)
        return err;
    
    return 0;
}

static void __exit alsa_card_rpitx_exit(void)
{
    rpitx_unregister_alsa();
    unregister_char_device();
}

module_init(alsa_card_rpitx_init)
module_exit(alsa_card_rpitx_exit)
