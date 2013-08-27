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
#ifndef _LIBICQ_H_
#define _LIBICQ_H_

#include "oscar.h"

#define ICQ_ID     "prpl-icq"
#define ICQ_NAME   "ICQ"
#define ICQ_DOMAIN (g_quark_from_static_string(ICQ_ID))

#define ICQ_TYPE_PROTOCOL             (icq_protocol_get_type())
#define ICQ_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), ICQ_TYPE_PROTOCOL, ICQProtocol))
#define ICQ_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), ICQ_TYPE_PROTOCOL, ICQProtocolClass))
#define ICQ_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), ICQ_TYPE_PROTOCOL))
#define ICQ_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), ICQ_TYPE_PROTOCOL))
#define ICQ_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ICQ_TYPE_PROTOCOL, ICQProtocolClass))

typedef struct _ICQProtocol
{
	OscarProtocol parent;
} ICQProtocol;

typedef struct _ICQProtocolClass
{
	OscarProtocolClass parent_class;
} ICQProtocolClass;

/**
 * Returns the GType for the ICQProtocol object.
 */
GType icq_protocol_get_type(void);

#endif /* _LIBICQ_H_ */
