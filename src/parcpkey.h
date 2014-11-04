#ifndef _parcpkey
#define _parcpkey

#define ROTORSZ 256
#define MASK 0377
#define	KEYSZ	16

void parcpkey(const BYTE *n, BYTE *crypta);
int check(const BYTE *crypta, const BYTE *key);

#endif
