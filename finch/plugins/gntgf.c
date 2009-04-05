/**
 * @file gntgf.c Minimal toaster plugin in Gnt.
 *
 * Copyright (C) 2006 Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>
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
 */


#include "internal.h"

#define PLUGIN_STATIC_NAME	GntGf

#define PREFS_PREFIX          "/plugins/gnt/gntgf"
#define PREFS_EVENT           PREFS_PREFIX "/events"
#define PREFS_EVENT_SIGNONF   PREFS_EVENT "/signonf"
#define PREFS_EVENT_IM_MSG    PREFS_EVENT "/immsg"
#define PREFS_EVENT_CHAT_MSG  PREFS_EVENT "/chatmsg"
#define PREFS_EVENT_CHAT_NICK PREFS_EVENT "/chatnick"
#define PREFS_BEEP            PREFS_PREFIX "/beep"

#define MAX_COLS	3

#ifdef HAVE_X11
#define PREFS_URGENT          PREFS_PREFIX "/urgent"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <glib.h>

#include <plugin.h>
#include <version.h>
#include <blist.h>
#include <conversation.h>
#include <debug.h>
#include <eventloop.h>
#include <util.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntlabel.h>
#include <gnttree.h>

#include "gntplugin.h"
#include "gntconv.h"

typedef struct
{
	GntWidget *window;
	int timer;
	int column;
} GntToast;

static GList *toasters;
static int gpsy[MAX_COLS];
static int gpsw[MAX_COLS];

static void
destroy_toaster(GntToast *toast)
{
	toasters = g_list_remove(toasters, toast);
	gnt_widget_destroy(toast->window);
	purple_timeout_remove(toast->timer);
	g_free(toast);
}

static gboolean
remove_toaster(GntToast *toast)
{
	GList *iter;
	int h;
	int col;
	int nwin[MAX_COLS];

	gnt_widget_get_size(toast->window, NULL, &h);
	gpsy[toast->column] -= h;
	col = toast->column;

	memset(&nwin, 0, sizeof(nwin));
	destroy_toaster(toast);

	for (iter = toasters; iter; iter = iter->next)
	{
		int x, y;
		toast = iter->data;
		nwin[toast->column]++;
		if (toast->column != col) continue;
		gnt_widget_get_position(toast->window, &x, &y);
		y += h;
		gnt_screen_move_widget(toast->window, x, y);
	}

	if (nwin[col] == 0)
		gpsw[col] = 0;

	return FALSE;
}

#ifdef HAVE_X11
static int
error_handler(Display *dpy, XErrorEvent *error)
{
	char buffer[1024];
	XGetErrorText(dpy, error->error_code, buffer, sizeof(buffer));
	purple_debug_error("gntgf", "Could not set urgent to the window: %s.\n", buffer);
	return 0;
}

static void
urgent(void)
{
	/* This is from deryni/tuomov's urgent_test.c */
	Display *dpy;
	Window id;
	const char *ids;
	XWMHints *hints;

	ids = getenv("WINDOWID");
	if (ids == NULL)
		return;
	
	id = atoi(ids);

	dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		return;

	XSetErrorHandler(error_handler);
	hints = XGetWMHints(dpy, id);
	if (hints) {
		hints->flags|=XUrgencyHint;
		XSetWMHints(dpy, id, hints);
		XFree(hints);
	}
	XSetErrorHandler(NULL);

	XFlush(dpy);
	XCloseDisplay(dpy);
}
#endif

static void
notify(PurpleConversation *conv, const char *fmt, ...)
{
	GntWidget *window;
	GntToast *toast;
	char *str;
	int h, w, i;
	va_list args;

	if (purple_prefs_get_bool(PREFS_BEEP))
		beep();

	if (conv != NULL) {
		FinchConv *fc = conv->ui_data;
		if (gnt_widget_has_focus(fc->window))
			return;
	}

#ifdef HAVE_X11
	if (purple_prefs_get_bool(PREFS_URGENT))
		urgent();
#endif

	window = gnt_vbox_new(FALSE);
	GNT_WIDGET_SET_FLAGS(window, GNT_WIDGET_TRANSIENT);
	GNT_WIDGET_UNSET_FLAGS(window, GNT_WIDGET_NO_BORDER);

	va_start(args, fmt);
	str = g_strdup_vprintf(fmt, args);
	va_end(args);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(str, GNT_TEXT_FLAG_HIGHLIGHT));

	g_free(str);
	gnt_widget_size_request(window);
	gnt_widget_get_size(window, &w, &h);
	for (i = 0; i < MAX_COLS && gpsy[i] + h >= getmaxy(stdscr) ; ++i)
		;
	if (i >= MAX_COLS) {
		purple_debug_warning("GntGf", "Dude, that's way too many popups\n");
		gnt_widget_destroy(window);
		return;
	}

	toast = g_new0(GntToast, 1);
	toast->window = window;
	toast->column = i;
	gpsy[i] += h;
	if (w > gpsw[i]) {
		if (i == 0)
			gpsw[i] = w;
		else
			gpsw[i] = gpsw[i - 1] + w + 1;
	}

	if (i == 0 || (w + gpsw[i - 1] >= getmaxx(stdscr))) {
		/* if it's going to be too far left, overlap. */
		gnt_widget_set_position(window, getmaxx(stdscr) - w - 1,
				getmaxy(stdscr) - gpsy[i] - 1);
	} else {
		gnt_widget_set_position(window, getmaxx(stdscr) - gpsw[i - 1] - w - 1,
				getmaxy(stdscr) - gpsy[i] - 1);
	}
	gnt_widget_draw(window);

	toast->timer = purple_timeout_add_seconds(4, (GSourceFunc)remove_toaster, toast);
	toasters = g_list_prepend(toasters, toast);
}

