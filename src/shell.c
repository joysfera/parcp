/*
 * PARallel CoPy - written for transferring large files between any two machines
 *                 with parallel ports.
 *
 * Petr Stehlik (c) 1996-2023
 *
 */

#include "parcp.h"
#include <curses.h>
#include <panel.h>
#include <string.h>
#include <stdlib.h>
#ifdef __LINUX__
#include <locale.h>
#endif
#ifdef ATARI
#include <osbind.h>		/* Kbshift() */
#include <support.h>	/* unx2dos() */
#endif
#include "global.h"		/* global vars */

#include "shell.h"		/* vyska, sirka */

int sirka, vyska;		/* originally COLS, LINES-1 */
MYBOOL is_resized;

#define stred	(sirka/2)
#define posazeni	4
#define WW		(stred-2)
#define WH		(vyska-posazeni-3)

#define RADEK(a,i)		(a->buf+(i)*DIRLINELEN)
#define JE_ADR(a,i)		(*(RADEK(a,i)+MAXFNAME+5) == '<')
#define OZNACENI(a,i)	(*(RADEK(a,i)+DIRLINELEN-1))

#define TITULEK		A_REVERSE

#define ZLEVA	1	/* width of left padding before file name */

#define PROGRESS_Y_OFFSET  -4 /* progress window moved up a bit */

#define SHELLCFGHEAD	"[PARSHELL]"
#define DRIVES_LIST		"List of installed logical drives"
#define CLI				"[command line interface]"		/* internal flag */

struct _okno {
	char *buf;
	int lines;
	char adresar[MAXPATH];
	WINDOW *pwin;
	int radek;
	int kurzor;
	MYBOOL pozice;
	MYBOOL remote;
	MYBOOL visible;
};

typedef struct _okno OKNO;

OKNO left, right;
WINDOW *pwinl = NULL, *pwinr = NULL, *pwincent = NULL;
PANEL *ppanl = NULL, *ppanr = NULL, *ppancent = NULL;
MYBOOL aktivni;

int original_cursor;	/* remember cursor state */
int progress_width = 60;
extern ULONG64 g_bytes;
ULONG64 shell_total_bytes;
extern unsigned long g_files, g_folders, g_start_time;
extern long buffer_len;		/* for Block Size setting */
unsigned long shell_total_files, shell_total_folders;
MYBOOL shell_arch_mode;	/* needed for passing info into zjistit_info() */

MYBOOL curses_initialized = FALSE;

MYBOOL has_bold = TRUE;
MYBOOL has_dim = TRUE;

MYBOOL use_colors=TRUE;
MYBOOL use_marks=FALSE;
MYBOOL smoothscroll=TRUE;
MYBOOL vypisovat_delku=TRUE, vypisovat_datum=TRUE, vypisovat_cas=FALSE;
MYBOOL _confirm_copy=TRUE, _confirm_move=TRUE, _confirm_delete=TRUE;
#ifdef ATARI
extern int _ncurses_ANSI_graphics;
#endif

char path_to_viewer[MAXPATH]="";
char path_to_editor[MAXPATH]="";
char path_to_temp[MAXPATH]="";
/******************/

#include "box.h"
#include "menu.h"
#include "viewer.h"

#define REALOKUJ_BUF_OKEN				\
{										\
	realokuj_buf_okna(&left);			\
	realokuj_buf_okna(&right);			\
}

#define OBNOV_OBE_OKNA					\
{										\
	nacti_obsah(&left, left.adresar);	\
	prekresli_stranku(&left);			\
	nacti_obsah(&right, right.adresar);	\
	prekresli_stranku(&right);			\
}

#define OBNOV_OBE_OKNA_S_CESTOU			\
{										\
	nacti_obsah_s_cestou(&left);		\
	prekresli_stranku(&left);			\
	nacti_obsah_s_cestou(&right);		\
	prekresli_stranku(&right);			\
}

#define PREKRESLI_OBE_STRANKY			\
{										\
	prekresli_stranku(&left);			\
	prekresli_stranku(&right);			\
}

MYBOOL oznaceny(OKNO *okno, int i)
{
	return (OZNACENI(okno, i) != '\n');		/* '\n' is there from list_dir() */
}

void soznaceny(OKNO *okno, int i, MYBOOL val)
{
	OZNACENI(okno, i) = val ? '\r' : '\n';	/* '\r' is simply a different value */
}

void prekresli_radek(OKNO *okno, int i)
{
	int index = okno->radek+i;
	char c;
	char *radek, *prad, *p;
	int atribut = 0;
	int delka_jmena;


	if (index >= okno->lines) {			/* empty lines below dir list */
		char tmprad[WW+1];
		memset(tmprad, ' ', WW);
		tmprad[WW] = 0;
		mvwaddstr(okno->pwin,i,0,tmprad);
		return;
	}

	if (okno->kurzor == index && okno->pozice == aktivni && okno->visible)
		atribut |= A_REVERSE;
	if (oznaceny(okno, index))
		atribut |= A_BOLD;
	wattron(okno->pwin, atribut);

	if (use_colors)
		wattron(okno->pwin, (oznaceny(okno, index)) ? COLOR_PAIR(2) : COLOR_PAIR(1) );

#if ZLEVA
	if (use_marks && oznaceny(okno, index))
		c = JE_ADR(okno,index) ? '#' : '*';
	else
		c = JE_ADR(okno,index) ? '<' : ' ';
	mvwaddch(okno->pwin,i,0,c | atribut);
#endif

	delka_jmena = WW-ZLEVA;
	if (vypisovat_delku)
		delka_jmena -= 10;
	if (vypisovat_datum)
		delka_jmena -= (1+10);
	if (vypisovat_cas)
		delka_jmena -= (1+5);
	if (delka_jmena > MAXFNAME)	/* unlikely */
		delka_jmena = MAXFNAME;

	radek = orez_jmeno(p = RADEK(okno,index), delka_jmena);

	prad = radek + delka_jmena;
	if (vypisovat_delku) {
		strncpy(prad, p+MAXFNAME, 10);
		prad += 10+1;
	}
	if (vypisovat_datum) {
		strncpy(prad, p+MAXFNAME+10+1,10);
		prad += 10+1;
	}
	if (vypisovat_cas)
		strncpy(prad, p+MAXFNAME+10+1+10+1,5);
	radek[WW-ZLEVA] = 0;
	mvwaddstr(okno->pwin,i,ZLEVA,radek);
	wattroff(okno->pwin, atribut);
}

void kurzor(OKNO *okno, MYBOOL visible)
{
	int radek = okno->kurzor - okno->radek;

	okno->visible = visible;

	prekresli_radek(okno, radek);
}

void prekresli_stranku(OKNO *okno)
{
	int i;

	for(i=0; i<WH; i++)
		prekresli_radek(okno, i);

	update_panels();
}

#if defined(ATARI) || defined(__MSDOS__)	// neither MiNTlib nor DJGPP know atoll
ULONG64 atoll(const char *c)
{
	ULONG64 result = 0;
	while(isspace(*c))
		c++;
	while(isdigit(*c)) {
		result = (result << 3) + (result << 1);	// => result * 10
		result += (*c++ - '0');
	}
	return result;
}
#endif

void vypis_podpis(OKNO *okno)
{
	int oznacenych=0, i;
	ULONG64 celkdelka=0;
	char podpis[MAXSTRING];

	/* count the number of selected files */
	for(i=0; i<okno->lines; i++)
		if (oznaceny(okno, i)) {
			oznacenych++;
			celkdelka += atoll(RADEK(okno, i)+MAXFNAME);
		}

	/* display bottom info line */
	if (oznacenych) {
		char buf_bytes[32];
		show_size64(buf_bytes, celkdelka);
		sprintf(podpis, "%s in %4d selected files\n", buf_bytes, oznacenych);
		if (use_colors)
			attron( COLOR_PAIR(2) );
		else
			attron(A_BOLD);
	}
	else {
		strcpy(podpis, RADEK(okno, okno->lines));
	}
	i = strlen(podpis)-1;	/*  strip the last '\n' */
	if (i < WW)
		memset(podpis+i, ' ', WW-i);
	podpis[WW]=0;
	mvaddstr(vyska-2,1+okno->pozice*stred,podpis);
	update_panels(); // refresh();
	if (oznacenych) {
		if (use_colors)
			attron( COLOR_PAIR(1) );
		else
			attroff(A_BOLD);
	}
}

