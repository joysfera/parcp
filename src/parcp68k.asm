YAMAHA	= $ffff8800		* reg. A0
*       = $ffff8802		* reg. A1
GPIP	= $fffffa01		* reg. A2
CLOCK	= $4ba			* reg. A3
*       = buffer		* reg. A4

* reg. D0  - temporary register
* reg. D1  - keeps original contents of SR
* reg. D2  - counter of remaining length to transmit
* reg. D3  - number 15, for quicker writting to Yamaha regs.
* reg. D4  - Timeout, for quicker adding

BDIRECT	= 7
BSTROBE	= 5

		xref		_time_out

RYCHLY	= 1				* rychly dotaz na zacatku WAIT_xxx

RUPT	SET			1	* na zacatku jsou preruseni povolena

CLI		MACRO
		IFNE		RUPT
RUPT	SET			0
		move.w		sr,d1
		or.w		#$700,sr
		ENDC
		ENDM

STI		MACRO
		IFEQ		RUPT
RUPT	SET			1
		move.w		d1,sr
		ENDC
		ENDM

SET_CTRL	MACRO
		CLI
		move.b		\1,(a0)
		move.b		#(1<<\2),d0
		or.b		(a0),d0
		move.b		d0,(a1)
		STI
		ENDM

CLR_CTRL	MACRO
		CLI
		move.b		\1,(a0)
		move.b		#$ff-(1<<\2),d0
		and.b		(a0),d0
		move.b		d0,(a1)
		STI
		ENDM

SET_INPUT	MACRO
		CLR_CTRL	#7,BDIRECT
		ENDM

SET_OUTPUT	MACRO
		SET_CTRL	#7,BDIRECT
		ENDM

STROBE_HIGH	MACRO
		SET_CTRL	#14,BSTROBE
		ENDM

STROBE_LOW	MACRO
		CLR_CTRL	#14,BSTROBE
		ENDM

IS_READY	MACRO
		btst		#0,(a2)
		ENDM

GET_BYTE	MACRO
		CLI
		move.b		d3,(a0)
		move.b		(a0),(a4)+
*		STI							necht STI da az nasledujici STROBE_xxx
		ENDM

PUT_BYTE	MACRO
		CLI
		move.b		d3,(a0)
		move.b		(a4)+,(a1)
*		STI							necht STI da az nasledujici STROBE_xxxx
		ENDM

WAIT_LOW:	MACRO
		IFNE		RYCHLY
		IS_READY					* pro urychleni dotaz hned na pocatku
		beq.s		.end\@
		ENDC
		move.l		(a3),d0
		add.l		d4,d0
.loop\@	IS_READY
		beq.s		.end\@
		cmp.l		(a3),d0
		bne.s		.loop\@
		bra		timeout
.end\@
		ENDM

WAIT_HIGH:	MACRO
		IFNE		RYCHLY
		IS_READY					* pro urychleni dotaz hned na pocatku
		bne.s		.end\@
		ENDC
		move.l		(a3),d0
		add.l		d4,d0
.loop\@	IS_READY
		bne.s		.end\@
		cmp.l		(a3),d0
		bne.s		.loop\@
		bra		timeout
.end\@
		ENDM

GODMODE		MACRO
		clr.l		-(sp)
		move		#32,-(sp)
		trap		#1
		addq		#6,sp
		move.l		d0,zasobnik
		ENDM

USERMODE	MACRO
		move.l		zasobnik,-(sp)
		move		#32,-(sp)
		trap		#1
		addq		#6,sp
		ENDM

SAVE_REGS	MACRO
		movem.l		a2-a4/d2-d4,-(sp)
		endm

RESTORE_REGS	MACRO
		movem.l		(sp)+,a2-a4/d2-d4
		ENDM

prebrat_parametry	MACRO
		move.l		28(sp),a4		/* odkud */
		move.l		32(sp),d2		/* kolik slov */
		asr.l		#1,d2			/* tolik bajtu */
		ENDM

naloudovat_registry		MACRO
		lea			YAMAHA,a0
		lea			2(a0),a1
		lea			GPIP,a2
		lea			CLOCK,a3
		moveq		#15,d3
		move.l		_time_out,d4
		mulu		#200,d4
		ENDM

		xdef 		_fast_client_read_block
		xdef		_fast_client_write_block
		xdef 		_fast_server_read_block
		xdef		_fast_server_write_block
*********************************************************************
_fast_client_read_block:
		SAVE_REGS
		prebrat_parametry
		GODMODE
		naloudovat_registry
		SET_INPUT
		bra.s		.cyklus

.cl_re	GET_BYTE
.cyklus	STROBE_LOW
		WAIT_LOW
		GET_BYTE
		STROBE_HIGH
		WAIT_HIGH
		subq.l		#1,d2
		bge.s		.cl_re

.konec	USERMODE
		RESTORE_REGS
		moveq		#0,d0
		rts
*********************************************************************
_fast_server_read_block:
		SAVE_REGS
		prebrat_parametry
		GODMODE
		naloudovat_registry
		SET_INPUT
		bra.s		.cyklus

.se_re	WAIT_HIGH
		GET_BYTE
		STROBE_HIGH
.cyklus WAIT_LOW
		GET_BYTE
		STROBE_LOW
		subq.l		#1,d2
		bge.s		.se_re

.konec	WAIT_HIGH
		STROBE_HIGH
		USERMODE
		RESTORE_REGS
		moveq		#0,d0
		rts
*********************************************************************
_fast_client_write_block:
		SAVE_REGS
		prebrat_parametry
		GODMODE
		naloudovat_registry
		SET_OUTPUT
		bra.s		.cyklus

.cl_wr	PUT_BYTE
		STROBE_HIGH
		WAIT_HIGH
.cyklus PUT_BYTE
		STROBE_LOW
		WAIT_LOW
		subq.l		#1,d2
 		bge.s		.cl_wr

.konec	STROBE_HIGH
		WAIT_HIGH
		SET_INPUT
		USERMODE
		RESTORE_REGS
		moveq		#0,d0
		rts
*********************************************************************
_fast_server_write_block:
		SAVE_REGS
		prebrat_parametry
		GODMODE
		naloudovat_registry
		WAIT_LOW
		SET_OUTPUT
		bra.s		.cyklus

.se_wr	PUT_BYTE
		STROBE_HIGH
		WAIT_LOW
.cyklus PUT_BYTE
		STROBE_LOW
		WAIT_HIGH
		subq.l		#1,d2
		bge.s		.se_wr

.konec	STROBE_HIGH
		SET_INPUT
		USERMODE
		RESTORE_REGS
		moveq		#0,d0
		rts
*********************************************************************
timeout:
		SET_INPUT
		STROBE_HIGH
		USERMODE
		RESTORE_REGS
		moveq		#-1,d0
		rts

		BSS
zasobnik:
		ds.l		1
