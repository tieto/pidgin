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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "internal.h"

#define PLUGIN_ID			"core-plugin_pack-offlinemsg"
#define PLUGIN_NAME			"Offline Message Emulation"
#define PLUGIN_STATIC_NAME	"offlinemsg"
#define PLUGIN_SUMMARY		"Save messages sent to an offline user as pounce."
#define PLUGIN_DESCRIPTION	"Save messages sent to an offline user as pounce."
#define PLUGIN_AUTHOR		"Sadrul H Chowdhury <sadrul@users.sourceforge.net>"

/* Gaim headers */
#include <version.h>

#include <blist.h>
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
	GaimAccount *account;
	GaimConversation *conv;
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
	gaim_conversation_set_data(offline->conv, "plugin_pack:offlinemsg",
				GINT_TO_POINTER(OFFLINE_MSG_NO));
	gaim_conv_im_send_with_flags(GAIM_CONV_IM(offline->conv), offline->message, 0);
	discard_data(offline);
}

static void
record_pounce(OfflineMsg *offline)
{
	GaimPounce *pounce;
	GaimPounceEvent event;
	GaimPounceOption option;
	GaimConversation *conv;

	event = GAIM_POUNCE_SIGNON;
	option = GAIM_POUNCE_OPTION_NONE;

	pounce = gaim_pounce_new(gaim_core_get_ui(), offline->account, offline->who,
					event, option);

	gaim_pounce_action_set_enabled(pounce, "send-message", TRUE);
	gaim_pounce_action_set_attribute(pounce, "send-message", "message", offline->message);
 
	conv = offline->conv;
	if (!gaim_conversation_get_data(conv, "plugin_pack:offlinemsg"))
		gaim_conversation_write(conv, NULL, _("The rest of the messages will be saved "
							"as pounce. You can edit/delete the pounce from the `Buddy "
							"Pounce' dialog."),
							GAIM_MESSAGE_SYSTEM, time(NULL));
	gaim_conversation_set_data(conv, "plugin_pack:offlinemsg",
				GINT_TO_POINTER(OFFLINE_MSG_YES));

	gaim_conv_im_write(GAIM_CONV_IM(conv), offline->who, offline->message,
				GAIM_MESSAGE_SEND, time(NULL));

	discard_data(offline);
}

static void
sending_msg_cb(GaimAccount *account, const char *who, char **message, gpointer handle)
{
	GaimBuddy *buddy;
	OfflineMsg *offline;
	GaimConversation *conv;
	OfflineMessageSetting setting;

	buddy = gaim_find_buddy(account, who);
	if (!buddy)
		return;

	if (gaim_presence_is_online(gaim_buddy_get_presence(buddy)))
		return;

	if (gaim_account_supports_offline_message(account, buddy))
	{
		gaim_debug_info("offlinemsg", "Account \"%s\" supports offline message.",
					gaim_account_get_username(account));
		return;
	}

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM,
					who, account);

	if (!conv)
		return;

	setting = GPOINTER_TO_INT(gaim_conversation_get_data(conv, "plugin_pack:offlinemsg"));
	if (setting == OFFLINE_MSG_NO)
		return;

	offline = g_new0(OfflineMsg, 1);
	offline->conv = conv;
	offline->account = account;
	offline->who = g_strdup(who);
	offline->message = *message;
	*message = NULL;

	if (gaim_prefs_get_bool(PREF_ALWAYS) || setting == OFFLINE_MSG_YES)
		record_pounce(offline);
	else if (setting == OFFLINE_MSG_NONE)
	{
		char *ask;
		ask = g_strdup_printf(_("\"%s\" is currently offline. Do you want to save the "
						"rest of the messages in a pounce and automatically send them "
						"when \"%s\" logs back in?"), who, who);
	
		gaim_request_action(handle, _("Offline Message"), ask,
					_("You can edit/delete the pounce from the `Buddy Pounces' dialog"),
					1, offline, 2,
					_("Yes"), record_pounce,
					_("No"), cancel_poune);
		g_free(ask);
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_conversations_get_handle(), "sending-im-msg",
					plugin, GAIM_CALLBACK(sending_msg_cb), plugin);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *pref;

	frame = gaim_plugin_pref_frame_new();

	pref = gaim_plugin_pref_new_with_label(_("Save offline messages in pounce"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREF_ALWAYS,
					_("Do not ask. Always save in pounce."));
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,			/* Magic				*/
	GAIM_MAJOR_VERSION,			/* Gaim Major Version	*/
	GAIM_MINOR_VERSION,			/* Gaim Minor Version	*/
	GAIM_PLUGIN_STANDARD,		/* plugin type			*/
	NULL,						/* ui requirement		*/
	0,							/* flags				*/
	NULL,						/* dependencies			*/
	GAIM_PRIORITY_DEFAULT,		/* priority				*/

	PLUGIN_ID,					/* plugin id			*/
	N_(PLUGIN_NAME),			/* name					*/
	VERSION,					/* version				*/
	N_(PLUGIN_SUMMARY),			/* summary				*/
	N_(PLUGIN_DESCRIPTION),		/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	GAIM_WEBSITE,				/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,						/* destroy				*/

	NULL,						/* ui_info				*/
	NULL,						/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL						/* actions				*/
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none(PREF_PREFIX);
	gaim_prefs_add_bool(PREF_ALWAYS, FALSE);
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
