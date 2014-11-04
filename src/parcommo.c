/* common functions for PARCP, PARINOUT and PARTEST */

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
		strcpy(cesta, p);						/* by environment variable */

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
		strcpy(cesta, argv[0]);					/* in PARCP home folder */
		/* strip program name but retain folder delimiter */
		p = strrchr(cesta, SLASH);
		if (p == NULL)
			p = strrchr(cesta, BACKSLASH);
		/* if there isn't folder delimiter it was not a file path thus it's unusable */
		if (p != NULL) {
			*(p+1) = 0;
			strcat(cesta, CFGFILE);
			konfigOK = file_exists(cesta);
		}
	}

	if (! konfigOK) {
		char *ret = getcwd(cesta, MAXPATH);		/* in current folder */
		ret = ret; // UNUSED
		p = cesta + strlen(cesta)-1;
		if (*p != SLASH && *p != BACKSLASH)
			strcat(cesta, SLASH_STR);
		strcat(cesta, CFGFILE);
		konfigOK = file_exists(cesta);
	}

	return konfigOK;
}
