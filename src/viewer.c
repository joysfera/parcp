#include "element.h"
#include <string.h>
#include <curses.h>
#include <panel.h>

#define REVERSE_INFOLINE

#define ESC		27
#define KEY_BACKS 	0x8
#define MAXCHAR		160
#define POCETRADKU 	(LINES-1)
#define LASTLINE	(POCETRADKU-1)
#define DELKASTRANY	POCETRADKU

long	aktradek,	/* number of the actual line */
	ActPos,		/* number of the first displayed line */
	LastPagePos,	/* position of the first undisplayable line on the first row */
	EndPos,
	poslstrana,
	pocetradku,	/* number of lines of the whole document */
	pocetstran,	/* number of pages of the whole document */
	pgbuf;		/* offset of the text in the buffer */

BYTE	Sestava[512],	/* buffer for one line */
	String[256],	/* space for last searched string */
	view_fname[260];

int	StartCh,	/* horizontal offset */
	HWscroll,	/* insert&delete features capability */
	cursor_visibility,
	tab_size=8;

FILE *fdd;

WINDOW *okno, *headline;
PANEL *pokno, *phead;

extern BYTE *block_buffer;
extern long buffer_len;

#define	Redraw	showpage(aktradek)

BYTE soubor(pos)
long pos;
{
	size_t newpage, bufcount;

	if((newpage = (pos / buffer_len)) != pgbuf) {
		pgbuf = newpage;
		fseek(fdd, pgbuf * buffer_len, SEEK_SET);
		bufcount = fread(block_buffer, 1, buffer_len, fdd);
		memset(block_buffer+bufcount, '\n', buffer_len-bufcount);
	}
	return ( block_buffer[pos % buffer_len] );
}

long findprevnl(pos)
long pos;
{
	if (pos) pos--;		/* move to the end of the previous line */
	while(pos > 0 && soubor(pos - 1) != '\n')
		--pos;
	return pos;
}

long findnextnl(pos)
long pos;
{
	long i = pos;
	while(i < EndPos && soubor(i) != '\n')
		i++;
	if (i < EndPos)		/* if we are somewhere in the middle of the text */
		return i + 1;	/* then return the next position after '\n' */
	else
		return pos;	/* otherwise return the same position we started from */
}

void spocitej()
{
	long pos;
	int i;

	fseek(fdd,0L,SEEK_END);
	pos = EndPos = ftell(fdd);
	pocetradku = 1;
	while((pos = findprevnl(pos)) > 0)
		pocetradku++;
	pocetstran = pocetradku / DELKASTRANY;
	if (pocetradku % DELKASTRANY)
		pocetstran++;
	if ((poslstrana = pocetradku - POCETRADKU) < 0)
		poslstrana = 0;
	pos = EndPos;	
	for(i = 0; i < POCETRADKU; i++)
		if ((pos = findprevnl(pos)) == 0)
			break;
	LastPagePos = pos;
}

void ctiradek(pos)
long pos;
/* Limitations:
   1) last line must contain '\n'
   2) each line must be shorter than 512 characters
*/
{
	int i, j;
	BYTE a;

	memset(Sestava, ' ', COLS);
	for(i = 0; i < StartCh; i++) {
		if ((a = soubor(pos++)) == '\n') {
			strcpy(Sestava,"");
			return;
		}
	}
	for(i = 0; i < COLS; ) {
		if ((a = soubor(pos++)) == '\n')
			break;
		if (a == '\t') {
			a = ' ';
			for(j=(i+1)%tab_size; j%tab_size; j++)
				Sestava[i++] = a;
		}
		else if (a < ' ')
			a = ' ';
		Sestava[i++] = a;
	}
	Sestava[COLS-1] = '\0';
}

void showpage(radek)
long radek;
{
	long pozice, i;

	if (radek == aktradek-1) {
		if (radek >= 0) {
			aktradek = radek;
			ActPos = findprevnl(ActPos);
			ctiradek(ActPos);
			wscrl(okno,-1);
			mvwaddstr(okno, 0, 0, Sestava);
		}
	}
	else if (radek == aktradek+1) {
		if (radek <= poslstrana) {
			aktradek = radek;
			ActPos = findnextnl(ActPos);
			pozice = ActPos;
			for(i = 0; i < LASTLINE; i++)
				pozice = findnextnl(pozice);
			ctiradek(pozice);
			scroll(okno);
			mvwaddstr(okno, LASTLINE, 0, Sestava);
		}
	}
	else {
		if (radek <= 0) {
			aktradek = radek = 0;
			ActPos = 0;
		}
		else if (radek >= poslstrana) {
			aktradek = radek = poslstrana;
			ActPos = LastPagePos;
		}
		if (aktradek < radek)
			for(i = radek - aktradek; i > 0; --i)
				ActPos = findnextnl(ActPos);
		if (aktradek > radek)
			for(i = aktradek - radek; i > 0; --i)
				ActPos = findprevnl(ActPos);
		aktradek = radek;
		pozice = ActPos;
		for (i = 0; i <= LASTLINE; i++) {
			ctiradek(pozice);
			pozice = findnextnl(pozice);
			mvwaddstr(okno, i, 0, Sestava);
		}
	}

#ifdef REVERSE_INFOLINE
	wattron(headline, A_REVERSE);
#endif
	mvwprintw(headline,0,0,"'%s' Line: %d  Page: %d/%d  Press F1 for help ",view_fname,aktradek+1,(aktradek+1)/DELKASTRANY+1,pocetstran);
#ifdef REVERSE_INFOLINE
	wattroff(headline, A_REVERSE);
#endif
	wclrtoeol(headline);
	update_panels();
}

