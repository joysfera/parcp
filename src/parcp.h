/*
 * PARallel CoPy - written for transferring large files between any two machines
 * with parallel ports.
 *
 * Petr Stehlik (c) 1996-2015
 *
 */

#ifndef _PARCP_H
#define _PARCP_H

#define VERZE	"4.1.0"		/* displays on the screen when PARCP starts */

#define PROTOKOL		0x0380	/* UWORD that ensures compatibility of communication protocol between different versions of PARCP */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#ifndef _WIN32
# ifndef __APPLE__
#  include <sys/vfs.h>
# endif
# include <sys/utsname.h>
# include <termios.h>
#endif
#include <sys/time.h>
#include <time.h>			/* added for GCC 2.95.2 */
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <utime.h>

#include "element.h"

#include "cfgopts.h"
#include "parcpkey.h"
#include "parcperr.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  /* exclude winsock.h because of clash with select() */
#include <windows.h>         /* needed for WinAPI stuff */
#undef WIN32_LEAN_AND_MEAN

#define _fullpath(res,path,size) \
  (GetFullPathName ((path), (size), (res), NULL) ? (res) : NULL)

#define realpath(path,resolved_path) _fullpath(resolved_path, path, MAX_PATH)

/*********************** statfs *****************************/
/* fake block size */
#define FAKED_BLOCK_SIZE 512

/* linux-compatible values for fs type */
#define MSDOS_SUPER_MAGIC     0x4d44
#define NTFS_SUPER_MAGIC      0x5346544E

struct statfs {
	long    f_type;     /* type of filesystem (see below) */
	long    f_bsize;    /* optimal transfer block size */
	unsigned long    f_blocks;   /* total data blocks in file system */
	long    f_bfree;    /* free blocks in fs */
	unsigned long    f_bavail;   /* free blocks avail to non-superuser */
	long    f_files;    /* total file nodes in file system */
	long    f_ffree;    /* free file nodes in fs */
	unsigned long    f_fsid;     /* file system id */
	unsigned long    f_namelen;  /* maximum length of filenames */
	long    f_spare[6]; /* spare for later */
};

struct utsname {
	char sysname[65];
	char machine[65];
	char release[65];
};
#endif

#define ULONG64 unsigned long long

#define CFGFILE		"parcp.cfg"
#define CFGHEAD		"[PARCP]"
#define PARCPDIR	"PARCPDIR"

#define TIMER		(time(NULL))
#define TIME_OUT	10			/* main TIMEOUT is set to 10 seconds */
#define WAIT4CLIENT	100000UL			/* 100 ms for giving back the spare CPU cycles */
#ifndef _WIN32
# define mkdir(a)	mkdir(a,0777)		/* directory with proper access rights */
#endif

#define MAXSTRING	260
#define MAXPATH		260
#define MAXFNAME	100
#define PRINTF_TEMPLATE	"%-100s"
#define DIRLINELEN	(MAXFNAME+10+1+16+1)
#define DIRBUF_LIN	256
#define BUFFER_LENKB	16
#define	KILO	1024UL
#define	MAX_DEPTH	8
#define REPEAT_TRANSFER	3
#define REMAINING_TIME_UPDATE_RATE		3		/* how often the remaining time should be updated */

#define SLASH		'/'
#define SLASH_STR	"/"
#define BACKSLASH	'\\'
#define DVOJTEKA	':'
#define TOS_DEV		"/dev/"
#define Ltos_dev	strlen(TOS_DEV)

#define HASH_CHAR	'.'

#define USAGE	"Usage:\n"\
				"PARCP [-s] [-f configfile] [-b batchfile] [-q]\n"\
				"'-s' invokes PARCP Server\n"\
				"'-f configfile' points to PARCP alternate configuration file\n"\
				"'-b batchfile' points to PARCP batch file\n"\
				"'-q' means 'quiet mode' i.e. no messages are written on screen\n"

/******************************************************************************/
/* DO NOT change following values - they are important for handshaking */
#define HI_CLIENT	0x4843		/* 'HC' */
#define HI_SERVER	0x4853		/* 'HS' */
#define M_OK		0x0105
#define M_ERR       0xFFFF

/* following values can be changed without a risk providing you increase PROTOKOL version */
#define M_QUIT		0x0a00
#define M_LQUIT		0x0a01
#define M_PARS		0x0b00
#define M_PWD		0x0f01
#define M_CD		0x0f02
#define M_MD		0x0f03
#define M_PUT		0x0f04
#define M_PEND		0x0f05
#define M_GET		0x0f06
#define M_DIR		0x0f07
#define M_DEL		0x0f08
#define M_DRV		0x0f09
#define M_OWRITE	0x0f0a
#define M_OSKIP		0x0f0b
#define M_OQUIT		0x0f0c
#define M_REN       0x0f0d
#define M_INT		0x0f0e		/* file transfer interrupted */
#define M_FULL		0x0f0f		/* disk full while writting */
#define M_REPEAT	0x0f10		/* repeat block transfer */
#define M_UTS		0x0f11		/* send machine info */
#define M_GETDEL	0x0f12
#define M_OASK		0x0f13
#define M_GETINFO	0x0f14
#define M_GETTIME	0x0f15
#define M_PUTTIME	0x0f16
#define M_DELINFO	0x0f17
#define M_PSTART	0x0f18
#define M_EXCHANGE_FEATURES	0x0f19
#define M_SENDFILESINFO	0x0f20
#define M_EXEC		0x0f21
#define M_EXECNOWAIT 0x0f22

