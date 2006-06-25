extern int time_out;

#if LOWDEBUG
#undef PEKNE_POCKEJ	1
#endif

#define WAIT_LOW	{int i=TIMER+time_out; while(IS_READY) if (TIMER>i) {SET_INPUT;STROBE_HIGH;return -1;};}
#define WAIT_HIGH	{int i=TIMER+time_out; while(! IS_READY) if (TIMER>i) {SET_INPUT;STROBE_HIGH;return -1;};}

long client_read_block(BYTE *block, long n)
{
	BYTE x;
	long i = 0;

#if IBM
	if (!cable_type)
		return laplink_client_read_block(block, n);
#endif

	SET_INPUT;

	while(TRUE) {
		LDPRINT("l STROBE is LOW, waiting for LOW\n");
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
		STROBE_LOW; WAIT_LOW;
		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 1st byte = $%x, STROBE is HIGH, waiting for HIGH\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
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

#if IBM
	if (!cable_type)
		return laplink_server_read_block(block, n);
#endif

	SET_INPUT;

	while(TRUE) {
		LDPRINT("l Waiting for LOW\n");
		WAIT_LOW;

		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 1st byte = $%x, STROBE is LOW\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
		STROBE_LOW;

		if (i>=n)
			break;

		LDPRINT("l Waiting for HIGH\n");
		WAIT_HIGH;
		GET_BYTE(x);
		block[i++]=x;
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
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

#if IBM
	if (!cable_type)
		return laplink_client_write_block(block, n);
#endif

#ifdef IODEBUG
	GET_BYTE(x);
	if (x != 0xff)
		LDPRINT("! WARNING!!! Other side is not receiving!");
	else
		LDPRINT("_ Other side's input has been set OK.\n");
#endif

	SET_OUTPUT;

	while(TRUE) {
		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 1st byte = $%x, STROBE is LOW, waiting for LOW\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
		STROBE_LOW;
		WAIT_LOW;

		if (i>=n)
			break;

		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH, waiting for HIGH\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
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

#if IBM
	if (!cable_type)
		return laplink_server_write_block(block, n);
#endif

	LDPRINT("l Waiting for LOW\n");
	WAIT_LOW;

#ifdef IODEBUG
	GET_BYTE(x);
	if (x != 0xff)
		LDPRINT("! WARNING!!! Other side is not receiving!");
	else
		LDPRINT("Other side's input has been set OK.\n");
#endif

	SET_OUTPUT;

	while(TRUE) {
		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 1st byte = $%x, STROBE is LOW, waiting for HIGH\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
		STROBE_LOW;
		WAIT_HIGH;

		if (i>=n)
			break;

		PUT_BYTE(x=block[i++]);
		LDPRINT1("l 2nd byte = $%x, STROBE is HIGH, waiting for LOW\n",x);
#if PEKNE_POCKEJ
	sleep(PEKNE_POCKEJ);
#endif
		STROBE_HIGH;
		WAIT_LOW;
	}
	STROBE_HIGH;
	LDPRINT("l Write_block: STROBE is HIGH\n");
	SET_INPUT;
	return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef IBM

#define A_REMEMBER											\
		"incw	%%dx\n\t"			/* dx = +1 */			\
		"andb	$0x24,%%bl\n\t"								\
		"movb	%%bl,%%bh\n\t"		/* bh = STROBE HIGH */	\
		"incb	%%bl\n\t"			/* bl = STROBE LOW */	\
		"movl	$0x46c,%%edi\n\t"	/* 0x46c = TIMER */

#define A_GET_BYTE											\
		"decw	%%dx\n\t"			/* dx = 0 */			\
		"inb	%%dx,%%al\n\t"								\
		"incw	%%dx\n\t"			/* dx = +1 */			\
		"movb	%%al,(%%esi)\n\t"							\
		"incl	%%esi\n\t"			/* *(esi++) = inportb() */

#define A_PUT_BYTE											\
		"decw	%%dx\n\t"			/* dx = 0 */			\
		"movb	(%%esi),%%al\n\t"							\
		"outb	%%al,%%dx\n\t"								\
		"incw	%%dx\n\t"			/* dx = +1 */			\
		"incl	%%esi\n\t"			/* outportb(*(esi++)) */

#define	A_STROBE_LOW										\
		"incw	%%dx\n\t"			/* dx = +2 */			\
		"movb	%%bl,%%al\n\t"								\
		"outb	%%al,%%dx\n\t"								\
		"decw	%%dx\n\t"			/* dx = +1 */

#define A_STROBE_HIGH										\
		"incw	%%dx\n\t"			/* dx = +2 */			\
		"movb	%%bh,%%al\n\t"								\
		"outb	%%al,%%dx\n\t"								\
		"decw	%%dx\n\t"			/* dx = +1 */

#define	A_WAIT_LOW											\
		"movw	__go32_info_block+26,%%fs\n\t"				\
		"movl	_time_out,%%ebp\n\t"						\
		"leal	(%%ebp,%%ebp,8),%%ebp\n\t"					\
		"leal	(%%ebp,%%ebp),%%ebp\n\t"	/* ebp = 18 x timeout */ \
		".byte 0x64\n\t"									\
		"addl	(%%edi),%%ebp\n\t"							\
		"0:\n\t"											\
		"inb	%%dx,%%al\n\t"								\
		"testb	%%al,%%al\n\t"								\
		"jl		2f\n\t"										\
		".byte 0x64\n\t"									\
		"cmpl	(%%edi),%%ebp\n\t"							\
		"jnbe	0b\n\t"										\
		"jmp	8f\n\t"										\
		"2:\n\t"

#define A_WAIT_HIGH											\
		"movw	__go32_info_block+26,%%fs\n\t"				\
		"movl	_time_out,%%ebp\n\t"						\
		"leal	(%%ebp,%%ebp,8),%%ebp\n\t"					\
		"leal	(%%ebp,%%ebp),%%ebp\n\t"					\
		".byte 0x64\n\t"									\
		"addl	(%%edi),%%ebp\n\t"							\
		"1:\n\t"											\
		"inb	%%dx,%%al\n\t"								\
		"testb	%%al,%%al\n\t"								\
		"jge	3f\n\t"										\
		".byte 0x64\n\t"									\
		"cmpl	(%%edi),%%ebp\n\t"							\
		"jnbe	1b\n\t"										\
		"jmp	8f\n\t"										\
		"3:\n\t"

#define	A_END												\
		"movl	$0,%%eax\n\t"								\
		"jmp	9f\n\t"										\
		"8:\n\t"											\
		"movl	$-1,%%eax\n\t"								\
		"9:\n\t"

#define A_PARAMETERS										\
		: "=a" (status)										\
		: "b" (gcontrol), "c" (n), "d" (print_port), "S" (block)			\
		: "%edi", "%ebp"

long fast_client_read_block(BYTE *block, long n)
{
	long status = 0;

	SET_INPUT;

#ifdef __MSDOS__
	if (port_type && cable_type) {
__asm__ __volatile__("shrl	%%ecx\n\t"
			"addl	$1,%%ecx\n\t"		/* ecx =  number of words */
			A_REMEMBER
			"jmp	ClientReadJump\n\t"

		"ClientReadLoop:\n\t"
			A_GET_BYTE
		"ClientReadJump:\n\t"
			A_STROBE_LOW
			A_WAIT_LOW
			A_GET_BYTE
			A_STROBE_HIGH
			A_WAIT_HIGH
			"decl	%%ecx\n\t"
			"jne	ClientReadLoop\n\t"
			A_END
			A_PARAMETERS );
		FIX_GCONTROL_STROBE_HIGH;	/* because gcontrol was not updated inside of the asm block */
	}
	else
#endif	/* __MSDOS__ */
		status = client_read_block(block, n);

	return status;
}

long fast_server_read_block(BYTE *block, long n)
{
	long status = 0;

	SET_INPUT;

#ifdef __MSDOS__
	if (port_type && cable_type) {
__asm__ __volatile__("shrl	%%ecx\n\t"
			"addl	$1,%%ecx\n\t"		/* ecx = number of words */
			A_REMEMBER
			"jmp	ServerReadJump\n\t"

		"ServerReadLoop:\n\t"
			A_GET_BYTE
			A_STROBE_HIGH
		"ServerReadJump:\n\t"
			A_WAIT_LOW
			A_GET_BYTE
			A_STROBE_LOW
			A_WAIT_HIGH
			"decl	%%ecx\n\t"
			"jne	ServerReadLoop\n\t"
			A_STROBE_HIGH
			A_END
			A_PARAMETERS );
		FIX_GCONTROL_STROBE_HIGH;	/* because gcontrol was not updated inside of the asm block */
	}
	else
#endif	/* __MSDOS __ */
		status = server_read_block(block, n);

	return status;
}

long fast_client_write_block(const BYTE *block, long n)
{
	long status = 0;

	SET_OUTPUT;

#ifdef __MSDOS__
	if (port_type && cable_type) {
__asm__ __volatile__("shrl	%%ecx\n\t"
			"addl	$1,%%ecx\n\t"		/* ecx = number of words */
			A_REMEMBER
			"jmp	ClientWriteJump\n\t"

		"ClientWriteLoop:\n\t"
			A_PUT_BYTE
			A_STROBE_HIGH
			A_WAIT_HIGH
		"ClientWriteJump:\n\t"
			A_PUT_BYTE
			A_STROBE_LOW
			A_WAIT_LOW
			"decl	%%ecx\n\t"
			"jne	ClientWriteLoop\n\t"
			A_STROBE_HIGH
			A_WAIT_HIGH
			A_END
			A_PARAMETERS );
		FIX_GCONTROL_STROBE_HIGH;	/* because gcontrol was not updated inside of the asm block */
	}
	else
#endif	/* __MSDOS __ */
		status = client_write_block(block, n);

	SET_INPUT;

	return status;
}

long fast_server_write_block(const BYTE *block, long n)
{
	long status = 0;

	WAIT_LOW;

	SET_OUTPUT;

#ifdef __MSDOS__
	if (port_type && cable_type) {
__asm__ __volatile__("shrl	%%ecx\n\t"
			"addl	$1,%%ecx\n\t"		/* ecx = number of words */
			A_REMEMBER
			"jmp	ServerWriteJump\n\t"

		"ServerWriteLoop:\n\t"
			A_PUT_BYTE
			A_STROBE_HIGH
			A_WAIT_LOW
		"ServerWriteJump:\n\t"
			A_PUT_BYTE
			A_STROBE_LOW
			A_WAIT_HIGH
			"decl	%%ecx\n\t"
			"jne	ServerWriteLoop\n\t"
			A_STROBE_HIGH
			A_END
			A_PARAMETERS );
		FIX_GCONTROL_STROBE_HIGH;	/* because gcontrol was not updated inside of the asm block */
	}
	else
#endif	/* __MSDOS __ */
		status = server_write_block(block, n);

	SET_INPUT;

	return status;
}

/* ---------------------------------------------------------------------- */

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
