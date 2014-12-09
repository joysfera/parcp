/*
 * PARallel CoPy - written for transferring large files between any two machines
 *                 with parallel ports.
 *
 * Petr Stehlik (c) 1996-2014
 *
 */

#include "parcp.h"			/* contants, function headers */
#include "parcplow.h"		/* various low level things defined there */
#include "global.h"			/* list of global variables */
#include "parstruc.h"		/* config struct */
#include "match.h"

#ifdef SHELL
#include <curses.h>
#include <panel.h>
extern MYBOOL curses_initialized;
extern int progress_width;
extern WINDOW *pwincent;
#endif

MYBOOL client = TRUE;	/* PARCP is Client by default */
MYBOOL remote_status_of_mkdir; /* HACK */

BYTE *block_buffer;
char *dir_buffer, *file_buffer,  string_buffer[MAXSTRING+1];
long buffer_len = KILO * BUFFER_LENKB;		/* the size of the transferred block */
#ifdef __MSDOS__
char original_path[MAXSTRING];
#endif

MYBOOL INT_flag = FALSE;				/* registers the press of CTRL-C */
MYBOOL _quiet_mode = FALSE;			/* if set to TRUE PARCP doesn't display anything but ERRORs */

unsigned long g_files = 0, g_folders = 0, g_start_time = 0;
unsigned long g_files_pos, g_time_pos;
ULONG64 g_bytes, g_bytes_pos;	/* 64-bit wide counters */

/* features */
unsigned long g_exch_features = 0;

short page_length = 25;
short page_width  = 80;

int g_last_status = 0;		/* CLI will record errors into this var */

MYBOOL bInBatchMode = FALSE;

/******************************************************************************/

char *show_size64(char *buf, ULONG64 size)
{
	char *format;
	unsigned long x = 0;
	if (size < (10*KILO)) {
		format = "%lu bytes";
		x = size;
	}
	else if (size < (10*KILO*KILO)) {
		format = "%lu kB";
		x = size / KILO;
	}
	else if (size < (10*KILO*KILO*KILO)) {
		format = "%lu MB";
		x = size / (KILO * KILO);
	}
	else {
		format = "%lu GB";
		x = size / (KILO * KILO * KILO);
	}
	sprintf(buf, format, x);
	return buf;
}

/******************************************************************************/
void wait_for_client(void)	/* waits and returns spare CPU cycles to other processes */
{
	while(IS_READY) {
		usleep(WAIT4CLIENT);
		if (stop_waiting())
			errexit("Waiting for client interrupted by user.", ERROR_USERSTOP);
	}
}

void wait_before_read(void)		/* waits before read_xxx() operation to not time out on that */
{
	SET_INPUT;
	DPRINT("wait_before_read()\n");
	if (client) {
		/* we must put the STROBE low to allow server to send us data */
		STROBE_LOW;
	}
	while(IS_READY) {
		if (stop_waiting())
			errexit("Waiting for other side interrupted by user.", ERROR_USERSTOP);
	}
	DPRINT("STROBE and BUSY are both LOW now\n");
}

#ifndef PARCP_SERVER
#include "parftpcl.c"		/* command line (FTP-like) user interface */
#endif

#ifndef STANDALONE

#include "parcplow.c"		/* set of routines in C that are coded in assembler as well */

void read_block(BYTE *block, long n)
{
	long status = client ? client_read_block(block, n) : server_read_block(block, n);
	if (status < 0)
		errexit("Timeout. Please increase Timeout value in PARCP config file.", ERROR_TIMEOUT);
}

void write_block(const BYTE *block, long n)
{
	long status = client ? client_write_block(block, n) : server_write_block(block, n);
	if (status < 0)
		errexit("Timeout. Please increase Timeout value in PARCP config file.", ERROR_TIMEOUT);
}

#else
void read_block(BYTE *block, long n) { }
void write_block(const BYTE *block, long n) { }
#endif	/* STANDALONE */

/******************************************************************************/
UWORD read_word(void)
{
	BYTE a[2+1];
	UWORD x;

	read_block(a,2);
	x = (a[0]<<8) | a[1];

	DPRINT2("l Read_word() -> %u=$%04x\n", x, x);

	return x;
}

long read_long(void)
{
	BYTE a[4+1];
	long x;

	read_block(a,4);
	x = a[0];
	x = (x<<8) | a[1];
	x = (x<<8) | a[2];
	x = (x<<8) | a[3];

	DPRINT2("l Read_long() -> %ld=$%08lx\n", x, x);

	return x;
}

long long read_longlong(void)
{
	BYTE a[8+1];
	long long x;

	read_block(a,8);
	x = a[0];
	x = (x<<8) | a[1];
	x = (x<<8) | a[2];
	x = (x<<8) | a[3];
	x = (x<<8) | a[4];
	x = (x<<8) | a[5];
	x = (x<<8) | a[6];
	x = (x<<8) | a[7];

	DPRINT2("l Read_long_long() -> %lld=$%llx\n", x, x);

	return x;
}

void write_word(UWORD x)
{
	BYTE a[2+1];
	a[0] = x>>8;
	a[1] = x;

	DPRINT2("l Write_word(%u=$%04x)\n", x, x);

	write_block(a,2);
}

void write_long(long x)
{
	BYTE a[4+1];

	a[0] = x>>24;
	a[1] = x>>16;
	a[2] = x>>8;
	a[3] = x;

	DPRINT2("l Write_long(%ld=$%08lx)\n", x, x);

	write_block(a,4);
}

void write_longlong(long long x)
{
	BYTE a[8+1];

	a[0] = x>>56;
	a[1] = x>>48;
	a[2] = x>>40;
	a[3] = x>>32;
	a[4] = x>>24;
	a[5] = x>>16;
	a[6] = x>>8;
	a[7] = x;

	DPRINT2("l Write_long_long(%lld=$%llx)\n", x, x);

	write_block(a,8);
}

/*******************************************************************************/

void client_reconnecting(void)
{
	int val;
	write_word(HI_CLIENT);
	val = read_word();
	if (val != PROTOKOL) {
		printf("\nServer's Debug info: expected $%04x, received $%04x\n", PROTOKOL, val);
		write_word(M_QUIT);
		errexit("Client uses different revision of PARCP communication protocol.", ERROR_HANDSHAKE);
	}
	write_word(M_OK);
}

void catch_ctrl_c(int x)
{
	if (! INT_flag)
		INT_flag = TRUE;		/* remember the key press */
	signal(SIGINT, catch_ctrl_c);
}

/* return TRUE if user holds EmergencyQuit keys */
/* EmergencyQuit keys are either Alt+Shift+Control or Control-C */
MYBOOL stop_waiting(void)
{
	MYBOOL was_break = INT_flag;

#ifdef KEYPRESSED
	if (KEYPRESSED >= 13)		/* Alt+Shift+Control */
		was_break = TRUE;
#endif

	INT_flag = FALSE;			/* in any case clear the CTRL-C flag */

	return was_break;
}

#ifndef PARCP_SERVER
/* return TRUE if user holds Stop-File-Transfer keys on Client */
/* Stop-File-Transfer keys are either Shif+Control or Control-C */
MYBOOL break_file_transfer(void)
{
#ifndef PARCP_SERVER
	MYBOOL was_break = INT_flag;

#ifdef KEYPRESSED
	if (KEYPRESSED == 6 || KEYPRESSED == 7)	/* Shift+Control */
		was_break = TRUE;
#endif

	INT_flag = FALSE;		/* clear flag CTRL-C */

	return was_break;
#else
	return FALSE;			/* on Server you can't stop file transfer */
#endif /* PARCP_SERVER */
}

/* return TRUE if user pressed Esc key on Client */
MYBOOL zastavit_prenos_souboru(void)
{
	if (client) {
		int keys = 0;

#ifdef SHELL
		if (shell && curses_initialized)
			keys = wgetch(pwincent);
#endif
/*
		else
			keys = fetch_key();
*/

		if (keys == 27)		/* ESC key */
			return TRUE;

#ifdef ATARI
#ifdef KEYPRESSED
		if (KEYPRESSED == 3)	/* both shifts */
			return TRUE;
#endif
#endif
	}
	return FALSE;
}
#endif	/* !PARCP_SERVER */

/*******************************************************************************/
void port_reset(void)
{
	SET_INPUT;
	STROBE_HIGH;
#ifdef IBM
#ifdef USB
	usb_exit();
#else
	if (port_type == 2 && !PCunidirect)
		EPP2ECP;
#endif
#endif
}
/*******************************************************************************/

void errexit(const char *a, int error_code)
{
	DPRINT1("! Errexit: %s\n", a);
	port_reset();
#ifdef SHELL
	if (shell && curses_initialized)
		endwin();
#endif
	puts("");
	if (a != NULL)
		puts(a);
	else
		puts("An unknown error occured.");

#ifdef __MSDOS__
	chdir(original_path);
#endif

	puts("Press Enter to exit.");
	getchar();

	exit(error_code);
}
/*******************************************************************************/

#if defined(DEBUG) || defined(LOWDEBUG)

void debug_print(const char *format, ... )
{
	va_list args;
	char buffer[MAXSTRING]="";

	va_start(args,format);
	vsprintf(buffer,format,args);

	if (format != NULL && debuglevel > 0) {
		char c = format[0];

		if (debuglevel&1 && strchr(nolog_str, c) == NULL) {
			FILE *stream;

			if ((stream = fopen(logfile, "at")) != NULL) {
				fputs(buffer,stream);
				fclose(stream);
			}
		}

		if (debuglevel&2 && strchr(nodisp_str, c) == NULL)
			printf("%s", buffer);
	}
	va_end(args);
}
#endif /* DEBUG */

/*******************************************************************************/
static MYBOOL copyinfo_sending;
static long copyinfo_size_in_blocks;
static long copyinfo_pos;
static clock_t copyinfo_time;

