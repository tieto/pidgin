/**
 * @file pidginstock.c GTK+ Stock resources
 * @ingroup gtkui
 *
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "pidgin.h"

#include "pidginstock.h"

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] =
{
	{ PIDGIN_STOCK_ABOUT,           "buttons", "about_menu.png"           },
	{ PIDGIN_STOCK_ACTION,          NULL,      GTK_STOCK_EXECUTE          },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_ALIAS,           NULL,      GTK_STOCK_EDIT             },
#else
	{ PIDGIN_STOCK_ALIAS,           "buttons", "edit.png"                 },
#endif
	{ PIDGIN_STOCK_BLOCK,           NULL,      GTK_STOCK_STOP             },
	{ PIDGIN_STOCK_UNBLOCK,         NULL,      GTK_STOCK_STOP /* XXX: */  },
	{ PIDGIN_STOCK_CHAT,            NULL,      GTK_STOCK_JUMP_TO          },
	{ PIDGIN_STOCK_CLEAR,           NULL,      GTK_STOCK_CLEAR            },
	{ PIDGIN_STOCK_CLOSE_TABS,      NULL,      GTK_STOCK_CLOSE            },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_CONNECT,         NULL,      GTK_STOCK_CONNECT          },
#else
	{ PIDGIN_STOCK_CONNECT,         "icons",   "stock_connect_16.png"     },
#endif
	{ PIDGIN_STOCK_DEBUG,           NULL,      GTK_STOCK_PROPERTIES       },
	{ PIDGIN_STOCK_DOWNLOAD,        NULL,      GTK_STOCK_GO_DOWN          },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_DISCONNECT,      NULL,      GTK_STOCK_DISCONNECT       },
#else
	{ PIDGIN_STOCK_DISCONNECT,      "icons",   "stock_disconnect_16.png"  },
#endif
	{ PIDGIN_STOCK_FGCOLOR,         "buttons", "change-fgcolor-small.png" },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_EDIT,            NULL,      GTK_STOCK_EDIT             },
#else
	{ PIDGIN_STOCK_EDIT,            "buttons", "edit.png"                 },
#endif
	{ PIDGIN_STOCK_FILE_CANCELED,   NULL,      GTK_STOCK_CANCEL           },
	{ PIDGIN_STOCK_FILE_DONE,       NULL,      GTK_STOCK_APPLY            },
	{ PIDGIN_STOCK_FILE_TRANSFER,   NULL,      GTK_STOCK_REVERT_TO_SAVED  },
	{ PIDGIN_STOCK_IGNORE,          NULL,      GTK_STOCK_DIALOG_ERROR     },
	{ PIDGIN_STOCK_INVITE,          NULL,      GTK_STOCK_JUMP_TO          },
	{ PIDGIN_STOCK_MODIFY,          NULL,      GTK_STOCK_PREFERENCES      },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_PAUSE,           NULL,      GTK_STOCK_MEDIA_PAUSE      },
#else
	{ PIDGIN_STOCK_PAUSE,           "buttons", "pause.png"                },
#endif
	{ PIDGIN_STOCK_POUNCE,          NULL,      GTK_STOCK_REDO             },
	{ PIDGIN_STOCK_OPEN_MAIL,       NULL,      GTK_STOCK_JUMP_TO          },
	{ PIDGIN_STOCK_SIGN_ON,         NULL,      GTK_STOCK_EXECUTE          },
	{ PIDGIN_STOCK_SIGN_OFF,        NULL,      GTK_STOCK_CLOSE            },
	{ PIDGIN_STOCK_TYPED,           "pidgin",  "typed.png"                },
	{ PIDGIN_STOCK_UPLOAD,          NULL,      GTK_STOCK_GO_UP            },
};

static const GtkStockItem stock_items[] =
{
	{ PIDGIN_STOCK_ALIAS,               N_("_Alias"),      0, 0, NULL },
	{ PIDGIN_STOCK_CHAT,                N_("_Join"),       0, 0, NULL },
	{ PIDGIN_STOCK_CLOSE_TABS,          N_("Close _tabs"), 0, 0, NULL },
	{ PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, N_("I_M"),         0, 0, NULL },
	{ PIDGIN_STOCK_TOOLBAR_USER_INFO,   N_("_Get Info"),   0, 0, NULL },
	{ PIDGIN_STOCK_INVITE,              N_("_Invite"),     0, 0, NULL },
	{ PIDGIN_STOCK_MODIFY,              N_("_Modify"),     0, 0, NULL },
	{ PIDGIN_STOCK_OPEN_MAIL,           N_("_Open Mail"),  0, 0, NULL },
	{ PIDGIN_STOCK_PAUSE,               N_("_Pause"),      0, 0, NULL },
};

