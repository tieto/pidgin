/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "internal.h"
#include "pidgin.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkwebview.h"

#define HISTORY_PLUGIN_ID "gtk-history"

#define HISTORY_SIZE (4 * 1024)

static gboolean _scroll_webview_to_end(gpointer data)
{
	GtkWebView *webview = data;
	gtk_webview_scroll_to_end(GTK_WEBVIEW(webview), FALSE);
	g_object_unref(G_OBJECT(webview));
	return FALSE;
}

static void historize(PurpleConversation *c)
{
	PurpleAccount *account = purple_conversation_get_account(c);
	const char *name = purple_conversation_get_name(c);
	GList *logs = NULL;
	const char *alias = name;
	guint flags;
	char *history;
	PidginConversation *gtkconv;
#if 0
	/* FIXME: WebView has no options */
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
#endif
	char *header;
#if 0
	/* FIXME: WebView has no protocol setting */
	char *protocol;
#endif
	char *escaped_alias;
	const char *header_date;

	gtkconv = PIDGIN_CONVERSATION(c);
	g_return_if_fail(gtkconv != NULL);

	/* An IM which is the first active conversation. */
	g_return_if_fail(gtkconv->convs != NULL);
	if (PURPLE_IS_IM_CONVERSATION(c) && !gtkconv->convs->next)
	{
		GSList *buddies;
		GSList *cur;

		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/purple/logging/log_ims"))
			return;

		/* Find buddies for this conversation. */
		buddies = purple_find_buddies(account, name);

		/* If we found at least one buddy, save the first buddy's alias. */
		if (buddies != NULL)
			alias = purple_buddy_get_contact_alias((PurpleBuddy *)buddies->data);

		for (cur = buddies; cur != NULL; cur = cur->next)
		{
			PurpleBListNode *node = cur->data;
			PurpleBListNode *prev = purple_blist_node_get_sibling_prev(node);
			PurpleBListNode *next = purple_blist_node_get_sibling_next(node);
			if ((node != NULL) && ((prev != NULL) || (next != NULL)))
			{
				PurpleBListNode *node2;
				PurpleBListNode *parent = purple_blist_node_get_parent(node);
				PurpleBListNode *child = purple_blist_node_get_first_child(parent);

				alias = purple_buddy_get_contact_alias((PurpleBuddy *)node);

				/* We've found a buddy that matches this conversation.  It's part of a
				 * PurpleContact with more than one PurpleBuddy.  Loop through the PurpleBuddies
				 * in the contact and get all the logs. */
				for (node2 = child ; node2 != NULL ; node2 = purple_blist_node_get_sibling_next(node2))
				{
					logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM,
							purple_buddy_get_name((PurpleBuddy *)node2),
							purple_buddy_get_account((PurpleBuddy *)node2)),
							logs);
				}
				break;
			}
		}
		g_slist_free(buddies);

		if (logs == NULL)
			logs = purple_log_get_logs(PURPLE_LOG_IM, name, account);
		else
			logs = g_list_sort(logs, purple_log_compare);
	}
	else if (PURPLE_IS_CHAT_CONVERSATION(c))
	{
		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/purple/logging/log_chats"))
			return;

		logs = purple_log_get_logs(PURPLE_LOG_CHAT, name, account);
	}

	if (logs == NULL)
		return;

	history = purple_log_read((PurpleLog*)logs->data, &flags);
	gtkconv = PIDGIN_CONVERSATION(c);
#if 0
	/* FIXME: WebView has no options */
	if (flags & PURPLE_LOG_READ_NO_NEWLINE)
		options |= GTK_IMHTML_NO_NEWLINE;
#endif

#if 0
	/* FIXME: WebView has no protocol setting */
	protocol = g_strdup(gtk_imhtml_get_protocol_name(GTK_IMHTML(gtkconv->imhtml)));
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->imhtml),
			purple_account_get_protocol_name(((PurpleLog*)logs->data)->account));
#endif

#if 0
	/* TODO WebKit: Do this properly... */
	if (!gtk_webview_is_empty(GTK_WEBVIEW(gtkconv->webview)))
		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), "<BR>");
#endif

	escaped_alias = g_markup_escape_text(alias, -1);

	if (((PurpleLog *)logs->data)->tm)
		header_date = purple_date_format_full(((PurpleLog *)logs->data)->tm);
	else
		header_date = purple_date_format_full(localtime(&((PurpleLog *)logs->data)->time));

	header = g_strdup_printf(_("<b>Conversation with %s on %s:</b><br>"), escaped_alias, header_date);
	gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), header);
	g_free(header);
	g_free(escaped_alias);

	g_strchomp(history);
	gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), history);
	g_free(history);

	gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), "<hr>");

#if 0
	/* FIXME: WebView has no protocol setting */
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->imhtml), protocol);
	g_free(protocol);
#endif

	g_object_ref(G_OBJECT(gtkconv->webview));
	g_idle_add(_scroll_webview_to_end, gtkconv->webview);

	g_list_foreach(logs, (GFunc)purple_log_free, NULL);
	g_list_free(logs);
}

static void
history_prefs_check(PurplePlugin *plugin)
{
	if (!purple_prefs_get_bool("/purple/logging/log_ims") &&
	    !purple_prefs_get_bool("/purple/logging/log_chats"))
	{
		purple_notify_warning(plugin, NULL, _("History Plugin Requires Logging"),
							_("Logging can be enabled from Tools -> Preferences -> Logging.\n\n"
							  "Enabling logs for instant messages and/or chats will activate "
							  "history for the same conversation type(s)."));
	}
}

static void history_prefs_cb(const char *name, PurplePrefType type,
							 gconstpointer val, gpointer data)
{
	history_prefs_check((PurplePlugin *)data);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(),
						"conversation-created",
						plugin, PURPLE_CALLBACK(historize), NULL);
	/* XXX: Do we want to listen to pidgin's "conversation-displayed" signal? */

	purple_prefs_connect_callback(plugin, "/purple/logging/log_ims",
								history_prefs_cb, plugin);
	purple_prefs_connect_callback(plugin, "/purple/logging/log_chats",
								history_prefs_cb, plugin);

	history_prefs_check(plugin);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("History"),
	DISPLAY_VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert "
	   "the last conversation into the current conversation."),
	"Sean Egan <seanegan@gmail.com>",
	PURPLE_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
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
}

PURPLE_INIT_PLUGIN(history, init_plugin, info)
