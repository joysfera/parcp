#include <curses.h>
#include <string.h>

#define sirka	(COLS)
#define vyska	(LINES-1)

int main(int argc, char *argv[])
{
	char tmpstr[260];
	int original_cursor;
	int i;
	int posun;

	if (argc > 1)
		posun = atoi(argv[1]);

	initscr();
	noecho();
	//original_cursor = curs_set(0);
	cbreak();
	keypad(stdscr,TRUE);
	meta(stdscr, TRUE);		/* allow 8-bit */
	sprintf(tmpstr, " PARCP 3.73b by Petr Stehlik (c) 1996-2000. Registered to %s", "Petr Stehlik");
	mvaddstr(0,0,tmpstr);

	/* lista s tlacitky */
	/* strcpy(tmpstr,"  F2=CLI F3=View F4=Edit F5=Copy F6=Move F7=MkDir F8=Delete F9=Menu F10=LQuit "); */
	strcpy(tmpstr," F1=Help F2=CLI F3=View F4=Edit F5=Copy F6=Move F7=MkDir F8=Del F9=Menu F10=LQuit F20=Quit");
	i = strlen(tmpstr);
	memset(tmpstr+i, ' ', sizeof(tmpstr)-i);
	tmpstr[sirka-posun-1] = 0;
	mvaddstr(vyska,posun,tmpstr);
	//mvaddstr(vyska,8," F1=Help F2=CLI F3=View F4=Edit F5=Copy");
	refresh();
	doupdate();
	sleep(1);
	clear();
	update_panels(); //refresh();
	endwin();
}
