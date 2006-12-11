/*
 * Autoreply - Autoreply feature for all the protocols
 * Copyright (C) 2005
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

#define PLUGIN_ID			"core-plugin_pack-autoreply"
#define PLUGIN_NAME			"Autoreply"
#define PLUGIN_STATIC_NAME	"Autoreply"
#define PLUGIN_SUMMARY		"Autoreply for all the protocols"
#define PLUGIN_DESCRIPTION	"This plugin lets you set autoreply message for any protocol. "\
							"You can set the global autoreply message from the Plugin-options dialog. " \
							"To set some specific autoreply message for a particular buddy, right click " \
							"on the buddy in the buddy-list window. To set autoreply messages for some " \
							"account, go to the `Advanced' tab of the Account-edit dialog."
#define PLUGIN_AUTHOR		"Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>"

/* System headers */
#include <glib.h>

/* Gaim headers */
#include <account.h>
#include <accountopt.h>
#include <blist.h>
#include <conversation.h>
#include <plugin.h>
#include <pluginpref.h>
#include <request.h>
#include <savedstatuses.h>
#include <status.h>
#include <util.h>
#include <version.h>

#define	PREFS_PREFIX		"/core/" PLUGIN_ID
#define	PREFS_IDLE			PREFS_PREFIX "/idle"
#define	PREFS_AWAY			PREFS_PREFIX "/away"
#define	PREFS_GLOBAL		PREFS_PREFIX "/global"
#define	PREFS_MINTIME		PREFS_PREFIX "/mintime"
#define	PREFS_MAXSEND		PREFS_PREFIX "/maxsend"
#define	PREFS_USESTATUS		PREFS_PREFIX "/usestatus"

typedef struct _GaimAutoReply GaimAutoReply;

struct _GaimAutoReply
{
	GaimBuddy *buddy;
	char *reply;
};

typedef enum
{
	STATUS_NEVER,
	STATUS_ALWAYS,
	STATUS_FALLBACK
} UseStatusMessage;

static GHashTable *options = NULL;

/**
 * Returns the auto-reply message for buddy
 */
static const char *
get_autoreply_message(GaimBuddy *buddy, GaimAccount *account)
{
	const char *reply = NULL;
	UseStatusMessage use_status;

	use_status = gaim_prefs_get_int(PREFS_USESTATUS);
	if (use_status == STATUS_ALWAYS)
	{
		GaimStatus *status = gaim_account_get_active_status(account);
		GaimStatusType *type = gaim_status_get_type(status);
		if (gaim_status_type_get_attr(type, "message") != NULL)
			reply = gaim_status_get_attr_string(status, "message");
		else
			reply = gaim_savedstatus_get_message(gaim_savedstatus_get_current());
	}

	if (!reply && buddy)
	{
		/* Is there any special auto-reply for this buddy? */
		reply = gaim_blist_node_get_string((GaimBlistNode*)buddy, "autoreply");

		if (!reply && GAIM_BLIST_NODE_IS_BUDDY((GaimBlistNode*)buddy))
		{
			/* Anything for the contact, then? */
			reply = gaim_blist_node_get_string(((GaimBlistNode*)buddy)->parent, "autoreply");
		}
	}

	if (!reply)
	{
		/* Is there any specific auto-reply for this account? */
		reply = gaim_account_get_string(account, "autoreply", NULL);
	}

	if (!reply)
	{
		/* Get the global auto-reply message */
		reply = gaim_prefs_get_string(PREFS_GLOBAL);
	}

	if (*reply == ' ')
		reply = NULL;

	if (!reply && use_status == STATUS_FALLBACK)
		reply = gaim_status_get_attr_string(gaim_account_get_active_status(account), "message");

	return reply;
}

