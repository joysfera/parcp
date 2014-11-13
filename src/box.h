#ifndef _box_h
#define _box_h

#include "cfgopts.h"	/* for EditNumber */

#define myIDOK		1
#define myIDCANCEL	2
#define myIDYES		3
#define myIDNO		4
#define myIDRETRY		5
#define myIDABORT		6
#define myIDIGNORE	7

#define myMB_OK				1
#define myMB_OKCANCEL			2
#define myMB_YESNO			3
#define myMB_YESNOCANCEL		4
#define myMB_RETRYCANCEL		5
#define myMB_ABORTRETRYIGNORE	6
#define myMB_DEFBUTTON1		0x00
#define myMB_DEFBUTTON2		0x10
#define myMB_DEFBUTTON3		0x20

int myMessageBox(const char *text, int type);
void InfoBox(const char *text, int timeout, MYBOOL button);
MYBOOL EditBox(const char *title, const char *text, char *return_str, int maxlen);
int EditNumber(const char *title, const char *text, TAG_TYPE tag, void *storage);

#endif
