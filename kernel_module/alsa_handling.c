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

#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "alsa_handling.h"
#include "iq_sample_generation.h"

/* Basic configuration */
#define SND_RPITX_DRIVER "snd_rpitx"

/* Buffer size definition */
#define NUMBER_OF_PERIODS 8
#define MAX_BUFFER (PERIOD_BYTES * NUMBER_OF_PERIODS)

/* Device names definition */
#define SND_DRIVER_NAME "snd_rpitx"
#define SND_CARD_NAME "rpitx"
#define STEREO_IQ_DEVICE_NAME "sendiq"
#define MONO_USB_DEVICE_NAME "usbdata"

/* Arrays needed for ALSA */
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = {1, [1 ... (SNDRV_CARDS - 1)] = 0};
static struct platform_device *devices[SNDRV_CARDS];


/* PCM configuration for stereo (I-Q) */
static struct snd_pcm_hardware rpitx_pcm_stereo_hw =
{
    .info = SNDRV_PCM_INFO_INTERLEAVED,
    .formats = SNDRV_PCM_FMTBIT_S16_LE,
    .rates = SNDRV_PCM_RATE_8000_48000,
    .rate_min = 8000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .buffer_bytes_max = MAX_BUFFER,
    .period_bytes_min = PERIOD_BYTES,
    .period_bytes_max = PERIOD_BYTES,
    .periods_min = 1,
    .periods_max = NUMBER_OF_PERIODS,
};

/* PCM configuration for mono (USB) */
static struct snd_pcm_hardware rpitx_pcm_mono_hw =
{
    .info = SNDRV_PCM_INFO_INTERLEAVED,
    .formats = SNDRV_PCM_FMTBIT_S16_LE,
    .rates = SNDRV_PCM_RATE_8000_48000,
    .rate_min = 8000,
    .rate_max = 48000,
    .channels_min = 1,
    .channels_max = 1,
    .buffer_bytes_max = MAX_BUFFER,
    .period_bytes_min = PERIOD_BYTES / 2,
    .period_bytes_max = PERIOD_BYTES / 2,
    .periods_min = 1,
    .periods_max = NUMBER_OF_PERIODS * 2,
};


/* Structure definition for "private" date.
 * Those are passed to ALSA and are accessible within callbacks.
 */
struct rpitx_private_data
{
    struct mutex cable_lock;
    struct snd_pcm_substream *substream;
    int is_stereo;
};

struct rpitx_device
{
    struct rpitx_private_data stereo_iq_private_data;
    struct rpitx_private_data mono_usb_private_data;
    int is_stereo_iq_open;
    int is_mono_usb_open;
};

/* Global state variables */
struct rpitx_device *mydev;
size_t buffer_hw_pointer = 0;

/* Free callback, unused */
static int rpitx_pcm_dev_free(struct snd_device *device)
{
    return 0;
}

static struct snd_device_ops dev_ops =
{
    .dev_free = rpitx_pcm_dev_free,
};


/* === Common callback functions === */

/* Allocation callbacks */
static int rpitx_hw_params(struct snd_pcm_substream *ss, struct snd_pcm_hw_params *hw_params)
{
    /* We let ALSA handle allocation. */
    return snd_pcm_lib_malloc_pages(ss, params_buffer_bytes(hw_params));
}

static int rpitx_hw_free(struct snd_pcm_substream *ss)
{
    return snd_pcm_lib_free_pages(ss);
}

/* Device close callback */
static int rpitx_pcm_close(struct snd_pcm_substream *ss)
{
    mydev->is_mono_usb_open = 0;
    mydev->is_stereo_iq_open = 0;
    return 0;
}

/* Prepare callback, unused */
static int rpitx_pcm_prepare(struct snd_pcm_substream *ss)
{
    return 0;
}

/* Trigger callback, unused */
static int rpitx_pcm_trigger(struct snd_pcm_substream *ss, int cmd)
{
    return 0;
}

/* Pointer callback returns the size of the used buffer */
static snd_pcm_uframes_t rpitx_pcm_pointer(struct snd_pcm_substream *ss)
{
    struct snd_pcm_runtime *runtime = ss->runtime;
    return bytes_to_frames(runtime, buffer_hw_pointer);
}


