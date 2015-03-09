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
  .info = (SNDRV_PCM_INFO_MMAP |
           SNDRV_PCM_INFO_INTERLEAVED |
           SNDRV_PCM_INFO_BLOCK_TRANSFER |
           SNDRV_PCM_INFO_MMAP_VALID),
  .formats =          SNDRV_PCM_FMTBIT_S16_LE,
  .rates =            SNDRV_PCM_RATE_44100,
  .rate_min =         44100,
  .rate_max =         44100,
  .channels_min =     2,
  .channels_max =     2,
  .buffer_bytes_max = 32768,
  .period_bytes_min = 4096,
  .period_bytes_max = 32768,
  .periods_min =      1,
  .periods_max =      1024,
};

int counter = 0;

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
	int period_elapsed;
	unsigned int pcm_buffer_size;
	unsigned int len, ret, i;
	char k;

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

	if (!atomic_read(&chip->snd_stream))
		return;

	snd_pcm_stream_lock(substream);

	pcm_buffer_size = snd_pcm_lib_buffer_bytes(substream);
	if (chip->snd_buffer_pos + PCM_PACKET_SIZE <= pcm_buffer_size){
		memcpy(chip->snd_bulk_urb->transfer_buffer, runtime->dma_area + chip->snd_buffer_pos, PCM_PACKET_SIZE);
		counter++;
	}
	else{
		len = pcm_buffer_size - chip->snd_buffer_pos;

		memcpy(chip->snd_bulk_urb->transfer_buffer, runtime->dma_area + chip->snd_buffer_pos, len);
		memcpy(chip->snd_bulk_urb->transfer_buffer + len, runtime->dma_area, PCM_PACKET_SIZE - len);	
		counter++;
	}

	printk("Counter = %d\n", counter);
	printk("Values = \n");
	if (counter <= 24){

		for (i=0; i<150; i++){
			k = ((char*)chip->snd_bulk_urb->transfer_buffer)[i];
			printk(" %3d", k);
		}
		printk("\n");
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

	printk("PCM Audio Start\n");

	chip->snd_bulk_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (chip->snd_bulk_urb == NULL)
		goto err_alloc_urb;

	pipe = chip->bulk_out_pipe;

	chip->snd_bulk_urb->transfer_buffer = kzalloc(
		PCM_PACKET_SIZE, GFP_KERNEL);

	if (chip->snd_bulk_urb->transfer_buffer == NULL)
		goto err_transfer_buffer;

	usb_fill_bulk_urb(chip->snd_bulk_urb, chip->udev, pipe,
		chip->snd_bulk_urb->transfer_buffer, PCM_PACKET_SIZE,
		beagleaudio_audio_urb_received, chip);

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

	printk("PCM Stop\n");

	if (chip->snd_bulk_urb) {
		usb_kill_urb(chip->snd_bulk_urb);
		kfree(chip->snd_bulk_urb->transfer_buffer);
		printk("kill urb = %p\n", chip->snd_bulk_urb);
		usb_free_urb(chip->snd_bulk_urb);
		chip->snd_bulk_urb = NULL;
	}

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
	struct beagleaudio *chip = snd_pcm_substream_chip(substream);

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

int beagleaudio_audio_init(struct beagleaudio *beagleaudio)
{
	int rv;
	struct snd_card *card;
	struct snd_pcm *pcm;

	printk("Audio Init\n");

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
