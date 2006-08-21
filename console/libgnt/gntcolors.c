#include <ncursesw/ncurses.h>
#include "gntcolors.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

static struct
{
	short r, g, b;
} colors[GNT_TOTAL_COLORS];

static void
backup_colors()
{
	short i;
	for (i = 0; i < GNT_TOTAL_COLORS; i++)
	{
		color_content(i, &colors[i].r,
				&colors[i].g, &colors[i].b);
	}
}

static void
restore_colors()
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
	start_color();
	if (can_change_color())
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
	}
	else
	{
		init_pair(GNT_COLOR_NORMAL, COLOR_BLACK, COLOR_WHITE);
		init_pair(GNT_COLOR_HIGHLIGHT, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_SHADOW, COLOR_BLACK, COLOR_BLACK);
		init_pair(GNT_COLOR_TITLE, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_TITLE_D, COLOR_WHITE, COLOR_BLACK);
		init_pair(GNT_COLOR_TEXT_NORMAL, COLOR_WHITE, COLOR_BLUE);
		init_pair(GNT_COLOR_HIGHLIGHT_D, COLOR_CYAN, COLOR_BLACK);
		init_pair(GNT_COLOR_DISABLED, COLOR_YELLOW, COLOR_WHITE);
	}
}

void
gnt_uninit_colors()
{
	restore_colors();
}

static int
get_color(char *key)
{
	int color;

	key = g_strstrip(key);

	if (strcmp(key, "black") == 0)
		color = GNT_COLOR_BLACK;
	else if (strcmp(key, "red") == 0)
		color = GNT_COLOR_RED;
	else if (strcmp(key, "green") == 0)
		color = GNT_COLOR_GREEN;
	else if (strcmp(key, "blue") == 0)
		color = GNT_COLOR_BLUE;
	else if (strcmp(key, "white") == 0)
		color = GNT_COLOR_WHITE;
	else if (strcmp(key, "gray") == 0)
		color = GNT_COLOR_GRAY;
	else if (strcmp(key, "darkgray") == 0)
		color = GNT_COLOR_DARK_GRAY;
	else
		color = -1;
	return color;
}

#if GLIB_CHECK_VERSION(2,6,0)
void gnt_colors_parse(GKeyFile *kfile)
{
	GError *error = NULL;
	gsize nkeys;
	char **keys = g_key_file_get_keys(kfile, "colors", &nkeys, &error);

	if (error)
	{
		g_printerr("GntColors: %s\n", error->message);
		g_error_free(error);
		error = NULL;
	}
	else
	{
		while (nkeys--)
		{
			gsize len;
			char *key = keys[nkeys];
			char **list = g_key_file_get_string_list(kfile, "colors", key, &len, NULL);
			if (len == 3)
			{
				int r = atoi(list[0]);
				int g = atoi(list[1]);
				int b = atoi(list[2]);
				int color = -1;

				g_ascii_strdown(key, -1);
				color = get_color(key);
				if (color == -1)
					continue;

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
		g_printerr("GntColors: %s\n", error->message);
		g_error_free(error);
		return;
	}

	while (nkeys--)
	{
		gsize len;
		char *key = keys[nkeys];
		char **list = g_key_file_get_string_list(kfile, "colorpairs", key, &len, NULL);
		if (len == 2)
		{
			GntColorType type = 0;
			int fg = get_color(g_ascii_strdown(list[0], -1));
			int bg = get_color(g_ascii_strdown(list[1], -1));
			if (fg == -1 || bg == -1)
				continue;

			g_ascii_strdown(key, -1);

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
			else
				continue;

			init_pair(type, fg, bg);
		}
		g_strfreev(list);
	}

	g_strfreev(keys);
}

#endif  /* GKeyFile */