char * convert_backslash_to_slash(char *cesta)
{
	int i = 0;
	while(cesta[i]) {
		if (cesta[i] == '\\')
			cesta[i] = '/';
		i++;
	}
	return cesta;
}

void nacti_obsah(OKNO *okno, const char *new_path)
{
	char adresar[MAXPATH] = "";
	char nadpis[MAXSTRING];
	int i;

	if (new_path)
		strcpy(adresar, new_path);

	if (!strcmp(adresar, DRIVES_LIST)) {
		if (okno->remote) {
			write_word(M_DRV);
			wait_before_read();
			receive_string(okno->buf);
		}
		else
			list_drives(okno->buf);

		okno->lines = strlen(okno->buf)/DIRLINELEN;
		sprintf(okno->buf + strlen(okno->buf), "%2d drives\n", okno->lines);
	}
	else {
		char *p = okno->buf;
		char *dirbufer = okno->buf + DIRLINELEN;	/* buffer for dir list */
		time_t dneska = time(NULL);
		struct tm *cas;
		MYBOOL chdir_flag;

		/* make sure we have folder with trailing slash */
		if (adresar[strlen(adresar)-1] != SLASH)
				strcat(adresar, SLASH_STR);

		p += sprintf(p, PRINTF_TEMPLATE, "..");
		p += sprintf(p, "     <DIR>");
		cas = localtime(&dneska);
		p += sprintf(p, " %4d/%02d/%02d %02d:%02d\n",1900+cas->tm_year,1+cas->tm_mon,cas->tm_mday,cas->tm_hour,cas->tm_min);
		okno->lines = 1;

		/* change current dir to the chosen path */
		if (okno->remote) {
			char tmp_adr[MAXPATH];

			write_word(M_CD);
			send_string(adresar);
			chdir_flag = (read_word() == M_OK);
			receive_string(tmp_adr);
		}
		else {
			chdir_flag = change_dir(adresar, NULL);
		}

		/* check if change dir was successful and if not go back to original directory */
		if (! chdir_flag) {
			char errmsg[MAXSTRING+26];
			sprintf(errmsg, "Can't go to '%s' directory.", adresar);
			myMessageBox(errmsg, myMB_OK);

			/* go back to original dir so shell can display a valid directory list */
			if (okno->remote) {
				write_word(M_PWD);
				receive_string(adresar);
			}
			else {
				get_cwd(adresar, MAXPATH);
			}

			convert_backslash_to_slash(adresar);
		}

		/* get list of files into dirbufer */
		if (okno->remote)
		{
			write_word(M_DIR);
			send_string("");	/* filemask */
			write_word(0);
			wait_before_read();
			receive_string(dirbufer);	/* dir list comes this way */
		}
		else {
			list_dir("", 0, dirbufer);	/* filemask */
		}

		/* sort that list dir */
		setridit_list_dir(dirbufer);

		/* set number of lines in ParShell window */
		okno->lines += strlen(dirbufer)/DIRLINELEN;
	}

	strcpy(okno->adresar, adresar);		/* remember the folder */

	/* display the upper info line */
	strcpy(nadpis, adresar);
	i = strlen(nadpis);
	if (i >= WW) {		/*  shorten the file length using 'c:/...' */
		nadpis[3] = nadpis[4] = nadpis[5] = '.';
		strcpy(nadpis+6,adresar+6+i-WW);
	}
	else
		memset(nadpis+i, ' ', WW-i);	/* right pad with spaces */

	// replace UTF-8 and other accents
	for(i = 0; i < WW; i++)
		if (nadpis[i] < 32)
			nadpis[i] = '?';

	nadpis[WW]=0;
	mvaddstr(posazeni-2,1+okno->pozice*stred,nadpis);

	/* display the bottom line */
	vypis_podpis(okno);

	/* if there are less files make sure the cursor position is correct */
	if (okno->kurzor >= okno->lines)
		okno->kurzor = okno->lines-1;
	if (okno->radek > okno->kurzor)
		okno->radek = okno->kurzor;

	prekresli_stranku(okno);
}

void nacti_obsah_s_cestou(OKNO *okno)
{
	char cesta[MAXPATH];

	if (okno->remote) {
		write_word(M_PWD);
		receive_string(cesta);
	}
	else
		get_cwd(cesta, sizeof(cesta));

	convert_backslash_to_slash(cesta);

	nacti_obsah(okno, cesta);
}

void realokuj_buf_okna(OKNO *okno)
{
	long amount = (dirbuf_lines+2) * DIRLINELEN;

	if ((okno->buf = realloc(okno->buf, amount)) == NULL)
		errexit("Fatal problem with reallocating memory. Aborting..", ERROR_MEMORY);
}

void inicializace_okna(OKNO *okno, MYBOOL pozice, MYBOOL remote)
{
	char tmpbuf[MAXSTRING+14];

	okno->buf = NULL;
	realokuj_buf_okna(okno);

	*(okno->adresar) = 0;
	okno->lines = 0;
	okno->pwin = pozice?pwinr:pwinl;
	okno->radek = 0;
	okno->kurzor = 0;
	okno->pozice = pozice;	/* right or left window */
	okno->remote = remote;	/* Server or Client */
	okno->visible = FALSE;

	if (remote)
		sprintf(tmpbuf, " Server  [%s] ", remote_machine);
	else
		sprintf(tmpbuf, " Client  [%s] ", local_machine);
	// limit too long uname() string
	tmpbuf[WW-2] = ' ';
	tmpbuf[WW-1] = '\0';
	mvaddstr(posazeni-3,2+stred*pozice, tmpbuf);

	nacti_obsah_s_cestou(okno);
}

OKNO *aktivizuj(MYBOOL prave)
{
	int i;
	OKNO *okno;
	char c;

	aktivni = prave;
	okno = aktivni ? &right : &left;
	kurzor(okno, TRUE);

	for(i=2;i<10;i++) {
		c = mvinch(posazeni-3,i) & A_CHARTEXT;
		mvaddch(posazeni-3,i,aktivni?c:c|TITULEK);
		c = mvinch(posazeni-3,stred+i) & A_CHARTEXT;
		mvaddch(posazeni-3,stred+i,!aktivni?c:c|TITULEK);
	}
	update_panels(); // refresh();
	PREKRESLI_OBE_STRANKY
	return okno;
}

char *vyjisti_jmeno(OKNO *okno, int radek)
{
	static char filename[MAXFNAME+1];
	char *p;

	strncpy(filename, RADEK(okno, radek), MAXFNAME);
	filename[MAXFNAME] = 0;
	p = filename + strlen(filename);
	while(*(--p) == ' ' && p >= filename)
		*p = 0;

	return filename;
}

char *pure_filename(OKNO *okno)
{
	return vyjisti_jmeno(okno, okno->kurzor);
}

void zmenit_adresar(OKNO *okno)
{
	char cesta[MAXPATH], adr[MAXPATH], najdi_dir[MAXPATH], *p;
	MYBOOL najdi_pozic = FALSE;

	/* get just the folder name */
	strcpy(cesta, pure_filename(okno));

	strcpy(adr, okno->adresar);
	DPRINT1("S zmenit '%s'\n", adr);

	if (! strcmp(cesta,"..")) {
		/* going up in the dir tree */

		*(adr + strlen(adr)-1) = 0;	/* strip out trailing slash */
		if ((p = strrchr(adr, '/')) != NULL) {
			strcpy(najdi_dir, p+1);
			najdi_pozic = TRUE;
			*(p+1) = 0;
		}
		else { /* display the logical disks list */
			strcpy(najdi_dir, adr);
			strcat(najdi_dir, SLASH_STR);	/* now we need the trailing slash again */
			najdi_pozic = TRUE;
			strcpy(adr, DRIVES_LIST);
		}
	}
	else {
		/* going down in the dir tree */

		if (strcmp(adr, DRIVES_LIST)) {
			strcat(adr, cesta);		/* down to the folder */
			strcat(adr,"/");
		}
		else
			strcpy(adr, cesta);		/* change the logical disk */
	}
	okno->radek=okno->kurzor=0;
	nacti_obsah(okno, adr);

	/* find out the location of the parent if you're going up */
	if (najdi_pozic) {
		int i;
		for(i=1; i<okno->lines; i++) {
			if (! strcmp(vyjisti_jmeno(okno,i), najdi_dir))
				okno->kurzor = i;
		}
		if (okno->kurzor >= WH) {
			if ((okno->radek = (okno->kurzor - WH+1)) < 0)
				okno->radek = 0;
		}
		prekresli_stranku(okno);
	}
}

