/*
 * PARallel CoPy - written for transferring large files between any two machines
 * with parallel ports.
 *
 * Petr Stehlik (c) 1996,1997
 *
 * You need a special HW interface if your PC offers unidirectional parallel
 * port only.
 */

/*******************************************************************************/

#ifndef _PARCPLOW_H
#define _PARCPLOW_H

#ifdef ATARI

#include <osbind.h>
#include <mintbind.h>
#include <support.h>

#define	clrscr()	printf("\033E")

#define STROBE_HIGH	Ongibit(0x20)
#define STROBE_LOW	Offgibit(0xcf)

MYBOOL _is_ready(void)
{
	BYTE	*GPIP=(BYTE *)0xfffffa01L;
	return (*GPIP & 1);
}
#define IS_READY	Supexec(_is_ready)

#define GET_BYTE(x)	{x = Giaccess(0,15);}
#define PUT_BYTE(x)	(void)Giaccess(x,15|0x80)

#define SET_INPUT	(void)Giaccess(Giaccess(0,7)&~(1 << 7),7|0x80)
#define SET_OUTPUT	(void)Giaccess(Giaccess(0,7)| (1 << 7),7|0x80)

#define KEYPRESSED	(Kbshift(-1) & 0x0f)
#define STOP_KEYSTROKE	"Shift+Control+Alternate"

#define ARCHIVE_BIT		FA_CHANGED
#endif /* ATARI */

/******************************************************************************/

#ifdef IBM
#  ifndef _WIN32
#    include <mntent.h>
#  endif

// general things
#ifdef __MSDOS__
extern __Go32_Info_Block _go32_info_block;

int __opendir_flags = __OPENDIR_PRESERVE_CASE;	/* preserve upper/lower filenames */

void clrscr(void);
int getch(void);

#define KEYPRESSED	(_farpeekb(_dos_ds, 0x417) & 0x0f)
#define STOP_KEYSTROKE	"Shift+Ctrl+Alt"
#define ARCHIVE_BIT		_A_ARCH
#define _PATH_MOUNTED	""

#elif defined _WIN32

#define clrscr()		system("cls")
#define STOP_KEYSTROKE	"Ctrl-C"
#define ARCHIVE_BIT		_A_ARCH	/* ?? */

#else

#define __LINUX__
#define clrscr()
#define STOP_KEYSTROKE	"Ctrl-C"
#define ARCHIVE_BIT		0x0020	/* needs to be implemented with mtime/atime */
#endif

// communication
#ifdef USB

#include "parcp-usb.h"
#define STROBE_HIGH	set_strobe(1)
#define STROBE_LOW	set_strobe(0);
#define IS_READY	get_busy()
#define GET_BYTE(x)
#define PUT_BYTE(x)
#define SET_OUTPUT	set_mode(1);
#define SET_INPUT	set_mode(0);

#else /* !USB */

#define CFG_PORT	"Port"
#define CFG_UNIBI	"UNI-BI"

MYBOOL PCunidirect = FALSE;
int print_port = 0x378;
int port_type = 1;	/* 0 = SPP, 1 = bidir/EPP, 2 = ECP2EPP */
int cable_type = 1;	/* 0 = LapLink, 1 = bidirectional */
int old_ecp_val;	/* the value of the ECP register saved during the PARCP run */

#define ECP2EPP									\
{												\
	iopl(3);									\
	old_ecp_val = inportb(print_port + 0x402);	\
	outportb(print_port + 0x402,0x80);			\
	iopl(0);									\
}

#define EPP2ECP									\
{												\
	iopl(3);									\
	outportb(print_port+0x402,old_ecp_val);		\
	iopl(0);									\
}

#ifdef __MSDOS__
#include <inlines/pc.h>
#include <sys/farptr.h>
#include <go32.h>
#include <dos.h>
#include <fcntl.h>
#define iopl(a)		/* pro DOS je to nic */
#else	/* ! __MSDOS__ */
#include <sys/io.h>
#include <sys/perm.h>
void outportb(a,b)	{outb(b,a);}
BYTE inportb(a)		{return inb(a);}
#endif	/* !__MSDOS__ alias Linux */

