/*
 * Purple buddy notification plugin.
 *
 * Copyright (C) 2000-2001, Eric Warmenhoven (original code)
 * Copyright (C) 2002, Etan Reisner <deryni@eden.rutgers.edu> (rewritten code)
 * Copyright (C) 2003, Christian Hammond (update for changed API)
 * Copyright (C) 2003, Brian Tarricone <bjt23@cornell.edu> (mostly rewritten)
 * Copyright (C) 2003, Mark Doliner (minor cleanup)
 * Copyright (C) 2003, Etan Reisner (largely rewritten again)
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

/* TODO
 * 22:22:17 <seanegan> deryni: speaking of notify.c... you know what else
 * might be a neat feature?
 * 22:22:30 <seanegan> Changing the window icon.
 * 22:23:25 <deryni> seanegan: To what?
 * 22:23:42 <seanegan> deryni: I dunno. Flash it between the regular icon and
 * blank or something.
 * 22:23:53 <deryni> Also I think purple might re-set that sort of frequently,
 * but I'd have to look.
 * 22:25:16 <seanegan> deryni: I keep my conversations in one workspace and am
 * frequently in an another, and the icon flashing in the pager would be a
 * neat visual clue.
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
 *
 * Etan again, 12 August 2003:
 *  -Better use of the new xml prefs
 *  -Removed all bitmask stuff
 *  -Even better pref change handling
 *  -Removed unnecessary functions
 *  -Reworking of notification/unnotification stuff
 *  -Header file include cleanup
 *  -General code cleanup
 *
 * Etan yet again, 04 April 2004:
 *  -Re-added Urgent option
 *  -Re-added unnotify on focus option (still needs work, as it will only
 *  react to focus-in events when the entry or history widgets are focused)
 *
 * Sean, 08 January, 2005:
 *  -Added Raise option, formally in Purple proper
 */

#include "internal.h"
#include "pidgin.h"
#include "gtkprefs.h"

#include "conversation.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"
#include "debug.h"

#include "gtk3compat.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#define NOTIFY_PLUGIN_ID "gtk-x11-notify"

static PurplePlugin *my_plugin = NULL;
#ifdef HAVE_X11
static GdkAtom _Cardinal = GDK_NONE;
static GdkAtom _PurpleUnseenCount = GDK_NONE;
#endif

/* notification set/unset */
static int notify(PurpleConversation *conv, gboolean increment);
static void notify_win(PidginConvWindow *purplewin, PurpleConversation *conv);
static void unnotify(PurpleConversation *conv, gboolean reset);
static int unnotify_cb(GtkWidget *widget, gpointer data,
                       PurpleConversation *conv);

static void apply_method(void);
static void apply_notify(void);

/* string function */
static void handle_string(PidginConvWindow *purplewin);

/* count_title function */
static void handle_count_title(PidginConvWindow *purplewin);

/* count_xprop function */
static void handle_count_xprop(PidginConvWindow *purplewin);

/* urgent function */
static void handle_urgent(PidginConvWindow *purplewin, gboolean set);

/* raise function */
static void handle_raise(PidginConvWindow *purplewin);

/* present function */
static void handle_present(PurpleConversation *conv);

/****************************************/
/* Begin doing stuff below this line... */
/****************************************/
static guint
count_messages(PidginConvWindow *purplewin)
{
	guint count = 0;
	GList *convs = NULL, *l;

	for (convs = purplewin->gtkconvs; convs != NULL; convs = convs->next) {
		PidginConversation *conv = convs->data;
		for (l = conv->convs; l != NULL; l = l->next) {
			count += GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l->data), "notify-message-count"));
		}
	}

	return count;
}