/******************************************************************************/

void shell_open_progress_window(char *title, MYBOOL progress)
{
	int title_len = strlen(title);
	char *space_title = alloca(title_len+3);
	int ww, wh;

	if (progress) {
		ww = progress_width+2;
		wh = _check_info ? 6 : 4;
	}
	else {
		ww = title_len+4;
		wh = 3;
	}

	strcpy(space_title+1, title);
	space_title[0] = space_title[title_len+1] = ' ';
	space_title[title_len+2] = 0;

	pwincent = newwin(wh, ww, (vyska-wh)/2 + PROGRESS_Y_OFFSET, (sirka-ww)/2);
	nodelay(pwincent, TRUE);	/* do not wait for wgetch */
	ppancent = new_panel(pwincent);
	box(pwincent, ACS_VLINE, ACS_HLINE);
	wattron(pwincent, A_REVERSE);
	mvwaddstr(pwincent, 0, 1, space_title);
	wattroff(pwincent, A_REVERSE);
	if (progress && _check_info) {
		mvwaddstr(pwincent, 3, 1, "Total progress:");
#define REMAINING_TEXT "Remaining time "
		mvwaddstr(pwincent, 3, progress_width-8-sizeof(REMAINING_TEXT), REMAINING_TEXT);
	}
#ifdef ATARI
#define CANCEL_TEXT	" Press Esc or both Shifts to Cancel "
#else
#define CANCEL_TEXT	" Press Esc to Cancel "
#endif
	mvwaddstr(pwincent, wh-1, ww-1-strlen(CANCEL_TEXT), CANCEL_TEXT);
	update_panels();
	doupdate();
}

void shell_close_progress_window()
{
	del_panel(ppancent);
	delwin(pwincent);
	update_panels();
}

int shell_q_overwrite(const char *name)
{
	int ret=0;
	char tmptxt[MAXSTRING];

	sprintf(tmptxt,"File '%s' already exists. Do you want to overwrite it?", name);
	switch(myMessageBox(tmptxt, myMB_YESNOCANCEL)) {
		case myIDYES:	ret = 2; break;
		case myIDNO:	ret = 1; break;
		case myIDCANCEL: ret = 0; break;
	}
	return ret;
}

int shell_q_bugreport(const char *text)
{
	int reakce; 

	switch(myMessageBox(text, myMB_ABORTRETRYIGNORE)) {
		case myIDABORT:
			reakce = 0;
			break;
		case myIDRETRY:
			reakce = 2;
			break;
		default:
			reakce = 1;
	}

	return reakce;
}

/******************************************************************************/

void obnov_soucasne(OKNO *okno)
{
	nacti_obsah(okno, okno->adresar);
	prekresli_stranku(okno);
}

void obnov_opacne(OKNO *okno)
{
	if (! okno->pozice)
		okno = &right;
	else
		okno = &left;
	obnov_soucasne(okno);
}

int shell_config_file(char *soubor, MYBOOL vytvorit)
{
	int vysledek;

	struct Config_Tag sconfigs[] = {
#ifdef ATARI
		{"ANSIfont", Boolean_Tag, &_ncurses_ANSI_graphics },
#endif
		{ "UseColors", Boolean_Tag, &use_colors },
		{ "UseMarks", Boolean_Tag, &use_marks },
		{ "SmoothScroll", Boolean_Tag, &smoothscroll },
		{ "ShowSize", Boolean_Tag, &vypisovat_delku },
		{ "ShowDate", Boolean_Tag, &vypisovat_datum },
		{ "ShowTime", Boolean_Tag, &vypisovat_cas },
		{ "ConfirmCopy", Boolean_Tag, &_confirm_copy },
		{ "ConfirmMove", Boolean_Tag, &_confirm_move },
		{ "ConfirmDelete", Boolean_Tag, &_confirm_delete },
		{ "Viewer", String_Tag, path_to_viewer, sizeof(path_to_viewer) },
		{ "Editor", String_Tag, path_to_editor, sizeof(path_to_editor) },
		{ "TempDir", String_Tag, path_to_temp, sizeof(path_to_temp) },
		{ NULL , Error_Tag, NULL        }
	};

	if (vytvorit) {
		vysledek = update_config(soubor, sconfigs, SHELLCFGHEAD);
	}
	else {
		vysledek = input_config(soubor, sconfigs, SHELLCFGHEAD);

		/* parameters should be tested for correctness */
	}

	return vysledek;
}

int execute_command(char *cmd, char *cmdline)
{
	int ret_code = 0;

	if (cmd != NULL && *cmd) {
		char prikaz[MAXPATH], cmd_dir[MAXPATH], bak_pwd[MAXPATH]="";
		MYBOOL gotodir = FALSE;

		/* separate directory */
		split_filename(cmd, cmd_dir, NULL);

		gotodir = *cmd_dir ? TRUE : FALSE;

		if (gotodir) {
			char *ret = getcwd(bak_pwd, MAXPATH);
			ret = ret; // UNUSED
		}

		DPRINT2("c cmd_dir = '%s' bak_pwd = '%s'\n", cmd_dir, bak_pwd);
		/* build complete command line */
		strcpy(prikaz, cmd);

		if (cmdline != NULL && *cmdline) {
			char line[MAXPATH];
			if (gotodir && !is_absolute_path(cmdline)) {
				strcpy(line, bak_pwd);
				add_last_slash(line);
				strcat(line, cmdline);
			}
			else
				strcpy(line, cmdline);

			strcat(prikaz, " ");
#ifdef ATARI
			unx2dos(line, prikaz+strlen(prikaz));
#else
			strcat(prikaz, line);
#endif
			DPRINT1("c prikaz = '%s'\n", prikaz);
		}

		def_prog_mode();	/* save current tty mode */
		endwin();			/* restore original tty mode */
		//curs_set(original_cursor);
		if (strcmp(cmd, CLI)) {
			if (gotodir) {
				int ret = chdir(cmd_dir);
				ret = ret; // UNUSED
			}
			ret_code = system(prikaz);
			if (gotodir) {
				int ret = chdir(bak_pwd);
				ret = ret; // UNUSED
			}
		}
		else {
			setvbuf(stdout,NULL,_IONBF,0);	/* disable screen output buffering */
			do_client(1, stdin);
		}
		curs_set(0);		/* turn the cursor off again */
#ifdef __PDCURSES__
		touchwin(stdscr);
		touchwin(pwinl);
		touchwin(pwinr);
		// update_panels();
#endif
		refresh();			/* restore saved tty mode */
		update_panels();
	}
	else {
		myMessageBox("Command is not defined.", myMB_OK);
		ret_code = -1;
	}

	return ret_code;
}

/* set of functions called from process_entries (the function for processing selected items) */
int zpracovat_soubory(OKNO *okno, int i, MYBOOL pote_smazat)
{
	return copy_files(! okno->remote, vyjisti_jmeno(okno, i), pote_smazat);
}

int kopirovat(OKNO *okno, int i)
{
	return zpracovat_soubory(okno, i, FALSE);
}

int presouvat(OKNO *okno, int i)
{
	return zpracovat_soubory(okno, i, TRUE);
}

int mazat(OKNO *okno, int i)
{
	return delete_files(! okno->remote, vyjisti_jmeno(okno, i));
}

int zjistit_info(OKNO *okno, int i)
{
	return get_files_info(! okno->remote, vyjisti_jmeno(okno, i), shell_arch_mode);
}

void process_entries(OKNO *okno, int (*funkce)(OKNO *okno, int i))
{
	int i, radek, count = 0;
	MYBOOL ukoncit = FALSE;
	MYBOOL progress = TRUE;
	char *win_title = "";

	if (funkce == kopirovat)
		win_title = "Copying";
	else if (funkce == presouvat)
		win_title = "Moving";
	else if (funkce == mazat) {
		win_title = "Deleting";
		progress = FALSE;
	}
	else if (funkce == zjistit_info) {
		win_title = "Collecting info";
		progress = FALSE;
	}

	shell_open_progress_window(win_title, progress);

	for(i = 0; (i < okno->lines) && !ukoncit; i++) {
		if (! oznaceny(okno, i))
			continue;

		if ((*funkce)(okno, i) == QUIT_TRANSFER)
			ukoncit = TRUE;

		if (funkce == zjistit_info) {
			shell_total_bytes += g_bytes;
			shell_total_files += g_files;
			shell_total_folders += g_folders;
		}
		else {
			soznaceny(okno, i, FALSE);	/* unselect immediately after the action */
			radek = i - okno->radek;
			if (radek >= 0 && radek < WH) {
				hide_panel(ppancent);
				prekresli_radek(okno, radek);	/* the selected items get unselected one by one on the display - quite elegant */
				show_panel(ppancent);
			}
		}
		count++;					/* count the number of processed entries */
	}
	if (count == 0) {				/* there were none selected? */
		if (strcmp(pure_filename(okno), "..")) {	/* ".." cannot be processed! */
			(*funkce)(okno, okno->kurzor);			/* so process the item under cursor */

			if (funkce == zjistit_info) {
				shell_total_bytes = g_bytes;
				shell_total_files = g_files;
				shell_total_folders = g_folders;
			}
		}
/*
		else
			myMessageBox("The '..' stands for parent dir. You can't copy nor delete it.", myMB_OK);
*/
	}

	shell_close_progress_window();

	vypis_podpis(okno);
}

