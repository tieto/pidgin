/**
 * @file gtknotify.h GTK+ Notification API
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 */
#ifndef _PIDGINNOTIFY_H_
#define _PIDGINNOTIFY_H_

#include "notify.h"
#include "pounce.h"

G_BEGIN_DECLS

/**
 * Adds a buddy pounce to the buddy pounce dialog
 *
 * @param account	The account
 * @param pounce	The pounce
 * @param alias		The buddy alias
 * @param event		Event description
 * @param message	Pounce message
 * @param date		Pounce date
 */
void pidgin_notify_pounce_add(PurpleAccount *account, PurplePounce *pounce,
		const char *alias, const char *event, const char *message, const char *date);

/**
 * Returns the UI operations structure for GTK+ notification functions.
 *
 * @return The GTK+ UI notify operations structure.
 */
PurpleNotifyUiOps *pidgin_notify_get_ui_ops(void);

/**
 * Initializes the GTK+ notifications subsystem.
 */
void pidgin_notify_init(void);

/**
 * Uninitialized the GTK+ notifications subsystem.
 */
void pidgin_notify_uninit(void);

G_END_DECLS

#endif /* _PIDGINNOTIFY_H_ */
