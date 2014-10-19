#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "element.h"
#include "parcp-usb.h"

#define LIBUSB	0

// Change these as needed to match idVendor and idProduct in your device's device descriptor.
#ifdef VUSB
static const int VENDOR_ID = 0x16c0;
static const int PRODUCT_ID = 0x05df;
#else
static const int VENDOR_ID = 0x03eb;
static const int PRODUCT_ID = 0x204f;
#endif

#if LIBUSB
#include <libusb-1.0/libusb.h>
struct libusb_device_handle *devh = NULL;
static const int CONTROL_REQUEST_TYPE_IN = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
// From the HID spec:
static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;
static const int INTERFACE_NUMBER = 0;
static const int TIMEOUT_MS = 5000;

int Lusb_init()
{
	int device_ready = 0;
	int result;

	result = libusb_init(NULL);
	if (result >= 0)
	{
		devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);

		if (devh != NULL)
		{
			// The HID has been detected.
			// Detach the hidusb driver from the HID to enable using libusb.
			libusb_detach_kernel_driver(devh, INTERFACE_NUMBER);
			{
				result = libusb_claim_interface(devh, INTERFACE_NUMBER);
				if (result >= 0)
				{
					device_ready = 1;
				}
				else
				{
					fprintf(stderr, "libusb_claim_interface error %d\n", result);
				}
			}
		}
		else
		{
			fprintf(stderr, "Unable to find the device.\n");
		}
	}
	else
	{
		fprintf(stderr, "Unable to initialize libusb.\n");
	}

	return device_ready;
}

void Lusb_exit()
{
	// Finished using the device.
	if (devh != NULL) {
		libusb_release_interface(devh, 0);
		libusb_close(devh);
	}
	libusb_exit(NULL);
}

int hid_send_feature_report(void *dev, const BYTE *buffer, int wLength)
{
	unsigned char reportNumber = buffer[0];
	int bytes_sent = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_OUT,	// bRequestType
			HID_SET_REPORT,
			(HID_REPORT_TYPE_OUTPUT<<8)|reportNumber, // wValue
			INTERFACE_NUMBER,		// wIndex
			buffer,				// pointer to buffer
			wLength,			// wLength
			TIMEOUT_MS);

	return bytes_sent;
}

int hid_get_feature_report(void *dev, BYTE *buffer, int wLength)
{
	unsigned char reportNumber = buffer[0];
	int bytes_received = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_IN,
			HID_GET_REPORT,
			(HID_REPORT_TYPE_INPUT<<8)|reportNumber,
			INTERFACE_NUMBER,
			buffer,
			wLength,
			TIMEOUT_MS);
	return bytes_received;
}
#else
#include "/home/joy/Qt/HIDAPI/hidapi.h"
struct hid_device *devh = NULL;
#endif

int usb_init(void)
{
#if LIBUSB
	return Lusb_init();
#else
	if (hid_init()) {
		fprintf(stderr, "HID init failed\n");
		return FALSE;
	}

        devh = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
	if (!devh) {
		fprintf(stderr, "HID open failed\n");
		return FALSE;
	}

	return TRUE;
#endif
}

void usb_exit()
{
#if LIBUSB
	Lusb_exit();
#else
	if (devh != NULL) {
		hid_close(devh);
	}
	hid_exit();
#endif
}

int usb_send(const BYTE *block, int len)
{
	BYTE buf[64+1];
	buf[0] = 0; // report number for HIDAPI
	memcpy(buf+1, block, len);
	int ret = hid_send_feature_report(devh, buf, len+1); // len should be exactly size of descriptor for MS-Windows
	if (ret > 0)
		ret--; // correct returned number for HIDAPI
	return ret;
}

int usb_receive(BYTE *block, int len)
{
	BYTE buf[64+1];
	buf[0] = 0; // report number for HIDAPI
	int ret = hid_get_feature_report(devh, buf, sizeof(buf));
	if (ret > 0) {
		ret--; // correct returned number for HIDAPI
		// fprintf(stderr, "usb_receive() copying %d bytes\n", len);
		memcpy(block, buf+1, len);
	}
	return ret;
}

void set_mode(unsigned char output)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[2];
	buf[0] = 0x05; // mode
	buf[1] = output;
	while(bytes_sent < 0) {
		bytes_sent = usb_send(buf, 2);
		if (bytes_sent != 2) {
			fprintf(stderr, "%d. error sending set_mode: %d\n", error_counter, bytes_sent);
			if (++error_counter >= 9)
				return;
		}
	}
	// fprintf(stderr, "set_mode %s\n", output ? "OUT" : "IN");
}

void set_strobe(unsigned char strobe)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[2];
	buf[0] = 0x06; // strobe
	buf[1] = strobe;
	while(bytes_sent < 0) {
		bytes_sent = usb_send(buf, 2);
		if (bytes_sent != 2) {
			fprintf(stderr, "%d. error sending set_strobe: %d\n", error_counter, bytes_sent);
			if (++error_counter >= 9)
				return;
		}
	}
	// fprintf(stderr, "set_strobe %s\n", strobe ? "HIGH" : "LOW");
}

