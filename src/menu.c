#include "shell.h"
#include "menu.h"
#include <panel.h>
#include <string.h>
#include <stdlib.h>

#define  WINWIDTH		(TITWIDTH+2)	/* width of inner window */
#define  BORWIDTH		(WINWIDTH+2)	/* window border width */

#define  MENU_MAXCOLUMNS	sirka	/* defined in shell.h */
#define  MENU_MAXROWS		24 /*vyska*/	/* defined in shell.h */

WINDOW *st_text;
PANEL *pst_text;
BOOLEAN end_menu;

TMENU *new_item(BYTE * title, BYTE * stat_text, int command, int parent)
{
	TMENU *pmenu;
	pmenu = (TMENU *) malloc(sizeof(TMENU));

	if (title) {
		strncpy(pmenu->title, title, TITWIDTH);
		pmenu->title[TITWIDTH] = '\0';
		pmenu->flag = MENU_PASSED;
	} else {
		pmenu->flag = MENU_DISABLED;
	}

	if (stat_text)
		strcpy(pmenu->stat_text, stat_text);
	else
		*pmenu->stat_text = 0;

	pmenu->command = command;
	pmenu->parent = parent;
	return pmenu;
}

TMENU *new_menu(TMENU ** itemlist)
{
	TMENU *item, *next;
	int i, j, rodic = -2;

	for (i = 0; (item = itemlist[i]) != NULL; i++) {
		item->wnew = NULL;
		if ((j = item->parent) >= 0) {
			if (j == rodic)
				continue;
			rodic = j;
			itemlist[rodic]->wnew = item;
			itemlist[rodic]->flag |= MENU_SUBTREE;
		}
	}
	next = NULL;
	rodic = -2;
	while (i--) {				/* backward */
		item = itemlist[i];
		j = item->parent;		/* parent number */
		if (j != rodic) {		/* differs from the last one? */
			item->next = NULL;	/* end of the new submenu */
			rodic = j;			/* remember the parent of this submenu */
		} else
			item->next = next;
		next = item;
	}

	return itemlist[0];
}

void free_menu(TMENU * menu)
{
	TMENU *next;

	while (menu) {
		next = menu->next;
		free(menu);
		menu = next;
	}
}

void show_items(WINDOW * win, TMENU * smenu, int poloha)
{
	TMENU *act;
	BYTE tmpspc[TITWIDTH + 3];
	int i;

	act = smenu;
	i = 0;
	while (act != NULL) {
		if (poloha != i)
			wattron(win, A_REVERSE);
		else
			wattroff(win, A_REVERSE);

		if (!(act->flag & MENU_DISABLED)) {
			BOOLEAN odymil = FALSE;

			memset(tmpspc, ' ', WINWIDTH);
			if (!(act->flag & MENU_PASSED)) {
				if (has_dim) {
					wattron(win, A_DIM);
					odymil = TRUE;
				} else
					memset(tmpspc, '-', WINWIDTH);
			}

			strncpy(tmpspc + 1, act->title, strlen(act->title));

			if (act->flag & MENU_SUBTREE)
				tmpspc[TITWIDTH + 1] = '>';

			tmpspc[WINWIDTH] = 0;
			mvwaddstr(win, i, 0, tmpspc);

			if (act->flag & MENU_CHECKED)
				mvwaddch(win, i, 0, '+');

			if (odymil)
				wattroff(win, A_DIM);
		} else {
			wmove(win, i, 0);
			whline(win, ACS_HLINE, WINWIDTH);
		}

		act = act->next;
		i++;
	}
}

TMENU * find_prev_item(TMENU *first, TMENU *act)
{
	TMENU *item = first;
	while(item != NULL && item->next != act)
		item = item->next;

	return item;
}

int find_menu_prev(TMENU * first, int p, int pocet)
{
	int i, j;
	TMENU *act;

	i = p;
	do {
		if (i == 0)
			i = pocet;
		i--;
		act = first;
		for (j = 0; j < i; j++)
			act = act->next;
	} while (!(act->flag & MENU_PASSED));
	return i;
}

int find_menu_next(TMENU * first, int p, int pocet)
{
	int i, j;
	TMENU *act;

	i = p;
	do {
		i++;
		if (i >= pocet)
			i = 0;
		act = first;
		for (j = 0; j < i; j++)
			act = act->next;
	} while (!(act->flag & MENU_PASSED));
	return i;
}

void toggle_command(TMENU * first, int cm, int set)
{
	TMENU *act;

	act = first;
	while (act != NULL) {
		if (act->command == cm && !(act->flag & MENU_DISABLED)) {
			if (set)
				act->flag |= MENU_PASSED;
			else
				act->flag &= ~MENU_PASSED;
			break;
		} else if (act->flag & MENU_SUBTREE)
			toggle_command(act->wnew, cm, set);
		act = act->next;
	}
}

void toggle_icheck(TMENU * first, int cm, int set)
{
	TMENU *act;

	act = first;
	while (act != NULL) {
		if (act->command == cm) {
			act->flag |= MENU_CHECKABLE;
			if (set)
				act->flag |= MENU_CHECKED;
			else
				act->flag &= ~MENU_CHECKED;
			break;
		} else if (act->flag & MENU_SUBTREE)
			toggle_icheck(act->wnew, cm, set);
		act = act->next;
	}
}

