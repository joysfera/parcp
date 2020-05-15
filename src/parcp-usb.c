#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "element.h"
#include "parcp-usb.h"

static const int VENDOR_ID = 0x03eb;
static const int PRODUCT_ID = 0x204f;

#define ERROR_RETRIES	4

#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )

#include <hidapi/hidapi.h>
hid_device *devh = NULL;

int usb_init(const char *serial)
{
	const int MAX_STR = 255;
	wchar_t wstr[MAX_STR];

	if (hid_init()) {
		fprintf(stderr, "HID init failed\n");
		return FALSE;
	}

	wchar_t *serial_number = NULL;
	if (serial != NULL && strlen(serial) > 0) {
		int i;
		for(i = 0; i <= strlen(serial) && i < MAX_STR; i++)
			wstr[i] = btowc(serial[i]);
		serial_number = wstr;
	}

	devh = hid_open(VENDOR_ID, PRODUCT_ID, serial_number);
	if (!devh) {
		if (serial_number == NULL)
			fprintf(stderr, "HID open failed\n");
		else
			fprintf(stderr, "HID open of serial '%ls' failed\n", serial_number);
		return FALSE;
	}

	int res = hid_get_product_string(devh, wstr, MAX_STR);
	if (wcscmp(L"PARCP", wstr)) {
		printf("This is not PARCP USB! Product string of the attached device: '%ls'\n", wstr);
		return FALSE;
	}

	if (serial_number == NULL) {
		res = hid_get_serial_number_string(devh, wstr, MAX_STR);
		if (res == 0)
			printf("PARCP USB Serial Number: %ls\n", wstr);
	}

	return TRUE;
}

void usb_exit()
{
	if (devh != NULL) {
		hid_close(devh);
	}
	hid_exit();
}

int usb_send(const BYTE *block, int len)
{
	BYTE buf[64+1];
	buf[0] = 0; // report number for HIDAPI
	memcpy(buf+1, block, len);
	int ret = hid_send_feature_report(devh, buf, sizeof(buf));
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
		memcpy(block, buf+1, len);
	}
	return ret;
}

MYBOOL set_mode(unsigned char output)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[2];
#if IODEBUG
	fprintf(stderr, "set_mode %s\n", output ? "OUT" : "IN");
#endif
	buf[0] = 0x05; // mode
	buf[1] = output;
	while(bytes_sent < 2) {
		bytes_sent = usb_send(buf, 2);
		if (bytes_sent < 2) {
			if (error_counter)
				fprintf(stderr, "%d. error sending set_mode: %d\n", error_counter, bytes_sent);
#if USBDEBUG
			else
				fputc('^', stderr);
#endif
			if (++error_counter >= ERROR_RETRIES)
				return FALSE;
		}
	}
	return TRUE;
}

MYBOOL set_strobe(unsigned char strobe)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[2];
#if IODEBUG
	fprintf(stderr, "set_strobe %s\n", strobe ? "HIGH" : "LOW");
#endif
	buf[0] = 0x06; // strobe
	buf[1] = strobe;
	while(bytes_sent < 2) {
		bytes_sent = usb_send(buf, 2);
		if (bytes_sent < 2) {
			if (error_counter)
				fprintf(stderr, "%d. error sending set_strobe: %d\n", error_counter, bytes_sent);
#if USBDEBUG
			else
				fputc('/', stderr);
#endif
			if (++error_counter >= ERROR_RETRIES)
				return FALSE;
		}
	}
	return TRUE;
}

MYBOOL parcpusb_command(unsigned char command)
{
	int bytes_sent = -1;
	int error_counter = 0;
	unsigned char buf[2];
#if IODEBUG
	fprintf(stderr, "parcpusb_command %d\n", command);
#endif
	buf[0] = 0x07; // command
	buf[1] = command;
	while(bytes_sent < 2) {
		bytes_sent = usb_send(buf, 2);
		if (bytes_sent < 2) {
			if (error_counter)
				fprintf(stderr, "%d. error sending parcpusb_command: %d\n", error_counter, bytes_sent);
#if USBDEBUG
			else
				fputc('~', stderr);
#endif
			if (++error_counter >= ERROR_RETRIES)
				return FALSE;
		}
	}
	return TRUE;
}

int usb_receive_block(BYTE *data_in, int n)
{
	unsigned char buf[USB_BLOCK_SIZE+4+1];
	memset(buf, 0, sizeof(buf));
	int bytes_received = -1;
	int error_counter = 0;
	while(bytes_received < n) {
		bytes_received = usb_receive(buf, sizeof(buf));
		if (bytes_received < n) {
			if (error_counter)
				fprintf(stderr, "Fatal error receiving block(%d) = %d\n", n, bytes_received);
#if USBDEBUG
			else
				fputc('&', stderr);
#endif
// must not repeat usb_receive_block or the client-server sync breaks
//			if (++error_counter >= ERROR_RETRIES)
				return -1;
		}
	}
	memcpy(data_in, buf, n);
	return MIN(bytes_received, n);
}

