/*
 * Gaim buddy notification plugin.
 *
 * Copyright (C) 2000-2001, Eric Warmenhoven (original code)
 * Copyright (C) 2002, Etan Reisner <deryni@eden.rutgers.edu> (rewritten code)
 * Copyright (C) 2003, Christian Hammond (update for changed API)
 * Copyright (C) 2003, Brian Tarricone <bjt23@cornell.edu> (mostly rewritten)
 * Copyright (C) 2003, Mark Doliner (minor cleanup)
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

/*
 * From Etan, 2002:
 *  -Added config dialog
 *  -Added control over notification method
 *  -Added control over when to release notification
 *
 *  -Added option to get notification for chats also
 *  -Cleaned up code
 *  -Added option to notify on click as it's own option
 *   rather then as what happens when on focus isn't clicked
 *  -Added apply button to change the denotification methods for
 *   open conversation windows
 *  -Fixed apply to conversations, count now keeps count across applies
 *  -Fixed(?) memory leak, and in the process fixed some stupidities 
 *  -Hit enter when done editing the title string entry box to save it
 *
 * Thanks to Carles Pina i Estany <carles@pinux.info>
 *   for count of new messages option
 * 
 * From Brian, 20 July 2003:
 *  -Use new xml prefs
 *  -Better handling of notification states tracking
 *  -Better pref change handling
 *  -Fixed a possible memleak and possible crash (rare)
 *  -Use gtk_window_get_title() rather than gtkwin->title
 *  -Other random fixes and cleanups
 */

#include "gtkinternal.h"

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"


#include "gtkplugin.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <glib.h>

#define NOTIFY_PLUGIN_ID "gtk-x11-notify"

#define OPT_TYPE_IM				((guint)0x00000001)
#define OPT_TYPE_CHAT			((guint)0x00000002)
#define OPT_NOTIFY_FOCUS		((guint)0x00000004)
#define OPT_NOTIFY_TYPE			((guint)0x00000008)
#define OPT_NOTIFY_IN_FOCUS		((guint)0x00000010)
#define OPT_NOTIFY_CLICK		((guint)0x00000020)
#define OPT_METHOD_STRING		((guint)0x00000040)
#define OPT_METHOD_QUOTE		((guint)0x00000080)
#define OPT_METHOD_URGENT		((guint)0x00000100)
#define OPT_METHOD_COUNT		((guint)0x00000200)
#define OPT_METHOD_STRING_CHNG	((guint)0x00000400)
#define STATE_IS_NOTIFIED		((guint)0x80000000)

#define TITLE_STR_BUFSIZE		256

#define GDATASTR				"notify-plugin-opts"
#define GDATASTRCNT				"notify-plugin-count"

static GaimPlugin *my_plugin = NULL;

static guint notify_opts = 0;
static gchar title_string[TITLE_STR_BUFSIZE+1];

/* notification set/unset */
static int notify(GaimConversation *c);
static void unnotify(GaimConversation *c);
static int unnotify_cb(GtkWidget *widget, gpointer data);

/* gtk widget callbacks for prefs panel */
static void options_toggle_cb(GtkWidget *widget, gpointer data);
static gboolean options_settitle_cb(GtkWidget *w, GdkEventFocus *evt, GtkWidget *entry);
static void options_toggle_title_cb(GtkWidget *w, GtkWidget *entry);
static void apply_options(int opt_chng);

/* string functions */
static void string_add(GtkWidget *widget);
static void string_remove(GtkWidget *widget);

/* count functions */
static void count_add(GtkWidget *widget);
static void count_remove(GtkWidget *widget);

/* quote functions */
static void quote_add(GtkWidget *widget);
static void quote_remove(GtkWidget *widget);

/* urgent functions */
static void urgent_add(GaimConversation *c);
static void urgent_remove(GaimConversation *c);

/****************************************/
/* Begin doing stuff below this line... */
/****************************************/