void zjistit_kompletni_info(OKNO *okno, MYBOOL arch_mode)
{
	shell_total_bytes = shell_total_files = shell_total_folders = 0;
	shell_arch_mode = arch_mode;

	process_entries(okno, zjistit_info);

	g_bytes = shell_total_bytes;
	g_files = shell_total_files;
	g_folders = shell_total_folders;
	g_start_time = TIMER;
}

#define CM_SORT		1
#define CM_SORTNAME	101
#define CM_SORTEXT 	102
#define CM_SORTDATE	103
#define CM_SORTSIZE	104
#define CM_UNSORTED	105
#define CM_SORTREV	106
#define CM_SORTCASE	107
#define CM_LIST		2
#define CM_LISTSIZE	201
#define CM_LISTDATE	202
#define CM_LISTTIME	203
#define	CM_PARCP	3
#define CM_SHELLSCROLL	301
#define CM_SHELLPATHVIEW	302
#define CM_SHELLPATHEDIT	303
#define CM_SHELLPATHTMP	304
#define CM_OPT_CRC	306
#define CM_OPT_OWO	307
#define CM_OWO_SKIP 3071
#define CM_OWO_REPL 3072
#define CM_OWO_ASK  3073
#define CM_OPT_OWN	308
#define CM_OWN_SKIP 3081
#define CM_OWN_REPL 3082
#define CM_OWN_ASK  3083
#ifdef ATARI
#define CM_SHELL_ANSI 309
#endif
#define CM_OPT_HID 	310
#define CM_OPT_ATTR	311
#define CM_OPT_ARCH	312
#define CM_OPT_TIME	313
/* #define CM_OPT_CASE 314 */
#define CM_OPT_CHCK 315
#define CM_OPT_FAST_ROUTINES	316
#define CM_OPT_DIRBUF_LINES		317
#define CM_OPT_BLOCK_SIZE		318
#define CM_OPT_FILE_BUFFERS		319
#define CM_OPT_PRESERVE			320
#define CM_SHELL_CONFCOPY		321
#define CM_SHELL_CONFMOVE		322
#define CM_SHELL_CONFDELETE		323

#define CM_SHELL	4
#define CM_SAVE		5
#define CM_LOAD		6
#define CM_QUIT		7

TMENU *pmenu;
#define MAX_MENU_ITEMS	87
TMENU *pitem[MAX_MENU_ITEMS];

void init_menu()
{
	int i=0, overpos;

	pitem[i++] = new_item("Dir listing", NULL, CM_LIST, -1);	/* 0 */
	pitem[i++] = new_item(NULL, NULL, 0, -1);
	pitem[i++] = new_item("List sorting", NULL, CM_SORT, -1);	/* 2 */
	pitem[i++] = new_item(NULL, NULL, 0, -1);
	pitem[i++] = new_item("PARCP Config", NULL, CM_PARCP, -1);		/* 4 */
	pitem[i++] = new_item("ParShell Config", NULL, CM_SHELL, -1);	/* 5 */
	pitem[i++] = new_item(NULL, NULL, 0, -1);
	pitem[i++] = new_item("Save settings", NULL, CM_SAVE, -1);
	pitem[i++] = new_item("Load settings", NULL, CM_LOAD, -1);
	pitem[i++] = new_item(NULL, NULL, 0, -1);
	pitem[i++] = new_item("Back to ParShell", NULL, 0, -1);
	pitem[i++] = new_item("Exit PARCP", NULL, CM_QUIT, -1);

	pitem[i++] = new_item("Size", NULL, CM_LISTSIZE, 0);
	pitem[i++] = new_item("Date", NULL, CM_LISTDATE, 0);
	pitem[i++] = new_item("Time", NULL, CM_LISTTIME, 0);
	pitem[i++] = new_item(NULL, NULL, 0, 0);
	pitem[i++] = new_item("Preserve case", NULL, CM_OPT_PRESERVE, 0);
	pitem[i++] = new_item("Show hidden files", NULL, CM_OPT_HID, 0);

	pitem[i++] = new_item("by Name", NULL, CM_SORTNAME, 2);
	pitem[i++] = new_item("by Ext", NULL, CM_SORTEXT, 2);
	pitem[i++] = new_item("by Date", NULL, CM_SORTDATE, 2);
	pitem[i++] = new_item("by Size", NULL, CM_SORTSIZE, 2);
	pitem[i++] = new_item("Unsorted", NULL, CM_UNSORTED, 2);
	pitem[i++] = new_item(NULL, NULL, 0, 2);
	pitem[i++] = new_item("Reverse", NULL, CM_SORTREV, 2);
	pitem[i++] = new_item(NULL, NULL, 0, 2);
	pitem[i++] = new_item("Case Sensitive", NULL, CM_SORTCASE, 2);

	/* volby pro PARCP (vlastnosti prenosu atd.) */
	pitem[i++] = new_item("Directory Lines", NULL, CM_OPT_DIRBUF_LINES, 4);
	pitem[i++] = new_item("Block Size", NULL, CM_OPT_BLOCK_SIZE, 4);
	pitem[i++] = new_item("File Buffers", NULL, CM_OPT_FILE_BUFFERS, 4);
	pitem[i++] = new_item(NULL, NULL, 0, 4);
	pitem[i++] = new_item("Fast Routines", NULL, CM_OPT_FAST_ROUTINES, 4);
	pitem[i++] = new_item("Control checksum", NULL, CM_OPT_CRC, 4);
/*	pitem[i++] = new_item("Case sensitive", NULL, CM_OPT_CASE, 4); */
	pitem[i++] = new_item("Collect info", NULL, CM_OPT_CHCK, 4);
	pitem[i++] = new_item(NULL, NULL, 0, 4);
	pitem[i++] = new_item("Keep file attribs", NULL, CM_OPT_ATTR, 4);
	pitem[i++] = new_item("Keep file timestamp", NULL, CM_OPT_TIME, 4);
	pitem[i++] = new_item("Archive Mode", NULL, CM_OPT_ARCH, 4);
	overpos = i;
	pitem[i++] = new_item("Overwrite older", NULL, CM_OPT_OWO, 4);
	pitem[i++] = new_item("Overwrite same&new", NULL, CM_OPT_OWN, 4);

	/* volby pro ParShell prostredi */
	pitem[i++] = new_item("Smooth scrolling", NULL, CM_SHELLSCROLL, 5);
	pitem[i++] = new_item("Confirm Copy", NULL, CM_SHELL_CONFCOPY, 5);
	pitem[i++] = new_item("Confirm Move", NULL, CM_SHELL_CONFMOVE, 5);
	pitem[i++] = new_item("Confirm Delete", NULL, CM_SHELL_CONFDELETE, 5);
#ifdef ATARI
	pitem[i++] = new_item("Use ANSI graphics", NULL, CM_SHELL_ANSI, 5);
#endif
	pitem[i++] = new_item(NULL, NULL, 0, 5);
	pitem[i++] = new_item("Path to viewer", NULL, CM_SHELLPATHVIEW, 5);
	pitem[i++] = new_item("Path to editor", NULL, CM_SHELLPATHEDIT, 5);
	pitem[i++] = new_item("Path to TMP", NULL, CM_SHELLPATHTMP, 5);

	/* podvolby pro Overwrite */
	pitem[i++] = new_item("Skip", NULL, CM_OWO_SKIP, overpos);
	pitem[i++] = new_item("Replace", NULL, CM_OWO_REPL, overpos);
	pitem[i++] = new_item("Ask", NULL, CM_OWO_ASK, overpos);
	pitem[i++] = new_item("Skip", NULL, CM_OWN_SKIP, overpos+1);
	pitem[i++] = new_item("Replace", NULL, CM_OWN_REPL, overpos+1);
	pitem[i++] = new_item("Ask", NULL, CM_OWN_ASK, overpos+1);

	pitem[i++] = NULL;

	pmenu = new_menu(pitem);
}

