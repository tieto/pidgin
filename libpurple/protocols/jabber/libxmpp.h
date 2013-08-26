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
#ifndef _LIBXMPP_H_
#define _LIBXMPP_H_

#include "jabber.h"

#define XMPP_ID     "prpl-jabber"
#define XMPP_NAME   "XMPP"
#define XMPP_DOMAIN (g_quark_from_static_string(XMPP_ID))

#define XMPP_TYPE_PROTOCOL             (xmpp_protocol_get_type())
#define XMPP_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XMPP_TYPE_PROTOCOL, XMPPProtocol))
#define XMPP_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), XMPP_TYPE_PROTOCOL, XMPPProtocolClass))
#define XMPP_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XMPP_TYPE_PROTOCOL))
#define XMPP_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), XMPP_TYPE_PROTOCOL))
#define XMPP_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), XMPP_TYPE_PROTOCOL, XMPPProtocolClass))

typedef struct _XMPPProtocol
{
	JabberProtocol parent;
} XMPPProtocol;

typedef struct _XMPPProtocolClass
{
	JabberProtocolClass parent_class;
} XMPPProtocolClass;

/**
 * Returns the GType for the XMPPProtocol object.
 */
GType xmpp_protocol_get_type(void);

#endif /* _LIBXMPP_H_ */