static int notify(GaimConversation *c) {
	GaimGtkWindow *gtkwin;
	Window focus_return;
	int revert_to_return;
	guint opts;
	gint count;

	gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	/* increment message counter */
	count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtkwin->window), GDATASTRCNT));
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER(count+1));

	/* if we aren't doing notifications for this type of convo, bail */
	if (((gaim_conversation_get_type(c) == GAIM_CONV_IM) && !(notify_opts & OPT_TYPE_IM)) ||
		((gaim_conversation_get_type(c) == GAIM_CONV_CHAT) && !(notify_opts & OPT_TYPE_CHAT)))
		return 0;

	XGetInputFocus(GDK_WINDOW_XDISPLAY(gtkwin->window->window), &focus_return, &revert_to_return);

	if ((notify_opts & OPT_NOTIFY_IN_FOCUS) || 
		(focus_return != GDK_WINDOW_XWINDOW(gtkwin->window->window))) {
		if (notify_opts & OPT_METHOD_STRING)
			string_add(gtkwin->window);
		if (notify_opts & OPT_METHOD_COUNT)
			count_add(gtkwin->window);
		if (notify_opts & OPT_METHOD_QUOTE)
			quote_add(gtkwin->window);
		if (notify_opts & OPT_METHOD_URGENT)
			urgent_add(c);
	}

	opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtkwin->window), GDATASTR));
	opts |= STATE_IS_NOTIFIED;
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER(opts));

	return 0;
}

static void unnotify(GaimConversation *c) {
	GaimGtkWindow *gtkwin;

	gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	urgent_remove(c);
	quote_remove(GTK_WIDGET(gtkwin->window));
	count_remove(GTK_WIDGET(gtkwin->window));
	string_remove(GTK_WIDGET(gtkwin->window));
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER((guint)0));
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER((guint)0));
}

static int unnotify_cb(GtkWidget *widget, gpointer data) {
	GaimConversation *c = g_object_get_data(G_OBJECT(widget), "user_data");

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "in unnotify_cb()\n");

	if (c)
		urgent_remove(c);
	quote_remove(widget);
	count_remove(widget);
	string_remove(widget);
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER((guint)0));
	g_object_set_data(G_OBJECT(widget), GDATASTRCNT, GINT_TO_POINTER((guint)0));

	return 0;
}

static void chat_recv_im(GaimConnection *gc, int id, char **who, char **text) {
	GaimConversation *c = gaim_find_chat(gc, id);

	if (c)
		notify(c);
	return;
}

static void chat_sent_im(GaimConnection *gc, int id, char **text) {
	GaimConversation *c = gaim_find_chat(gc, id);

	if (c)
		unnotify(c);
	return;
}

static int im_recv_im(GaimConnection *gc, char **who, char **what, void *m) {
	GaimConversation *c = gaim_find_conversation(*who);

	if (c)
		notify(c);
	return 0;
}

static int im_sent_im(GaimConnection *gc, char *who, char **what, void *m) {
	GaimConversation *c = gaim_find_conversation(who);

	if (c)
		unnotify(c);
	return 0;
}

static int attach_signals(GaimConversation *c) {
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;

	gtkconv = GAIM_GTK_CONVERSATION(c);
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	if (notify_opts & OPT_NOTIFY_FOCUS) {
		g_signal_connect(G_OBJECT(gtkwin->window), "focus-in-event", G_CALLBACK(unnotify_cb), NULL);
	}

	if (notify_opts & OPT_NOTIFY_CLICK) {
		g_signal_connect(G_OBJECT(gtkwin->window), "button_press_event", G_CALLBACK(unnotify_cb), NULL);
		g_signal_connect_swapped(G_OBJECT(gtkconv->imhtml), "button_press_event", G_CALLBACK(unnotify_cb), G_OBJECT(gtkwin->window));
		g_signal_connect_swapped(G_OBJECT(gtkconv->entry), "button_press_event", G_CALLBACK(unnotify_cb), G_OBJECT(gtkwin->window));
	}

	if (notify_opts & OPT_NOTIFY_TYPE) {
		g_signal_connect_swapped(G_OBJECT(gtkconv->entry), "key-press-event", G_CALLBACK(unnotify_cb), G_OBJECT(gtkwin->window));
	}

	return 0;
}

