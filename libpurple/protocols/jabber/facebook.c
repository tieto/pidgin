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

#include "facebook.h"

static const char *
facebook_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "facebook";
}

static void
facebook_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	JabberStream *js;

	jabber_login(account);

	gc = purple_account_get_connection(account);
	if (!gc)
		return;

	purple_connection_set_flags(gc, 0);

	js = purple_connection_get_protocol_data(gc);
	if (!js)
		return;

	js->server_caps |= JABBER_CAP_FACEBOOK;
}

static void
facebook_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	protocol->id      = "prpl-facebook-xmpp";
	protocol->name    = "Facebook (XMPP)";
	protocol->options = 0;
	purple_protocol_override(protocol, PURPLE_PROTOCOL_OVERRIDE_ICON_SPEC);

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), "chat.facebook.com", '@');
	purple_account_user_split_set_reverse(split, FALSE);
	purple_account_user_split_set_constant(split, TRUE);
	protocol->user_splits = g_list_append(protocol->user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	purple_account_user_split_set_constant(split, TRUE);
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
	protocol->account_options = g_list_append(protocol->account_options,
						   option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	protocol->account_options = g_list_append(protocol->account_options,
						  option);
}

static void
facebook_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login     = facebook_login;
	klass->list_icon = facebook_list_icon;
}

static void
facebook_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions     = NULL;
	client_iface->find_blist_chat = NULL;
	client_iface->blist_node_menu = NULL;
	client_iface->get_moods       = NULL;
}

static void
facebook_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->register_user   = NULL;
	server_iface->unregister_user = NULL;
	server_iface->set_info        = NULL;
	server_iface->add_buddy       = NULL;
	server_iface->remove_buddy    = NULL;
	server_iface->alias_buddy     = NULL;
	server_iface->group_buddy     = NULL;
	server_iface->rename_group    = NULL;
	server_iface->set_buddy_icon  = NULL;
}

static void
facebook_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->info               = NULL;
	chat_iface->info_defaults      = NULL;
	chat_iface->join               = NULL;
	chat_iface->get_name           = NULL;
	chat_iface->invite             = NULL;
	chat_iface->leave              = NULL;
	chat_iface->send               = NULL;
	chat_iface->get_user_real_name = NULL;
	chat_iface->set_topic          = NULL;
}

static void
facebook_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_deny = NULL;
	privacy_iface->rem_deny = NULL;
}

static void
facebook_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *roomlist_iface)
{
	roomlist_iface->get_list = NULL;
	roomlist_iface->cancel   = NULL;
}

static void
facebook_protocol_attention_iface_init(PurpleProtocolAttentionIface *attention_iface)
{
	attention_iface->send      = NULL;
	attention_iface->get_types = NULL;
}

static void
facebook_protocol_media_iface_init(PurpleProtocolMediaIface *media_iface)
{
	media_iface->initiate_session = NULL;
	media_iface->get_caps         = NULL;
}

static void
facebook_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->can_receive = NULL;
	xfer_iface->send        = NULL;
	xfer_iface->new_xfer    = NULL;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	FacebookProtocol, facebook_protocol, JABBER_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  facebook_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  facebook_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  facebook_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  facebook_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  facebook_protocol_roomlist_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE,
	                                  facebook_protocol_attention_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_MEDIA_IFACE,
	                                  facebook_protocol_media_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  facebook_protocol_xfer_iface_init)
);