static int
notify(PurpleConversation *conv, gboolean increment)
{
	gint count;
	gboolean has_focus;
	PidginConvWindow *purplewin = NULL;

	if (conv == NULL || PIDGIN_CONVERSATION(conv) == NULL)
		return 0;

	/* We want to remove the notifications, but not reset the counter */
	unnotify(conv, FALSE);

	purplewin = PIDGIN_CONVERSATION(conv)->win;

	/* If we aren't doing notifications for this type of conversation, return */
	if ((PURPLE_IS_IM_CONVERSATION(conv) &&
	     !purple_prefs_get_bool("/plugins/gtk/X11/notify/type_im")) ||
	    (PURPLE_IS_CHAT_CONVERSATION(conv) &&
	     !purple_prefs_get_bool("/plugins/gtk/X11/notify/type_chat")))
		return 0;

	g_object_get(G_OBJECT(purplewin->window),
	             "has-toplevel-focus", &has_focus, NULL);

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/type_focused") ||
	    !has_focus) {
		if (increment) {
			count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "notify-message-count"));
			count++;
			g_object_set_data(G_OBJECT(conv), "notify-message-count", GINT_TO_POINTER(count));
		}

		notify_win(purplewin, conv);
	}

	return 0;
}

static void
notify_win(PidginConvWindow *purplewin, PurpleConversation *conv)
{
	if (count_messages(purplewin) <= 0)
		return;

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_count"))
		handle_count_title(purplewin);
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_count_xprop"))
		handle_count_xprop(purplewin);
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_string"))
		handle_string(purplewin);
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_urgent"))
		handle_urgent(purplewin, TRUE);
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_raise"))
		handle_raise(purplewin);
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/method_present"))
		handle_present(conv);
}

static void
unnotify(PurpleConversation *conv, gboolean reset)
{
	PurpleConversation *active_conv = NULL;
	PidginConvWindow *purplewin = NULL;

	g_return_if_fail(conv != NULL);
	if (PIDGIN_CONVERSATION(conv) == NULL)
		return;

	purplewin = PIDGIN_CONVERSATION(conv)->win;
	active_conv = pidgin_conv_window_get_active_conversation(purplewin);

	/* reset the conversation window title */
	purple_conversation_autoset_title(active_conv);

	if (reset) {
		/* Only need to actually remove the urgent hinting here, since
		 * removing it just to have it readded in re-notify is an
		 * unnecessary couple extra RTs to the server */
		handle_urgent(purplewin, FALSE);
		g_object_set_data(G_OBJECT(conv), "notify-message-count", GINT_TO_POINTER(0));
		/* Same logic as for the urgent hint, xprops are also a RT.
		 * This needs to go here so that it gets the updated message
		 * count. */
		handle_count_xprop(purplewin);
	}

	return;
}

static int
unnotify_cb(GtkWidget *widget, gpointer data, PurpleConversation *conv)
{
	if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "notify-message-count")) != 0)
		unnotify(conv, TRUE);

	return 0;
}

static gboolean
message_displayed_cb(PurpleConversation *conv, PurpleMessage *msg, gpointer _unused)
{
	PurpleMessageFlags flags = purple_message_get_flags(msg);

	/* Ignore anything that's not a received message or a system message */
	if (!(flags & (PURPLE_MESSAGE_RECV|PURPLE_MESSAGE_SYSTEM)))
		return FALSE;
	/* Don't highlight for delayed messages */
	if ((flags & PURPLE_MESSAGE_RECV) && (flags & PURPLE_MESSAGE_DELAYED))
		return FALSE;
	/* Check whether to highlight for system message for either chat or IM */
	if (flags & PURPLE_MESSAGE_SYSTEM) {
		if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
			if (!purple_prefs_get_bool("/plugins/gtk/X11/notify/type_chat_sys"))
				return FALSE;
		} else if (PURPLE_IS_IM_CONVERSATION(conv)) {
			if (!purple_prefs_get_bool("/plugins/gtk/X11/notify/type_im_sys"))
				return FALSE;
		} else {
			/* System message not from chat or IM, ignore */
			return FALSE;
		}
	}
	
	/* If it's a chat, check if we should only highlight when nick is mentioned */
	if ((PURPLE_IS_CHAT_CONVERSATION(conv) &&
	     purple_prefs_get_bool("/plugins/gtk/X11/notify/type_chat_nick") &&
	     !(flags & PURPLE_MESSAGE_NICK)))
	    return FALSE;

	/* Nothing speaks against notifying, do so */
	notify(conv, TRUE);

	return FALSE;
}

