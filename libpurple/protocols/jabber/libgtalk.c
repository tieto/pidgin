/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 *
 */

/* libgtalk is the Google Talk XMPP protocol plugin. It is linked against
 * libjabbercommon, which may be used to support other protocols (Bonjour) which
 * may need to share code.
 */

#include "internal.h"

#include "accountopt.h"
#include "core.h"
#include "debug.h"
#include "version.h"

#include "iq.h"
#include "jabber.h"
#include "chat.h"
#include "disco.h"
#include "message.h"
#include "roster.h"
#include "si.h"
#include "message.h"
#include "plugins.h"
#include "presence.h"
#include "google/google.h"
#include "pep.h"
#include "usermood.h"
#include "usertune.h"
#include "caps.h"
#include "data.h"
#include "ibb.h"

static const char *
gtalk_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "google-talk";
}

static PurpleProtocol *my_protocol = NULL;

static PurpleProtocol protocol =
{
	"prpl-gtalk",                           /* id */
	"Google Talk (XMPP)",                   /* name */
	sizeof(PurpleProtocol),       /* struct_size */
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_MAIL_CHECK |
#ifdef HAVE_CYRUS_SASL
	OPT_PROTO_PASSWORD_OPTIONAL |
#endif
	OPT_PROTO_SLASH_COMMANDS_NATIVE,
	NULL,							/* user_splits */
	NULL,							/* protocol_options */
	{"png", 32, 32, 96, 96, 0, PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	jabber_get_actions,				/* get_actions */
	gtalk_list_icon,				/* list_icon */
	jabber_list_emblem,			/* list_emblems */
	jabber_status_text,				/* status_text */
	jabber_tooltip_text,			/* tooltip_text */
	jabber_status_types,			/* status_types */
	jabber_blist_node_menu,			/* blist_node_menu */
	jabber_chat_info,				/* chat_info */
	jabber_chat_info_defaults,		/* chat_info_defaults */
	jabber_login,					/* login */
	jabber_close,					/* close */
	jabber_message_send_im,			/* send_im */
	jabber_set_info,				/* set_info */
	jabber_send_typing,				/* send_typing */
	jabber_buddy_get_info,			/* get_info */
	jabber_set_status,				/* set_status */
	jabber_idle_set,				/* set_idle */
	NULL,							/* change_passwd */
	jabber_roster_add_buddy,		/* add_buddy */
	NULL,							/* add_buddies */
	jabber_roster_remove_buddy,		/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	jabber_add_deny,				/* add_deny */
	NULL,							/* rem_permit */
	jabber_rem_deny,				/* rem_deny */
	NULL,							/* set_permit_deny */
	jabber_chat_join,				/* join_chat */
	NULL,							/* reject_chat */
	jabber_get_chat_name,			/* get_chat_name */
	jabber_chat_invite,				/* chat_invite */
	jabber_chat_leave,				/* chat_leave */
	NULL,							/* chat_whisper */
	jabber_message_send_chat,		/* chat_send */
	jabber_keepalive,				/* keepalive */
	NULL,							/* register_user */
	NULL,							/* get_cb_info */
	jabber_roster_alias_change,		/* alias_buddy */
	jabber_roster_group_change,		/* group_buddy */
	jabber_roster_group_rename,		/* rename_group */
	NULL,							/* buddy_free */
	jabber_convo_closed,			/* convo_closed */
	jabber_normalize,				/* normalize */
	jabber_set_buddy_icon,			/* set_buddy_icon */
	NULL,							/* remove_group */
	jabber_chat_user_real_name,	/* get_cb_real_name */
	jabber_chat_set_topic,			/* set_chat_topic */
	jabber_find_blist_chat,			/* find_blist_chat */
	jabber_roomlist_get_list,		/* roomlist_get_list */
	jabber_roomlist_cancel,			/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	jabber_can_receive_file,		/* can_receive_file */
	jabber_si_xfer_send,			/* send_file */
	jabber_si_new_xfer,				/* new_xfer */
	jabber_offline_message,			/* offline_message */
	NULL,							/* whiteboard_prpl_ops */
	jabber_prpl_send_raw,			/* send_raw */
	jabber_roomlist_room_serialize, /* roomlist_room_serialize */
	NULL,							/* unregister_user */
	jabber_send_attention,			/* send_attention */
	jabber_attention_types,			/* attention_types */
	NULL, /* get_account_text_table */
	jabber_initiate_media,          /* initiate_media */
	jabber_get_media_caps,                  /* get_media_caps */
	jabber_get_moods,  							/* get_moods */
	NULL, /* set_public_alias */
	NULL  /* get_public_alias */
};

static PurpleAccount *find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean xmpp_uri_handler(const char *proto, const char *user, GHashTable *params)
{
	char *acct_id = params ? g_hash_table_lookup(params, "account") : NULL;
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "xmpp"))
		return FALSE;

	acct = find_acct(my_protocol->id, acct_id);

	if (!acct)
		return FALSE;

	/* xmpp:romeo@montague.net?message;subject=Test%20Message;body=Here%27s%20a%20test%20message */
	/* params is NULL if the URI has no '?' (or anything after it) */
	if (!params || g_hash_table_lookup_extended(params, "message", NULL, NULL)) {
		char *body = g_hash_table_lookup(params, "body");
		if (user && *user) {
			PurpleIMConversation *im =
					purple_im_conversation_new(acct, user);
			purple_conversation_present(PURPLE_CONVERSATION(im));
			if (body && *body)
				purple_conversation_send_confirm(PURPLE_CONVERSATION(im), body);
		}
	} else if (g_hash_table_lookup_extended(params, "roster", NULL, NULL)) {
		char *name = g_hash_table_lookup(params, "name");
		if (user && *user)
			purple_blist_request_add_buddy(acct, user, NULL, name);
	} else if (g_hash_table_lookup_extended(params, "join", NULL, NULL)) {
		PurpleConnection *gc = purple_account_get_connection(acct);
		if (user && *user) {
			GHashTable *params = jabber_chat_info_defaults(gc, user);
			jabber_chat_join(gc, params);
		}
		return TRUE;
	}

	return FALSE;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-gtalk",
		"name",         "Google Talk (XMPP)",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("Google Talk Protocol Plugin"),
		"description",  N_("Google Talk Protocol Plugin"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL |
		                GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), "gmail.com", '@');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol.user_splits = g_list_append(protocol.user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol.user_splits = g_list_append(protocol.user_splits, split);

#define ADD_VALUE(list, desc, v) { \
	PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1); \
	kvp->key = g_strdup((desc)); \
	kvp->value = g_strdup((v)); \
	list = g_list_prepend(list, kvp); \
}

	ADD_VALUE(encryption_values, _("Require encryption"), "require_tls");
	ADD_VALUE(encryption_values, _("Use encryption if available"), "opportunistic_tls");
	ADD_VALUE(encryption_values, _("Use old-style SSL"), "old_ssl");
