#include <math.h>

extern int time_out;

#define WAIT_LOW	{int i=TIMER+time_out; while(IS_READY) if (TIMER>i) {SET_INPUT;STROBE_HIGH;return -1;};}
#define WAIT_HIGH	{int i=TIMER+time_out; while(! IS_READY) if (TIMER>i) {SET_INPUT;STROBE_HIGH;return -1;};}

long client_read_block(BYTE *block, long n)
{
	BYTE x;
	long i = 0;

#ifdef ATARI
	if (_assembler)
		return fast_client_read_block(block, n);
#endif

#ifdef IBM
#  ifdef USB
	long ret = 0;
	long offset = 0;
	usb_set_client_read_size(n);
	while(n > 0) {
		unsigned lbl = min(USB_BLOCK_SIZE - (offset == 0 ? 1 : 0), n);
		ret = usb_read_block(block, offset, lbl);
		offset += lbl;
		n -= lbl;
	}
	return ret;
#  else
	if (!cable_type)
		return laplink_client_read_block(block, n);
#  endif
#endif

	SET_INPUT;

	while(TRUE) {
		LDPRINT("l STROBE is LOW, waiting for LOW\n");
		STROBE_LOW; WAIT_LOW;
		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 1st byte = $%x, STROBE is HIGH, waiting for HIGH\n",x);
		STROBE_HIGH; WAIT_HIGH;
		if (i>=n)
			break;

		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 2nd byte = $%x\n",x);
	}
	return 0;
}

long server_read_block(BYTE *block, long n)
{
	BYTE x;
	long i = 0;

#ifdef ATARI
	if (_assembler)
		return fast_server_read_block(block, n);
#endif

#ifdef IBM
#  ifdef USB
	long ret = 0;
	long offset = 0;
	usb_set_server_read_size(n);
	while(n > 0) {
		unsigned lbl = min(USB_BLOCK_SIZE - (offset == 0 ? 1 : 0), n);
		ret = usb_read_block(block, offset, lbl);
		offset += lbl;
		n -= lbl;
	}
	LDPRINT("l STROBE is LOW, waiting for HIGH\n");
	WAIT_HIGH;
	LDPRINT("l STROBE is HIGH\n");
	STROBE_HIGH;
	return ret;
#  else
	if (!cable_type)
		return laplink_server_read_block(block, n);
#  endif
#endif

	SET_INPUT;

	while(TRUE) {
		LDPRINT("l Waiting for LOW\n");
		WAIT_LOW;

		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 1st byte = $%x, STROBE is LOW\n",x);
		STROBE_LOW;

		if (i>=n)
			break;

		LDPRINT("l Waiting for HIGH\n");
		WAIT_HIGH;
		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH\n",x);
		STROBE_HIGH;
	}
	LDPRINT("l STROBE is LOW, waiting for HIGH\n");
	WAIT_HIGH;
	LDPRINT("l STROBE is HIGH\n");
	STROBE_HIGH;
	return 0;
}

long client_write_block(const BYTE *block, long n)
{
	BYTE x;
	long i = 0;

#ifdef ATARI
	if (_assembler)
		return fast_client_write_block(block, n);
#endif

#ifdef IBM
#  ifdef USB
	long ret = 0;
	long offset = 0;
	unsigned lbl = min(USB_BLOCK_SIZE - 1, n);
	usb_set_client_write_size(n, block);
	offset += lbl;
	n -= lbl;
	while(n > 0) {
		lbl = min(USB_BLOCK_SIZE, n);
		ret = usb_write_block(block, offset, lbl);
		if (ret < 0) break;
		offset += lbl;
		n -= lbl;
	}
	LDPRINT("l STROBE is HIGH, waiting for HIGH write_block\n");
	STROBE_HIGH;
	WAIT_HIGH;
	SET_INPUT;
	return ret; // doesn't really work, usb_client_write_block does not return anything
#  else
	if (!cable_type)
		return laplink_client_write_block(block, n);
#  endif
#endif

#ifdef IODEBUG
	GET_BYTE(x);
	if (x != 0xff) {
		LDPRINT("! WARNING! Other side is not receiving!");
		return -1;
	}
#endif

	SET_OUTPUT;

	while(TRUE) {
		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 1st byte = $%x, STROBE is LOW, waiting for LOW\n",x);
		STROBE_LOW;
		WAIT_LOW;

		if (i>=n)
			break;

		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH, waiting for HIGH\n",x);
		STROBE_HIGH;
		WAIT_HIGH;
	}
	LDPRINT("l STROBE is HIGH, waiting for HIGH\n");
	STROBE_HIGH;
	WAIT_HIGH;
	SET_INPUT;
	return 0;
}

