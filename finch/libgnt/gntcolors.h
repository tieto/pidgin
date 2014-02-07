/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_COLORS_H
#define GNT_COLORS_H
/**
 * SECTION:gntcolors
 * @section_id: libgnt-gntcolors
 * @short_description: <filename>gntcolors.h</filename>
 * @title: Colors
 */

#include <glib.h>

/**
 * GntColorType:
 *
 * Different classes of colors.
 */
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
	GNT_COLOR_TITLE_D,
	GNT_COLOR_URGENT,       /* this is for the 'urgent' windows */
	GNT_COLORS
} GntColorType;

enum
{
	GNT_COLOR_BLACK = 0,
	GNT_COLOR_RED,
	GNT_COLOR_GREEN,
	GNT_COLOR_BLUE,
	GNT_COLOR_WHITE,
	GNT_COLOR_GRAY,
	GNT_COLOR_DARK_GRAY,
	GNT_TOTAL_COLORS
};

/**
 * gnt_init_colors:
 *
 * Initialize the colors.
 */
void gnt_init_colors(void);

/**
 * gnt_uninit_colors:
 *
 * Uninitialize the colors.
 */
void gnt_uninit_colors(void);

/**
 * gnt_colors_parse:
 * @kfile:  The file containing color information.
 *
 * Parse color information from a file.
 */
void gnt_colors_parse(GKeyFile *kfile);

/**
 * gnt_color_pairs_parse:
 * @kfile: The file containing the color-pair information.
 *
 * Parse color-pair information from a file.
 */
void gnt_color_pairs_parse(GKeyFile *kfile);

/**
 * gnt_colors_get_color:
 * @key: The string value
 *
 * Parse a string color
 *
 * Returns: A color. For an unknown color name, returns -EINVAL.
 *
 * Since: 2.4.0
 */
int gnt_colors_get_color(char *key);

/**
 * gnt_color_pair:
 * @color:   The color code.
 *
 * Return the appropriate character attribute for a specified color.
 * If the terminal doesn't have color support, this returns A_STANDOUT
 * when deemed appropriate.
 *
 * Returns:  A character attribute.
 *
 * Since: 2.3.0
 */
int gnt_color_pair(int color);

/**
 * gnt_color_add_pair:
 * @fg:   Foreground
 * @bg:   Background
 *
 * Adds a color definition
 *
 * Returns:  A color pair
 *
 * Since: 2.4.0
 */
int gnt_color_add_pair(int fg, int bg);

#endif