int usb_transmit_block(const BYTE *data_out, int n)
{
	int bytes_sent = -1;
	int error_counter = 0;
	while(bytes_sent < n) {
		bytes_sent = usb_send(data_out, n);
		if (bytes_sent < n) {
			if (error_counter)
				fprintf(stderr, "%d. error sending block(%d) = %d\n", error_counter, n, bytes_sent);
#if USBDEBUG
			else
				fputc('|', stderr);
#endif
			if (++error_counter >= ERROR_RETRIES)
				return -1;
		}
	}
	return MIN(bytes_sent, n);
}

int get_busy()
{
	unsigned char buf[6];
	// requires new firmware and two roundtrips just to get the BUSY status
	parcpusb_command(0);	// any command sets the global_action in firmware to zero
	usb_receive_block(buf, sizeof(buf));	// read with global_action == 0 returns information packet
	// PROTOCOL_VERSION in buf[0]
	// FIRMWARE_VERSION in buf[1],buf[2]
	MYBOOL busy = buf[3];
#if IODEBUG
	fprintf(stderr, "get_busy OK: %s\n", busy ? "HIGH" : "LOW");
#endif
	if (buf[5])
		fprintf(stderr, "!! error in adapter - write offset mismatch #%d\n", buf[5]);
	return busy;
}

int usb_set_client_read_size(long n)
{
	unsigned char buffer[4];
	buffer[0] = 0x01; // 0x01 = client read
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	int sent = usb_transmit_block(buffer, 4);
	return (sent == 4) ? 0 : -1;
}

int usb_set_server_read_size(long n)
{
	unsigned char buffer[4];
	buffer[0] = 0x02; // 0x02 = server read
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	int sent = usb_transmit_block(buffer, 4);
	return (sent == 4) ? 0 : -1;
}

int usb_set_client_write_size(long n, const BYTE *block)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x03; // 0x03 = client write
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	int to_send = MIN(n, USB_BLOCK_SIZE);
	memcpy(buffer + 4, block, to_send);
#if IODEBUG
	fprintf(stderr, "usb_set_client_write_size(%ld): %d %d %d...\n", n, block[0], block[1], block[2]);
#endif
	int sent = usb_transmit_block(buffer, to_send+4);
	return (sent == to_send+4) ? 0 : -1;
}

int usb_set_server_write_size(long n, const BYTE *block)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x04; // 0x04 = server write
	buffer[1] = n >> 16;
	buffer[2] = n >> 8;
	buffer[3] = n;
	int to_send = MIN(n, USB_BLOCK_SIZE);
	memcpy(buffer + 4, block, to_send);
	int sent = usb_transmit_block(buffer, to_send+4);
	return (sent == to_send+4) ? 0 : -1;
}

int usb_read_block(BYTE *block, long offset, int n)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	assert(n <= USB_BLOCK_SIZE);
	memset(buffer, 0, sizeof(buffer));
	int received = usb_receive_block(buffer, n+4);
	int data_received = buffer[0];
	MYBOOL all_data_received = (received >= n+4 && (data_received == n || data_received-1 == n)); // -1 => correction for always even transfer size
	if (!all_data_received)
		fprintf(stderr, "!! read_block(%ld, %d) read only %d bytes\n", offset, n, data_received);
#if DEBUG
	else
		fprintf(stderr, "oo read_block(%ld, %d) OK\n", offset, n);
#endif
	long bufoff = (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	if (bufoff != offset)
		fprintf(stderr, "!! read_block(%ld, %d) received offset %ld does not match\n", offset, n, bufoff);
#if IODEBUG
	fprintf(stderr, "usb_read_block(%ld, %d) = [$%02x $%02x $%02x $%02x $%02x $%02x]\n", offset, n, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
#endif
	memcpy(block + offset, buffer+4, n);
	return all_data_received ? 0 : -1;
}

int usb_write_block(const BYTE *block, long offset, int n)
{
	unsigned char buffer[USB_BLOCK_SIZE+4];
	buffer[0] = 0x08; // data transmit
	buffer[1] = offset >> 16;
	buffer[2] = offset >> 8;
	buffer[3] = offset;
	memcpy(buffer+4, block + offset, n);
#if IODEBUG
	fprintf(stderr, "usb_write_block(%ld, %d)\n", offset, n);
#endif
	int sent = usb_transmit_block(buffer, n+4);
	return (sent == n+4) ? 0 : -1;
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