void tisk_inverz(text)
BYTE *text;
{
	wattron(headline,A_REVERSE);
	mvwaddstr(headline,0,0,text);
	wattroff(headline,A_REVERSE);
	wclrtoeol(headline);
}

int Nacti_cislo()
{
	int c, cislo = 0;

	curs_set(cursor_visibility);
	for(;;) {
		update_panels();
		doupdate();
		c = wgetch(headline);
		if (c == '\n')
			break;
		if (c >= '0' && c <= '9') {
			waddch(headline, c);
			cislo = cislo*10 + (c - '0');
		}
	}
	curs_set(0);
	return cislo;
}

void ReadString(StringName)
BYTE	*StringName;
{
	curs_set(cursor_visibility);
	getstr(StringName);
	curs_set(0);
}

long FindDString()
{
	long pos, kon, radek;
	int j, Find, delka;

	delka = strlen(String);
	for(radek = aktradek+1, kon = findnextnl(ActPos); radek < pocetradku; radek++) {
		pos = kon;
		kon = findnextnl(pos);
		while(pos < kon)
			if (soubor(pos++) == String[0]) {
				Find = TRUE;
				for (j = 1; j < delka; j++)
					if (soubor(pos++) != String[j]) {
						Find = FALSE;
						break;
					}
				if (Find)
					return radek;
			}
	}
	return -1;
}

void FindDownString(Repeat)
int	Repeat;
{
	long radek;

	if (! Repeat) {
		tisk_inverz(" Search string: ");
		ReadString(String);
	}
	tisk_inverz(" Searching... ");
	if ((radek = FindDString()) >= 0)
		showpage(radek);
	else {
		tisk_inverz(" String not found. ");
		beep();
	}
}

void Help()
{
	werase(okno);
	waddstr(okno, "ParShell Internal Viewer (c) Petr Stehlik 1994,1997\n\n");
	waddstr(okno, "Navigation:\n");
	waddstr(okno, "-----------\n");
	waddstr(okno, "Cursor keys, PageUp/Down, Home/End\n");
	waddstr(okno, "F1 .... this help\n");
	waddstr(okno, "F2 .... first page\n");
	waddstr(okno, "F3 .... last page\n");
	waddstr(okno, "F4 .... direct jump to a page\n");
	waddstr(okno, "F7 .... find a string\n");
	waddstr(okno, "F5 .... repeat finding\n");
	waddstr(okno, "F8 .... change tabstop\n");
	waddstr(okno, "F9 .... redraw screen\n");
	waddstr(okno, "F10 ... exit viewer\n");
	waddstr(okno, "\npress Return");
	update_panels();
	doupdate();
	wgetch(okno);
	Redraw;
}

void KeyLoop()
{
	int Done = FALSE;
	long nastranu;

	while (! Done) {
		update_panels();
		doupdate();
		switch(wgetch(okno)) {
		case KEY_F(2):	showpage(0); break;
		case KEY_F(3):	showpage(poslstrana); break;
		case KEY_UP	:	showpage(aktradek-1); break;
		case KEY_DOWN:	showpage(aktradek+1); break;
		case '8':
		case KEY_PPAGE:	showpage(aktradek-POCETRADKU); break;
		case '2':
		case KEY_NPAGE:	showpage(aktradek+POCETRADKU); break;
		case KEY_LEFT:	if (StartCh > 0)
							--StartCh;
						Redraw;
						break;
		case KEY_RIGHT:	if (StartCh < MAXCHAR-COLS)
							StartCh++;
						Redraw;
						break;
		case KEY_HOME:	StartCh = 0;
						Redraw;
						break;
		case KEY_END:	StartCh = MAXCHAR-COLS;
						Redraw;
						break;
		case KEY_F(8):	if ( (tab_size <<= 1) > 8)
							tab_size = 2;
						Redraw;
						break;
		case KEY_F(9):	Redraw;
						break;
		case KEY_F(4):	tisk_inverz(" Go to page: ");
						nastranu = (Nacti_cislo() - 1) * DELKASTRANY;
						showpage(nastranu);
						break;
		case KEY_F(1):	Help(); break;	
		case ESC:
		case KEY_F(10):	Done = TRUE; break;
		case KEY_F(7): 	FindDownString(FALSE); break;	
		case KEY_F(5): 	FindDownString(TRUE); break;	
		}
	}
}

void viewer(char *fname)
{
	strcpy(view_fname, fname);
	if ((fdd = fopen(view_fname, "r")) == NULL)
		return;
	pgbuf = -1;
	headline = newwin(1,COLS,0,0);
	phead = new_panel(headline);
	okno = newwin(POCETRADKU,COLS,1,0);
	pokno = new_panel(okno);
	keypad(okno, TRUE);
	scrollok(okno,TRUE);
	wclear(okno);
	spocitej();
	StartCh = 0;
	showpage(0);
	KeyLoop();
	fclose(fdd); fdd = NULL;

	del_panel(pokno);
	delwin(okno);

	del_panel(phead);
	delwin(headline);
}
