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
/**
 * SECTION:gtkprivacy
 * @section_id: pidgin-gtkprivacy
 * @short_description: <filename>gtkprivacy.h</filename>
 * @title: Privacy UI
 */

#ifndef _PIDGINPRIVACY_H_
#define _PIDGINPRIVACY_H_

#include "account.h"

G_BEGIN_DECLS

/**
 * pidgin_privacy_init:
 *
 * Initializes the GTK+ privacy subsystem.
 */
void pidgin_privacy_init(void);

/**
 * pidgin_privacy_dialog_show:
 *
 * Shows the privacy dialog.
 */
void pidgin_privacy_dialog_show(void);

/**
 * pidgin_privacy_dialog_hide:
 *
 * Hides the privacy dialog.
 */
void pidgin_privacy_dialog_hide(void);

/**
 * pidgin_request_add_permit:
 * @account: The account.
 * @name:    The name of the user to add.
 *
 * Requests confirmation to add a user to the allow list for an account,
 * and then adds it.
 *
 * If @name is not specified, an input dialog will be presented.
 */
void pidgin_request_add_permit(PurpleAccount *account, const char *name);

/**
 * pidgin_request_add_block:
 * @account: The account.
 * @name:    The name of the user to add.
 *
 * Requests confirmation to add a user to the block list for an account,
 * and then adds it.
 *
 * If @name is not specified, an input dialog will be presented.
 */
void pidgin_request_add_block(PurpleAccount *account, const char *name);

G_END_DECLS

#endif /* _PIDGINPRIVACY_H_ */
