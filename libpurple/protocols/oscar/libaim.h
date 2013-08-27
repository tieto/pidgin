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
#ifndef _LIBAIM_H_
#define _LIBAIM_H_

#include "oscar.h"

#define AIM_ID     "prpl-aim"
#define AIM_NAME   "AIM"
#define AIM_DOMAIN (g_quark_from_static_string(AIM_ID))

#define AIM_TYPE_PROTOCOL             (aim_protocol_get_type())
#define AIM_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), AIM_TYPE_PROTOCOL, AIMProtocol))
#define AIM_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), AIM_TYPE_PROTOCOL, AIMProtocolClass))
#define AIM_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), AIM_TYPE_PROTOCOL))
#define AIM_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), AIM_TYPE_PROTOCOL))
#define AIM_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), AIM_TYPE_PROTOCOL, AIMProtocolClass))

typedef struct _AIMProtocol
{
	OscarProtocol parent;
} AIMProtocol;

typedef struct _AIMProtocolClass
{
	OscarProtocolClass parent_class;
} AIMProtocolClass;

/**
 * Returns the GType for the AIMProtocol object.
 */
GType aim_protocol_get_type(void);

#endif /* _LIBAIM_H_ */
