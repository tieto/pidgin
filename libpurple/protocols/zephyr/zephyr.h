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
#ifndef _ZEPHYR_H_
#define _ZEPHYR_H_

#include "protocol.h"

#define ZEPHYR_ID     "prpl-zephyr"
#define ZEPHYR_NAME   "Zephyr"
#define ZEPHYR_DOMAIN (g_quark_from_static_string(ZEPHYR_ID))

#define ZEPHYR_TYPE_PROTOCOL             (zephyr_protocol_get_type())
#define ZEPHYR_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), ZEPHYR_TYPE_PROTOCOL, ZephyrProtocol))
#define ZEPHYR_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), ZEPHYR_TYPE_PROTOCOL, ZephyrProtocolClass))
#define ZEPHYR_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZEPHYR_TYPE_PROTOCOL))
#define ZEPHYR_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), ZEPHYR_TYPE_PROTOCOL))
#define ZEPHYR_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ZEPHYR_TYPE_PROTOCOL, ZephyrProtocolClass))

typedef struct _ZephyrProtocol
{
	PurpleProtocol parent;
} ZephyrProtocol;

typedef struct _ZephyrProtocolClass
{
	PurpleProtocolClass parent_class;
} ZephyrProtocolClass;

/**
 * Returns the GType for the ZephyrProtocol object.
 */
GType zephyr_protocol_get_type(void);

#endif /* _ZEPHYR_H_ */
