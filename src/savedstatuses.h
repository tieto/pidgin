/**
 * @file savedstatuses.h Saved Status API
 * @ingroup core
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
#ifndef _GAIM_SAVEDSTATUSES_H_
#define _GAIM_SAVEDSTATUSES_H_

/**
 * Saved statuses don't really interact much with the rest of Gaim.  It
 * could really be a plugin.  It's just a list of away states.  When
 * a user chooses one of the saved states, their Gaim accounts are set
 * to the settings of that state.
 */

typedef struct _GaimSavedStatus     GaimSavedStatus;
typedef struct _GaimSavedStatusSub  GaimSavedStatusSub;

/**************************************************************************/
/** @name Saved status subsystem                                          */
/**************************************************************************/
/*@{*/

/**
 * Create a new saved status.  This will add the saved status to the
 * list of saved statuses and writes the revised list to status.xml.
 *
 * @param title The title of the saved status.  This must be unique.
 * @param type  The type of saved status.
 *
 * @return The newly created saved status, or NULL if the title you
 *         used was already taken.
 */
GaimSavedStatus *gaim_savedstatus_new(const char *title,
									  GaimStatusPrimitive type);

/**
 * Set the message for the given saved status.
 *
 * @param status  The saved status.
 * @param message The message, or NULL if you want to unset the
 *                message for this status.
 */
void gaim_savedstatus_set_message(GaimSavedStatus *status,
								  const char *message);

/**
 * Delete a saved status.  This removes the saved status from the list
 * of saved statuses, and writes the revised list to status.xml.
 *
 * @param title The title of the saved status.
 *
 * @return TRUE if the status was successfully deleted.  FALSE if the
 *         status could not be deleted because no saved status exists
 *         with the given title.
 */
gboolean gaim_savedstatus_delete(const char *title);

/**
 * Returns all saved statuses.
 *
 * @return A list of saved statuses.
 */
const GList *gaim_savedstatuses_get_all(void);

/**
 * Finds a saved status with the specified title.
 *
 * @param title The name of the saved status.
 *
 * @return The saved status if found, or NULL.
 */
GaimSavedStatus *gaim_savedstatus_find(const char *title);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The title.
 */
const char *gaim_savedstatus_get_title(const GaimSavedStatus *saved_status);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
GaimStatusPrimitive gaim_savedstatus_get_type(const GaimSavedStatus *saved_status);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
const char *gaim_savedstatus_get_message(const GaimSavedStatus *saved_status);

/**
 * Get the handle for the status subsystem.
 *
 * @return the handle to the status subsystem
 */
void *gaim_savedstatuses_get_handle();

/**
 * Initializes the status subsystem.
 */
void gaim_savedstatuses_init(void);

/**
 * Uninitializes the status subsystem.
 */
void gaim_savedstatuses_uninit(void);

/*@}*/

#endif /* _GAIM_SAVEDSTATUSES_H_ */
