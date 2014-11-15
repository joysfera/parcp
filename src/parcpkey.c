#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef __MINT__
typedef unsigned short int uint16_t;
typedef int int32_t;
#else
#include <stdint.h>
#endif

typedef unsigned char BYTE;

#include "parcpkey.h"

void parcpkey(const BYTE *n, BYTE *crypta)
{
	BYTE	t1[ROTORSZ];
	BYTE	t2[ROTORSZ];
	BYTE	t3[ROTORSZ];
	BYTE	buf[13];
	BYTE	name[KEYSZ];
	int	ic, k, temp;
	uint16_t random;
	int32_t seed;
	int i, n1;

	if (! crypta)
		return;
	strncpy((char *)name, (const char *)n, sizeof(name));
	strcpy((char *)buf,"abcdefghijkl");
	buf[12] = 'm';
	
	seed = 123;
	for(i=0; i<13; i++)
		seed = seed*buf[i] + i;

	for(i=0;i<ROTORSZ;i++) {
		t1[i] = i;
		t3[i] = 0;
	}
	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		random = seed % 65521U;
		k = ROTORSZ-1 - i;
		ic = (random&MASK)%(k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0)
			continue;
		ic = (random&MASK) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0;i<ROTORSZ;i++)
		t2[t1[i]&MASK] = i;

	for(n1 = 0; n1 < KEYSZ/2; n1++) {
		i = name[n1];
		crypta[n1] = t2[(t3[(t1[(i+n1)&MASK])&MASK])&MASK]-n1;
	}
}

int check(const BYTE *crypta, const BYTE *key)
{
	register int i, a;
	
	for(i = 0; i < KEYSZ/2; i++) {
		a = ((toupper(key[2*i])-'A') & 0x0f) + (((toupper(key[2*i+1])-'K')&0x0f)<<4);
		if (a != crypta[i])
			return 0;
	}
	return 1;
}

#ifdef GENERATE

void generate(const BYTE *crypta, BYTE *key)
{
	int i, a;
	
	for(i = 0; i < KEYSZ/2; i++) {
		a = crypta[i];
		key[2*i] =	'A' + (a&0x0f);
		key[2*i+1] = 'K' + ((a>>4)&0x0f);
	}
}

int main(void)
{
	BYTE name[ROTORSZ];
	BYTE key[KEYSZ+1];
	BYTE crypta[KEYSZ];

	fprintf(stderr, "Enter user name: ");
	char *ret = fgets((char *)name, sizeof(name), stdin);
	ret = ret; // UNUSED
	int len = strlen((char *)name);
	if (name[len-1] == '\n')
		name[len-1] = 0;
	parcpkey(name, crypta);
	generate(crypta, key);
	key[KEYSZ] = '\0';
	printf("\nUserName = %s\nKeyCode = %s\n", name, key);
	fprintf(stderr,"Key: %s %s\n", key, check(crypta, key) ? "OK" : "wrong!");
	return 0;
}

#endif
