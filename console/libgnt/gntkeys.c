#include "gntkeys.h"

#include <stdlib.h>
#include <string.h>

char *gnt_key_cup;
char *gnt_key_cdown;
char *gnt_key_cleft;
char *gnt_key_cright;


static const char *term;

void gnt_init_keys()
{
	if (term == NULL) {
		term = getenv("TERM");
		if (!term)
			term = "";  /* Just in case */
	}

	if (strcmp(term, "xterm") == 0 || strcmp(term, "rxvt") == 0) {
		gnt_key_cup    = "\033" "[1;5A";
		gnt_key_cdown  = "\033" "[1;5B";
		gnt_key_cright = "\033" "[1;5C";
		gnt_key_cleft  = "\033" "[1;5D";
	} else if (strcmp(term, "screen") == 0 || strcmp(term, "rxvt-unicode") == 0) {
		gnt_key_cup    = "\033" "Oa";
		gnt_key_cdown  = "\033" "Ob";
		gnt_key_cright = "\033" "Oc";
		gnt_key_cleft  = "\033" "Od";
	}
}

void gnt_keys_refine(char *text)
{
	if (*text == 27 && *(text + 1) == '[' && *(text + 3) == '\0' &&
			(*(text + 2) >= 'A' && *(text + 2) <= 'D')) {
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

