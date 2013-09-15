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

#include "internal.h"
#include "chat.h"
#include "core.h"
#include "plugins.h"

#include "gtalk.h"

static const char *
gtalk_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "google-talk";
}

static void
gtalk_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	protocol->id   = "gtalk";
	protocol->name = "Google Talk (XMPP)";

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), "gmail.com", '@');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol->user_splits = g_list_append(protocol->user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol->user_splits = g_list_append(protocol->user_splits, split);

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
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						   option);

	option = purple_account_option_bool_new(
						_("Allow plaintext auth over unencrypted streams"),
						"auth_plain_in_clear", FALSE);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						   option);

	option = purple_account_option_int_new(_("Connect port"), "port", 5222);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						   option);

	option = purple_account_option_string_new(_("Connect server"),
						  "connect_server", NULL);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						  option);

	option = purple_account_option_string_new(_("File transfer proxies"),
						  "ft_proxies",
						/* TODO: Is this an acceptable default?
						 * Also, keep this in sync as they add more servers */
						  JABBER_DEFAULT_FT_PROXIES);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						  option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
						  option);

	/* this should probably be part of global smiley theme settings later on,
	  shared with MSN */
	option = purple_account_option_bool_new(_("Show Custom Smileys"),
		"custom_smileys", TRUE);
	protocol->protocol_options = g_list_append(protocol->protocol_options,
		option);
}

static void
gtalk_protocol_class_init(PurpleProtocolClass *klass)
{
}

static void
gtalk_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->list_icon        = gtalk_list_icon;

	/* disable xmpp functions not available for gtalk */
	client_iface->register_user    = NULL;
	client_iface->unregister_user  = NULL;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	GTalkProtocol, gtalk_protocol, JABBER_TYPE_PROTOCOL, 0,
	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
		                              gtalk_protocol_client_iface_init)
);
