int usb_init(void);
void usb_exit();
void set_strobe(unsigned char strobe);
int get_busy();
void write_byte(unsigned char value);
int read_byte();
void set_mode(unsigned char output);
