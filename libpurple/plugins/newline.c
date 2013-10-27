/*
 * Displays messages on a new line, below the nick
 * Copyright (C) 2004 Stu Tomlinson <stu@nosnilmot.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */
#include "internal.h"

#include <string.h>

#include <conversation.h>
#include <debug.h>
#include <plugins.h>
#include <signals.h>
#include <util.h>
#include <version.h>

static gboolean
addnewline_msg_cb(PurpleAccount *account, char *sender, char **message,
					 PurpleConversation *conv, int *flags, void *data)
{
	if ((PURPLE_IS_IM_CONVERSATION(conv) &&
		 !purple_prefs_get_bool("/plugins/core/newline/im")) ||
		(PURPLE_IS_CHAT_CONVERSATION(conv) &&
		 !purple_prefs_get_bool("/plugins/core/newline/chat")))
		return FALSE;

	if (g_ascii_strncasecmp(*message, "/me ", strlen("/me "))) {
		char *tmp = g_strdup_printf("<br/>%s", *message);
		g_free(*message);
		*message = tmp;
	}

	return FALSE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/core/newline/im", _("Add new line in IMs"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/core/newline/chat", _("Add new line in Chats"));
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}


static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Stu Tomlinson <stu@nosnilmot.com>",
		NULL
	};

	return purple_plugin_info_new(
		"id",             "core-plugin_pack-newline",
		"name",           N_("New Line"),
		"version",        DISPLAY_VERSION,
		"category",       N_("User interface"),
		"summary",        N_("Prepends a newline to displayed message."),
		"description",    N_("Prepends a newline to messages so that the "
		                     "rest of the message appears below the "
		                     "username in the conversation window."),
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	void *conversation = purple_conversations_get_handle();

	purple_prefs_add_none("/plugins/core/newline");
	purple_prefs_add_bool("/plugins/core/newline/im", TRUE);
	purple_prefs_add_bool("/plugins/core/newline/chat", TRUE);

	purple_signal_connect(conversation, "writing-im-msg",
						plugin, PURPLE_CALLBACK(addnewline_msg_cb), NULL);
	purple_signal_connect(conversation, "writing-chat-msg",
						plugin, PURPLE_CALLBACK(addnewline_msg_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(newline, plugin_query, plugin_load, plugin_unload);
