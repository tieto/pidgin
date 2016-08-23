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
	protocol->id   = "prpl-aim";
	protocol->name = "AIM";

	oscar_init_account_options(protocol, FALSE);
}

static void
aim_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->list_icon = oscar_list_icon_aim;
}

static void
aim_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_max_message_size = oscar_get_max_message_size;
}

static void
aim_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_permit      = oscar_add_permit;
	privacy_iface->rem_permit      = oscar_rem_permit;
	privacy_iface->set_permit_deny = oscar_set_aim_permdeny;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	AIMProtocol, aim_protocol, OSCAR_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  aim_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  aim_protocol_privacy_iface_init)
);