void open_copyinfo(MYBOOL sending, const char *name, long size)
{
	char title_txt[10];
	strcpy(title_txt, sending?"Sending":"Receiving");

	copyinfo_sending = sending;
	copyinfo_size_in_blocks = (size + buffer_len-1) / buffer_len;
	copyinfo_pos = 0;	/* largest copied size */
	copyinfo_time = TIMER;

#ifdef SHELL
	if (shell && curses_initialized) {
		int name_delta = strlen(name) - progress_width;

		wmove(pwincent, 1, 1);whline(pwincent, ' ', progress_width);

		if (name_delta > 0) {
			mvwaddstr(pwincent, 1, 1, "...");
			mvwaddstr(pwincent, 1, 4, name + 3 + name_delta);
		}
		else
			mvwaddstr(pwincent, 1, 1, name);

		wmove(pwincent, 2, 1);whline(pwincent, ' ', progress_width);
		update_panels();
		doupdate();
	}
	else
#endif
	{
		if (_quiet_mode)
			return;

		if (size > 0) {
			if (hash_mark)
				printf("%s %s (%ld bytes = %ld blocks)", title_txt, name, size, copyinfo_size_in_blocks);
			else
				printf("%s %s    ", title_txt, name);
		}
		else {
			printf("%s empty file %s\n", title_txt, name);
		}
	}
}

void open_sendinfo(const char *name, long size)
{
	open_copyinfo(TRUE, name, size);
}

void open_recvinfo(const char *name, long size)
{
	open_copyinfo(FALSE, name, size);
}

void update_copyinfo(unsigned long x)
{
	unsigned long pos_block = (x + buffer_len-1) / buffer_len;

	copyinfo_pos = x;	/* largest copied size */

#ifdef SHELL
	if (shell && curses_initialized) {
		unsigned int p = (pos_block * progress_width) / copyinfo_size_in_blocks;

		/* update single file progress indicator */
		wmove(pwincent, 2, 1);whline(pwincent, ACS_BLOCK, p);

		if (_check_info && g_bytes) {
			p = ((g_bytes_pos + x) * progress_width) / g_bytes;

			/* update total size progress indicator */
			wmove(pwincent, 4, 1);whline(pwincent, ACS_BLOCK, p);

			{
				static long last_display_time = -1;
				char buf[MAXSTRING];
				ULONG64 current_pos = g_bytes_pos + x;
				unsigned long current_time = TIMER;
				unsigned long elapsed_time = current_time - g_start_time;
				unsigned long avg_speed = current_pos / (elapsed_time+1);	/* +1 to not divide by zero */
				ULONG64 remaining_length = g_bytes - current_pos;
				unsigned long remaining_time = remaining_length / avg_speed;
				int rem_days = remaining_time / (3600 * 24);
				int rem_hours = remaining_time / 3600;
				int rem_mins = (remaining_time % 3600) / 60;
				int rem_secs = (remaining_time % 60);

				if (last_display_time < g_start_time)
					last_display_time = g_start_time + REMAINING_TIME_UPDATE_RATE;
				if ((current_time - last_display_time) > REMAINING_TIME_UPDATE_RATE) {
					last_display_time = current_time;
					if (rem_days)
						sprintf(buf, "%3d days", rem_days);
					else if (rem_hours)
						sprintf(buf, "%2d:%02d:%02d", rem_hours, rem_mins, rem_secs);
					else if (rem_mins)
						sprintf(buf, "%2d:%02d   ", rem_mins, rem_secs);
					else
						sprintf(buf, "%2d secs ", rem_secs);
					mvwaddstr(pwincent, 3, progress_width-8, buf);
				}
			}
		}
		update_panels();
		doupdate();
	}
	else
#endif
	{
		if (_quiet_mode)
			return;

		if (hash_mark)
			putchar(HASH_CHAR);
		else
			printf("\b\b\b%02d%%", (int)((pos_block*100)/copyinfo_size_in_blocks));
	}
}

void close_copyinfo(int ret_flag)
{
	unsigned long speed;
	copyinfo_time = TIMER-copyinfo_time;		/* find out the transfer time */
	if (copyinfo_time == 0)
		copyinfo_time = 1;

	if (ret_flag == FILE_SKIPPED)
		g_bytes -= copyinfo_pos;
	else
		g_bytes_pos += copyinfo_pos;

	if (_quiet_mode)
		return;

	if (copyinfo_pos == 0)
		return;
#ifdef SHELL
	if (shell && curses_initialized)
		return;
#endif

	if (hash_mark) {
		char title_txt[MAXSTRING], buf_bytes[MAXSTRING];
		show_size64(buf_bytes, copyinfo_pos);
		sprintf(title_txt, "\n%s %s", copyinfo_sending?"Sent":"Received", buf_bytes);
		printf("%s", title_txt);
	}
	else
		printf("\b\b\b\b");

	speed = (copyinfo_pos / copyinfo_time);
	if (_check_info && g_bytes) {
		unsigned int position = (g_bytes_pos * 100ULL) / g_bytes;
		char bytes_buf[MAXSTRING];
		show_size64(bytes_buf, g_bytes);
		printf(" OK (%lu cps) (%02u%% of total size %s)\n", speed, position, bytes_buf);
	}
	else
		printf(" OK (%lu cps)\n", speed);
}

/*******************************************************************************/

#include "crc32.c"

int send_file(FILE *handle, long lfile)
{
	long remaining_length, lblock;
	UWORD receiver_status, ret_flag = 0;
	int repeat_transfer;

	DPRINT1("s Sending file %ld bytes long\n", lfile);

	remaining_length = lfile;
	do {
		long block_len_check;
#ifndef PARCP_SERVER
		if (break_file_transfer()) {
			lblock = -1;		/* internal indicator */
			ret_flag = INTERRUPT_TRANSFER;
		}
		else
#endif
		{
			lblock = fread(block_buffer, 1, buffer_len, handle);
			if (lblock == 0)
				ret_flag = ERROR_READING_FILE;
		}

		write_long(lblock);		/* sending length of block. Special values 0 and -1 */
		block_len_check = read_long();
		if (block_len_check != lblock) {	/* read it back and compare it */
			DPRINT2("send_file(): block length check failed: %08lx != %08lx\n", lblock, block_len_check);
			errexit("A communication error in send_file() - sorry", ERROR_BADDATA);
		}

		if (ret_flag)
			break;		/* quit send_file() due to either error reading or interrupt transfer */

		repeat_transfer = REPEAT_TRANSFER;
		do {
			write_block(block_buffer, lblock);		/* send the entire block of data */

			if (_checksum)
				write_long(compute_CRC32(block_buffer, lblock));	/* send the CRC */

			/* here we must wait for other side that writes the block to (possibly) slow
			   device such as network drive or even floppy */
			wait_before_read();
			receiver_status = read_word();
		} while(receiver_status == M_REPEAT && repeat_transfer--);

		if (receiver_status == M_REPEAT)
			ret_flag = ERROR_CRC_FAILED;
		else if (receiver_status == M_INT)
			ret_flag = INTERRUPT_TRANSFER;
		else if (receiver_status == M_FULL)
			ret_flag = ERROR_WRITING_FILE;

		if (ret_flag)
			break;	/* STOP sending file */

		remaining_length -= lblock;
		update_copyinfo(lfile - remaining_length);

	} while(remaining_length);

	DPRINT1("s send_file() returning %d\n", ret_flag);

	/* ret_flag got one of the following values:
		0 = OK
		INTERRUPT_TRANSFER
		ERROR_READING_FILE
		ERROR_WRITING_FILE
		ERROR_CRC_FAILED
	*/
	return ret_flag;
}

int receive_file(FILE *handle, long lfile)
{
	long lblock, zal_lfile = lfile;
	int repeat_transfer;
	int ret_flag = 0;

	do {
		/* here we must wait for other side that reads the block from (possibly) slow
		   device such as network drive or even floppy */
		lblock = read_long();
		write_long(lblock);		/* send it back - to check if we got it right */

		if (lblock == 0) {			/* error reading the input file on remote side */
			ret_flag = ERROR_READING_FILE;
			break;			
		}
		else if (lblock == -1) {	/* transmitter wants to stop transfer */
			ret_flag = INTERRUPT_TRANSFER;
			break;			
		}

		repeat_transfer = REPEAT_TRANSFER;
		do {
			unsigned long crc;
			read_block(block_buffer, lblock);			/* read the entire block of data */

			if (!_checksum)
				break;

			wait_before_read();	// give the other side time to compute the block checksum
			crc = read_long();
			if (crc == compute_CRC32(block_buffer, lblock))
				break;	/* CRC is OK */

			DPRINT("l CRC fail, repeating block transfer\n");
			write_word(M_REPEAT);			/* repeat the block transfer */
		} while(repeat_transfer--);

		if (_checksum && repeat_transfer < 0) {
			ret_flag = ERROR_CRC_FAILED;
			break;
		}

		if (fwrite(block_buffer, 1, lblock, handle) < lblock) {	/* write the data block to file */
			write_word(M_FULL);		/* send the error code "disk full" */
			ret_flag = ERROR_WRITING_FILE;
			break;					/* file transfer ends here due to full disk */
		}

#ifndef PARCP_SERVER
		if (break_file_transfer()) {
			write_word(M_INT);		/* send the error code "transfer interrupted" */
			ret_flag = INTERRUPT_TRANSFER;
			break;					/* file transfer ends here due to user interruption */
		}
#endif

		write_word(M_OK);		/* send confirmation of received block */

		lfile -= lblock;
		update_copyinfo(zal_lfile-lfile);

	} while(lfile);

	/* ret_flag got one of the following values:
		0 = OK
		INTERRUPT_TRANSFER
		ERROR_READING_FILE
		ERROR_WRITING_FILE
		ERROR_CRC_FAILED
	*/
	return ret_flag;
}

/*******************************************************************************/

void send_string(const char *a)
{
	UWORD len, ret_len;

	if (a == NULL)
		errexit("A programming error in send_string() - sorry", ERROR_BUGPRG);

	len = strlen(a)+1;	/* +1 for EOS */
	write_word(len);
	ret_len = read_word();
	if (ret_len == len)
		write_block((const BYTE *)a, len);
	else {
		char *errstr = malloc(90 + len);
		sprintf(errstr, "Communication error in send_string(\"%s\"): write_word($%04x), read_word($%04x)\n", a, len, ret_len);
		errexit(errstr, ERROR_BADDATA);
		free(errstr);
	}
}

void receive_string(char *a)
{
	UWORD len = read_word();
	write_word(len);			/* send it back for elementar safety */
	read_block((BYTE *)a, len);
}

