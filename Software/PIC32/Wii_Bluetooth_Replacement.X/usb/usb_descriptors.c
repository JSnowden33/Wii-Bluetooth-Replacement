/*
 * USB Descriptors file
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license as
 * this file.
 *
 * M-Stack is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  For details, see sections 7, 8, and 9
 * of the Apache License, version 2.0 which apply to this file.  If you have
 * purchased a commercial license for this software from Signal 11 Software,
 * your commerical license superceeds the information in this header.
 *
 * Alan Ott
 * Signal 11 Software
 */

#include "usb_config.h"
#include "usb.h"
#include "usb_ch9.h"
#include "usb_hid.h"

#ifdef __C18
#define ROMPTR rom
#else
#define ROMPTR
#endif

/** DFU Functional Descriptor **/
struct dfu_functional_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bmAttributes;
	uint16_t wDetachTimeOut;
	uint16_t wTransferSize;
} __attribute__((packed));

/* Configuration Packet
 *
 * This packet contains a configuration descriptor, one or more interface
 * descriptors, class descriptors(optional), and endpoint descriptors for a
 * single configuration of the device.  This struct is specific to the
 * device, so the application will need to add any interfaces, classes and
 * endpoints it intends to use.  It is sent to the host in response to a
 * GET_DESCRIPTOR[CONFIGURATION] request.
 *
 * While Most devices will only have one configuration, a device can have as
 * many configurations as it needs.  To have more than one, simply make as
 * many of these structs as are required, one for each configuration.
 *
 * An instance of each configuration packet must be put in the
 * usb_application_config_descs[] array below (which is #defined in
 * usb_config.h) so that the USB stack can find it.
 *
 * See Chapter 9 of the USB specification from usb.org for details.
 *
 * It's worth noting that adding endpoints here does not automatically
 * enable them in the USB stack.  To use an endpoint, it must be declared
 * here and also in usb_config.h.
 *
 * The configuration packet below is for the mouse demo application.
 * Yours will of course vary.
 */
struct configuration_1_packet {
	struct configuration_descriptor  config;

	struct interface_descriptor      interface0;
	struct endpoint_descriptor       ep0_0;
	struct endpoint_descriptor       ep0_1;
	struct endpoint_descriptor       ep0_2;

	struct interface_descriptor      interface1_0;
	struct endpoint_descriptor       ep1_0_0;
	struct endpoint_descriptor       ep1_0_1;

	struct interface_descriptor      interface1_1;
	struct endpoint_descriptor       ep1_1_0;
	struct endpoint_descriptor       ep1_1_1;

	struct interface_descriptor      interface1_2;
	struct endpoint_descriptor       ep1_2_0;
	struct endpoint_descriptor       ep1_2_1;

	struct interface_descriptor      interface1_3;
	struct endpoint_descriptor       ep1_3_0;
	struct endpoint_descriptor       ep1_3_1;

	struct interface_descriptor      interface1_4;
	struct endpoint_descriptor       ep1_4_0;
	struct endpoint_descriptor       ep1_4_1;

	struct interface_descriptor      interface1_5;
	struct endpoint_descriptor       ep1_5_0;
	struct endpoint_descriptor       ep1_5_1;

	struct interface_descriptor      interface2;
	struct endpoint_descriptor       ep2_0;
	struct endpoint_descriptor       ep2_1;

	struct interface_descriptor      dfu_interface;
	struct dfu_functional_descriptor dfu_fd;
};


/* Device Descriptor
 *
 * Each device has a single device descriptor describing the device.  The
 * format is described in Chapter 9 of the USB specification from usb.org.
 * USB_DEVICE_DESCRIPTOR needs to be defined to the name of this object in
 * usb_config.h.  For more information, see USB_DEVICE_DESCRIPTOR in usb.h.
 */
