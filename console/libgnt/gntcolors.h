#ifndef GNT_COLORS_H
#define GNT_COLORS_H

typedef enum
{
	GNT_COLOR_NORMAL = 1,
	GNT_COLOR_HIGHLIGHT,		/* eg. when a button is selected */
	GNT_COLOR_DISABLED,		/* eg. when a button is disabled */
	GNT_COLOR_HIGHLIGHT_D,	/* eg. when a button is selected, but some other window is in focus */
	GNT_COLOR_TEXT_NORMAL,
	GNT_COLOR_TEXT_INACTIVE,	/* when the entry is out of focus */
	GNT_COLOR_MNEMONIC,
	GNT_COLOR_MNEMONIC_D,
	GNT_COLOR_SHADOW,
	GNT_COLOR_TITLE,
	GNT_COLORS
} GntColorType;

enum
{
	GNT_COLOR_BLACK = 1,
	GNT_COLOR_RED,
	GNT_COLOR_GREEN,
	GNT_COLOR_BLUE,
	GNT_COLOR_WHITE,
	GNT_COLOR_GRAY,
	GNT_COLOR_DARK_GRAY,
	GNT_TOTAL_COLORS
};

/* populate some default colors */
void gnt_init_colors();

#endif
