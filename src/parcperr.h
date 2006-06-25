#ifndef _parcperr_h
#define _parcperr_h

/* list of PARCP result codes */

#define NO_ERROR				0
/* errors during file transfer */
#define INTERRUPT_TRANSFER		1
#define QUIT_TRANSFER			2
#define FILE_NOTFOUND			3
#define FILE_SKIPPED			4
#define ERROR_CRC_FAILED		5
#define ERROR_READING_FILE		6
#define ERROR_WRITING_FILE		7
#define ERROR_DELETING_FILE		8
/* fatal errors - PARCP exited */
#define ERROR_USERSTOP			100
#define ERROR_TIMEOUT			101
#define ERROR_BADDATA			102
#define ERROR_BUGPRG			103
#define ERROR_MEMORY			104
#define ERROR_HANDSHAKE			105
#define ERROR_BADCFG			106
#define ERROR_NOTROOT			107

#endif