const ROMPTR struct device_descriptor this_device_descriptor = {
	sizeof(struct device_descriptor), // bLength
	DESC_DEVICE, // bDescriptorType
	0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
	0xe0, // Device class
	0x01, // Device Subclass
	0x01, // Protocol.
	EP_0_LEN, // bMaxPacketSize0
	0x057e, // Vendor
	0x0305, // Product
	0x0100, // device release (1.0)
	1, // Manufacturer
	2, // Product
	0, // Serial
	NUMBER_OF_CONFIGURATIONS // NumConfigurations
};

/* Configuration Packet Instance
 *
 * This is an instance of the configuration_packet struct containing all the
 * data describing a single configuration of this device.  It is wise to use
 * as much C here as possible, such as sizeof() operators, and #defines from
 * usb_config.h.  When stuff is wrong here, it can be difficult to track
 * down exactly why, so it's good to get the compiler to do as much of it
 * for you as it can.
 */
static const ROMPTR struct configuration_1_packet configuration_1 = {
	{
	  0x09, /* configuration.bLength */
	  DESC_CONFIGURATION, /* configuration.bDescriptorType */
	  216, /* configuration.wTotalLength */
	  0x04, /* configuration.bNumInterfaces */
	  0x01, /* configuration.bConfigurationValue */
	  0x00, /* configuration.iConfiguration */
	  0xa0, /* configuration.bmAttributes */ // bus powered, remote wakeup
	  0x32, /* configuration.bMaxPower */           // 100mA
	},
	{
	  // ---- INTERFACE DESCRIPTOR 0 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x0, /* interface.bInterfaceNumber */
	  0x0, /* interface.bAlternateSetting */
	  0x3, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 1 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x1, /* endpoint.bEndpointAddress */
	  EP_INTERRUPT, /* endpoint.bmAttributes */
	  0x0010, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 2 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x2, /* endpoint.bEndpointAddress */
	  EP_BULK, /* endpoint.bmAttributes */
	  0x0040, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 2 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x2, /* endpoint.bEndpointAddress */
	  EP_BULK, /* endpoint.bmAttributes */
	  0x0040, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 0 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x0, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0000, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0000, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 1 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x1, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0009, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0009, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 2 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x2, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0011, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0011, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 3 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x3, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0019, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0019, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 4 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x4, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0021, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0021, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 1 : 5 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x1, /* interface.bInterfaceNumber */
	  0x5, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xe0, /* interface.bInterfaceClass */ // Wireless
	  0x1, /* interface.bInterfaceSubClass */ // Radio Frequency
	  0x1, /* interface.bInterfaceProtocol */ // Bluetooth
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0031, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 3 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x3, /* endpoint.bEndpointAddress */
	  EP_ISOCHRONOUS, /* endpoint.bmAttributes */
	  0x0031, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 2 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x2, /* interface.bInterfaceNumber */
	  0x0, /* interface.bAlternateSetting */
	  0x2, /* interface.bNumEndpoints */
	  0xFF, /* interface.bInterfaceClass */ // Vendor specific
	  0xFF, /* interface.bInterfaceSubClass */ // Vendor specific
	  0xFF, /* interface.bInterfaceProtocol */ // Vendor specific
	  0x0, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 4 IN ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x80 | 0x4, /* endpoint.bEndpointAddress */
	  EP_BULK, /* endpoint.bmAttributes */
	  0x0020, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- ENDPOINT DESCRIPTOR EP 4 OUT ----
	  0x07, /* endpoint.bLength */
	  DESC_ENDPOINT, /* endpoint.bDescriptorType */
	  0x00 | 0x4, /* endpoint.bEndpointAddress */
	  EP_BULK, /* endpoint.bmAttributes */
	  0x0020, /* endpoint.wMaxPacketSize */
	  0x01, /* endpoint.bInterval */
	},
	{
	  // ---- INTERFACE DESCRIPTOR 3 ----
	  0x09, /* interface.bLength */
	  DESC_INTERFACE, /* interface.bDescriptorType */
	  0x3, /* interface.bInterfaceNumber */
	  0x0, /* interface.bAlternateSetting */
	  0x0, /* interface.bNumEndpoints */
	  0xfe, /* interface.bInterfaceClass */ // Application specific
	  0x01, /* interface.bInterfaceSubClass */ // Device firmware update
	  0x00, /* interface.bInterfaceProtocol */
	  0x00, /* interface.iInterface */ // String (n/a)
	},
	{
	  // ---- FUNCTIONAL DESCRIPTOR for DFU Interface ----
	  0x07, /* dfu_functional.bLength */
	  0x21, /* dfu_functional.bDescriptorType */
	  0x5, /* dfu_functional.bmAttributes */
	  0x1388, /* dfu_functional.wDetatchTimeOut */ // 5000ms
	  0x40 /* dfu_functional.wTransferSize */ // 64 bytes
	}
};

