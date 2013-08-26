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

/* libxmpp is the XMPP protocol plugin. It is linked against libjabbercommon,
 * which may be used to support other protocols (Bonjour) which may need to
 * share code.
 */

#include "internal.h"
#include "chat.h"
#include "core.h"
#include "plugins.h"

#include "libxmpp.h"

static PurpleProtocol *my_protocol = NULL;

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

	acct = find_acct(purple_protocol_get_id(my_protocol), acct_id);

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

static void
xmpp_protocol_base_init(XMPPProtocolClass *klass)
{
	PurpleProtocolClass *proto_class = PURPLE_PROTOCOL_CLASS(klass);
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	proto_class->id        = XMPP_ID;
	proto_class->name      = XMPP_NAME;

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), NULL, '@');
	purple_account_user_split_set_reverse(split, FALSE);
	proto_class->user_splits = g_list_append(proto_class->user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	proto_class->user_splits = g_list_append(proto_class->user_splits, split);

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
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						   option);

	option = purple_account_option_bool_new(
						_("Allow plaintext auth over unencrypted streams"),
						"auth_plain_in_clear", FALSE);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						   option);

	option = purple_account_option_int_new(_("Connect port"), "port", 5222);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						   option);

	option = purple_account_option_string_new(_("Connect server"),
						  "connect_server", NULL);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						  option);

	option = purple_account_option_string_new(_("File transfer proxies"),
						  "ft_proxies",
						/* TODO: Is this an acceptable default?
						 * Also, keep this in sync as they add more servers */
						  JABBER_DEFAULT_FT_PROXIES);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						  option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
						  option);

	/* this should probably be part of global smiley theme settings later on,
	  shared with MSN */
	option = purple_account_option_bool_new(_("Show Custom Smileys"),
		"custom_smileys", TRUE);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options,
		option);

	purple_prefs_remove("/plugins/prpl/jabber");
}

static void xmpp_protocol_base_finalize(XMPPProtocolClass *klass) { }
static void xmpp_protocol_interface_init(PurpleProtocolInterface *iface) { }

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           XMPP_ID,
		"name",         XMPP_NAME,
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("XMPP Protocol Plugin"),
		"description",  N_("XMPP Protocol Plugin"),
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
	my_protocol = purple_protocols_add(XMPP_TYPE_PROTOCOL);
	if (!my_protocol) {
		g_set_error(error, XMPP_DOMAIN, 0, _("Failed to add jabber protocol"));
		return FALSE;
	}

	purple_signal_connect(purple_get_core(), "uri-handler", my_protocol,
		PURPLE_CALLBACK(xmpp_uri_handler), NULL);

	jabber_protocol_init(my_protocol);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	jabber_protocol_uninit(my_protocol);
	if (!purple_protocols_remove(my_protocol)) {
		g_set_error(error, XMPP_DOMAIN, 0, _("Failed to remove jabber protocol"));
		return FALSE;
	}

	return TRUE;
}

PURPLE_PROTOCOL_DEFINE_EXTENDED(XMPPProtocol, xmpp_protocol,
                                JABBER_TYPE_PROTOCOL, 0);

PURPLE_PLUGIN_INIT(jabber, plugin_query, plugin_load, plugin_unload);
