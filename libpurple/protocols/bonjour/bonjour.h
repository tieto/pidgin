/**
 * @file bonjour.h The Purple interface to mDNS and peer to peer Jabber.
 *
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

#ifndef _BONJOUR_H_
#define _BONJOUR_H_

#include "protocol.h"

#include "mdns_common.h"
#include "internal.h"
#include "jabber.h"

#define BONJOUR_GROUP_NAME _("Bonjour")
#define BONJOUR_PROTOCOL_NAME "bonjour"
#define BONJOUR_ICON_NAME "bonjour"

#define BONJOUR_ID     "prpl-bonjour"
#define BONJOUR_NAME   "Bonjour"
#define BONJOUR_DOMAIN (g_quark_from_static_string(BONJOUR_ID))

#define BONJOUR_STATUS_ID_OFFLINE   "offline"
#define BONJOUR_STATUS_ID_AVAILABLE "available"
#define BONJOUR_STATUS_ID_AWAY      "away"

#define BONJOUR_DEFAULT_PORT 5298

#define BONJOUR_TYPE_PROTOCOL             (bonjour_protocol_get_type())
#define BONJOUR_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), BONJOUR_TYPE_PROTOCOL, BonjourProtocol))
#define BONJOUR_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), BONJOUR_TYPE_PROTOCOL, BonjourProtocolClass))
#define BONJOUR_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), BONJOUR_TYPE_PROTOCOL))
#define BONJOUR_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), BONJOUR_TYPE_PROTOCOL))
#define BONJOUR_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), BONJOUR_TYPE_PROTOCOL, BonjourProtocolClass))

typedef struct _BonjourProtocol
{
	PurpleProtocol parent;
} BonjourProtocol;

typedef struct _BonjourProtocolClass
{
	PurpleProtocolClass parent_class;
} BonjourProtocolClass;

typedef struct _BonjourData
{
	BonjourDnsSd *dns_sd_data;
	BonjourJabber *jabber_data;
	GSList *xfer_lists;
	gchar *jid;
} BonjourData;

/**
 * Returns the GType for the BonjourProtocol object.
 */
GType bonjour_protocol_get_type(void);

/**
 *  This will always be username@machinename
 */
const char *bonjour_get_jid(PurpleAccount *account);

#endif /* _BONJOUR_H_ */
