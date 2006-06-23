#include <ncursesw/ncurses.h>
#include "gntcolors.h"

void gnt_init_colors()
{
	if (can_change_color())
	{
		/* XXX: Do some init_color()s */
		init_color(GNT_COLOR_BLACK, 0, 0, 0);
		init_color(GNT_COLOR_RED, 1000, 0, 0);
		init_color(GNT_COLOR_GREEN, 0, 1000, 0);
		init_color(GNT_COLOR_BLUE, 0, 0, 1000);
		init_color(GNT_COLOR_WHITE, 1000, 1000, 1000);
		init_color(GNT_COLOR_GRAY, 799, 799, 799);
		init_color(GNT_COLOR_DARK_GRAY, 256, 256, 256);

		/* Now some init_pair()s */
		init_pair(GNT_COLOR_NORMAL, GNT_COLOR_BLACK, GNT_COLOR_WHITE);
		init_pair(GNT_COLOR_HIGHLIGHT, GNT_COLOR_BLUE, GNT_COLOR_GRAY);
		init_pair(GNT_COLOR_SHADOW, GNT_COLOR_BLACK, GNT_COLOR_DARK_GRAY);
		init_pair(GNT_COLOR_TITLE, GNT_COLOR_WHITE, GNT_COLOR_DARK_GRAY);
		init_pair(GNT_COLOR_TEXT_NORMAL, GNT_COLOR_BLACK, GNT_COLOR_GRAY);
	}
	else
	{
		init_pair(GNT_COLOR_NORMAL, COLOR_BLACK, COLOR_WHITE);
		init_pair(GNT_COLOR_HIGHLIGHT, COLOR_CYAN, COLOR_BLACK);
		init_pair(GNT_COLOR_SHADOW, COLOR_BLACK, COLOR_BLACK);
		init_pair(GNT_COLOR_TITLE, COLOR_WHITE, COLOR_BLACK);
		init_pair(GNT_COLOR_TEXT_NORMAL, COLOR_BLACK, COLOR_WHITE);
	}
}

