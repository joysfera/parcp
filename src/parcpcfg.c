/*
 * PARallel TEST - written for testing PC parallel port bidirectional capability
 *
 * (c) Petr Stehlik, 1996-2000
 */

#include "parcp.h"
#include "parcplow.h"
int getche(void);
#include "global.h"
#include "parstruc.h"
#include "parcommo.c"

void zpracovani_parametru(int argc, char *argv[], char *cesta)
{
	BOOLEAN konfigOK = FALSE;

/* nejdrive podle priorit najdu konfiguracni soubor */
	konfigOK = hledej_config(argv, cesta);

/* zkusim jeste jestli neni cesta ke konfigu na prikazove radce */
	if (argc == 2 && argv[1] != NULL) {
		strcpy(cesta, argv[1]);
		konfigOK = file_exists(cesta);
	}

	if (konfigOK) {
		printf("Reading configuration file: %s\n", cesta);
		input_config(cesta,configs,CFGHEAD);
	}
	else {	/* pokud nebyl nalezen konfiguracni soubor, tak ho zkusim vytvorit */
		printf("Configuration file not found.\nIt will be created at: %s\n", cesta);
	}
}

void EXIT(char *a)
{
	printf("\n%s", a);
	exit(1);
}

BOOLEAN is_bidir(int Port)
{
	BOOLEAN Ret = FALSE;
	BYTE j;

	for(j=127;j;j>>=1) {
		outportb(Port, j);
		if (inportb(Port) != j) {
			Ret = TRUE;
			break;
		}
	}

	return Ret;
}

