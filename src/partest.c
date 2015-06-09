/*
 * PARallel CoPy - written for transferring large files between any two machines
 *                 with parallel ports.
 *
 * Petr Stehlik (c) 1996-1998
 *
 */

#include "parcp.h"			/* konstanty, hlavicky funkci */
#include "parcplow.h"		/* definice ruznych nizkych zalezitosti */
#include "global.h"			/* seznam globalnich promennych */
#include "parstruc.h"		/* konfiguracni struktura */
#include "match.h"

#ifdef SHELL
#include <curses.h>
#include <panel.h>
extern MYBOOL curses_initialized;
extern int progress_width;
extern WINDOW *pwincent;
#endif

MYBOOL client = TRUE;	/* PARCP is Client by default */

BYTE *block_buffer, *dir_buffer, *file_buffer, string_buffer[MAXSTRING+1];
long buffer_len = KILO * BUFFER_LENKB;		/* velikost prenaseneho bloku */
#ifdef __MSDOS__
char original_path[MAXSTRING];
#endif

MYBOOL INT_flag = FALSE;				/* registruje stisk CTRL-C */
MYBOOL _quiet_mode = FALSE;			/* kdyz je TRUE tak se vubec nic nevypisuje (krome ERRORu) */

unsigned long g_files = 0, g_bytes = 0, g_folders = 0;
unsigned long g_files_pos, g_bytes_pos, g_time_pos;

short page_length = 25;
short page_width  = 80;

int g_last_status = 0;		/* CLI will record errors into this var */

MYBOOL bInBatchMode = FALSE;

/******************************************************************************/
void wait_for_client(void)	/* ceka a nezdrzuje zbytek pocitace */
{
	while(IS_READY) {
		usleep(WAIT4CLIENT);
		if (stop_waiting())
			errexit("Waiting for client interrupted by user.", ERROR_USERSTOP);
	}
}

void wait_before_read(void)		/* ceka pred read_neco() aby nevytimeoutoval */
{
	SET_INPUT;
	if (client) {
		/* musime shodit STROBE aby Server zacal psat */
		STROBE_LOW;
	}
	while(IS_READY) {
		if (stop_waiting())
			errexit("Waiting for other side interrupted by user.", ERROR_USERSTOP);
	}
}

#include "parcplow.c"		/* C rutiny, jejichz ekvivalenty mam i v assembleru */

void read_block(BYTE *block, long n)
{
	long status = 0;

	DPRINT2("l Read_block(%p, %ld)\n", block, n);

	if (client) {
		if (_assembler)
			status = fast_client_read_block(block, n);
		else
			status = client_read_block(block, n);
	}
	else {
		if (_assembler)
			status = fast_server_read_block(block, n);
		else
			status = server_read_block(block, n);
	}

	if (status < 0)
		errexit("Timeout. Please increase Timeout value in PARCP config file.", ERROR_TIMEOUT);

	DPRINT("l Read_block: end\n");
}

void write_block(const BYTE *block, long n)
{
	long status = 0;

	DPRINT2("l Write_block(%p, %ld)\n", block, n);

	if (client) {
		if (_assembler)
			status = fast_client_write_block(block, n);
		else
			status = client_write_block(block, n);
	}
	else {
		if (_assembler)
			status = fast_server_write_block(block, n);
		else
			status = server_write_block(block, n);
	}

	if (status < 0)
		errexit("Timeout. Please increase Timeout value in PARCP config file.", ERROR_TIMEOUT);

	DPRINT("l Write_block: end\n");
}

/******************************************************************************/
UWORD read_word(void)
{
	BYTE a[2+1];
	UWORD x;

	read_block(a,2);
	x = (a[0]<<8) | a[1];

	DPRINT2("l Read_word() -> %u=$%x\n", x, x);

	return x;
}

long read_long(void)
{
	BYTE a[4+1];
	long x;

	read_block(a,4);
	x = a[0];
	x = (x<<8) | a[1];
	x = (x<<8) | a[2];
	x = (x<<8) | a[3];

	DPRINT2("l Read_long() -> %ld=$%lx\n", x, x);

	return x;
}

