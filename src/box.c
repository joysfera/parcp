/*
 * PARallel CoPy - written for transferring large files between any two machines
 * with parallel ports.
 *
 * Petr Stehlik (c) 1996-1999
 *
 */

#include "element.h"
#include "shell.h"
#include "box.h"
#include <curses.h>
#include <panel.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define BOX_MAX_WIDTH	60

#ifndef DJGPP
int min(int a, int b)
{
	return a < b ? a : b;
}

int max(int a, int b)
{
	return a > b ? a : b;
}
#endif

/*
   Rozdeli jeden retezec na sadu retezcu oddelenych '\0' a vrati pocet radku
   potrebnych pro zobrazeni daneho textu.
*/
int FormatTextToBox(char *text, int max_width)
{
	int lines=1;
	int delka = strlen(text);
	char *p;		/* ukazuje na zacatek aktualniho radku */
	char *q;		/* ukazuje na aktualni misto odkud se hleda */
	char *rozdel;	/* ukazuje na posledni vhodne misto k rozdeleni */
	char *konec;	/* ukazuje na konec textu */
	q = p = text;
	rozdel = text-1;/* ukazuje na minuly rozdelovac */
	konec = text + delka;

	if (delka > max_width) {
		while(q < konec) {		/* q bylo posledni vhodne misto pro rozdeleni */
			char *r = strpbrk(q, " \t/\\\n");	/* najdi dalsi vhodne misto pro rozdeleni */
			if (r == NULL)
				r = konec;		/* kdyz uz neni kde rozdelit, ukazu na konec */

#if 1
			if ((r-p) < max_width && *r != '\n') {		/* '\n' je vzdy rozdelen! */
				rozdel = r;		/* pamatovat si nove vhodne misto pro rozdeleni */
				q++;
				continue;		/* hledat dal */
			}

			if ((r-p) > max_width) {	/* uz moc dlouhy radek? */
				if (p > rozdel)				/* je to spatne, neni kde rozdelit. Udelame to natvrdo */ 
					rozdel = p + max_width;		/* ztratime jeden znak */
			}
			else
				rozdel = r;			/* rozdel v prave navrzenem miste */
#else	/* jiny algoritmus - mozna prehlednejsi, ale nefunguje pro max_width=40 */
			if (*r == '\n')
				rozdel = r;			/* '\n' ma nejvyssi prioritu pro rozdeleni */
			else {
				if ((r-p) < max_width) {
					rozdel = r;
					q++;
					continue;
				}
				if (p > rozdel)
					rozdel = p + max_width;
			}
#endif

			*rozdel = '\0';			/* rozdelit */
			p = q = rozdel+1;		/* dalsi radek zacina zde */
			lines++;				/* pripocitat radky */
		}
	}
	return lines;
}

