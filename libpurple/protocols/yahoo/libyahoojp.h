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
#ifndef _YAHOOJP_H_
#define _YAHOOJP_H_

#include "libyahoo.h"

#define YAHOOJP_ID     "prpl-yahoojp"
#define YAHOOJP_NAME   "Yahoo JAPAN"
#define YAHOOJP_DOMAIN (g_quark_from_static_string(YAHOOJP_ID))

#define YAHOOJP_TYPE_PROTOCOL             (yahoojp_protocol_get_type())
#define YAHOOJP_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), YAHOOJP_TYPE_PROTOCOL, YahooJPProtocol))
#define YAHOOJP_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), YAHOOJP_TYPE_PROTOCOL, YahooJPProtocolClass))
#define YAHOOJP_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), YAHOOJP_TYPE_PROTOCOL))
#define YAHOOJP_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), YAHOOJP_TYPE_PROTOCOL))
#define YAHOOJP_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), YAHOOJP_TYPE_PROTOCOL, YahooJPProtocolClass))

typedef struct _YahooJPProtocol
{
	YahooProtocol parent;
} YahooJPProtocol;

typedef struct _YahooJPProtocolClass
{
	YahooProtocolClass parent_class;
} YahooJPProtocolClass;

/**
 * Returns the GType for the YahooJPProtocol object.
 */
GType yahoojp_protocol_get_type(void);

#endif /* _YAHOOJP_H_ */
