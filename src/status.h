/**
 * @file status.h Status API
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
#ifndef _GAIM_STATUS_H_
#define _GAIM_STATUS_H_

typedef struct _GaimStatusType   GaimStatusType;
typedef struct _GaimStatusAttr   GaimStatusAttr;
typedef struct _GaimPresence     GaimPresence;
typedef struct _GaimStatus       GaimStatus;

/**
 * A context for a presence.
 *
 * The context indicates what the presence applies to.
 */
typedef enum
{
	GAIM_PRESENCE_CONTEXT_UNSET   = 0,
	GAIM_PRESENCE_CONTEXT_ACCOUNT,
	GAIM_PRESENCE_CONTEXT_CONV,
	GAIM_PRESENCE_CONTEXT_BUDDY

} GaimPresenceContext;

/**
 * A primitive defining the basic structure of a status type.
 */
typedef enum
{
	GAIM_STATUS_UNSET     = -1,
	GAIM_STATUS_OFFLINE   = 0,
	GAIM_STATUS_ONLINE,
	GAIM_STATUS_AVAILABLE,
	GAIM_STATUS_UNAVAILABLE,
	GAIM_STATUS_HIDDEN,
	GAIM_STATUS_AWAY,
	GAIM_STATUS_EXTENDED_AWAY

} GaimStatusPrimitive;

#include "account.h"
#include "blist.h"
#include "conversation.h"
#include "value.h"

/**************************************************************************/
/** @name GaimStatusType API                                              */
/**************************************************************************/
/*@{*/

/**
 * Creates a new status type.
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type.
 * @param name          The name presented to the user.
 * @param saveable      TRUE if the information set for this status by the
 *                      user can be saved for future sessions.
 * @param user_settable TRUE if this is a status the user can manually set.
 * @param independent   TRUE if this is an independent (non-exclusive)
 *                      status type.
 *
 * @return A new status type.
 */
GaimStatusType *gaim_status_type_new_full(GaimStatusPrimitive primitive,
										  const char *id, const char *name,
										  gboolean saveable,
										  gboolean user_settable,
										  gboolean independent);

/**
 * Creates a new status type with some default values.
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type.
 * @param name          The name presented to the user.
 * @param user_settable TRUE if this is a status the user can manually set.
 *
 * @return A new status type.
 */
GaimStatusType *gaim_status_type_new(GaimStatusPrimitive primitive,
									 const char *id, const char *name,
									 gboolean user_settable);

/**
 * Creates a new status type with attributes.
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type.
 * @param name          The name presented to the user.
 * @param saveable      TRUE if the information set for this status by the
 *                      user can be saved for future sessions.
 * @param user_settable TRUE if this is a status the user can manually set.
 * @param independent   TRUE if this is an independent (non-exclusive)
 *                      status type.
 * @param attr_id       The ID of the first attribute.
 * @param attr_name     The name of the first attribute.
 * @param attr_value    The value type of the first attribute attribute.
 * @param ...           Additional attribute information.
 *
 * @return A new status type.
 */
GaimStatusType *gaim_status_type_new_with_attrs(GaimStatusPrimitive primitive,
												const char *id,
												const char *name,
												gboolean saveable,
												gboolean user_settable,
												gboolean independent,
												const char *attr_id,
												const char *attr_name,
												GaimValue *attr_value, ...);

/**
 * Destroys a status type.
 *
 * @param status_type The status type to destroy.
 */
void gaim_status_type_destroy(GaimStatusType *status_type);

/**
 * Sets a status type's primary attribute.
 *
 * The value for the primary attribute is used as the description for
 * the particular status type. An example is an away message. The message
 * would be the primary attribute.
 *
 * @param status_type The status type.
 * @param attr_id     The ID of the primary attribute.
 */
void gaim_status_type_set_primary_attr(GaimStatusType *status_type,
									   const char *attr_id);

/**
 * Adds an attribute to a status type.
 *
 * @param status_type The status type to add the attribute to.
 * @param id          The ID of the attribute.
 * @param name        The name presented to the user.
 * @param value       The value type of this attribute.
 */
void gaim_status_type_add_attr(GaimStatusType *status_type, const char *id,
							   const char *name, GaimValue *value);

