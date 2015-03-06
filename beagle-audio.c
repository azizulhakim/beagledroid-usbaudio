/*	Beaglebone USB Driver for Android Device Using AOA v2.0 Protocol
 *
 *   Copyright (C) 2014  Azizul Hakim
 *   
 *	 Work on this driver is mostly based on the work of Federico Simoncelli
 *	 for Fushicai USBTV007 Audio-Video Grabber
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>
#include <sound/pcm_params.h>

#include "beagle-audio.h"

static struct snd_pcm_hardware snd_beagleaudio_digital_hw = {
/*	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_KNOT,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.period_bytes_min = 64,
	.period_bytes_max = 12544,
	.periods_min = 2,
	.periods_max = 98,
	.buffer_bytes_max = 62720 * 8,*/ /* value in usbaudio.c */

  .info = (SNDRV_PCM_INFO_MMAP |
           SNDRV_PCM_INFO_INTERLEAVED |
           SNDRV_PCM_INFO_BLOCK_TRANSFER |
           SNDRV_PCM_INFO_MMAP_VALID),
  .formats =          SNDRV_PCM_FMTBIT_S16_LE,
  .rates =            SNDRV_PCM_RATE_8000_48000,
  .rate_min =         8000,
  .rate_max =         48000,
  .channels_min =     2,
  .channels_max =     2,
  .buffer_bytes_max = 32768,
  .period_bytes_min = 4096,
  .period_bytes_max = 32768,
  .periods_min =      1,
  .periods_max =      1024,
};

static int snd_beagleaudio_pcm_open(struct snd_pcm_substream *substream)
{
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	printk("PCM Open\n");

	chip->snd_substream = substream;
	runtime->hw = snd_beagleaudio_digital_hw;

	printk("PCM Open Exit\n");

	return 0;
}

static int snd_beagleaudio_pcm_close(struct snd_pcm_substream *substream)
{
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

	printk("PCM Close\n");

	if (atomic_read(&chip->snd_stream)) {
		atomic_set(&chip->snd_stream, 0);
		schedule_work(&chip->snd_trigger);
	}

	printk("PCM CLose exit\n");

	return 0;
}

static int snd_beagleaudio_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int rv;
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

	printk("PCM HW Params\n");

	rv = snd_pcm_lib_malloc_pages(substream,
		params_buffer_bytes(hw_params));

	if (rv < 0) {
		dev_warn(chip->dev, "pcm audio buffer allocation failure %i\n", rv);
		return rv;
	}

	printk("PCM HW Params exit\n");

	return 0;
}

static int snd_beagleaudio_hw_free(struct snd_pcm_substream *substream)
{
	printk("PCM HW Free\n");
	snd_pcm_lib_free_pages(substream);
	printk("PCM HW Free Exit\n");
	return 0;
}

static int snd_beagleaudio_prepare(struct snd_pcm_substream *substream)
{
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

	printk("PCM Prepare\n");

	chip->snd_buffer_pos = 0;
	chip->snd_period_pos = 0;

	printk("PCM Prepare Exit\n");

	return 0;
}