static void detach_signals(GaimConversation *c) {
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;
	
	gtkconv = GAIM_GTK_CONVERSATION(c);
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));
	
	if (notify_opts & OPT_NOTIFY_FOCUS) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkwin->window), unnotify_cb, NULL);
	}

	if (notify_opts & OPT_NOTIFY_CLICK) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkwin->window), unnotify_cb, NULL);
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkconv->imhtml), unnotify_cb, gtkwin->window);
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkconv->entry), unnotify_cb, gtkwin->window);
	}

	if (notify_opts & OPT_NOTIFY_TYPE) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkconv->entry), unnotify_cb, gtkwin->window);
	}
}

static void new_conv(char *who) {
	GaimConversation *c = gaim_find_conversation(who);
	GaimGtkWindow *gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER((guint)0));
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER((guint)0));

	if (c && (notify_opts & OPT_TYPE_IM))
		attach_signals(c);
}

static void chat_join(GaimConnection *gc, int id, char *room) {
	GaimConversation *c = gaim_find_chat(gc, id);
	GaimGtkWindow *gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER((guint)0));
	g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER((guint)0));

	if (c && (notify_opts & OPT_TYPE_CHAT))
		attach_signals(c);
}

static void string_add(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	gchar newtitle[256];
	const gchar *curtitle = gtk_window_get_title(win);
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "string_add(): opts=0x%04x\n", opts);

	if (opts & OPT_METHOD_STRING)
		return;

	if (!strstr(curtitle, title_string)) {
		if (opts & OPT_METHOD_COUNT) {
			char *p = strchr(curtitle, ']');
			int len1;
			if (!p)
				return;
			len1 = p-curtitle+2;
			memcpy(newtitle, curtitle, len1);
			strncpy(newtitle+len1, title_string, sizeof(newtitle)-len1);
			strncpy(newtitle+len1+strlen(title_string), curtitle+len1,
					sizeof(newtitle)-len1-strlen(title_string));
		} else if (opts & OPT_METHOD_QUOTE) {
			g_snprintf(newtitle, sizeof(newtitle), "\"%s%s", title_string, curtitle+1);
		} else {
			g_snprintf(newtitle, sizeof(newtitle), "%s%s", title_string, curtitle);
		}
		gtk_window_set_title(win, newtitle);
		gaim_debug(GAIM_DEBUG_INFO, "notify.c", "added string to window title\n");
	}

	opts |= OPT_METHOD_STRING;
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER(opts));
}

static void string_remove(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	gchar newtitle[256];
	const gchar *curtitle;
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "string_remove(): opts=0x%04x\n", opts);

	if (!(opts & OPT_METHOD_STRING))
		return;

	curtitle = gtk_window_get_title(win);

	if (strstr(curtitle, title_string)) {
		if (opts & OPT_METHOD_COUNT) {
			char *p = strchr(curtitle, ']');
			int len1;
			if (!p)
				return;
			len1 = p-curtitle+2;
			memcpy(newtitle, curtitle, len1);
			strncpy(newtitle+len1, curtitle+len1+strlen(title_string), sizeof(newtitle)-len1);
		} else if (opts & OPT_METHOD_QUOTE) {
			g_snprintf(newtitle, sizeof(newtitle), "\"%s", curtitle+strlen(title_string)+1);
		} else 
			strncpy(newtitle, curtitle+strlen(title_string), sizeof(newtitle));
		}
		
		gtk_window_set_title(win, newtitle);
		gaim_debug(GAIM_DEBUG_INFO, "notify.c", "removed string from window title (title now %s)\n", newtitle);
}