static void
im_sent_im(PurpleAccount *account, PurpleMessage *msg, gpointer _unused)
{
	PurpleIMConversation *im = NULL;

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_send")) {
		im = purple_conversations_find_im_with_account(
			purple_message_get_recipient(msg), account);
		unnotify(PURPLE_CONVERSATION(im), TRUE);
	}
}

static void
chat_sent_im(PurpleAccount *account, PurpleMessage *msg, int id)
{
	PurpleChatConversation *chat = NULL;

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_send")) {
		chat = purple_conversations_find_chat(purple_account_get_connection(account), id);
		unnotify(PURPLE_CONVERSATION(chat), TRUE);
	}
}

static int
attach_signals(PurpleConversation *conv)
{
	PidginConversation *gtkconv = NULL;
	GSList *webview_ids = NULL, *entry_ids = NULL;
	guint id;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv) {
		purple_debug_misc("notify", "Failed to find gtkconv\n");
		return 0;
	}

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_focus")) {
		/* TODO should really find a way to make this work no matter
		 * where the focus is inside the conv window, without having
		 * to bind to focus-in-event on the g(d|t)kwindow */
		/* try setting the signal on the focus-in-event for
		 * gtkwin->notebook->container? */
		id = g_signal_connect(G_OBJECT(gtkconv->entry), "focus-in-event",
		                      G_CALLBACK(unnotify_cb), conv);
		entry_ids = g_slist_append(entry_ids, GUINT_TO_POINTER(id));

		id = g_signal_connect(G_OBJECT(gtkconv->webview), "focus-in-event",
		                      G_CALLBACK(unnotify_cb), conv);
		webview_ids = g_slist_append(webview_ids, GUINT_TO_POINTER(id));
	}

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_click")) {
		/* TODO similarly should really find a way to allow for
		 * clicking in other places of the window */
		id = g_signal_connect(G_OBJECT(gtkconv->entry), "button-press-event",
		                      G_CALLBACK(unnotify_cb), conv);
		entry_ids = g_slist_append(entry_ids, GUINT_TO_POINTER(id));

		id = g_signal_connect(G_OBJECT(gtkconv->webview), "button-press-event",
		                      G_CALLBACK(unnotify_cb), conv);
		webview_ids = g_slist_append(webview_ids, GUINT_TO_POINTER(id));
	}

	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_type")) {
		id = g_signal_connect(G_OBJECT(gtkconv->entry), "key-press-event",
		                      G_CALLBACK(unnotify_cb), conv);
		entry_ids = g_slist_append(entry_ids, GUINT_TO_POINTER(id));
	}

	g_object_set_data(G_OBJECT(conv), "notify-webview-signals", webview_ids);
	g_object_set_data(G_OBJECT(conv), "notify-entry-signals", entry_ids);

	return 0;
}

static void
detach_signals(PurpleConversation *conv)
{
	PidginConversation *gtkconv = NULL;
	GSList *ids = NULL, *l;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return;

	ids = g_object_get_data(G_OBJECT(conv), "notify-webview-signals");
	for (l = ids; l != NULL; l = l->next)
		g_signal_handler_disconnect(gtkconv->webview, GPOINTER_TO_INT(l->data));
	g_slist_free(ids);

	ids = g_object_get_data(G_OBJECT(conv), "notify-entry-signals");
	for (l = ids; l != NULL; l = l->next)
		g_signal_handler_disconnect(gtkconv->entry, GPOINTER_TO_INT(l->data));
	g_slist_free(ids);

	g_object_set_data(G_OBJECT(conv), "notify-message-count", GINT_TO_POINTER(0));

	g_object_set_data(G_OBJECT(conv), "notify-webview-signals", NULL);
	g_object_set_data(G_OBJECT(conv), "notify-entry-signals", NULL);
}

