/*
 * Offline Message Emulation - Save messages sent to an offline user as pounce
 * Copyright (C) 2004
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"

#define PLUGIN_ID			"core-plugin_pack-offlinemsg"
#define PLUGIN_NAME			N_("Offline Message Emulation")
#define PLUGIN_CATEGORY		N_("Utility")
#define PLUGIN_STATIC_NAME	offlinemsg
#define PLUGIN_SUMMARY		N_("Save messages sent to an offline user as pounce.")
#define PLUGIN_DESCRIPTION	N_("Save messages sent to an offline user as pounce.")
#define PLUGIN_AUTHORS		{"Sadrul H Chowdhury <sadrul@users.sourceforge.net>", NULL}

/* Purple headers */
#include <version.h>

#include <buddylist.h>
#include <conversation.h>
#include <core.h>
#include <debug.h>
#include <pounce.h>
#include <request.h>

#define	PREF_PREFIX		"/plugins/core/" PLUGIN_ID
#define	PREF_ALWAYS		PREF_PREFIX "/always"

typedef struct _OfflineMsg OfflineMsg;

typedef enum
{
	OFFLINE_MSG_NONE,
	OFFLINE_MSG_YES,
	OFFLINE_MSG_NO
} OfflineMessageSetting;

struct _OfflineMsg
{
	PurpleAccount *account;
	PurpleConversation *conv;
	char *who;
	char *message;
};

static void
discard_data(OfflineMsg *offline)
{
	g_free(offline->who);
	g_free(offline->message);
	g_free(offline);
}

static void
cancel_poune(OfflineMsg *offline)
{
	g_object_set_data(G_OBJECT(offline->conv), "plugin_pack:offlinemsg",
				GINT_TO_POINTER(OFFLINE_MSG_NO));
	purple_conversation_send_with_flags(offline->conv, offline->message, 0);
	discard_data(offline);
}

static void
record_pounce(OfflineMsg *offline)
{
	PurplePounce *pounce;
	PurplePounceEvent event;
	PurplePounceOption option;
	PurpleConversation *conv;
	char *temp;

	event = PURPLE_POUNCE_SIGNON;
	option = PURPLE_POUNCE_OPTION_NONE;

	pounce = purple_pounce_new(purple_core_get_ui(), offline->account, offline->who,
					event, option);

	purple_pounce_action_set_enabled(pounce, "send-message", TRUE);

	temp = g_strdup_printf("(%s) %s", _("Offline message"),
			offline->message);
	purple_pounce_action_set_attribute(pounce, "send-message", "message",
			temp);
	g_free(temp);

	conv = offline->conv;
	if (!g_object_get_data(G_OBJECT(conv), "plugin_pack:offlinemsg")) {
		purple_conversation_write_system_message(conv,
			_("The rest of the messages will be saved "
			"as pounces. You can edit/delete the pounce from the `Buddy "
			"Pounce' dialog."), 0);
	}
	g_object_set_data(G_OBJECT(conv), "plugin_pack:offlinemsg",
				GINT_TO_POINTER(OFFLINE_MSG_YES));

	/* TODO: use a reference to a PurpleMessage */
	purple_conversation_write_message(conv,
		purple_message_new_outgoing(offline->who, offline->message, 0));

	discard_data(offline);
}

static void
sending_msg_cb(PurpleAccount *account, PurpleMessage *msg, gpointer handle)
{
	PurpleBuddy *buddy;
	OfflineMsg *offline;
	PurpleConversation *conv;
	OfflineMessageSetting setting;
	const gchar *who = purple_message_get_recipient(msg);

	if (purple_message_is_empty(msg))
		return;

	buddy = purple_blist_find_buddy(account, who);
	if (!buddy)
		return;

	if (purple_presence_is_online(purple_buddy_get_presence(buddy)))
		return;

	if (purple_account_supports_offline_message(account, buddy))
	{
		purple_debug_info("offlinemsg", "Account \"%s\" supports offline messages.\n",
					purple_account_get_username(account));
		return;
	}

	conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(who, account));

	if (!conv)
		return;

	setting = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "plugin_pack:offlinemsg"));
	if (setting == OFFLINE_MSG_NO)
		return;

	offline = g_new0(OfflineMsg, 1);
	offline->conv = conv;
	offline->account = account;
	offline->who = g_strdup(who);
	offline->message = g_strdup(purple_message_get_contents(msg));
	purple_message_set_contents(msg, NULL);

	if (purple_prefs_get_bool(PREF_ALWAYS) || setting == OFFLINE_MSG_YES)
		record_pounce(offline);
	else if (setting == OFFLINE_MSG_NONE)
	{
		char *ask;
		ask = g_strdup_printf(_("\"%s\" is currently offline. Do you want to save the "
						"rest of the messages in a pounce and automatically send them "
						"when \"%s\" logs back in?"), who, who);

		purple_request_action(handle, _("Offline Message"), ask,
					_("You can edit/delete the pounce from the `Buddy Pounces' dialog"),
					0, purple_request_cpar_from_conversation(offline->conv),
					offline, 2,
					_("Yes"), record_pounce,
					_("No"), cancel_poune);
		g_free(ask);
	}
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_label(_("Save offline messages in pounce"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_ALWAYS,
					_("Do not ask. Always save in pounce."));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = PLUGIN_AUTHORS;

	return purple_plugin_info_new(
		"id",             PLUGIN_ID,
		"name",           PLUGIN_NAME,
		"version",        DISPLAY_VERSION,
		"category",       PLUGIN_CATEGORY,
		"summary",        PLUGIN_SUMMARY,
		"description",    PLUGIN_DESCRIPTION,
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
	purple_prefs_add_none(PREF_PREFIX);
	purple_prefs_add_bool(PREF_ALWAYS, FALSE);

	purple_signal_connect_priority(purple_conversations_get_handle(), "sending-im-msg",
					plugin, PURPLE_CALLBACK(sending_msg_cb), plugin, PURPLE_SIGNAL_PRIORITY_HIGHEST);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(PLUGIN_STATIC_NAME, plugin_query, plugin_load, plugin_unload);
