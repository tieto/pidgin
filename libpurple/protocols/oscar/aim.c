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

#include "aim.h"

#include "core.h"
#include "plugins.h"
#include "signals.h"

#include "oscarcommon.h"

static void
aim_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountOption *option;

	protocol->id   = "aim";
	protocol->name = "AIM";

	oscar_init_protocol_options(protocol);

	option = purple_account_option_bool_new(_("Allow multiple simultaneous logins"), "allow_multiple_logins",
											OSCAR_DEFAULT_ALLOW_MULTIPLE_LOGINS);
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);

	option = purple_account_option_string_new(_("Server"), "server", oscar_get_login_server(FALSE, TRUE));
	protocol->protocol_options = g_list_append(protocol->protocol_options, option);
}

static void
aim_protocol_class_init(PurpleProtocolClass *klass)
{
}

static void
aim_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->list_icon            = oscar_list_icon_aim;
	client_iface->add_permit           = oscar_add_permit;
	client_iface->rem_permit           = oscar_rem_permit;
	client_iface->set_permit_deny      = oscar_set_aim_permdeny;
	client_iface->get_max_message_size = oscar_get_max_message_size;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	AIMProtocol, aim_protocol, OSCAR_TYPE_PROTOCOL, 0,
	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
		                              aim_protocol_client_iface_init)
);