static void
conv_created(PurpleConversation *conv)
{
	g_object_set_data(G_OBJECT(conv), "notify-message-count",
	                           GINT_TO_POINTER(0));

	/* always attach the signals, notify() will take care of conversation
	 * type checking */
	attach_signals(conv);
}

static void
conv_switched(PurpleConversation *conv)
{
#if 0
	PidginConvWindow *purplewin = purple_conversation_get_window(new_conv);
#endif

	/*
	 * If the conversation was switched, then make sure we re-notify
	 * because Purple will have overwritten our custom window title.
	 */
	notify(conv, FALSE);

#if 0
	printf("conv_switched - %p - %p\n", old_conv, new_conv);
	printf("count - %d\n", count_messages(purplewin));
	if (purple_prefs_get_bool("/plugins/gtk/X11/notify/notify_switch"))
		unnotify(new_conv, FALSE);
	else {
		/* if we don't have notification on the window then we don't want to
		 * re-notify it */
		if (count_messages(purplewin))
			notify_win(purplewin);
	}
#endif
}

static void
deleting_conv(PurpleConversation *conv)
{
	PidginConvWindow *purplewin = NULL;
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	if (gtkconv == NULL)
		return;

	detach_signals(conv);

	purplewin = gtkconv->win;

	handle_urgent(purplewin, FALSE);
	g_object_set_data(G_OBJECT(conv), "notify-message-count", GINT_TO_POINTER(0));

	return;

#if 0
	/* i think this line crashes */
	if (count_messages(purplewin))
		notify_win(purplewin);
#endif
}

#if 0
static void
conversation_dragging(PurpleConversation *active_conv,
                        PidginConvWindow *old_purplewin,
                        PidginConvWindow *new_purplewin)
{
	if (old_purplewin != new_purplewin) {
		if (old_purplewin == NULL) {
			/*
			purple_conversation_autoset_title(active_conv);
			handle_urgent(new_purplewin, FALSE);
				*/

			if (count_messages(new_purplewin))
				notify_win(new_purplewin);
		} else {
			printf("if else count = %d\n", count_messages(new_purplewin));
			printf("if else count = %d\n", count_messages(old_purplewin));
			/*
			PurpleConversation *old_active_conv = NULL;
			old_active_conv = purple_conversation_window_get_active_conversation(new_purplewin);

			purple_conversation_autoset_title(old_active_conv);
			handle_urgent(old_purplewin, FALSE);

			if (count_messages(old_purplewin))
				notify_win(old_purplewin);

			purple_conversation_autoset_title(active_conv);
			handle_urgent(new_purplewin, FALSE);

			if (count_messages(new_purplewin))
				notify_win(new_purplewin);
				*/
		}
	} else {
		printf("else count = %d\n", count_messages(new_purplewin));
		printf("else count = %d\n", count_messages(old_purplewin));
		/*
		purple_conversation_autoset_title(active_conv);
		handle_urgent(old_purplewin, FALSE);

		if (count_messages(old_purplewin))
			notify_win(old_purplewin);
			*/
	}
}
#endif

static void
handle_string(PidginConvWindow *purplewin)
{
	GtkWindow *window = NULL;
	gchar newtitle[256];

	g_return_if_fail(purplewin != NULL);

	window = GTK_WINDOW(purplewin->window);
	g_return_if_fail(window != NULL);

	g_snprintf(newtitle, sizeof(newtitle), "%s%s",
	           purple_prefs_get_string("/plugins/gtk/X11/notify/title_string"),
	           gtk_window_get_title(window));
	gtk_window_set_title(window, newtitle);
}

static void
handle_count_title(PidginConvWindow *purplewin)
{
	GtkWindow *window;
	char newtitle[256];

	g_return_if_fail(purplewin != NULL);

	window = GTK_WINDOW(purplewin->window);
	g_return_if_fail(window != NULL);

	g_snprintf(newtitle, sizeof(newtitle), "[%d] %s",
	           count_messages(purplewin), gtk_window_get_title(window));
	gtk_window_set_title(window, newtitle);
}