MYBOOL interakce_menu()
{
	MYBOOL obnovit_okna = FALSE;
	MYBOOL quit_parcp = FALSE;

	short old_pars = PackParameters();
	short old_dirbuflin = dirbuf_lines;
	char old_over_older = _over_older;
	char old_over_newer = _over_newer;
	char old_sort_jak = _sort_jak;
	MYBOOL old_sort_case = _sort_case;
	MYBOOL old_show_hidden = _show_hidden;
/*	MYBOOL old_case_sensitive = _case_sensitive; */
	MYBOOL old_preserve_case = _preserve_case;

	switch(toupper(_sort_jak)) {
		case 'N': click_radio(pmenu, CM_SORTNAME); break;
		case 'E': click_radio(pmenu, CM_SORTEXT); break;
		case 'S': click_radio(pmenu, CM_SORTSIZE); break;
		case 'D': click_radio(pmenu, CM_SORTDATE); break;
		default:  click_radio(pmenu, CM_UNSORTED); break;
	}

	toggle_icheck(pmenu, CM_SORTREV, (_sort_jak == tolower(_sort_jak)));
	toggle_icheck(pmenu, CM_SORTCASE, _sort_case);

	toggle_icheck(pmenu, CM_LISTSIZE, vypisovat_delku);
	toggle_icheck(pmenu, CM_LISTDATE, vypisovat_datum);
	toggle_icheck(pmenu, CM_LISTTIME, vypisovat_cas);

	toggle_icheck(pmenu, CM_OPT_FAST_ROUTINES, _assembler);
	toggle_icheck(pmenu, CM_OPT_CRC, _checksum);
	toggle_icheck(pmenu, CM_SHELLSCROLL, smoothscroll);
	toggle_icheck(pmenu, CM_SHELL_CONFCOPY, _confirm_copy);
	toggle_icheck(pmenu, CM_SHELL_CONFMOVE, _confirm_move);
	toggle_icheck(pmenu, CM_SHELL_CONFDELETE, _confirm_delete);
#ifdef ATARI
	toggle_icheck(pmenu, CM_SHELL_ANSI, _ncurses_ANSI_graphics);
#endif
/*	toggle_icheck(pmenu, CM_OPT_CASE, _case_sensitive); */
	toggle_icheck(pmenu, CM_OPT_PRESERVE, _preserve_case);
	toggle_icheck(pmenu, CM_OPT_HID, _show_hidden);
	toggle_icheck(pmenu, CM_OPT_ATTR, _keep_attribs);
	toggle_icheck(pmenu, CM_OPT_ARCH, _archive_mode);
	toggle_icheck(pmenu, CM_OPT_CHCK, _check_info);
	toggle_icheck(pmenu, CM_OPT_TIME, _keep_timestamp);

	click_radio(pmenu, _over_older == 'S' ? CM_OWO_SKIP : (_over_older == 'R' ? CM_OWO_REPL : CM_OWO_ASK));
	click_radio(pmenu, _over_newer == 'S' ? CM_OWN_SKIP : (_over_newer == 'R' ? CM_OWN_REPL : CM_OWN_ASK));

	switch(show_menu(pmenu,0,0)) {

		case CM_SAVE:
			for(;;) {
				if ((config_file(cfg_fname, TRUE) < 0) || (shell_config_file(cfg_fname, TRUE) < 0))
					if (myMessageBox("Error saving configuration. Isn't the PARCP.CFG write-protected?", myMB_ABORTRETRYIGNORE) == myIDRETRY)
						continue;
				break;
			}
			break;

		case CM_LOAD:
			config_file(cfg_fname, FALSE);
			shell_config_file(cfg_fname, FALSE);
			send_parameters();
			break;

		case CM_OPT_DIRBUF_LINES:
			if (EditNumber("Directory Lines", "Enter number of lines in the window", Word_Tag, &dirbuf_lines)) {
				send_parameters();
				REALOKUJ_BUF_OKEN;
				obnovit_okna = TRUE;
			}

			break;
		case CM_OPT_BLOCK_SIZE:
			if (EditNumber("Block Size", "Enter size of transferred block in kB", Long_Tag, &buffer_lenkb)) {
				buffer_len = buffer_lenkb * KILO;	/* update real buffer lenght variable */
				send_parameters();
			}
			break;
			
		case CM_OPT_FILE_BUFFERS:
			if (EditNumber("File Buffers", "Enter number of file buffers", Byte_Tag, &filebuffers)) {
				send_parameters();
			}
			break;
			
		case CM_SHELLPATHVIEW:
			EditBox("External Viewer","Enter path to external file viewer",path_to_viewer,sizeof(path_to_viewer));
			break;
			
		case CM_SHELLPATHEDIT:
			EditBox("External Editor","Enter path to external text editor",path_to_editor,sizeof(path_to_editor));
			break;
			
		case CM_SHELLPATHTMP:
			EditBox("Temporary place","Enter path to folder for temporary files",path_to_temp,sizeof(path_to_temp));
			break;
			
		case CM_QUIT:
			quit_parcp = TRUE;
			break;
	}
	if (is_checked(pmenu, CM_SORTNAME)) _sort_jak = 'N';
	else if (is_checked(pmenu, CM_SORTEXT)) _sort_jak = 'E';
	else if (is_checked(pmenu, CM_SORTDATE)) _sort_jak = 'D';
	else if (is_checked(pmenu, CM_SORTSIZE)) _sort_jak = 'S';
	else if (is_checked(pmenu, CM_UNSORTED)) _sort_jak = 'U';

	if (is_checked(pmenu, CM_SORTREV))
		_sort_jak = tolower(_sort_jak);
	else
		_sort_jak = toupper(_sort_jak);
	if (toupper(_sort_jak) == 'U')
		_sort_jak = 'U';				/* unsorted can't be reversed */
	_sort_case = is_checked(pmenu, CM_SORTCASE);

	if (is_checked(pmenu, CM_OWO_SKIP)) _over_older = 'S';
	else if (is_checked(pmenu, CM_OWO_REPL)) _over_older = 'R';
	else if (is_checked(pmenu, CM_OWO_ASK)) _over_older = 'A';

	if (is_checked(pmenu, CM_OWN_SKIP)) _over_newer = 'S';
	else if (is_checked(pmenu, CM_OWN_REPL)) _over_newer = 'R';
	else if (is_checked(pmenu, CM_OWN_ASK)) _over_newer = 'A';

	vypisovat_delku = is_checked(pmenu, CM_LISTSIZE);
	vypisovat_datum = is_checked(pmenu, CM_LISTDATE);
	vypisovat_cas = is_checked(pmenu, CM_LISTTIME);

	_assembler = is_checked(pmenu, CM_OPT_FAST_ROUTINES);
	_checksum = is_checked(pmenu, CM_OPT_CRC);
	smoothscroll = is_checked(pmenu, CM_SHELLSCROLL);
	_confirm_copy = is_checked(pmenu, CM_SHELL_CONFCOPY);
	_confirm_move = is_checked(pmenu, CM_SHELL_CONFMOVE);
	_confirm_delete = is_checked(pmenu, CM_SHELL_CONFDELETE);
#ifdef ATARI
	_ncurses_ANSI_graphics = is_checked(pmenu, CM_SHELL_ANSI);
#endif
/*	_case_sensitive = is_checked(pmenu, CM_OPT_CASE); */
	_preserve_case = is_checked(pmenu, CM_OPT_PRESERVE);
	_show_hidden = is_checked(pmenu, CM_OPT_HID);
	_keep_attribs = is_checked(pmenu, CM_OPT_ATTR);
	_keep_timestamp = is_checked(pmenu, CM_OPT_TIME);
	_archive_mode = is_checked(pmenu, CM_OPT_ARCH);
	_check_info = is_checked(pmenu, CM_OPT_CHCK);

	/* if any of the key parameters got changed send it to the Server! */
	if (old_over_older != _over_older || old_over_newer != _over_newer
		|| old_pars != PackParameters() || old_dirbuflin != dirbuf_lines)
	{
		send_parameters();		/* send changed parameters to server */
	}

	/* if one of the parameters that affect directory listing then restore the windows */
	if (_show_hidden != old_show_hidden ||			/* On/Off invisible files */
/*		_case_sensitive != old_case_sensitive || */	/* upper/lower names in match() */
		_preserve_case != old_preserve_case ||		/* DOS names conversion */
		(_sort_jak == 'U' && _sort_jak != old_sort_jak))	/* if unsorted listing is required then read it from disk again */
	{
		obnovit_okna = TRUE;
	}

	/* if sorting was changed the re-sort the buffered content (do not read it from disk again) */
	if (!obnovit_okna && (_sort_jak != old_sort_jak || _sort_case != old_sort_case)) {
		setridit_list_dir(left.buf+DIRLINELEN);
		setridit_list_dir(right.buf+DIRLINELEN);
	}

	if (obnovit_okna) {
		OBNOV_OBE_OKNA			/* we loose user selections, unfortunately */
	}
	else {
		PREKRESLI_OBE_STRANKY
	}

	return quit_parcp;
}

