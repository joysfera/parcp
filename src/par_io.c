#include "parcp.h"
#include "parcplow.h"

#ifdef IBM
#include "global.h"
#include "parstruc.h"
#include "parcommo.c"

void zpracovani_parametru(int argc, char *argv[])
{
	char cesta[MAXPATH];
	extern char *optarg;
	BOOLEAN konfigOK = FALSE;

/* nejdrive podle priorit najdu konfiguracni soubor */
	konfigOK = hledej_config(argv, cesta);

/* zkusim jeste jestli neni cesta ke konfigu na prikazove radce */
	if (argc == 2 && argv[1] != NULL) {
		strcpy(cesta, argv[1]);
		konfigOK = file_exists(cesta);
	}

	if (konfigOK) {
		printf("Configuration file used: %s\n", cesta);
		print_port = port_type = -1;
		if (input_config(cesta,configs,CFGHEAD) <= 0) {
			printf("Error while reading/processing the config file.\n\n");
			exit(-1);
		}
		if (print_port == -1 || port_type == -1) {
			puts("Invalid PARCP configuration file. Please run PARTEST for updating it.");
			exit(-1);
		}
	}
	else {
		printf("Fatal error - can't find PARCP config file.\n");
		exit(-1);
	}
	printf("Parallel port base address: $%x\n", print_port);
	printf("Parallel port type: %s\n", port_type == 0 ? "unidirectional" : "bidirectional");
	if (PCunidirect)
		printf("Using UNI-BI HW parallel adapter\n");
}
#endif	/* IBM */

void main(int argc, char *argv[])
{
#ifdef IBM

	zpracovani_parametru(argc, argv);

/* v Linuxu si vyhradim pravo pristupu na hardware paralelniho portu */
#ifndef __MSDOS__
	ioperm(print_port,4,1);
#endif

	if (port_type == 2 && !PCunidirect)
		ECP2EPP;
#endif	/* IBM */

	SET_INPUT;

	while(KEYPRESSED != 8) {
		BYTE x;

		putchar(' ');
		putchar(IS_READY ? '^' : 'v');
		GET_BYTE(x);
		printf("%02x", x);
		switch(KEYPRESSED) {
			case 4:
				STROBE_HIGH;
				break;
			case 2:
				STROBE_LOW;
				break;
		}
	}
}