static struct SizedStockIcon {
  const char *name;
  const char *dir;
  const char *filename;
  gboolean extra_small;
  gboolean small;
  gboolean medium;
  gboolean large;
  gboolean huge;
  gboolean rtl;
} const sized_stock_icons [] = {
	{ PIDGIN_STOCK_STATUS_AVAILABLE,   "status", "available.png", 	TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_AVAILABLE_I, "status", "available_i.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_AWAY, 	   "status", "away.png",	TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_AWAY_I, 	"status", "away_i.png",		TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_BUSY, 	"status", "busy.png", 		TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_BUSY_I, 	"status", "busy_i.png", 	TRUE, FALSE, FALSE, FALSE, FALSE,FALSE },
	{ PIDGIN_STOCK_STATUS_CHAT, 	"status", "chat.png",		TRUE, TRUE, TRUE, TRUE, FALSE, TRUE },
	{ PIDGIN_STOCK_STATUS_XA, 	"status", "extended-away.png",	TRUE, TRUE, TRUE, TRUE, FALSE, TRUE },
	{ PIDGIN_STOCK_STATUS_XA_I, 	"status", "extended-away_i.png",TRUE, FALSE, FALSE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_STATUS_LOGIN, 	"status", "log-in.png",		TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_LOGOUT, 	"status", "log-out.png",	TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_OFFLINE, 	"status", "offline.png",	TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_PERSON, 	"status", "person.png",		TRUE, TRUE, TRUE, TRUE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_MESSAGE, 	"status", "message-pending.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	
	{ PIDGIN_STOCK_STATUS_IGNORED,	"emblems", "blocked.png",	TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_FOUNDER,	"emblems", "founder.png",	TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_OPERATOR,	"emblems", "operator.png",	TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_HALFOP, 	"emblems", "half-operator.png",	TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_VOICE, 	"emblems", "voice.png",		TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },

	{ PIDGIN_STOCK_DIALOG_AUTH,	"dialogs", "auth.png",		TRUE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_COOL,	"dialogs", "cool.png", 		FALSE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_ERROR,	"dialogs", "error.png",		TRUE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_INFO,	"dialogs", "info.png",		TRUE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_MAIL,	"dialogs", "mail.png",		TRUE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_QUESTION,	"dialogs", "question.png",	TRUE, FALSE, FALSE, FALSE, TRUE, FALSE },
	{ PIDGIN_STOCK_DIALOG_WARNING,	"dialogs", "warning.png",	FALSE, FALSE, FALSE, FALSE, TRUE, FALSE },

	{ PIDGIN_STOCK_ANIMATION_CONNECT0, "animations", "connect0.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT1, "animations", "connect1.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT2, "animations", "connect2.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT3, "animations", "connect3.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT4, "animations", "connect4.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT5, "animations", "connect5.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT6, "animations", "connect6.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT7, "animations", "connect7.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_CONNECT8, "animations", "connect8.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_TYPING0,  "animations", "typing0.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_TYPING1,  "animations", "typing1.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_TYPING2,  "animations", "typing2.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_TYPING3,  "animations", "typing3.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_ANIMATION_TYPING4,  "animations", "typing4.png",TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },

	{ PIDGIN_STOCK_TOOLBAR_ACCOUNTS, "toolbar", "accounts.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_BGCOLOR, "toolbar", "change-bgcolor.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_FGCOLOR, "toolbar", "change-fgcolor.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_SMILEY, "toolbar", "emote-select.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER, "toolbar", "font-size-down.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_TEXT_LARGER, "toolbar", "font-size-up.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE, "toolbar", "insert-image.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_INSERT_LINK, "toolbar", "insert-link.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, "toolbar", "message-new.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_PLUGINS, "toolbar", "plugins.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_TYPING, "toolbar", "typing.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TOOLBAR_PENDING, "status", "message-pending.png", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
	
	{ PIDGIN_STOCK_TRAY_AVAILABLE, "tray", "tray-online.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_AWAY, "tray", "tray-away.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_BUSY, "tray", "tray-busy.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_XA, "tray", "tray-extended-away.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_OFFLINE, "tray", "tray-offline.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_CONNECT, "tray", "tray-connecting.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_TRAY_PENDING, "tray", "tray-message.png", TRUE, TRUE, FALSE, FALSE, FALSE, FALSE }
};

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "pidgin"))
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", base, NULL);
	else
	{
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", dir,
									base, NULL);
	}

	return filename;
}