static void
buddy_signed_on(PurpleBuddy *buddy, gpointer null)
{
	if (purple_prefs_get_bool(PREFS_EVENT_SIGNONF))
		notify(NULL, _("%s just signed on"), purple_buddy_get_alias(buddy));
}

static void
buddy_signed_off(PurpleBuddy *buddy, gpointer null)
{
	if (purple_prefs_get_bool(PREFS_EVENT_SIGNONF))
		notify(NULL, _("%s just signed off"), purple_buddy_get_alias(buddy));
}

static void
received_im_msg(PurpleAccount *account, const char *sender, const char *msg,
		PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	if (purple_prefs_get_bool(PREFS_EVENT_IM_MSG))
		notify(conv, _("%s sent you a message"), sender);
}

static void
received_chat_msg(PurpleAccount *account, const char *sender, const char *msg,
		PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	const char *nick;

	if (flags & PURPLE_MESSAGE_WHISPER)
		return;
	
	nick = PURPLE_CONV_CHAT(conv)->nick;

	if (g_utf8_collate(sender, nick) == 0)
		return;

	if (purple_prefs_get_bool(PREFS_EVENT_CHAT_NICK) &&
			(purple_utf8_has_word(msg, nick)))
		notify(conv, _("%s said your nick in %s"), sender, purple_conversation_get_name(conv));
	else if (purple_prefs_get_bool(PREFS_EVENT_CHAT_MSG))
		notify(conv, _("%s sent a message in %s"), sender, purple_conversation_get_name(conv));
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on", plugin,
			PURPLE_CALLBACK(buddy_signed_on), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off", plugin,
			PURPLE_CALLBACK(buddy_signed_off), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", plugin,
			PURPLE_CALLBACK(received_im_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "received-chat-msg", plugin,
			PURPLE_CALLBACK(received_chat_msg), NULL);

	memset(&gpsy, 0, sizeof(gpsy));
	memset(&gpsw, 0, sizeof(gpsw));

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	while (toasters)
	{
		GntToast *toast = toasters->data;
		destroy_toaster(toast);
	}
	return TRUE;
}

static struct
{
	char *pref;
	char *display;
} prefs[] =
{
	{PREFS_EVENT_SIGNONF, N_("Buddy signs on/off")},
	{PREFS_EVENT_IM_MSG, N_("You receive an IM")},
	{PREFS_EVENT_CHAT_MSG, N_("Someone speaks in a chat")},
	{PREFS_EVENT_CHAT_NICK, N_("Someone says your name in a chat")},
	{NULL, NULL}
};

static void
pref_toggled(GntTree *tree, char *key, gpointer null)
{
	purple_prefs_set_bool(key, gnt_tree_get_choice(tree, key));
}

static void
toggle_option(GntCheckBox *check, gpointer str)
{
	purple_prefs_set_bool(str, gnt_check_box_get_checked(check));
}

static GntWidget *
config_frame(void)
{
	GntWidget *window, *tree, *check;
	int i;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_fill(GNT_BOX(window), TRUE);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new(_("Notify with a toaster when")));

	tree = gnt_tree_new();
	gnt_box_add_widget(GNT_BOX(window), tree);

	for (i = 0; prefs[i].pref; i++)
	{
		gnt_tree_add_choice(GNT_TREE(tree), prefs[i].pref,
				gnt_tree_create_row(GNT_TREE(tree), prefs[i].display), NULL, NULL);
		gnt_tree_set_choice(GNT_TREE(tree), prefs[i].pref,
				purple_prefs_get_bool(prefs[i].pref));
	}
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 40);
	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(pref_toggled), NULL);

	check = gnt_check_box_new(_("Beep too!"));
	gnt_check_box_set_checked(GNT_CHECK_BOX(check), purple_prefs_get_bool(PREFS_BEEP));
	g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(toggle_option), PREFS_BEEP);
	gnt_box_add_widget(GNT_BOX(window), check);

#ifdef HAVE_X11
	check = gnt_check_box_new(_("Set URGENT for the terminal window."));
	gnt_check_box_set_checked(GNT_CHECK_BOX(check), purple_prefs_get_bool(PREFS_URGENT));
	g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(toggle_option), PREFS_URGENT);
	gnt_box_add_widget(GNT_BOX(window), check);
#endif

	return window;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	FINCH_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gntgf",
	N_("GntGf"),
	DISPLAY_VERSION,
	N_("Toaster plugin"),
	N_("Toaster plugin"),
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	config_frame,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gnt");
	
	purple_prefs_add_none("/plugins/gnt/gntgf");
	purple_prefs_add_none(PREFS_EVENT);

	purple_prefs_add_bool(PREFS_EVENT_SIGNONF, TRUE);
	purple_prefs_add_bool(PREFS_EVENT_IM_MSG, TRUE);
	purple_prefs_add_bool(PREFS_EVENT_CHAT_MSG, TRUE);
	purple_prefs_add_bool(PREFS_EVENT_CHAT_NICK, TRUE);

	purple_prefs_add_bool(PREFS_BEEP, TRUE);
#ifdef HAVE_X11
	purple_prefs_add_bool(PREFS_URGENT, FALSE);
#endif
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
