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

/* libaim is the AIM protocol plugin. It is linked against liboscar,
 * which contains all the shared implementation code with libicq
 */

#include "core.h"
#include "plugins.h"
#include "signals.h"

#include "libaim.h"
#include "oscarcommon.h"

static PurpleProtocol *my_protocol = NULL;

static void
aim_protocol_base_init(AIMProtocolClass *klass)
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

	proto_class->id        = "aim";
	proto_class->name      = "AIM";

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
		_("Always use AIM proxy server for\nfile transfers and direct IM (slower,\nbut does not reveal your IP address)"), "always_use_rv_proxy",
		OSCAR_DEFAULT_ALWAYS_USE_RV_PROXY);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_bool_new(_("Allow multiple simultaneous logins"), "allow_multiple_logins",
											OSCAR_DEFAULT_ALLOW_MULTIPLE_LOGINS);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Server"), "server", oscar_get_login_server(FALSE, TRUE));
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);
}

static void
aim_protocol_interface_init(PurpleProtocolInterface *iface)
{
	iface->list_icon            = oscar_list_icon_aim;
	iface->add_permit           = oscar_add_permit;
	iface->rem_permit           = oscar_rem_permit;
	iface->set_permit_deny      = oscar_set_aim_permdeny;
	iface->get_max_message_size = oscar_get_max_message_size;
}

static void aim_protocol_base_finalize(AIMProtocolClass *klass) { }

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "protocol-aim",
		"name",         "AIM Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("AIM Protocol Plugin"),
		"description",  N_("AIM Protocol Plugin"),
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
	my_protocol = purple_protocols_add(AIM_TYPE_PROTOCOL, error);
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

extern PurplePlugin *_oscar_plugin;

PURPLE_PROTOCOL_DEFINE_EXTENDED(_oscar_plugin, AIMProtocol, aim_protocol,
                                OSCAR_TYPE_PROTOCOL, 0);

PURPLE_PLUGIN_INIT_VAL(_oscar_plugin, aim, plugin_query, plugin_load,
                       plugin_unload);