void allocate_buffers(void)
{
	/* dirbuf_lines + 2 more lines: ".." and "total length is ... bytes" */
	long dirbuf_len = (dirbuf_lines+2) * DIRLINELEN;	/* size of buffer for directory list */

	DPRINT1("> Allocating block buffer %ld bytes\n", buffer_len);
	/* buffer_len for block transfer + 2 bytes as a round up */
	if ((block_buffer = realloc(block_buffer, buffer_len+2)) == NULL)
		errexit("Fatal problem with reallocating memory. Aborting..", ERROR_MEMORY);

	DPRINT1("> Allocating DIR buffer %ld\n", dirbuf_len);
	if ((dir_buffer = realloc(dir_buffer, dirbuf_len)) == NULL)
		errexit("Fatal problem with reallocating memory. Aborting..", ERROR_MEMORY);

	if (filebuffers) {
		DPRINT1("> Allocating FILE buffer %ld\n", filebuffers*buffer_len);
		if ((file_buffer = realloc(file_buffer, filebuffers*buffer_len)) == NULL) {
			errexit("Fatal error with allocating file buffer!", ERROR_MEMORY);
		}
	}
	else {
		if (file_buffer)
			free(file_buffer);
		file_buffer = NULL;
	}

}

#ifdef ATARI
#include "ceh.c"
#endif

void setup_opendir(void)
{
#ifdef __MSDOS__
	/* preserve file name case in the dir list */
	if (_preserve_case)
		__opendir_flags |= __OPENDIR_PRESERVE_CASE;
	else
		__opendir_flags &=~__OPENDIR_PRESERVE_CASE;
	/* show hidden files */
	if (_show_hidden)
		__opendir_flags |= __OPENDIR_FIND_HIDDEN;
	else
		__opendir_flags &=~__OPENDIR_FIND_HIDDEN;
#endif
}

#ifndef PARCP_SERVER
UWORD PackParameters(void)
{
	return													\
	(_case_sensitive ? B_CASE : 0)							\
|	(_send_subdir ? B_SUBDIR : 0)							\
|	(_keep_timestamp ? B_TIMESTAMP : 0)						\
|	(_checksum ? B_CHECKSUM : 0)							\
|	(_show_hidden ? B_HIDDEN : 0)							\
|	(_keep_attribs ? B_ATTRIBS : 0)							\
|	(_archive_mode ? B_ARCH_MODE : 0)						\
|	(_preserve_case ? B_PRESERVE : 0)						\
|	(_check_info ? B_CHECKINFO : 0)							\
|	B_MKDIR_STATUS	/* HACK, client annonces mkdir status protocol extension */	\
	;
}
#endif /* !PARCP_SERVER */

void UnpackParameters(UWORD x)
{
	_case_sensitive = (x & B_CASE) ? TRUE : FALSE;
	_send_subdir = (x & B_SUBDIR) ? TRUE : FALSE;
	_keep_timestamp = (x & B_TIMESTAMP) ? TRUE : FALSE;
	_checksum = (x & B_CHECKSUM) ? TRUE : FALSE;
	_show_hidden = (x & B_HIDDEN) ? TRUE : FALSE;
	_keep_attribs = (x & B_ATTRIBS) ? TRUE : FALSE;
	_archive_mode = (x & B_ARCH_MODE) ? TRUE : FALSE;
	_preserve_case = (x & B_PRESERVE) ? TRUE : FALSE;
	_check_info = (x & B_CHECKINFO) ? TRUE : FALSE;
	remote_status_of_mkdir = (x & B_MKDIR_STATUS) ? TRUE : FALSE;	/* HACK, server receives client's capability */
}

#ifndef PARCP_SERVER
void send_parameters(void)
{
	UWORD par = PackParameters();
	UWORD over= _over_older | (_over_newer << 8);

	write_word(M_PARS);
/* send the parameters block */
	write_word(par);
	write_long(buffer_len);
	write_word(dirbuf_lines);
	write_word(over);

/* set up the application with the new parameters */
	setup_opendir();
	allocate_buffers();
}
#endif /* !PARCP_SERVER */

void receive_parameters(void)
{
	UWORD par, over;

/* receive the parameters block */
	par = read_word();
	buffer_len = read_long();
	dirbuf_lines = read_word();
	over = read_word();

	UnpackParameters(par);
	_over_older = over & 0xff;
	_over_newer = (over >> 8) & 0xff;

/* set up the application with the new parameters */
	setup_opendir();
	allocate_buffers();

/* print information */
	printf("Client has configured server as follows:\n\
Block size: %ld kB/s\n\
Directory lines: %d\n\
CRC: %s\n\
Extended protocol: %s\n\n", buffer_len / 1024, dirbuf_lines, _checksum ? "Yes" : "No", remote_status_of_mkdir ? "Yes" : "No");
}

/*******************************************************************************/
#ifdef _WIN32
typedef enum { FILESYS_CASE_SENSITIVE, FILESYS_LFN_SUPPORTED } gvi;
MYBOOL get_volume_info(const char *fname, gvi x)
{
	char volumeName[256];
	DWORD MaximumComponentLength, FileSystemFlags;
	GetVolumeInformation(fname, volumeName, sizeof(volumeName), NULL, &MaximumComponentLength, &FileSystemFlags, NULL, 0);
	switch(x) {
		case FILESYS_CASE_SENSITIVE: return ((FileSystemFlags & FILE_CASE_SENSITIVE_SEARCH) > 0);
		case FILESYS_LFN_SUPPORTED: return (MaximumComponentLength > (8+3+1));
	}
	return FALSE;
}
#endif

MYBOOL fs_sensitive(const char *fname)
{
#if defined(ATARI)
	return (pathconf(fname, _PC_NAME_CASE) == 0) ? TRUE : FALSE;
#elif defined(__MSDOS__)
	return (_get_volume_info(fname, NULL, NULL, NULL) & _FILESYS_CASE_SENSITIVE) ? TRUE : FALSE;
#elif defined(_WIN32)
	return get_volume_info(fname, FILESYS_CASE_SENSITIVE);
#else
	return TRUE;	/* Linux */
#endif
}

MYBOOL lfn_fs(const char *fname)
{
#if defined(ATARI) || defined(__LINUX__)
	return (pathconf(fname, _PC_NAME_MAX) > (8+3+1)) ? TRUE : FALSE;
#elif defined(__MSDOS__)
	return (_get_volume_info(fname, NULL, NULL, NULL) & _FILESYS_LFN_SUPPORTED) ? TRUE : FALSE;
#elif defined(_WIN32)
	return get_volume_info(fname, FILESYS_LFN_SUPPORTED);
#else
	abort; /* return TRUE; */	/* currently unused */
#endif
}
/*******************************************************************************/

char *get_cwd(char *path, int maxlen)
{
	char *ret = getcwd(path, maxlen);
	ret = ret; // UNUSED
#ifdef ATARI
	if (! strncmp(path,TOS_DEV,Ltos_dev)) {
		DPRINT1("> Get_cwd() = '%s' but will be converted\n", path);
		memmove(path, path+Ltos_dev-1, strlen(path)-Ltos_dev+2);	/* remove /dev/ */
		if (path[2] == SLASH) {		/* it is /x/ */
			path[0] = path[1];
			path[1] = DVOJTEKA;	/* convert to x:/ */
		}
	}
#endif
	DPRINT1("> Get_cwd() = '%s'\n", path);

	return path;
}

MYBOOL change_dir(const char *p, char *q)
{
	if (p && *p) {
		int status;
		DPRINT1("> Change_dir: '%s'\n", p);

		if ((status = chdir(p)) != 0) {
			if (q)
				strcpy(q, "BAD DIRECTORY");
			DPRINT1("> Error in chdir(): %d\n", status);
			return FALSE;
		}
	}
	if (q)
		get_cwd(q, MAXPATH);

	return TRUE;
}

/*******************************************************************************/
#if _WIN32
static int statfs (const char *path, struct statfs *buf)
{
	HINSTANCE h;
	FARPROC f;
	int retval = 0;
	char tmp [MAX_PATH], resolved_path [MAX_PATH];
	realpath(path, resolved_path);

	/* check whether GetDiskFreeSpaceExA is supported */
	h = LoadLibraryA ("kernel32.dll");
	f = h ? GetProcAddress (h, "GetDiskFreeSpaceExA") : NULL;
	if (f) {
		ULARGE_INTEGER bytes_free, bytes_total, bytes_free2;
            if (!f (resolved_path, &bytes_free2, &bytes_total, &bytes_free)) {
                errno = ENOENT;
                retval = - 1;
              }
            else {
                buf -> f_bsize = FAKED_BLOCK_SIZE;
                buf -> f_bfree = (bytes_free.QuadPart) / FAKED_BLOCK_SIZE;
                buf -> f_files = buf -> f_blocks = (bytes_total.QuadPart) / FAKED_BLOCK_SIZE;
                buf -> f_ffree = buf -> f_bavail = (bytes_free2.QuadPart) / FAKED_BLOCK_SIZE;
              }
          }
	else {
            DWORD sectors_per_cluster, bytes_per_sector;
            if (h) FreeLibrary (h);
            if (!GetDiskFreeSpaceA (resolved_path, &sectors_per_cluster, &bytes_per_sector, &buf -> f_bavail, &buf -> f_blocks)) {
                errno = ENOENT;
                retval = -1;
              }
            else {
                buf -> f_bsize = sectors_per_cluster * bytes_per_sector;
                buf -> f_files = buf -> f_blocks;
                buf -> f_ffree = buf -> f_bavail;
                buf -> f_bfree = buf -> f_bavail;
              }
          }
	if (h) FreeLibrary (h);

	/* get the FS volume information */
	if (strspn (":", resolved_path) > 0) resolved_path [3] = '\0'; /* we want only the root */
	if (GetVolumeInformation (resolved_path, NULL, 0, &buf -> f_fsid, &buf -> f_namelen, NULL, tmp, MAX_PATH)) {
		buf -> f_type = (strcasecmp ("NTFS", tmp) == 0) ? NTFS_SUPER_MAGIC : MSDOS_SUPER_MAGIC;
	}
	else {
		errno = ENOENT;
		retval = - 1;
	}
	return retval;
}

int uname(struct utsname *uts)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	char *arch = "unknown";
	switch(sysInfo.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
		case PROCESSOR_ARCHITECTURE_AMD64: arch = "x86_64"; break;
		case PROCESSOR_ARCHITECTURE_IA64: arch = "ia64"; break;
		case PROCESSOR_ARCHITECTURE_ARM: arch = "arm"; break;
	}
	strcpy(uts->machine, arch);
	strcpy(uts->sysname, "Windows");

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	char *win_ver = "";
	switch(osvi.dwMajorVersion*10 + osvi.dwMinorVersion) {
		case 52: win_ver = "XP"; break;
		case 60: win_ver = "Vista"; break;
		case 61: win_ver = "7"; break;
		case 62: win_ver = "8+"; break;
	}
	strcpy(uts->release, win_ver);
	return 0;
}

