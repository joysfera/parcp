/* common functions between PARCP, PARINOUT and PARTEST */

BOOLEAN file_exists(char *fname)
{
	FILE *f = fopen(fname, "r");
	if (f != NULL) {
		fclose(f);
		return TRUE;
	}
	else
		return FALSE;
}

BOOLEAN hledej_config(char *argv[], char *cesta)
{
	char *p;
	BOOLEAN konfigOK = FALSE;

	if ((p = getenv(PARCPDIR)) != NULL) {
		strcpy(cesta, p);						/* podle environment promenne */

		p = cesta + strlen(cesta)-1;
		if (*p != SLASH && *p != BACKSLASH)
			strcat(cesta, SLASH_STR);
		strcat(cesta, CFGFILE);
		konfigOK = file_exists(cesta);
		if (! konfigOK) {
			printf(PARCPDIR" is set but '%s' is not found!\n\n", cesta);
			sleep(2);
		}
	}

	if (! konfigOK && argv[0] != NULL) {
		strcpy(cesta, argv[0]);					/* v HOME adresari PARCP */
		/* odstranit jmeno programu, ale nechat lomitko */
		p = strrchr(cesta, SLASH);
		if (p == NULL)
			p = strrchr(cesta, BACKSLASH);
		/* pokud neni lomitko, nebyla cesta a tedy nelze pouzit */
		if (p != NULL) {
			*(p+1) = 0;
			strcat(cesta, CFGFILE);
			konfigOK = file_exists(cesta);
		}
	}

	if (! konfigOK) {
		getcwd(cesta, MAXPATH);							/* v aktualnim adresari */
		p = cesta + strlen(cesta)-1;
		if (*p != SLASH && *p != BACKSLASH)
			strcat(cesta, SLASH_STR);
		strcat(cesta, CFGFILE);
		konfigOK = file_exists(cesta);
	}

	return konfigOK;
}
