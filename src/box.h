#ifndef _box_h
#define _box_h

#include "cfgopts.h"	/* for EditNumber */

#define IDOK		1
#define IDCANCEL	2
#define IDYES		3
#define IDNO		4
#define IDRETRY		5
#define IDABORT		6
#define IDIGNORE	7

#define MB_OK				1
#define MB_OKCANCEL			2
#define MB_YESNO			3
#define MB_YESNOCANCEL		4
#define MB_RETRYCANCEL		5
#define MB_ABORTRETRYIGNORE	6
#define MB_DEFBUTTON1		0x00
#define MB_DEFBUTTON2		0x10
#define MB_DEFBUTTON3		0x20

int MessageBox(const char *text, int type);
void InfoBox(const char *text, int timeout, BOOLEAN button);
BOOLEAN EditBox(const char *title, const char *text, char *return_str, int maxlen);
int EditNumber(const char *title, const char *text, TAG_TYPE tag, void *storage);

#endif
