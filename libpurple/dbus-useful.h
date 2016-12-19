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
#ifndef _PURPLE_DBUS_USEFUL_H_
#define _PURPLE_DBUS_USEFUL_H_
/**
 * SECTION:dbus-useful
 * @section_id: libpurple-dbus-useful
 * @short_description: <filename>dbus-useful.h</filename>
 * @title: Misc functions for DBUS server
 */

#include "conversation.h"

G_BEGIN_DECLS

PurpleAccount *purple_accounts_find_ext(const char *name, const char *protocol_id,
				    gboolean (*account_test)(const PurpleAccount *account));

PurpleAccount *purple_accounts_find_any(const char *name, const char *protocol);

PurpleAccount *purple_accounts_find_connected(const char *name, const char *protocol);

G_END_DECLS

#endif
