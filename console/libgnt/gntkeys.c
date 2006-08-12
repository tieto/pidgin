#include "gntkeys.h"

#include <string.h>

void gnt_keys_refine(char *text)
{
	if (text[0] == 27)
	{
		/* These are for urxvt */
		if (strcmp(text + 1, "Oa") == 0)
		{
			strcpy(text + 1, GNT_KEY_CTRL_UP);
		}
		else if (strcmp(text + 1, "Ob") == 0)
		{
			strcpy(text + 1, GNT_KEY_CTRL_DOWN);
		}
	}
	else if ((unsigned char)text[0] == 195)
	{
		/* These for xterm */
		if (text[2] == 0)
		{
			text[0] = 27;
			text[1] -= 64;
		}
	}
}

