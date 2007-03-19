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
#define PLUGIN_NAME			N_("Autoreply")
#define PLUGIN_STATIC_NAME	"Autoreply"
#define PLUGIN_SUMMARY		N_("Autoreply for all the protocols")
#define PLUGIN_DESCRIPTION	N_("This plugin lets you set autoreply message for any protocol. "\
							"You can set the global autoreply message from the Plugin-options dialog. " \
							"To set some specific autoreply message for a particular buddy, right click " \
							"on the buddy in the buddy-list window. To set autoreply messages for some " \
							"account, go to the `Advanced' tab of the Account-edit dialog.")
#define PLUGIN_AUTHOR		"Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>"

/* System headers */
#include <glib.h>

/* Purple headers */
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

typedef struct _PurpleAutoReply PurpleAutoReply;

struct _PurpleAutoReply
{
	PurpleBuddy *buddy;
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
get_autoreply_message(PurpleBuddy *buddy, PurpleAccount *account)
{
	const char *reply = NULL;
	UseStatusMessage use_status;

	use_status = purple_prefs_get_int(PREFS_USESTATUS);
	if (use_status == STATUS_ALWAYS)
	{
		PurpleStatus *status = purple_account_get_active_status(account);
		PurpleStatusType *type = purple_status_get_type(status);
		if (purple_status_type_get_attr(type, "message") != NULL)
			reply = purple_status_get_attr_string(status, "message");
		else
			reply = purple_savedstatus_get_message(purple_savedstatus_get_current());
	}

	if (!reply && buddy)
	{
		/* Is there any special auto-reply for this buddy? */
		reply = purple_blist_node_get_string((PurpleBlistNode*)buddy, "autoreply");

		if (!reply && PURPLE_BLIST_NODE_IS_BUDDY((PurpleBlistNode*)buddy))
		{
			/* Anything for the contact, then? */
			reply = purple_blist_node_get_string(((PurpleBlistNode*)buddy)->parent, "autoreply");
		}
	}

	if (!reply)
	{
		/* Is there any specific auto-reply for this account? */
		reply = purple_account_get_string(account, "autoreply", NULL);
	}

	if (!reply)
	{
		/* Get the global auto-reply message */
		reply = purple_prefs_get_string(PREFS_GLOBAL);
	}

	if (*reply == ' ')
		reply = NULL;

	if (!reply && use_status == STATUS_FALLBACK)
		reply = purple_status_get_attr_string(purple_account_get_active_status(account), "message");

	return reply;
}

static void
written_msg(PurpleAccount *account, const char *who, const char *message,
				PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	PurpleBuddy *buddy;
	PurplePresence *presence;
	const char *reply = NULL;
	gboolean trigger = FALSE;

	if (!(flags & PURPLE_MESSAGE_RECV))
		return;

	if (!message || !*message)
		return;

	/* Do not send an autoreply for an autoreply */
	if (flags & PURPLE_MESSAGE_AUTO_RESP)
		return;

	g_return_if_fail(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM);

	presence = purple_account_get_presence(account);

	if (purple_prefs_get_bool(PREFS_AWAY) && !purple_presence_is_available(presence))
		trigger = TRUE;
	if (purple_prefs_get_bool(PREFS_IDLE) && purple_presence_is_idle(presence))
	   trigger = TRUE;

	if (!trigger)
		return;	

	buddy = purple_find_buddy(account, who);
	reply = get_autoreply_message(buddy, account);

	if (reply)
	{
		PurpleConnection *gc;
		PurpleMessageFlags flag = PURPLE_MESSAGE_SEND;
		time_t last_sent, now;
		int count_sent, maxsend;

		last_sent = GPOINTER_TO_INT(purple_conversation_get_data(conv, "autoreply_lastsent"));
		now = time(NULL);

		/* Have we spent enough time after our last autoreply? */
		if (now - last_sent >= (purple_prefs_get_int(PREFS_MINTIME)*60))
		{
			count_sent = GPOINTER_TO_INT(purple_conversation_get_data(conv, "autoreply_count"));
			maxsend = purple_prefs_get_int(PREFS_MAXSEND);

			/* Have we sent the autoreply enough times? */
			if (count_sent < maxsend || maxsend == -1)
			{
				purple_conversation_set_data(conv, "autoreply_count", GINT_TO_POINTER(++count_sent));
				purple_conversation_set_data(conv, "autoreply_lastsent", GINT_TO_POINTER(now));
				gc = purple_account_get_connection(account);
				if (gc->flags & PURPLE_CONNECTION_AUTO_RESP)
					flag |= PURPLE_MESSAGE_AUTO_RESP;
				serv_send_im(gc, who, reply, flag);
				purple_conv_im_write(PURPLE_CONV_IM(conv), NULL, reply, flag, time(NULL));
			}
		}
	}
}

static void
set_auto_reply_cb(PurpleBlistNode *node, char *message)
{
	if (!message || !*message)
		message = " ";
	purple_blist_node_set_string(node, "autoreply", message);
}

static void
set_auto_reply(PurpleBlistNode *node, gpointer plugin)
{
	char *message;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleConnection *gc;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		buddy = (PurpleBuddy *)node;
	else
		buddy = purple_contact_get_priority_buddy((PurpleContact*)node);

	account = purple_buddy_get_account(buddy);
	gc = purple_account_get_connection(account);

	/* XXX: There should be a way to reset to the default/account-default autoreply */

	message = g_strdup_printf(_("Set autoreply message for %s"),
					purple_buddy_get_contact_alias(buddy));
	purple_request_input(plugin, _("Set Autoreply Message"), message,
					_("The following message will be sent to the buddy when "
						"the buddy sends you a message and autoreply is enabled."),
					get_autoreply_message(buddy, account), TRUE, FALSE,
					(gc->flags & PURPLE_CONNECTION_HTML) ? "html" : NULL,
					_("_Save"), G_CALLBACK(set_auto_reply_cb),
					_("_Cancel"), NULL, node);
	g_free(message);
}

static void
context_menu(PurpleBlistNode *node, GList **menu, gpointer plugin)
{
	PurpleMenuAction *action;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node) && !PURPLE_BLIST_NODE_IS_CONTACT(node))
		return;

	action = purple_menu_action_new(_("Set _Autoreply Message"),
					PURPLE_CALLBACK(set_auto_reply), plugin, NULL);
	(*menu) = g_list_prepend(*menu, action);
}

