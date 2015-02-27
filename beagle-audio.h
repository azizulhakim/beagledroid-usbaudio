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

//#include <media/v4l2-device.h>
#include <media/videobuf2-vmalloc.h>

/* Hardware. */
//#define BEAGLEAUDIO_VIDEO_ENDP	0x81
//#define BEAGLEAUDIO_AUDIO_ENDP	0x83
#define BEAGLEAUDIO_BASE		0xc000
#define BEAGLEAUDIO_REQUEST_REG	12

/* Number of concurrent isochronous urbs submitted.
 * Higher numbers was seen to overly saturate the USB bus. */
//#define BEAGLEAUDIO_ISOC_TRANSFERS	16
//#define BEAGLEAUDIO_ISOC_PACKETS	8

//#define BEAGLEAUDIO_CHUNK_SIZE	256
//#define BEAGLEAUDIO_CHUNK		240

//#define BEAGLEAUDIO_AUDIO_URBSIZE	20480
//#define BEAGLEAUDIO_AUDIO_HDRSIZE	4
#define BEAGLEAUDIO_AUDIO_BUFFER	65536
#define PCM_PACKET_SIZE 			4096

/* Chunk header. */
/*#define BEAGLEAUDIO_MAGIC_OK(chunk)	((be32_to_cpu(chunk[0]) & 0xff000000) \
							== 0x88000000)*/
//#define BEAGLEAUDIO_FRAME_ID(chunk)	((be32_to_cpu(chunk[0]) & 0x00ff0000) >> 16)
//#define BEAGLEAUDIO_ODD(chunk)	((be32_to_cpu(chunk[0]) & 0x0000f000) >> 15)
//#define BEAGLEAUDIO_CHUNK_NO(chunk)	(be32_to_cpu(chunk[0]) & 0x00000fff)

//#define BEAGLEAUDIO_TV_STD  (V4L2_STD_525_60 | V4L2_STD_PAL)

/* parameters for supported TV norms */
struct beagleaudio_norm_params {
	v4l2_std_id norm;
	int cap_width, cap_height;
};

/* A single videobuf2 frame buffer. */
/*struct beagleaudio_buf {
	struct vb2_buffer vb;
	struct list_head list;
};*/

/* Per-device structure. */
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
	unsigned int bulk_out_pipe;
};

int beagleaudio_set_regs(struct beagleaudio *beagleaudio, const u16 regs[][2], int size);

//int beagleaudio_video_init(struct beagleaudio *beagleaudio);
//void beagleaudio_video_free(struct beagleaudio *beagleaudio);

int beagleaudio_audio_init(struct beagleaudio *beagleaudio);
void beagleaudio_audio_free(struct beagleaudio *beagleaudio);
void beagleaudio_audio_suspend(struct beagleaudio *beagleaudio);
void beagleaudio_audio_resume(struct beagleaudio *beagleaudio);