static void
written_msg(GaimAccount *account, const char *who, const char *message,
				GaimConversation *conv, GaimMessageFlags flags, gpointer null)
{
	GaimBuddy *buddy;
	GaimPresence *presence;
	const char *reply = NULL;
	gboolean trigger = FALSE;

	if (!(flags & GAIM_MESSAGE_RECV))
		return;

	if (!message || !*message)
		return;

	/* Do not send an autoreply for an autoreply */
	if (flags & GAIM_MESSAGE_AUTO_RESP)
		return;

	g_return_if_fail(gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM);

	presence = gaim_account_get_presence(account);

	if (gaim_prefs_get_bool(PREFS_AWAY) && !gaim_presence_is_available(presence))
		trigger = TRUE;
	if (gaim_prefs_get_bool(PREFS_IDLE) && gaim_presence_is_idle(presence))
	   trigger = TRUE;

	if (!trigger)
		return;	

	buddy = gaim_find_buddy(account, who);
	reply = get_autoreply_message(buddy, account);

	if (reply)
	{
		GaimConnection *gc;
		GaimMessageFlags flag = GAIM_MESSAGE_SEND;
		time_t last_sent, now;
		int count_sent, maxsend;

		last_sent = GPOINTER_TO_INT(gaim_conversation_get_data(conv, "autoreply_lastsent"));
		now = time(NULL);

		/* Have we spent enough time after our last autoreply? */
		if (now - last_sent >= (gaim_prefs_get_int(PREFS_MINTIME)*60))
		{
			count_sent = GPOINTER_TO_INT(gaim_conversation_get_data(conv, "autoreply_count"));
			maxsend = gaim_prefs_get_int(PREFS_MAXSEND);

			/* Have we sent the autoreply enough times? */
			if (count_sent < maxsend || maxsend == -1)
			{
				gaim_conversation_set_data(conv, "autoreply_count", GINT_TO_POINTER(++count_sent));
				gaim_conversation_set_data(conv, "autoreply_lastsent", GINT_TO_POINTER(now));
				gc = gaim_account_get_connection(account);
				if (gc->flags & GAIM_CONNECTION_AUTO_RESP)
					flag |= GAIM_MESSAGE_AUTO_RESP;
				serv_send_im(gc, who, reply, flag);
				gaim_conv_im_write(GAIM_CONV_IM(conv), NULL, reply, flag, time(NULL));
			}
		}
	}
}

static void
set_auto_reply_cb(GaimBlistNode *node, char *message)
{
	if (!message || !*message)
		message = " ";
	gaim_blist_node_set_string(node, "autoreply", message);
}

static void
set_auto_reply(GaimBlistNode *node, gpointer plugin)
{
	char *message;
	GaimBuddy *buddy;
	GaimAccount *account;
	GaimConnection *gc;

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		buddy = (GaimBuddy *)node;
	else
		buddy = gaim_contact_get_priority_buddy((GaimContact*)node);

	account = gaim_buddy_get_account(buddy);
	gc = gaim_account_get_connection(account);

	/* XXX: There should be a way to reset to the default/account-default autoreply */

	message = g_strdup_printf(_("Set autoreply message for %s"),
					gaim_buddy_get_contact_alias(buddy));
	gaim_request_input(plugin, _("Set Autoreply Message"), message,
					_("The following message will be sent to the buddy when "
						"the buddy sends you a message and autoreply is enabled."),
					get_autoreply_message(buddy, account), TRUE, FALSE,
					(gc->flags & GAIM_CONNECTION_HTML) ? "html" : NULL,
					_("_Save"), G_CALLBACK(set_auto_reply_cb),
					_("_Cancel"), NULL, node);
	g_free(message);
}

static void
context_menu(GaimBlistNode *node, GList **menu, gpointer plugin)
{
	GaimMenuAction *action;

	if (!GAIM_BLIST_NODE_IS_BUDDY(node) && !GAIM_BLIST_NODE_IS_CONTACT(node))
		return;

	action = gaim_menu_action_new(_("Set _Autoreply Message"),
					GAIM_CALLBACK(set_auto_reply), plugin, NULL);
	(*menu) = g_list_prepend(*menu, action);
}

static void
add_option_for_protocol(GaimPlugin *plg)
{
	GaimPluginProtocolInfo *info = GAIM_PLUGIN_PROTOCOL_INFO(plg);
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Autoreply message"), "autoreply", NULL);
	info->protocol_options = g_list_append(info->protocol_options, option);

	if (!g_hash_table_lookup(options, plg))
		g_hash_table_insert(options, plg, option);
}

