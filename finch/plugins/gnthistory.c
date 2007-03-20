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

static void historize(PurpleConversation *c)
{
	PurpleAccount *account = purple_conversation_get_account(c);
	const char *name = purple_conversation_get_name(c);
	PurpleConversationType convtype;
	GList *logs = NULL;
	const char *alias = name;
	PurpleLogReadFlags flags;
	char *history;
	char *header;
	PurpleMessageFlags mflag;

	convtype = purple_conversation_get_type(c);
	if (convtype == PURPLE_CONV_TYPE_IM)
	{
		GSList *buddies;
		GSList *cur;

		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/core/logging/log_ims"))
			return;

		/* Find buddies for this conversation. */
	        buddies = purple_find_buddies(account, name);

		/* If we found at least one buddy, save the first buddy's alias. */
		if (buddies != NULL)
			alias = purple_buddy_get_contact_alias((PurpleBuddy *)buddies->data);

	        for (cur = buddies; cur != NULL; cur = cur->next)
	        {
	                PurpleBlistNode *node = cur->data;
	                if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
	                {
				PurpleBlistNode *node2;

				alias = purple_buddy_get_contact_alias((PurpleBuddy *)node);

				/* We've found a buddy that matches this conversation.  It's part of a
				 * PurpleContact with more than one PurpleBuddy.  Loop through the PurpleBuddies
				 * in the contact and get all the logs. */
				for (node2 = node->parent->child ; node2 != NULL ; node2 = node2->next)
				{
					logs = g_list_concat(
						purple_log_get_logs(PURPLE_LOG_IM,
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
	else if (convtype == PURPLE_CONV_TYPE_CHAT)
	{
		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/core/logging/log_chats"))
			return;

		logs = purple_log_get_logs(PURPLE_LOG_CHAT, name, account);
	}

	if (logs == NULL)
		return;

	mflag = PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_DELAYED;
	history = purple_log_read((PurpleLog*)logs->data, &flags);

	header = g_strdup_printf(_("<b>Conversation with %s on %s:</b><br>"), alias,
							 purple_date_format_full(localtime(&((PurpleLog *)logs->data)->time)));
	purple_conversation_write(c, "", header, mflag, time(NULL));
	g_free(header);

	if (flags & PURPLE_LOG_READ_NO_NEWLINE)
		purple_str_strip_char(history, '\n');
	purple_conversation_write(c, "", history, mflag, time(NULL));
	g_free(history);

	purple_conversation_write(c, "", "<hr>", mflag, time(NULL));

	g_list_foreach(logs, (GFunc)purple_log_free, NULL);
	g_list_free(logs);
}

static void
history_prefs_check(PurplePlugin *plugin)
{
	if (!purple_prefs_get_bool("/core/logging/log_ims") &&
	    !purple_prefs_get_bool("/core/logging/log_chats"))
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

	purple_prefs_connect_callback(plugin, "/core/logging/log_ims",
								history_prefs_cb, plugin);
	purple_prefs_connect_callback(plugin, "/core/logging/log_chats",
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
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("GntHistory"),
	VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert "
	   "the last conversation into the current conversation."),
	"Sean Egan <seanegan@gmail.com>\n"
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	PURPLE_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(gnthistory, init_plugin, info)