int gcontrol = (1 << 2);	/* Ucc */

#define	SET_GCONTROL(x)		{ gcontrol |= (1 << x); }
#define	CLR_GCONTROL(x)		{ gcontrol &=~(1 << x); }
#define SET_CTRL(x)						\
{										\
	SET_GCONTROL(x);					\
	outportb(print_port+2, gcontrol);	\
}
#define CLR_CTRL(x)						\
{										\
	CLR_GCONTROL(x);					\
	outportb(print_port+2, gcontrol);	\
}
#define STATUS	inportb(print_port+1)

/* POST PC: STROBE, AUTOLF, INIT are +5V, while SLCTIN is 0V) */
/* STROBE is 0. bit and is negated - log.1 = 0V, log.0 = +5V */
/* AUTOLF is 1. bit and is negated - log.1 = 0V, log.0 = +5V */
/* SLCTIN is 3. bit and is negated - log.1 = 0V, log.0 = +5V */
/* INIT is 2. bit, log.1 = +5V, which gives current to the intelligent UNI-BI interface */

#define STROBE_HIGH					\
{									\
	if (cable_type) {				\
		CLR_CTRL(0);				\
	}								\
	else {							\
		outportb(print_port, 0x10);	\
	}								\
}

#define FIX_GCONTROL_STROBE_HIGH	\
{									\
	if (cable_type)	{				\
		CLR_GCONTROL(0);			\
	}								\
}

#define STROBE_LOW					\
{									\
	if (cable_type)	{				\
		SET_CTRL(0);				\
	}								\
	else {							\
		outportb(print_port, 0);	\
	}								\
}


#define IS_READY	!(STATUS & 0x80)

#define CLOCK_LOW	SET_CTRL(3)
#define CLOCK_HIGH	CLR_CTRL(3)
#define CLOCK_RAISE	{CLOCK_LOW; CLOCK_HIGH;}	/* impuls for the latch to copy the data on the output */
#define GET_LOW_NIBBLE	((STATUS >> 3) & 0x0f)
#define GET_HIGH_NIBBLE	((STATUS << 1) & 0xf0)

#define GET_BYTE(x)					\
{									\
	if (PCunidirect) {				\
		CLOCK_LOW;					\
		x=GET_LOW_NIBBLE;			\
		CLOCK_HIGH;					\
		x|=GET_HIGH_NIBBLE;			\
	}								\
	else {							\
		x=inportb(print_port);		\
	}								\
}

#define PUT_BYTE(x)					\
{									\
	outportb(print_port, x);		\
	if (PCunidirect) {				\
		CLOCK_RAISE; 				\
	}								\
}

#define SET_OUTPUT					\
{									\
	if (cable_type)					\
	{								\
		if (PCunidirect) {			\
			SET_CTRL(1);			\
		}							\
		else {						\
			CLR_CTRL(5);			\
		}							\
	}								\
}

#define SET_INPUT					\
{									\
	if (cable_type)					\
	{								\
		if (PCunidirect) {			\
			CLR_CTRL(1);			\
		}							\
		else {						\
			SET_CTRL(5);			\
		}							\
	}								\
}

long laplink_client_read_block(BYTE *block, long n);
long laplink_server_read_block(BYTE *block, long n);
long laplink_client_write_block(const BYTE *block, long n);
long laplink_server_write_block(const BYTE *block, long n);

#endif /* USB */

#endif	/* IBM */
/*******************************************************************************/
#ifdef STANDALONE

#undef SET_INPUT
#undef SET_OUTPUT
#undef STROBE_HIGH
#undef STROBE_LOW
#undef IS_READY
#undef GET_BYTE
#undef PUT_BYTE
#undef ECP2EPP
#undef EPP2ECP

#define SET_INPUT	{}
#define SET_OUTPUT	{}
#define STROBE_HIGH	{}
#define STROBE_LOW	{}
#define IS_READY	FALSE
#define GET_BYTE(x)	0
#define PUT_BYTE(x)	{}
#define ECP2EPP
#define EPP2ECP

#endif

#endif	/* _PARCPLOW_H */
