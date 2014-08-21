#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include "element.h"
#include "parcp-usb.h"

// Values for bmRequestType in the Setup transaction's Data packet.

static const int CONTROL_REQUEST_TYPE_IN = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

// From the HID spec:

static const int HID_GET_REPORT = 0x01;
static const int HID_GET_IDLE   = 0x02;
static const int HID_GET_PROTOCOL = 0x03;
static const int HID_SET_REPORT = 0x09;
static const int HID_SET_IDLE   = 0x0a;
static const int HID_SET_PROTOCOL = 0x0b;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;

static const int INTERFACE_NUMBER = 0;
static const int TIMEOUT_MS = 5000;

int exchange_feature_reports_via_control_transfers(libusb_device_handle *devh);
int exchange_input_and_output_reports_via_control_transfers(libusb_device_handle *devh);
int exchange_input_and_output_reports_via_interrupt_transfers(libusb_device_handle *devh);

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

struct libusb_device_handle *devh = NULL;

int usb_init(void)
{
	// Change these as needed to match idVendor and idProduct in your device's device descriptor.
	static const int VENDOR_ID = 0x16c0;
	static const int PRODUCT_ID = 0x05df;

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

void usb_exit()
{
	// Finished using the device.
	if (devh != NULL) {
		libusb_release_interface(devh, 0);
		libusb_close(devh);
	}
	libusb_exit(NULL);
}

void set_strobe(unsigned char strobe)
{
	int bytes_sent = -1;
	int error_counter = 5;
	while(bytes_sent < 0) {
		bytes_sent = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_OUT,	// bRequestType
			HID_SET_IDLE,			// bRequest
			(HID_REPORT_TYPE_OUTPUT<<8)|strobe, // wValue
			INTERFACE_NUMBER,		// wIndex
			NULL,				// pointer to buffer
			0,				// wLength
			TIMEOUT_MS);

		if (bytes_sent < 0) {
			fprintf(stderr, "Error sending set_strobe\n");
			if (!error_counter--)
				return -1;
		}
	}
}

int get_busy()
{
	unsigned char data_in[1];
	int error_counter = 5;
	int bytes_received = 0;
	while(bytes_received != 1) {
		bytes_received = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_IN,
			HID_GET_IDLE,
			(HID_REPORT_TYPE_INPUT<<8)|0x00,
			INTERFACE_NUMBER,
			data_in,
			sizeof(data_in),
			TIMEOUT_MS);

		if (bytes_received != 1) {
			fprintf(stderr, "Error receiving get_busy\n");
			if (!error_counter--)
				return -1;
		}
	}
	return data_in[0];
}

void write_byte(unsigned char value)
{
	int bytes_sent = -1;
	int error_counter = 5;
	while(bytes_sent < 0) {
		bytes_sent = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_OUT,	// bRequestType
			HID_SET_PROTOCOL,		// bRequest
			(HID_REPORT_TYPE_OUTPUT<<8)|value, // wValue
			INTERFACE_NUMBER,		// wIndex
			NULL,				// pointer to buffer
			0,				// wLength
			TIMEOUT_MS);
		if (bytes_sent < 0) {
			fprintf(stderr, "Error sending write_byte\n");
			if (!error_counter--)
				return -1;
		}
	}
}

int read_byte()
{
	unsigned char data_in[1];
	int bytes_received = 0;
	int error_counter = 5;
	while(bytes_received != 1) {
		bytes_received = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_IN,
			HID_GET_PROTOCOL,
			(HID_REPORT_TYPE_INPUT<<8)|0x00,
			INTERFACE_NUMBER,
			data_in,
			sizeof(data_in),
			TIMEOUT_MS);

		if (bytes_received != 1) {
			fprintf(stderr, "Error receiving read_byte\n");
			if (!error_counter--)
				return -1;
		}
	}
	return data_in[0];
}

int usb_client_read_block(BYTE *block, int n, BOOLEAN first)
{
	unsigned char data_in[USB_BLOCK_SIZE+1];
	int bytes_received = 0;
	int error_counter = 9;
	while(bytes_received != n) {
		bytes_received = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_IN,
			HID_GET_IDLE,
			(HID_REPORT_TYPE_INPUT<<8)|(first ? 0x01 : 0x02),
			INTERFACE_NUMBER,
			data_in,
			n,
			TIMEOUT_MS);

		if (bytes_received != n) {
			fprintf(stderr, "Error receiving block(%p, %d) = %d\n", block, n, bytes_received);
			if (!error_counter--)
				return -1;
		}
	}
	memcpy(block, data_in, n);
	return 0;
}

void set_mode(unsigned char output)
{
	int bytes_sent = -1;
	int error_counter = 5;
	while(bytes_sent < 0) {
		bytes_sent = libusb_control_transfer(
			devh,
			CONTROL_REQUEST_TYPE_OUT,	// bRequestType
			HID_SET_IDLE,			// bRequest
			(0<<8)|output, // wValue
			INTERFACE_NUMBER,		// wIndex
			NULL,				// pointer to buffer
			0,				// wLength
			TIMEOUT_MS);

		if (bytes_sent < 0) {
			fprintf(stderr, "Error sending set_mode: %d\n", bytes_sent);
			if (!error_counter--)
				return -1;
		}
	}
}
#if 0
int main()
{
	int strobe = 0;

	if (usb_init() == 0) return 1;

	while(1) {
		set_strobe(strobe);
		strobe = !strobe;
		printf("Busy %s, Data = $%02x\n", get_busy() ? "HIGH" : "LOW", read_byte());
		sleep(1);
	}

	usb_exit();
	return 0;
}
#endif
