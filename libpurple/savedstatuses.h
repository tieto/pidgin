/* purple
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
 */
/**
 * SECTION:savedstatuses
 * @section_id: libpurple-savedstatuses
 * @short_description: <filename>savedstatuses.h</filename>
 * @title: Saved Status API
 * @see_also: <link linkend="chapter-signals-savedstatus">Saved Status signals</link>
 */

#ifndef _PURPLE_SAVEDSTATUSES_H_
#define _PURPLE_SAVEDSTATUSES_H_

#define PURPLE_TYPE_SAVEDSTATUS  (purple_savedstatus_get_g_type())

/**
 * PurpleSavedStatus:
 *
 * Saved statuses don't really interact much with the rest of Purple.  It
 * could really be a plugin.  It's just a list of away states.  When
 * a user chooses one of the saved states, their Purple accounts are set
 * to the settings of that state.
 *
 * In the savedstatus API, there is the concept of a 'transient'
 * saved status.  A transient saved status is one that is not
 * permanent.  Purple will removed it automatically if it isn't
 * used for a period of time.  Transient saved statuses don't
 * have titles and they don't show up in the list of saved
 * statuses.  In fact, if a saved status does not have a title
 * then it is transient.  If it does have a title, then it is not
 * transient.
 *
 * What good is a transient status, you ask?  They can be used to
 * keep track of the user's 5 most recently used statuses, for
 * example.  Basically if they just set a message on the fly,
 * we'll cache it for them in case they want to use it again.  If
 * they don't use it again, we'll just delete it.
 */
/*
 * TODO: Hmm.  We should probably just be saving PurplePresences.  That's
 *       something we should look into once the status box gets fleshed
 *       out more.
 */
typedef struct _PurpleSavedStatus     PurpleSavedStatus;

typedef struct _PurpleSavedStatusSub  PurpleSavedStatusSub;

#include "status.h"

G_BEGIN_DECLS

/**************************************************************************/
/** @name Saved status subsystem                                          */
/**************************************************************************/
/*@{*/

/**
 * purple_savedstatus_get_g_type:
 *
 * Returns: The #GType for the #PurpleSavedStatus boxed structure.
 */
/* TODO Boxing of PurpleSavedStatus is a temporary solution to having a GType
 *      for saved statuses. This should rather be a GObject instead of a GBoxed.
 */
GType purple_savedstatus_get_g_type(void);

/**
 * purple_savedstatus_new:
 * @title:     The title of the saved status.  This must be
 *                  unique.  Or, if you want to create a transient
 *                  saved status, then pass in NULL.
 * @type:      The type of saved status.
 *
 * Create a new saved status.  This will add the saved status to the
 * list of saved statuses and writes the revised list to status.xml.
 *
 * Returns: The newly created saved status, or NULL if the title you
 *         used was already taken.
 */
PurpleSavedStatus *purple_savedstatus_new(const char *title,
									  PurpleStatusPrimitive type);

/**
 * purple_savedstatus_set_title:
 * @status:  The saved status.
 * @title:   The title of the saved status.
 *
 * Set the title for the given saved status.
 */
void purple_savedstatus_set_title(PurpleSavedStatus *status,
								const char *title);

/**
 * purple_savedstatus_set_type:
 * @status:  The saved status.
 * @type:    The type of saved status.
 *
 * Set the type for the given saved status.
 */
void purple_savedstatus_set_type(PurpleSavedStatus *status,
							   PurpleStatusPrimitive type);

/**
 * purple_savedstatus_set_message:
 * @status:  The saved status.
 * @message: The message, or NULL if you want to unset the
 *                message for this status.
 *
 * Set the message for the given saved status.
 */
void purple_savedstatus_set_message(PurpleSavedStatus *status,
								  const char *message);

/**
 * purple_savedstatus_set_substatus:
 * @status:	The saved status.
 * @account:	The account.
 * @type:		The status type for the account in the staved
 *                  status.
 * @message:	The message for the account in the substatus.
 *
 * Set a substatus for an account in a saved status.
 */
void purple_savedstatus_set_substatus(PurpleSavedStatus *status,
									const PurpleAccount *account,
									const PurpleStatusType *type,
									const char *message);

/**
 * purple_savedstatus_unset_substatus:
 * @saved_status: The saved status.
 * @account:      The account.
 *
 * Unset a substatus for an account in a saved status.  This clears
 * the previosly set substatus for the PurpleSavedStatus.  If this
 * saved status is activated then this account will use the default
 * status type and message.
 */