#define	M_UNKNOWN	0xe000		/* unknown command */

/* bitove prepinace parametru */
#define B_CASE		0x0001		/* TRUE = case sensitive matching */
#define B_HIDDEN	0x0002		/* TRUE = show hidden files on MS-DOS fs */
#define B_SUBDIR	0x0004		/* TRUE = traverse through folders recursively and sends all files */
#define B_TIMESTAMP	0x0008		/* TRUE = restore time stamp on copied file */
#define B_CHECKSUM	0x0010		/* TRUE = use CRC for data safety */
#define B_ATTRIBS 	0x0020		/* TRUE = restore file attributes on copied files */
#define B_ARCH_MODE	0x0040		/* TRUE = use DOS ATTRIB flag for file backup */
#define B_PRESERVE	0x0080		/* TRUE = preserve file name upper/lower case on DOS like filesystems */
#define B_CHECKINFO	0x0100		/* TRUE = show progress info */

#define	LS_DIRS_ONLY	0x0001
#define	LS_FILES_ONLY	0x0002
#define LS_NEGATE_MASK	0x0080

/******************************************************************************/
#define FEATURE_LONGLONG		(1UL << 0)
#define FEATURE_SENDFILESINFO	(1UL << 1)
#define FEATURE_MKDIR_STATUS	(1UL << 2)
#define FEATURE_CMD_EXEC		(1UL << 3)
#define ALL_FEATURES	(FEATURE_LONGLONG | FEATURE_SENDFILESINFO | FEATURE_MKDIR_STATUS | FEATURE_CMD_EXEC)

/******************************************************************************/

char *show_size64(char *buf, ULONG64 size);
void send_long(ULONG64);
ULONG64 receive_long();
void send_collected_info(void);
void split_filename(const char *pathname, char *path, char *name);
void prepare_fname_for_opendir(const char *pathname, char *path, char *name);
MYBOOL is_absolute_path(const char *path);
MYBOOL has_last_slash(const char *path);
char *add_last_slash(char *path);
char *remove_last_slash(char *path);

void errexit(const char *, int error_code);

char *orez_jmeno(const char *jmeno, int delka_jmena);
void view(const char *s, MYBOOL is_dir);
MYBOOL do_client(int, FILE *);
void do_server(void);
void inicializace(void);
void client_server_handshaking(MYBOOL client);
void wait_before_read(void);

MYBOOL change_dir(const char *p, char *q);
void list_dir(const char *p2, int maska, char *zacatek);
void setridit_list_dir(char *buffer);
int list_drives(char *p);
int delete_files(MYBOOL local, const char *del_mask);

UWORD read_word(void);
long read_long(void);
void read_block(BYTE *, long);
void receive_string(char *a);
long client_read_block(BYTE *, long);
long server_read_block(BYTE *, long);
long fast_client_read_block(BYTE *, long);
long fast_server_read_block(BYTE *, long);

void write_word(UWORD x);
void write_long(long x);
void write_block(const BYTE *, long);
void send_string(const char *a);
long client_write_block(const BYTE *, long);
long server_write_block(const BYTE *, long);
long fast_client_write_block(const BYTE *, long);
long fast_server_write_block(const BYTE *, long);
int get_files_info(MYBOOL local, const char *src_mask, MYBOOL arch_mode);
int GetLocalTimeAndSendItOver(void);
int ReceiveDataAndSetLocalTime(void);
//int get_server_files_info(const char *filemask, MYBOOL arch_mode);

UWORD PackParameters(void);
void send_parameters(void);

int copy_files(MYBOOL source, const char *p_srcmask, MYBOOL pote_smazat);
MYBOOL stop_waiting(void);
int config_file(const char *soubor, MYBOOL vytvorit);
char *get_cwd(char *path, int maxlen);
MYBOOL file_existuje(char *fname);
char *str_err(int status);

void parcp_sort(void *base, size_t nel, size_t width,
		   int (*comp) (const void *, const void *),
		   void (*swap) (void *, void *));

/* SHELL */
#ifdef SHELL
void do_shell(void);
/*
void shell_open_progress_info(const char *, const char *, long);
void shell_update_progress_info(long);
void shell_close_progress_info(void);
*/
int  shell_q_overwrite(const char *);
int shell_q_bugreport(const char *text);
#endif

#ifdef DEBUG
#define DPRINT(a)		debug_print(a)
#define	DPRINT1(a,b)	debug_print(a,b)
#define	DPRINT2(a,b,c)	debug_print(a,b,c)
void debug_print(const char *, ... );
#else
#define DPRINT(a)
#define DPRINT1(a,b)
#define DPRINT2(a,b,c)
#endif

#ifdef LOWDEBUG
#define LDPRINT(a)		debug_print(a)
#define	LDPRINT1(a,b)	debug_print(a,b)
#define	LDPRINT2(a,b,c)	debug_print(a,b,c)
void debug_print(const char *, ... );
#else
#define LDPRINT(a)
#define LDPRINT1(a,b)
#define LDPRINT2(a,b,c)
#endif

/*******************************************************************************/
#endif	/* _PARCP_H */