static void
add_option_for_protocol(PurplePlugin *plg)
{
	PurplePluginProtocolInfo *info = PURPLE_PLUGIN_PROTOCOL_INFO(plg);
	PurpleAccountOption *option;

	option = purple_account_option_string_new(_("Autoreply message"), "autoreply", NULL);
	info->protocol_options = g_list_append(info->protocol_options, option);

	if (!g_hash_table_lookup(options, plg))
		g_hash_table_insert(options, plg, option);
}

static void
remove_option_for_protocol(PurplePlugin *plg)
{
	PurplePluginProtocolInfo *info = PURPLE_PLUGIN_PROTOCOL_INFO(plg);
	PurpleAccountOption *option = g_hash_table_lookup(options, plg);

	if (g_list_find(info->protocol_options, option))
	{
		info->protocol_options = g_list_remove(info->protocol_options, option);
		purple_account_option_destroy(option);
		g_hash_table_remove(options, plg);
	}
}

static void
plugin_load_cb(PurplePlugin *plugin, gboolean load)
{
	if (plugin->info && plugin->info->type == PURPLE_PLUGIN_PROTOCOL)
	{
		if (load)
			add_option_for_protocol(plugin);
		else
			remove_option_for_protocol(plugin);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	GList *list;

	purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg", plugin,
						PURPLE_CALLBACK(written_msg), NULL);
	purple_signal_connect(purple_blist_get_handle(), "blist-node-extended-menu", plugin,
						PURPLE_CALLBACK(context_menu), plugin);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-load", plugin,
						PURPLE_CALLBACK(plugin_load_cb), GINT_TO_POINTER(TRUE));
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload", plugin,
						PURPLE_CALLBACK(plugin_load_cb), GINT_TO_POINTER(FALSE));

	/* Perhaps it's necessary to do this after making sure the prpl-s have been loaded? */
	options = g_hash_table_new(g_direct_hash, g_direct_equal);
	list = purple_plugins_get_protocols();
	while (list)
	{
		add_option_for_protocol(list->data);
		list = list->next;
	}
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *list;

	if (options == NULL)
		return TRUE;

	list = purple_plugins_get_protocols();
	while (list)
	{
		remove_option_for_protocol(list->data);
		list = list->next;
	}
	g_hash_table_destroy(options);
	options = NULL;

	return TRUE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_label(_("Send autoreply messages when"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_AWAY,
					_("When my account is _away"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_IDLE,
					_("When my account is _idle"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_GLOBAL,
					_("_Default reply"));
	purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_STRING_FORMAT);
	purple_plugin_pref_set_format_type(pref,
				PURPLE_STRING_FORMAT_TYPE_MULTILINE | PURPLE_STRING_FORMAT_TYPE_HTML);
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_label(_("Status message"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_USESTATUS,
						_("Autoreply with status message"));
	purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(pref, _("Never"),	
						GINT_TO_POINTER(STATUS_NEVER));
	purple_plugin_pref_add_choice(pref, _("Always when there is a status message"),
						GINT_TO_POINTER(STATUS_ALWAYS));
	purple_plugin_pref_add_choice(pref, _("Only when there's no autoreply message"),
						GINT_TO_POINTER(STATUS_FALLBACK));

	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_label(_("Delay between autoreplies"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_MINTIME,
					_("_Minimum delay (mins)"));
	purple_plugin_pref_set_bounds(pref, 0, 9999);
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_label(_("Times to send autoreplies"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_MAXSEND,
					_("Ma_ximum count"));
	purple_plugin_pref_set_bounds(pref, 0, 9999);
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,			/* Magic				*/
	PURPLE_MAJOR_VERSION,			/* Purple Major Version	*/
	PURPLE_MINOR_VERSION,			/* Purple Minor Version	*/
	PURPLE_PLUGIN_STANDARD,			/* plugin type			*/
	NULL,					/* ui requirement		*/
	0,					/* flags				*/
	NULL,					/* dependencies			*/
	PURPLE_PRIORITY_DEFAULT,			/* priority				*/

	PLUGIN_ID,				/* plugin id			*/
	PLUGIN_NAME,				/* name					*/
	VERSION,				/* version				*/
	PLUGIN_SUMMARY,				/* summary				*/
	PLUGIN_DESCRIPTION,			/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	PURPLE_WEBSITE,				/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,					/* destroy				*/

	NULL,					/* ui_info				*/
	NULL,					/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL					/* actions				*/
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none(PREFS_PREFIX);
	purple_prefs_add_bool(PREFS_IDLE, TRUE);
	purple_prefs_add_bool(PREFS_AWAY, TRUE);
	purple_prefs_add_string(PREFS_GLOBAL, _("I am currently not available. Please leave your message, "
							"and I will get back to you as soon as possible."));
	purple_prefs_add_int(PREFS_MINTIME, 10);
	purple_prefs_add_int(PREFS_MAXSEND, 10);
	purple_prefs_add_int(PREFS_USESTATUS, STATUS_NEVER);
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