/**
 * Adds multiple attributes to a status type.
 *
 * @param status_type The status type to add the attribute to.
 * @param id          The ID of the first attribute.
 * @param name        The description of the first attribute.
 * @param value       The value type of the first attribute attribute.
 * @param ...         Additional attribute information.
 */
void gaim_status_type_add_attrs(GaimStatusType *status_type, const char *id,
								const char *name, GaimValue *value, ...);

/**
 * Adds multiple attributes to a status type using a va_list.
 *
 * @param status_type The status type to add the attribute to.
 * @param args        The va_list of attributes.
 */
void gaim_status_type_add_attrs_vargs(GaimStatusType *status_type,
									  va_list args);

/**
 * Returns the primitive type of a status type.
 *
 * @param status_type The status type.
 *
 * @return The primitive type of the status type.
 */
GaimStatusPrimitive gaim_status_type_get_primitive(
	const GaimStatusType *status_type);

/**
 * Returns the ID of a status type.
 *
 * @param status_type The status type.
 *
 * @return The ID of the status type.
 */
const char *gaim_status_type_get_id(const GaimStatusType *status_type);

/**
 * Returns the name of a status type.
 *
 * @param status_type The status type.
 *
 * @return The name of the status type.
 */
const char *gaim_status_type_get_name(const GaimStatusType *status_type);

/**
 * Returns whether or not the status type is saveable.
 *
 * @param status_type The status type.
 *
 * @return TRUE if user-defined statuses based off this type are saveable.
 *         FALSE otherwise.
 */
gboolean gaim_status_type_is_saveable(const GaimStatusType *status_type);

/**
 * Returns whether or not the status type can be set or modified by the
 * user.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type can be set or modified by the user.
 *         FALSE if it's a protocol-set setting.
 */
gboolean gaim_status_type_is_user_settable(const GaimStatusType *status_type);

/**
 * Returns whether or not the status type is independent.
 *
 * Independent status types are non-exclusive. If other status types on
 * the same hierarchy level are set, this one will not be affected.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type is independent, or FALSE otherwise.
 */
gboolean gaim_status_type_is_independent(const GaimStatusType *status_type);

/**
 * Returns whether the status type is exclusive.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type is exclusive, FALSE otherwise.
 */
gboolean gaim_status_type_is_exclusive(const GaimStatusType *status_type);

/**
 * Returns whether or not a status type is available.
 *
 * Available status types are online and possibly hidden, but not away.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status is available, or FALSE otherwise.
 */
gboolean gaim_status_type_is_available(const GaimStatusType *status_type);

/**
 * Returns a status type's primary attribute ID.
 *
 * @param type The status type.
 *
 * @return The primary attribute's ID.
 */
const char *gaim_status_type_get_primary_attr(const GaimStatusType *type);

/**
 * Returns the attribute with the specified ID.
 *
 * @param status_type The status type containing the attribute.
 * @param id          The ID of the desired attribute.
 *
 * @return The attribute, if found. NULL otherwise.
 */
GaimStatusAttr *gaim_status_type_get_attr(const GaimStatusType *status_type,
										  const char *id);

/**
 * Returns a list of all attributes in a status type.
 *
 * @param status_type The status type.
 *
 * @return The list of attributes.
 */
const GList *gaim_status_type_get_attrs(const GaimStatusType *status_type);

/*@}*/

/**************************************************************************/
/** @name GaimStatusAttr API                                              */
/**************************************************************************/
/*@{*/

/**
 * Creates a new status attribute.
 *
 * @param id         The ID of the attribute.
 * @param name       The name presented to the user.
 * @param value_type The type of data contained in the attribute.
 *
 * @return A new status attribute.
 */
GaimStatusAttr *gaim_status_attr_new(const char *id, const char *name,
									 GaimValue *value_type);

/**
 * Destroys a status attribute.
 *
 * @param attr The status attribute to destroy.
 */
void gaim_status_attr_destroy(GaimStatusAttr *attr);

/**
 * Returns the ID of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's ID.
 */
const char *gaim_status_attr_get_id(const GaimStatusAttr *attr);

/**
 * Returns the name of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's name.
 */