int settimeofday(const struct timeval * tp, const struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	static const ULONG64 EPOCH = ((ULONG64) 116444736000000000ULL);
	ULONG64 time = EPOCH + tp->tv_sec * 10000000L + tp->tv_usec;
	FILETIME file_time;
	SYSTEMTIME  system_time;

	file_time.dwLowDateTime = time;
	file_time.dwHighDateTime = time >> 32;
	FileTimeToSystemTime(&file_time, &system_time);

	SetSystemTime( &system_time );
	return 0;
}
#endif

void list_dir(const char *p2, int maska, char *zacatek)
{
	int pocet = 0;
	long total = 0;
	DIR *dir_p;
	struct dirent *dir_ent;
	struct stat stb;
	struct statfs stfs;
	time_t dneska = time(NULL);
	struct tm *cas;
	char dname[MAXPATH],tempo[MAXPATH],fname[MAXFNAME+1],pname[MAXFNAME+1],*p;
	MYBOOL compare_sensitive = _case_sensitive & fs_sensitive(p2);

	DPRINT1("d Entering list_dir(%s)\n", p2);

	prepare_fname_for_opendir(p2, dname, pname);

	p = zacatek;
	*zacatek = 0;	/* clear old contents */

	DPRINT2("d Opening directory: '%s', using '%s' as file-mask\n", dname, pname);

	if ( (dir_p = opendir(dname)) != NULL) {
		char *ret;
		while( (dir_ent = readdir(dir_p)) != NULL) {
			MYBOOL matched;
			MYBOOL is_dir;

			strncpy(fname, dir_ent->d_name, MAXFNAME);
			fname[MAXFNAME-1] = 0;

			if (!strcmp(fname,".") || !strcmp(fname,"..") )
				continue;				/* don't display neither "." nor ".." */

			if (! _show_hidden)
				if (fname[0] == '.')	/* file name beginning with dot means it's hidden */
					continue;

			matched = match(pname, fname, compare_sensitive);
			if (maska & LS_NEGATE_MASK)
				matched = !matched;
			if (! matched)
				continue;

			DPRINT1("d Filename '%s' matches filemask\n", fname);

			strcpy(tempo, dname);
			strcat(tempo, fname);		/* find the absolute file name */
			if (stat(tempo, &stb)) {
				DPRINT2("d stat(%s) returned error %d\n", tempo, errno);
				continue;	// if the stat() fails then the file basically does not exist so we need to skip it (this definitely hides those VFAT ghost entries with ugly file names)
			}
#if ATARI
			if (! _show_hidden && (stb.st_attr & 0x02)) // file is hidden in 8.3 DOS/TOS
				continue;
			if (stb.st_attr & 0x08) // entry is VolumeID
				continue;			// this should also remove VFAT ghost entries that have all VolumeID set
#endif
			is_dir = S_ISDIR(stb.st_mode);
			if (is_dir && (maska & LS_FILES_ONLY))
				continue;
			if (!is_dir && (maska & LS_DIRS_ONLY))
				continue;

/*
  display: 32 of name chars, 10 chars for file length, space char, 16 chars for date in format 1997/07/02 22:18. Filename aligned to left, the file length aligned to right, date in format YY/MM/DD HH:MM including leading zeros.
  That means [0,31] = file name | [32,41] = file length | [43,58] = file timestamp | [59] = '\n'  for folders display "<DIR>" instead of file length.
*/

			p += sprintf(p, PRINTF_TEMPLATE, fname);
			if (is_dir) {
				p += sprintf(p, "     <DIR>");
			}
			else {
				p += sprintf(p, "%10ld", (long)stb.st_size);
				total += stb.st_size;
			}

			cas = localtime(&stb.st_mtime);
			if (cas == NULL)
				cas = localtime(&dneska);

			p += sprintf(p, " %4d/%02d/%02d %02d:%02d\n",1900+cas->tm_year,1+cas->tm_mon,cas->tm_mday,cas->tm_hour,cas->tm_min);
			if (++pocet == dirbuf_lines)
				break;		/* DIRBUF array is full */
		}
		closedir(dir_p);

		if (pocet == dirbuf_lines) {
			sprintf(p, "Incomplete - increase DirectoryLines!!\n");
			return;
		}

		ret = getcwd(dname, sizeof(dname));	/* a workaround for a bug in MiNT libs */
		ret = ret; // UNUSED
		DPRINT1("d Directory closed, going to find out free space in %s\n", dname);
		statfs(dname, &stfs);			/* zjistit volne misto v aktualnim adresari */
		DPRINT2("d %ld free clusters of %ld bytes each.\n", stfs.f_bfree, stfs.f_bsize);
		{
			char buf_total[MAXSTRING], buf_free[MAXSTRING];
			show_size64(buf_total, total);
			show_size64(buf_free, (ULONG64)(stfs.f_bfree*stfs.f_bsize));
			sprintf(p, "%s in %4d files, %s free\n", buf_total, pocet, buf_free);
		}
	}
}

/*******************************************************************************/

int list_drives(char *p)
{
	char *dest = p;
	int i;
	time_t dneska = time(NULL);
	struct tm *cas;
	int drives = 0, max_drives = 32 < dirbuf_lines ? 32 : dirbuf_lines;

#if defined(ATARI) || defined(_WIN32)
	long drvmap = 0;
# ifdef ATARI
	long ssp = Super(0L);
	drvmap = *((long *)0x4c2);  /* get which drives are attached from drvmap TOS defined variable */
	if ((*(short *)0x4a6) != 2)
		drvmap &= ~2;             /* drive B: isn't really there */
	Super(ssp);
# else
	drvmap = GetLogicalDrives();
# endif
	for (i = 0; i < max_drives; i++) {
		if (drvmap & (1L << i)) {
			DIR *dir;
			char tmpline[]="a:/";
			tmpline[0] += i;

			/* check removable medias */
			if ((dir = opendir(tmpline)) == NULL)
				continue;	/* there's no media, skip it */
			closedir(dir);

			p += sprintf(p, PRINTF_TEMPLATE, tmpline);
			p += sprintf(p, "     <DIR>");
			cas = localtime(&dneska);
			p += sprintf(p, " %4d/%02d/%02d %02d:%02d\n",1900+cas->tm_year,1+cas->tm_mon,cas->tm_mday,cas->tm_hour,cas->tm_min);
			drives++;
		}
	}

#else	/* DJGPP and Linux got their own function for that */

	struct mntent *m;
	FILE *f;

	f = setmntent(_PATH_MOUNTED, "r");
	if (f != NULL) {
		for(i = 0; (m = getmntent(f)) != NULL && (i < max_drives); i++) {
			p += sprintf(p, PRINTF_TEMPLATE, m->mnt_dir);
			p += sprintf(p, "     <DIR>");
			cas = localtime(&dneska);
			p += sprintf(p, " %4d/%02d/%02d %02d:%02d\n",1900+cas->tm_year,1+cas->tm_mon,cas->tm_mday,cas->tm_hour,cas->tm_min);
			drives++;
		}
		endmntent(f);
	}
#endif

	if (p == dest) {	/* if there's nothing to return */
		p += sprintf(p, PRINTF_TEMPLATE, "/");	/* return the ROOT */
		p += sprintf(p, "     <DIR>");
		cas = localtime(&dneska);
		p += sprintf(p, " %4d/%02d/%02d %02d:%02d\n",1900+cas->tm_year,1+cas->tm_mon,cas->tm_mday,cas->tm_hour,cas->tm_min);
		drives=1;
	}

	return drives;
}

/*******************************************************************************/

unsigned get_fileattr(const char *fname)
{
	unsigned attrib = 0;
#ifdef ATARI
	char tosname[MAXPATH];

	unx2dos(fname, tosname);
	attrib = Fattrib(tosname, 0, 0);
#endif
#ifdef __MSDOS__
	_dos_getfileattr(fname, &attrib);
#endif

	return attrib & 0x002f;
}

int set_fileattr(const char *fname, unsigned attrib)
{
	int ret_flag = 0;
#ifdef ATARI
	char tosname[MAXPATH];

	unx2dos(fname, tosname);
	ret_flag = Fattrib(tosname, 1, attrib);
#endif
#ifdef __MSDOS__
	ret_flag = _dos_setfileattr(fname, attrib);
#endif

	return ret_flag;
}

/*******************************************************************************/

char *short_lfn(const char *lfn, char *dos_name)
{
	char name[9]="", ext[4]="";
	char *dot = strrchr(lfn, '.');
	if (dot == NULL || dot == lfn) {
		strncpy(name, lfn, 8);
		name[8] = '\0';
		if (strlen(lfn) > 8) {
			strncpy(ext, lfn+8, 3);		/* "description ==> "DESCRIPT.ION" */
			ext[3] = '\0';
		}
		else
			ext[0] = '\0';				/* no extension */
	}
	else {
		int name_len = dot - lfn;
		if (name_len > 8)
			name_len = 8;
		strncpy(name, lfn, name_len);
		name[name_len] = '\0';

		/* copy extension to ext[] */
		strncpy(ext, dot+1, 3);
		ext[3] = '\0';
	}

	/* convert SPACEs and dots to underscores */
	{
	char *ptr;
	for(ptr = name; *ptr; ptr++)
		if (*ptr == ' ' || *ptr == '.')
			*ptr = '_';
	for(ptr = ext; *ptr; ptr++)
		if (*ptr == ' ' || *ptr == '.')
			*ptr = '_';
	}

	/* construct DOS name */
	strcpy(dos_name, name);
	if (*ext) {
		strcat(dos_name, ".");
		strcat(dos_name, ext);
	}

	return dos_name;
}

void split_filename(const char *pathname, char *path, char *name)
{
	char pname[MAXPATH], *x;

	x = strcpy(pname, pathname);	/* work with a copy */

	DPRINT1("> split_filename(%s)\n", pname);

	/* convert from DOS like to Unix file file paths */
	if (isalpha(x[0]) && x[1]==DVOJTEKA && x[2]==BACKSLASH) {	/* X:\ */
		while((x=strchr(x,BACKSLASH)) != NULL)
			*x = SLASH;
	}

	/* split to file path and file name */
	if ((x = strrchr(pname, SLASH)) != NULL) {
		*x = 0;
		if (path)
			strcpy(path, pname);
		if (name)
			strcpy(name, ++x);
	}
	else {
		if (path)
			*path = 0;		/* path is empty */
		if (name) {
			strncpy(name, pname, MAXFNAME);
			name[MAXFNAME-1] = 0;
		}
	}

	if (path && *path)				/* if path is not empty, add the last slash for easier concatenating */
		strcat(path, SLASH_STR);

	if (path)
		DPRINT1("> directory '%s'\n", path);
	if (name)
		DPRINT1("> filename '%s'\n", name);
}

