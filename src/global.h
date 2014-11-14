/* Extern definitions of global variables for PARCP */

extern MYBOOL registered;
extern char cfg_fname[MAXPATH];

extern char local_machine[MAXSTRING];
extern char remote_machine[MAXSTRING];

extern MYBOOL hash_mark;
extern MYBOOL _case_sensitive;
extern MYBOOL _preserve_case;
extern MYBOOL _show_hidden;
extern char _over_older;
extern char _over_newer;
extern MYBOOL _send_subdir;
extern MYBOOL _keep_timestamp;
extern MYBOOL _keep_attribs;
extern MYBOOL _archive_mode;
extern MYBOOL _check_info;
extern MYBOOL _assembler;
extern MYBOOL afterdrop;
extern MYBOOL _checksum;
extern char _sort_jak;
extern MYBOOL _sort_case;
extern short dirbuf_lines;
extern long buffer_lenkb;
extern BYTE filebuffers;
extern int time_out;
extern char username[MAXSTRING];
extern char keycode[MAXSTRING];
extern char autoexec[MAXPATH];
#ifdef SHELL
extern MYBOOL shell;
#endif
#ifdef USB
extern char usb_serial[MAXSTRING];
#endif

#ifdef DEBUG
extern short debuglevel;
extern char logfile[MAXSTRING];
extern char nolog_str[MAXSTRING];
extern char nodisp_str[MAXSTRING];
#endif
