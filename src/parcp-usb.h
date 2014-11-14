#include "element.h"
#define USB_BLOCK_SIZE	60
int usb_init(const char *serial);
void usb_exit();
MYBOOL set_mode(unsigned char output);
MYBOOL set_strobe(unsigned char strobe);
MYBOOL parcpusb_command(unsigned char command);
int get_busy();
int usb_set_client_read_size(long n);
int usb_set_server_read_size(long n);
int usb_set_client_write_size(long n, const BYTE *);
int usb_set_server_write_size(long n, const BYTE *);
int usb_read_block(BYTE *block, long offset, int n);
int usb_write_block(const BYTE *block, long offset, int n);
