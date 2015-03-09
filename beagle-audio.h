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


#include <linux/slab.h>
#include <linux/usb.h>

#define BEAGLEAUDIO_AUDIO_BUFFER	65536
#define PCM_PACKET_SIZE 			4096


struct beagleaudio {
	struct device *dev;
	struct usb_device *udev;

	/* audio */
	struct snd_card *snd;
	struct snd_pcm_substream *snd_substream;
	atomic_t snd_stream;
	struct work_struct snd_trigger;
	struct urb *snd_bulk_urb;
	size_t snd_buffer_pos;
	size_t snd_period_pos;
	__u8	bulk_out_endpointAddr;
	unsigned int bulk_out_pipe;

	unsigned char *inBuffer;
	struct urb *irq;
};

int beagleaudio_audio_init(struct beagleaudio *beagleaudio);
void beagleaudio_audio_free(struct beagleaudio *beagleaudio);
void beagleaudio_audio_suspend(struct beagleaudio *beagleaudio);
void beagleaudio_audio_resume(struct beagleaudio *beagleaudio);
