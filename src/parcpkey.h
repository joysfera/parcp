#ifndef _parcpkey
#define _parcpkey

#define ROTORSZ 256
#define MASK 0377
#define	KEYSZ	16

void parcpkey(BYTE *n, BYTE *crypta);
int check(BYTE *crypta, BYTE *key);

#endif
