/*
 * PARCP IN/OUT - written for setting the direction of parallel port.
 *
 * Petr Stehlik (c) 1996,1997
 *
 * You need a special HW interface if your PC offers unidirectional parallel
 * port only.
 */

#include "parcp.h"
#include "parcplow.h"

#ifdef IBM
#include "global.h"
#include "parstruc.h"
int io_config_file(char *soubor)
{
	return input_config(soubor,configs,CFGHEAD);
}

#include "parcommo.c"

void zpracovani_parametru(int argc, char *argv[])
{
	char cesta[MAXPATH];
	extern char *optarg;
	BOOLEAN konfigOK = FALSE;

/* find the config file */
	konfigOK = hledej_config(argv, cesta);

/* check if there isn't config file path on the command line */
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

int main(int argc, char *argv[])
{
#ifdef IN
#define TXT "IN"
#else
#define TXT "OUT"
#endif

	puts("\nPARCP "TXT" - written by Petr Stehlik (c) 1996,1997.\n"\
		 "Version "VERZE" (compiled on "__DATE__")\n");

#ifdef IBM

	zpracovani_parametru(argc, argv);

/* in Linux get the permission to access the hardware of parallel port directly */
#ifndef __MSDOS__
	ioperm(print_port,4,1);
#endif

	if (port_type == 2 && !PCunidirect)
		ECP2EPP;
#endif	/* IBM */

#ifdef IN
	SET_INPUT;
	printf("Parallel port is set to input (safe) state.\n");
#else
	SET_OUTPUT;
#ifdef IBM
	if (port_type == 2 && !PCunidirect)
		EPP2ECP;
#endif	/* IBM */
	printf("Parallel port is set to output (standard) state.\n");
#endif
	STROBE_HIGH;

	return 0;
}
