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

typedef struct _GaimStatusSaved     GaimStatusSaved;
typedef struct _GaimStatusSavedSub  GaimStatusSavedSub;

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
 * @return The newly created saved status.
 */
GaimStatusSaved *gaim_savedstatuses_new(const char *title, GaimStatusPrimitive type);

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
gboolean gaim_savedstatuses_delete(const char *title);

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
GaimStatusSaved *gaim_savedstatuses_find(const char *title);

/**
 * Create a new saved status and add it to the list
 * of saved statuses.
 *
 * @param title The title for the new saved status.
 * @param type  The type of saved status.
 *
 * @return The newly created saved status.
 */
GaimStatusSaved *gaim_savedstatuses_new(const char *title,
									  GaimStatusPrimitive type);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The title.
 */
const char *gaim_savedstatuses_get_title(const GaimStatusSaved *saved_status);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
GaimStatusPrimitive gaim_savedstatuses_get_type(const GaimStatusSaved *saved_status);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
const char *gaim_savedstatuses_get_message(const GaimStatusSaved *saved_status);

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