void prepare_fname_for_opendir(const char *pathname, char *path, char *name)
{
	split_filename(pathname, path, name);

	if (path && !*path)			/* if path is empty */
		strcpy(path, "./");		/* add there the current dir */

	if (name && !*name)			/* empty filename, for example during drag&drop of a folder */
		strcpy(name, "*");		/* in such case all files apply */
}

MYBOOL is_absolute_path(const char *path)
{
	int len;

	if (! (path && *path))
		return FALSE;

	if (path[0] == SLASH || path[0] == BACKSLASH)
		return TRUE;

	len = strlen(path);
	if (len >= 3)
		if (path[1] == DVOJTEKA && (path[2] == SLASH || path[2] == BACKSLASH))
			return TRUE;

	return FALSE;
}

MYBOOL has_last_slash(const char *path)
{
	char c;

	if (! (path && *path))
		return FALSE;

	c = path[strlen(path)-1];
	return (c == SLASH || c == BACKSLASH);
}

char *add_last_slash(char *path)
{
	if (path && !has_last_slash(path))
		strcat(path, SLASH_STR);

	return path;
}

char *remove_last_slash(char *path)
{
	if (has_last_slash(path))
		path[strlen(path)-1] = 0;

	return path;
}

void check_and_convert_filename(char *fname)
{
	char path[MAXPATH];
	char *copy, *ptr;
	MYBOOL last_slash;
	MYBOOL bTOSDRIVE = (fname && *fname && (fname[1] == DVOJTEKA));

	/* check current path and remember if the fs can handle LFN or not */
	if (is_absolute_path(fname)) {
		strcpy(path, fname);
	}
	else {
		char *ret = getcwd(path, sizeof(path));
		ret = ret; // UNUSED
	}

	if (lfn_fs(path))		/* lfn is supported so we can go away */
		return;

	DPRINT2("c converting(%s) because '%s' is on DOS fs\n", fname, path);

	/* parse path and separate single names */
	copy = strdup(bTOSDRIVE ? fname+2 : fname);		/* skip X: if present */
	ptr = strtok(copy, "/\\");
	last_slash = has_last_slash(fname);
	*fname = 0;

	while(ptr) {
		char dosname[13];
		/* convert every single name */
		short_lfn(ptr, dosname);
		strcat(fname, dosname);
		strcat(fname, SLASH_STR);
		ptr = strtok(NULL, "/\\");
	}
	if (!last_slash)
		remove_last_slash(fname);
	free(copy);
	DPRINT1("c conversion result: %s\n", fname);
}

/******************************************************************************/

#define HAS_LONGLONG		(g_exch_features & FEATURE_LONGLONG)
#define HAS_SENDFILESINFO	(g_exch_features & FEATURE_SENDFILESINFO)

void send_features()
{
	long client_features = ALL_FEATURES;
	write_word(M_EXCHANGE_FEATURES);
	write_long(client_features);
	g_exch_features = read_long();
}

void receive_features()
{
	long server_features = ALL_FEATURES;
	long client_features = read_long();
	g_exch_features = client_features & server_features;
	write_long(g_exch_features);
}

void write_long64(ULONG64 number)
{
	if (HAS_LONGLONG)
		write_longlong(number);
	else
		write_long(number);
}

ULONG64 read_long64()
{
	return (HAS_LONGLONG) ? read_longlong() : read_long();
}

#ifndef PARCP_SERVER
void send_collected_info(void)
{
	if (HAS_SENDFILESINFO) {
		write_word(M_SENDFILESINFO);
		write_long(g_files);
		write_long64(g_bytes);
		write_long(g_folders);
	}
}
#endif /* PARCP_SERVER */

void receive_collected_info(void)
{
	if (HAS_SENDFILESINFO) {
		g_files = read_long();
		g_bytes = read_long64();
		g_folders = read_long();
	}

	/* reset the counters */
	g_files_pos = g_bytes_pos = 0;
	g_time_pos = TIMER;
}

/*******************************************************************************/

int send_1_file(const char *name, struct stat *stb, unsigned long file_attrib)
{
	FILE *stream;
	UWORD report_val = M_OK, ret_flag = 0;
	unsigned long file_mode;
	long long size = stb->st_size;	/* make it 64bit enhanced */

	file_mode = file_attrib << 16;
	file_mode = stb->st_mode & 0x0000ffff;

	DPRINT1("l Trying to Send_1_file('%s')\n", name);

	if ((stream = fopen(name, "rb")) == NULL) {
		return ERROR_READING_FILE;		/* cannot open file */
	}

	/* file buffering enabled */
	if (file_buffer)
		setvbuf(stream, file_buffer, _IOFBF, filebuffers*buffer_len);

	DPRINT1("s Send_1_file('%s') starting\n", name);

	write_word(M_PUT);				/* sending file */
	send_string(name);				/* its name */
	write_long64(size);				/* its length */
	write_long(stb->st_mtime);		/* its timestamp */
	write_long(file_mode);			/* its mode */

	if (! client)
		wait_for_client();	/* give client time to determine whether to overwrite existing file */
	report_val = read_word();	/* M_OK, M_ERR, M_OSKIP, M_OASK or M_OQUIT */

#ifndef PARCP_SERVER
	if (client) {
		if (report_val == M_OASK) {
			switch (q_overwrite(name)) {
				case 2:
					write_word(M_OK);
					report_val = read_word();	/* M_OK or M_ERR */
					break;

				case 1:
					write_word(M_OSKIP);	/* skip the unfinished file */
					report_val = M_OSKIP;
					break;

				case 0:
					write_word(M_OQUIT);	/* skip the unfinished file and quit */
					report_val = M_OQUIT;
			}
		}
	}
#endif	/* PARCP_SERVER */

	switch(report_val) {
		case M_OK:
			open_sendinfo(name, size);
			if (size > 0) {	/* send just non-empty files */
				ret_flag = send_file(stream, size);
			}
			close_copyinfo(ret_flag);

			/* in ARCHIVE mode clear the Archive/Changed bit after successful copying */
			if (_archive_mode && !ret_flag)
				set_fileattr(name, file_attrib & ~ARCHIVE_BIT);
			break;

		case M_OSKIP:
			ret_flag = FILE_SKIPPED;
			break;
		case M_OQUIT:
			ret_flag = QUIT_TRANSFER;
			break;
		case M_ERR:
			ret_flag = ERROR_WRITING_FILE;
			break;
	}

	fclose(stream);

	DPRINT1("s Send_1_file() returning %d\n", ret_flag);

	/* ret_flag got one of the following values:
		0 = OK
		FILE_SKIPPED
		INTERRUPT_TRANSFER
		QUIT_TRANSFER
		ERROR_READING_FILE
		ERROR_WRITING_FILE
		ERROR_CRC_FAILED
	*/
	return ret_flag;
}

/*******************************************************************************/

int receive_1_file(void)
{
	DIR *dir_p;
	FILE *stream;
	struct stat stb;
	static char name[MAXPATH], *p;
	long long size;
	long timestamp;
	unsigned file_attr;
	unsigned long file_mode;
	struct utimbuf timbuf;
	int ret_flag = 0;

	DPRINT("r receive_1_file\n");
	receive_string(name);
	size = read_long64();
	timestamp = read_long();
	file_mode = read_long();
	file_attr = file_mode >> 16;

	timbuf.actime = timestamp;
	timbuf.modtime = timestamp;

/* here I must convert the LFN to DOS 8+3 if dest fs is TOS/DOS */
	check_and_convert_filename(name);

	if (stat(name, &stb) == 0) {
		/* file with same name exists already */
		MYBOOL novejsi = (stb.st_mtime > timestamp );
		int action;
		char over;

		if (timestamp == stb.st_mtime)		/* if both files have same timestamp then */
			novejsi = (stb.st_size >= size);/* check the file length */

		/* select the right switch */
		over = novejsi ? _over_newer : _over_older;

		if (over == 'S')
			action = 1;		/* SAY_ERROR("Skipping the file\n"); */
		else if (over == 'R')
			action = 2;	/* replace */
		else {
#ifndef PARCP_SERVER
			if (client)
				action = q_overwrite(name);
			else
#endif
			{	/* we're server - let's send message about possible file overwriting */
				write_word(M_OASK);
				wait_for_client();		/* wait for client until they decide what to do */
				switch(read_word()) {
					case M_OSKIP:
						return FILE_SKIPPED;
					case M_OQUIT:
						return QUIT_TRANSFER;
					default:
						action = 2;
				}
			}
		}

		switch(action) {
			case 1:
				write_word(M_OSKIP);	/* skip this file */
				return FILE_SKIPPED;
			case 0:
				write_word(M_OQUIT);	/* skip this file and quit */
				return QUIT_TRANSFER;
			default:
				break;			/* replace, so you can continue */
		}
	}

	p = name;
	while((p=strchr(p, SLASH))) {	/* file comes with file path - need to create parent folders */
		*p=0;
		if (! (dir_p = opendir(name)))	/* can you open this folder? */
			mkdir(name);				/* no, so create it */
		else
			closedir(dir_p);			/* it can be opened, so it exists - so let's close it :-) */
		*p++ = SLASH;
	}

	stream = fopen(name, "wb");
	if (stream == NULL) {
		write_word(M_ERR);				/* can't create file */
		return ERROR_WRITING_FILE;
	}

	/* real send ~~~~> real receive */

	/* zapinam bufrovani souboru */
	if (file_buffer)
		setvbuf(stream, file_buffer, _IOFBF, filebuffers*buffer_len);

	write_word(M_OK);
	open_recvinfo(name, size);
	if (size > 0) {	/*receive just non-empty files */
		ret_flag = receive_file(stream, size);
	}
	fclose(stream);
	close_copyinfo(ret_flag);

	/* update original timestamp */
	if (_keep_timestamp)
		utime(name, &timbuf);

	/* update file attributes */
	if (_keep_attribs)
	{
		chmod(name, file_mode & 0x0000ffff);		/* Unix-like, universal */
		set_fileattr(name, file_attr & 0x000f);		/* READONLY, HIDDEN, SYSTEM, VOLUME */
	}

	/* ret_flag got one of the following values:
		0 = OK
		FILE_SKIPPED
		INTERRUPT_TRANSFER
		QUIT_TRANSFER
		ERROR_READING_FILE
		ERROR_WRITING_FILE
		ERROR_CRC_FAILED
	*/
	return ret_flag;
}

