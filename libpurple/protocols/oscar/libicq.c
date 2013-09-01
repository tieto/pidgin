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

/* libicq is the ICQ protocol plugin. It is linked against liboscar,
 * which contains all the shared implementation code with libaim
 */

#include "core.h"
#include "plugins.h"
#include "signals.h"

#include "libicq.h"
#include "oscarcommon.h"

static PurpleProtocol *my_protocol = NULL;

static GHashTable *
icq_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("ICQ UIN..."));
	return table;
}

static gssize
icq_get_max_message_size(PurpleConversation *conv)
{
	/* XXX: got from pidgin-otr - verify and document it */
	return 2346;
}

static void
icq_protocol_base_init(ICQProtocolClass *klass)
{
	PurpleProtocolClass *proto_class = PURPLE_PROTOCOL_CLASS(klass);
	PurpleAccountOption *option;
	static const gchar *encryption_keys[] = {
		N_("Use encryption if available"),
		N_("Require encryption"),
		N_("Don't use encryption"),
		NULL
	};
	static const gchar *encryption_values[] = {
		OSCAR_OPPORTUNISTIC_ENCRYPTION,
		OSCAR_REQUIRE_ENCRYPTION,
		OSCAR_NO_ENCRYPTION,
		NULL
	};
	GList *encryption_options = NULL;
	int i;

	proto_class->id        = "icq";
	proto_class->name      = "ICQ";

	option = purple_account_option_int_new(_("Port"), "port", OSCAR_DEFAULT_LOGIN_PORT);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	for (i = 0; encryption_keys[i]; i++) {
		PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1);
		kvp->key = g_strdup(_(encryption_keys[i]));
		kvp->value = g_strdup(encryption_values[i]);
		encryption_options = g_list_append(encryption_options, kvp);
	}
	option = purple_account_option_list_new(_("Connection security"), "encryption", encryption_options);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_bool_new(_("Use clientLogin"), "use_clientlogin",
			OSCAR_DEFAULT_USE_CLIENTLOGIN);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_bool_new(
		_("Always use ICQ proxy server for\nfile transfers and direct IM (slower,\nbut does not reveal your IP address)"), "always_use_rv_proxy",
		OSCAR_DEFAULT_ALWAYS_USE_RV_PROXY);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Server"), "server", oscar_get_login_server(TRUE, TRUE));
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Encoding"), "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);
}

static void
icq_protocol_interface_init(PurpleProtocolInterface *iface)
{
	iface->list_icon              = oscar_list_icon_icq;
	iface->get_account_text_table = icq_get_account_text_table;
	iface->get_moods              = oscar_get_purple_moods;
	iface->get_max_message_size   = icq_get_max_message_size;
}

static void icq_protocol_base_finalize(ICQProtocolClass *klass) { }

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const dependencies[] = {
		"protocol-aim",
		NULL
	};

	return purple_plugin_info_new(
		"id",            "protocol-icq",
		"name",          "ICQ Protocol",
		"version",       DISPLAY_VERSION,
		"category",      N_("Protocol"),
		"summary",       N_("ICQ Protocol Plugin"),
		"description",   N_("ICQ Protocol Plugin"),
		"website",       PURPLE_WEBSITE,
		"abi-version",   PURPLE_ABI_VERSION,
		"dependencies",  dependencies,
		"flags",         GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL |
		                 GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	my_protocol = purple_protocols_add(ICQ_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	purple_signal_connect(purple_get_core(), "uri-handler", my_protocol,
		PURPLE_CALLBACK(oscar_uri_handler), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}

static PurplePlugin *my_plugin;

PURPLE_PROTOCOL_DEFINE_EXTENDED(my_plugin, ICQProtocol, icq_protocol,
                                OSCAR_TYPE_PROTOCOL, 0);

PURPLE_PLUGIN_INIT_VAL(my_plugin, icq, plugin_query, plugin_load,
                       plugin_unload);
