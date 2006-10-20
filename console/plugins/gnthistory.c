/**
 * @file gnthistory.c Show log from previous conversation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Ripped from gtk/plugins/history.c */

#include "internal.h"
#include "gntgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gntplugin.h"

#define HISTORY_PLUGIN_ID "gnt-history"

#define HISTORY_SIZE (4 * 1024)

static void historize(GaimConversation *c)
{
	GaimAccount *account = gaim_conversation_get_account(c);
	const char *name = gaim_conversation_get_name(c);
	GaimConversationType convtype;
	GList *logs = NULL;
	const char *alias = name;
	GaimLogReadFlags flags;
	char *history;
	char *header;
	GaimMessageFlags mflag;

	convtype = gaim_conversation_get_type(c);
	if (convtype == GAIM_CONV_TYPE_IM)
	{
		GSList *buddies;
		GSList *cur;

		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!gaim_prefs_get_bool("/core/logging/log_ims"))
			return;

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
	else if (convtype == GAIM_CONV_TYPE_CHAT)
	{
		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!gaim_prefs_get_bool("/core/logging/log_chats"))
			return;

		logs = gaim_log_get_logs(GAIM_LOG_CHAT, name, account);
	}

	if (logs == NULL)
		return;

	mflag = GAIM_MESSAGE_NO_LOG | GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_DELAYED;
	history = gaim_log_read((GaimLog*)logs->data, &flags);

	header = g_strdup_printf(_("<b>Conversation with %s on %s:</b><br>"), alias,
							 gaim_date_format_full(localtime(&((GaimLog *)logs->data)->time)));
	gaim_conversation_write(c, "", header, mflag, time(NULL));
	g_free(header);

	if (flags & GAIM_LOG_READ_NO_NEWLINE)
		gaim_str_strip_char(history, '\n');
	gaim_conversation_write(c, "", history, mflag, time(NULL));
	g_free(history);

	gaim_conversation_write(c, "", "<hr>", mflag, time(NULL));

	g_list_foreach(logs, (GFunc)gaim_log_free, NULL);
	g_list_free(logs);
}

static void
history_prefs_check(GaimPlugin *plugin)
{
	if (!gaim_prefs_get_bool("/core/logging/log_ims") &&
	    !gaim_prefs_get_bool("/core/logging/log_chats"))
	{
		gaim_notify_warning(plugin, NULL, _("History Plugin Requires Logging"),
							_("Logging can be enabled from Tools -> Preferences -> Logging.\n\n"
							  "Enabling logs for instant messages and/or chats will activate "
							  "history for the same conversation type(s)."));
	}
}

static void history_prefs_cb(const char *name, GaimPrefType type,
							 gconstpointer val, gpointer data)
{
	history_prefs_check((GaimPlugin *)data);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_conversations_get_handle(),
						"conversation-created",
						plugin, GAIM_CALLBACK(historize), NULL);

	gaim_prefs_connect_callback(plugin, "/core/logging/log_ims",
								history_prefs_cb, plugin);
	gaim_prefs_connect_callback(plugin, "/core/logging/log_chats",
								history_prefs_cb, plugin);

	history_prefs_check(plugin);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("GntHistory"),
	VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert "
	   "the last conversation into the current conversation."),
	"Sean Egan <seanegan@gmail.com>\n"
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
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

GAIM_INIT_PLUGIN(gnthistory, init_plugin, info)