/*******************************************************************************/

#define PROCESS_ARCHIVE	0x0100		/* flag for respecting _archive_mode */
#define PROCESS_INFO	0x0001
#define PROCESS_COPY	0x0002
#define PROCESS_DELETE	0x0004
#define PROCESS_MOVE	(PROCESS_COPY | PROCESS_DELETE)

MYBOOL PF_compare_sensitive;
int PF_Process;

/* forward reference */
int process_files_rec(const char *src_mask);

/*
 * will increase g_files, g_folders, g_bytes
 */
int process_files(int Process, const char *src_mask)
{
	char root[MAXPATH], fname[MAXFNAME], pwd_backup[MAXPATH], new_srcmask[MAXPATH];
	int status = 0;

	/* setup global vars for recursion */
	PF_compare_sensitive = _case_sensitive & fs_sensitive(src_mask);
	PF_Process = Process;

	/* separate absolute path name */
	prepare_fname_for_opendir(src_mask, root, fname);
	DPRINT2("s process_files: directory '%s', filemask '%s'\n", root, fname);

	get_cwd(pwd_backup, sizeof(pwd_backup));
	/* a quick work-around of chdir("./") MiNTlibs bug - Dsetpath(".\") doesn't work */
	if (! strcmp(root, "./"))
		strcpy(root, pwd_backup);

	if (change_dir(root, root)) {
		int ret;
		DPRINT1("s Directory changed to: '%s'\n", root);

		/* construct new source mask with relative path from current directory */
		strcpy(new_srcmask, "./");
		strcat(new_srcmask, fname);

		status = process_files_rec(new_srcmask);

		ret = chdir(pwd_backup);
		ret = ret; // UNUSED
	}
	else {
		DPRINT("s Error while changing directory\n");
		status = ERROR_READING_FILE;
	}

	return status;
}

/* the following function is called recursivelly so please minimize stack variables! */
int process_files_rec(const char *src_mask)
{
	DIR *dir_p;
	struct dirent *dir_ent;
	static struct stat stb;
	char *memblock, *directory, *path_name, *file_mask;
	char *p_dirname;
	int status = 0;

	DPRINT1("r Process_files_rec(%s)\n", src_mask);
	if ((memblock = malloc(2*MAXPATH+MAXFNAME+1)) == NULL)
		errexit("Fatal problem with allocating memory. Aborting..", ERROR_MEMORY);
	directory = memblock;
	path_name = memblock+MAXPATH;
	file_mask = memblock+2*MAXPATH;

	prepare_fname_for_opendir(src_mask, directory, file_mask);

	if ( (dir_p = opendir(directory)) == NULL ) {
		DPRINT1("r Can't open directory '%s'\n", directory);
		free(memblock);
		return ERROR_READING_FILE;	/* cannot open directory */
	}

/* remove useless "./" from file path */
	p_dirname = directory;
	if (p_dirname[0] == '.' && p_dirname[1] == SLASH) {
		p_dirname += 2;
#ifdef __LINUX__
/* special work-around of Linux libc bug ".//" */
		if (p_dirname[0] == SLASH)
			p_dirname++;
#endif
	}

	if ((PF_Process & PROCESS_COPY) && *p_dirname) {
		DPRINT1("r M_MD(%s)\n", p_dirname);
		write_word(M_MD);
		send_string(p_dirname);
		if (remote_status_of_mkdir) {
			int status;
			wait_before_read();	// wait for the other side to create the folder
			status = read_word();
			status = status; // UNUSED
		}
	}

	while( (dir_ent = readdir(dir_p)) != NULL) {
		char *name = dir_ent->d_name;

		if (!strcmp(name, ".") || !strcmp(name, "..") )
			continue;				/* ignore "." and ".." */
		if (! match(file_mask, name, PF_compare_sensitive) )
			continue;

		strcpy(path_name, p_dirname);
		strcat(path_name, name);			/* create absolute file path */
		if (stat(path_name, &stb)) {
			DPRINT2("r stat(%s) returned error %d\n", path_name, errno);
			continue;	/* go for next file */
		}

		if (S_ISDIR(stb.st_mode) && _send_subdir) {
			/* this is a directory - process it recursively */
			if (PF_Process & PROCESS_INFO)
				g_folders++;

			strcat(path_name, SLASH_STR);		/* add slash to path */
			status = process_files_rec(path_name);
			path_name[strlen(path_name)-1] = 0;		/* remove the slash */

			/* at last do something with the directory */
			if (!status && (PF_Process & PROCESS_DELETE)) {
				if (rmdir(path_name)) {
					DPRINT2("r rmdir(%s) -> %d\n", path_name, errno);
					status = ERROR_DELETING_FILE;
				}
			}
		}
		else {
			unsigned attrib = get_fileattr(path_name);

			/* in ARCHIVE mode process just files with Archive/Changed bit set */
			if ((PF_Process & PROCESS_ARCHIVE) && _archive_mode)
				if (!(attrib & ARCHIVE_BIT))
					continue;

			if (PF_Process & PROCESS_INFO)  {
				g_files++;
				g_bytes += stb.st_size;
			}
			else {
				for(;;) {
					UWORD reaction;
	
#ifndef PARCP_SERVER
					if (zastavit_prenos_souboru()) {
						status = QUIT_TRANSFER;	/* STOP sending files */
						break;
					}
#endif

					if (PF_Process & PROCESS_COPY)
						status = send_1_file(path_name, &stb, attrib);
					else
						status = 0;	/* to ensure Delete will work */
	
					if (!status && (PF_Process & PROCESS_DELETE)) {
						if (!client && !(PF_Process & PROCESS_COPY)) {
							write_word(M_DELINFO);	/* will be sending status, soon */
							send_string(path_name);
						}
						if (remove(path_name)) {
							DPRINT2("r remove(%s) -> %d\n", path_name, errno);
							status = ERROR_DELETING_FILE;
						}
					}
#ifndef PARCP_SERVER
					if (client) {
						if (status == 0)
							break;
						reaction = q_bugreport(status);
					}
					else
#endif
					{
						write_word(status);		/* Server sends status to Client */
						wait_for_client();
						reaction = read_word();	/* and waits for decision */
					}

					DPRINT1("r process_files_rec: %s ->", path_name);
					DPRINT2(" status = %x, reaction = %d\n", status, reaction);
	
					if (reaction == 2)
						continue;		/* continue for(;;) to REPEAT action */
	
					if (reaction == 0)
						status = QUIT_TRANSFER;

					break;	/* exit for(;;) cycle */
				}
			}
		}

		if (status == QUIT_TRANSFER)
			break;	/* stop sending files */
	}
	closedir(dir_p);
	DPRINT2("p process(%s) -> %d\n", directory, status);
	free(memblock);

	/* status got one of the following values:
		0 = OK
		QUIT_TRANSFER
	*/
	return status;
}

/*******************************************************************************/

int process_files_on_receiver_side()
{
	UWORD status = 0;

	DPRINT("p process_files_on_receiver_side:begin\n");
	for(;;) {
		char fname[MAXPATH];
		UWORD Process;

		wait_before_read();

		Process = read_word();
		DPRINT1("p pfors = %x\n", Process);

		if (Process == M_PEND)
			break;
		else if (Process == M_PUT) {
			receive_1_file();
		}
		else if (Process == ERROR_READING_FILE) {	// send_1_file couldn't open the file at all
		}
		else if (Process == M_MD) {
			int status;
			receive_string(fname);
			check_and_convert_filename(fname);
			remove_last_slash(fname);
			DPRINT1("p vytvarim adresar '%s' ... ", fname);
			status = mkdir(fname);
			DPRINT1("p status = %d\n", status);
			if (remote_status_of_mkdir)
				write_word(status);
			continue;
		}
		else if (Process == M_DELINFO) {
			receive_string(fname);
			/* show just deleted file */
		}
		else {
			char errstr[90];
			sprintf(errstr, "Error in pfors: received '%04x'\n", Process);
			errexit(errstr, ERROR_BADDATA);
		}

#ifndef PARCP_SERVER
		if (client) {
			int reaction;

			if (Process == ERROR_READING_FILE)
				status = Process;
			else {
				/* receive report from server's process_files */
				wait_before_read();		/* deleting can take a long time */
				status = read_word();
			}
			DPRINT1("p pfors:status = %x", status);

			if (zastavit_prenos_souboru())
				reaction = 0;	/* Quit */
			else {
				if (status)
					reaction = q_bugreport(status);
				else
					reaction = 1;	/* Ignore */
			}

			write_word(reaction);
			DPRINT1(" --> reaction = %x\n", reaction);

			if (reaction == 2)
				continue;	/* REPEAT action */
	
			if (reaction == 0) {
				status = QUIT_TRANSFER;
				break;
			}
		}
#endif
	}
	DPRINT1("p process_files_on_receiver_side:close (status = %x)\n", status);

	/* status got one of the following values:
		0 = OK
		QUIT_TRANSFER
	*/
	return status;
}

/*******************************************************************************/

int get_files_info(MYBOOL local, const char *src_mask, MYBOOL arch_mode)
{
	UWORD status;

	if (local) {
		int mode = PROCESS_INFO | (arch_mode ? PROCESS_ARCHIVE : 0);
		g_files = g_bytes = g_folders = 0;		/* this should not happen if this get_files_info() is called several times in a row from ParShell */
		status = process_files(mode, src_mask);	/* g_bytes/g_files/g_folders changed */
	}
	else {
		write_word(M_GETINFO);
		write_word(arch_mode);
		send_string(src_mask);

		/* here we must wait for the server collecting infos */
		wait_before_read();

		status = read_word();
		g_files = read_long();
		g_bytes = read_long64();
		g_folders = read_long();
	}

	g_files_pos = g_bytes_pos = 0;
	g_time_pos = TIMER;

	return status;
}

