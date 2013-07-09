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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

/* Ripped from gtk/plugins/history.c */

#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "request.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gntconv.h"
#include "gntplugin.h"
#include "gntrequest.h"

#define HISTORY_PLUGIN_ID "gnt-history"

#define HISTORY_SIZE (4 * 1024)

static void historize(PurpleConversation *c)
{
	PurpleAccount *account = purple_conversation_get_account(c);
	const char *name = purple_conversation_get_name(c);
	GList *logs = NULL;
	const char *alias = name;
	PurpleLogReadFlags flags;
	char *history;
	char *header;
	PurpleMessageFlags mflag;

	if (PURPLE_IS_IM_CONVERSATION(c)) {
		GSList *buddies;
		GSList *cur;
		FinchConv *fc = FINCH_CONV(c);
		if (fc->list && fc->list->next) /* We were already in the middle of a conversation. */
			return;

		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/purple/logging/log_ims"))
			return;

		/* Find buddies for this conversation. */
		buddies = purple_blist_find_buddies(account, name);

		/* If we found at least one buddy, save the first buddy's alias. */
		if (buddies != NULL)
			alias = purple_buddy_get_contact_alias((PurpleBuddy *)buddies->data);

		for (cur = buddies; cur != NULL; cur = cur->next) {
			PurpleBListNode *node = cur->data;
			if ((node != NULL) &&
					((purple_blist_node_get_sibling_prev(node) != NULL) ||
						(purple_blist_node_get_sibling_next(node) != NULL))) {
				PurpleBListNode *node2;

				alias = purple_buddy_get_contact_alias((PurpleBuddy *)node);

				/* We've found a buddy that matches this conversation.  It's part of a
				 * PurpleContact with more than one PurpleBuddy.  Loop through the PurpleBuddies
				 * in the contact and get all the logs. */
				for (node2 = purple_blist_node_get_first_child(purple_blist_node_get_parent(node));
						node2 != NULL ; node2 = purple_blist_node_get_sibling_next(node2)) {
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
	} else if (PURPLE_IS_CHAT_CONVERSATION(c)) {
		/* If we're not logging, don't show anything.
		 * Otherwise, we might show a very old log. */
		if (!purple_prefs_get_bool("/purple/logging/log_chats"))
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
	if (!purple_prefs_get_bool("/purple/logging/log_ims") &&
	    !purple_prefs_get_bool("/purple/logging/log_chats"))
	{
		PurpleRequestFields *fields = purple_request_fields_new();
		PurpleRequestFieldGroup *group;
		PurpleRequestField *field;
		struct {
			const char *pref;
			const char *label;
		} prefs[] = {
			{"/purple/logging/log_ims", N_("Log IMs")},
			{"/purple/logging/log_chats", N_("Log chats")},
			{NULL, NULL}
		};
		int iter;
		GList *list = purple_log_logger_get_options();
		const char *system = purple_prefs_get_string("/purple/logging/format");

		group = purple_request_field_group_new(_("Logging"));

		field = purple_request_field_list_new("/purple/logging/format", _("Log format"));
		while (list) {
			const char *label = _(list->data);
			list = g_list_delete_link(list, list);
			purple_request_field_list_add_icon(field, label, NULL, list->data);
			if (system && strcmp(system, list->data) == 0)
				purple_request_field_list_add_selected(field, label);
			list = g_list_delete_link(list, list);
		}
		purple_request_field_group_add_field(group, field);

		for (iter = 0; prefs[iter].pref; iter++) {
			field = purple_request_field_bool_new(prefs[iter].pref, _(prefs[iter].label),
						purple_prefs_get_bool(prefs[iter].pref));
			purple_request_field_group_add_field(group, field);
		}

		purple_request_fields_add_group(fields, group);

		purple_request_fields(plugin, NULL, _("History Plugin Requires Logging"),
				      _("Logging can be enabled from Tools -> Preferences -> Logging.\n\n"
				      "Enabling logs for instant messages and/or chats will activate "
				      "history for the same conversation type(s)."),
				      fields,
				      _("OK"), G_CALLBACK(finch_request_save_in_prefs),
				      _("Cancel"), NULL,
				      NULL, NULL, NULL, plugin);
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
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("GntHistory"),
	DISPLAY_VERSION,
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

PURPLE_INIT_PLUGIN(gnthistory, init_plugin, info)