void purple_savedstatus_unset_substatus(PurpleSavedStatus *saved_status,
												  const PurpleAccount *account);

/**
 * purple_savedstatus_delete:
 * @title: The title of the saved status.
 *
 * Delete a saved status.  This removes the saved status from the list
 * of saved statuses, and writes the revised list to status.xml.
 *
 * Returns: TRUE if the status was successfully deleted.  FALSE if the
 *         status could not be deleted because no saved status exists
 *         with the given title.
 */
gboolean purple_savedstatus_delete(const char *title);

/**
 * purple_savedstatus_delete_by_status:
 * @saved_status: the status to delete, the pointer is invalid after
 *        the call
 *
 * Delete a saved status.  This removes the saved status from the list
 * of saved statuses, and writes the revised list to status.xml.
 */
void purple_savedstatus_delete_by_status(PurpleSavedStatus *saved_status);

/**
 * purple_savedstatuses_get_all:
 *
 * Returns all saved statuses.
 *
 * Returns: (transfer none): A list of saved statuses.
 */
GList *purple_savedstatuses_get_all(void);

/**
 * purple_savedstatuses_get_popular:
 * @how_many: The maximum number of saved statuses
 *                 to return, or '0' to get all saved
 *                 statuses sorted by popularity.
 *
 * Returns the n most popular saved statuses.  "Popularity" is
 * determined by when the last time a saved_status was used and
 * how many times it has been used. Transient statuses without
 * messages are not included in the list.
 *
 * Returns: A linked list containing at most how_many
 *         PurpleSavedStatuses.  This list should be
 *         g_list_free'd by the caller (but the
 *         PurpleSavedStatuses must not be free'd).
 */
GList *purple_savedstatuses_get_popular(unsigned int how_many);

/**
 * purple_savedstatus_get_current:
 *
 * Returns the currently selected saved status.  If we are idle
 * then this returns purple_savedstatus_get_idleaway().  Otherwise
 * it returns purple_savedstatus_get_default().
 *
 * Returns: A pointer to the in-use PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_current(void);

/**
 * purple_savedstatus_get_default:
 *
 * Returns the default saved status that is used when our
 * accounts are not idle-away.
 *
 * Returns: A pointer to the in-use PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_default(void);

/**
 * purple_savedstatus_get_idleaway:
 *
 * Returns the saved status that is used when your
 * accounts become idle-away.
 *
 * Returns: A pointer to the idle-away PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_idleaway(void);

/**
 * purple_savedstatus_is_idleaway:
 *
 * Return TRUE if we are currently idle-away.  Otherwise
 * returns FALSE.
 *
 * Returns: TRUE if our accounts have been set to idle-away.
 */
gboolean purple_savedstatus_is_idleaway(void);

/**
 * purple_savedstatus_set_idleaway:
 * @idleaway: TRUE if accounts should be switched to use the
 *                 idle-away saved status.  FALSE if they should
 *                 be switched to use the default status.
 *
 * Set whether accounts in Purple are idle-away or not.
 */
void purple_savedstatus_set_idleaway(gboolean idleaway);

/**
 * purple_savedstatus_get_startup:
 *
 * Returns the status to be used when purple is starting up
 *
 * Returns: A pointer to the startup PurpleSavedStatus.
 *         This function never returns NULL.
 */
PurpleSavedStatus *purple_savedstatus_get_startup(void);

/**
 * purple_savedstatus_find:
 * @title: The name of the saved status.
 *
 * Finds a saved status with the specified title.
 *
 * Returns: The saved status if found, or NULL.
 */
PurpleSavedStatus *purple_savedstatus_find(const char *title);

/**
 * purple_savedstatus_find_by_creation_time:
 * @creation_time: The timestamp when the saved
 *        status was created.
 *
 * Finds a saved status with the specified creation time.
 *
 * Returns: The saved status if found, or NULL.
 */
PurpleSavedStatus *purple_savedstatus_find_by_creation_time(time_t creation_time);

/**
 * purple_savedstatus_find_transient_by_type_and_message:
 * @type: The PurpleStatusPrimitive for the status you're trying
 *        to find.
 * @message: The message for the status you're trying
 *        to find.
 *
 * Finds a saved status with the specified primitive and message.
 *
 * Returns: The saved status if found, or NULL.
 */
