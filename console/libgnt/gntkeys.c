#include "gntkeys.h"

#include <stdlib.h>
#include <string.h>

static const char *term;

void gnt_keys_refine(char *text)
{
	if (term == NULL) {
		term = getenv("TERM");
		if (!term)
			term = "";  /* Just in case */
	}

	if (*text == 27 && *(text + 1) == '[' && *(text + 3) == '\0' &&
			(*(text + 2) >= 'A' || *(text + 2) <= 'D')) {
		/* Apparently this is necessary for urxvt and screen and xterm */
		if (strcmp(term, "screen") == 0 || strcmp(term, "rxvt-unicode") == 0 ||
				strcmp(term, "xterm") == 0)
			*(text + 1) = 'O';
	} else if (*(unsigned char*)text == 195) {
		if (*(text + 2) == 0 && strcmp(term, "xterm") == 0) {
			*(text) = 27;
			*(text + 1) -= 64;  /* Say wha? */
		}
	}
}

