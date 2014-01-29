/**
 * @file gtksavedstatuses.h GTK+ Saved Status Editor UI
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
#ifndef _PIDGINSAVEDSTATUSES_H_
#define _PIDGINSAVEDSTATUSES_H_

#include "savedstatuses.h"
#include "status.h"

G_BEGIN_DECLS

/**
 * Shows the status window.
 */
void pidgin_status_window_show(void);

/**
 * Hides the status window.
 */
void pidgin_status_window_hide(void);

/**
 * Shows a status editor (used for adding a new saved status or
 * editing an already existing saved status).
 *
 * @param edit   TRUE if we want to edit an existing saved
 *               status or FALSE to create a new one.  You
 *               can not edit transient statuses--they don't
 *               have titles.  If you want to edit a transient
 *               status, set this to FALSE and seed the dialog
 *               with the transient status using the status
 *               parameter to this function.
 * @param status If edit is TRUE then this should be a
 *               pointer to the PurpleSavedStatus to edit.
 *               If edit is FALSE then this can be NULL,
 *               or you can pass in a saved status to
 *               seed the initial values of the new status.
 */
void pidgin_status_editor_show(gboolean edit, PurpleSavedStatus *status);

/**
 * Creates a dropdown menu of saved statuses and calls a callback
 * when one is selected
 *
 * @param status   The default saved_status to show as 'selected'
 * @param callback The callback to call when the selection changes
 * @return         The menu widget
 */
GtkWidget *pidgin_status_menu(PurpleSavedStatus *status, GCallback callback);

/**
 * Returns the GTK+ status handle.
 *
 * @return The handle to the GTK+ status system.
 */
void *pidgin_status_get_handle(void);

/**
 * Initializes the GTK+ status system.
 */
void pidgin_status_init(void);

/**
 * Uninitializes the GTK+ status system.
 */
void pidgin_status_uninit(void);

G_END_DECLS

#endif /* _PIDGINSAVEDSTATUSES_H_ */
