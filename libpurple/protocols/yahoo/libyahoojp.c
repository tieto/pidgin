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

#include "libyahoojp.h"
#include "libymsg.h"
#include "yahoochat.h"
#include "yahoo_aliases.h"
#include "yahoo_doodle.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"

static PurpleProtocol *my_protocol = NULL;

static void yahoojp_register_commands(void)
{
	purple_cmd_register("join", "s", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_chat_join,
	                  _("join &lt;room&gt;:  Join a chat room on the Yahoo network"), NULL);
	purple_cmd_register("list", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_chat_list,
	                  _("list: List rooms on the Yahoo network"), NULL);
	purple_cmd_register("buzz", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoopurple_cmd_buzz,
	                  _("buzz: Buzz a user to get their attention"), NULL);
	purple_cmd_register("doodle", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                  "prpl-yahoojp", yahoo_doodle_purple_cmd_start,
	                 _("doodle: Request user to start a Doodle session"), NULL);
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
yahoojp_protocol_base_init(YahooJPProtocolClass *klass)
{
	PurpleProtocolClass *proto_class = PURPLE_PROTOCOL_CLASS(klass);
	PurpleAccountOption *option;

	proto_class->id        = YAHOOJP_ID;
	proto_class->name      = YAHOOJP_NAME;

	/* delete yahoo's protocol options */
	while (proto_class->protocol_options) {
		PurpleAccountOption *option = proto_class->protocol_options->data;
		purple_account_option_destroy(option);
		proto_class->protocol_options =
				g_list_delete_link(proto_class->protocol_options,
				proto_class->protocol_options);
	}

	option = purple_account_option_int_new(_("Pager port"), "port", YAHOO_PAGER_PORT);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("File transfer server"), "xfer_host", YAHOOJP_XFER_HOST);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Chat room locale"), "room_list_locale", YAHOOJP_ROOMLIST_LOCALE);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Encoding"), "local_charset", "UTF-8");
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_bool_new(_("Ignore conference and chatroom invitations"), "ignore_invites", FALSE);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);
}

static void
yahoojp_protocol_interface_init(PurpleProtocolInterface *iface)
{
	iface->get_account_text_table   = yahoojp_get_account_text_table;

	/* disable yahoo functions not available for yahoojp */
	iface->can_receive_file         = NULL;
}

static void yahoojp_protocol_base_finalize(YahooJPProtocolClass *klass) { }

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",            YAHOOJP_ID,
		"name",          YAHOOJP_NAME,
		"version",       DISPLAY_VERSION,
		"category",      N_("Protocol"),
		"summary",       N_("Yahoo! JAPAN Protocol Plugin"),
		"description",   N_("Yahoo! JAPAN Protocol Plugin"),
		"website",       PURPLE_WEBSITE,
		"abi-version",   PURPLE_ABI_VERSION,
		"dependencies",  YAHOO_ID,
		"flags",         GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL |
		                 GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	my_protocol = purple_protocols_add(YAHOOJP_TYPE_PROTOCOL);

	if (!my_protocol) {
		g_set_error(error, YAHOOJP_DOMAIN, 0, _("Failed to add yahoojp protocol"));
		return FALSE;
	}

	yahoojp_register_commands();

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (!purple_protocols_remove(my_protocol)) {
		g_set_error(error, YAHOOJP_DOMAIN, 0, _("Failed to remove yahoojp protocol"));
		return FALSE;
	}

	return TRUE;
}

PURPLE_PROTOCOL_DEFINE_EXTENDED(YahooJPProtocol, yahoojp_protocol,
                                YAHOO_TYPE_PROTOCOL, 0);

PURPLE_PLUGIN_INIT(yahoojp, plugin_query, plugin_load, plugin_unload);
