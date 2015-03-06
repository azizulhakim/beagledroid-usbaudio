#ifndef AOA_H
#define AOA_H	1



#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>



#define DRIVER_VERSION ""
#define DRIVER_AUTHOR "Azizul Hakim <azizulfahim2002@gmail.com>"
#define DRIVER_DESC "USB HID Boot Protocol keyboard driver"
#define DRIVER_LICENSE "GPL"

#define REQ_GET_PROTOCOL				51
#define REQ_SEND_ID						52
#define REQ_AOA_ACTIVATE				53
#define ACCESSORY_REGISTER_HID			54
#define ACCESSORY_UNREGISTER_HID		55
#define ACCESSORY_SET_HID_REPORT_DESC	56
#define ACCESSORY_SEND_HID_EVENT		57
#define ACCESSORY_REGISTER_AUDIO		58

#define MANU	"BeagleBone"
#define MODEL	"BeagleBone Black"
#define DESCRIPTION	"Development platform"
#define VERSION	"1.0"
#define URI		"http://beagleboard.org/"
#define SERIAL	"42"

#define ID_MANU		0
#define ID_MODEL	1
#define ID_DESC		2
#define ID_VERSION	3
#define ID_URI		4
#define ID_SERIAL	5

#define VAL_AOA_REQ		0
#define VAL_HID_MOUSE	1234
#define VAL_NO_AUDIO	0
#define VAL_AUDIO		1


int GetProtocol(struct usb_device *usbdev, char *buffer);
int SendIdentificationInfo(struct usb_device *usbdev, int id_index, char *id_info);
int SendAOAActivationRequest(struct usb_device *usbdev);
int SendAudioActivationRequest(struct usb_device *usbdev);
int SetConfiguration(struct usb_device *usbdev, char *buffer);
#endif
