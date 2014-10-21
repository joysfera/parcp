/* Routines for PARCP FTP-like command line client */

#ifdef __MSDOS__	/* Unix doesn't know getch() while DJGPP does */

#define get_key()	getch()

#else

#include <sys/ioctl.h>

int get_key()
{
	BYTE key=0;
#ifdef TCGETA	/* Linux uses POSIX */
	struct termios oldt, newt;

	tcgetattr(0, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);	/* don't echo, return immediately */
	tcsetattr(0,TCSANOW, &newt);	/* setup the keyboard */
	read(0,&key,1);					/* read key */
	tcsetattr(0,TCSANOW, &oldt);	/* restore the keyboard */

#else			/* MiNT builds on BSD and uses sgtty */

	struct sgttyb oldt, newt;

	ioctl(0,TIOCGETP,&oldt);
	newt = oldt;
	newt.sg_flags &= ~(ICANON|ECHO|CBREAK);/* don't echo, return immediately, watch out for the ^C */
	ioctl(0,TIOCSETP,&newt);		/* setup the keyboard */
	read(0,&key,1);					/* read key */
	ioctl(0,TIOCSETP,&oldt);		/* restore the keyboard */
#endif

	return key;
}
#endif /* !__MSDOS__ */
/*******************************************************************************/

int q_overwrite(const char *name)
{
	int ans;

	if (bInBatchMode) {
		ans = 1;	/* SKIP ==> default answer in unattended mode */
	}
	else {
#ifdef SHELL
		if (shell && curses_initialized) {
			ans = shell_q_overwrite(name);
		}
		else
#endif
		{
			printf("File '%s' already exists - Overwrite/Skip/Quit? [O/s/q]", name);
			switch(toupper(get_key())) {
				case 'S':
					ans=1;
					printf(" SKIP\n");		/* on the screen the "SKIP" info remains */
					break;
	
				case 'O':
				case '\r':
				case '\n':
					ans=2;
					printf("\r                                                                             \r");
					break;
	
				default:
					ans = 0;
					printf(" QUIT\n");
			}
		}
	}

	return ans;
}

/*******************************************************************************/

char *str_err(int status)
{
	switch(status) {
		case INTERRUPT_TRANSFER:
			return "File transfer was stopped";
		case FILE_SKIPPED:
			return "File skipped";
		case ERROR_CRC_FAILED:
			return "CRC failed - file not transferred correctly";
		case ERROR_READING_FILE:
			return "Error while reading file";
		case ERROR_WRITING_FILE:
			return "Error while writing file - disk read-only or full?";
		case ERROR_DELETING_FILE:
			return "Error deleting file - check if it's not read-only";
		default:
			{
				static char tmptxt[16];
				sprintf(tmptxt, "Error #%d", status);
				return tmptxt;
			}
	}
}

int q_bugreport(int status)
{
	int ans = 1;	/* IGNORE ==> default answer in unattended mode */

	if (status == QUIT_TRANSFER)
		return 0;		/* Abort */

	if (!status || status == FILE_SKIPPED)
		return ans;

	if (bInBatchMode) {
		return ans;
	}
	else {
		char *err_txt = str_err(status);

#ifdef SHELL
		if (shell && curses_initialized) {
			ans = shell_q_bugreport(err_txt);
		}
		else
#endif
		{
			printf("%s  - Retry/Ignore/Abort? [R/i/a]", err_txt);
			switch(toupper(get_key())) {
				case 'I':	/* Ignore */
					ans=1;
					printf(" Ignore\n");
					break;
	
				case 'R':	/* Retry */
				case '\r':
				case '\n':
					ans=2;
					printf("\r                                                                             \r");
					break;
	
				default:	/* Abort */
					ans = 0;
					printf(" Abort\n");
			}
		}
	}

	return ans;
}

#include "sort.c"

int real_delka_jmena(const char *jmeno, int min_delka)
{
	int j, real_delka = 0;

	for(j=min_delka; j<MAXFNAME; j++)
		if (jmeno[j] != ' ')
			real_delka = j+1;

	return real_delka;
}