static void
handle_count_xprop(PidginConvWindow *purplewin)
{
#ifdef HAVE_X11
	guint count;
	GtkWidget *window;
	GdkWindow *gdkwin;

	window = purplewin->window;
	g_return_if_fail(window != NULL);

	if (_PurpleUnseenCount == GDK_NONE) {
		_PurpleUnseenCount = gdk_atom_intern("_PIDGIN_UNSEEN_COUNT", FALSE);
	}

	if (_Cardinal == GDK_NONE) {
		_Cardinal = gdk_atom_intern("CARDINAL", FALSE);
	}

	count = count_messages(purplewin);
	gdkwin = gtk_widget_get_window(window);

	gdk_property_change(gdkwin, _PurpleUnseenCount, _Cardinal, 32,
	                    GDK_PROP_MODE_REPLACE, (guchar *) &count, 1);
#endif
}

static void
handle_urgent(PidginConvWindow *purplewin, gboolean set)
{
	g_return_if_fail(purplewin != NULL);
	g_return_if_fail(purplewin->window != NULL);

	pidgin_set_urgent(GTK_WINDOW(purplewin->window), set);
}

static void
handle_raise(PidginConvWindow *purplewin)
{
	pidgin_conv_window_raise(purplewin);
}

static void
handle_present(PurpleConversation *conv)
{
	if (pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv)))
		return;

	purple_conversation_present(conv);
}

static void
apply_method()
{
	GList *convs;

	for (convs = purple_conversations_get_all(); convs != NULL;
	     convs = convs->next) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* remove notifications */
		unnotify(conv, FALSE);

		if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "notify-message-count")) != 0)
			/* reattach appropriate notifications */
			notify(conv, FALSE);
	}
}

static void
apply_notify()
{
	GList *convs = purple_conversations_get_all();

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* detach signals */
		detach_signals(conv);
		/* reattach appropriate signals */
		attach_signals(conv);

		convs = convs->next;
	}
}

static void
settings_changed_cb(const char *name, PurplePrefType type, gconstpointer val,
		gpointer data)
{
	if (g_str_has_prefix(name, "/plugins/gtk/X11/notify/method_")) {
		apply_method();
	} else if (g_str_has_prefix(name, "/plugins/gtk/X11/notify/notify_")) {
		apply_notify();
	}
}

static PurplePluginPrefFrame *
get_config_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	/* Notify For */

	pref = purple_plugin_pref_new_with_label(_("Notify For"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_im", _("_IM windows"));
	purple_plugin_pref_frame_add(frame, pref);

	/* TODO: Item should be enabled only when "IM windows" is on" */
	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_im_sys",
			_("\t_Notify for System messages"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_chat",
			_("C_hat windows"));
	purple_plugin_pref_frame_add(frame, pref);

	/* TODO: Item should be enabled only when "Chat windows" is on" */
	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_chat_nick",
			_("\t_Only when someone says your username"));
	purple_plugin_pref_frame_add(frame, pref);

	/* TODO: Item should be enabled only when "Chat windows" is on" */
	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_chat_sys",
			_("\tNotify for _System messages"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/type_focused",
			_("_Focused windows"));
	purple_plugin_pref_frame_add(frame, pref);

	/* Notification Methods */

	pref = purple_plugin_pref_new_with_label(_("Notification Methods"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_string",
			_("Prepend _string into window title:"));
	purple_plugin_pref_frame_add(frame, pref);

	/* TODO: Item should be enabled only when "Prepend string" is on" */
	pref = purple_plugin_pref_new_with_name(
			"/plugins/gtk/X11/notify/title_string");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_count",
			_("Insert c_ount of new messages into window title"));
	purple_plugin_pref_frame_add(frame, pref);

#ifdef HAVE_X11
	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_count_xprop",
			_("Insert count of new message into _X property"));
	purple_plugin_pref_frame_add(frame, pref);
