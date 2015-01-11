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

#define BOX_MAX_WIDTH	70

int min(int a, int b)
{
	return a < b ? a : b;
}

int max(int a, int b)
{
	return a > b ? a : b;
}

/*
   Breaks long string to several strings of max_width, divided by '\0'
   and returns the number of lines you need to display the strings.
*/
int FormatTextToBox(char *text, int max_width)
{
	int lines=1;
	int delka = strlen(text);
	char *p;		/* pointer to begin of actual line */
	char *q;		/* pointer to start of next search */
	char *rozdel;	/* pointer to last place suitable for breaking the line */
	char *konec;	/* pointer to end of the text */
	q = p = text;
	rozdel = text-1;/* pointer to last line break */
	konec = text + delka;

	if (delka > max_width) {
		while(q < konec) {		/* q was last place suitable for breaking */
			char *r = strpbrk(q, " \t/\\\n");	/* find next suitable place for the break */
			if (r == NULL)
				r = konec;		/* if there's no place then point to the end */

			if ((r-p) < max_width && *r != '\n') {		/* '\n' is always used for breaking */
				rozdel = r;		/* remember new place suitable for breaking */
				q++;
				continue;		/* search again */
			}

			if ((r-p) > max_width) {	/* too long line already? */
				if (p > rozdel)				/* bad luck - no place for the delimiter. Let's do it the strong way */
					rozdel = p + max_width;		/* we loose one character */
			}
			else
				rozdel = r;			/* break in this place */

			*rozdel = '\0';			/* BREAK */
			p = q = rozdel+1;		/* next line begins here */
			lines++;				/* increment line counter */
		}
	}
	return lines;					/* return line counter */
}