const char *gaim_status_attr_get_name(const GaimStatusAttr *attr);

/**
 * Returns the value type of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's value type.
 */
GaimValue *gaim_status_attr_get_value_type(const GaimStatusAttr *attr);

/*@}*/

/**************************************************************************/
/** @name GaimStatus API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a new status.
 *
 * @param status_type The type of status.
 * @param presence    The parent presence.
 *
 * @return The new status.
 */
GaimStatus *gaim_status_new(GaimStatusType *status_type,
							GaimPresence *presence);

/**
 * Destroys a status.
 *
 * @param status The status to destroy.
 */
void gaim_status_destroy(GaimStatus *status);

/**
 * Sets whether or not a status is active.
 *
 * This should only be called by the account, conversation, and buddy APIs.
 *
 * @param status  The status.
 * @param active The active state.
 */
void gaim_status_set_active(GaimStatus *status, gboolean active);

/**
 * Sets the boolean value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 * @param value  The boolean value.
 */
void gaim_status_set_attr_boolean(GaimStatus *status, const char *id,
								  gboolean value);

/**
 * Sets the integer value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 * @param value  The integer value.
 */
void gaim_status_set_attr_int(GaimStatus *status, const char *id,
							  int value);

/**
 * Sets the string value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 * @param value  The string value.
 */
void gaim_status_set_attr_string(GaimStatus *status, const char *id,
								 const char *value);

/**
 * Returns the status's type.
 *
 * @param status The status.
 *
 * @return The status's type.
 */
GaimStatusType *gaim_status_get_type(const GaimStatus *status);

/**
 * Returns the status's presence.
 *
 * @param status The status.
 *
 * @return The status's presence.
 */
GaimPresence *gaim_status_get_presence(const GaimStatus *status);

/**
 * Returns the status's type ID.
 *
 * This is a convenience method for
 * gaim_status_type_get_id(gaim_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return The status's ID.
 */
const char *gaim_status_get_id(const GaimStatus *status);

/**
 * Returns the status's name.
 *
 * This is a convenience method for
 * gaim_status_type_get_name(gaim_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return The status's name.
 */
const char *gaim_status_get_name(const GaimStatus *status);

/**
 * Returns whether or not a status is independent.
 *
 * This is a convenience method for
 * gaim_status_type_is_independent(gaim_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is independent, or FALSE otherwise.
 */
gboolean gaim_status_is_independent(const GaimStatus *status);

/**
 * Returns whether or not a status is exclusive.
 *
 * This is a convenience method for
 * gaim_status_type_is_exclusive(gaim_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is exclusive, FALSE otherwise.
 */
gboolean gaim_status_is_exclusive(const GaimStatus *status);

/**
 * Returns whether or not a status is available.
 *
 * Available statuses are online and possibly hidden, but not away or idle.
 *
 * This is a convenience method for
 * gaim_status_type_is_available(gaim_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is available, or FALSE otherwise.
 */
gboolean gaim_status_is_available(const GaimStatus *status);

/**
 * Returns the active state of a status.
 *
 * @param status The status.
 *
 * @return The active state of the status.
 */
gboolean gaim_status_is_active(const GaimStatus *status);

/**
 * Returns whether or not a status is considered 'online'
 *
 * @param status The status.
 *
 * @return TRUE if the status is considered online, FALSE otherwise
 */
gboolean gaim_status_is_online(const GaimStatus *status);

/**
 * Returns the value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The value of the attribute.
 */
GaimValue *gaim_status_get_attr_value(const GaimStatus *status,
									  const char *id);

/**
 * Returns the boolean value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The boolean value of the attribute.
 */
gboolean gaim_status_get_attr_boolean(const GaimStatus *status,
									  const char *id);

/**
 * Returns the integer value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The integer value of the attribute.
 */
int gaim_status_get_attr_int(const GaimStatus *status, const char *id);

/**
 * Returns the string value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The string value of the attribute.
 */
const char *gaim_status_get_attr_string(const GaimStatus *status,
										const char *id);

/**
 * Compares two statuses for availability.
 *
 * @param status1 The first status.
 * @param status2 The second status.
 *
 * @return -1 if @a status1 is more available than @a status2.
 *          0 if @a status1 is equal to @a status2.
 *          1 if @a status2 is more available than @a status1.
 */