long server_write_block(const BYTE *block, long n)
{
	BYTE x;
	long i = 0;

#ifdef ATARI
	if (_assembler)
		return fast_server_write_block(block, n);
#endif

#ifdef IBM
#  ifdef USB
	long ret = 0;
	long offset = 0;
	unsigned lbl = min(USB_BLOCK_SIZE - 1, n);
	usb_set_server_write_size(n, block);
	offset += lbl;
	n -= lbl;
	while(n > 0) {
		lbl = min(USB_BLOCK_SIZE, n);
		ret = usb_write_block(block, offset, lbl);
		if (ret < 0) break;
		offset += lbl;
		n -= lbl;
	}
	STROBE_HIGH;
	LDPRINT("l Write_block: STROBE is HIGH\n");
	SET_INPUT;
	return ret; // doesn't really work, usb_server_write_block does not return anything
#  else
	if (!cable_type)
		return laplink_server_write_block(block, n);
#  endif
#endif

	LDPRINT("l Waiting for LOW\n");
	WAIT_LOW;

#ifdef IODEBUG
	GET_BYTE(x);
	if (x != 0xff) {
		LDPRINT("! WARNING! Other side is not receiving!");
		return -1;
	}
#endif

	SET_OUTPUT;

	while(TRUE) {
		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 1st byte = $%x, STROBE is LOW, waiting for HIGH\n",x);
		STROBE_LOW;
		WAIT_HIGH;

		if (i>=n)
			break;

		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH, waiting for LOW\n",x);
		STROBE_HIGH;
		WAIT_LOW;
	}
	STROBE_HIGH;
	LDPRINT("l Write_block: STROBE is HIGH\n");
	SET_INPUT;
	return 0;
}

/* ---------------------------------------------------------------------- */
#if defined(IBM) && !defined(USB)

long laplink_client_read_block(BYTE *block, long n)
{
	BYTE x;
	long i = 0;

	while(TRUE) {
		LDPRINT("l STROBE is LOW, waiting for LOW\n");
		STROBE_LOW; WAIT_LOW;
		x = GET_LOW_NIBBLE;
		LDPRINT1("l low nibble = $%x, STROBE is HIGH, waiting for HIGH\n",x);
		STROBE_HIGH; WAIT_HIGH;
		if (i>=n)
			break;
		x |= GET_HIGH_NIBBLE;
		block[i++]=x;
		LDPRINT1("l byte = $%x\n",x);
	}
	return 0;
}

long laplink_server_read_block(BYTE *block, long n)
{
	BYTE x;
	long i = 0;

	while(TRUE) {
		LDPRINT("l Waiting for LOW\n");
		WAIT_LOW;

		x = GET_LOW_NIBBLE;
		LDPRINT1("l low nibble = $%x, STROBE is LOW\n",x);
		STROBE_LOW;
		if (i>=n)
			break;
		LDPRINT("l Waiting for HIGH\n");
		WAIT_HIGH;
		x |= GET_HIGH_NIBBLE;
		block[i++]=x;
		LDPRINT1("l byte = $%x, STROBE is HIGH\n",x);
		STROBE_HIGH;
	}
	LDPRINT("l STROBE is LOW, waiting for HIGH\n");
	WAIT_HIGH;
	LDPRINT("l STROBE is HIGH\n");
	STROBE_HIGH;
	return 0;
}

long laplink_client_write_block(const BYTE *block, long n)
{
	BYTE x;
	long i = 0;

	while(TRUE) {
		PUT_BYTE((x=block[i])&0x0f);
		LDPRINT1("l low nibble = $%x, STROBE is LOW, waiting for LOW\n",x);
		WAIT_LOW;

		if (i>=n)
			break;

		PUT_BYTE(0x10 | x>>4);
		i++;
		LDPRINT1("l byte = $%x, STROBE is HIGH, waiting for HIGH\n",x);
		WAIT_HIGH;
	}
	LDPRINT("l STROBE is HIGH, waiting for HIGH\n");
	STROBE_HIGH;
	WAIT_HIGH;
	return 0;
}

long laplink_server_write_block(const BYTE *block, long n)
{
	BYTE x;
	long i = 0;

	LDPRINT("l Waiting for LOW\n");
	WAIT_LOW;

	while(TRUE) {
		PUT_BYTE((x=block[i]) & 0x0f);
		LDPRINT1("l low nibble = $%x, STROBE is LOW, waiting for HIGH\n",x);
		WAIT_HIGH;

		if (i>=n)
			break;

		PUT_BYTE(0x10 | x>>4);
		i++;
		LDPRINT1("l byte = $%x, STROBE is HIGH, waiting for LOW\n",x);
		WAIT_LOW;
	}
	STROBE_HIGH;
	LDPRINT("l Write_block: STROBE is HIGH\n");
	return 0;
}

#endif