/*
   Displays a dialog box and allows user to select from one to three buttons.
   Selection can be done by arrows or TAB key or even by the first char of 
   the button label. If there's the "Cancel" button it can be selected by
   pressing the ESC key.
*/
int myMessageBox(const char *text, int type)
{
	WINDOW *pwinalert;
	PANEL *ppanalert;
	int ww, wh, radku, butposy, numbut, key, sumlen, spclen, active, i, chosen;
	char *button[3];
	int butlen[3], butpos[3], hotkey[3], ret[3];
	char *kopie_text = strdup(text), *txtptr;

	switch(type & 0x0f) {
		case myMB_OK:
			numbut = 1;
			button[0] = "[ OK ]"; hotkey[0] = 'O'; ret[0] = myIDOK;
			break;
		case myMB_OKCANCEL:
			numbut = 2;
			button[0] = "[ OK ]"; hotkey[0] = 'O'; ret[0] = myIDOK;
			button[1] = "[ Cancel ]"; hotkey[1] = 'C'; ret[1] = myIDCANCEL;
			break;
		case myMB_YESNO:
			numbut = 2;
			button[0] = "[ Yes ]"; hotkey[0] = 'Y'; ret[0] = myIDYES;
			button[1] = "[ No ]"; hotkey[1] = 'N'; ret[1] = myIDNO;
			break;
		case myMB_YESNOCANCEL:
			numbut = 3;
			button[0] = "[ Yes ]"; hotkey[0] = 'Y'; ret[0] = myIDYES;
			button[1] = "[ No ]"; hotkey[1] = 'N'; ret[1] = myIDNO;
			button[2] = "[ Cancel ]"; hotkey[2] = 'C'; ret[2] = myIDCANCEL;
			break;
		case myMB_RETRYCANCEL:
			numbut = 2;
			button[0] = "[ Retry ]"; hotkey[0] = 'R'; ret[0] = myIDRETRY;
			button[1] = "[ Cancel ]"; hotkey[1] = 'C'; ret[1] = myIDCANCEL;
			break;
		case myMB_ABORTRETRYIGNORE:
			numbut = 3;
			button[0] = "[ Abort ]"; hotkey[0] = 'A'; ret[0] = myIDABORT;
			button[1] = "[ Retry ]"; hotkey[1]= 'R'; ret[1] = myIDRETRY;
			button[2] = "[ Ignore ]"; hotkey[2] = 'I'; ret[2] = myIDIGNORE;
			break;
		default:
			return 0;
	}

	sumlen = 0;
	for(i=0;i<numbut;i++) {
		butlen[i] = strlen(button[i]);
		sumlen += butlen[i];
	}

	ww = max(strlen(text)+4, sumlen+2*(numbut+1));	/* if the description text is short the dialog box will be smaller */
	ww = min(ww, BOX_MAX_WIDTH);					/* if the desc. text is too long the box will be BOX_MAX_WIDTH wide */
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
	Shows a message box and waits defined number of seconds ('timeout')
*/
void InfoBox(const char *text, int timeout, MYBOOL button)
{
	WINDOW *pwinalert;
	PANEL *ppanalert;
	int ww, wh, radku, butposx, butposy, i;
	char *kopie_text = strdup(text), *txtptr;

	ww = strlen(text)+4;	/* if the desc. text is short the box will be smaller */
	ww = min(ww, BOX_MAX_WIDTH);	/* if the text is long the box will be max BOX_MAX_WIDTH wide */
	radku = FormatTextToBox(kopie_text, ww-4);
	wh = radku + 4;

	butposx = (ww - strlen("[ OK ]"))/2;
	butposy = wh - 2;

	pwinalert = newwin(wh, ww, (vyska-wh)/2, (sirka-ww)/2);
	ppanalert = new_panel(pwinalert);
	box(pwinalert, ACS_VLINE, ACS_HLINE);

	/* displays the desc. text */
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
		int ret;
		/* shows the "OK" button and waits for the timeout */
		keypad(pwinalert,TRUE);
		wattron(pwinalert, A_REVERSE);
		mvwaddstr(pwinalert,butposy,butposx,"[ OK ]");
		update_panels();
		doupdate();
		ret = wgetch(pwinalert);
		ret = ret; // UNUSED
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
  Shows a dialog with one editable field. If 'return_str' contains a text
  the text is displayed and can be further edited.
  By pressing the ESC, Undo, F9 or F10 keys the function returns FALSE and
  the 'return_str' is not changed. By pressing Return or Enter keys
  the function returns TRUE and 'return_str' contains the new string.
*/
#define display_row(x)															\
{																				\
	char tmp[BOX_MAX_WIDTH];													\
	char *curptr = kopie_string + pozice_zacatku + x;							\
	int maxlen = sirka_pole - x;												\
	int curlen = min(strlen(curptr), maxlen);									\
	strncpy(tmp, curptr, curlen);												\
	memset(tmp + curlen, ' ', maxlen - curlen);	/* right pad with spaces */		\
	tmp[maxlen] = 0;															\
	mvwaddstr(w, xrow, xcol + x, tmp);											\
}
#define RELCURPOS	(cursor_pos - pozice_zacatku)
#define REDRAWROW	display_row(0)	/* redraw whole row */
MYBOOL EditBox(const char *title, const char *text, char *return_str, int maxlen)
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

	/* point cursor right after the end of text */
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

		/* show cursor at the right location */
		wmove(w, xrow, xcol + RELCURPOS);
		update_panels();
		doupdate();
		switch(key = wgetch(w)) {
			case KEY_HELP:
			case KEY_F(1):
				myMessageBox("Help for ParShell EditBox:\n\nNavigation - arrow keys Right and Left, Home and End\nBackspace and Delete keys erase characters\nCtrl-X or Ctrl-Y keypress erases whole line\nFunction keys F9, F10, Undo or Escape close the EditBox\nReturn or Enter keys end the editting box and send the entered string to ParShell", myMB_OK);
				break;
			case KEY_F(9):
			case KEY_F(10):
			case KEY_UNDO:
			case 27:	/* Escape */
				cursor_pos = -1;		/* editing canceled */
				break;

			case '\r':
			case '\n':
#ifdef __PDCURSES__
			case PADENTER:
#endif
				cursor_pos = maxlen;	/* editing confirmed */
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
					break;			/* Control characters are neither permitted */

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
		strcpy(return_str, kopie_string);	/* copy the resulting string to the 'return_str' */
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
			break;
	}
	return TRUE;
}
