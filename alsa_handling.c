/*
 * 
 */

#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "alsa_handling.h"

/* Basic configuration */
#define SND_RPITX_DRIVER "snd_rpitx"
#define MAX_BUFFER (256 * 8)
#define SND_DRIVER_NAME "snd_rpitx"
#define SND_CARD_NAME "rpitx"
#define STEREO_IQ_DEVICE_NAME "sendiq"
#define MONO_USB_DEVICE_NAME "usbdata"

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = {1, [1 ... (SNDRV_CARDS - 1)] = 0};
static struct platform_device *devices[SNDRV_CARDS];

static struct snd_pcm_hardware rpitx_pcm_stereo_hw =
{
    .info = SNDRV_PCM_INFO_INTERLEAVED,
    .formats = SNDRV_PCM_FMTBIT_U8,
    .rates = SNDRV_PCM_RATE_8000_48000,
    .rate_min = 8000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .buffer_bytes_max = MAX_BUFFER,
    .period_bytes_min = 48,
    .period_bytes_max = 48,
    .periods_min = 1,
    .periods_max = 32,
};

static struct snd_pcm_hardware rpitx_pcm_mono_hw =
{
    .info = SNDRV_PCM_INFO_INTERLEAVED,
    .formats = SNDRV_PCM_FMTBIT_U8,
    .rates = SNDRV_PCM_RATE_8000_48000,
    .rate_min = 8000,
    .rate_max = 48000,
    .channels_min = 1,
    .channels_max = 1,
    .buffer_bytes_max = MAX_BUFFER,
    .period_bytes_min = 256,
    .period_bytes_max = 256,
    .periods_min = 1,
    .periods_max = 8,
};

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

struct rpitx_device *mydev;

/*static struct {
    struct mutex lock;
    size_t size;
    char data[MAX_BUFFER];
} internal_buffer;*/
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
   /* struct snd_pcm_runtime *runtime = ss->runtime;
    if (!runtime)
        return 0;
    return runtime->dma_bytes;*/
    // FIXME Actually handle non emptied buffer.
    //return mysubstream->runtime->dma_bytes;
//    struct rpitx_private_data *pdata = ss->private_data;
//    struct snd_pcm_runtime *runtime = ss->runtime;
//    return bytes_to_frames(runtime, internal_buffer.size);
   // if (pdata->is_stereo)
//        return internal_buffer.size;
   // else
     //   return internal_buffer.size/2; // FIXME Sample size
    return buffer_hw_pointer;
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

    mydev->is_mono_usb_open = 1;
    return 0;
}

static struct snd_pcm_ops rpitx_pcm_ops_stereo =
{
    .open = rpitx_pcm_open_stereo,
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
    .open = rpitx_pcm_open_mono,
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
    
    // FIXME: mutex on buffer
    //mutex_init(&internal_buffer.lock);
    //internal_buffer.size = 0;

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
    /*int err, i;
    size_t size_to_read;
    
    len -= len % 4;
    
    mutex_lock(&internal_buffer.lock);
    
    size_to_read = min(internal_buffer.size, len);
    
    err = copy_to_user(buffer, internal_buffer.data, size_to_read);
    if (err != 0)
        return -EINVAL;
    
    for (i = 0; i < internal_buffer.size - size_to_read; i++) {
        internal_buffer.data[i] = internal_buffer.data[size_to_read + i];
    }
    
    internal_buffer.size = internal_buffer.size - size_to_read;
    
    mutex_unlock(&internal_buffer.lock);
    
    if (mydev->is_stereo_iq_open)
        snd_pcm_period_elapsed(mydev->stereo_iq_private_data.substream);
    if (mydev->is_mono_usb_open)
        snd_pcm_period_elapsed(mydev->mono_usb_private_data.substream);
    
    return size_to_read;*/
    
    unsigned long available_frames;
    int err, i;
    struct snd_pcm_substream *ss = NULL;
    
    if (len < 256)
        return 0;
    
    if (mydev->is_stereo_iq_open)
        ss = mydev->stereo_iq_private_data.substream;
    else if (mydev->is_mono_usb_open)
        ss = mydev->mono_usb_private_data.substream;
    else
        return 0;
    
    available_frames = snd_pcm_playback_hw_avail(ss->runtime);
    
    if (available_frames == 0)
        return 0;
    
    printk("Read available frames: %d", available_frames);
    
    buffer_hw_pointer += 256;
    //buffer_hw_pointer += available_frames;
    //if (buffer_hw_pointer >= MAX_BUFFER)
        buffer_hw_pointer %= MAX_BUFFER;
    
    //err = copy_to_user(buffer, ss->runtime->dma_area, ss->runtime->dma_bytes);
    
    snd_pcm_period_elapsed(ss);
    
    err = copy_to_user(buffer, ss->runtime->dma_area + buffer_hw_pointer, 256);
    
    //for (i = 0; i < 256; i++)
    //    buffer[i] = 'z';
    return 256;
}