void konec_menu(void)
{
	int i = 0;
	while(pitem[i])
		free_item(pitem[i++]);
}

void initial_draw(void)
{
	/* header */
	char tmpstr[MAXSTRING];
	int i;
#ifdef STANDALONE
	sprintf(tmpstr, " PARCP "VERZE"demo by Petr Stehlik (c) 1996-2023");
#else
	sprintf(tmpstr, " PARCP "VERZE" by Petr Stehlik (c) 1996-2023.");
#endif	/* STANDALONE */
	standend();
	mvaddstr(0,0,tmpstr);

	/* button bar */
	strcpy(tmpstr," F1=Help F2=CLI F3=View F4=Edit F5=Copy F6=Move F7=MkDir F8=Del F9=Menu F10=LQuit F12=Quit");
	i = strlen(tmpstr);
	memset(tmpstr+i, ' ', sizeof(tmpstr)-i);
	tmpstr[sizeof(tmpstr)-1] = 0;
	if (sirka < sizeof(tmpstr))
		tmpstr[sirka-1] = 0;
	mvaddstr(vyska, 0, tmpstr);

	if (use_colors) attron(COLOR_PAIR(1));

	/* borders */
	mvaddch(posazeni-3,0,ACS_ULCORNER);hline(ACS_HLINE,sirka-2);mvaddch(posazeni-3,sirka-1,ACS_URCORNER);
	mvaddch(posazeni-3,stred,ACS_ULCORNER);mvaddch(posazeni-3,stred-1,ACS_URCORNER);
	move(posazeni-1,1);hline(ACS_HLINE,sirka-2);
	move(vyska-3,1);hline(ACS_HLINE,sirka-2);
	move(vyska-1,1);hline(ACS_HLINE,sirka-2);
	move(posazeni-2,0);vline(ACS_VLINE,vyska-posazeni+2);
	move(posazeni-2,stred-1);vline(ACS_VLINE,vyska-posazeni+1);
	move(posazeni-2,stred);vline(ACS_VLINE,vyska-posazeni+1);
	move(posazeni-2,sirka-1);vline(ACS_VLINE,vyska-posazeni+2);
	mvaddch(vyska-1,0,ACS_LLCORNER);hline(ACS_HLINE,sirka-2);mvaddch(vyska-1,sirka-1,ACS_LRCORNER);
	mvaddch(vyska-1,stred,ACS_LLCORNER);mvaddch(vyska-1,stred-1,ACS_LRCORNER);

	/* open working windows */
	pwinl = newwin(WH,WW,posazeni,1);
	keypad(pwinl,TRUE);
	ppanl = new_panel(pwinl);
	pwinr = newwin(WH,WW,posazeni,1+stred);
	keypad(pwinr,TRUE);
	ppanr = new_panel(pwinr);
	if (use_colors) {
		wattron(pwinl,COLOR_PAIR(1));
		wattron(pwinr,COLOR_PAIR(1));
	}

	inicializace_okna(&left, FALSE, FALSE);
#ifdef STANDALONE
	inicializace_okna(&right, TRUE, FALSE);
#else
	inicializace_okna(&right, TRUE, TRUE);
#endif
}

void final_cleanup(void)
{
	if (ppanl) {
		del_panel(ppanl);
		ppanl = NULL;
	}
	if (ppanr) {
		del_panel(ppanr);
		ppanr = NULL;
	}
	if (pwinl) {
		delwin(pwinl);
		pwinl = NULL;
	}
	if (pwinr) {
		delwin(pwinr);
		pwinr = NULL;
	}
}

void fetch_term_size(void)
{
	int x, y;
	getmaxyx(stdscr, y, x);
	sirka = x;
	vyska = y - 1;
	// safety boundaries
	if (sirka < 64) sirka = 64; // crashes due to shortening file names below zero length?
	if (sirka > 2*MAXSTRING) sirka = 2*MAXSTRING; // just in case...
}

void set_resize(int sig)
{
	is_resized = TRUE;
}

void handle_resize()
{
	endwin();
	refresh();
	final_cleanup();

	initscr();
	curs_set(0);
	noecho();

	fetch_term_size();
	is_resized = FALSE;

	clear();
	initial_draw();
	aktivizuj(aktivni);
	doupdate();
}

void scroll_up(OKNO *okno)
{
	if (okno->kurzor > 0) {
		kurzor(okno, 0);
		okno->kurzor--;

		if (okno->kurzor < okno->radek) {
			if (smoothscroll) {
				scrollok(okno->pwin,TRUE);
				wscrl(okno->pwin,-1);
				scrollok(okno->pwin,FALSE);
				okno->radek--;
				prekresli_radek(okno,0);
			}
			else {
				okno->radek -= WH/2;
				if (okno->radek < 0)
					okno->radek = 0;
				prekresli_stranku(okno);
			}
			update_panels();
		}
		kurzor(okno, 1);
		update_panels();
	}
}

void scroll_down(OKNO *okno)
{
	if (okno->kurzor < okno->lines-1) {
		kurzor(okno, 0);
		okno->kurzor++;

		if (okno->kurzor > okno->radek+WH-1) {
			if (smoothscroll) {
				scrollok(okno->pwin,TRUE);
				scroll(okno->pwin);
				scrollok(okno->pwin,FALSE);
				okno->radek++;
				prekresli_radek(okno,WH-1);
			}
			else {
				okno->radek += WH/2;
				if (okno->radek > okno->lines-WH)
					okno->radek = okno->lines-WH;
				prekresli_stranku(okno);
			}
			update_panels();
		}
		kurzor(okno, 1);
		update_panels();
	}
}

void select_item(OKNO *okno)
{
	if (strcmp(pure_filename(okno), "..")) {	/* ".." can't be selected! */
		soznaceny(okno, okno->kurzor, !oznaceny(okno, okno->kurzor));
		prekresli_radek(okno, okno->kurzor-okno->radek);
		vypis_podpis(okno);
	}
}

void activate_item(OKNO *okno)
{
	if (JE_ADR(okno,okno->kurzor))
		zmenit_adresar(okno);
	else
		/* define activate action for regular files */ ;
}