char *orez_jmeno(const char *jmeno, int delka_jmena)
{
	static char radek[MAXSTRING];
	int real_delka = 0;

	memset(radek, ' ', sizeof(radek));
	strncpy(radek, jmeno, delka_jmena);

	if ( (real_delka = real_delka_jmena(jmeno, delka_jmena)) ) {
		/* we must display the shortened file name */
		radek[delka_jmena-4] = '~';
		strncpy(radek+delka_jmena-3, jmeno+real_delka-3, 3);	/* extension */
	}
	radek[MAXSTRING-1] = 0;

	return radek;
}

void view(const char *s, BOOLEAN is_dir)      /* view a multi-line string, pausing on screenful */
{
	int odchod, radek;
	int maxdel = 0;

	if (!isatty(fileno(stdout))) {
		puts(s);
		return;
	}

	if (is_dir) {
		const char *t = s;
		int x, maximalne = page_width-1-10-1-16;

		/* find the longest file name in this list */
		while(strlen(t) >= DIRLINELEN) {
			if ( (x = real_delka_jmena(t, 0)) > maxdel)
				maxdel = x;

			t+=DIRLINELEN;
		}
		if (maxdel > maximalne)
			maxdel = maximalne;

		DPRINT1(" maxdel = %d\n", maxdel);
	}

	radek = odchod = 0;

	while(!odchod) {
		int sloup=0, c = *s;

		if (is_dir && strlen(s) >= DIRLINELEN) {
			char *pradek;

			pradek = orez_jmeno(s, maxdel);
			strncpy(pradek+maxdel, s+MAXFNAME, DIRLINELEN-MAXFNAME);
			pradek[maxdel + DIRLINELEN-MAXFNAME] = 0;
			printf(pradek);
			s+=DIRLINELEN;
		}
		else {
			while((c = *s++) && ++sloup<page_width) {
				putchar(c);
				if (c=='\n')
					break;
			}
		}
		if (!c || !*s)
			break;
		radek++;
		if (!bInBatchMode && page_length > 0 && radek >= (page_length-2)) {
			int key;

			printf("..... more ..... [Return/Space/q]");
			key = get_key();
			printf("\r                                   \r");
			switch(key) {
				case 'q' : odchod = 1; break;
				case '\r':
				case '\n': radek = page_length-2; break; /* display one line */
				case ' ' :
				default  : radek = 0; break;	/* display whole page */
			}
		}
	}
}

/*******************************************************************************/

void normalize_over_switches(void)
{
	_over_older = toupper(_over_older);
	if (_over_older != 'S' && _over_older != 'R')
		_over_older = 'A';

	_over_newer = toupper(_over_newer);
	if (_over_newer != 'S' && _over_newer != 'R')
		_over_newer = 'A';
}

void print_status(int co)
{
#define ONOFF(a)	a?"YES":"NO"

	if (!co)
		puts("Version "VERZE" (compiled on "__DATE__")\n");
	if (!co || co == 1)
		printf("Hash mark: %s (block size is %ld kB)\n", ONOFF(hash_mark), buffer_len/KILO);
	if (!co || co == 2)
		printf("Upper/lower-case sensitive matching of filenames: %s\n", ONOFF(_case_sensitive));
	if (!co || co == 13)
		printf("Preserve DOS Filename Case: %s\n", ONOFF(_preserve_case));
	if (!co || co == 10)
		printf("Show hidden files: %s\n", ONOFF(_show_hidden));
	if (!co || co == 3)
		printf("Existing older files: %s\n", _over_older == 'S' ? "Skip" : _over_older == 'R' ? "Replace" : "Ask");
	if (!co || co == 9)
		printf("Existing newer files: %s\n", _over_newer == 'S' ? "Skip" : _over_newer == 'R' ? "Replace" : "Ask");
	if (!co || co == 4)
		printf("Sending whole subdirectory trees: %s\n", ONOFF(_send_subdir));
	if (!co || co == 5)
		printf("Keep timestamp of copied files: %s\n", ONOFF(_keep_timestamp));
	if (!co || co == 11)
		printf("Keep attributes of copied files: %s\n", ONOFF(_keep_attribs));
	if (!co || co == 12)
		printf("Archive mode: %s\n", ONOFF(_archive_mode));
	if (!co || co == 8)
		printf("CRC (check sum) of every copied block: %s\n", ONOFF(_checksum));
	if (!co || co == 6)
		printf("Page length is %d lines\n", page_length);
	if (!co || co == 7) {
		char how[MAXSTRING]="";

		if (_sort_case)
			strcat(how, "Case sensitive ");

		if (_sort_jak == tolower(_sort_jak))
			strcat(how, "Descending ");

		strcat(how, "Sorting by file ");
		switch(toupper(_sort_jak)) {
			case 'N':	strcat(how, "name\n");	break;
			case 'E':	strcat(how, "extension\n");	break;
			case 'S':	strcat(how, "size\n");	break;
			case 'D':	strcat(how, "date & time\n"); break;
			default:	strcpy(how, "Dir listing is unsorted\n");
		}
		printf(how);
	}
	puts("");
}

