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

/*
 * TODO: Hmm.  We should probably just be saving GaimPresences.  That's
 *       something we should look into once the status box gets fleshed
 *       out more.
 */

typedef struct _GaimSavedStatus     GaimSavedStatus;
typedef struct _GaimSavedStatusSub  GaimSavedStatusSub;

#include "status.h"

/**************************************************************************/
/** @name Saved status subsystem                                          */
/**************************************************************************/
/*@{*/

/**
 * Create a new saved status.  This will add the saved status to the
 * list of saved statuses and writes the revised list to status.xml.
 *
 * @param title     The title of the saved status.  This must be
 *                  unique.
 * @param type      The type of saved status.
 *
 * @return The newly created saved status, or NULL if the title you
 *         used was already taken.
 */
GaimSavedStatus *gaim_savedstatus_new(const char *title,
									  GaimStatusPrimitive type);

/**
 * Set the title for the given saved status.
 *
 * @param status  The saved status.
 * @param title   The title of the saved status.
 */
void gaim_savedstatus_set_title(GaimSavedStatus *status,
								const char *title);

/**
 * Set the type for the given saved status.
 *
 * @param status  The saved status.
 * @param type    The type of saved status.
 */
void gaim_savedstatus_set_type(GaimSavedStatus *status,
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
 * Set a substatus for an account in a saved status.
 *
 * @param status	The saved status.
 * @param account	The account.
 * @param type		The status type for the account in the staved
 *                  status.
 * @param message	The message for the account in the substatus.
 */
void gaim_savedstatus_set_substatus_for_account(GaimSavedStatus *status,
												const GaimAccount *account,
												const GaimStatusType *type,
												const char *message);

/**
 * Unset a substatus for an account in a saved status.  This clears
 * the previosly set substatus for the GaimSavedStatus.  If this
 * saved status is activated then this account will use the default
 * status type and message.
 *
 * @param status	The saved status.
 * @param account	The account.
*/
void gaim_savedstatus_unset_substatus_for_account(GaimSavedStatus *saved_status,
												  const GaimAccount *account);

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
 * Determines if a given saved status is "transient."
 * A transient saved status is one that was not
 * explicitly added by the user.  Transient statuses
 * are automatically removed if they are not used
 * for a period of time.
 *
 * A transient saved statuses is automatically
 * created by the status box when the user sets himself
 * to one of the generic primitive statuses.  The reason
 * we need to save this status information is so we can
 * restore it when Gaim restarts.
 *
 * @param saved_status The saved status.
 *
 * @return TRUE if the saved status is transient.
 */
gboolean gaim_savedstatus_is_transient(const GaimSavedStatus *saved_status);

/**
 * Return the name of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The title.
 */
const char *gaim_savedstatus_get_title(const GaimSavedStatus *saved_status);

/**
 * Return the type of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
GaimStatusPrimitive gaim_savedstatus_get_type(const GaimSavedStatus *saved_status);

/**
 * Return the default message of a given saved status.
 *
 * @param saved_status The saved status.
 *
 * @return The name.
 */
const char *gaim_savedstatus_get_message(const GaimSavedStatus *saved_status);

/**
 * Determine if a given saved status has "substatuses,"
 * or if it is a simple status (the same for all
 * accounts).
 *
 * @param saved_status The saved status.
 *
 * @return TRUE if the saved_status has substatuses.
 *         FALSE otherwise.
 */
gboolean gaim_savedstatus_has_substatuses(const GaimSavedStatus *saved_status);

/**
 * Get the substatus for an account in a saved status.
 *
 * @param status  The saved status.
 * @param account The account.
 *
 * @return The GaimSavedStatusSub for the account, or NULL if
 *         the given account does not have a substatus that
 *         differs from the default status of this GaimSavedStatus.
 */
GaimSavedStatusSub *gaim_savedstatus_get_substatus_for_account(
									const GaimSavedStatus *saved_status,
									const GaimAccount *account);

/**
 * Get the status type of a given substatus.
 *
 * @param substatus The substatus.
 *
 * @return The status type.
 */
const GaimStatusType *gaim_savedstatus_substatus_get_type(const GaimSavedStatusSub *substatus);

/**
 * Get the message of a given substatus.
 *
 * @param substatus The substatus.
 *
 * @return The message of the substatus, or NULL if this substatus does
 *         not have a message.
 */
const char *gaim_savedstatus_substatus_get_message(const GaimSavedStatusSub *substatus);

/**
 * Sets the statuses for all your accounts to those specified
 * by the given saved_status.  This function calls
 * gaim_savedstatus_activate_for_account() for all your accounts.
 *
 * @param saved_status The status you want to set your accounts to.
 */
void gaim_savedstatus_activate(const GaimSavedStatus *saved_status);

/**
 * Sets the statuses for a given account to those specified
 * by the given saved_status.
 *
 * @param saved_status The status you want to set your accounts to.
 * @param account      The account whose statuses you want to change.
 */
void gaim_savedstatus_activate_for_account(const GaimSavedStatus *saved_status, GaimAccount *account);

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
