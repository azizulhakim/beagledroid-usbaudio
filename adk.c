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

static void usb_kbd_irq(struct urb *urb)
{
	struct beagleaudio *beagleaudio = urb->context;
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
	default:		/* error */
		goto resubmit;
	}

	for (i=0; i<8; i++){
		printk("%d ", beagleaudio->inBuffer[i]);
	}

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
	int datalen;
	u8 data[8];
	struct device *dev = &intf->dev;
	struct beagleaudio *beagleaudio;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface_descriptor;
	struct usb_endpoint_descriptor *endpoint;
	int interval;
	unsigned int inputPipe = 0;

	if (id->idVendor == 0x18D1 && (id->idProduct >= 0x2D00 && id->idProduct <= 0x2D05)){
		printk("BEAGLEDROID-USBAUDIO:  [%04X:%04X] Connected in AOA mode\n", id->idVendor, id->idProduct);
		printk("bInterfaceClass = %d  bInterfaceSubClass = %d   bInterfaceProtocol = %d bInterfaceNumber: %d\n", id->bInterfaceClass, id->bInterfaceSubClass, id->bInterfaceProtocol, id->bInterfaceNumber);

		intfcount++;

		printk("BEAGLEDROID-USBAUDIO: intfcount = %d\n", intfcount);


		printk("BEAGLEDROID-USBAUDIO: intf.num_altsetting: %d\n", intf->num_altsetting);
		if (intf->altsetting != NULL)
			printk("BEAGLEDROID-USBAUDIO: intf->altsetting[0].des.bNumEndpoints : %d\n", intf->altsetting[0].desc.bNumEndpoints);

		interface_descriptor = intf->cur_altsetting;

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
					printk("BEAGLEDROID-USBAUDIO: Bulk out endpoint");

					beagleaudio->bulk_out_endpointAddr = endpoint->bEndpointAddress;
					beagleaudio->bulk_out_pipe = usb_sndbulkpipe(beagleaudio->udev, endpoint->bEndpointAddress);

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

		usb_set_intfdata(intf, beagleaudio);


		ret = beagleaudio_audio_init(beagleaudio);
		if (ret < 0)
			goto beagleaudio_audio_fail;

		beagleaudio->irq = usb_alloc_urb(0, GFP_KERNEL);
		beagleaudio->inBuffer = kmalloc(8, GFP_KERNEL);
		usb_fill_bulk_urb(beagleaudio->irq, beagleaudio->udev, inputPipe,
				 beagleaudio->inBuffer, 8,
				 usb_kbd_irq, beagleaudio);
		usb_submit_urb(beagleaudio->irq, GFP_KERNEL);

		dev_info(dev, "BeagleBone Audio Grabber Driver\n");

		return 0;
	}
	else{
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

		//printk("<-----Audio Request------  %d ----------------->\n", SendAudioActivationRequest(usb_dev));
		SendAOAActivationRequest(usb_dev);

		return 0;
	}

	goto exit;

beagleaudio_audio_fail:
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

	usb_put_dev(beagleaudio->udev);
	beagleaudio->udev = NULL;
}

struct usb_device_id beagleaudio_id_table[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x4E41, 255, 255, 0) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x4E42, 255, 255, 0) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x18D1, 0x2D01, 255, 255, 0) },
	{}
};

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
