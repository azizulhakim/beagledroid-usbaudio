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

#include <linux/module.h>

#include "usbtv.h"
#include "aoa.h"

int beagleaudio_set_regs(struct beagleaudio *beagleaudio, const u16 regs[][2], int size)
{
	int ret;
	int pipe = usb_rcvctrlpipe(beagleaudio->udev, 0);
	int i;

	for (i = 0; i < size; i++) {
		u16 index = regs[i][0];
		u16 value = regs[i][1];

		ret = usb_control_msg(beagleaudio->udev, pipe, BEAGLEAUDIO_REQUEST_REG,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, NULL, 0, 0);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int beagleaudio_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	static int intfcount = 0;
	int ret = 0;
//	int size;
	int datalen;
	u8 data[8];
	struct device *dev = &intf->dev;
	struct beagleaudio *beagleaudio;
	struct usb_device *usb_dev = interface_to_usbdev(intf);

	if (intfcount == 0 && id->idVendor == 0x18D1 && (id->idProduct == 0x2D00 || id->idProduct == 0x2D01 ||
									id->idProduct == 0x2D02 || id->idProduct == 0x2D03 ||
								id->idProduct == 0x2D04 || id->idProduct == 0x2D05)){
		printk("BEAGLEDROID-USBAUDIO:  [%04X:%04X] Connected in AOA mode\n", id->idVendor, id->idProduct);

		intfcount++;

		printk("BEAGLEDROID-USBAUDIO: intfcount = %d\n", intfcount);


		printk("BEAGLEDROID-USBAUDIO: intf.num_altsetting: %d\n", intf->num_altsetting);
		if (intf->altsetting != NULL)
			printk("BEAGLEDROID-USBAUDIO: intf->altsetting[0].des.bNumEndpoints : %d\n", intf->altsetting[0].desc.bNumEndpoints);


		/* Packet size is split into 11 bits of base size and count of
		 * extra multiplies of it.*/
//		size = usb_endpoint_maxp(&intf->altsetting[1].endpoint[0].desc);
//		size = (size & 0x07ff) * (((size & 0x1800) >> 11) + 1);

		/* Device structure */
		beagleaudio = kzalloc(sizeof(struct beagleaudio), GFP_KERNEL);
		if (beagleaudio == NULL)
			return -ENOMEM;
		beagleaudio->dev = dev;
		beagleaudio->udev = usb_get_dev(interface_to_usbdev(intf));

		//beagleaudio->iso_size = size;		// video

		usb_set_intfdata(intf, beagleaudio);

		//ret = beagleaudio_video_init(beagleaudio);
		//if (ret < 0)
		//	goto beagleaudio_video_fail;

		ret = beagleaudio_audio_init(beagleaudio);
		if (ret < 0)
			goto beagleaudio_audio_fail;

		/* for simplicity we exploit the v4l2_device reference counting */
//		v4l2_device_get(&beagleaudio->v4l2_dev);

		dev_info(dev, "BeagleBone Audio Grabber Driver\n");

		return 0;
	}
	else if(intfcount == 0){
		datalen = GetProtocol(usb_dev, (char*)data);

		if (datalen < 0) {
			printk("BEAGLEDROID-KBD: usb_control_msg : [%d]", datalen);
		}
		else{
			printk("BEAGLEDROID-KBD: AOA version = %d\n", data[0]);
		}

		SendIdentificationInfo(usb_dev, ID_MANU, (char*)MANU);
		SendIdentificationInfo(usb_dev, ID_MODEL, (char*)MODEL);
		SendIdentificationInfo(usb_dev, ID_DESC, (char*)DESCRIPTION);
		SendIdentificationInfo(usb_dev, ID_VERSION, (char*)VERSION);
		SendIdentificationInfo(usb_dev, ID_URI, (char*)URI);
		SendIdentificationInfo(usb_dev, ID_SERIAL, (char*)SERIAL);

		printk("<-----Audio Request------  %d ----------------->\n", SendAudioActivationRequest(usb_dev));
		SendAOAActivationRequest(usb_dev);

		return 0;
	}

	goto exit;

beagleaudio_audio_fail:
	//beagleaudio_video_free(beagleaudio);

//beagleaudio_video_fail:
	printk("beagleaudio_audio_fail\n");
	kfree(beagleaudio);

exit:
	return ret;
}

static void beagleaudio_disconnect(struct usb_interface *intf)
{
	struct beagleaudio *beagleaudio = usb_get_intfdata(intf);

	printk("beagleaudio_disconnect\n");

	usb_set_intfdata(intf, NULL);

	if (!beagleaudio)
		return;

	beagleaudio_audio_free(beagleaudio);
//	beagleaudio_video_free(beagleaudio);

	usb_put_dev(beagleaudio->udev);
	beagleaudio->udev = NULL;

	/* the beagleaudio structure will be deallocated when v4l2 will be
	   done using it */
//	v4l2_device_put(&beagleaudio->v4l2_dev);
}

struct usb_device_id beagleaudio_id_table[] = {
	{ USB_DEVICE(0x18D1, 0x4E41) },
	{ USB_DEVICE(0x18D1, 0x2D04) },
	{}
};
MODULE_DEVICE_TABLE(usb, beagleaudio_id_table);

MODULE_AUTHOR("Azizul Hakim");
MODULE_DESCRIPTION("BeagleBone Audio Grabber Driver");
MODULE_LICENSE("GPL");

struct usb_driver beagleaudio_usb_driver = {
	.name = "beagleaudio",
	.id_table = beagleaudio_id_table,
	.probe = beagleaudio_probe,
	.disconnect = beagleaudio_disconnect,
};

module_usb_driver(beagleaudio_usb_driver);