int delete_files(MYBOOL local, const char *del_mask)
{
	int status = 0;

	DPRINT("d delete_files: begin\n");
	if (local) {
		status = process_files(PROCESS_DELETE, del_mask);
		/* if we are server and were deleting our files here we must
		   send client the command to not wait for messages because
		   we already deleted all files.
		   Though if we got a "QUIT_TRANSFER" command from the client
		   we don't have to tell them to stop because they did stop
		   already */
		if (!client && status != QUIT_TRANSFER)
			write_word(M_PEND);		/* end of processing files */
	}
	else {
		write_word(M_DEL);
		send_string(del_mask);
		status = process_files_on_receiver_side();
	}
	DPRINT1("d delete_files: end -> %d\n", status);

	return status;
}

int copy_files(MYBOOL source, const char *p_srcmask, MYBOOL pote_smazat)
{
	int status = 0;

	DPRINT("c copy_files: begin\n");

	if (source) {
		int mode = (pote_smazat ? PROCESS_MOVE : PROCESS_COPY) | PROCESS_ARCHIVE;
		write_word(M_PSTART);
		status = process_files(mode, p_srcmask);

		/* if the file transfer was interrupted by a key press and we are server
		  we will not be sending a message to the client because it ended up
		  already and doesn't wait for anything */
		if (! (status == QUIT_TRANSFER && !client))
			write_word(M_PEND);		/* end of processing files */
	}
	else {
		write_word(pote_smazat ? M_GETDEL : M_GET);
		send_string(p_srcmask);
		if (read_word() == M_PSTART)
			status = process_files_on_receiver_side();
	}
	DPRINT1("d copy_files: end -> %d\n", status);

	return status;
}

/*******************************************************************************/

/* the following function is server-only */
int return_server_files_info(void)
{
	char filemask[MAXPATH];
	MYBOOL arch_mode;
	int status = 0;

	/* M_GETINFO was received by server dispatch routine already */
	arch_mode = read_word();
	receive_string(filemask);

	status = get_files_info(TRUE, filemask, arch_mode);

	write_word(status);
	write_long(g_files);
	write_long64(g_bytes);
	write_long(g_folders);

	return status;
}

/*******************************************************************************/

int gmt_offset()
{
#if _WIN32
	TIME_ZONE_INFORMATION tz;
	int tzi = GetTimeZoneInformation(&tz);
	int bias = tz.Bias;
	if (tzi > 0)
		bias += (tzi == 1 ? tz.StandardBias : tz.DaylightBias);
	return -bias * 60;
#else
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	return lt->tm_gmtoff;
#endif
}

int GetLocalTimeAndSendItOver(void)
{
	int gmtoff = gmt_offset();
	struct timeval tv;
	int status = gettimeofday(&tv, NULL);
	write_long(status == 0 ? tv.tv_sec + gmtoff : 0L);
	return status;
}

int ReceiveDataAndSetLocalTime(void)
{
	int gmtoff = gmt_offset();
	struct timeval tv;

	tv.tv_sec = read_long();
	if (tv.tv_sec == 0L)
		return -2;	/* cannot get time from remote computer */
	tv.tv_sec -= gmtoff;
	tv.tv_usec = 0;
	return settimeofday(&tv, NULL);
}

/*******************************************************************************/

int config_file(const char *soubor, MYBOOL vytvorit)
{
	BYTE crypta[MAXSTRING];
	int vysledek;

	if (vytvorit) {
		buffer_lenkb = buffer_len / KILO;
		vysledek = update_config(soubor,configs,CFGHEAD);
	}
	else {
		vysledek = input_config(soubor,configs,CFGHEAD);

#ifdef STANDALONE
		registered = TRUE;
#else
		parcpkey((const BYTE*)username, crypta);
		registered = check(crypta, (const BYTE*)keycode);
#endif

		if (dirbuf_lines < 1)
			dirbuf_lines = 1;

		if (time_out < 1)
			time_out = 1;

		buffer_len = buffer_lenkb * KILO;

#ifndef PARCP_SERVER
		if (client)
			normalize_over_switches();	/* normalize switches on client only because server will get from client the already normalized ones */
#endif

#ifdef SHELL
		if (! client)
			shell = FALSE;		/* don't try to open windows if we are server! */
#endif

#ifdef DEBUG
		/* if log file name doesn't contain a file path we must add the current path */
		if (!strchr(logfile, SLASH) && !strchr(logfile, BACKSLASH)) {
			char cesta[MAXPATH];

			char *ret = getcwd(cesta, sizeof(cesta));
			ret = ret; // UNUSED
			add_last_slash(cesta);
			strcat(cesta, logfile);
			strcpy(logfile, cesta);
		}
#endif
	}

 	/* set up unregistered version limitations */
	if (!registered) {
		if (dirbuf_lines > UNREG_DIRBUF_LIN)
			dirbuf_lines = UNREG_DIRBUF_LIN;
		_archive_mode = FALSE;
	}

	return vysledek;
}
/*******************************************************************************/

void do_server(void)
{
	static char name[MAXPATH], name2[MAXPATH];

	if (! _quiet_mode)
		puts("\n                            File transfer server\n");

	while(1) {
		int cmd;
		wait_for_client();

		switch(cmd = read_word()) {
			case M_PARS:
				receive_parameters();
				break;

			case M_QUIT:
				if (! _quiet_mode)
					puts("QUIT command received");
				return;

			case M_LQUIT:
				if (! _quiet_mode) {
					clrscr();
					puts("LQUIT command received, Server will wait for another connection...");
				}
				client_server_handshaking(FALSE);		/* wait for new connection */
				continue;								/* get the parameters immediately */

			case HI_SERVER:				/* interrupted client tries to connect again */
				if (! _quiet_mode)
					printf("Client tries to reconnect... ");
				client_reconnecting();
				if (! _quiet_mode)
					puts("Client reconnected.");
				continue;

			case M_PWD:
				get_cwd(name, MAXPATH);
				send_string(name);
				break;

			case M_CD:
				receive_string(name);
				write_word( change_dir(name, name2) ? M_OK : M_ERR );
				send_string(name2);
				break;

			case M_DIR:
				receive_string(name);
				list_dir(name, read_word(), dir_buffer);
				send_string(dir_buffer);
				break;

			case M_DRV:
				list_drives(dir_buffer);
				send_string(dir_buffer);
				break;

			case M_MD:
				receive_string(name);
				check_and_convert_filename(name);
				write_word( mkdir(name) ? M_ERR : M_OK);
				break;

			case M_PSTART:
				process_files_on_receiver_side();
				break;

			case M_GET:
				receive_string(name);
				copy_files(TRUE, name, FALSE);
				break;

			case M_GETDEL:
				receive_string(name);
				copy_files(TRUE, name, TRUE);
				break;

			case M_DEL:
				receive_string(name);
				delete_files(TRUE, name);
				break;

			case M_REN:
				receive_string(name);
				receive_string(name2);
				write_word( rename(name, name2) );
				break;

			case M_UTS:
				{
					/* underscore hack that will be removed once the new PROTOKOL is established */
					char temp[256];
					strcpy(temp, local_machine);
					if (remote_status_of_mkdir)	/* if client has this capability then */
						strcat(temp, "_");	/* confirm server capability via this HACK */
					/* end of underscore hack */
					send_string(temp);
				}
				break;

			case M_GETINFO:
				return_server_files_info();
				break;

			case M_GETTIME:
				GetLocalTimeAndSendItOver();
				break;

			case M_PUTTIME:
				ReceiveDataAndSetLocalTime();
				break;

			case M_EXCHANGE_FEATURES:
				receive_features();
				break;

			case M_SENDFILESINFO:
				receive_collected_info();
				break;

			default:
				write_word(M_UNKNOWN);		/* say 'unknown command' */
				if (! _quiet_mode)
					printf("Unrecognized command: %04x\n", cmd);
		}
	}
}
/*******************************************************************************/
void inicializace(void)
{
#ifdef __MSDOS__
	getcwd(original_path, sizeof(original_path));	/* remember the original path */
#endif

#ifdef ATARI
	DODisableCEH();
#endif

#ifndef STANDALONE

#ifdef IBM

#ifdef USB
	if (!usb_init(usb_serial)) exit(1);
#else
	if (! _quiet_mode) {
		printf("Parallel port base address: %x\n", print_port);
		printf("Parallel port type: ");
		switch(port_type) {
			case 0:	printf("unidirectional/ECP unable to switch to EPP\n"); break;
			case 1: printf("bidirectional/EPP\n"); break;
			case 2: printf("ECP switched to EPP\n"); break;
		}
		printf("Cable type: %s\n", cable_type ? "PARCP bidirectional" : "LapLink");
		if (cable_type) {
			if (PCunidirect)
				puts("UNI-BI HW parallel adapter has to be plugged-in.");
			else {
				if (port_type)
					puts("No HW adapter - PARCP cable is enough.");
				else
					errexit("Bidirectional routines on unidirectional port? Never!\n"\
							"Run PARCPCFG.EXE in order to correct your PARCP setup.", ERROR_BADCFG);
			}
		}
		else
			if (PCunidirect)
				errexit("UNI-BI HW parallel adapter and LapLink parallel cable? Bad idea!\n"\
						"Choose either UNI-BI adapter or LapLink cable - run PARCPCFG.EXE.", ERROR_BADCFG);
	}

#ifdef __LINUX__
	/* in Linux reserve the permission to access parallel port hardware directly */

	DPRINT("> Trying to get permisions to in/out the port directly\n");
	if (iopl(0)) {
		errexit("Need root privileges to drive parallel port\n", ERROR_NOTROOT);
	}
	ioperm(print_port,4,1);
#endif	/* __LINUX__ */

	if (port_type == 2 && !PCunidirect)
		ECP2EPP;

#endif  /* USB */

#endif	/* IBM */

#endif	/* STANDALONE */

#ifndef PARCP_SERVER
	/* try to find the real number of lines on the screen for client */
	{
#if defined(ATARI) || defined(__LINUX__)
		struct winsize term_size;

		if (ioctl(0,TIOCGWINSZ,&term_size) == 0) {
			page_length = term_size.ws_row;
			page_width  = term_size.ws_col;
		}
		else
#elif defined(_WIN32)
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
			page_length = csbi.srWindow.Bottom - csbi.srWindow.Top + 1; // csbi.dwSize.Y;
			page_width = csbi.dwSize.X;
		}
		else
#endif
		{
			char *p;
			if ( (p=getenv("LINES")) != NULL)
				page_length = atoi(p);
			if ( (p=getenv("COLUMNS")) != NULL)
				page_width  = atoi(p);
		}
	}