/* === Differentiated callback functions === */

/* Device open callbacks */
static int rpitx_pcm_open_stereo(struct snd_pcm_substream *ss)
{
    struct rpitx_private_data *pdata = ss->private_data;

    if (mydev->is_stereo_iq_open || mydev->is_mono_usb_open)
        return -1;
    
    mutex_lock(&pdata->cable_lock);
    
    ss->runtime->hw = rpitx_pcm_stereo_hw;

    pdata->substream = ss;
    ss->runtime->private_data = pdata;

    mutex_unlock(&pdata->cable_lock);
    
    mydev->is_stereo_iq_open = 1;
    return 0;
}

static int rpitx_pcm_open_mono(struct snd_pcm_substream *ss)
{
    struct rpitx_private_data *pdata = ss->private_data;

    if (mydev->is_stereo_iq_open || mydev->is_mono_usb_open)
        return -1;

    mutex_lock(&pdata->cable_lock);

    ss->runtime->hw = rpitx_pcm_mono_hw;

    pdata->substream = ss;
    ss->runtime->private_data = pdata;

    mutex_unlock(&pdata->cable_lock);

    clear_iq_sample_generation();
    
    mydev->is_mono_usb_open = 1;
    return 0;
}

static struct snd_pcm_ops rpitx_pcm_ops_stereo =
{
    .open = rpitx_pcm_open_stereo, /* Only difference */
    .close = rpitx_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = rpitx_hw_params,
    .hw_free = rpitx_hw_free,
    .prepare = rpitx_pcm_prepare,
    .trigger = rpitx_pcm_trigger,
    .pointer = rpitx_pcm_pointer,
};

static struct snd_pcm_ops rpitx_pcm_ops_mono =
{
    .open = rpitx_pcm_open_mono, /* Only difference */
    .close = rpitx_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = rpitx_hw_params,
    .hw_free = rpitx_hw_free,
    .prepare = rpitx_pcm_prepare,
    .trigger = rpitx_pcm_trigger,
    .pointer = rpitx_pcm_pointer,
};

/* Probe callback */
static int rpitx_probe(struct platform_device *devptr)
{

    struct snd_card *card;
    int ret;

    struct snd_pcm *stereo_pcm, *mono_pcm;

    int dev = devptr->id;

    ret = snd_card_new(&devptr->dev, index[dev], id[dev], THIS_MODULE, sizeof(struct rpitx_device), &card);

    if (ret < 0)
        goto __nodev;

    mydev = card->private_data;
    
    mydev->is_stereo_iq_open = 0;
    mydev->is_mono_usb_open = 0;
    mydev->stereo_iq_private_data.is_stereo = 1;
    mydev->mono_usb_private_data.is_stereo = 0;
    mutex_init(&mydev->stereo_iq_private_data.cable_lock);
    mutex_init(&mydev->mono_usb_private_data.cable_lock);
    
    sprintf(card->driver, SND_DRIVER_NAME);
    sprintf(card->shortname, SND_CARD_NAME);
    sprintf(card->longname, SND_CARD_NAME);
    
    snd_card_set_dev(card, &devptr->dev);

    ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, mydev, &dev_ops);
    if (ret < 0)
        goto __nodev;

    /* Stereo (I/Q data) playback device */
    ret = snd_pcm_new(card, STEREO_IQ_DEVICE_NAME, 0, 1, 0, &stereo_pcm);
    if (ret < 0)
        goto __nodev;

    snd_pcm_set_ops(stereo_pcm, SNDRV_PCM_STREAM_PLAYBACK, &rpitx_pcm_ops_stereo);
    stereo_pcm->private_data = &mydev->stereo_iq_private_data;
    stereo_pcm->info_flags = 0;
    strcpy(stereo_pcm->name, STEREO_IQ_DEVICE_NAME);

    ret = snd_pcm_lib_preallocate_pages_for_all(stereo_pcm, SNDRV_DMA_TYPE_CONTINUOUS, snd_dma_continuous_data(GFP_KERNEL), MAX_BUFFER, MAX_BUFFER);
    if (ret < 0)
        goto __nodev;

    /* Mono (USB data) playback device */
    ret = snd_pcm_new(card, MONO_USB_DEVICE_NAME, 1, 1, 0, &mono_pcm);
    if (ret < 0)
        goto __nodev;

    snd_pcm_set_ops(mono_pcm, SNDRV_PCM_STREAM_PLAYBACK, &rpitx_pcm_ops_mono);
    mono_pcm->private_data = &mydev->mono_usb_private_data;
    mono_pcm->info_flags = 0;
    strcpy(mono_pcm->name, MONO_USB_DEVICE_NAME);

    ret = snd_pcm_lib_preallocate_pages_for_all(mono_pcm, SNDRV_DMA_TYPE_CONTINUOUS, snd_dma_continuous_data(GFP_KERNEL), MAX_BUFFER, MAX_BUFFER);
    if (ret < 0)
        goto __nodev;

    
    ret = snd_card_register(card);

    if (ret == 0)
    {
        platform_set_drvdata(devptr, card);
        return 0;
    }