int get_busy()
{
	unsigned char buf[USB_BLOCK_SIZE+4];
	buf[0] = 0x01;
	int error_counter = 0;
	int bytes_received = -1;
	while(bytes_received < 0) {
		bytes_received = hid_get_feature_report(devh, buf, sizeof(buf));
		if (bytes_received != sizeof(buf)) {
			if (error_counter)
				fprintf(stderr, "%d. error receiving get_busy, received %d bytes\n", error_counter, bytes_received);
			else
				fputc('\\', stderr);
			if (++error_counter >= 9)
				return -1;
		}
	}
	BOOLEAN busy = buf[0];
	// fprintf(stderr, "get_busy OK: %s\n", busy ? "HIGH" : "LOW");
	return busy;
}

int usb_receive_block(BYTE *data_in, int n)
{
	unsigned char buf[USB_BLOCK_SIZE+4+1];
	memset(buf, 0, sizeof(buf));
	int bytes_received = -1;
	// int error_counter = 9;
	while(bytes_received < 0) {
		bytes_received = usb_receive(buf, sizeof(buf));
		if (bytes_received != n) {
			fprintf(stderr, "Fatal error receiving block(%d) = %d\n", n, bytes_received);
			// if (!error_counter--)
				return -1;
		}
	}
	memcpy(data_in, buf, n);
	// fprintf(stderr, "read_block OK, received %d bytes: [%02x %02x %02x]\n", bytes_received, data_in[0], data_in[1], data_in[2]);
	return bytes_received;
}

int usb_transmit_block(const BYTE *data_out, int n)
{
	int bytes_sent = -1;
	int error_counter = 0;
	while(bytes_sent < 0) {
		bytes_sent = usb_send(data_out, n);
		if (bytes_sent != n) {
			if (error_counter)
				fprintf(stderr, "%d. error sending block(%d) = %d\n", error_counter, n, bytes_sent);
			else
				fputc('|', stderr);
			if (++error_counter >= 9)
				return -1;
		}
	}
	// fprintf(stderr, "write_block OK, sent %d bytes\n", bytes_sent);
	return bytes_sent;
}

int usb_set_client_read_size(long n)
{
	unsigned char buffer[4];
	buffer[0] = 0x01; // 0x01 = client read
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	int ret = usb_transmit_block(buffer, 4);
	// fprintf(stderr, "usb_set_client_read_size(%ld) = %d\n", n, ret);
	return ret;
}

int usb_set_server_read_size(long n)
{
	unsigned char buffer[4];
	buffer[0] = 0x02; // 0x02 = server read
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	return usb_transmit_block(buffer, 4);
}

int usb_set_client_write_size(long n, const BYTE *block)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x03; // 0x03 = client write
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	memcpy(buffer + 4, block, USB_BLOCK_SIZE);
	// fprintf(stderr, "usb_set_client_write_size(%ld): %d %d %d...\n", n, block[0], block[1], block[2]);
	return usb_transmit_block(buffer, USB_BLOCK_SIZE+4);
}

int usb_set_server_write_size(long n, const BYTE *block)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x04; // 0x04 = server write
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	memcpy(buffer + 4, block, USB_BLOCK_SIZE);
	return usb_transmit_block(buffer, USB_BLOCK_SIZE+4);
}

int usb_read_block(BYTE *block, long offset, int n)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	memset(buffer, 0, sizeof(buffer));
	int ret = usb_receive_block(buffer, sizeof(buffer));
	// TODO: check that buffer[0] == n;
	// TODO: check that buffer[1-3] == offset;
	// fprintf(stderr, "usb_read_block(%ld, %d) = %d, [%02x %02x %02x %02x %02x %02x]\n", offset, n, ret, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	memcpy(block + offset, buffer+4, n);
	return ret;
}

int usb_write_block(const BYTE *block, long offset, int n)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x08; // data transmit
	buffer[1] = offset >> 16;
	buffer[2] = offset >> 8;
	buffer[3] = offset;
	memcpy(buffer+4, block + offset, n);
	int ret = usb_transmit_block(buffer, 4+n);
	// fprintf(stderr, "usb_write_block(%ld, %d) = %d\n", offset, n, ret);
	return ret;
}

#if 0
int main()
{
	BYTE buf[USB_BLOCK_SIZE+2];

	int strobe = 0;

	if (usb_init() == 0) { fprintf(stderr, "USB init failed\n"); return 1; }

	set_mode(TRUE);

	int i;
	for(i=0; i<5; i++) {
#if 1
		printf("STROBE %s\n", strobe ? "HIGH" : "LOW");
		set_strobe(strobe);
	set_mode(strobe);
		strobe = !strobe;
		printf("BUSY %s\n", get_busy() ? "HIGH" : "LOW");
#else
		buf[0] = buf[1] = buf[2] = buf[3] = 0;
		usb_client_read_block(buf, 2, TRUE);
		printf("[0]=%d, [1]=%d, [2]=%d, [3]=%d\n", buf[0], buf[1], buf[2], buf[3]);
#endif
		sleep(1);
	}

	usb_exit();
	return 0;
}
#endif