#if 0
	ADD_VALUE(encryption_values, "None", "none");
#endif
	encryption_values = g_list_reverse(encryption_values);

#undef ADD_VALUE

	option = purple_account_option_list_new(_("Connection security"), "connection_security", encryption_values);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						   option);

	option = purple_account_option_bool_new(
						_("Allow plaintext auth over unencrypted streams"),
						"auth_plain_in_clear", FALSE);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						   option);

	option = purple_account_option_int_new(_("Connect port"), "port", 5222);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						   option);

	option = purple_account_option_string_new(_("Connect server"),
						  "connect_server", NULL);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						  option);

	option = purple_account_option_string_new(_("File transfer proxies"),
						  "ft_proxies",
						/* TODO: Is this an acceptable default?
						 * Also, keep this in sync as they add more servers */
						  JABBER_DEFAULT_FT_PROXIES);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						  option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
						  option);

	/* this should probably be part of global smiley theme settings later on,
	  shared with MSN */
	option = purple_account_option_bool_new(_("Show Custom Smileys"),
		"custom_smileys", TRUE);
	protocol.protocol_options = g_list_append(protocol.protocol_options,
		option);

	my_protocol = &protocol;

	purple_signal_connect(purple_get_core(), "uri-handler", my_protocol,
		PURPLE_CALLBACK(xmpp_uri_handler), NULL);

	purple_protocols_add(my_protocol);
	jabber_plugin_init(my_protocol);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	jabber_plugin_uninit(my_protocol);
	purple_protocols_remove(my_protocol);

	return TRUE;
}

PURPLE_PLUGIN_INIT(gtalk, plugin_query, plugin_load, plugin_unload);

