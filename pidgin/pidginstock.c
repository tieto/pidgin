/**
 * @file pidginstock.c GTK+ Stock resources
 * @ingroup pidgin
 */

/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "pidgin.h"
#include "prefs.h"

#include "gtkicon-theme-loader.h"
#include "theme-manager.h"

#include "pidginstock.h"

/**************************************************************************
 * Globals
 **************************************************************************/

static gboolean stock_initted = FALSE;
static GtkIconSize microscopic, extra_small, small, medium, large, huge;

/**************************************************************************
 * Structures
 **************************************************************************/

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] = {

	{ PIDGIN_STOCK_ACTION,          NULL,      GTK_STOCK_EXECUTE          },
#if GTK_CHECK_VERSION(2,6,0)
	{ PIDGIN_STOCK_ALIAS,           NULL,      GTK_STOCK_EDIT             },
#else
	{ PIDGIN_STOCK_ALIAS,           "buttons", "edit.png"                 },
#endif
	{ PIDGIN_STOCK_CHAT,            NULL,      GTK_STOCK_JUMP_TO          },
	{ PIDGIN_STOCK_CLEAR,           NULL,      GTK_STOCK_CLEAR            },
	{ PIDGIN_STOCK_CLOSE_TABS,      NULL,      GTK_STOCK_CLOSE            },
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
	{ PIDGIN_STOCK_IGNORE,          NULL,      GTK_STOCK_DIALOG_ERROR     },
	{ PIDGIN_STOCK_INVITE,          NULL,      GTK_STOCK_JUMP_TO          },
	{ PIDGIN_STOCK_MODIFY,          NULL,      GTK_STOCK_PREFERENCES      },
	{ PIDGIN_STOCK_ADD,             NULL,	   GTK_STOCK_ADD			  },
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
#if GTK_CHECK_VERSION(2,8,0)
	{ PIDGIN_STOCK_INFO,            NULL,      GTK_STOCK_INFO             },
#else
	{ PIDGIN_STOCK_INFO,            "buttons", "info.png"                 },
#endif
};

static const GtkStockItem stock_items[] =
{
	{ PIDGIN_STOCK_ALIAS,               N_("_Alias"),      0, 0, NULL },
	{ PIDGIN_STOCK_CHAT,                N_("_Join"),       0, 0, NULL },
	{ PIDGIN_STOCK_CLOSE_TABS,          N_("Close _tabs"), 0, 0, NULL },
	{ PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, N_("I_M"),         0, 0, NULL },
	{ PIDGIN_STOCK_TOOLBAR_USER_INFO,   N_("_Get Info"),   0, 0, NULL },
	{ PIDGIN_STOCK_INVITE,              N_("_Invite"),     0, 0, NULL },
	{ PIDGIN_STOCK_MODIFY,              N_("_Modify..."),  0, 0, NULL },
	{ PIDGIN_STOCK_ADD,                 N_("_Add..."),     0, 0, NULL },
	{ PIDGIN_STOCK_OPEN_MAIL,           N_("_Open Mail"),  0, 0, NULL },
	{ PIDGIN_STOCK_PAUSE,               N_("_Pause"),      0, 0, NULL },
	{ PIDGIN_STOCK_EDIT,                N_("_Edit"),       0, 0, NULL }
};

typedef struct {
	const char *name;
 	const char *dir;
	const char *filename;
	gboolean microscopic;
	gboolean extra_small;
	gboolean small;
	gboolean medium;
	gboolean large;
	gboolean huge;
	gboolean rtl;
	const char *translucent_name;
} SizedStockIcon;