static void count_add(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	char newtitle[256];
	const gchar *curtitle;
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));
	gint curcount = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTRCNT));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "count_add(): opts=0x%04x\n", opts);

	if (curcount>0 && (opts & OPT_METHOD_COUNT))
		count_remove(widget);

	curtitle = gtk_window_get_title(win);
	if (opts & OPT_METHOD_QUOTE)
		g_snprintf(newtitle, sizeof(newtitle), "\"[%d] %s", curcount, curtitle+1);
	else
		g_snprintf(newtitle, sizeof(newtitle), "[%d] %s", curcount, curtitle);
	gtk_window_set_title(win, newtitle);

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "added count of %d to window\n", curcount);

	opts |= OPT_METHOD_COUNT;
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER(opts));
}

static void count_remove(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	char newtitle[256], *p;
	const gchar *curtitle;
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "count_remove(): opts=0x%04x\n", opts);

	if (!(opts & OPT_METHOD_COUNT))
		return;

	curtitle = gtk_window_get_title(win);

	p = strchr(curtitle, ']');

	if (p) {
		if (opts & OPT_METHOD_QUOTE)
			g_snprintf(newtitle, sizeof(newtitle), "\"%s", p+2);
		else
			g_snprintf(newtitle, sizeof(newtitle), p+2);
		gtk_window_set_title(win, newtitle);
		gaim_debug(GAIM_DEBUG_INFO, "notify.c", "removed count from title (title now %s)\n", newtitle);
	}

	opts &= ~OPT_METHOD_COUNT;
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER(opts));
}

static void quote_add(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	char newtitle[256];
	const gchar *curtitle = gtk_window_get_title(win);
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "quote_add(): opts=0x%04x\n", opts);

	if (opts & OPT_METHOD_QUOTE)
		return;

	if (*curtitle != '\"') {
		g_snprintf(newtitle, sizeof(newtitle), "\"%s\"", curtitle);
		gtk_window_set_title(win, newtitle);
		gaim_debug(GAIM_DEBUG_INFO, "notify.c", "quoted title\n");
	}

	opts |= OPT_METHOD_QUOTE;
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER(opts));
}

static void quote_remove(GtkWidget *widget) {
	GtkWindow *win = GTK_WINDOW(widget);
	char newtitle[512];
	const gchar *curtitle = gtk_window_get_title(win);
	guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), GDATASTR));

	gaim_debug(GAIM_DEBUG_INFO, "notify.c", "quote_remove(): opts=0x%04x\n", opts);

	if (!(opts & OPT_METHOD_QUOTE))
		return;

	if (*curtitle == '\"' && strlen(curtitle)-2<sizeof(newtitle)) {
		memcpy(newtitle, curtitle+1, strlen(curtitle)-2);
		newtitle[strlen(curtitle)-2] = 0;
		gtk_window_set_title(win, newtitle);
		gaim_debug(GAIM_DEBUG_INFO, "notify.c", "removed quotes from title (title now %s)\n", newtitle);
 	}

	opts &= ~OPT_METHOD_QUOTE;
	g_object_set_data(G_OBJECT(widget), GDATASTR, GINT_TO_POINTER(opts));
}

static void urgent_add(GaimConversation *c) {
	GaimGtkWindow *gtkwin;
	XWMHints *hints;

	gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

	hints = XGetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window));
	hints->flags |= XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window), hints);
	XFree(hints);
}