/* String Descriptors
 *
 * String descriptors are optional. If strings are used, string #0 is
 * required, and must contain the language ID of the other strings.  See
 * Chapter 9 of the USB specification from usb.org for more info.
 *
 * Strings are UTF-16 Unicode, and are not NULL-terminated, hence the
 * unusual syntax.
 */

/* String index 0, only has one character in it, which is to be set to the
   language ID of the language which the other strings are in. */
static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t lang; } str00 = {
	sizeof(str00),
	DESC_STRING,
	0x0409 // US English
};

static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[13]; } vendor_string = {
	sizeof(vendor_string),
	DESC_STRING,
	{'B','r','o','a','d','c','o','m',' ','C','o','r','p'}
};

static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[8]; } product_string = {
	sizeof(product_string),
	DESC_STRING,
	{'B','C','M','2','0','4','5','A'}
};

/* Get String function
 *
 * This function is called by the USB stack to get a pointer to a string
 * descriptor.  If using strings, USB_STRING_DESCRIPTOR_FUNC must be defined
 * to the name of this function in usb_config.h.  See
 * USB_STRING_DESCRIPTOR_FUNC in usb.h for information about this function.
 * This is a function, and not simply a list or map, because it is useful,
 * and advisable, to have a serial number string which may be read from
 * EEPROM or somewhere that's not part of static program memory.
 */
int16_t usb_application_get_string(uint8_t string_number, const void **ptr) {
	if (string_number == 0) {
		*ptr = &str00;
		return sizeof(str00);
	}
	else if (string_number == 1) {
		*ptr = &vendor_string;
		return sizeof(vendor_string);
	}
	else if (string_number == 2) {
		*ptr = &product_string;
		return sizeof(product_string);
	}
	else if (string_number == 3) {
		/* This is where you might have code to do something like read
		   a serial number out of EEPROM and return it. */
		return -1;
	}

	return -1;
}

/* Configuration Descriptor List
 *
 * This is the list of pointters to the device's configuration descriptors.
 * The USB stack will read this array looking for descriptors which are
 * requsted from the host.  USB_CONFIG_DESCRIPTOR_MAP must be defined to the
 * name of this array in usb_config.h.  See USB_CONFIG_DESCRIPTOR_MAP in
 * usb.h for information about this array.  The order of the descriptors is
 * not important, as the USB stack reads bConfigurationValue for each
 * descriptor to know its index.  Make sure NUMBER_OF_CONFIGURATIONS in
 * usb_config.h matches the number of descriptors in this array.
 */
const struct configuration_descriptor *usb_application_config_descs[] = {
	(struct configuration_descriptor*) &configuration_1,
};

STATIC_SIZE_CHECK_EQUAL(USB_ARRAYLEN(USB_CONFIG_DESCRIPTOR_MAP), NUMBER_OF_CONFIGURATIONS);
STATIC_SIZE_CHECK_EQUAL(sizeof(USB_DEVICE_DESCRIPTOR), 18);

/* HID Descriptor Function */
int16_t usb_application_get_hid_descriptor(uint8_t interface, const void **ptr) {
	
}

/** HID Report Descriptor Function */
int16_t usb_application_get_hid_report_descriptor(uint8_t interface, const void **ptr) {
	
}
