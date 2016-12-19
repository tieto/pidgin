/*
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
#ifndef _PURPLE_DBUS_MAYBE_H_
#define _PURPLE_DBUS_MAYBE_H_
/**
 * SECTION:dbus-maybe
 * @section_id: libpurple-dbus-maybe
 * @short_description: <filename>dbus-maybe.h</filename>
 * @title: DBUS Wrappers
 *
 * This file contains macros that wrap calls to the purple dbus module.
 * These macros call the appropriate functions if the build includes
 * dbus support and do nothing otherwise.  See "dbus-server.h" for
 * documentation.
 */

#ifdef HAVE_DBUS

#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#include "dbus-server.h"

/* this provides a type check */
#define PURPLE_DBUS_REGISTER_POINTER(ptr, type) { \
    type *typed_ptr = ptr; \
    purple_dbus_register_pointer(typed_ptr, PURPLE_DBUS_TYPE(type));	\
}
#define PURPLE_DBUS_UNREGISTER_POINTER(ptr) purple_dbus_unregister_pointer(ptr)

#else  /* !HAVE_DBUS */

#define PURPLE_DBUS_REGISTER_POINTER(ptr, type) { \
    if (ptr) {} \
}

#define PURPLE_DBUS_UNREGISTER_POINTER(ptr)

#endif	/* HAVE_DBUS */

#endif
