#ifndef _menu_h
#define _menu_h

#include "element.h"
#include <curses.h>

#define  MENU_PASSED	(1<<0)
#define  MENU_SUBTREE	(1<<1)
#define  MENU_DISABLED	(1<<2)
#define  MENU_CHECKED	(1<<3)
#define  MENU_CHECKABLE	(1<<4)
#define	 MENU_RADIO	(1<<5)

#define  TITWIDTH		20	/* max. delka titulku */

struct _tmenu {
	BYTE	title[TITWIDTH+1]; /* nazev polozky */
	BYTE	stat_text[70];  /* popis */
	BYTE	flag;           /* maska */
	int		command;        /* akce */
	int		parent;/* rodic */
	struct _tmenu   *next;   /* dalsi */
	struct _tmenu   *wnew;    /* novy */
};

typedef struct _tmenu TMENU;

TMENU *new_item(BYTE *title, BYTE *stat_text, int  command, int parent);
TMENU *new_menu(TMENU **itemlist);
void free_menu(TMENU *menu);
void show_items(WINDOW *win, TMENU *smenu, int poloha);
void toggle_command(TMENU *first, int cm, int set);
void toggle_icheck(TMENU *first, int cm, int set);
void click_radio(TMENU *first, int cm);
BOOLEAN is_checked(TMENU *first, int cm);
int show_menu(TMENU *imenu, int cx, int cy);

#endif /* _menu_h */
