/* Extern definitions of global variables for PARCP */

extern BOOLEAN registered;
extern char cfg_fname[MAXPATH];

extern char local_machine[MAXSTRING];
extern char remote_machine[MAXSTRING];

extern BOOLEAN hash_mark;
extern BOOLEAN _case_sensitive;
extern BOOLEAN _preserve_case;
extern BOOLEAN _show_hidden;
extern char _over_older;
extern char _over_newer;
extern BOOLEAN _send_subdir;
extern BOOLEAN _keep_timestamp;
extern BOOLEAN _keep_attribs;
extern BOOLEAN _archive_mode;
extern BOOLEAN _check_info;
extern BOOLEAN _assembler;
extern BOOLEAN afterdrop;
extern BOOLEAN _checksum;
extern char _sort_jak;
extern BOOLEAN _sort_case;
extern short dirbuf_lines;
extern long buffer_lenkb;
extern BYTE filebuffers;
extern int time_out;
extern char username[MAXSTRING];
extern char keycode[MAXSTRING];
extern char autoexec[MAXPATH];
#ifdef SHELL
extern BOOLEAN shell;
#endif

#ifdef DEBUG
extern short debuglevel;
extern char logfile[MAXSTRING];
extern char nolog_str[MAXSTRING];
extern char nodisp_str[MAXSTRING];
#endif