int main(int argc, char *argv[])
{
	int LPTport;
	int Ctrl,Orig;
	BOOLEAN Ret;
	char cfg_file[MAXPATH];
	int ports[3];

	clrscr();
	setvbuf(stdout,NULL,_IONBF,0);

	puts("PARallel CoPy ConFiGuration - written by Petr Stehlik (c) 1996-2000.\n"\
		 "Version "VERZE" (compiled on "__DATE__")\n");

	zpracovani_parametru(argc, argv, cfg_file);

	puts("\n"\
"Welcome to PARCP configuration program!\n"\
"---------------------------------------\n"\
"Since PARCP works over parallel ports, this program will configure PARCP\n"\
"according to chosen parallel port. Please read the following instructions\n"\
"carefully and when in doubt, consult PARCP's documentation or author directly.\n");

	puts("Please disconnect all peripherals and cables from all parallel (printer) ports\n"\
		 "of your computer before you proceed in the configuration program.\n"\
		 "If everything is disconnected you might also consider rebooting to clean DOS,\n"\
		 "because various resident drivers may distort results of my HW tests.\n");

	printf("Is everything disconnected from the parallel (printer) ports? (Y/n): ");
	if (tolower(getch()) == 'n')
		EXIT("\nSo disconnect it and rerun PARCPCFG.EXE.");
	printf("\n");

	puts("\nChecking hardware of this computer...\n");

	puts("This machine is equipped with:");
	for(LPTport = 0; LPTport < 3; LPTport++) {
		int adr = ports[LPTport] = _farpeekw(_dos_ds,(LPTport)*2+0x408);
		if (adr)
			printf("%d ... parallel port LPT%d (base address: %x)\n", LPTport+1, LPTport+1, adr);
	}

	printf("\nEnter the number of parallel port you will use for PARCP: ");
	LPTport = getche() - '0';
	printf("\n");
	if (LPTport < 1 || LPTport > 3)
		EXIT("Wrong parallel port number.");

	print_port = ports[LPTport-1];

	if (! print_port)
		EXIT("No such parallel port on this machine.");

	puts("\nTesting parallel port...\n");

	Ctrl = print_port+2;
	Orig = inportb(Ctrl);

	/* Try to turn port in */
#if 1
	outportb(Ctrl,32 | Orig);
#else
	outportb(Ctrl,32+4);
#endif
	Ret = is_bidir(print_port);
	/* Turn port back out */
	outportb(Ctrl,Orig);

	if (Ret)	/* Bidir or EPP */
		port_type = 1;
	else {		/* Normal or ECP or ECP/EPP */

		/* Try to turn port to EPP mode */
		old_ecp_val = inportb(print_port + 0x402);
		outportb(print_port + 0x402,0x80);

		outportb(Ctrl,32 | Orig);
		Ret = is_bidir(print_port);
		/* Turn port back out */
		outportb(Ctrl,Orig);

		/* Turn port back to ECP */
		outportb(print_port+0x402,old_ecp_val);

		if (Ret)	/* ECP/EPP port */
			port_type = 2;
		else {
			port_type = 0;
			if ((old_ecp_val & 15) == 5) {
				puts(\
"This port is in the ECP mode and can't be switched into the EPP mode.\n"\
"PARCP cannot use this type of port for bidirectional transfers.\n"\
"However you may enter your BIOS setup and look for option of switching\n"\
"the mode of parallel port. If you find such an option, change it to ECP/EPP,\n"\
"EPP/SPP or similar and try to run this test again.\n");
			}
		}
	}

	if (port_type == 0) {
		PCunidirect = TRUE;
		puts(\
"Unidirectional port detected. That means you will have to make\n"\
"and always use the UNI-BI HW adapter (described in documentation)\n"\
"which will emulate the bidirectional capability of your parallel port.\n"\
"When you want to connect to a PC, you may also use LapLink cable.\n");

		puts("Do you want to use your UNI-BI HW adapter and PARCP cable? (Y/n): ");
		if (tolower(getch()) == 'n') {
			PCunidirect = FALSE;	/* NOTE: LapLink cable is required! */
			puts("\n"\
"OK, but now you have to use LapLink cable only and you cannot connect this\n"\
"computer to an Atari ST.\n");
		}
	}
	else {
		PCunidirect = FALSE;
		puts(\
"This parallel port seems to have bidirectional capability.\n\n"\
"For connecting this computer to Atari ST you need just PARCP cable.\n"\
"For connecting with PC you may use either PARCP or LapLink cable.\n"\
"Please note that PARCP cable gives you twice the transfer speed compared\n"\
"to LapLink cable throughput.\n");
	}

	if (PCunidirect)
		cable_type = 1; 						/* s UNI-BI musi mit PARCP kabel */
	else if (port_type == 0 && !PCunidirect)
		cable_type = 0; 						/* na blbem portu bez UNI-BI musi mit LapLink */
	else {
		puts(\
"PARCP is able to use two kinds of parallel cables for connecting two PC's:\n"\
"0 ... unidirectional LapLink cable (sold in every computer shop)\n"\
"1 ... PARCP bidirectional cable (for the fastest transfer speed)\n"\
"Note that for connecting to an Atari ST you always have to use PARCP cable (1)\n");

		printf("Choose the parallel cable you want to use with PARCP (0/1): ");
		cable_type = getche() - '0';
		printf("\n");
		if (cable_type < 0 || cable_type > 1)
			EXIT("Wrong parallel cable type.");
	}

	/* display new settings */
	puts("\n"\
"New settings are as follows:\n"\
"----------------------------");
	printf("Parallel port base address: %x\n", print_port);
	printf("Parallel port type: ");
	switch(port_type) {
		case 0: printf("unidirectional/ECP unable to switch to EPP\n"); break;
		case 1: printf("bidirectional/EPP\n"); break;
		case 2: printf("ECP switched to EPP\n"); break;
	}
	printf("Cable type: %s\n", cable_type ? "PARCP bidirectional" : "LapLink");
	if (cable_type) {
		if (PCunidirect)
			puts("UNI-BI HW parallel adapter has to be plugged into the parallel port");
		else {
			if (port_type)
				puts("Do not use UNI-BI HW parallel adapter");
			else
				EXIT("Bug in PARCPCFG? Contact author.");
		}
	}
	else {
		if (PCunidirect)
			EXIT("Bug in PARCPCFG? Contact author.");
		else
			puts("Do not use UNI-BI HW parallel adapter");
	}


	printf("\nUpdate %s with these settings? (y/N): ", cfg_file);
	if (tolower(getch()) == 'y') {
		printf("\nUpdating PARCP configuration file...");
		if (update_config(cfg_file,configs,CFGHEAD) < 0) {
			printf(" ERROR! PARCP.CFG not updated correctly!\nCheck if the file isn't write-protected and if there's enough free disk space.\n");
			return 1;
		}
		else
			printf("Done.\n");
	}
	else
		puts("\nConfiguration file unchanged. Bye!");
	return 0;
}