MYBOOL menubar_action(OKNO *okno, int x)
{
	char tmpstr[MAXSTRING];
	char tmpfnam[MAXFNAME];
	MYBOOL ukoncit_vse = FALSE;

	switch(x) {
		case 1:
#ifdef ATARI
# define TRANSFER_STOP_KEY	"Both Shifts"
# define KEYS_FOR_MOVING_CURSOR	"Arrow keys Up/Down move cursor, hold Shift for paging.\n"
# define FKEYS_BLOCKED		""
#else
# define TRANSFER_STOP_KEY	"Esc key"
# define KEYS_FOR_MOVING_CURSOR	"Arrow keys Up/Down and PageUp/Down/Home/End move cursor.\n"
# define FKEYS_BLOCKED		"Use numeric keys instead if function keys are not available.\n"
#endif
# define IMMEDIATE_BREAK	"Ctrl+C breaks file transfer immediately (sometimes).\n"
				myMessageBox("PARCP's home at http://joy.sophics.cz/parcp/\n\n"\
		"Help for ParShell user interface:\n\n"\
		"Tab key switches between Client (left) and Server (right) window.\n"\
		"Arrow keys Left/Right and '/' change directory.\n"\
		KEYS_FOR_MOVING_CURSOR\
		"Insert and SpaceBar keys select/unselect files.\n"\
		"Keys '+','-' and '*' on numeric keypad select files, too.\n"\
		"Typing lowercase moves cursor.\n"\
		"Typing uppercase selects files.\n"\
		"Del key deletes selected (if any) or current (under cursor) file.\n"\
		"Function key actions (F5=Copy) are listed at bottom of window.\n"FKEYS_BLOCKED\
		"Ctrl+R (or Esc key) refreshes contents.\n"\
		TRANSFER_STOP_KEY" stops file copying/moving/deleting.\n"IMMEDIATE_BREAK\
		"F12 (Shift+F10 on Atari) quits both Client and Server.", myMB_OK);
		break;

		case 2:
			execute_command(CLI, NULL);
			OBNOV_OBE_OKNA_S_CESTOU
			break;

		case 3:
			if (! JE_ADR(okno,okno->kurzor)) {
				int ret;
				char cesta[MAXPATH], fname[MAXFNAME];
				MYBOOL kopie = FALSE;

				get_cwd(cesta, sizeof(cesta));
				strcpy(fname, pure_filename(okno));

				if (okno->remote) {
					/* copy to TMP and then delete it */
					if (*path_to_temp) {
						DPRINT1("v going to change current dir to %s\n", path_to_temp);
						ret = chdir(path_to_temp);
						ret = ret; // UNUSED

						DPRINT("v going to copy file under cursor\n");

						shell_open_progress_window("Copying", TRUE);
						copy_files(FALSE, fname, FALSE);
						shell_close_progress_window();
						kopie = TRUE;
					}
					else
						myMessageBox("Path to TMP is not set!", myMB_OK);
				}
				else {
					int ret = chdir(okno->adresar);
					ret = ret; // UNUSED
				}

				if (*path_to_viewer)
					execute_command(path_to_viewer, fname);
				else
					viewer(fname);	/* internal viewer */

				if (kopie)
					remove(fname);

				ret = chdir(cesta);
				ret = ret; // UNUSED
			}
			break;

		case 4:
			if (! JE_ADR(okno,okno->kurzor)) {
				char cesta[MAXPATH], fname[MAXFNAME];
				MYBOOL kopie = FALSE;
				int ret;

				get_cwd(cesta, sizeof(cesta));
				strcpy(fname, pure_filename(okno));

				if (okno->remote) {
					/* copy to TMP and move it back after editing */
					if (*path_to_temp) {
						DPRINT1("v going to change current dir to %s\n", path_to_temp);
						ret = chdir(path_to_temp);
						ret = ret; // UNUSED

						DPRINT("v going to copy file under cursor\n");

						shell_open_progress_window("Reading", TRUE);
						copy_files(FALSE, fname, FALSE);
						shell_close_progress_window();

						kopie = TRUE;
					}
					else
						myMessageBox("Path to TMP is not set!", myMB_OK);
				}
				else {
					int ret = chdir(okno->adresar);
					ret = ret; // UNUSED;
				}

				if (*path_to_editor)
					execute_command(path_to_editor, fname);
				else
					myMessageBox("External editor is not set!", myMB_OK);

				if (kopie) {
					shell_open_progress_window("Writting", TRUE);
					copy_files(TRUE, fname, TRUE);
					shell_close_progress_window();
				}

				ret = chdir(cesta);
				ret = ret; // UNUSED

				obnov_soucasne(okno);
			}
			break;

		case 5:
			if (_check_info) {
				char buf_total[32];
				zjistit_kompletni_info(okno, TRUE);
				show_size64(buf_total, shell_total_bytes);
				sprintf(tmpstr, "Copy %lu files? (total size %s)", shell_total_files, buf_total);
				send_collected_info();
			}
			else
				strcpy(tmpstr, "Copy file(s)?");

			if (_confirm_copy) {
				if (myMessageBox(tmpstr, myMB_YESNO | myMB_DEFBUTTON1) == myIDNO)
					break;
			}
			process_entries(okno, kopirovat);
			obnov_opacne(okno);
			break;

		case 6:
			if (_check_info) {
				char buf_total[32];
				zjistit_kompletni_info(okno, TRUE);
				show_size64(buf_total, shell_total_bytes);
				sprintf(tmpstr, "Move %lu files? (total size %s)", shell_total_files, buf_total);
				send_collected_info();
			}
			else
				strcpy(tmpstr, "Move file(s)?");

			if (_confirm_move) {
				if (myMessageBox(tmpstr, myMB_YESNO | myMB_DEFBUTTON1) == myIDNO)
					break;
			}
			process_entries(okno, presouvat);
			OBNOV_OBE_OKNA
			break;

		case 7:
			*tmpfnam = 0;
			if (! EditBox("MkDir", "Enter name of new directory", tmpfnam, sizeof(tmpfnam)))
				break;
			if (*tmpfnam) {				/* if something was entered */
				int i;
				if (okno->remote) {
					write_word(M_MD);
					send_string(tmpfnam);
					read_word();
				}
				else
					mkdir(tmpfnam);

				obnov_soucasne(okno);

				/* position cursor on the just created folder */
				for(i=1; i<okno->lines; i++) {
					if (! strcmp(vyjisti_jmeno(okno,i), tmpfnam)) {
						okno->kurzor = i;
						okno->radek = okno->kurzor-WH+1;
						if (okno->radek < 0)
							okno->radek = 0;
						break;
					}
				}
				prekresli_stranku(okno);
			}
			break;

		case 8:
			if (_check_info) {
				zjistit_kompletni_info(okno, FALSE);	/* ignore _archive_mode */
				sprintf(tmpstr, "Delete %lu files in %lu folders?", shell_total_files, shell_total_folders);
			}
			else
				strcpy(tmpstr, "Delete file(s)?");

			if (_confirm_delete) {
				if (myMessageBox(tmpstr, myMB_YESNO | myMB_DEFBUTTON2) == myIDNO)
					break;
			}
			process_entries(okno, mazat);
			obnov_soucasne(okno);
			break;

		case 9:
			if (interakce_menu()) {
				int retval = myMessageBox("Quit also the PARCP Server?", myMB_YESNOCANCEL);
				if (retval != myIDCANCEL) {
					write_word(retval == myIDYES ? M_QUIT : M_LQUIT);
					ukoncit_vse = TRUE;
				}
			}
			break;

		case 10:
			write_word(M_LQUIT);
			ukoncit_vse = TRUE;
			break;

		case 11: /* fall through */
		case 12:
			write_word(M_QUIT);
			ukoncit_vse = TRUE;
			break;
	}
	return ukoncit_vse;
}

