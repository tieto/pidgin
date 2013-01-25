/**
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

#include "config.h"

#include <ncurses.h>

#include "gntinternal.h"
#undef GNT_LOG_DOMAIN
#define GNT_LOG_DOMAIN "Colors"

#include "gntcolors.h"
#include "gntstyle.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static gboolean hascolors;
static int custom_type = GNT_COLORS;
static struct
{
	short r, g, b;
} colors[GNT_TOTAL_COLORS];

static void
backup_colors(void)
{
	short i;
	for (i = 0; i < GNT_TOTAL_COLORS; i++)
	{
		color_content(i, &colors[i].r,
				&colors[i].g, &colors[i].b);
	}
}

static gboolean
can_use_custom_color(void)
{
	return (gnt_style_get_bool(GNT_STYLE_COLOR, FALSE) && can_change_color());
}

static void
restore_colors(void)
{
	short i;
	for (i = 0; i < GNT_TOTAL_COLORS; i++)
	{
		init_color(i, colors[i].r,
				colors[i].g, colors[i].b);
	}
}

void gnt_init_colors()
{
	static gboolean init = FALSE;
	int defaults;

	if (init)
		return;
	init = TRUE;

	start_color();
	if (!(hascolors = has_colors()))
		return;
	defaults = use_default_colors();

	if (can_use_custom_color())
	{
		backup_colors();

		/* Do some init_color()s */
		init_color(GNT_COLOR_BLACK, 0, 0, 0);
		init_color(GNT_COLOR_RED, 1000, 0, 0);
		init_color(GNT_COLOR_GREEN, 0, 1000, 0);
		init_color(GNT_COLOR_BLUE, 250, 250, 700);
		init_color(GNT_COLOR_WHITE, 1000, 1000, 1000);
		init_color(GNT_COLOR_GRAY, 699, 699, 699);
		init_color(GNT_COLOR_DARK_GRAY, 256, 256, 256);

		/* Now some init_pair()s */
		init_pair(GNT_COLOR_NORMAL, GNT_COLOR_BLACK, GNT_COLOR_WHITE);
		init_pair(GNT_COLOR_HIGHLIGHT, GNT_COLOR_WHITE, GNT_COLOR_BLUE);
		init_pair(GNT_COLOR_SHADOW, GNT_COLOR_BLACK, GNT_COLOR_DARK_GRAY);

		init_pair(GNT_COLOR_TITLE, GNT_COLOR_WHITE, GNT_COLOR_BLUE);
		init_pair(GNT_COLOR_TITLE_D, GNT_COLOR_WHITE, GNT_COLOR_GRAY);

		init_pair(GNT_COLOR_TEXT_NORMAL, GNT_COLOR_WHITE, GNT_COLOR_BLUE);
		init_pair(GNT_COLOR_HIGHLIGHT_D, GNT_COLOR_BLACK, GNT_COLOR_GRAY);
		init_pair(GNT_COLOR_DISABLED, GNT_COLOR_GRAY, GNT_COLOR_WHITE);
		init_pair(GNT_COLOR_URGENT, GNT_COLOR_WHITE, GNT_COLOR_RED);
	}
	else
	{
		int bg;

		if (defaults == OK) {
			init_pair(GNT_COLOR_NORMAL, -1, -1);
			bg = -1;
		} else {
			init_pair(GNT_COLOR_NORMAL, COLOR_BLACK, COLOR_WHITE);
			bg = COLOR_WHITE;
		}
		init_pair(GNT_COLOR_DISABLED, COLOR_YELLOW, bg);
		init_pair(GNT_COLOR_URGENT, COLOR_GREEN, bg);

		init_pair(GNT_COLOR_HIGHLIGHT, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_SHADOW, COLOR_BLACK, COLOR_BLACK);
		init_pair(GNT_COLOR_TITLE, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_TITLE_D, COLOR_WHITE, COLOR_BLACK);
		init_pair(GNT_COLOR_TEXT_NORMAL, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_HIGHLIGHT_D, COLOR_CYAN, COLOR_BLACK);
	}
}

void
gnt_uninit_colors()
{
	if (can_use_custom_color())
		restore_colors();
}