#endif	/* PARCP_SERVER */

	/* allocate RAM */
	block_buffer = NULL; dir_buffer = file_buffer = NULL;	/* tyto bufry alokovat az po handshake() */

	/* get system type (uname) */
	{
		struct utsname uts;
		if (uname(&uts) == 0) {
			strcpy(local_machine, uts.machine);
			strcat(local_machine, "/");
			strcat(local_machine, uts.sysname);
			strcat(local_machine, " ");
			strcat(local_machine, uts.release);
		}
		else
			*local_machine = 0;
	}

	/* initialize tables for fast CRC32 */
	init_CRC32();
}

/*******************************************************************************/

void client_server_handshaking(MYBOOL client)
{
	int val=0;
	char protorev[16];

	sprintf(protorev, "rev %x.%02x", PROTOKOL / 0x100, PROTOKOL % 0x100);

	SET_INPUT;
	STROBE_HIGH;

	if (client) {
		if (! _quiet_mode)
			printf("\n[CLIENT %s] Connecting to Server (will timeout in %d seconds)... ", protorev, time_out);

		/* make it more robust: request Server start before Client start */
		if (! IS_READY)
			errexit("Server has NOT been started yet - or a problem occured with parallel cable.\n"\
					"Check your HW and PARCP configuration, then start Server *BEFORE* Client.", ERROR_HANDSHAKE);

		usleep(2*WAIT4CLIENT);	/* keep the STROBE HIGH for a little while so that server can detect us */

		write_word(HI_SERVER);
		val = read_word();

		if (val != HI_CLIENT) {
			if (val == M_QUIT) {
				errexit("Write down the Server's debug info and contact PARCP author", ERROR_HANDSHAKE);
			}
			else {
				printf("\nClient's Debug info: expected $%04x, received $%04x\n", HI_CLIENT, val);
				errexit("Other side is not PARCP Server", ERROR_HANDSHAKE);
			}
		}

		write_word(PROTOKOL);
		val = read_word();

		if (val != M_OK) {
			if (val != M_QUIT) {
				printf("\nClient's Debug info: expected $%04x, received $%04x\n", M_OK, val);
			}
			errexit("Server uses different revision of PARCP communication protocol.", ERROR_HANDSHAKE);
		}
	}

	/* server handshake code follows: */
	else {
		if (! _quiet_mode)
			printf("\n[SERVER %s] Awaiting connection (press %s to quit)... ", protorev, STOP_KEYSTROKE);

		/* if the BUSY signal is LOW the other side is not set */
		/* do not confuse with client started too early! */
		while(! IS_READY) {
			usleep(WAIT4CLIENT);
			if (stop_waiting())
				errexit("Waiting for client connection interrupted by user.", ERROR_HANDSHAKE);
		}

		wait_for_client();
		val = read_word();

		if (val != HI_SERVER) {
			printf("\nServer's Debug info: expected $%04x, received $%04x\n", HI_SERVER, val);
			write_word(M_QUIT);
			errexit("Other side is not PARCP Client", ERROR_HANDSHAKE);
		}

		client_reconnecting();
	}
	if (! _quiet_mode)
		puts("OK\n");
}

/*******************************************************************************/

#include "parcommo.c"

int zpracovani_parametru(int argc, char *argv[])
{
	int i;
	extern int optind;
	extern char *optarg;
	MYBOOL konfigOK = FALSE;

	/* try to find the config file first */
	konfigOK = hledej_config(argv, cfg_fname);

#define ARG_OPTIONS		"sf:qb:u:c:"
	/* now parse some of the command line parameters (mainly the config file path) */
	while((i=getopt(argc, argv, ARG_OPTIONS)) != EOF) {
		switch(tolower(i)) {
			case 's':
				client = FALSE;
				break;

			case 'f':
				strcpy(cfg_fname, optarg);
				konfigOK = file_existuje(cfg_fname);
				break;

			case 'q':
				_quiet_mode = TRUE;
				break;

			case '?':
				puts(USAGE); exit(1);
		}
	}

	if (konfigOK) {
		int valid_num = 0;
		if (! _quiet_mode)
			printf("Configuration file used: %s\n", cfg_fname);
	
		valid_num = config_file(cfg_fname, FALSE);

		if (! _quiet_mode) {
			if (valid_num >= 0)
				printf("(valid directives found: %d)\n\n", valid_num);
			else
				printf("Error while reading/processing the config file.\n\n");
		}
	}
	else {
		/* config file not found! */
#if defined(ATARI) || defined(USB)
		/* on Atari or with USB simply create it with the default values */
		if (! _quiet_mode)
			printf("Configuration file not found.\nIt's being created: %s\n\n", cfg_fname);
		DPRINT1("! Creating configuration file: %s\n", cfg_fname);
		if (config_file(cfg_fname, TRUE) < 0)
			printf("ERROR: PARCP.CFG creation failed.\n");
#else
		/* on the other machines (where parallel port type/number is important) we must quit now */
# ifndef STANDALONE
		printf("PARCP configuration file (%s) not found.\nPlease start PARCPCFG in order to create it\n", cfg_fname);
		exit(ERROR_BADCFG);
# endif
#endif
	}

	/* now parse the rest of command line parameters (so they get higher priority over the config file values) */
	optind = 1;
	while((i=getopt(argc, argv, ARG_OPTIONS)) != EOF) {
		switch(tolower(i)) {
			case 'b':
				strcpy(autoexec, optarg);
				break;

#ifdef USB
			case 'u':
				if (!usb_init(usb_serial)) exit(1);
				int cmd = atoi(optarg);
				parcpusb_command(cmd);
				usb_exit();
				exit(1);
#endif

			case 'c':
#ifdef SHELL
				if (!strcmp("shell", optarg))
					shell = TRUE;
				else if (!strcmp("noshell", optarg))
					shell = FALSE;
#endif
				if (!strcmp("crc", optarg))
					_checksum = TRUE;
				else if (!strcmp("nocrc", optarg))
					_checksum = FALSE;
				break;
		}
	}

	return optind;		/* return the number of first non-parameter (i.e. the first file name) */
}

int main(int argc, char *argv[])
{
#if defined(DEBUG) || defined(WILL_EXPIRE)
	time_t start_time = time(NULL);
#endif
	int i;
#ifndef PARCP_SERVER
	MYBOOL go_interactive = TRUE;
#else
	client = FALSE;
#endif	/* PARCP_SERVER */

	setvbuf(stdout,NULL,_IONBF,0);	/* screen output buffering disabled */
	signal(SIGINT, catch_ctrl_c);	/* setup CTRL-C handler */
	INT_flag = FALSE;				/* clear CTRL-C flag */

	clrscr();						/* clear screen */

	/* read command line parameters and also the PARCP.CFG file */
	i = zpracovani_parametru(argc, argv);

	DPRINT1("  PARCP "VERZE" started on %s", ctime(&start_time));

	if (! _quiet_mode) {
		puts("PARallel CoPy - written by Petr Stehlik (c) 1996-2014.");
#ifdef DEBUG
		puts("DEBUG version "VERZE" (compiled on "__DATE__")\n");
#else
#ifdef BETA
		puts("Test version "VERZE"beta (compiled on "__DATE__")\n");
#else
		puts("Version "VERZE" (compiled on "__DATE__")\n");
#endif
#endif
	}

#ifdef WILL_EXPIRE
/* vaid till the end of October 1998 */
#define ROK		1998
#define MESIC	10
{
	struct tm *cas = localtime(&start_time);
	if ( (cas->tm_year > ROK) || (cas->tm_year == ROK && cas->tm_mon >= MESIC) ) {
		puts("\nSorry, this beta version expired. It's been destined for testing only,\n"\
			"so please go and obtain new, full release of PARCP.");
		return 0;
	}
}
#endif	/* WILL_EXPIRE */

#ifdef SHELL
	if (! shell)
#endif
	{
		if (registered && !_quiet_mode) {
			printf("This copy of PARCP is registered to %s\n\n", username);
		}
		else if (client) {	/* server will not complain about being unregistered */
			puts("Unregistered version can be used for evaluation for 30-days period.\n"
				"If you continue using PARCP after that period you should either\n"
				"register yourself or be ashamed for breaking Shareware principle.\n");
			sleep(5);
		}
	}

	inicializace();

#ifndef STANDALONE
	client_server_handshaking(client);
#endif

	if (client) {
#ifndef PARCP_SERVER
		/* exchange features */
		send_features();	/* sends also HACKish client_status_of_mkdir */

		/* send client parameters to the server */
		send_parameters();	/* allocate block and dir buffers here */

		/* detect server type */
		write_word(M_UTS);
		receive_string(remote_machine);

		/* underscore hack that will be removed once the PROTOKOL is increased */
		{
			int len = strlen(remote_machine);
			remote_status_of_mkdir = FALSE;
			if (remote_machine[len-1] == '_') {	/* server confirms it has this capability as well */
				remote_status_of_mkdir = TRUE;	/* alright, new protocol established */
				remote_machine[len-1] = '\0';	/* underscore removed */
			}
		}

		/* execute commands from auto exec file first */
		if (*autoexec) {
			if (registered) {
				FILE *fp = fopen(autoexec, "rt");

				if (fp != NULL) {
					printf("Using script file '%s'\n", autoexec);
					go_interactive = do_client(0, fp);
					fclose(fp);
				}
				else {
					printf("Script file '%s' not found.\n", autoexec);
					sleep(2);
				}
			}
			else {
				printf("Scripting is supported in registered version only.\n");
				sleep(5);
			}
		}

		if (i < argc) {			/* process file names on command line */
			do {
				char *p = argv[i];
				printf("Sending Drag&dropped file '%s'\n", p);

				remove_last_slash(p);	/* so that drag&dropped folders are copied as a whole */

				if (copy_files(TRUE, p, FALSE))	/* send them all */
					break;		/* if there were problems then quit */
			} while(++i < argc);

			write_word(afterdrop ? M_QUIT : M_LQUIT);	/* quit PARCP Server */

			go_interactive = FALSE;		/* quit also PARCP Client */
		}

		if (go_interactive) {
#ifdef SHELL
			if (shell)
				do_shell();		/* 'curses_initialized' will be set there */
			else
#endif
				do_client(0, stdin);
		}
#endif	/* PARCP_SERVER */
	}
	else
		do_server();

	port_reset();

#ifdef __MSDOS__
	chdir(original_path);
#endif
	return g_last_status;
}

/*
vim:ts=4:sw=4:
*/