void click_radio(TMENU * first, int cm)
{
	TMENU *act;

	act = first;
	while (act != NULL) {
		if (act->command == cm) {
			TMENU *item;
			act->flag |= MENU_RADIO;
			// nastav vsem predchazejicim itemum flag RADIO
			item = act;
			while((item = find_prev_item(first, item))) {
				if (item->flag & (MENU_SUBTREE | MENU_DISABLED))
					break;
				else {
					item->flag |= MENU_RADIO;
					item->flag &= ~MENU_CHECKED;
				}
			}
			// nastav vsem nasledujicim itemum flag RADIO
			item = act;
			while((item = item->next)) {
				if (item->flag & (MENU_SUBTREE | MENU_DISABLED))
					break;
				else {
					item->flag |= MENU_RADIO;
					item->flag &= ~MENU_CHECKED;
				}
			}

			// nastav samotny button
			act->flag |= MENU_CHECKED;
			break;
		} else if (act->flag & MENU_SUBTREE)
			click_radio(act->wnew, cm);
		act = act->next;
	}
}

BOOLEAN is_checked(TMENU * first, int cm)
{
	TMENU *act;
	int vysledek = -1;

	act = first;
	while (act != NULL && vysledek == -1) {
		if (act->command == cm) {
			vysledek = (act->flag & MENU_CHECKED) ? TRUE : FALSE;	/* found! */
			break;
		} else if (act->flag & MENU_SUBTREE)
			vysledek = is_checked(act->wnew, cm);
		act = act->next;
	}
	return vysledek;
}

int show_menu(TMENU * imenu, int cx, int cy)
{
	int i, sy, j;
	WINDOW *wwborder, *wwmenu;
	PANEL *pwwborder;
	int klavesa = 0;
	int comm;
	TMENU *act;

	sy = 0;
	act = imenu;
	end_menu = 0;
	while (act != NULL) {
		act = act->next;
		sy++;
	}

	if ((cy + sy) > MENU_MAXROWS - 2) {
		cy = MENU_MAXROWS - 2 - sy;
		if (cy < 0)
			return 0;			/* do not open this menu */
	}
	if ((cx + BORWIDTH) > MENU_MAXCOLUMNS - 1) {
		cx = MENU_MAXCOLUMNS - 1 - BORWIDTH;
		if (cx < 0)
			return 0;			/* do not open this menu */
	}

	wwborder = newwin(sy + 2, BORWIDTH, cy, cx);
	wattron(wwborder, A_REVERSE);
	box(wwborder, ACS_VLINE, ACS_HLINE);

#if NAPOVEDA
	st_text = newwin(1, 79, 0, 0);
	pst_text = new_panel(st_text);
	wattron(st_text, A_REVERSE);
	wclear(st_text);
#endif

	wwmenu = derwin(wwborder, sy, WINWIDTH, 1, 1);
	wattron(wwmenu, A_REVERSE);
	klavesa = 0;
	comm = 0;
	keypad(wwmenu, TRUE);
	// refresh();
	pwwborder = new_panel(wwborder);
	update_panels();
	i = find_menu_next(imenu, sy, sy);

	while ((klavesa != KEY_LEFT) && (end_menu == 0)) {
		act = imenu;
		for (j = 0; j < i; j++)
			act = act->next;
		show_items(wwmenu, imenu, i);
		update_panels();
		doupdate();
		klavesa = wgetch(wwmenu);
		switch (klavesa) {
		case '\r':
		case '\n':
			if (act->flag & MENU_SUBTREE)
				comm = show_menu(act->wnew, cx + BORWIDTH, cy + i);
			else if (act->flag & MENU_RADIO) {
				if (!(act->flag & MENU_CHECKED)) {
					TMENU *item = act;
					/* unselect all previous items */
					while((item = find_prev_item(imenu, item))) {
						if (item->flag & MENU_RADIO)
							item->flag &= ~MENU_CHECKED;
						else
							break;
					}
					/* unselect all following items */
					item = act;
					while((item = item->next)) {
						if (item->flag & MENU_RADIO)
							item->flag &= ~MENU_CHECKED;
						else
							break;
					}
					/* select the current one */
					act->flag |= MENU_CHECKED;
				}
			}
			else if (act->flag & MENU_CHECKABLE)
				act->flag ^= MENU_CHECKED;
			else {
				comm = act->command;
				end_menu = 1;
			}
			break;

		case 32:
			if (act->flag & MENU_RADIO) {
				if (!(act->flag & MENU_CHECKED)) {
					TMENU *item = act;
					/* unselect all previous items */
					while((item = find_prev_item(imenu, item))) {
						if (item->flag & MENU_RADIO)
							item->flag &= ~MENU_CHECKED;
						else
							break;
					}
					/* unselect all following items */
					item = act;
					while((item = item->next)) {
						if (item->flag & MENU_RADIO)
							item->flag &= ~MENU_CHECKED;
						else
							break;
					}
					/* select the current one */
					act->flag |= MENU_CHECKED;
				}
			}
			else if (act->flag & MENU_CHECKABLE)
				act->flag ^= MENU_CHECKED;
			break;

		case KEY_F(9):
		case KEY_F(10):
		case KEY_UNDO:
		case 27:				/* Escape */
			comm = 0;
			end_menu = 1;
			break;
		case KEY_UP:
			i = find_menu_prev(imenu, i, sy);
			break;
		case KEY_DOWN:
			i = find_menu_next(imenu, i, sy);
			break;
		case KEY_RIGHT:
			if (act->flag & MENU_SUBTREE)
				comm = show_menu(act->wnew, cx + BORWIDTH, cy + i);
			break;
		}
	}

	delwin(wwmenu);
#if NAPOVEDA
	del_panel(pst_text);
	delwin(st_text);
#endif
	del_panel(pwwborder);
	delwin(wwborder);

	update_panels();
	return comm;
}