/*
   Zobrazi dialog a dovoli vybrat z jednoho az tri buttonu. Da se volit stiskem
   sipek, TABem a taky pocatecnim pismenem daneho buttonu. Pokud je Cancel button,
   da se zvolit stiskem klavesy Escape.
*/
int MessageBox(const char *text, int type)
{
	WINDOW *pwinalert;
	PANEL *ppanalert;
	int ww, wh, radku, butposy, numbut, key, sumlen, spclen, active, i, chosen;
	char *button[3];
	int butlen[3], butpos[3], hotkey[3], ret[3];
	char *kopie_text = strdup(text), *txtptr;

	switch(type & 0x0f) {
		case MB_OK:
			numbut = 1;
			button[0] = "[ OK ]"; hotkey[0] = 'O'; ret[0] = IDOK;
			break;
		case MB_OKCANCEL:
			numbut = 2;
			button[0] = "[ OK ]"; hotkey[0] = 'O'; ret[0] = IDOK;
			button[1] = "[ Cancel ]"; hotkey[1] = 'C'; ret[1] = IDCANCEL;
			break;
		case MB_YESNO:
			numbut = 2;
			button[0] = "[ Yes ]"; hotkey[0] = 'Y'; ret[0] = IDYES;
			button[1] = "[ No ]"; hotkey[1] = 'N'; ret[1] = IDNO;
			break;
		case MB_YESNOCANCEL:
			numbut = 3;
			button[0] = "[ Yes ]"; hotkey[0] = 'Y'; ret[0] = IDYES;
			button[1] = "[ No ]"; hotkey[1] = 'N'; ret[1] = IDNO;
			button[2] = "[ Cancel ]"; hotkey[2] = 'C'; ret[2] = IDCANCEL;
			break;
		case MB_RETRYCANCEL:
			numbut = 2;
			button[0] = "[ Retry ]"; hotkey[0] = 'R'; ret[0] = IDRETRY;
			button[1] = "[ Cancel ]"; hotkey[1] = 'C'; ret[1] = IDCANCEL;
			break;
		case MB_ABORTRETRYIGNORE:
			numbut = 3;
			button[0] = "[ Abort ]"; hotkey[0] = 'A'; ret[0] = IDABORT;
			button[1] = "[ Retry ]"; hotkey[1]= 'R'; ret[1] = IDRETRY;
			button[2] = "[ Ignore ]"; hotkey[2] = 'I'; ret[2] = IDIGNORE;
			break;
		default:
			return 0;
	}

	sumlen = 0;
	for(i=0;i<numbut;i++) {
		butlen[i] = strlen(button[i]);
		sumlen += butlen[i];
	}

	ww = max(strlen(text)+4, sumlen+2*(numbut+1));	/* pokud je text kratky, bude mit Box na miru */
	ww = min(ww, BOX_MAX_WIDTH);					/* pokud je text dlouhy, Box bude max. 60 znaku */
	radku = FormatTextToBox(kopie_text, ww-4);
	wh = radku + 4;
	butposy = wh - 2;

	butpos[0] = spclen = (ww-sumlen) / (numbut+1);
	for(i=1;i<numbut;i++)
		butpos[i] = butpos[i-1] + butlen[i-1] + spclen;

	pwinalert = newwin(wh, ww, (vyska-wh)/2, (sirka-ww)/2);
	keypad(pwinalert,TRUE);
	ppanalert = new_panel(pwinalert);
	box(pwinalert, ACS_VLINE, ACS_HLINE);

	/* vypis text */
	txtptr = kopie_text;
	for(i=0;i<radku;i++) {
		mvwaddstr(pwinalert,1+i,2,txtptr);
		txtptr += strlen(txtptr)+1;
	}
	free(kopie_text);

	active = (type >> 4);
	if (active < 0 || active >= numbut)
		active = 0;

	chosen = -1;
	/* interakce */
	while(chosen == -1) {
		for(i=0;i<numbut;i++) {
			if (active == i)
				wattron(pwinalert, A_REVERSE);
			else
				wattroff(pwinalert, A_REVERSE);
			mvwaddstr(pwinalert,butposy,butpos[i],button[i]);
		}
		update_panels();
		doupdate();
		key = wgetch(pwinalert);
		if (key == KEY_RIGHT || key == 9)	/* TAB */
			active = ++active > (numbut-1) ? 0 : active;
		if (key == KEY_LEFT)
			active = --active < 0 ? (numbut-1) : active;
		if (key == 27)	/* ESC */
			key = 'C';	/* Cancel */
		for(i=0;i<numbut;i++) {
			if (toupper(key) == hotkey[i]) {
				chosen = i;
				break;
			}
		}
#ifdef __PDCURSES__
		if (key == PADENTER)
			key = '\n';
#endif
		if (key == '\n' || key == '\r')
			chosen = active;
	}
	del_panel(ppanalert);
	delwin(pwinalert);
	update_panels();
	return ret[chosen];
}

