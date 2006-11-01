#include "gntkeys.h"

#include <stdlib.h>
#include <string.h>

const char *term;

void gnt_keys_refine(char *text)
{
	if (*text == 27 && *(text + 1) == '[' && *(text + 3) == '\0' &&
			(*(text + 2) >= 'A' || *(text + 2) <= 'D')) {
		if (term == NULL)
			term = getenv("TERM");
		/* Apparently this is necessary for urxvt and screen */
		if (strcmp(term, "screen") == 0 || strcmp(term, "rxvt-unicode") == 0)
			*(text + 1) = 'O';
	}
}

