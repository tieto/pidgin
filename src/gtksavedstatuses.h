/**
 * @file gtksavedstatuses.h GTK+ Saved Status Editor UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKSAVEDSTATUSES_H_
#define _GAIM_GTKSAVEDSTATUSES_H_

#include "savedstatuses.h"
#include "status.h"

/**
 * Shows the status window.
 */
void gaim_gtk_status_window_show(void);

/**
 * Hides the status window.
 */
void gaim_gtk_status_window_hide(void);

/**
 * Shows a status editor (used for adding a new saved status or
 * editing an already existing saved status).
 *
 * @param status The saved status to edit, or @c NULL if you
 *               want to add a new saved status.
 */
void gaim_gtk_status_editor_show(GaimSavedStatus *status);

/**
 * Returns the gtkstatus handle.
 *
 * @return The handle to the GTK+ status system.
 */
void *gaim_gtk_status_get_handle(void);

/**
 * Initializes the GTK+ status system.
 */
void gaim_gtk_status_init(void);

/**
 * Uninitializes the GTK+ status system.
 */
void gaim_gtk_status_uninit(void);

#endif /* _GAIM_GTKSAVEDSTATUSES_H_ */
