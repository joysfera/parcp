#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "/home/joy/Qt/HIDAPI/hidapi.h"
#include "element.h"
#include "parcp-usb.h"

static const int TIMEOUT_MS = 5000;

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

#ifdef VUSB
#define UO	0
#else
#define UO	1
#endif

struct hid_device *devh = NULL;

int usb_init(void)
{
	// Change these as needed to match idVendor and idProduct in your device's device descriptor.
#ifdef VUSB
	static const int VENDOR_ID = 0x16c0;
	static const int PRODUCT_ID = 0x05df;
#else
	static const int VENDOR_ID = 0x03eb;
	static const int PRODUCT_ID = 0x204f;
#endif

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
}

void usb_exit()
{
	// Finished using the device.
	if (devh != NULL) {
		hid_close(devh);
	}
	hid_exit();
}

void set_strobe(unsigned char strobe)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[USB_BLOCK_SIZE+2];
	buf[0] = ( strobe ? 0x06: 0x05 );
	while(bytes_sent < 0) {
		bytes_sent = hid_send_feature_report(devh, buf, 1);
		if (bytes_sent < 0) {
			fprintf(stderr, "%d. error sending set_strobe: %d\n", error_counter, bytes_sent);
			if (++error_counter >= 9)
				return;
		}
	}
	//fprintf(stderr, "set_strobe OK\n");
}

void set_mode(unsigned char output)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[USB_BLOCK_SIZE+2];
	buf[0] = ( output ? 0x08: 0x07 );
	while(bytes_sent < 0) {
		bytes_sent = hid_send_feature_report(devh, buf, 1);
		if (bytes_sent < 0) {
			fprintf(stderr, "%d. error sending set_mode: %d\n", error_counter, bytes_sent);
			if (++error_counter >= 9)
				return;
		}
	}
	//fprintf(stderr, "set_mode OK\n");
}

int get_busy()
{
	unsigned char buf[USB_BLOCK_SIZE+2];
	buf[0] = 0x05;
	int error_counter = 0;
	int bytes_received = -1;
	while(bytes_received < 0) {
		bytes_received = hid_get_feature_report(devh, buf, 1+UO);
		if (bytes_received < 0) {
			fprintf(stderr, "%d. error receiving get_busy, received %d bytes\n", error_counter, bytes_received);
			if (++error_counter >= 9)
				return -1;
		}
	}
	BOOLEAN busy = buf[UO];
	// fprintf(stderr, "get_busy OK: %s\n", busy ? "HIGH" : "LOW");
	return busy;
}

int usb_read_block(BYTE *data_in, int n)
{
	int bytes_received = -1;
	// int error_counter = 9;
	while(bytes_received < 0) {
		bytes_received = hid_get_feature_report(devh, data_in, n+UO);
		if (bytes_received != n+UO) {
			fprintf(stderr, "Fatal error receiving block(%d) = %d\n", n, bytes_received);
			// if (!error_counter--)
				return -1;
		}
	}
	// fprintf(stderr, "read_block OK, received %d bytes\n", bytes_received);
	return 0;
}

int usb_write_block(const BYTE *data_out, int n)
{
	int bytes_sent = -1;
	int error_counter = 0;
	while(bytes_sent < 0) {
		bytes_sent = hid_send_feature_report(devh, data_out, n+1);
		if (bytes_sent != n+1) {
			fprintf(stderr, "%d. error sending block(%d) = %d\n", error_counter, n, bytes_sent);
			if (++error_counter >= 9)
				return -1;
		}
	}
	// fprintf(stderr, "write_block OK, sent %d bytes\n", bytes_sent);
	return 0;
}

int usb_client_read_block(BYTE *block, int n, BOOLEAN first)
{
	unsigned char data_in[USB_BLOCK_SIZE+2];
	data_in[0] = (first ? 0x01 : 0x02);
	int ret = usb_read_block(data_in, n);
	memcpy(block, data_in+UO, n);
	return ret;
}

int usb_server_read_block(BYTE *block, int n, BOOLEAN first)
{
	unsigned char data_in[USB_BLOCK_SIZE+2];
	data_in[0] = (first ? 0x03 : 0x04);
	int ret = usb_read_block(data_in, n);
	memcpy(block, data_in+UO, n);
	return ret;
}

int usb_client_write_block(const BYTE *block, int n, BOOLEAN first)
{
	unsigned char data_out[USB_BLOCK_SIZE+2];
	data_out[0] = (first ? 0x01 : 0x02);
	memcpy(data_out+1, block, n);
	return usb_write_block(data_out, n);
}

int usb_server_write_block(const BYTE *block, int n, BOOLEAN first)
{
	unsigned char data_out[USB_BLOCK_SIZE+2];
	data_out[0] = (first ? 0x03 : 0x04);
	memcpy(data_out+1, block, n);
	return usb_write_block(data_out, n);
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