gint gaim_status_compare(const GaimStatus *status1, const GaimStatus *status2);

/*@}*/

/**************************************************************************/
/** @name GaimPresence API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new presence.
 *
 * @param context The presence context.
 *
 * @return A new presence.
 */
GaimPresence *gaim_presence_new(GaimPresenceContext context);

/**
 * Creates a presence for an account.
 *
 * @param account The account.
 *
 * @return The new presence.
 */
GaimPresence *gaim_presence_new_for_account(GaimAccount *account);

/**
 * Creates a presence for a conversation.
 *
 * @param conv The conversation.
 *
 * @return The new presence.
 */
GaimPresence *gaim_presence_new_for_conv(GaimConversation *conv);

/**
 * Creates a presence for a buddy.
 *
 * @param buddy The buddy.
 *
 * @return The new presence.
 */
GaimPresence *gaim_presence_new_for_buddy(GaimBuddy *buddy);

/**
 * Destroys a presence.
 *
 * All statuses added to this list will be destroyed along with
 * the presence.
 *
 * If this presence belongs to a buddy, you must call
 * gaim_presence_remove_buddy() first.
 *
 * @param presence The presence to destroy.
 */
void gaim_presence_destroy(GaimPresence *presence);

/**
 * Removes a buddy from a presence.
 *
 * This must be done before destroying a buddy in a presence.
 *
 * @param presence The presence.
 * @param buddy    The buddy.
 */
void gaim_presence_remove_buddy(GaimPresence *presence, GaimBuddy *buddy);

/**
 * Adds a status to a presence.
 *
 * @param presence The presence.
 * @param status   The status to add.
 */
void gaim_presence_add_status(GaimPresence *presence, GaimStatus *status);

/**
 * Adds a list of statuses to the presence.
 *
 * @param presence    The presence.
 * @param source_list The source list of statuses to add.
 */
void gaim_presence_add_list(GaimPresence *presence, const GList *source_list);

/**
 * Sets the active state of a status in a presence.
 *
 * Only independent statuses can be set unactive. Normal statuses can only
 * be set active, so if you wish to disable a status, set another
 * non-independent status to active, or use gaim_presence_switch_status().
 *
 * @param presence  The presence.
 * @param status_id The ID of the status.
 * @param active    The active state.
 */
void gaim_presence_set_status_active(GaimPresence *presence,
									 const char *status_id, gboolean active);

/**
 * Switches the active status in a presence.
 *
 * This is similar to gaim_presence_set_status_active(), except it won't
 * activate independent statuses.
 *
 * @param presence The presence.
 * @param status_id The status ID to switch to.
 */
void gaim_presence_switch_status(GaimPresence *presence,
								 const char *status_id);

/**
 * Sets the idle state and time on a presence.
 *
 * @param presence  The presence.
 * @param idle      The idle state.
 * @param idle_time The idle time, if @a idle is TRUE.
 */
void gaim_presence_set_idle(GaimPresence *presence, gboolean idle,
							time_t idle_time);

/**
 * Sets the login time on a presence.
 *
 * @param presence   The presence.
 * @param login_time The login time.
 */
void gaim_presence_set_login_time(GaimPresence *presence, time_t login_time);

/**
 * Sets the warning level on a presence.
 *
 * @param presence The presence.
 * @param level    The warning level.  An integer between 0 and 100
 *                 (inclusive) representing the percentage warned.
 */
void gaim_presence_set_warning_level(GaimPresence *presence,
									 unsigned int level);

/**
 * Returns the presence's context.
 *
 * @param presence The presence.
 *
 * @return The presence's context.
 */
GaimPresenceContext gaim_presence_get_context(const GaimPresence *presence);

/**
 * Returns a presence's account.
 *
 * @param presence The presence.
 *
 * @return The presence's account.
 */
GaimAccount *gaim_presence_get_account(const GaimPresence *presence);

/**
 * Returns a presence's conversation.
 *
 * @param presence The presence.
 *
 * @return The presence's conversation.
 */
GaimConversation *gaim_presence_get_conversation(const GaimPresence *presence);