PurpleSavedStatus *purple_savedstatus_find_transient_by_type_and_message(PurpleStatusPrimitive type, const char *message);

/**
 * purple_savedstatus_is_transient:
 * @saved_status: The saved status.
 *
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
 * restore it when Purple restarts.
 *
 * Returns: TRUE if the saved status is transient.
 */
gboolean purple_savedstatus_is_transient(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_get_title:
 * @saved_status: The saved status.
 *
 * Return the name of a given saved status.
 *
 * Returns: The title.  This value may be a static buffer which may
 *         be overwritten on subsequent calls to this function.  If
 *         you need a reference to the title for prolonged use then
 *         you should make a copy of it.
 */
const char *purple_savedstatus_get_title(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_get_type:
 * @saved_status: The saved status.
 *
 * Return the type of a given saved status.
 *
 * Returns: The primitive type.
 */
PurpleStatusPrimitive purple_savedstatus_get_type(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_get_message:
 * @saved_status: The saved status.
 *
 * Return the default message of a given saved status.
 *
 * Returns: The message.  This will return NULL if the saved
 *         status does not have a message.  This will
 *         contain the normal markup that is created by
 *         Purple's IMHTML (basically HTML markup).
 */
const char *purple_savedstatus_get_message(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_get_creation_time:
 * @saved_status: The saved status.
 *
 * Return the time in seconds-since-the-epoch when this
 * saved status was created.  Note: For any status created
 * by Purple 1.5.0 or older this value will be invalid and
 * very small (close to 0).  This is because Purple 1.5.0
 * and older did not record the timestamp when the status
 * was created.
 *
 * However, this value is guaranteed to be a unique
 * identifier for the given saved status.
 *
 * Returns: The timestamp when this saved status was created.
 */
time_t purple_savedstatus_get_creation_time(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_has_substatuses:
 * @saved_status: The saved status.
 *
 * Determine if a given saved status has "substatuses,"
 * or if it is a simple status (the same for all
 * accounts).
 *
 * Returns: TRUE if the saved_status has substatuses.
 *         FALSE otherwise.
 */
gboolean purple_savedstatus_has_substatuses(const PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_get_substatus:
 * @saved_status: The saved status.
 * @account:      The account.
 *
 * Get the substatus for an account in a saved status.
 *
 * Returns: The PurpleSavedStatusSub for the account, or NULL if
 *         the given account does not have a substatus that
 *         differs from the default status of this PurpleSavedStatus.
 */
PurpleSavedStatusSub *purple_savedstatus_get_substatus(
									const PurpleSavedStatus *saved_status,
									const PurpleAccount *account);

/**
 * purple_savedstatus_substatus_get_type:
 * @substatus: The substatus.
 *
 * Get the status type of a given substatus.
 *
 * Returns: The status type.
 */
const PurpleStatusType *purple_savedstatus_substatus_get_type(const PurpleSavedStatusSub *substatus);

/**
 * purple_savedstatus_substatus_get_message:
 * @substatus: The substatus.
 *
 * Get the message of a given substatus.
 *
 * Returns: The message of the substatus, or NULL if this substatus does
 *         not have a message.
 */
const char *purple_savedstatus_substatus_get_message(const PurpleSavedStatusSub *substatus);

/**
 * purple_savedstatus_activate:
 * @saved_status: The status you want to set your accounts to.
 *
 * Sets the statuses for all your accounts to those specified
 * by the given saved_status.  This function calls
 * purple_savedstatus_activate_for_account() for all your accounts.
 */
void purple_savedstatus_activate(PurpleSavedStatus *saved_status);

/**
 * purple_savedstatus_activate_for_account:
 * @saved_status: The status you want to set your accounts to.
 * @account:      The account whose statuses you want to change.
 *
 * Sets the statuses for a given account to those specified
 * by the given saved_status.
 */
void purple_savedstatus_activate_for_account(const PurpleSavedStatus *saved_status, PurpleAccount *account);

/**
 * purple_savedstatuses_get_handle:
 *
 * Get the handle for the status subsystem.
 *
 * Returns: the handle to the status subsystem
 */
void *purple_savedstatuses_get_handle(void);

/**
 * purple_savedstatuses_init:
 *
 * Initializes the status subsystem.
 */
void purple_savedstatuses_init(void);

/**
 * purple_savedstatuses_uninit:
 *
 * Uninitializes the status subsystem.
 */
void purple_savedstatuses_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_SAVEDSTATUSES_H_ */

