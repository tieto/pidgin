/**
 * @file stock.c GTK+ Stock resources
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
 * 
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "stock.h"
#include "core.h"
#include <gtk/gtk.h>
#include <string.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] =
{
	{ GAIM_STOCK_BGCOLOR,       "buttons", "change-bgcolor-small.png" },
	{ GAIM_STOCK_BLOCK,         NULL,      GTK_STOCK_STOP             },
	{ GAIM_STOCK_DOWNLOAD,      NULL,      GTK_STOCK_GO_DOWN          },
	{ GAIM_STOCK_FGCOLOR,       "buttons", "change-fgcolor-small.png" },
	{ GAIM_STOCK_FILE_CANCELED, NULL,      GTK_STOCK_CANCEL           },
	{ GAIM_STOCK_FILE_DONE,     NULL,      GTK_STOCK_APPLY            },
	{ GAIM_STOCK_IGNORE,        NULL,      GTK_STOCK_DIALOG_ERROR     },
	{ GAIM_STOCK_IMAGE,         "menus",   "insert-image-small.png"   },
	{ GAIM_STOCK_INFO,          NULL,      GTK_STOCK_FIND             },
	{ GAIM_STOCK_INVITE,        NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_LINK,          "menus",   "insert-link-small.png"    },
	{ GAIM_STOCK_SEND,          NULL,      GTK_STOCK_CONVERT          },
	{ GAIM_STOCK_SMILEY,        "buttons", "insert-smiley-small.png"  },
	{ GAIM_STOCK_TEXT_BIGGER,   "buttons", "text_bigger.png"          },
	{ GAIM_STOCK_TEXT_NORMAL,   "buttons", "text_normal.png"          },
	{ GAIM_STOCK_TEXT_SMALLER,  "buttons", "text_smaller.png"         },
	{ GAIM_STOCK_TYPED,         NULL,      GTK_STOCK_JUSTIFY_FILL     },
	{ GAIM_STOCK_TYPING,        NULL,      GTK_STOCK_EXECUTE          },
	{ GAIM_STOCK_UPLOAD,        NULL,      GTK_STOCK_GO_UP            },
	{ GAIM_STOCK_WARN,          NULL,      GTK_STOCK_DIALOG_WARNING   },
	{ GAIM_STOCK_IM,            NULL,      GTK_STOCK_CONVERT          },
	{ GAIM_STOCK_CHAT,          NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_AWAY,          "buttons",      "away.xpm"                 }
};

static gint stock_icon_count = sizeof(stock_icons) / sizeof(*stock_icons);

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "gaim"))
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", base, NULL);
	else
		filename = g_build_filename(DATADIR, "pixmaps", "gaim",
									dir, base, NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		debug_printf("Unable to load stock pixmap %s\n", base);

		g_free(filename);

		return NULL;
	}

	return filename;
}

void
setup_stock()
{
	GtkIconFactory *icon_factory;
	int i;
	GtkWidget *win;

	/* Setup the icon factory. */
	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	/* Er, yeah, a hack, but it works. :) */
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	for (i = 0; i < stock_icon_count; i++) {
		GdkPixbuf *pixbuf;
		GtkIconSet *iconset;
		gchar *filename;

		if (stock_icons[i].dir == NULL) {

			/* GTK+ Stock icon */
			iconset = gtk_style_lookup_icon_set(gtk_widget_get_style(win),
												stock_icons[i].filename);
		}
		else {
			filename = find_file(stock_icons[i].dir, stock_icons[i].filename);

			if (filename == NULL)
				continue;

			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

			g_free(filename);

			iconset = gtk_icon_set_new_from_pixbuf(pixbuf);
		}

		gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);

		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);

	g_object_unref(G_OBJECT(icon_factory));
}