/**
 * Returns a presence's chat user.
 *
 * @param presence The presence.
 *
 * @return The chat's user.
 */
const char *gaim_presence_get_chat_user(const GaimPresence *presence);

/**
 * Returns a presence's list of buddies.
 *
 * @param presence The presence.
 *
 * @return The presence's list of buddies.
 */
const GList *gaim_presence_get_buddies(const GaimPresence *presence);

/**
 * Returns all the statuses in a presence.
 *
 * @param presence The presence.
 *
 * @return The statuses.
 */
const GList *gaim_presence_get_statuses(const GaimPresence *presence);

/**
 * Returns the status with the specified ID from a presence.
 *
 * @param presence  The presence.
 * @param status_id The ID of the status.
 *
 * @return The status if found, or NULL.
 */
GaimStatus *gaim_presence_get_status(const GaimPresence *presence,
									 const char *status_id);

/**
 * Returns the active exclusive status from a presence.
 *
 * @param presence The presence.
 *
 * @return The active exclusive status.
 */
GaimStatus *gaim_presence_get_active_status(const GaimPresence *presence);

/**
 * Returns whether or not a presence is available.
 *
 * Available presences are online and possibly hidden, but not away or idle.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is available, or FALSE otherwise.
 */
gboolean gaim_presence_is_available(const GaimPresence *presence);

/**
 * Returns whether or not a presence is online.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is online, or FALSE otherwise.
 */
gboolean gaim_presence_is_online(const GaimPresence *presence);

/**
 * Returns whether or not a status in a presence is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * @param presence  The presence.
 * @param status_id The ID of the status.
 *
 * @return TRUE if the status is active, or FALSE.
 */
gboolean gaim_presence_is_status_active(const GaimPresence *presence,
										const char *status_id);

/**
 * Returns whether or not a status with the specified primitive type
 * in a presence is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * @param presence  The presence.
 * @param primitive The status primitive.
 *
 * @return TRUE if the status is active, or FALSE.
 */
gboolean gaim_presence_is_status_primitive_active(
	const GaimPresence *presence, GaimStatusPrimitive primitive);

/**
 * Returns whether or not a presence is idle.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is idle, or FALSE otherwise.
 */
gboolean gaim_presence_is_idle(const GaimPresence *presence);

/**
 * Returns the presence's idle time.
 *
 * @param presence The presence.
 *
 * @return The presence's idle time.
 */
time_t gaim_presence_get_idle_time(const GaimPresence *presence);

/**
 * Returns the presence's warning level.
 *
 * @param presence The presence.
 *
 * @return The presence's warning level.
 */
unsigned int gaim_presence_get_warning_level(const GaimPresence *presence);

/**
 * Compares two presences for availability.
 *
 * @param presence1 The first presence.
 * @param presence2 The second presence.
 *
 * @return -1 if @a presence1 is less available than @a presence2.
 *          0 if @a presence1 is equal to @a presence2.
 *          1 if @a presence2 is more available than @a presence1.
 */
gint gaim_presence_compare(const GaimPresence *presence1,
						   const GaimPresence *presence2);

/*@}*/


/**************************************************************************/
/** @name Status subsystem                                                */
/**************************************************************************/
/*@{*/

/**
 * Returns all stored statuses.
 *
 * @return A list of stored statuses.
 */
const GList *gaim_statuses_get_stored(void);

/**
 * Finds a stored status with the specified status type and primary ID.
 *
 * @param status_type The status type of the status.
 * @param id          The primary attribute ID.
 *
 * @return The stored status if found, or NULL.
 */
GaimStatus *gaim_statuses_find_stored(const GaimStatusType *status_type,
									  const char *id);

/**
 * Get the handle for the status subsystem.
 *
 * @return the handle to the status subsystem
 */
void *gaim_statuses_get_handle();

/**
 * Initializes the status subsystem.
 */
void gaim_statuses_init(void);

/**
 * Uninitializes the status subsystem.
 */
void gaim_statuses_uninit(void);

/**
 * Syncs status information to the file.
 */
void gaim_statuses_sync(void);

/**
 * Syncs status information from a file.
 */
void gaim_statuses_load(void);

/*@}*/

#endif /* _GAIM_STATUS_H_ */
