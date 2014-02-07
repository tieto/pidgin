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
/**
 * SECTION:gtknotify
 * @section_id: pidgin-gtknotify
 * @short_description: <filename>gtknotify.h</filename>
 * @title: Notification API
 */

#include "notify.h"
#include "pounce.h"

G_BEGIN_DECLS

/**
 * pidgin_notify_pounce_add:
 * @account:	The account
 * @pounce:	The pounce
 * @alias:		The buddy alias
 * @event:		Event description
 * @message:	Pounce message
 * @date:		Pounce date
 *
 * Adds a buddy pounce to the buddy pounce dialog
 */
void pidgin_notify_pounce_add(PurpleAccount *account, PurplePounce *pounce,
		const char *alias, const char *event, const char *message, const char *date);

/**
 * pidgin_notify_get_ui_ops:
 *
 * Returns the UI operations structure for GTK+ notification functions.
 *
 * Returns: The GTK+ UI notify operations structure.
 */
PurpleNotifyUiOps *pidgin_notify_get_ui_ops(void);

/**
 * pidgin_notify_init:
 *
 * Initializes the GTK+ notifications subsystem.
 */
void pidgin_notify_init(void);

/**
 * pidgin_notify_uninit:
 *
 * Uninitialized the GTK+ notifications subsystem.
 */
void pidgin_notify_uninit(void);

/**
 * pidgin_notify_emails_pending:
 *
 * Returns TRUE if there are unseen emails, FALSE otherwise.
 *
 * Returns: TRUE if there are unseen emails, FALSE otherwise.
 */
gboolean pidgin_notify_emails_pending(void);

/**
 * pidgin_notify_emails_present:
 *
 * Presents mail dialog to the user.
 *
 * Returns: void.
 */
void pidgin_notify_emails_present(void *data);

G_END_DECLS

#endif /* _PIDGINNOTIFY_H_ */