void write_word(UWORD x)
{
	BYTE a[2+1];
	a[0] = x>>8;
	a[1] = x;

	DPRINT2("l Write_word(%u=$%x)\n", x, x);

	write_block(a,2);
}

void write_long(long x)
{
	BYTE a[4+1];

	a[3] = x;
	a[2] = (x = x>>8);
	a[1] = (x = x>>8);
	a[0] = x>>8;

	DPRINT2("l Write_long(%ld=$%lx)\n", x, x);

	write_block(a,4);
}

/*******************************************************************************/

void catch_ctrl_c(int x)
{
	if (! INT_flag)
		INT_flag = TRUE;		/* zaznamenat stisknuti klavesy */
	signal(SIGINT, catch_ctrl_c);
}

/* return TRUE if user holds EmergencyQuit keys */
/* EmergencyQuit keys are either Alt+Shift+Control or Control-C */
MYBOOL stop_waiting(void)
{
	MYBOOL was_break = INT_flag;

#ifdef KEYPRESSED
	if (KEYPRESSED >= 13)		/* Alt+Shift+Control */
		was_break = TRUE;
#endif

	INT_flag = FALSE;			/* v kazdem pripade vymazat flag CTRL-C */

	return was_break;
}

/* return TRUE if user holds Stop-File-Transfer keys on Client */
/* Stop-File-Transfer keys are either Shif+Control or Control-C */
MYBOOL break_file_transfer(void)
{
	MYBOOL was_break = INT_flag;

#ifdef KEYPRESSED
	if (KEYPRESSED == 6 || KEYPRESSED == 7)	/* Shift+Control */
		was_break = TRUE;
#endif

	INT_flag = FALSE;		/* clear flag CTRL-C */

	return was_break;
}

/* return TRUE if user pressed Esc key on Client */
MYBOOL zastavit_prenos_souboru(void)
{
	if (client) {
		int keys = 0;

/*
		else
			keys = fetch_key();
*/

		if (keys == 27)		/* ESC key */
			return TRUE;

#ifdef ATARI
#ifdef KEYPRESSED
		if (KEYPRESSED == 3)	/* both shifts */
			return TRUE;
#endif
#endif
	}
	return FALSE;
}

/*******************************************************************************/
void port_reset(void)
{
	SET_INPUT;
	STROBE_HIGH;
#ifdef IBM
	if (port_type == 2 && !PCunidirect)
		EPP2ECP;
#endif
}
/*******************************************************************************/

void errexit(const char *a, int error_code)
{
	DPRINT1("! Errexit: %s\n", a);
	port_reset();
	puts("");
	if (a != NULL)
		puts(a);
	else
		puts("An unknown error occured.");


/*
	puts("Press Return to exit.");
	getchar();
*/
	exit(error_code);
}
/*******************************************************************************/

char *get_cwd(char *path, int maxlen)
{
	getcwd(path, maxlen);
#ifdef ATARI
	if (! strncmp(path,TOS_DEV,Ltos_dev)) {
		DPRINT1("> Get_cwd() = '%s' but will be converted\n", path);
		memmove(path, path+Ltos_dev-1, strlen(path)-Ltos_dev+2);	/* odstranit /dev/ */
		if (path[2] == SLASH) {		/* je to /x/ */
			path[0] = path[1];
			path[1] = DVOJTEKA;	/* prevedu na x:/ */
		}
	}
#endif
	DPRINT1("> Get_cwd() = '%s'\n", path);
	return path;
}
/*******************************************************************************/