static void urgent_remove(GaimConversation *c) {
	GaimGtkConversation *gtkconv;
	const char *convo_placement = gaim_prefs_get_string("/core/conversations/placement");

	gtkconv = GAIM_GTK_CONVERSATION(c);

	if (!strcmp(convo_placement, "new")) {
		GaimGtkWindow *gtkwin;
		XWMHints *hints;

		gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));
		hints = XGetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window));

		if (hints->flags & XUrgencyHint) {
			hints->flags &= ~XUrgencyHint;
			XSetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window), hints);
		}
		XFree(hints);
	} else {
		if (gaim_conversation_get_type(c) == GAIM_CONV_CHAT) {
			GaimConversation *c = (GaimConversation *)gaim_get_chats()->data;
			GaimGtkWindow *gtkwin;
			GdkWindow *win;
			XWMHints *hints;

			gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

			win = gtkwin->window->window;

			hints = XGetWMHints(GDK_WINDOW_XDISPLAY(win), GDK_WINDOW_XWINDOW(win));
			if (hints->flags & XUrgencyHint) {
				hints->flags &= ~XUrgencyHint;
				XSetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window), hints);
			}
			XFree(hints);
		} else {
			GaimConversation *c;
			GaimGtkWindow *gtkwin;
			GdkWindow *win;
			XWMHints *hints;

			c = (GaimConversation *)gaim_get_ims()->data;
			gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));
			win = gtkwin->window->window;

			hints = XGetWMHints(GDK_WINDOW_XDISPLAY(win), GDK_WINDOW_XWINDOW(win));
			if (hints->flags & XUrgencyHint) {
				hints->flags &= ~XUrgencyHint;
				XSetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window), hints);
			}
			XFree(hints);
		}
	}
}

static void save_notify_prefs() {
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_im", notify_opts & OPT_TYPE_IM);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_chat", notify_opts & OPT_TYPE_CHAT);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_in_focus", notify_opts & OPT_NOTIFY_IN_FOCUS);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_focus", notify_opts & OPT_NOTIFY_FOCUS);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_click", notify_opts & OPT_NOTIFY_CLICK);	
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/notify_type", notify_opts & OPT_NOTIFY_TYPE);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/method_string", notify_opts & OPT_METHOD_STRING);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/method_quote", notify_opts & OPT_METHOD_QUOTE);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/method_urgent", notify_opts & OPT_METHOD_URGENT);
	gaim_prefs_set_bool("/plugins/gtk/X11/notify/method_count", notify_opts & OPT_METHOD_COUNT);

	gaim_prefs_set_string("/plugins/gtk/X11/notify/title_string", title_string);
}

static void load_notify_prefs() {
	notify_opts = 0;
	
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_im") ? OPT_TYPE_IM : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_chat") ? OPT_TYPE_CHAT : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_in_focus") ? OPT_NOTIFY_IN_FOCUS : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_focus") ? OPT_NOTIFY_FOCUS : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_click") ? OPT_NOTIFY_CLICK : 0);	
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_type") ? OPT_NOTIFY_TYPE : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_string") ? OPT_METHOD_STRING : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_quote") ? OPT_METHOD_QUOTE : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_urgent") ? OPT_METHOD_URGENT : 0);
	notify_opts |= (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_count") ? OPT_METHOD_COUNT : 0);

	strncpy(title_string, gaim_prefs_get_string("/plugins/gtk/X11/notify/title_string"), TITLE_STR_BUFSIZE);
}

static void options_toggle_cb(GtkWidget *w, gpointer data) {
	gint option = GPOINTER_TO_INT(data);

	notify_opts ^= option;

	/* save prefs and re-notify the windows */
 	save_notify_prefs();
	apply_options(option);
}

static gboolean options_settitle_cb(GtkWidget *w, GdkEventFocus *evt, GtkWidget *entry) {
 	GList *cnv = gaim_get_conversations();

	/* first we have to kill all the old strings */
	while (cnv) {
		GaimConversation *c = (GaimConversation *)cnv->data;
		GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));
		string_remove(gtkwin->window);
		cnv = cnv->next;
	}

	g_snprintf(title_string, sizeof(title_string), gtk_entry_get_text(GTK_ENTRY(entry)));

	/* save prefs and re-notify the windows */
	save_notify_prefs();
	apply_options(OPT_METHOD_STRING_CHNG);

	return FALSE;
}