void do_shell(void)
{
	OKNO *okno;
	char selectmask[MAXFNAME]="";
	MYBOOL ukoncit_vse = FALSE;
	int i;

	/* read the config file - section [PARSHELL] */
	shell_config_file(cfg_fname, FALSE);

#ifdef __LINUX__
	setlocale(LC_ALL, "");
#endif
	initscr();
	curses_initialized = TRUE;
	noecho();
	original_cursor = curs_set(0);
	cbreak();
	keypad(stdscr,TRUE);
	meta(stdscr, TRUE);		/* allow 8-bit */
	mousemask(ALL_MOUSE_EVENTS, NULL);

	if (use_colors) {
		use_default_colors();
		start_color();
		init_pair(1,COLOR_WHITE,COLOR_BLUE);	/* define standard output - white on blue */
		init_pair(2,COLOR_YELLOW,COLOR_BLUE);	/* define selected files - yellow on blue */
	}

	fetch_term_size();
	is_resized = FALSE;
	signal(SIGWINCH, set_resize);

	/* figure out whether to use colors or not */
	use_colors &= has_colors();

#ifdef NCURSES_VERSION	/* because PDCurses doesn't know tigetstr */
	/* if neither colors not bold is available then the marks are simply a must */
	if (tigetstr("bold") <= 0)
		has_bold = FALSE;
	if (tigetstr("dim") <= 0)
		has_dim = FALSE;
#else
	has_dim = FALSE;	/* seems like PDCurses doesn't support DIM attrib */
#endif
	use_marks |= !use_colors && !has_bold;

	initial_draw();

	/* make the left window the active one */
	okno = aktivizuj(FALSE);

	init_menu();

	while(!ukoncit_vse) {
		int key;
		MYBOOL smazat_masku = TRUE;
		MEVENT mevent;

#ifdef ATARI
		while(Cconis()) Cnecin();
#endif
		update_panels();
		doupdate();
		// nodelay(okno->pwin, TRUE);
		halfdelay(10);
		do {
			key = wgetch(okno->pwin);
			if (is_resized) handle_resize();
		} while(key == ERR);
		cbreak();
		switch(key) {
			case KEY_MOUSE:
				if (getmouse(&mevent) == OK) {
					if (mevent.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON3_CLICKED)) {
						if (mevent.y >= 4 && mevent.y < vyska-1) {
							MYBOOL right_panel = (mevent.x >= stred);
							if (aktivni != right_panel) {
								// click on inactive window activates it
								okno = aktivizuj(right_panel);
							}
							// click in active window moves file cursor
							kurzor(okno, 0);
							okno->kurzor = okno->radek + mevent.y - 4;
							kurzor(okno, 1);
							update_panels();
							if (mevent.bstate & BUTTON3_CLICKED) {
								select_item(okno);
							}
							else if (mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
								activate_item(okno);
							}
						}
						else if (mevent.y == vyska) {
							ukoncit_vse = menubar_action(okno, (mevent.x / 8) + 1);
						}
					}
#ifdef BUTTON4_PRESSED
					else if (mevent.bstate & BUTTON4_PRESSED) {
						scroll_up(okno);
					}
#endif
#ifdef BUTTON5_PRESSED
					else if (mevent.bstate & BUTTON5_PRESSED) {
						scroll_down(okno);
					}
#endif
				}
				break;
			case 9:
				okno = aktivizuj(!aktivni);
				break;

			case '\r':
			case '\n':
#ifdef __PDCURSES__
			case PADENTER:
#endif
				activate_item(okno);
				break;

			/* new keys for quick navigation in the dir tree */
			/* go down in the tree */
			case KEY_RIGHT:
				if (JE_ADR(okno,okno->kurzor))
					zmenit_adresar(okno);
				break;

			/* go up in the tree */
			case KEY_LEFT:
				if (strcmp(vyjisti_jmeno(okno, 0), "..") == 0 && JE_ADR(okno, 0)) {
					okno->kurzor = 0;
					zmenit_adresar(okno);
				}
				break;

			/* scrolling */
			case KEY_UP:
				scroll_up(okno);
				break;

#ifdef ATARI
			case '8':
				if ((Kbshift(-1) & 3) == 0)	/* PgUp is Shift+ArrowUp */
					break;
				/* FALLTHRU */
#endif
			case KEY_PPAGE:
				if (okno->kurzor > 0) {
					kurzor(okno, 0);
					okno->kurzor -= WH;
					if (okno->kurzor < 0)
						okno->kurzor = 0;

					if (okno->kurzor < okno->radek) {
						okno->radek = okno->kurzor;
						prekresli_stranku(okno);
					}
					kurzor(okno, 1);
					update_panels();
				}
				break;

			case KEY_IC:	/* 'Insert' key */
			case ' ':		/* SpaceBar key */
				select_item(okno);
			/* fall thru KEY_DOWN */

			case KEY_DOWN:
				scroll_down(okno);
				break;

#ifdef ATARI
			case '2':
				if ((Kbshift(-1) & 3) == 0)	/* PgDn is Shift+ArrowDown */
					break;
				/* FALLTHRU */
#endif
			case KEY_NPAGE:
				if (okno->kurzor < okno->lines-1) {
					kurzor(okno, 0);
					okno->kurzor += WH;
					if (okno->kurzor >= okno->lines-1)
						okno->kurzor = okno->lines-1;

					if (okno->kurzor > okno->radek+WH-1) {
						okno->radek = okno->kurzor-WH+1;
						prekresli_stranku(okno);
					}
					kurzor(okno, 1);
					update_panels();
				}
				break;

			case KEY_HOME:
				okno->radek = okno->kurzor = 0;
				prekresli_stranku(okno);
				break;

#ifdef ATARI
			case '7':
#endif
			case KEY_END:
				okno->kurzor = okno->lines-1;
				okno->radek = okno->kurzor-WH+1;
				if (okno->radek < 0)
					okno->radek = 0;
				prekresli_stranku(okno);
				break;

			case '+':
#ifdef __PDCURSES__
			case PADPLUS:
#endif
				for(i = (strcmp(vyjisti_jmeno(okno, 0), "..") ? 0 : 1); i < okno->lines; i++)
					soznaceny(okno, i, TRUE);
				prekresli_stranku(okno);
				vypis_podpis(okno);
				break;

			case '-':
#ifdef __PDCURSES__
			case PADMINUS:
#endif
				for(i = (strcmp(vyjisti_jmeno(okno, 0), "..") ? 0 : 1); i < okno->lines; i++)
					soznaceny(okno, i, FALSE);
				prekresli_stranku(okno);
				vypis_podpis(okno);
				break;

			case '*':
#ifdef __PDCURSES__
			case PADSTAR:
#endif
				for(i = (strcmp(vyjisti_jmeno(okno, 0), "..") ? 0 : 1); i < okno->lines; i++)
					soznaceny(okno, i, !oznaceny(okno, i));
				prekresli_stranku(okno);
				vypis_podpis(okno);
				break;

			/* file functions */
			case '/':			/* open logical disk listing */
#ifdef __PDCURSES__
			case PADSLASH:
#endif
				strcpy(okno->adresar, DRIVES_LIST);
				okno->radek=okno->kurzor=0;
				obnov_soucasne(okno);
				break;

			case 27:					/* Esc = Desktop compatible hot-key */
			case 'R'-64:				/* Ctrl-R = NC compatible: Re-read directory */
				obnov_soucasne(okno);
				break;

			case KEY_HELP:
			case KEY_F(1):
#ifndef ATARI
			case '1':
#endif
				menubar_action(okno, 1);
				break;

			case KEY_F(2):
#ifndef ATARI
			case '2':
#endif
				menubar_action(okno, 2);
				break;

			case KEY_F(3):
#ifndef ATARI
			case '3':
#endif
				menubar_action(okno, 3);
				break;

			case KEY_F(4):
#ifndef ATARI
			case '4':
#endif
				menubar_action(okno, 4);
				break;

			case KEY_F(5):
#ifndef ATARI
			case '5':
#endif
				menubar_action(okno, 5);
				break;

			case KEY_F(6):
#ifndef ATARI
			case '6':
#endif
				menubar_action(okno, 6);
				break;

			case KEY_F(7):
#ifndef ATARI
			case '7':
#endif
				menubar_action(okno, 7);
				break;

			case KEY_DC:		/* Thing, Windows compatible */
			case KEY_F(8):
#ifndef ATARI
			case '8':
#endif
				menubar_action(okno, 8);
				break;

			case KEY_F(9):
#ifndef ATARI
			case '9':
#endif
				ukoncit_vse = menubar_action(okno, 9);
				break;

			case KEY_F(10):
#ifndef ATARI
			case '0':
#endif
				ukoncit_vse = menubar_action(okno, 10);
				break;

			case KEY_F(12):
				ukoncit_vse = menubar_action(okno, 11);
				break;

			case KEY_BACKSPACE:
#ifdef __PDCURSES__
			case 8:
#endif
				selectmask[0] = 0;
				break;

			default:
				smazat_masku = FALSE;
				if (isascii(key)) {
					if (strlen(selectmask) < MAXFNAME-1) {
						int selectlen = strlen(selectmask), pos, foundpos, counter;
						MYBOOL marking;
						selectmask[selectlen++] = key;
						selectmask[selectlen] = 0;
						marking = isupper(selectmask[0]);

						pos = marking ? 0 : okno->kurzor;
						counter = okno->lines;
						foundpos = -1;
						while(counter--) {
							MYBOOL found = (strncasecmp(selectmask, vyjisti_jmeno(okno, pos), selectlen) == 0);
							if (found) {
								if (foundpos < 0)
									foundpos = pos;
								if (! marking)
									break;
							}
							if (marking)
								soznaceny(okno, pos, found);

							if (++pos == okno->lines)
								pos = 0;
						}
						if (marking) {
							vypis_podpis(okno);
						}
						if (foundpos >= 0 && foundpos != okno->kurzor) {
							okno->kurzor = foundpos;
							okno->radek = okno->kurzor-WH+1;
							if (okno->radek < 0)
								okno->radek = 0;
							prekresli_stranku(okno);
						}
						else if (marking)
							prekresli_stranku(okno);

						if (foundpos < 0)
							beep();
					}
				}
				else
					beep();
		}
		if (smazat_masku)
			selectmask[0] = 0;
	}
	konec_menu();
	clear();
	endwin();
}