static void beagleaudio_audio_urb_received(struct urb *urb)
{
	struct beagleaudio *chip = urb->context;
	struct snd_pcm_substream *substream = chip->snd_substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
//	size_t i, frame_bytes, chunk_length, buffer_pos, period_pos;
	int period_elapsed;
//	void *urb_current;
	unsigned int pcm_buffer_size;
	unsigned int len, ret;

	printk("PCM URB Received\n");

	switch (urb->status) {
	case 0:
		printk("case SUCCESS\n");
		break;
	case -ETIMEDOUT:
		printk("case ETIMEDOUT\n");
		return;
	case -ENOENT:
		printk("case ENOENT\n");
		return;
	case -EPROTO:
		printk("case EPROTO\n");
		return;
	case -ECONNRESET:
		printk("case ECONNRESET\n");
		return;
	case -ESHUTDOWN:
		printk("case ESHUTDOWN\n");
		return;
	default:
		dev_warn(chip->dev, "unknown audio urb status %i\n",
			urb->status);
	}

	printk("before lock\n");
	if (!atomic_read(&chip->snd_stream))
		return;

	printk("lock\n");

	snd_pcm_stream_lock(substream);

	pcm_buffer_size = snd_pcm_lib_buffer_bytes(substream);
	if (chip->snd_buffer_pos + PCM_PACKET_SIZE <= pcm_buffer_size){
		//source = runtime->dma_area + chip->snd_buffer_pos;
		memcpy(chip->snd_bulk_urb->transfer_buffer, runtime->dma_area + chip->snd_buffer_pos, PCM_PACKET_SIZE);
	}
	else{
		len = pcm_buffer_size - chip->snd_buffer_pos;

		//source = runtime->dma_area + chip->snd_buffer_pos;
		memcpy(chip->snd_bulk_urb->transfer_buffer, runtime->dma_area + chip->snd_buffer_pos, len);
		memcpy(chip->snd_bulk_urb->transfer_buffer + len, runtime->dma_area, PCM_PACKET_SIZE - len);	
	}

	period_elapsed = 0;
	chip->snd_buffer_pos += PCM_PACKET_SIZE;
	if (chip->snd_buffer_pos >= pcm_buffer_size)
		chip->snd_buffer_pos -= pcm_buffer_size;

	chip->snd_period_pos += PCM_PACKET_SIZE;
	if (chip->snd_period_pos >= runtime->period_size) {
		chip->snd_period_pos %= runtime->period_size;
		period_elapsed = 1;
	}

//	snd_pcm_stream_lock(substream);


	snd_pcm_stream_unlock(substream);

	if (period_elapsed)
		snd_pcm_period_elapsed(substream);

	printk("submit urb = %p\n", urb);
	ret = usb_submit_urb(urb, GFP_ATOMIC);

	printk("PCM URB Received Exit  %d\n", ret);
}

static int beagleaudio_audio_start(struct beagleaudio *chip)
{
	unsigned int pipe;
	int ret;
	static const u16 setup[][2] = {
		/* These seem to enable the device. */
		{ BEAGLEAUDIO_BASE + 0x0008, 0x0001 },
		{ BEAGLEAUDIO_BASE + 0x01d0, 0x00ff },
		{ BEAGLEAUDIO_BASE + 0x01d9, 0x0002 },

		{ BEAGLEAUDIO_BASE + 0x01da, 0x0013 },
		{ BEAGLEAUDIO_BASE + 0x01db, 0x0012 },
		{ BEAGLEAUDIO_BASE + 0x01e9, 0x0002 },
		{ BEAGLEAUDIO_BASE + 0x01ec, 0x006c },
		{ BEAGLEAUDIO_BASE + 0x0294, 0x0020 },
		{ BEAGLEAUDIO_BASE + 0x0255, 0x00cf },
		{ BEAGLEAUDIO_BASE + 0x0256, 0x0020 },
		{ BEAGLEAUDIO_BASE + 0x01eb, 0x0030 },
		{ BEAGLEAUDIO_BASE + 0x027d, 0x00a6 },
		{ BEAGLEAUDIO_BASE + 0x0280, 0x0011 },
		{ BEAGLEAUDIO_BASE + 0x0281, 0x0040 },
		{ BEAGLEAUDIO_BASE + 0x0282, 0x0011 },
		{ BEAGLEAUDIO_BASE + 0x0283, 0x0040 },
		{ 0xf891, 0x0010 },

		/* this sets the input from composite */
		{ BEAGLEAUDIO_BASE + 0x0284, 0x00aa },
	};

	printk("PCM Audio Start\n");

	chip->snd_bulk_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (chip->snd_bulk_urb == NULL)
		goto err_alloc_urb;

	//pipe = usb_rcvbulkpipe(chip->udev, BEAGLEAUDIO_AUDIO_ENDP);
	pipe = chip->bulk_out_pipe;
	printk("pipe = %d\n", pipe);

	chip->snd_bulk_urb->transfer_buffer = kzalloc(
		PCM_PACKET_SIZE, GFP_KERNEL);

	if (chip->snd_bulk_urb->transfer_buffer) printk("Buffer founddddddddddddddddddddddd\n");
	if (chip->snd_bulk_urb->transfer_buffer == NULL)
		goto err_transfer_buffer;

	usb_fill_bulk_urb(chip->snd_bulk_urb, chip->udev, pipe,
		chip->snd_bulk_urb->transfer_buffer, PCM_PACKET_SIZE,
		beagleaudio_audio_urb_received, chip);

	/* starting the stream */
	//beagleaudio_set_regs(chip, setup, ARRAY_SIZE(setup));

	ret = usb_clear_halt(chip->udev, pipe);
	printk("usb_clear_halt: %d\n", ret);
	printk("submit urb = %p\n", chip->snd_bulk_urb);
	ret = usb_submit_urb(chip->snd_bulk_urb, GFP_ATOMIC);

	printk("PCM Audio Start Exit %d\n", ret);

	return 0;

err_transfer_buffer:
	printk("kill urb = %p\n", chip->snd_bulk_urb);
	usb_free_urb(chip->snd_bulk_urb);
	chip->snd_bulk_urb = NULL;

err_alloc_urb:
	return -ENOMEM;
}