static void
remove_option_for_protocol(GaimPlugin *plg)
{
	GaimPluginProtocolInfo *info = GAIM_PLUGIN_PROTOCOL_INFO(plg);
	GaimAccountOption *option = g_hash_table_lookup(options, plg);

	if (g_list_find(info->protocol_options, option))
	{
		info->protocol_options = g_list_remove(info->protocol_options, option);
		gaim_account_option_destroy(option);
		g_hash_table_remove(options, plg);
	}
}

static void
plugin_load_cb(GaimPlugin *plugin, gboolean load)
{
	if (plugin->info && plugin->info->type == GAIM_PLUGIN_PROTOCOL)
	{
		if (load)
			add_option_for_protocol(plugin);
		else
			remove_option_for_protocol(plugin);
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GList *list;

	gaim_signal_connect(gaim_conversations_get_handle(), "wrote-im-msg", plugin,
						GAIM_CALLBACK(written_msg), NULL);
	gaim_signal_connect(gaim_blist_get_handle(), "blist-node-extended-menu", plugin,
						GAIM_CALLBACK(context_menu), plugin);
	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-load", plugin,
						GAIM_CALLBACK(plugin_load_cb), GINT_TO_POINTER(TRUE));
	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-unload", plugin,
						GAIM_CALLBACK(plugin_load_cb), GINT_TO_POINTER(FALSE));

	/* Perhaps it's necessary to do this after making sure the prpl-s have been loaded? */
	options = g_hash_table_new(g_direct_hash, g_direct_equal);
	list = gaim_plugins_get_protocols();
	while (list)
	{
		add_option_for_protocol(list->data);
		list = list->next;
	}
	
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	GList *list;

	if (options == NULL)
		return TRUE;

	list = gaim_plugins_get_protocols();
	while (list)
	{
		remove_option_for_protocol(list->data);
		list = list->next;
	}
	g_hash_table_destroy(options);
	options = NULL;

	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *pref;

	frame = gaim_plugin_pref_frame_new();

	pref = gaim_plugin_pref_new_with_label(_("Send autoreply messages when"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_AWAY,
					_("When my account is _away"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_IDLE,
					_("When my account is _idle"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_GLOBAL,
					_("_Default reply"));
	gaim_plugin_pref_set_type(pref, GAIM_PLUGIN_PREF_STRING_FORMAT);
	gaim_plugin_pref_set_format_type(pref,
				GAIM_STRING_FORMAT_TYPE_MULTILINE | GAIM_STRING_FORMAT_TYPE_HTML);
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_label(_("Status message"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_USESTATUS,
						_("Autoreply with status message"));
	gaim_plugin_pref_set_type(pref, GAIM_PLUGIN_PREF_CHOICE);
	gaim_plugin_pref_add_choice(pref, _("Never"),	
						GINT_TO_POINTER(STATUS_NEVER));
	gaim_plugin_pref_add_choice(pref, _("Always when there is a status message"),
						GINT_TO_POINTER(STATUS_ALWAYS));
	gaim_plugin_pref_add_choice(pref, _("Only when there's no autoreply message"),
						GINT_TO_POINTER(STATUS_FALLBACK));

	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_label(_("Delay between autoreplies"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_MINTIME,
					_("_Minimum delay (mins)"));
	gaim_plugin_pref_set_bounds(pref, 0, 9999);
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_label(_("Times to send autoreplies"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREFS_MAXSEND,
					_("Ma_ximum count"));
	gaim_plugin_pref_set_bounds(pref, 0, 9999);
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL
};

static GaimPluginInfo info = {
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
	gaim_prefs_add_none(PREFS_PREFIX);
	gaim_prefs_add_bool(PREFS_IDLE, TRUE);
	gaim_prefs_add_bool(PREFS_AWAY, TRUE);
	gaim_prefs_add_string(PREFS_GLOBAL, _("I am currently not available. Please leave your message, "
							"and I will get back to you as soon as possible."));
	gaim_prefs_add_int(PREFS_MINTIME, 10);
	gaim_prefs_add_int(PREFS_MAXSEND, 10);
	gaim_prefs_add_int(PREFS_USESTATUS, STATUS_NEVER);
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