/*
	Zobrazi dialog a ceka urcity pocet sekund
*/
void InfoBox(const char *text, int timeout, BOOLEAN button)
{
	WINDOW *pwinalert;
	PANEL *ppanalert;
	int ww, wh, radku, butposx, butposy, key, i;
	char *kopie_text = strdup(text), *txtptr;

	ww = strlen(text)+4;	/* pokud je text kratky, bude mit Box na miru */
	ww = min(ww, BOX_MAX_WIDTH);					/* pokud je text dlouhy, Box bude max. 60 znaku */
	radku = FormatTextToBox(kopie_text, ww-4);
	wh = radku + 4;

	butposx = (ww - strlen("[ OK ]"))/2;
	butposy = wh - 2;

	pwinalert = newwin(wh, ww, (vyska-wh)/2, (sirka-ww)/2);
	ppanalert = new_panel(pwinalert);
	box(pwinalert, ACS_VLINE, ACS_HLINE);

	/* vypis text */
	txtptr = kopie_text;
	for(i=0;i<radku;i++) {
		mvwaddstr(pwinalert,1+i,2,txtptr);
		txtptr += strlen(txtptr)+1;
	}
	free(kopie_text);
	update_panels();
	doupdate();

	sleep(timeout);

	if (button) {
		/* button OK a cekani na klavesu */
		keypad(pwinalert,TRUE);
		wattron(pwinalert, A_REVERSE);
		mvwaddstr(pwinalert,butposy,butposx,"[ OK ]");
		update_panels();
		doupdate();
		key = wgetch(pwinalert);
	}

	del_panel(ppanalert);
	delwin(pwinalert);
	update_panels();
}

void display_N_spaces(WINDOW *w, int row, int col, int N)
{
	int i;
	wmove(w, row, col);
	for(i=0; i<N; i++)
		waddch(w, ' ');
}

