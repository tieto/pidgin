/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define HISTORY_PLUGIN_ID "gtk-history"

#define HISTORY_SIZE (4 * 1024)

static gboolean _scroll_imhtml_to_end(gpointer data)
{
	GtkIMHtml *imhtml = data;
	gtk_imhtml_scroll_to_end(GTK_IMHTML(imhtml));
	g_object_unref(G_OBJECT(imhtml));
	return FALSE;
}

static void historize(GaimConversation *c)
{
	GaimAccount *account = gaim_conversation_get_account(c);
	const char *name = gaim_conversation_get_name(c);
	GaimConversationType convtype;
	GList *logs = NULL;
	const char *alias = name;
	guint flags;
	char *history;
	GaimGtkConversation *gtkconv;
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
	time_t tm;
	char day[64];
	char *header;

	convtype = gaim_conversation_get_type(c);
	if (convtype == GAIM_CONV_IM)
	{
		GSList *buddies;
		GSList *cur;

		/* Find buddies for this conversation. */
	        buddies = gaim_find_buddies(account, name);

		/* If we found at least one buddy, save the first buddy's alias. */
		if (buddies != NULL)
			alias = gaim_buddy_get_contact_alias((GaimBuddy *)buddies->data);

	        for (cur = buddies; cur != NULL; cur = cur->next)
	        {
	                GaimBlistNode *node = cur->data;
	                if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
	                {
				GaimBlistNode *node2;

				alias = gaim_buddy_get_contact_alias((GaimBuddy *)node);

				/* We've found a buddy that matches this conversation.  It's part of a
				 * GaimContact with more than one GaimBuddy.  Loop through the GaimBuddies
				 * in the contact and get all the logs. */
				for (node2 = node->parent->child ; node2 != NULL ; node2 = node2->next)
				{
					logs = g_list_concat(
						gaim_log_get_logs(GAIM_LOG_IM,
							gaim_buddy_get_name((GaimBuddy *)node2),
							gaim_buddy_get_account((GaimBuddy *)node2)),
						logs);
				}
				break;
	                }
	        }
	        g_slist_free(buddies);

		if (logs == NULL)
			logs = gaim_log_get_logs(GAIM_LOG_IM, name, account);
		else
        		logs = g_list_sort(logs, gaim_log_compare);
	}
	else if (convtype == GAIM_CONV_CHAT)
		logs = gaim_log_get_logs(GAIM_LOG_CHAT, name, account);

	if (logs == NULL)
		return;

	history = gaim_log_read((GaimLog*)logs->data, &flags);
	gtkconv = GAIM_GTK_CONVERSATION(c);
	if (flags & GAIM_LOG_READ_NO_NEWLINE)
		options |= GTK_IMHTML_NO_NEWLINE;

	tm = ((GaimLog *)logs->data)->time;
	gaim_strftime(day, sizeof(day), "%c", localtime(&tm));
	header = g_strdup_printf("<b>Conversation with %s on %s:</b><br>", alias, day);
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), header, options);
	g_free(header);

	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), history, options);
	g_free(history);

	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<hr>", options);

	g_object_ref(G_OBJECT(gtkconv->imhtml));
	g_idle_add(_scroll_imhtml_to_end, gtkconv->imhtml);

	g_list_foreach(logs, (GFunc)gaim_log_free, NULL);
	g_list_free(logs);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_conversations_get_handle(),
						"conversation-created",
						plugin, GAIM_CALLBACK(historize), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("History"),
	VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert the last conversation into the current conversation."),
	"Sean Egan <bj91704@binghamton.edu>",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