const SizedStockIcon sized_stock_icons [] = {

	{ PIDGIN_STOCK_STATUS_IGNORED,  "emblems", "blocked.png",       FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_FOUNDER,  "emblems", "founder.png",       FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_OPERATOR, "emblems", "operator.png",      FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_HALFOP,   "emblems", "half-operator.png",	FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_VOICE,    "emblems", "voice.png",         FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },

	{ PIDGIN_STOCK_DIALOG_AUTH,     "dialogs", "auth.png",      FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_COOL,     "dialogs", "cool.png",      FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_ERROR,    "dialogs", "error.png",     FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_INFO,     "dialogs", "info.png",      FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_MAIL,     "dialogs", "mail.png",      FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_QUESTION, "dialogs", "question.png",  FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },
	{ PIDGIN_STOCK_DIALOG_WARNING,  "dialogs", "warning.png",   FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, NULL },

	{ PIDGIN_STOCK_ANIMATION_CONNECT0,  "animations", "process-working0.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT1,  "animations", "process-working1.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT2,  "animations", "process-working2.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT3,  "animations", "process-working3.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT4,  "animations", "process-working4.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT5,  "animations", "process-working5.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT6,  "animations", "process-working6.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT7,  "animations", "process-working7.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT8,  "animations", "process-working8.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT9,  "animations", "process-working9.png",  FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT10, "animations", "process-working10.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT11, "animations", "process-working11.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT12, "animations", "process-working12.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT13, "animations", "process-working13.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT14, "animations", "process-working14.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT15, "animations", "process-working15.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT16, "animations", "process-working16.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT17, "animations", "process-working17.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT18, "animations", "process-working18.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT19, "animations", "process-working19.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT20, "animations", "process-working20.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT21, "animations", "process-working21.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT22, "animations", "process-working22.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT23, "animations", "process-working23.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT24, "animations", "process-working24.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT25, "animations", "process-working25.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT26, "animations", "process-working26.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT27, "animations", "process-working27.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT28, "animations", "process-working28.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT29, "animations", "process-working29.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_CONNECT30, "animations", "process-working30.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },

	{ PIDGIN_STOCK_ANIMATION_TYPING0,  "animations", "typing0.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_TYPING1,  "animations", "typing1.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_TYPING2,  "animations", "typing2.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_TYPING3,  "animations", "typing3.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_TYPING4,  "animations", "typing4.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_ANIMATION_TYPING5,  "animations", "typing5.png", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },

	{ PIDGIN_STOCK_TOOLBAR_BGCOLOR,         "toolbar", "change-bgcolor.png", FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_BLOCK,           "emblems", "blocked.png",        FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_FGCOLOR,         "toolbar", "change-fgcolor.png", FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_SMILEY,          "toolbar", "emote-select.png",   FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_FONT_FACE,       "toolbar", "font-face.png",      FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER,    "toolbar", "font-size-down.png", FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_TEXT_LARGER,     "toolbar", "font-size-up.png",	 FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_INSERT,          "toolbar", "insert.png",         FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE,    "toolbar", "insert-image.png",	 FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_INSERT_LINK,     "toolbar", "insert-link.png",	 FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW,     "toolbar", "message-new.png",	 FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_PENDING,         "toolbar", "message-new.png",	 FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_PLUGINS,         "toolbar", "plugins.png",        FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_UNBLOCK,         "toolbar", "unblock.png",        FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR,   "toolbar", "select-avatar.png",  FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_SEND_FILE,       "toolbar", "send-file.png",      FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_TRANSFER,        "toolbar", "transfer.png",       FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
#ifdef USE_VV
	{ PIDGIN_STOCK_TOOLBAR_AUDIO_CALL,       "toolbar", "audio-call.png",       FALSE, TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_VIDEO_CALL,       "toolbar", "video-call.png",       FALSE, TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TOOLBAR_AUDIO_VIDEO_CALL, "toolbar", "audio-video-call.png", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
#endif
};

const SizedStockIcon sized_status_icons [] = {

	{ PIDGIN_STOCK_STATUS_AVAILABLE, "status",  "available.png",     TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, PIDGIN_STOCK_STATUS_AVAILABLE_I },
	{ PIDGIN_STOCK_STATUS_AWAY,      "status",  "away.png",          TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, PIDGIN_STOCK_STATUS_AWAY_I },
	{ PIDGIN_STOCK_STATUS_BUSY,      "status",  "busy.png",          TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, PIDGIN_STOCK_STATUS_BUSY_I },
	{ PIDGIN_STOCK_STATUS_CHAT,      "status",  "chat.png",          TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_INVISIBLE, "status",  "invisible.png",     TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_XA,        "status",  "extended-away.png", TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  PIDGIN_STOCK_STATUS_XA_I },
	{ PIDGIN_STOCK_STATUS_LOGIN,     "status",  "log-in.png",        TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  NULL },
	{ PIDGIN_STOCK_STATUS_LOGOUT,    "status",  "log-out.png",       TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  NULL },
	{ PIDGIN_STOCK_STATUS_OFFLINE,   "status",  "offline.png",       TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, PIDGIN_STOCK_STATUS_OFFLINE_I  },
	{ PIDGIN_STOCK_STATUS_PERSON,    "status",  "person.png",        TRUE, TRUE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_STATUS_MESSAGE,   "toolbar", "message-new.png",   TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },

	{ PIDGIN_STOCK_TRAY_AVAILABLE, "tray", "tray-online.png",        FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_INVISIBLE, "tray", "tray-invisible.png",     FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_AWAY,      "tray", "tray-away.png",          FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_BUSY,      "tray", "tray-busy.png",          FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_XA,        "tray", "tray-extended-away.png", FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_OFFLINE,   "tray", "tray-offline.png",       FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_CONNECT,   "tray", "tray-connecting.png",    FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_PENDING,   "tray", "tray-new-im.png",        FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL },
	{ PIDGIN_STOCK_TRAY_EMAIL,     "tray", "tray-message.png",       FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, NULL }
};

/*****************************************************************************
 * Private functions
 *****************************************************************************/

static gchar *
find_file_common(const char *name)
{
	gchar *filename;
#if GLIB_CHECK_VERSION(2,6,0)
	const gchar *userdir;
	const gchar * const *sysdirs;

	userdir = g_get_user_data_dir();
	filename = g_build_filename(userdir, name, NULL);
	if (g_file_test(filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free(filename);

	sysdirs = g_get_system_data_dirs();
	for (; *sysdirs; sysdirs++) {
		filename = g_build_filename(*sysdirs, name, NULL);
		if (g_file_test(filename, G_FILE_TEST_EXISTS))
			return filename;
		g_free(filename);
	}
#endif
	filename = g_build_filename(DATADIR, name, NULL);
	if (g_file_test(filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free(filename);
	return NULL;
}

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;
	char *ret;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "pidgin"))
		filename = g_build_filename("pixmaps", "pidgin", base, NULL);
	else
		filename = g_build_filename("pixmaps", "pidgin", dir, base, NULL);

	ret = find_file_common(filename);
	g_free(filename);
	return ret;
}


/* Altered from do_colorshift in gnome-panel */
static void
do_alphashift(GdkPixbuf *dest, GdkPixbuf *src)
{
	gint i, j;
	gint width, height, has_alpha, srcrowstride, destrowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	guchar a;

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	if (!has_alpha)
		return;

	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	srcrowstride = gdk_pixbuf_get_rowstride (src);
	destrowstride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*destrowstride;
		pixsrc = original_pixels + i*srcrowstride;
		for (j = 0; j < width; j++) {
			*(pixdest++) = *(pixsrc++);
			*(pixdest++) = *(pixsrc++);
			*(pixdest++) = *(pixsrc++);
			a = *(pixsrc++);
			*(pixdest++) = a / 2;
		}
	}
}

static gchar *
find_icon_file(PidginIconTheme *theme, const gchar *size, SizedStockIcon sized_icon, gboolean rtl)
{
	const gchar *file, *dir;
	gchar *file_full = NULL;
	gchar *tmp;

	if (theme != NULL) {
		file = pidgin_icon_theme_get_icon(PIDGIN_ICON_THEME(theme), sized_icon.name);
		dir = purple_theme_get_dir(PURPLE_THEME(theme));

		if (rtl)
			file_full = g_build_filename(dir, size, "rtl", file, NULL);
		else
			file_full = g_build_filename(dir, size, file, NULL);

		if (g_file_test(file_full, G_FILE_TEST_IS_REGULAR))
			return file_full;

		g_free(file_full);
	}

	if (rtl)
		tmp = g_build_filename("pixmaps", "pidgin", sized_icon.dir, size, "rtl", sized_icon.filename, NULL);
	else
		tmp = g_build_filename("pixmaps", "pidgin", sized_icon.dir, size, sized_icon.filename, NULL);

	file_full = find_file_common(tmp);
	g_free(tmp);
	return file_full;
}

static void
add_sized_icon(GtkIconSet *iconset, GtkIconSize sizeid, PidginIconTheme *theme,
		const char *size, SizedStockIcon sized_icon, gboolean translucent)
{
	char *filename;
	GtkIconSource *source;
	GdkPixbuf *pixbuf;

	filename = find_icon_file(theme, size, sized_icon, FALSE);
	g_return_if_fail(filename != NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	if (translucent)
		do_alphashift(pixbuf, pixbuf);

	source = gtk_icon_source_new();
	gtk_icon_source_set_pixbuf(source, pixbuf);
	gtk_icon_source_set_direction(source, GTK_TEXT_DIR_LTR);
	gtk_icon_source_set_direction_wildcarded(source, !sized_icon.rtl);
	gtk_icon_source_set_size(source, sizeid);
	gtk_icon_source_set_size_wildcarded(source, FALSE);
	gtk_icon_source_set_state_wildcarded(source, TRUE);
	gtk_icon_set_add_source(iconset, source);
	gtk_icon_source_free(source);

	if (sizeid == gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL)) {
		source = gtk_icon_source_new();
		gtk_icon_source_set_pixbuf(source, pixbuf);
		gtk_icon_source_set_direction_wildcarded(source, TRUE);
		gtk_icon_source_set_size(source, GTK_ICON_SIZE_MENU);
		gtk_icon_source_set_size_wildcarded(source, FALSE);
		gtk_icon_source_set_state_wildcarded(source, TRUE);
		gtk_icon_set_add_source(iconset, source);
		gtk_icon_source_free(source);
	}
	g_free(filename);
	g_object_unref(pixbuf);

	if (sized_icon.rtl) {
		filename = find_icon_file(theme, size, sized_icon, TRUE);
		g_return_if_fail(filename != NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		if (translucent)
			do_alphashift(pixbuf, pixbuf);

		source = gtk_icon_source_new();
		gtk_icon_source_set_pixbuf(source, pixbuf);
		gtk_icon_source_set_filename(source, filename);
		gtk_icon_source_set_direction(source, GTK_TEXT_DIR_RTL);
		gtk_icon_source_set_size(source, sizeid);
		gtk_icon_source_set_size_wildcarded(source, FALSE);
		gtk_icon_source_set_state_wildcarded(source, TRUE);
		gtk_icon_set_add_source(iconset, source);
		g_free(filename);
		g_object_unref(pixbuf);
		gtk_icon_source_free(source);
	}
}

static void
reload_settings(void)
{
#if GTK_CHECK_VERSION(2,4,0)
	GtkSettings *setting = NULL;
	setting = gtk_settings_get_default();
	gtk_rc_reset_styles(setting);
#endif
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

void
pidgin_stock_load_status_icon_theme(PidginStatusIconTheme *theme)
{
	GtkIconFactory *icon_factory;
	gint i;
	GtkIconSet *normal;
	GtkIconSet *translucent = NULL;
	GtkWidget *win;

	if (theme != NULL) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/status/icon-theme",
		                        purple_theme_get_name(PURPLE_THEME(theme)));
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir",
		                      purple_theme_get_dir(PURPLE_THEME(theme)));
	}
	else {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/status/icon-theme", "");
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir", "");
	}

	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	for (i = 0; i < G_N_ELEMENTS(sized_status_icons); i++)
	{
		normal = gtk_icon_set_new();
		if (sized_status_icons[i].translucent_name)
			translucent = gtk_icon_set_new();

#define ADD_SIZED_ICON(name, size) \
		if (sized_status_icons[i].name) { \
			add_sized_icon(normal, name, PIDGIN_ICON_THEME(theme), size, sized_status_icons[i], FALSE); \
			if (sized_status_icons[i].translucent_name) \
				add_sized_icon(translucent, name, PIDGIN_ICON_THEME(theme), size, sized_status_icons[i], TRUE); \
		}
		ADD_SIZED_ICON(microscopic, "11");
		ADD_SIZED_ICON(extra_small, "16");
		ADD_SIZED_ICON(small, "22");
		ADD_SIZED_ICON(medium, "32");
		ADD_SIZED_ICON(large, "48");
		ADD_SIZED_ICON(huge, "64");
#undef ADD_SIZED_ICON

		gtk_icon_factory_add(icon_factory, sized_status_icons[i].name, normal);
		gtk_icon_set_unref(normal);

		if (sized_status_icons[i].translucent_name) {
			gtk_icon_factory_add(icon_factory, sized_status_icons[i].translucent_name, translucent);
			gtk_icon_set_unref(translucent);
		}
	}

	gtk_widget_destroy(win);
	g_object_unref(G_OBJECT(icon_factory));
	reload_settings();
}

void
pidgin_stock_load_stock_icon_theme(PidginStockIconTheme *theme)
{
	GtkIconFactory *icon_factory;
	gint i;
	GtkWidget *win;

	if (theme != NULL) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/stock/icon-theme",
		                        purple_theme_get_name(PURPLE_THEME(theme)));
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir",
		                      purple_theme_get_dir(PURPLE_THEME(theme)));
	}
	else {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/stock/icon-theme", "");
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir", "");
	}

	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	/* All non-sized icons */
	for (i = 0; i < G_N_ELEMENTS(stock_icons); i++) {
		GtkIconSource *source;
		GtkIconSet *iconset;
		gchar *filename;

		if (stock_icons[i].dir == NULL) {
			/* GTK+ Stock icon */
			iconset = gtk_style_lookup_icon_set(gtk_widget_get_style(win),
					stock_icons[i].filename);
		} else {
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

	/* All non-status sized icons */
	for (i = 0; i < G_N_ELEMENTS(sized_stock_icons); i++)
	{
		GtkIconSet *iconset = gtk_icon_set_new();

#define ADD_SIZED_ICON(name, size) \
		if (sized_stock_icons[i].name) \
			add_sized_icon(iconset, name, PIDGIN_ICON_THEME(theme), size, sized_stock_icons[i], FALSE);
		ADD_SIZED_ICON(microscopic, "11");
		ADD_SIZED_ICON(extra_small, "16");
		ADD_SIZED_ICON(small, "22");
		ADD_SIZED_ICON(medium, "32");
		ADD_SIZED_ICON(large, "48");
		ADD_SIZED_ICON(huge, "64");
#undef ADD_SIZED_ICON

		gtk_icon_factory_add(icon_factory, sized_stock_icons[i].name, iconset);
		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);
	g_object_unref(G_OBJECT(icon_factory));
	reload_settings();
}

void
pidgin_stock_init(void)
{
	PidginIconThemeLoader *loader, *stockloader;
	const gchar *path = NULL;

	if (stock_initted)
		return;

	stock_initted = TRUE;

	/* Setup the status icon theme */
	loader = g_object_new(PIDGIN_TYPE_ICON_THEME_LOADER, "type", "status-icon", NULL);
	purple_theme_manager_register_type(PURPLE_THEME_LOADER(loader));
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/status");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/status/icon-theme", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir", "");

	stockloader = g_object_new(PIDGIN_TYPE_ICON_THEME_LOADER, "type", "stock-icon", NULL);
	purple_theme_manager_register_type(PURPLE_THEME_LOADER(stockloader));
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/stock");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/stock/icon-theme", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir", "");

	/* register custom icon sizes */
	microscopic =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC, 11, 11);
	extra_small =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL, 16, 16);
	small       =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_SMALL, 22, 22);
	medium      =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MEDIUM, 32, 32);
	large       =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_LARGE, 48, 48);
	huge        =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_HUGE, 64, 64);

	pidgin_stock_load_stock_icon_theme(NULL);

	/* Pre-load Status icon theme - this avoids a bug with displaying the correct icon in the tray, theme is destroyed after*/
	if (purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme") &&
	   (path = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir"))) {

		PidginStatusIconTheme *theme = PIDGIN_STATUS_ICON_THEME(purple_theme_loader_build(PURPLE_THEME_LOADER(loader), path));
		pidgin_stock_load_status_icon_theme(theme);
		if (theme)
			g_object_unref(G_OBJECT(theme));

	}
	else
		pidgin_stock_load_status_icon_theme(NULL);

	/* Register the stock items. */
	gtk_stock_add_static(stock_items, G_N_ELEMENTS(stock_items));
}

static void
pidgin_stock_icon_theme_class_init(PidginStockIconThemeClass *klass)
{
}

GType
pidgin_stock_icon_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (PidginStockIconThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_stock_icon_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginStockIconTheme),
			0, /* n_preallocs */
			NULL,
			NULL, /* value table */
		};
		type = g_type_register_static(PIDGIN_TYPE_ICON_THEME,
				"PidginStockIconTheme", &info, 0);
	}
	return type;
}