/*
  Zobrazi dialog s jednim vstupnim editacnim polem. Pokud 'return_str' jiz
  obsahuje text, tento bude zobrazen pro dalsi editaci.
  Pri stisku Esc, Undo, F9 nebo F10 je vracena hodnota FALSE a return_str je
  nezmenen. Pokud je stisknut Enter nebo Return, vraceno je TRUE a return_str
  obsahuje novy retezec.
*/
#define display_row(x)															\
{																				\
	char tmp[BOX_MAX_WIDTH];													\
	char *curptr = kopie_string + pozice_zacatku + x;							\
	int maxlen = sirka_pole - x;												\
	int curlen = min(strlen(curptr), maxlen);									\
	strncpy(tmp, curptr, curlen);												\
	memset(tmp + curlen, ' ', maxlen - curlen);	/* doplnit mezerami zprava */	\
	tmp[maxlen] = 0;															\
	mvwaddstr(w, xrow, xcol + x, tmp);											\
}
#define RELCURPOS	(cursor_pos - pozice_zacatku)
#define REDRAWROW	display_row(0)	/* redraw whole row */
BOOLEAN EditBox(const char *title, const char *text, char *return_str, int maxlen)
{
    WINDOW	*w;
	PANEL	*p;
	int erows, ecols, xrow, xcol, key, cursor_pos, radku, i;
	char *kopie_text, *kopie_string, *txtptr;
	int sirka_pole, pozice_zacatku;

	kopie_text = strdup(text);

	if ((kopie_string = (char *)malloc(maxlen)) == NULL) {
		free(kopie_text);
		return FALSE;
	}

	strcpy(kopie_string, return_str);

	ecols = max(strlen(text),strlen(return_str));
	ecols = min(ecols+4, BOX_MAX_WIDTH);

	sirka_pole = ecols-4;

	radku = FormatTextToBox(kopie_text, sirka_pole);
	erows = radku+4;

	w = newwin(erows, ecols, (LINES-erows)/2, (COLS-ecols)/2);
   	p = new_panel(w);
	box(w, 0, 0);
	if (title && *title) {
		wattron(w, A_REVERSE);
		mvwaddstr(w, 0, 2, title);
		wattroff(w, A_REVERSE);
	}
	keypad(w, TRUE);

	/* vypis text */
	txtptr = kopie_text;
	for(i=0;i<radku;i++) {
		mvwaddstr(w,1+i,2,txtptr);
		txtptr += strlen(txtptr)+1;
	}
	free(kopie_text);

	xrow = erows - 2;
	xcol = 2;
    wattron(w, A_REVERSE);
    pozice_zacatku = 0;
    if (*kopie_string) {
    	int kopie_len = strlen(kopie_string);
    	if (kopie_len > sirka_pole)
    		pozice_zacatku = kopie_len - sirka_pole + 1;
    }
	REDRAWROW;

	/* narichtuj kurzor prave za konec textu */
	cursor_pos = strlen(kopie_string);

	update_panels();
	doupdate();

    curs_set(original_cursor);
	while(cursor_pos >= 0 && cursor_pos < maxlen) {
		/* scroll with the text in horizontal direction */
		if (RELCURPOS >= sirka_pole) {
			pozice_zacatku = cursor_pos - sirka_pole + 1;
			REDRAWROW;
		}
		else if (RELCURPOS <= 0) {
			pozice_zacatku = cursor_pos - 1;
			if (pozice_zacatku < 0)
				pozice_zacatku = 0;
			REDRAWROW;
		}

		/* zobraz kurzor na spravne pozici */
		wmove(w, xrow, xcol + RELCURPOS);
		update_panels();
		doupdate();
		switch(key = wgetch(w)) {
			case KEY_HELP:
			case KEY_F(1):
				MessageBox("Help for ParShell EditBox:\n\nNavigation - arrow keys Right and Left, Home and End\nBackspace and Delete keys erase characters\nCtrl-X or Ctrl-Y keypress erases whole line\nFunction keys F9, F10, Undo or Escape close the EditBox\nReturn or Enter keys end the editting box and send the entered string to ParShell", MB_OK);
				break;
			case KEY_F(9):
			case KEY_F(10):
			case KEY_UNDO:
			case 27:	/* Escape */
				cursor_pos = -1;		/* neuspesny konec editace */
				break;

			case '\r':
			case '\n':
#ifdef __PDCURSES__
			case PADENTER:
#endif
				cursor_pos = maxlen;	/* uspesny konec editace */
				break;

			case KEY_BACKSPACE:
#ifdef __PDCURSES__
			case 8:
#endif
				if (cursor_pos == 0)
					break;
				cursor_pos--;
				/* fall through */

			case KEY_DC:
				memmove(kopie_string+cursor_pos, kopie_string+cursor_pos+1, maxlen-cursor_pos-1);
				display_row(RELCURPOS);
				break;

			case 'X'-64:	/* Control-X */
			case 'Y'-64:	/* Control-Y */
				memset(kopie_string, 0, maxlen);
				cursor_pos = 0;
				display_row(RELCURPOS);
				break;

			case KEY_LEFT:
				if (cursor_pos > 0)
					cursor_pos--;
				break;

			case KEY_HOME:
				cursor_pos = 0;
				break;

			case KEY_RIGHT:
				if (cursor_pos < strlen(kopie_string))
					cursor_pos++;
				break;

			case KEY_END:
				cursor_pos = strlen(kopie_string);
				break;

			default:
#ifdef __PDCURSES__
				if (key >= KEY_MIN)
#else
				if (key & KEY_CODE_YES)
#endif
					break;			/* it's a special key */
				if (key < ' ')
					break;			/* Control characters are not permitted as well */

				memmove(kopie_string+cursor_pos+1, kopie_string+cursor_pos, maxlen-cursor_pos-1);
				kopie_string[cursor_pos] = key;
				display_row(RELCURPOS);
				cursor_pos++;
		}
	}

    curs_set(0);

    del_panel(p);
    delwin(w);
    update_panels();

	if (cursor_pos >= 0)
		strcpy(return_str, kopie_string);	/* editace platna, zkopiruj retezec do vysledku */
	free(kopie_string);
    return (cursor_pos >= 0);
}

int EditNumber(const char *title, const char *text, TAG_TYPE tag, void *storage)
{
	char number_buf[32];

	switch(tag) {
		case Byte_Tag:
			sprintf(number_buf, "%d", *(char *)storage); break;

		case Word_Tag:
			sprintf(number_buf, "%d", *(short *)storage); break;
			
		case Long_Tag:
			sprintf(number_buf, "%ld", *(long *)storage); break;

		default:
			*number_buf = 0;
	}

	if (! EditBox(title, text, number_buf, sizeof(number_buf)))
		return FALSE;

	switch(tag) {
		case Byte_Tag:
			{
				int temp;
				sscanf(number_buf, "%d", &temp);
				*(char *)storage = temp;
			}
			break;

		case Word_Tag:
			sscanf(number_buf, "%hd", (short *)storage); break;

		case Long_Tag:
			sscanf(number_buf, "%ld", (long *)storage); break;

		default:
	}
	return TRUE;
}