__nodev:
    snd_card_free(card);
    return ret;
}

/* Free sound card callback */
static int rpitx_remove(struct platform_device *devptr)
{
    snd_card_free(platform_get_drvdata(devptr));
    platform_set_drvdata(devptr, NULL);
    return 0;
}

static struct platform_driver rpitx_driver =
{
    .probe = rpitx_probe,
    .remove = rpitx_remove,
    .driver = {
        .name = SND_RPITX_DRIVER,
        .owner = THIS_MODULE
    },
};


int rpitx_init_alsa_system(void)
{
    int i, err, cards;

    err = platform_driver_register(&rpitx_driver);
    if (err < 0)
        return err;

    cards = 0;
    for (i = 0; i < SNDRV_CARDS; i++)
    {
        struct platform_device *device;

        if (!enable[i])
            continue;

        device = platform_device_register_simple(SND_RPITX_DRIVER, i, NULL, 0);

        if (IS_ERR(device))
            continue;

        if (!platform_get_drvdata(device))
        {
            platform_device_unregister(device);
            continue;
        }

        devices[i] = device;
        cards++;
    }

    if (!cards)
    {
        rpitx_unregister_alsa();
        return -ENODEV;
    }

    return 0;
}


void rpitx_unregister_alsa(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(devices); ++i)
        platform_device_unregister(devices[i]);

    platform_driver_unregister(&rpitx_driver);
}

ssize_t rpitx_read_bytes_from_alsa_buffer(char *buffer, size_t len)
{
    unsigned long available_frames;
    int err;
    struct snd_pcm_substream *ss = NULL;
    
    /* Pick the correct structure, depending on the open device */
    if (mydev->is_stereo_iq_open)
        ss = mydev->stereo_iq_private_data.substream;
    else if (mydev->is_mono_usb_open)
        ss = mydev->mono_usb_private_data.substream;
    else
        return 0;
    
    /* We don't copy anything if the call doesn't ask for at least a period */
    if (len < PERIOD_BYTES)
        return 0;
    
    /* If the buffer is not full enough, we don't read. */
    available_frames = snd_pcm_playback_hw_avail(ss->runtime);
    if (available_frames < PERIOD_BYTES)
        return 0;

    buffer_hw_pointer %= MAX_BUFFER;
    
    /* We do the actual copy */
    if (mydev->is_stereo_iq_open) {
        err = copy_to_user(buffer,
                           ss->runtime->dma_area + buffer_hw_pointer,
                           PERIOD_BYTES);
        buffer_hw_pointer += PERIOD_BYTES;
    } else {
        process_iq_period(buffer,
                          ss->runtime->dma_area + buffer_hw_pointer);
        buffer_hw_pointer += PERIOD_BYTES;
        
    }
    
    /* We tell ALSA we have emptied some of the buffer */
    snd_pcm_period_elapsed(ss);
    
    return PERIOD_BYTES;
}