static int beagleaudio_audio_stop(struct beagleaudio *chip)
{
	static const u16 setup[][2] = {
/*		{ BEAGLEAUDIO_BASE + 0x00a2, 0x0013 }, */
		{ BEAGLEAUDIO_BASE + 0x027d, 0x0000 },
		{ BEAGLEAUDIO_BASE + 0x0280, 0x0010 },
		{ BEAGLEAUDIO_BASE + 0x0282, 0x0010 },
	};

	printk("PCM Stop\n");

	if (chip->snd_bulk_urb) {
		usb_kill_urb(chip->snd_bulk_urb);
		kfree(chip->snd_bulk_urb->transfer_buffer);
		printk("kill urb = %p\n", chip->snd_bulk_urb);
		usb_free_urb(chip->snd_bulk_urb);
		chip->snd_bulk_urb = NULL;
	}

	//beagleaudio_set_regs(chip, setup, ARRAY_SIZE(setup));

	printk("PCM Stop Exit\n");

	return 0;
}

void beagleaudio_audio_suspend(struct beagleaudio *beagleaudio)
{
	printk("PCM Suspend\n");

	if (atomic_read(&beagleaudio->snd_stream) && beagleaudio->snd_bulk_urb)
		usb_kill_urb(beagleaudio->snd_bulk_urb);

	printk("PCM Suspend Exit\n");
}

void beagleaudio_audio_resume(struct beagleaudio *beagleaudio)
{
	printk("PCM Resume\n");

	if (atomic_read(&beagleaudio->snd_stream) && beagleaudio->snd_bulk_urb){
		printk("submit urb = %p\n", beagleaudio->snd_bulk_urb);
		usb_submit_urb(beagleaudio->snd_bulk_urb, GFP_ATOMIC);
	}

	printk("PCM Resume Exit\n");
}

static void snd_beagleaudio_trigger(struct work_struct *work)
{
	struct beagleaudio *chip = container_of(work, struct beagleaudio, snd_trigger);

	printk("PCM Trigger\n");


	if (atomic_read(&chip->snd_stream))
		beagleaudio_audio_start(chip);
	else
		beagleaudio_audio_stop(chip);

	printk("PCM Trigger Exit\n");
}

static int snd_beagleaudio_card_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

	printk("PCM Card trigger\n");

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		atomic_set(&chip->snd_stream, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		atomic_set(&chip->snd_stream, 0);
		break;
	default:
		return -EINVAL;
	}

	schedule_work(&chip->snd_trigger);

	printk("PCM Card trigger Exit\n");

	return 0;
}

