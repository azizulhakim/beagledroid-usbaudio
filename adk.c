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

#include "beagle-audio.h"
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

static void usb_kbd_irq(struct urb *urb)
{
	struct beagleaudio *beagleaudio = urb->context;
	int filterId;
	int i;

	switch (urb->status) {
	case 0:			/* success */
		printk("Success\n");
		break;
	case -ECONNRESET:	/* unlink */
		printk("ECONNRESET\n");
		return;
	case -ENOENT:
		printk("ENOENT\n");
		return;
	case -ESHUTDOWN:
		printk("ESHUTDOWN\n");
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}

	for (i=0; i<8; i++){
		printk("%d ", beagleaudio->inBuffer[i]);
	}


/*	filterId = kbd->new[0];
	switch(filterId){
		case KEYBOARDCONTROL:
			// handle keyboard
			handle_keyboard(kbd);
			break;
		case MOUSECONTROL:
			// handle mouse

			handle_mouse(kbd);
			break;

		case CONTROLMESSAGE:
			// handle contorl message
			handle_control(kbd);
			break;
		default:
			break;
	}*/

resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC);
	if (i){
		printk("can't resubmit intr\n");
	}
}

static int beagleaudio_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	static int intfcount = 0;
	int ret = -1;
	int i;
//	int size;
	int datalen;
	u8 data[8];
	struct device *dev = &intf->dev;
	struct beagleaudio *beagleaudio;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface_descriptor;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_endpoint_descriptor *inEndpoint;
	int interval;
	unsigned int inputPipe;

	if (id->idVendor == 0x18D1 && (id->idProduct >= 0x2D00 && id->idProduct <= 0x2D05) 
							   /*&& (id->bInterfaceClass = 255 && id->bInterfaceSubClass == 255 && id->bInterfaceProtocol == 0*/){
		printk("BEAGLEDROID-USBAUDIO:  [%04X:%04X] Connected in AOA mode\n", id->idVendor, id->idProduct);
		printk("bInterfaceClass = %d  bInterfaceSubClass = %d   bInterfaceProtocol = %d bInterfaceNumber: %d\n", id->bInterfaceClass, id->bInterfaceSubClass, id->bInterfaceProtocol, id->bInterfaceNumber);

		intfcount++;

		printk("BEAGLEDROID-USBAUDIO: intfcount = %d\n", intfcount);


		printk("BEAGLEDROID-USBAUDIO: intf.num_altsetting: %d\n", intf->num_altsetting);
		if (intf->altsetting != NULL)
			printk("BEAGLEDROID-USBAUDIO: intf->altsetting[0].des.bNumEndpoints : %d\n", intf->altsetting[0].desc.bNumEndpoints);

		interface_descriptor = intf->cur_altsetting;

//return 0;
		/* Device structure */
		beagleaudio = kzalloc(sizeof(struct beagleaudio), GFP_KERNEL);
		if (beagleaudio == NULL)
			return -ENOMEM;
		beagleaudio->dev = dev;
		beagleaudio->udev = usb_get_dev(interface_to_usbdev(intf));
		//beagleaudio->bulk_out_pipe = bulkoutpipe;


		for(i=0; i<interface_descriptor->desc.bNumEndpoints; i++){
			endpoint = &interface_descriptor->endpoint[i].desc;

			if (((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) && 
				(endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK
			   ){
					//andromon_usb->bulk_out_endpointAddr = endpoint->bEndpointAddress;
					printk("BEAGLEDROID-USBAUDIO: Bulk out endpoint");

					beagleaudio->bulk_out_endpointAddr = endpoint->bEndpointAddress;
					beagleaudio->bulk_out_pipe = usb_sndbulkpipe(beagleaudio->udev, endpoint->bEndpointAddress);
					printk("BULK OUT PIPE = %d\n", beagleaudio->bulk_out_pipe);

					break;
				}

			/*if (!andromon_usb->bulk_in_endpointAddr &&
				((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) && 
				(endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK
			   ){
					andromon_usb->bulk_in_endpointAddr = endpoint->bEndpointAddress;
					Debug_Print("ANDROMON", "Bulk in endpoint");
				}*/
		}

		for(i=0; i<interface_descriptor->desc.bNumEndpoints; i++){
			endpoint = &interface_descriptor->endpoint[i].desc;

			if (((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) && 
				(endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK
			   ){
					inputPipe = usb_rcvbulkpipe(beagleaudio->udev, endpoint->bEndpointAddress);
					interval = endpoint->bInterval;
					printk("Bulk in endpoint\n");
					break;
				}
		}

		/* Packet size is split into 11 bits of base size and count of
		 * extra multiplies of it.*/
//		size = usb_endpoint_maxp(&intf->altsetting[1].endpoint[0].desc);
//		size = (size & 0x07ff) * (((size & 0x1800) >> 11) + 1);

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

		printk("prepare input port\n");
		beagleaudio->irq = usb_alloc_urb(0, GFP_KERNEL);
		beagleaudio->inBuffer = kmalloc(8, GFP_KERNEL);
		usb_fill_bulk_urb(beagleaudio->irq, beagleaudio->udev, inputPipe,
				 beagleaudio->inBuffer, 8,
				 usb_kbd_irq, beagleaudio);
		usb_submit_urb(beagleaudio->irq, GFP_KERNEL);

		dev_info(dev, "BeagleBone Audio Grabber Driver\n");

		return 0;
	}
	else/* if(intfcount == 0)*/{
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
	{ USB_DEVICE(0x0bb4, 0x0ff9) },
	{ USB_DEVICE(0x0bb4, 0x0cb0) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x4E41, 255, 255, 0) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x4E42, 255, 255, 0) },
	{ USB_DEVICE(0x05c6, 0x6764) },
	//{ USB_DEVICE(0x18D1, 0x2D00) },
	//{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D01, 255, 66, 1) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D04, 255, 255, 0) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D05, 255, 255, 0) },
	//{ USB_DEVICE(0x18D1, 0x2D01) },
	//{ USB_DEVICE(0x18D1, 0x2D02) },
	//{ USB_DEVICE(0x18D1, 0x2D04) },
	{}
};

/*struct usb_device_id beagleaudio_id_table[] = {
	{ USB_DEVICE(0x18D1, 0x4E41) },
	{ USB_DEVICE(0x18D1, 0x4E42) },
	{ USB_DEVICE(0x05c6, 0x6764) },
	{ USB_DEVICE(0x18D1, 0x2D00) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D01, 255, 66, 1) },
	{ USB_DEVICE(0x18D1, 0x2D05) },
//	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x4e42, 255, 255, 0) },
//	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D05, 255, 255, 0) },
//	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D01, 255, 66, 1) },
//	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D05, 255, 66, 1) },
	{}
};*/
MODULE_DEVICE_TABLE(usb, beagleaudio_id_table);

MODULE_AUTHOR("Azizul Hakim");
MODULE_DESCRIPTION("BeagleBone Audio Driver");
MODULE_LICENSE("GPL");

struct usb_driver beagleaudio_usb_driver = {
	.name = "beagleaudio",
	.id_table = beagleaudio_id_table,
	.probe = beagleaudio_probe,
	.disconnect = beagleaudio_disconnect,
};

module_usb_driver(beagleaudio_usb_driver);
