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
#ifndef _LIBGTALK_H_
#define _LIBGTALK_H_

#include "jabber.h"

#define GTALK_ID     "prpl-gtalk"
#define GTALK_NAME   "Google Talk (XMPP)"
#define GTALK_DOMAIN (g_quark_from_static_string(GTALK_ID))

#define GTALK_TYPE_PROTOCOL             (gtalk_protocol_get_type())
#define GTALK_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GTALK_TYPE_PROTOCOL, GTalkProtocol))
#define GTALK_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GTALK_TYPE_PROTOCOL, GTalkProtocolClass))
#define GTALK_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTALK_TYPE_PROTOCOL))
#define GTALK_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GTALK_TYPE_PROTOCOL))
#define GTALK_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GTALK_TYPE_PROTOCOL, GTalkProtocolClass))

typedef struct _GTalkProtocol
{
	JabberProtocol parent;
} GTalkProtocol;

typedef struct _GTalkProtocolClass
{
	JabberProtocolClass parent_class;
} GTalkProtocolClass;

/**
 * Returns the GType for the GTalkProtocol object.
 */
GType gtalk_protocol_get_type(void);

#endif /* _LIBGTALK_H_ */