BOOLEAN do_client(int coming_from_shell, FILE *input_commands)
{
#define IS_ON(a)	(!strcasecmp(a, "ON") || !strcasecmp(a, "YES"))

	if (input_commands == NULL)
		input_commands = stdin;

	bInBatchMode = (input_commands != stdin);

	if (bInBatchMode) {
		puts("\nPARCP file transfer client in batch mode:\n");
	}
	else {
		puts("\n                            File transfer client\n");
		print_status(0);		/* display switches setting */
		printf("Client machine: %s\n", local_machine);
		printf("Server machine: %s\n\n", remote_machine);
	}

#ifdef SHELL
	if (coming_from_shell) {
		shell = FALSE;	/*  disable shell! */
		puts("Type EXIT to return back to ParShell\n");
	}
#endif

	g_last_status = 0;
	while(1) {
		static char b[MAXSTRING];
		char *p1, *p2;

		if (! bInBatchMode)
			printf(">>");
		fgets(b, sizeof(b), input_commands);

		if (bInBatchMode && feof(input_commands)) {	/* end of AUTOEXEC script */
			bInBatchMode = FALSE;
			return TRUE;
		}

		p1 = strtok(b, " \n");
		if (p1 == NULL) continue;
		p2 = strtok(NULL, "\n");

		if (!strcasecmp(p1, "QUIT") || !strcasecmp(p1, "LQUIT") || !strcasecmp(p1, "BYE")) {
			if (! coming_from_shell) {
				write_word( toupper(p1[0]) == 'L' ? M_LQUIT : M_QUIT );
				bInBatchMode = FALSE;
				return FALSE;
			}
#ifdef SHELL
			else
				puts("Type EXIT instead");
		}

		else if (!strcasecmp(p1, "EXIT")) {
			if (coming_from_shell) {
				g_last_status = 0;
				shell = TRUE;	/*  enable shell! */
				bInBatchMode = FALSE;
				return TRUE;
			}
			else
				puts("ParShell not available");
#endif
		}

		else if(!strcasecmp(p1, "PWD")) {	/* print server current dir */
			write_word(M_PWD);
			receive_string(string_buffer);
			puts(string_buffer);
		}

		else if(!strcasecmp(p1, "LPWD")) {	/* local current directory */
			get_cwd(b, MAXSTRING);
			puts(b);
		}

		else if (!strcasecmp(p1, "CD")) {	/* chg dir on server */
			if (p2 == NULL) {
				puts("ERROR: no directory given");
				continue;
			}
			write_word(M_CD);
			send_string(p2);
			if (read_word() == M_OK)
				printf("Directory on server is now ");
			receive_string(string_buffer);
			puts(string_buffer);
		}

		else if(!strcasecmp(p1, "LCD")) {	/* local change dir */
			if (p2 == NULL) {
				puts("ERROR: no directory given");
				continue;
			}
			if (change_dir(p2, string_buffer))
				printf("Local directory is now ");
			puts(string_buffer);
		}

		else if (!strcasecmp(p1, "MD") || !strcasecmp(p1, "MKDIR")) {	/* make dir on server */
			if (p2 == NULL) {
				puts("ERROR: no directory given");
				continue;
			}
			write_word(M_MD);
			send_string(p2);
			puts( (read_word() == M_OK) ? "OK" : "CAN\'T CREATE DIRECTORY");
		}

		else if (!strcasecmp(p1, "LMD") || !strcasecmp(p1, "LMKDIR")) {	/* make dir on client */
			if (p2 == NULL) {
				puts("ERROR: no directory given");
				continue;
			}
			puts( (mkdir(p2) == 0) ? "OK" : "CAN\'T CREATE DIRECTORY");
		}

		else if (!strcasecmp(p1, "DIR") || !strcasecmp(p1, "LS") 	/* dir listing on server */
			|| !strcasecmp(p1, "LDIR") || !strcasecmp(p1, "LLS")) {/* display local dir */
			BOOLEAN local = (toupper(*p1) == 'L');
			UWORD maska = 0;

			if (p2 == NULL)
				p2 = "";
			else if (*p2 == '-') {
				char *p3 = strtok(p2, " \n");
				p2 = strtok(NULL, "\n");
				if (p2 == NULL)
					p2 = "";
				/* additional parameters */
				while(*(++p3)) {
					if (*p3 == 'd')
						maska |= LS_DIRS_ONLY;
					if (*p3 == 'f')
						maska |= LS_FILES_ONLY;
					if (*p3 == 'n')
						maska |= LS_NEGATE_MASK;
				}
			}

			if (! local) {
				write_word(M_DIR);
				send_string(p2);
				write_word(maska);
				wait_before_read();
				receive_string(dir_buffer);
			}
			else {
				list_dir(p2, maska, dir_buffer);
			}
			setridit_list_dir(dir_buffer);
			view(dir_buffer, TRUE);
		}

		else if (!strcasecmp(p1, "DRV")) {	/* listing of server's drives */
			write_word(M_DRV);
			receive_string(dir_buffer);
			view(dir_buffer, TRUE);
		}

		else if(!strcasecmp(p1, "LDRV")) {	/* display local drives */
			list_drives(dir_buffer);
			view(dir_buffer, TRUE);
		}

		else if (!strcasecmp(p1, "PUT") || !strcasecmp(p1, "PUTDEL")    /* PUT(DEL) file to server */
			||	!strcasecmp(p1, "GET") || !strcasecmp(p1, "GETDEL")) {	/* GET(DEL) file from server */
			BOOLEAN sending = strncasecmp(p1, "PUT", 3) == 0 ? TRUE : FALSE;

			if (p2 == NULL)	{				/* local file name */
				puts("ERROR: no file name");
				continue;
			}

			if (_check_info) {
				get_files_info(sending, p2, TRUE);

				if (!g_files && !g_folders) {
					printf("ERROR: '%s' file(s) not found.\n", p2);
					g_last_status = FILE_NOTFOUND;
					continue;
				}
				if (! _quiet_mode) {
					char buf_size[MAXSTRING];
					show_size64(buf_size, g_bytes);
					printf("%s %lu files in %lu folders (total size %s)\n",
							sending ? "Sending" : "Receiving", g_files, g_folders,buf_size);
				}
				send_collected_info();
			}

			g_last_status = copy_files(sending, p2, (strncasecmp(p1+3, "DEL", 3) == 0));
		}

		else if (!strcasecmp(p1, "DEL") || !strcasecmp(p1, "LDEL")
			|| !strcasecmp(p1, "RM") || !strcasecmp(p1, "LRM")) {
			BOOLEAN local = (toupper(*p1) == 'L');

			if (p2 == NULL)	{				/* local file name */
				puts("ERROR: no file name");
				continue;
			}

			if (_check_info) {
				get_files_info(local, p2, FALSE);	/* FALSE = do not respect _archive_mode */

				if (!g_files && !g_folders) {
					printf("ERROR: '%s' file(s) not found.\n", p2);
					g_last_status = FILE_NOTFOUND;
					continue;
				}
				if (! _quiet_mode) {
					char buf_size[MAXSTRING];
					show_size64(buf_size, g_bytes);
					printf("Deleting %ld files in %ld folders (total size %s)\n",
							g_files, g_folders, buf_size);
				}
			}

			g_last_status = delete_files(local, p2);

			puts( g_last_status ? "CAN\'T DELETE FILE" : "OK" );
		}

		else if (!strcasecmp(p1, "REN") || !strcasecmp(p1, "LREN")) {
			char c[MAXFNAME+1], *p3;

			if (p2 == NULL)	{				/* local file name */
				puts("ERROR: no file name");
				continue;
			}
			if (is_pattern(p2)) {
				puts("ERROR: wildcards not allowed");
				continue;
			}

			printf("New name: ");
			fgets(c, (int)sizeof(c), input_commands);
			p3 = strtok(c, "\n");
			if (!p3 || !*p3) {
				puts("ERROR: no new file name");
				continue;
			}

			if (toupper(*p1) != 'L') {	/* REN */
				write_word(M_REN);
				send_string(p2);
				send_string(p3);
				g_last_status = read_word() ? FILE_NOTFOUND : 0;
			}
			else						/* LREN */
				g_last_status = rename(p2, p3) ? FILE_NOTFOUND : 0;

			puts( g_last_status ? "CAN\'T RENAME FILE" : "OK" );
		}

		else if (!strcasecmp(p1, "GETTIME")) {
			write_word(M_GETTIME);
			g_last_status = ReceiveDataAndSetLocalTime();
		}

		else if (!strcasecmp(p1, "PUTTIME")) {
			write_word(M_PUTTIME);
			g_last_status = GetLocalTimeAndSendItOver();
		}

		else if (!strcasecmp(p1, "SHOWTIME")) {
			time_t time_now = time(NULL);
			printf("Client's time is %s", ctime(&time_now));
			write_word(M_GETTIME);
			time_now = read_long();
			printf("Server's time is %s", ctime(&time_now));
		}

		else if (!strcasecmp(p1, "HASH")) {
			if (p2 != NULL)
				hash_mark = IS_ON(p2) ? TRUE : FALSE;
			else
				hash_mark = !hash_mark;

			print_status(1);
		}

		else if (!strcasecmp(p1, "CASE")) {
			if (p2 != NULL)
				_case_sensitive = IS_ON(p2) ? TRUE : FALSE;
			else
				_case_sensitive = !_case_sensitive;

			send_parameters();
			print_status(2);
		}

		else if (!strcasecmp(p1, "DOSCASE")) {
			if (p2 != NULL)
				_preserve_case = IS_ON(p2) ? TRUE : FALSE;
			else
				_preserve_case = !_preserve_case;

			send_parameters();
			print_status(13);
		}

		else if (!strcasecmp(p1, "HIDDEN")) {
			if (p2 != NULL)
				_show_hidden = IS_ON(p2) ? TRUE : FALSE;
			else
				_show_hidden = !_show_hidden;

			send_parameters();
			print_status(10);
		}

		else if (!strcasecmp(p1, "OVEROLD")) {
			if (p2 != NULL) {
				_over_older = toupper(*p2);
				normalize_over_switches();
				send_parameters();
			}
			print_status(3);
		}

		else if (!strcasecmp(p1, "OVERNEW")) {
			if (p2 != NULL) {
				_over_newer = toupper(*p2);
				normalize_over_switches();
				send_parameters();
			}
			print_status(9);
		}

		else if (!strcasecmp(p1, "SUBD")) {
			if (p2 != NULL)
				_send_subdir = IS_ON(p2) ? TRUE : FALSE;
			else
				_send_subdir = !_send_subdir;

			send_parameters();
			print_status(4);
		}

		else if (!strcasecmp(p1, "KEEP")) {
			if (p2 != NULL)
				_keep_timestamp = IS_ON(p2) ? TRUE : FALSE;
			else
				_keep_timestamp = !_keep_timestamp;

			send_parameters();
			print_status(5);
		}

		else if (!strcasecmp(p1, "ATTRIB")) {
			if (p2 != NULL)
				_keep_attribs = IS_ON(p2) ? TRUE : FALSE;
			else
				_keep_attribs = !_keep_attribs;

			send_parameters();
			print_status(11);
		}

		else if (!strcasecmp(p1, "ARCHIVE")) {
			if (p2 != NULL)
				_archive_mode = IS_ON(p2) ? TRUE : FALSE;
			else
				_archive_mode = !_archive_mode;

			if (!registered) {
				_archive_mode = FALSE;
				puts("The archive mode is disabled in unregistered version, sorry.");
			}

			send_parameters();
			print_status(12);
		}

		else if (!strcasecmp(p1, "CRC")) {
			if (p2 != NULL)
				_checksum = IS_ON(p2) ? TRUE : FALSE;
			else
				_checksum = !_checksum;

			send_parameters();
			print_status(8);
		}

		else if (!strcasecmp(p1, "PGLEN")) {
			if (p2 != NULL)
				page_length = atol(p2);

			print_status(6);
		}

		else if (!strcasecmp(p1, "SORT")) {
			if (p2 != NULL)
				_sort_jak = *p2;

			print_status(7);
		}

		else if (!strcasecmp(p1, "SORTCASE")) {
			if (p2 != NULL)
				_sort_case = IS_ON(p2) ? TRUE : FALSE;
			else
				_sort_case = !_sort_case;

			print_status(7);
		}

		else if (!strcasecmp(p1, "STAT"))
			print_status(0);

		else if (!strcasecmp(p1, "SAVE"))
			config_file(cfg_fname, TRUE);

		else if (!strcasecmp(p1, "HELP")) {
			char *PARCP_HELP = "\n PARCP commands:\n\n"
				"QUIT                 quit both Client and Server (AKA bye)\n"
				"LQUIT                quit only Client, Server wait for another session\n"
#ifdef SHELL
				"EXIT                 go back to ParShell\n"
#endif
				"\n"
				"PUT     template     send local files (from Client) to Server\n"
				"GET     template     receive files from Server to Client\n"
				"PUTDEL  template     just like PUT, but delete sent files on Client\n"
				"GETDEL  template     just like GET, but delete received files on Server\n"
				"DIR  [-fdn] [templ]  display file list on Server (AKA ls)\n"
				"LDIR [-fdn] [templ]  display local file list (AKA lls)\n"
				"DEL     template     delete files on Server (AKA rm)\n"
				"LDEL    template     delete local files (AKA lrm)\n"
				"REN     filename     rename file on Server\n"
				"LREN    filename     rename local file\n"
				"GETTIME              get Server's date/time and set it on Client\n"
				"PUTTIME              send Client's local date/time and set it on Server\n"
				"SHOWTTIME            show current date and time on both Client and Server\n"
				"\n"
				"PWD                  print current working directory on Server\n"
				"LPWD                 print current working directory on Client\n"
				"CD      dir          change directory on Server\n"
				"LCD     dir          change local directory (on Client)\n"
				"MD      dir          make directory on Server (AKA mkdir)\n"
				"LMD     dir          make directory on Client (AKA lmkdir)\n"
				"DRV                  display drives on Server\n"
				"LDRV                 display local drives\n"
				"\n"
				"HASH    [ON/OFF]     display hash marks (= dot per block)\n"
				"CASE    [ON/OFF]     upper/lower-case sensitive pattern matching\n"
				"DOSCASE [ON/OFF]     preserve DOS Filename Case\n"
				"HIDDEN  [ON/OFF]     show hidden files on MS-DOS filesystem\n"
				"OVEROLD [SRA]        replace or skip existing older files\n"
				"OVERNEW [SRA]        replace or skip existing newer files\n"
				"SUBD    [ON/OFF]     sending whole subdirectory trees\n"
				"KEEP    [ON/OFF]     keeping timestamp of copied files\n"
				"ATTRIB  [ON/OFF]     keeping attributes of copied files\n"
				"ARCHIVE [ON/OFF]     copy files in archive mode\n"
				"CRC     [ON/OFF]     do CRC (check sum) of every block\n"
				"PGLEN   [number]     length of view page\n"
				"SORT    [NnEeSsDdU]  sorting of dir listing by Name, Ext, Size, Date, Unsorted\n"
				"SORTCASE [ON/OFF]    case sensitive sorting\n"
				"\n"
				"STAT                 display actual settings\n"
				"SAVE                 save actual settings into configuration file\n";

			view(PARCP_HELP, FALSE);
		}

		else	/* bad command */
			printf("UNKNOWN COMMAND '%s'\n", p1);
	}
}
