#ifndef _shell_h
#define _shell_h

#include "element.h"	/* specially for BOOLEAN */

/* whole Parshell is displayed in sirka x vyska screen array */
#define sirka	(COLS)
#define vyska	(LINES-1)

extern int original_cursor;	/* original cursor state */
extern BOOLEAN has_dim;

#endif /* _shell_h */