int config_file(const char *soubor, MYBOOL vytvorit)
{
	int vysledek;

	if (vytvorit) {
		buffer_lenkb = buffer_len / KILO;
		vysledek = update_config(soubor,configs,CFGHEAD);
	}
	else {
		vysledek = input_config(soubor,configs,CFGHEAD);

		if (dirbuf_lines < 1)
			dirbuf_lines = 1;

		if (time_out < 1)
			time_out = 1;

		buffer_len = buffer_lenkb * KILO;

#ifdef DEBUG
		/* pokud neni logfile s cestou, musim doplnit aktualni */
		if (!strchr(logfile, SLASH) && !strchr(logfile, BACKSLASH)) {
			char cesta[MAXPATH], *p;

			getcwd(cesta, sizeof(cesta));
			p = cesta + strlen(cesta)-1;
			if (*p != SLASH && *p != BACKSLASH)
				strcat(cesta, SLASH_STR);
			strcat(cesta, logfile);
			strcpy(logfile, cesta);
		}
#endif
	}

	return vysledek;
}
/*******************************************************************************/
void inicializace(void)
{
#ifdef __MSDOS__
	getcwd(original_path, sizeof(original_path));	/* pamatuj puvodni cestu */
#endif

#ifndef STANDALONE

#ifdef IBM
	if (! _quiet_mode) {
		printf("Parallel port base address: %x\n", print_port);
		printf("Parallel port type: ");
		switch(port_type) {
			case 0:	printf("unidirectional/ECP unable to switch to EPP\n"); break;
			case 1: printf("bidirectional/EPP\n"); break;
			case 2: printf("ECP switched to EPP\n"); break;
		}
		printf("Cable type: %s\n", cable_type ? "PARCP bidirectional" : "LapLink");
		if (cable_type) {
			if (PCunidirect)
				puts("UNI-BI HW parallel adapter has to be plugged-in.");
			else {
				if (port_type)
					puts("No HW adapter - PARCP cable is enough.");
				else
					errexit("Bidirectional routines on unidirectional port? Never!\n"\
							"Run PARCPCFG.EXE in order to correct your PARCP setup.", ERROR_BADCFG);
			}
		}
		else
			if (PCunidirect)
				errexit("UNI-BI HW parallel adapter and LapLink parallel cable? Bad idea!\n"\
						"Choose either UNI-BI adapter or LapLink cable - run PARCPCFG.EXE.", ERROR_BADCFG);
	}

#ifdef __LINUX__
	/* v Linuxu si vyhradim pravo pristupu na hardware paralelniho portu */

	/* zkusme zkontrolovat, ma-li uzivatel prava ROOTa */

	DPRINT("> Trying to get permisions to in/out the port directly\n");
	if (iopl(0)) {
		errexit("Need root privileges to drive parallel port\n", ERROR_NOTROOT);
	}
	ioperm(print_port,4,1);
#endif	/* __LINUX__ */

	if (port_type == 2 && !PCunidirect)
		ECP2EPP;

#endif	/* IBM */

#endif	/* STANDALONE */
}

/*******************************************************************************/

void client_server_handshaking(MYBOOL client)
{
	SET_INPUT;
	STROBE_HIGH;

	if (client) {
		printf("\n[Test Client] (will timeout in %d seconds)\n", time_out);

		/* protokol neni dost robustni: budu vyzadovat start Serveru pred spustenim Clienta */
		if (! IS_READY)
			errexit("Server has NOT been started yet - or a problem occured with parallel cable.\n"\
					"Check your HW and PARCP configuration, then start Server *BEFORE* Client.", ERROR_HANDSHAKE);

		usleep(2*WAIT4CLIENT);	/* podrzet STROBE HIGH na chvilku, aby nas Server odlisil od nestavu */

	}
	else {
		printf("\n[Test Server] Awaiting data patterns from Test Client (press %s to quit)\n", STOP_KEYSTROKE);

		/* pokud je BUSY LOW, druha strana je v nestavu */
		/* NESMIM zamenit s predcasne spustenym klientem! */
		while(! IS_READY) {
			usleep(WAIT4CLIENT);
			if (stop_waiting())
				errexit("Waiting for client startup interrupted by user.", ERROR_HANDSHAKE);
		}
	}
}

/*******************************************************************************/

#include "parcommo.c"

