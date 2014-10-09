#include "element.h"
#ifdef VUSB
#define USB_BLOCK_SIZE	62
#else
#define USB_BLOCK_SIZE	62
#endif
int usb_init(void);
void usb_exit();
void set_mode(unsigned char output);
void set_strobe(unsigned char strobe);
int get_busy();
int usb_client_read_block(BYTE *block, int n, BOOLEAN first);
int usb_client_write_block(const BYTE *block, int n, BOOLEAN first);
int usb_server_read_block(BYTE *block, int n, BOOLEAN first);
int usb_server_write_block(const BYTE *block, int n, BOOLEAN first);
