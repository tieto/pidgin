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
#ifndef _YAHOO_H_
#define _YAHOO_H_

#include "protocol.h"

#define YAHOO_ID     "prpl-yahoo"
#define YAHOO_NAME   "Yahoo"
#define YAHOO_DOMAIN (g_quark_from_static_string(YAHOO_ID))

#define YAHOO_TYPE_PROTOCOL             (yahoo_protocol_get_type())
#define YAHOO_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), YAHOO_TYPE_PROTOCOL, YahooProtocol))
#define YAHOO_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), YAHOO_TYPE_PROTOCOL, YahooProtocolClass))
#define YAHOO_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), YAHOO_TYPE_PROTOCOL))
#define YAHOO_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), YAHOO_TYPE_PROTOCOL))
#define YAHOO_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), YAHOO_TYPE_PROTOCOL, YahooProtocolClass))

typedef struct _YahooProtocol
{
	PurpleProtocol parent;
} YahooProtocol;

typedef struct _YahooProtocolClass
{
	PurpleProtocolClass parent_class;
} YahooProtocolClass;

/**
 * Returns the GType for the YahooProtocol object.
 */
GType yahoo_protocol_get_type(void);

#endif /* _YAHOO_H_ */