static snd_pcm_uframes_t snd_beagleaudio_pointer(struct snd_pcm_substream *substream)
{
	int i;
	int flag = 0;
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

	//printk("PCM Pointer\n");
/*
	for (i=0; i<(int)substream->runtime->buffer_size; i++){
		if (substream->runtime->dma_area[i] != 0){
			//printk("%d  ", substream->runtime->dma_area[i]);
			flag = 1;
//			break;
		}
	}
	printk("\n");

	if (flag != 0)
	printk("PCM Pointer Exit. Buffer size = %d Buffer pos = %d\n", (int)substream->runtime->buffer_size, chip->snd_buffer_pos);
*/
	return bytes_to_frames(substream->runtime, chip->snd_buffer_pos);
}

static struct snd_pcm_ops snd_beagleaudio_pcm_ops = {
	.open = snd_beagleaudio_pcm_open,
	.close = snd_beagleaudio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_beagleaudio_hw_params,
	.hw_free = snd_beagleaudio_hw_free,
	.prepare = snd_beagleaudio_prepare,
	.trigger = snd_beagleaudio_card_trigger,
	.pointer = snd_beagleaudio_pointer,
};

/*static void beagleaudio_release(struct v4l2_device *v4l2_dev)
{
	//struct beagleaudio *beagleaudio = container_of(v4l2_dev, struct beagleaudio, v4l2_dev);

	//v4l2_device_unregister(&beagleaudio->v4l2_dev);
	//vb2_queue_release(&beagleaudio->vb2q);
	//kfree(beagleaudio);
}*/

int beagleaudio_audio_init(struct beagleaudio *beagleaudio)
{
	int rv;
	struct snd_card *card;
	struct snd_pcm *pcm;

	printk("Audio Init\n");

/*	beagleaudio->v4l2_dev.release = beagleaudio_release;
	rv = v4l2_device_register(beagleaudio->dev, &beagleaudio->v4l2_dev);
	if (rv < 0) {
		dev_warn(beagleaudio->dev, "Could not register v4l2 device\n");
		goto v4l2_fail;
	}
*/

	INIT_WORK(&beagleaudio->snd_trigger, snd_beagleaudio_trigger);
	atomic_set(&beagleaudio->snd_stream, 0);

	rv = snd_card_create(SNDRV_DEFAULT_IDX1, "beagleaudio", THIS_MODULE, 0,
		&card);
	if (rv < 0)
		return rv;

	strncpy(card->driver, beagleaudio->dev->driver->name,
		sizeof(card->driver) - 1);
	strncpy(card->shortname, "beagleaudio", sizeof(card->shortname) - 1);
	snprintf(card->longname, sizeof(card->longname),
		"BEAGLEAUDIO at bus %d device %d", beagleaudio->udev->bus->busnum,
		beagleaudio->udev->devnum);

	snd_card_set_dev(card, beagleaudio->dev);

	beagleaudio->snd = card;

	rv = snd_pcm_new(card, "BEAGLEAUDIO", 0, 1, 0, &pcm);
	if (rv < 0)
		goto err;

	strncpy(pcm->name, "BeagleAudio Input", sizeof(pcm->name) - 1);
	pcm->info_flags = 0;
	pcm->private_data = beagleaudio;


	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_beagleaudio_pcm_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
		snd_dma_continuous_data(GFP_KERNEL), BEAGLEAUDIO_AUDIO_BUFFER,
		BEAGLEAUDIO_AUDIO_BUFFER);


	rv = snd_card_register(card);
	if (rv)
		goto err;

	printk("BeagleAudio Init Exit\n");

	return 0;

err:
	beagleaudio->snd = NULL;
	snd_card_free(card);

	return rv;
}

void beagleaudio_audio_free(struct beagleaudio *beagleaudio)
{
	printk("Audio Free\n");
	if (beagleaudio->snd && beagleaudio->udev) {
		snd_card_free(beagleaudio->snd);
		beagleaudio->snd = NULL;
	}
	printk("Audio Free Exit\n");
}
