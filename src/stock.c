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
	{ GAIM_STOCK_ABOUT,           "buttons", "about_menu.png"           },
	{ GAIM_STOCK_ACCOUNTS,        "buttons", "accounts.png"             },
	{ GAIM_STOCK_BGCOLOR,         "buttons", "change-bgcolor-small.png" },
	{ GAIM_STOCK_BLOCK,           NULL,      GTK_STOCK_STOP             },
	{ GAIM_STOCK_CHAT,            NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_DOWNLOAD,        NULL,      GTK_STOCK_GO_DOWN          },
	{ GAIM_STOCK_DIALOG_AUTH,     "dialogs", "gaim_auth.png"            },
	{ GAIM_STOCK_DIALOG_COOL,     "dialogs", "gaim_cool.png"            },
	{ GAIM_STOCK_DIALOG_ERROR,    "dialogs", "gaim_error.png"           },
	{ GAIM_STOCK_DIALOG_INFO,     "dialogs", "gaim_info.png"            },
	{ GAIM_STOCK_DIALOG_QUESTION, "dialogs", "gaim_question.png"        },
	{ GAIM_STOCK_DIALOG_WARNING,  "dialogs", "gaim_warning.png"         },
	{ GAIM_STOCK_FGCOLOR,         "buttons", "change-fgcolor-small.png" },
	{ GAIM_STOCK_EDIT,            "buttons", "edit.png"                 },
	{ GAIM_STOCK_FILE_CANCELED,   NULL,      GTK_STOCK_CANCEL           },
	{ GAIM_STOCK_FILE_DONE,       NULL,      GTK_STOCK_APPLY            },
	{ GAIM_STOCK_FILE_TRANSFER,   NULL,      GTK_STOCK_REVERT_TO_SAVED  },
	{ GAIM_STOCK_ICON_AWAY,       "icons",   "away.png"                 },
	{ GAIM_STOCK_ICON_AWAY_MSG,   "icons",   "msgpend.png"              },
	{ GAIM_STOCK_ICON_CONNECT,    "icons",   "connect.png"              },
	{ GAIM_STOCK_ICON_OFFLINE,    "icons",   "offline.png"              },
	{ GAIM_STOCK_ICON_ONLINE,     "icons",   "online.png"               },
	{ GAIM_STOCK_ICON_ONLINE_MSG, "icons",   "msgunread.png"            },
	{ GAIM_STOCK_IGNORE,          NULL,      GTK_STOCK_DIALOG_ERROR     },
	{ GAIM_STOCK_IM,              NULL,      GTK_STOCK_CONVERT          },
	{ GAIM_STOCK_IMAGE,           "buttons", "insert-image-small.png"   },
	{ GAIM_STOCK_INFO,            NULL,      GTK_STOCK_FIND             },
	{ GAIM_STOCK_INVITE,          NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_LINK,            "buttons", "insert-link-small.png"    },
	{ GAIM_STOCK_LOGO,            "gaim",    "logo.png"                 },
	{ GAIM_STOCK_PRIVACY,         NULL,      GTK_STOCK_PROPERTIES       },
	{ GAIM_STOCK_SEND,            NULL,      GTK_STOCK_CONVERT          },
	{ GAIM_STOCK_SIGN_ON,         NULL,      GTK_STOCK_EXECUTE          },
	{ GAIM_STOCK_SIGN_OFF,        NULL,      GTK_STOCK_CLOSE            },
	{ GAIM_STOCK_SMILEY,          "buttons", "insert-smiley-small.png"  },
	{ GAIM_STOCK_OPEN_MAIL,       NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_TEXT_BIGGER,     "buttons", "text_bigger.png"          },
	{ GAIM_STOCK_TEXT_NORMAL,     "buttons", "text_normal.png"          },
	{ GAIM_STOCK_TEXT_SMALLER,    "buttons", "text_smaller.png"         },
	{ GAIM_STOCK_TYPED,           "gaim",    "typed.png"                },
	{ GAIM_STOCK_TYPING,          "gaim",    "typing.png"               },
	{ GAIM_STOCK_UPLOAD,          NULL,      GTK_STOCK_GO_UP            },
	{ GAIM_STOCK_WARN,            NULL,      GTK_STOCK_DIALOG_WARNING   }
};

const GtkStockItem stock_items[] =
{
	{ GAIM_STOCK_OPEN_MAIL, N_("_Open Mail"), 0, 0, NULL }
};

static gint stock_icon_count = sizeof(stock_icons) / sizeof(*stock_icons);
static gint stock_item_count = sizeof(stock_items) / sizeof(*stock_items);

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "gaim"))
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", base, NULL);
	else
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", dir, base, NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		g_critical("Unable to load stock pixmap %s\n", base);

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

	/* register logo icon size */
	gtk_icon_size_register(GAIM_ICON_SIZE_LOGO, 210, 150);

	g_object_unref(G_OBJECT(icon_factory));

	/* Register the stock items. */
	gtk_stock_add_static(stock_items, stock_item_count);
}