#endif

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_urgent",
#ifdef HAVE_X11
			_("Set window manager \"_URGENT\" hint"));
#else
			_("_Flash window"));
#endif
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_raise",
			_("R_aise conversation window"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/method_present",
			/* Translators: "Present" as used here is a verb.
			 * The plugin presents the window to the user. */
			_("_Present conversation window"));
	purple_plugin_pref_frame_add(frame, pref);

	/* Notification Removals */

	pref = purple_plugin_pref_new_with_label(_("Notification Removal"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/notify_focus",
			_("Remove when conversation window _gains focus"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/notify_click",
			_("Remove when conversation window _receives click"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/notify_type",
			_("Remove when _typing in conversation window"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/X11/notify/notify_send",
			_("Remove when a _message gets sent"));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Etan Reisner <deryni@eden.rutgers.edu>",
		"Brian Tarricone <bjt23@cornell.edu>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",                   NOTIFY_PLUGIN_ID,
		"name",                 N_("Message Notification"),
		"version",              DISPLAY_VERSION,
		"category",             N_("Notification"),
		"summary",              N_("Provides a variety of ways of notifying "
		                           "you of unread messages."),
		"description",          N_("Provides a variety of ways of notifying "
		                           "you of unread messages."),
		"authors",              authors,
		"website",              PURPLE_WEBSITE,
		"abi-version",          PURPLE_ABI_VERSION,
		"pref-frame-cb",        get_config_frame,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	GList *convs = purple_conversations_get_all();
	void *conv_handle = purple_conversations_get_handle();
	void *gtk_conv_handle = pidgin_conversations_get_handle();

	my_plugin = plugin;

	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/X11");
	purple_prefs_add_none("/plugins/gtk/X11/notify");

	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_im", TRUE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_im_sys", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_chat", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_chat_nick", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_chat_sys", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/type_focused", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_string", FALSE);
	purple_prefs_add_string("/plugins/gtk/X11/notify/title_string", "(*)");
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_urgent", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_count", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_count_xprop", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_raise", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/method_present", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/notify_focus", TRUE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/notify_click", FALSE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/notify_type", TRUE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/notify_send", TRUE);
	purple_prefs_add_bool("/plugins/gtk/X11/notify/notify_switch", TRUE);

	purple_signal_connect(gtk_conv_handle, "displayed-im-msg", plugin,
	                    PURPLE_CALLBACK(message_displayed_cb), NULL);
	purple_signal_connect(gtk_conv_handle, "displayed-chat-msg", plugin,
	                    PURPLE_CALLBACK(message_displayed_cb), NULL);
	purple_signal_connect(gtk_conv_handle, "conversation-switched", plugin,
	                    PURPLE_CALLBACK(conv_switched), NULL);
	purple_signal_connect(conv_handle, "sent-im-msg", plugin,
	                    PURPLE_CALLBACK(im_sent_im), NULL);
	purple_signal_connect(conv_handle, "sent-chat-msg", plugin,
	                    PURPLE_CALLBACK(chat_sent_im), NULL);
	purple_signal_connect(conv_handle, "conversation-created", plugin,
	                    PURPLE_CALLBACK(conv_created), NULL);
	purple_signal_connect(conv_handle, "deleting-conversation", plugin,
	                    PURPLE_CALLBACK(deleting_conv), NULL);
#if 0
	purple_signal_connect(gtk_conv_handle, "conversation-dragging", plugin,
	                    PURPLE_CALLBACK(conversation_dragging), NULL);
#endif

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* attach signals */
		attach_signals(conv);

		convs = convs->next;
	}

	purple_prefs_connect_callback(plugin, "/plugins/gtk/X11/notify",
			settings_changed_cb, NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	GList *convs = purple_conversations_get_all();

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* kill signals */
		detach_signals(conv);

		convs = convs->next;
	}

	return TRUE;
}

PURPLE_PLUGIN_INIT(notify, plugin_query, plugin_load, plugin_unload);
