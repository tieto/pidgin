/**
 * @file gtkstock.c GTK+ Stock resources
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "internal.h"
#include "gtkgaim.h"

#include "gtkstock.h"

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] =
{
	{ GAIM_STOCK_ABOUT,           "buttons", "about_menu.png"           },
	{ GAIM_STOCK_ACCOUNTS,        "buttons", "accounts.png"             },
	{ GAIM_STOCK_ACTION,          NULL,      GTK_STOCK_EXECUTE          },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_ALIAS,           NULL,      GTK_STOCK_EDIT             },
#else
	{ GAIM_STOCK_ALIAS,           "buttons", "edit.png"                 },
#endif
	{ GAIM_STOCK_BGCOLOR,         "buttons", "change-bgcolor-small.png" },
	{ GAIM_STOCK_BLOCK,           NULL,      GTK_STOCK_STOP             },
	{ GAIM_STOCK_CHAT,            NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_CLEAR,           NULL,      GTK_STOCK_CLEAR            },
	{ GAIM_STOCK_CLOSE_TABS,      NULL,      GTK_STOCK_CLOSE            },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_CONNECT,         NULL,      GTK_STOCK_CONNECT          },
#else
	{ GAIM_STOCK_CONNECT,         "icons",   "stock_connect_16.png"     },
#endif
	{ GAIM_STOCK_DEBUG,           NULL,      GTK_STOCK_PROPERTIES       },
	{ GAIM_STOCK_DOWNLOAD,        NULL,      GTK_STOCK_GO_DOWN          },
	{ GAIM_STOCK_DIALOG_AUTH,     "dialogs", "gaim_auth.png"            },
	{ GAIM_STOCK_DIALOG_COOL,     "dialogs", "gaim_cool.png"            },
	{ GAIM_STOCK_DIALOG_ERROR,    "dialogs", "gaim_error.png"           },
	{ GAIM_STOCK_DIALOG_INFO,     "dialogs", "gaim_info.png"            },
	{ GAIM_STOCK_DIALOG_QUESTION, "dialogs", "gaim_question.png"        },
	{ GAIM_STOCK_DIALOG_WARNING,  "dialogs", "gaim_warning.png"         },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_DISCONNECT,      NULL,      GTK_STOCK_DISCONNECT       },
#else
	{ GAIM_STOCK_DISCONNECT,      "icons",   "stock_disconnect_16.png"  },
#endif
	{ GAIM_STOCK_FGCOLOR,         "buttons", "change-fgcolor-small.png" },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_EDIT,            NULL,      GTK_STOCK_EDIT             },
#else
	{ GAIM_STOCK_EDIT,            "buttons", "edit.png"                 },
#endif
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
	{ GAIM_STOCK_IM,              "buttons", "send-im.png"		    },
	{ GAIM_STOCK_IMAGE,           "buttons", "insert-image-small.png"   },
#if GTK_CHECK_VERSION(2,8,0)
	{ GAIM_STOCK_INFO,            NULL,      GTK_STOCK_INFO             },
#else
	{ GAIM_STOCK_INFO,            "buttons", "info.png"                 },
#endif
	{ GAIM_STOCK_INVITE,          NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_LINK,            "buttons", "insert-link-small.png"    },
	{ GAIM_STOCK_LOG,             NULL,      GTK_STOCK_DND_MULTIPLE     },
	{ GAIM_STOCK_MODIFY,          NULL,      GTK_STOCK_PREFERENCES      },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_PAUSE,           NULL,      GTK_STOCK_MEDIA_PAUSE      },
#else
	{ GAIM_STOCK_PAUSE,           "buttons", "pause.png"                },
#endif
	{ GAIM_STOCK_PENDING,         "buttons", "send-im.png"              },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_PLUGIN,          NULL,      GTK_STOCK_DISCONNECT       },
#else
	{ GAIM_STOCK_PLUGIN,          "icons",   "stock_disconnect_16.png"  },
#endif
	{ GAIM_STOCK_POUNCE,          NULL,      GTK_STOCK_REDO             },
	{ GAIM_STOCK_OPEN_MAIL,       NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_SEND,            "buttons", "send-im.png"              },
	{ GAIM_STOCK_SIGN_ON,         NULL,      GTK_STOCK_EXECUTE          },
	{ GAIM_STOCK_SIGN_OFF,        NULL,      GTK_STOCK_CLOSE            },
	{ GAIM_STOCK_SMILEY,          "buttons", "insert-smiley-small.png"  },
	{ GAIM_STOCK_TEXT_BIGGER,     "buttons", "text_bigger.png"          },
	{ GAIM_STOCK_TEXT_NORMAL,     "buttons", "text_normal.png"          },
	{ GAIM_STOCK_TEXT_SMALLER,    "buttons", "text_smaller.png"         },
	{ GAIM_STOCK_TYPED,           "gaim",    "typed.png"                },
	{ GAIM_STOCK_TYPING,          "gaim",    "typing.png"               },
	{ GAIM_STOCK_VOICE_CHAT,      "gaim",    "phone.png"                },
	{ GAIM_STOCK_STATUS_ONLINE,   "gaim",    "status-online.png"        },
	{ GAIM_STOCK_STATUS_OFFLINE,  "gaim",    "status-offline.png"       },
	{ GAIM_STOCK_STATUS_AWAY,     "gaim",    "status-away.png"          },
	{ GAIM_STOCK_STATUS_INVISIBLE,"gaim",    "status-invisible.png"     },
	{ GAIM_STOCK_STATUS_TYPING0,  "gaim",    "status-typing0.png"       },
	{ GAIM_STOCK_STATUS_TYPING1,  "gaim",    "status-typing1.png"       },
	{ GAIM_STOCK_STATUS_TYPING2,  "gaim",    "status-typing2.png"       },
	{ GAIM_STOCK_STATUS_TYPING3,  "gaim",    "status-typing3.png"       },
	{ GAIM_STOCK_STATUS_CONNECT0, "gaim",    "status-connect0.png"      },
	{ GAIM_STOCK_STATUS_CONNECT1, "gaim",    "status-connect1.png"      },
	{ GAIM_STOCK_STATUS_CONNECT2, "gaim",    "status-connect2.png"      },
	{ GAIM_STOCK_STATUS_CONNECT3, "gaim",    "status-connect3.png"      },
	{ GAIM_STOCK_UPLOAD,          NULL,      GTK_STOCK_GO_UP            }
};

static const GtkStockItem stock_items[] =
{
	{ GAIM_STOCK_ALIAS,      N_("_Alias"),      0, 0, NULL },
	{ GAIM_STOCK_CHAT,       N_("_Join"),       0, 0, NULL },
	{ GAIM_STOCK_CLOSE_TABS, N_("Close _tabs"), 0, 0, NULL },
	{ GAIM_STOCK_IM,         N_("I_M"),         0, 0, NULL },
	{ GAIM_STOCK_INFO,       N_("_Get Info"),   0, 0, NULL },
	{ GAIM_STOCK_INVITE,     N_("_Invite"),     0, 0, NULL },
	{ GAIM_STOCK_MODIFY,     N_("_Modify"),     0, 0, NULL },
	{ GAIM_STOCK_OPEN_MAIL,  N_("_Open Mail"),  0, 0, NULL },
	{ GAIM_STOCK_PAUSE,      N_("_Pause"),      0, 0, NULL },
};

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "gaim"))
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", base, NULL);
	else
	{
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", dir,
									base, NULL);
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
	{
		g_critical("Unable to load stock pixmap %s\n", base);

		g_free(filename);

		return NULL;
	}

	return filename;
}

void
gaim_gtk_stock_init(void)
{
	static gboolean stock_initted = FALSE;
	GtkIconFactory *icon_factory;
	size_t i;
	GtkWidget *win;

	if (stock_initted)
		return;

	stock_initted = TRUE;

	/* Setup the icon factory. */
	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	/* Er, yeah, a hack, but it works. :) */
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	for (i = 0; i < G_N_ELEMENTS(stock_icons); i++)
	{
		GdkPixbuf *pixbuf;
		GtkIconSet *iconset;
		gchar *filename;

		if (stock_icons[i].dir == NULL)
		{
			/* GTK+ Stock icon */
			iconset = gtk_style_lookup_icon_set(gtk_widget_get_style(win),
												stock_icons[i].filename);
		}
		else
		{
			filename = find_file(stock_icons[i].dir, stock_icons[i].filename);

			if (filename == NULL)
				continue;

			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

			g_free(filename);

			iconset = gtk_icon_set_new_from_pixbuf(pixbuf);

			g_object_unref(G_OBJECT(pixbuf));
		}

		gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);

		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);

	/* register custom icon sizes */
	gtk_icon_size_register(GAIM_ICON_SIZE_LOGO, 330, 90);
	gtk_icon_size_register(GAIM_ICON_SIZE_DIALOG_COOL, 40, 60);
	gtk_icon_size_register(GAIM_ICON_SIZE_STATUS, 30, 30);
	gtk_icon_size_register(GAIM_ICON_SIZE_STATUS_TWO_LINE, 30, 30);
	gtk_icon_size_register(GAIM_ICON_SIZE_STATUS_SMALL, 16, 16);
	gtk_icon_size_register(GAIM_ICON_SIZE_STATUS_SMALL_TWO_LINE, 24, 24);

	g_object_unref(G_OBJECT(icon_factory));

	/* Register the stock items. */
	gtk_stock_add_static(stock_items, G_N_ELEMENTS(stock_items));
}
