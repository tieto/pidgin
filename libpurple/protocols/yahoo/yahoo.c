/*
 * purple
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

#include "internal.h"

#include <account.h>
#include <core.h>
#include <plugins.h>

#include "ymsg.h"
#include "yahoo.h"
#include "yahoochat.h"
#include "yahoo_aliases.h"
#include "yahoo_doodle.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"

static PurpleProtocol *yahoo_protocol = NULL;

static GSList *cmds = NULL;

static void yahoo_register_commands(void)
{
	PurpleCmdId id;

	id = purple_cmd_register("join", "s", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_chat_join,
	                  _("join &lt;room&gt;:  Join a chat room on the Yahoo network"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("list", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_chat_list,
	                  _("list: List rooms on the Yahoo network"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("buzz", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_buzz,
	                  _("buzz: Buzz a user to get their attention"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("doodle", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoo", yahoo_doodle_purple_cmd_start,
	                 _("doodle: Request user to start a Doodle session"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));
}

static void yahoo_unregister_commands(void)
{
	while (cmds) {
		PurpleCmdId id = GPOINTER_TO_UINT(cmds->data);
		purple_cmd_unregister(id);
		cmds = g_slist_delete_link(cmds, cmds);
	}
}

static PurpleAccount *find_acct(const char *protocol, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, protocol);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(protocol, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

/* This may not be the best way to do this, but we find the first key w/o a value
 * and assume it is the buddy name */
static void yahoo_find_uri_novalue_param(gpointer key, gpointer value, gpointer user_data)
{
	char **retval = user_data;

	if (value == NULL && *retval == NULL) {
		*retval = key;
	}
}

static gboolean yahoo_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	char *acct_id = g_hash_table_lookup(params, "account");
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "ymsgr"))
		return FALSE;

	acct = find_acct(purple_protocol_get_id(yahoo_protocol), acct_id);

	if (!acct)
		return FALSE;

	/* ymsgr:SendIM?screename&m=The+Message */
	if (!g_ascii_strcasecmp(cmd, "SendIM")) {
		char *sname = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &sname);
		if (sname) {
			char *message = g_hash_table_lookup(params, "m");

			PurpleIMConversation *im = purple_conversations_find_im_with_account(
				sname, acct);
			if (im == NULL)
				im = purple_im_conversation_new(acct, sname);
			purple_conversation_present(PURPLE_CONVERSATION(im));

			if (message) {
				/* Spaces are encoded as '+' */
				g_strdelimit(message, "+", ' ');
				purple_conversation_send_confirm(PURPLE_CONVERSATION(im), message);
			}
		}
		/* else
			**If pidgindialogs_im() was in the core, we could use it here.
			 * It is all purple_request_* based, but I'm not sure it really belongs in the core
			pidgindialogs_im(); */

		return TRUE;
	}
	/* ymsgr:Chat?roomname */
	else if (!g_ascii_strcasecmp(cmd, "Chat")) {
		char *rname = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &rname);
		if (rname) {
			/* This is somewhat hacky, but the params aren't useful after this command */
			g_hash_table_insert(params, g_strdup("room"), g_strdup(rname));
			g_hash_table_insert(params, g_strdup("type"), g_strdup("Chat"));
			purple_serv_join_chat(purple_account_get_connection(acct), params);
		}
		/* else
			** Same as above (except that this would have to be re-written using purple_request_*)
			pidgin_blist_joinchat_show(); */

		return TRUE;
	}
	/* ymsgr:AddFriend?name */
	else if (!g_ascii_strcasecmp(cmd, "AddFriend")) {
		char *name = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &name);
		purple_blist_request_add_buddy(acct, name, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}

static GHashTable *
yahoo_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("Yahoo ID..."));
	return table;
}

static PurpleWhiteboardOps yahoo_whiteboard_ops =
{
	yahoo_doodle_start,
	yahoo_doodle_end,
	yahoo_doodle_get_dimensions,
	NULL,
	yahoo_doodle_get_brush,
	yahoo_doodle_set_brush,
	yahoo_doodle_send_draw_list,
	yahoo_doodle_clear,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
yahoo_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountOption *option;

	protocol->id        = "prpl-yahoo";
	protocol->name      = "Yahoo";
	protocol->options   = OPT_PROTO_MAIL_CHECK | OPT_PROTO_CHAT_TOPIC |
	                      OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE;
	protocol->icon_spec = purple_buddy_icon_spec_new("png,gif,jpeg",
	                                                    96, 96, 96, 96, 0,
	                                                    PURPLE_ICON_SCALE_SEND);

	protocol->whiteboard_ops = &yahoo_whiteboard_ops;

	option = purple_account_option_int_new(_("Pager port"), "port", YAHOO_PAGER_PORT);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("File transfer server"), "xfer_host", YAHOO_XFER_HOST);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Chat room locale"), "room_list_locale", YAHOO_ROOMLIST_LOCALE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Encoding"), "local_charset", "UTF-8");
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Ignore conference and chatroom invitations"), "ignore_invites", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

