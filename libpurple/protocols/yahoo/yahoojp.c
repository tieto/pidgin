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
#include <plugins.h>

#include "yahoojp.h"
#include "ymsg.h"
#include "yahoochat.h"
#include "yahoo_aliases.h"
#include "yahoo_doodle.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"

static GSList *cmds = NULL;

void yahoojp_register_commands(void)
{
	PurpleCmdId id;

	id = purple_cmd_register("join", "s", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_chat_join,
	                  _("join &lt;room&gt;:  Join a chat room on the Yahoo network"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("list", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_chat_list,
	                  _("list: List rooms on the Yahoo network"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("buzz", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_buzz,
	                  _("buzz: Buzz a user to get their attention"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("doodle", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoo_doodle_purple_cmd_start,
	                 _("doodle: Request user to start a Doodle session"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));
}

void yahoojp_unregister_commands(void)
{
	while (cmds) {
		PurpleCmdId id = GPOINTER_TO_UINT(cmds->data);
		purple_cmd_unregister(id);
		cmds = g_slist_delete_link(cmds, cmds);
	}
}

static GHashTable *
yahoojp_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("Yahoo JAPAN ID..."));
	return table;
}

static void
yahoojp_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountOption *option;

	protocol->id   = "prpl-yahoojp";
	protocol->name = "Yahoo JAPAN";

	/* delete yahoo's protocol options */
	purple_protocol_override(protocol, PURPLE_PROTOCOL_OVERRIDE_PROTOCOL_OPTIONS);

	option = purple_account_option_int_new(_("Pager port"), "port", YAHOO_PAGER_PORT);
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);

	option = purple_account_option_string_new(_("File transfer server"), "xfer_host", YAHOOJP_XFER_HOST);
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);

	option = purple_account_option_string_new(_("Chat room locale"), "room_list_locale", YAHOOJP_ROOMLIST_LOCALE);
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);

	option = purple_account_option_string_new(_("Encoding"), "local_charset", "UTF-8");
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);

	option = purple_account_option_bool_new(_("Ignore conference and chatroom invitations"), "ignore_invites", FALSE);
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);
}

static void
yahoojp_protocol_class_init(PurpleProtocolClass *klass)
{
}

static void
yahoojp_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_account_text_table = yahoojp_get_account_text_table;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	YahooJPProtocol, yahoojp_protocol, YAHOO_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  yahoojp_protocol_client_iface_init)
);
