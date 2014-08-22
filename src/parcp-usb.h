#include "element.h"
#define USB_BLOCK_SIZE	254
int usb_init(void);
void usb_exit();
void set_strobe(unsigned char strobe);
int get_busy();
void write_byte(unsigned char value);
int read_byte();
void set_mode(unsigned char output);
int usb_client_read_block(BYTE *block, int n, BOOLEAN first);
int usb_client_write_block(BYTE *block, int n, BOOLEAN first);
