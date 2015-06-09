/* Global variables for PARCP */
#include "parcp.h"

MYBOOL registered = FALSE;			/* program behaves like registered / shareware */
char cfg_fname[MAXPATH];			/* file path to actual config file */

char local_machine[MAXSTRING];
char remote_machine[MAXSTRING];

MYBOOL hash_mark = TRUE;
MYBOOL _case_sensitive = TRUE;			/* in the match() consider upper/lower case */
MYBOOL _preserve_case = TRUE;			/* in the opendir() do not convert DOS filenames to lower case */
MYBOOL _show_hidden = FALSE;			/* show hidden files */
char _over_older = 'R';					/* overwrite older files without prompt [Replace]*/
char _over_newer = 'A';					/* overwrite newer files with prompt [Ask]*/
MYBOOL _send_subdir = TRUE;			/* send files in folders recursively */
MYBOOL _keep_timestamp = TRUE;			/* restore original timestamp on copied files */
MYBOOL _keep_attribs = FALSE;			/* restore original attributes on copied files */
MYBOOL _archive_mode = FALSE;			/* DOS-like fs: copy just files with ARCHIVE attribute unset and update the attribute on the original file after succesful copying */
MYBOOL _check_info = TRUE;				/* count files/folders before copy/delete */
MYBOOL _assembler = TRUE;				/* use fast transfer routines */
MYBOOL afterdrop = TRUE;				/* server quits after drag&drop */
MYBOOL _checksum = FALSE;				/* use CRC for better safety of transferred data blocks */
char _sort_jak = 'U';					/* how to sort files [Unsorted] */
MYBOOL _sort_case = FALSE;				/* sort upper/lower case differently */
UWORD dirbuf_lines = 1000;	/* reserved number of lines of 'DIRLINELEN' length */
long buffer_lenkb = BUFFER_LENKB;		/* the size of transferred block in kilobytes */
BYTE filebuffers = 1;					/* the number of blocks reserved for buffered file I/O */
int time_out = TIME_OUT;				/* timeout in seconds */
char username[MAXSTRING]="Unregistered User";
char keycode[MAXSTRING]="ABCDEFGHIJKLMNOP";
char autoexec[MAXPATH]="";
#ifdef SHELL
MYBOOL shell = TRUE;
#endif
#ifdef USB
char usb_serial[MAXSTRING] = "";
#endif

#ifdef DEBUG
short debuglevel=1;
char logfile[MAXSTRING]="parcp.log";
char nolog_str[MAXSTRING]="l";
char nodisp_str[MAXSTRING]="ldsr>!";
#endif
