/* Global variables for PARCP */
#include "parcp.h"

BOOLEAN registered = FALSE;			/* program se chova jako registrovany / shareware */
char cfg_fname[MAXPATH];			/* cesta k aktualnimu konfiguracnimu souboru */

char local_machine[MAXSTRING];
char remote_machine[MAXSTRING];

BOOLEAN hash_mark = TRUE;
BOOLEAN _case_sensitive = TRUE;			/* pri match() jmen brat v potaz i velikost pismen v nazvech souboru */
BOOLEAN _preserve_case = TRUE;			/* v opendir() nekonvertovat DOSove nazvy na mala pismena */
BOOLEAN _show_hidden = FALSE;			/* DOS: zobrazovat skryte soubory */
char _over_older = 'R';					/* prepisovat starsi soubory bez ptani */
char _over_newer = 'A';					/* prepisovat novejsi soubory pouze po dotazu */
BOOLEAN _send_subdir = TRUE;			/* posilat i adresare se vsemi jejich soubory */
BOOLEAN _keep_timestamp = TRUE;			/* pri kopirovani zachovavat puvodni datum a cas souboru */
BOOLEAN _keep_attribs = FALSE;			/* pri kopirovani zachovavat puvodni pristupova prava a atributy */
BOOLEAN _archive_mode = FALSE;			/* kopirovat pouze soubory bez atributu ARCHIVE a ten jim pak nastavovat */
BOOLEAN _check_info = TRUE;				/* predem spocitat pocet souboru a adresaru pri copy/delete */
BOOLEAN _assembler = TRUE;				/* pouzivat rychle prenosove rutiny */
BOOLEAN afterdrop = TRUE;				/* Server se ukonci po drag&dropu */
BOOLEAN _checksum = FALSE;				/* kontrolni soucet pri prenosu bloku */
char _sort_jak = 'U';					/* tridit podle niceho */
BOOLEAN _sort_case = FALSE;				/* tridit velka/mala pismena jako odlisna */
UWORD dirbuf_lines = UNREG_DIRBUF_LIN;	/* vyhrazeny pocet radku o delce DIRLINELEN */
long buffer_lenkb = BUFFER_LENKB;		/* velikost prenaseneho bloku v kilobajtech */
BYTE filebuffers = 1;					/* pocet bloku vyhrazenych pro bufrovani cteni a zapisu souboru */
int time_out = TIME_OUT;				/* timeout v sekundach */
char username[MAXSTRING]="Unregistered User";
char keycode[MAXSTRING]="ABCDEFGHIJKLMNOP";
char autoexec[MAXPATH]="";
#ifdef SHELL
BOOLEAN shell = TRUE;
#endif

#ifdef DEBUG
short debuglevel=1;
char logfile[MAXSTRING]="parcp.log";
char nolog_str[MAXSTRING]="l";
char nodisp_str[MAXSTRING]="ldsr>!";
#endif