static void options_toggle_title_cb(GtkWidget *w, GtkWidget *entry) {
	notify_opts ^= OPT_METHOD_STRING;

	if (notify_opts & OPT_METHOD_STRING)
		gtk_widget_set_sensitive(entry, TRUE);
	else
		gtk_widget_set_sensitive(entry, FALSE);

	save_notify_prefs();
	apply_options(OPT_METHOD_STRING);
}

static void apply_options(int opt_chng) {
	GList *cnv = gaim_get_conversations();

	/* option-setting handlers should have cleared out all window notifications */

	while (cnv) {
		GaimConversation *c = (GaimConversation *)cnv->data;
		GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));
		guint opts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtkwin->window), GDATASTR));

		/* kill signals */
		detach_signals(c);

		/*
		 * do a full notify if the option that changed was an OPT_TYPE_*
		 * either notify if it was enabled, or unnotify if it was disabled
		 */
		if (opt_chng==OPT_TYPE_IM || opt_chng==OPT_TYPE_CHAT) {
			if ((gaim_conversation_get_type(c)==GAIM_CONV_IM && (notify_opts & OPT_TYPE_IM)) ||
				(gaim_conversation_get_type(c)==GAIM_CONV_CHAT && (notify_opts & OPT_TYPE_CHAT))) {
				Window focus_return;
				int revert_to_return;
				
				XGetInputFocus(GDK_WINDOW_XDISPLAY(gtkwin->window->window),
						&focus_return, &revert_to_return);
				if ((notify_opts & OPT_NOTIFY_IN_FOCUS) ||
						focus_return != GDK_WINDOW_XWINDOW(gtkwin->window->window)) {
					if (notify_opts & OPT_METHOD_STRING)
						string_add(gtkwin->window);
					if (notify_opts & OPT_METHOD_COUNT)
						count_add(gtkwin->window);
					if (notify_opts & OPT_METHOD_QUOTE)
						quote_add(gtkwin->window);
					if (notify_opts & OPT_METHOD_URGENT)
						urgent_add(c);
				}
			} else {
				//don't simply call unnotify(), because that will kill the msg counter
				urgent_remove(c);
				quote_remove(gtkwin->window);
				count_remove(gtkwin->window);
				string_remove(gtkwin->window);
			}
		} else if (opts & STATE_IS_NOTIFIED) {
			//add/remove the status that was changed
			switch(opt_chng) {
				case OPT_METHOD_COUNT:
					if (notify_opts & OPT_METHOD_COUNT)
						count_add(gtkwin->window);
					else
						count_remove(gtkwin->window);
					break;
				case OPT_METHOD_QUOTE:
					if (notify_opts & OPT_METHOD_QUOTE)
						quote_add(gtkwin->window);
					else
						quote_remove(gtkwin->window);
					break;
				case OPT_METHOD_STRING:
					if (notify_opts & OPT_METHOD_STRING)
						string_add(gtkwin->window);
					else
						string_remove(gtkwin->window);
					break;
				case OPT_METHOD_URGENT:
					if (notify_opts & OPT_METHOD_URGENT)
						urgent_add(c);
					else
						urgent_remove(c);
					break;
				case OPT_METHOD_STRING_CHNG:
					string_add(gtkwin->window);
					break;
			}
		}

		/* attach new unnotification signals */
 		attach_signals(c);

 		cnv = cnv->next;
	}
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *toggle, *entry;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER (ret), 12);

	/*---------- "Notify For" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notify For"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	toggle = gtk_check_button_new_with_mnemonic(_("_IM windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_TYPE_IM);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_TYPE_IM));

	toggle = gtk_check_button_new_with_mnemonic(_("_Chat windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_TYPE_CHAT);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_TYPE_CHAT));

	/*---------- "Notification Methods" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notification Methods"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	toggle = gtk_check_button_new_with_mnemonic(_("Prepend _string into window title:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_METHOD_STRING);
	gtk_box_pack_start(GTK_BOX(hbox), toggle, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_entry_set_max_length(GTK_ENTRY(entry), 10);
	gtk_widget_set_sensitive(GTK_WIDGET(entry), notify_opts & OPT_METHOD_STRING);
	gtk_entry_set_text(GTK_ENTRY(entry), title_string);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_title_cb), entry);
	g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(options_settitle_cb), entry);

 	toggle = gtk_check_button_new_with_mnemonic(_("_Quote window title"));
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_METHOD_QUOTE);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_METHOD_QUOTE));

 	toggle = gtk_check_button_new_with_mnemonic(_("Set Window Manager \"_URGENT\" Hint"));
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_METHOD_URGENT);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_METHOD_URGENT));

 	toggle = gtk_check_button_new_with_mnemonic(_("Insert c_ount of new messages into window title"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_METHOD_COUNT);
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_METHOD_COUNT));

 	toggle = gtk_check_button_new_with_mnemonic(_("_Notify even if conversation is in focus"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_NOTIFY_IN_FOCUS);
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_NOTIFY_IN_FOCUS));

	/*---------- "Notification Methods" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notification Removal"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

 	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window gains _focus"));
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_NOTIFY_FOCUS);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_NOTIFY_FOCUS));

 	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window _receives click"));
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_NOTIFY_CLICK);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_NOTIFY_CLICK));

 	toggle = gtk_check_button_new_with_mnemonic(_("Remove when _typing in conversation window"));
 	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), notify_opts & OPT_NOTIFY_TYPE);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options_toggle_cb), GINT_TO_POINTER(OPT_NOTIFY_TYPE));

 	gtk_widget_show_all(ret);
 	return ret;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GList *cnv = gaim_get_conversations();

	my_plugin = plugin;

	load_notify_prefs();

	gaim_signal_connect(plugin, event_im_recv, im_recv_im, NULL);
	gaim_signal_connect(plugin, event_chat_recv, chat_recv_im, NULL);
	gaim_signal_connect(plugin, event_im_send, im_sent_im, NULL);
	gaim_signal_connect(plugin, event_chat_send, chat_sent_im, NULL);
	gaim_signal_connect(plugin, event_new_conversation, new_conv, NULL);
	gaim_signal_connect(plugin, event_chat_join, chat_join, NULL);

	while (cnv) {
		GaimConversation *c = (GaimConversation *)cnv->data;
		GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

		/* attach signals */
		attach_signals(c);
		/* zero out data */
		g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER((guint)0));
		g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER((guint)0));

		cnv = cnv->next;
	}

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	GList *cnv = gaim_get_conversations();

	while (cnv) {
		GaimConversation *c = (GaimConversation *)cnv->data;
		GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(c));

		/* kill signals */
		detach_signals(c);
		/* zero out data */
		g_object_set_data(G_OBJECT(gtkwin->window), GDATASTR, GINT_TO_POINTER((guint)0));
		g_object_set_data(G_OBJECT(gtkwin->window), GDATASTRCNT, GINT_TO_POINTER((guint)0));

		cnv = cnv->next;
	}

	return TRUE;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	NOTIFY_PLUGIN_ID,                                 /**< id             */
	N_("Message Notification"),                       /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides a variety of ways of notifying you of unread messages."),
	                                                  /**  description    */
	N_("Provides a variety of ways of notifying you of unread messages."),
	"Etan Reisner <deryni@eden.rutgers.edu>\n\t\t\tBrian Tarricone <bjt23@cornell.edu",
	                                                  /**< author         */
	GAIM_WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk");
	gaim_prefs_add_none("/plugins/gtk/X11");
	gaim_prefs_add_none("/plugins/gtk/X11/notify");

	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_im", TRUE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_chat", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_string", FALSE);
	gaim_prefs_add_string("/plugins/gtk/X11/notify/title_string", "(*)");
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_quote", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_urgent", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_count", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_in_focus", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_focus", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_click", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_type", TRUE);
}

GAIM_INIT_PLUGIN(notify, init_plugin, info)