#if 0
	option = purple_account_option_bool_new(_("Use account proxy for HTTP and HTTPS connections"), "proxy_ssl", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Chat room list URL"), "room_list", YAHOO_ROOMLIST_URL);
	protocol->account_options = g_list_append(protocol->account_options, option);
#endif
}

static void
yahoo_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login        = yahoo_login;
	klass->close        = yahoo_close;
	klass->status_types = yahoo_status_types;
	klass->list_icon    = yahoo_list_icon;
}

static void
yahoo_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions            = yahoo_get_actions;
	client_iface->list_emblem            = yahoo_list_emblem;
	client_iface->status_text            = yahoo_status_text;
	client_iface->tooltip_text           = yahoo_tooltip_text;
	client_iface->blist_node_menu        = yahoo_blist_node_menu;
	client_iface->normalize              = purple_normalize_nocase;
	client_iface->offline_message        = yahoo_offline_message;
	client_iface->get_account_text_table = yahoo_get_account_text_table;
	client_iface->get_max_message_size   = yahoo_get_max_message_size;
}

static void
yahoo_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->get_info       = yahoo_get_info;
	server_iface->set_status     = yahoo_set_status;
	server_iface->set_idle       = yahoo_set_idle;
	server_iface->add_buddy      = yahoo_add_buddy;
	server_iface->remove_buddy   = yahoo_remove_buddy;
	server_iface->keepalive      = yahoo_keepalive;
	server_iface->alias_buddy    = yahoo_update_alias;
	server_iface->group_buddy    = yahoo_change_buddys_group;
	server_iface->rename_group   = yahoo_rename_group;
	server_iface->set_buddy_icon = yahoo_set_buddy_icon;
}

static void
yahoo_protocol_im_iface_init(PurpleProtocolIMIface *im_iface)
{
	im_iface->send        = yahoo_send_im;
	im_iface->send_typing = yahoo_send_typing;
}

static void
yahoo_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->info          = yahoo_c_info;
	chat_iface->info_defaults = yahoo_c_info_defaults;
	chat_iface->join          = yahoo_c_join;
	chat_iface->get_name      = yahoo_get_chat_name;
	chat_iface->invite        = yahoo_c_invite;
	chat_iface->leave         = yahoo_c_leave;
	chat_iface->send          = yahoo_c_send;
}

static void
yahoo_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_deny        = yahoo_add_deny;
	privacy_iface->rem_deny        = yahoo_rem_deny;
	privacy_iface->set_permit_deny = yahoo_set_permit_deny;
}

static void
yahoo_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *roomlist_iface)
{
	roomlist_iface->get_list        = yahoo_roomlist_get_list;
	roomlist_iface->cancel          = yahoo_roomlist_cancel;
	roomlist_iface->expand_category = yahoo_roomlist_expand_category;
}

static void
yahoo_protocol_attention_iface_init(PurpleProtocolAttentionIface *attention_iface)
{
	attention_iface->send      = yahoo_send_attention;
	attention_iface->get_types = yahoo_attention_types;
}

static void
yahoo_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->can_receive = yahoo_can_receive_file;
	xfer_iface->send        = yahoo_send_file;
	xfer_iface->new_xfer    = yahoo_new_xfer;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	YahooProtocol, yahoo_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  yahoo_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  yahoo_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  yahoo_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  yahoo_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  yahoo_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  yahoo_protocol_roomlist_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE,
	                                  yahoo_protocol_attention_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  yahoo_protocol_xfer_iface_init)
);

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-yahoo",
		"name",         "Yahoo Protocols",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("Yahoo! and Yahoo! JAPAN Protocols Plugin"),
		"description",  N_("Yahoo! and Yahoo! JAPAN Protocols Plugin"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	yahoo_protocol_register_type(plugin);

	yahoo_protocol = purple_protocols_add(YAHOO_TYPE_PROTOCOL, error);
	if (!yahoo_protocol)
		return FALSE;

	yahoo_init_colorht();

	yahoo_register_commands();

	purple_signal_connect(purple_get_core(), "uri-handler", yahoo_protocol,
		PURPLE_CALLBACK(yahoo_uri_handler), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	yahoo_unregister_commands();

	yahoo_dest_colorht();

	if (!purple_protocols_remove(yahoo_protocol, error))
		return FALSE;

	return TRUE;
}

PURPLE_PLUGIN_INIT(yahoo, plugin_query, plugin_load, plugin_unload);