int zpracovani_parametru(int argc, char *argv[])
{
	char cesta[MAXPATH], batch_file[MAXPATH];
	int i;
	extern int optind;
	extern char *optarg;
	MYBOOL konfigOK = FALSE;

	batch_file[0] = 0;

/* nejdrive podle priorit najdu konfiguracni soubor */
	konfigOK = hledej_config(argv, cesta);

/* nyni projdu parametry prikazoveho radku, ktere tak maji vyssi prioritu */

#define ARG_OPTIONS		"sf:b:q"

	while((i=getopt(argc, argv, ARG_OPTIONS)) != EOF) {
		switch(tolower(i)) {
			case 's':
				client = FALSE;
				break;

			case 'f':
				strcpy(cesta, optarg);
				konfigOK = file_existuje(cesta);
				break;

			case 'b':
				strcpy(batch_file, optarg);
				break;

			case 'q':
				_quiet_mode = TRUE;
				break;

			case '?':
				puts(USAGE); exit(1);
		}
	}

	if (konfigOK) {
		int valid_num = 0;
		if (! _quiet_mode)
			printf("Configuration file used: %s\n", cesta);
	
		valid_num = config_file(cesta, FALSE);

		if (! _quiet_mode) {
			if (valid_num >= 0)
				printf("(valid directives found: %d)\n\n", valid_num);
			else
				printf("Error while reading/processing the config file.\n\n");
		}
	}
	else {
		/* konfiguracni soubor nenalezen! */
#ifdef ATARI
		/* na Atari ho proste vytvorim s defaultnimi hodnotami */
		if (! _quiet_mode)
			printf("Configuration file not found.\nIt's being created: %s\n\n", cesta);
		DPRINT1("! Creating configuration file: %s\n", cesta);
		config_file(cesta, TRUE);
#else
		/* na strojich, kde zalezi na adrese a typu portu, vsak musim koncit */
#ifndef STANDALONE
		printf("PARCP configuration file (%s) not found.\nPlease start PARCPCFG in order to create it\n", cesta);
		exit(ERROR_BADCFG);
#endif	/* STANDALONE */
#endif	/* ATARI */
	}

	if (*batch_file)
		strcpy(autoexec, batch_file);	/* presun parametr do struktury */

	strcpy(cfg_fname, cesta);	/* dobre si zapamatovat, ktery konfiguracni soubor pouzivame */

	return optind;		/* vracim cislo prvniho NEparametru (tj. jmena souboru) */
}

MYBOOL test(UWORD x)
{
	UWORD y;
	MYBOOL result;

	printf("Sending %04x\t", x);
	write_word(x);
	printf("Receiving ");
	y = read_word();
	printf("%04x\t", y);
	result = (x == y);
	if (result)
		printf("OK\n");
	else
		printf("ERROR!\n");

	return result;
}

int main(int argc, char *argv[])
{
#if defined(DEBUG)
	time_t start_time = time(NULL);
#endif
	int i;

	setvbuf(stdout,NULL,_IONBF,0);	/* vypnu bufrovani vystupu na obrazovku */
	signal(SIGINT, catch_ctrl_c);	/* nastavim ovladac CTRL-C */
	INT_flag = FALSE;				/* vynuluju priznak stisku CTRL-C */

	clrscr();						/* smazu obrazovku */

/*
	{
		int i;
		for(i=1; i<argc; i++)
			printf("%d: '%s'\n", i, argv[i]);
	}
*/
	/* nacti prikazy na command line i kompletni PARCP.CFG soubor */
	i = zpracovani_parametru(argc, argv);

	inicializace();

	client_server_handshaking(client);

	if (client) {
		int i;
		for(i=0; i < 16; i++) {
			if (! test(1<<i))
				g_last_status = -1;
			if (! test(0xFFFF - (1<<i)))
				g_last_status = -1;
		}
		if (! test(0xFFFF))
			g_last_status = -1;
		if (! test(0x0000))
			g_last_status = -1;
	}
	else {
		for(;;) {
			UWORD x;
			wait_for_client();
			x = read_word();
			printf("Got %04x\n", x);
			write_word(x);
		}
	}

	port_reset();

	if (g_last_status)
		printf("\nPARCP Test failed! Something is wrong.");
	else
		printf("\nPARCP Test finished OK");

	printf(" Press Return key\n");
	getchar();

	return g_last_status;
}