static void
add_sized_icon(GtkIconSet *iconset, GtkIconSize sizeid, const char *dir, 
	       gboolean rtl, const char *size, const char *file)
{
	char *filename;
	GtkIconSource *source;	

	filename = g_build_filename(DATADIR, "pixmaps", "pidgin", dir, size, file, NULL);
	source = gtk_icon_source_new();
        gtk_icon_source_set_filename(source, filename);
	gtk_icon_source_set_direction(source, GTK_TEXT_DIR_LTR);
        gtk_icon_source_set_direction_wildcarded(source, !rtl);
	gtk_icon_source_set_size(source, sizeid);
        gtk_icon_source_set_size_wildcarded(source, FALSE);
        gtk_icon_source_set_state_wildcarded(source, TRUE);
        gtk_icon_set_add_source(iconset, source);
	gtk_icon_source_free(source);

	if (sizeid == gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL)) {
		source = gtk_icon_source_new();
	        gtk_icon_source_set_filename(source, filename);
        	gtk_icon_source_set_direction_wildcarded(source, TRUE);
	        gtk_icon_source_set_size(source, GTK_ICON_SIZE_MENU);
	        gtk_icon_source_set_size_wildcarded(source, FALSE);
        	gtk_icon_source_set_state_wildcarded(source, TRUE);
	        gtk_icon_set_add_source(iconset, source);
	        gtk_icon_source_free(source);
	}
        g_free(filename);

       if (rtl) {
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", dir, size, "rtl", file, NULL);
                source = gtk_icon_source_new();
                gtk_icon_source_set_filename(source, filename);
                gtk_icon_source_set_direction(source, GTK_TEXT_DIR_RTL);
                gtk_icon_source_set_size(source, sizeid);
                gtk_icon_source_set_size_wildcarded(source, FALSE);
                gtk_icon_source_set_state_wildcarded(source, TRUE);
                gtk_icon_set_add_source(iconset, source);
		g_free(filename);
		gtk_icon_source_free(source);
        }


}

void
pidgin_stock_init(void)
{
	static gboolean stock_initted = FALSE;
	GtkIconFactory *icon_factory;
	size_t i;
	GtkWidget *win;
	GtkIconSize extra_small, small, medium, large, huge;
	
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
		GtkIconSource *source;
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

			source = gtk_icon_source_new();
			gtk_icon_source_set_filename(source, filename);
			gtk_icon_source_set_direction_wildcarded(source, TRUE);
			gtk_icon_source_set_size_wildcarded(source, TRUE);
			gtk_icon_source_set_state_wildcarded(source, TRUE);
			

			iconset = gtk_icon_set_new();
			gtk_icon_set_add_source(iconset, source);
			
			gtk_icon_source_free(source);
			g_free(filename);
		}

		gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);

		gtk_icon_set_unref(iconset);
	}

	/* register custom icon sizes */
	extra_small =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL, 16, 16);
	small =        gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_SMALL, 22, 22);
	medium =       gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MEDIUM, 32, 32);
	large =        gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_LARGE, 48, 48);
	huge =         gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_HUGE, 64, 64);

	for (i = 0; i < G_N_ELEMENTS(sized_stock_icons); i++)
	{
		GtkIconSet *iconset;

		iconset = gtk_icon_set_new();
		if (sized_stock_icons[i].extra_small)
			add_sized_icon(iconset, extra_small,
				       sized_stock_icons[i].dir, sized_stock_icons[i].rtl,
				       "16", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].small)
                        add_sized_icon(iconset, small,
				       sized_stock_icons[i].dir,  sized_stock_icons[i].rtl,
                                       "22", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].medium)
                        add_sized_icon(iconset, medium,
			               sized_stock_icons[i].dir,  sized_stock_icons[i].rtl,
                                       "32", sized_stock_icons[i].filename);
	       if (sized_stock_icons[i].large)
		       add_sized_icon(iconset, large,
                                      sized_stock_icons[i].dir, sized_stock_icons[i].rtl,
                                      "48", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].huge)
                        add_sized_icon(iconset, huge,
	                               sized_stock_icons[i].dir,  sized_stock_icons[i].rtl,
                                       "64", sized_stock_icons[i].filename);

		gtk_icon_factory_add(icon_factory, sized_stock_icons[i].name, iconset);
		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);
	g_object_unref(G_OBJECT(icon_factory));

	/* Register the stock items. */
	gtk_stock_add_static(stock_items, G_N_ELEMENTS(stock_items));
}