int
gnt_colors_get_color(char *key)
{
	int color;
	gboolean custom = can_use_custom_color();

	key = g_strstrip(key);

	if (strcmp(key, "black") == 0)
		color = custom ? GNT_COLOR_BLACK : COLOR_BLACK;
	else if (strcmp(key, "red") == 0)
		color = custom ? GNT_COLOR_RED : COLOR_RED;
	else if (strcmp(key, "green") == 0)
		color = custom ? GNT_COLOR_GREEN : COLOR_GREEN;
	else if (strcmp(key, "blue") == 0)
		color = custom ? GNT_COLOR_BLUE : COLOR_BLUE;
	else if (strcmp(key, "white") == 0)
		color = custom ? GNT_COLOR_WHITE : COLOR_WHITE;
	else if (strcmp(key, "gray") == 0 || strcmp(key, "grey") == 0)
		color = custom ? GNT_COLOR_GRAY : COLOR_YELLOW;  /* eh? */
	else if (strcmp(key, "darkgray") == 0 || strcmp(key, "darkgrey") == 0)
		color = custom ? GNT_COLOR_DARK_GRAY : COLOR_BLACK;
	else if (strcmp(key, "magenta") == 0)
		color = COLOR_MAGENTA;
	else if (strcmp(key, "cyan") == 0)
		color = COLOR_CYAN;
	else if (strcmp(key, "default") == 0)
		color = -1;
	else {
		g_warning("Invalid color name: %s\n", key);
		color = -EINVAL;
	}
	return color;
}

void gnt_colors_parse(GKeyFile *kfile)
{
	GError *error = NULL;
	gsize nkeys;
	char **keys = g_key_file_get_keys(kfile, "colors", &nkeys, &error);

	if (error)
	{
		gnt_warning("%s", error->message);
		g_error_free(error);
		error = NULL;
	}
	else if (nkeys)
	{
		gnt_init_colors();
		while (nkeys--)
		{
			gsize len;
			gchar *key = keys[nkeys];
			char **list = g_key_file_get_string_list(kfile, "colors", key, &len, NULL);
			if (len == 3)
			{
				int r = atoi(list[0]);
				int g = atoi(list[1]);
				int b = atoi(list[2]);
				int color = -1;

				key = g_ascii_strdown(key, -1);
				color = gnt_colors_get_color(key);
				g_free(key);
				if (color == -EINVAL) {
					g_strfreev(list);
					continue;
				}

				init_color(color, r, g, b);
			}
			g_strfreev(list);
		}

		g_strfreev(keys);
	}

	gnt_color_pairs_parse(kfile);
}

void gnt_color_pairs_parse(GKeyFile *kfile)
{
	GError *error = NULL;
	gsize nkeys;
	char **keys = g_key_file_get_keys(kfile, "colorpairs", &nkeys, &error);

	if (error)
	{
		gnt_warning("%s", error->message);
		g_error_free(error);
		return;
	}
	else if (nkeys)
		gnt_init_colors();

	while (nkeys--)
	{
		gsize len;
		gchar *key = keys[nkeys];
		char **list = g_key_file_get_string_list(kfile, "colorpairs", key, &len, NULL);
		if (len == 2)
		{
			GntColorType type = 0;
			gchar *fgc = g_ascii_strdown(list[0], -1);
			gchar *bgc = g_ascii_strdown(list[1], -1);
			int fg = gnt_colors_get_color(fgc);
			int bg = gnt_colors_get_color(bgc);
			g_free(fgc);
			g_free(bgc);
			if (fg == -EINVAL || bg == -EINVAL) {
				g_strfreev(list);
				continue;
			}

			key = g_ascii_strdown(key, -1);

			if (strcmp(key, "normal") == 0)
				type = GNT_COLOR_NORMAL;
			else if (strcmp(key, "highlight") == 0)
				type = GNT_COLOR_HIGHLIGHT;
			else if (strcmp(key, "highlightd") == 0)
				type = GNT_COLOR_HIGHLIGHT_D;
			else if (strcmp(key, "shadow") == 0)
				type = GNT_COLOR_SHADOW;
			else if (strcmp(key, "title") == 0)
				type = GNT_COLOR_TITLE;
			else if (strcmp(key, "titled") == 0)
				type = GNT_COLOR_TITLE_D;
			else if (strcmp(key, "text") == 0)
				type = GNT_COLOR_TEXT_NORMAL;
			else if (strcmp(key, "disabled") == 0)
				type = GNT_COLOR_DISABLED;
			else if (strcmp(key, "urgent") == 0)
				type = GNT_COLOR_URGENT;
			else {
				g_strfreev(list);
				g_free(key);
				continue;
			}
			g_free(key);

			init_pair(type, fg, bg);
		}
		g_strfreev(list);
	}

	g_strfreev(keys);
}

int gnt_color_pair(int pair)
{
	return (hascolors ? COLOR_PAIR(pair) :
		((pair == GNT_COLOR_NORMAL || pair == GNT_COLOR_HIGHLIGHT_D ||
		  pair == GNT_COLOR_TITLE_D || pair == GNT_COLOR_DISABLED) ? 0 : A_STANDOUT));
}

int gnt_color_add_pair(int fg, int bg)
{
	init_pair(custom_type, fg, bg);
	return custom_type++;
}
