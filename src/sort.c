/********************************************************/
/* dir/file list sorting                                */
/********************************************************/
int sgn(int cislo)
{
	if (cislo > 0)
		return 1;
	else if (cislo < 0)
		return -1;
	else
		return 0;
}

int comp_uni(const char *r1, const char *r2, int odkud, int kolik)
{
	int porovnano;

	if (_sort_case)
		porovnano = strncmp(r1+odkud, r2+odkud, kolik);
	else
		porovnano = strncasecmp(r1+odkud, r2+odkud, kolik);

	return sgn(porovnano);
}

int comp_name(const char *a, const char *b)
{
	return comp_uni(a, b, 0, MAXFNAME);
}
int icomp_name(const char *a, const char *b)
{
	return - comp_name(a, b);
}

int hledej_tecku(const char *radek, int pokud)
{
	register int i;
	for(i=pokud-1; i>0; i--)
		if (radek[i] == '.')
			return i;
	return -1;
}

int comp_ext(const char *r1, const char *r2)
{
	int porovnat;
	int odkud1, odkud2, kolik;

	odkud1 = hledej_tecku(r1, MAXFNAME);
	odkud2 = hledej_tecku(r2, MAXFNAME);

	if (odkud1 >= 0 && odkud2 >= 0)		/* both files have extensions */
	{
		int min1 = MAXFNAME-odkud1;
		int min2 = MAXFNAME-odkud2;
		kolik = min1 < min2 ? min1 : min2;	/* common part */
		porovnat = comp_uni(r1+odkud1, r2+odkud2, 0, kolik);
		if (porovnat == 0) {		/* beginning of extension is same */
			porovnat = sgn(odkud1 - odkud2);	/* different extensions length */
			if (porovnat == 0)	/* extensions are the same, the names will resolve */
				porovnat = comp_uni(r1, r2, 0, MAXFNAME);
		}
	}

	else if (odkud1 < 0 && odkud2 < 0)	/* no extensions - names will resolve */
		porovnat = comp_uni(r1, r2, 0, MAXFNAME);

	else	/* only one with extension */
	{
		if (odkud2 < 0)
			porovnat = 1;
		else
			porovnat = -1;
	}

	return porovnat;
}

int icomp_ext(const char *a, const char *b)
{
	return - comp_ext(a, b);
}

int comp_size(const char *a, const char *b)
{
	return comp_uni(a, b, MAXFNAME, 10);
}
int icomp_size(const char *a, const char *b)
{
	return - comp_size(a, b);
}

int comp_date(const char *a, const char *b)
{
	return comp_uni(a, b, MAXFNAME+12, 16);
}
int icomp_date(const char *a, const char *b)
{
	return - comp_date(a, b);
}

void swap_lines(char *r1, char *r2)
{
	char tmpradek[DIRLINELEN];
	memcpy(tmpradek, r1, DIRLINELEN);
	memcpy(r1, r2, DIRLINELEN);
	memcpy(r2, tmpradek, DIRLINELEN);
}

/*
   **  ssort()  --  Fast, small, qsort()-compatible Shell sort
   **
   **  by Ray Gardner,  public domain   5/90
 */

#define  SWAP(a, b)  ((*swap)((void *)(a), (void *)(b)))

#define  COMP(a, b)  ((*comp)((void *)(a), (void *)(b)))

void parcp_sort(void *base, size_t nel, size_t width,
		   int (*comp) (const void *, const void *),
		   void (*swap) (void *, void *))
{
	size_t wnel, gap, wgap, i, j;
	char *a, *b;

	wnel = width * nel;
	for (gap = 0; ++gap < nel;)
		gap *= 3;
	while (gap /= 3) {
		wgap = width * gap;
		for (i = wgap; i < wnel; i += width) {
			for (j = i - wgap;; j -= wgap) {
				a = j + (char *) base;
				b = a + wgap;
				if (COMP(a, b) <= 0)
					break;
				SWAP(a,b);
				if (j < wgap)
					break;
			}
		}
	}
}

void setrid_obsah(char *zacatek, int a, int b, char jak)
{
	void *comp_ptr, *swap_ptr;

	switch(jak) {
		case 'N':	comp_ptr = comp_name; break;
		case 'n':	comp_ptr = icomp_name; break;
		case 'E':	comp_ptr = comp_ext; break;
		case 'e':	comp_ptr = icomp_ext; break;
		case 'S':	comp_ptr = comp_size; break;
		case 's':	comp_ptr = icomp_size; break;
		/*  timestamp in different order (Thing like) */
		case 'D':	comp_ptr = icomp_date; break;
		case 'd':	comp_ptr = comp_date; break;

		default:	return;		/* should be unsorted */
	}
	swap_ptr = swap_lines;

	parcp_sort(zacatek+a*DIRLINELEN, b-a+1, DIRLINELEN, comp_ptr, swap_ptr);
}

void dej_adresare_jako_prvni(char *zacatek, int lines)
{
#define _is_dir(i)	(*(zacatek+(i)*DIRLINELEN+MAXFNAME+9) == '>')

	int i, j;

	i = 0;
	j = lines-1;
	while(i <= j) {
		if (_is_dir(i) < _is_dir(j)) {
			char tmpradek[DIRLINELEN];
			memcpy(tmpradek, zacatek+i*DIRLINELEN, DIRLINELEN);
			memcpy(zacatek+i*DIRLINELEN, zacatek+j*DIRLINELEN, DIRLINELEN);
			memcpy(zacatek+j*DIRLINELEN, tmpradek, DIRLINELEN);
		}
		if (_is_dir(i))
			i++;
		if (!_is_dir(j))
			j--;
	}

	setrid_obsah(zacatek, 0, i-1, toupper(_sort_jak) == 'S' ? 'N' : _sort_jak);	/* sort folders */

	setrid_obsah(zacatek, i, lines-1, _sort_jak);	/* sort files */
}

void setridit_list_dir(char *buffer)
{
	int lines = strlen(buffer) / DIRLINELEN;
	if (lines > 1)
		dej_adresare_jako_prvni(buffer, lines);
}
