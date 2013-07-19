/**
 * @file presence.h Presence API
 * @ingroup core
 */
/*
 * purple
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
#ifndef _PURPLE_PRESENCE_H_
#define _PURPLE_PRESENCE_H_

#include "status.h"

/** @copydoc _PurplePresence */
typedef struct _PurplePresence  PurplePresence;
/** @copydoc _PurplePresenceClass */
typedef struct _PurplePresenceClass  PurplePresenceClass;

/** @copydoc _PurpleAccountPresence */
typedef struct _PurpleAccountPresence  PurpleAccountPresence;
/** @copydoc _PurpleAccountPresenceClass */
typedef struct _PurpleAccountPresenceClass  PurpleAccountPresenceClass;

/** @copydoc _PurpleBuddyPresence */
typedef struct _PurpleBuddyPresence  PurpleBuddyPresence;
/** @copydoc _PurpleBuddyPresenceClass */
typedef struct _PurpleBuddyPresenceClass  PurpleBuddyPresenceClass;

/**
 * A PurplePresence is like a collection of PurpleStatuses (plus some
 * other random info).  For any buddy, or for any one of your accounts,
 * or for any person with which you're chatting, you may know various
 * amounts of information.  This information is all contained in
 * one PurplePresence.  If one of your buddies is away and idle,
 * then the presence contains the PurpleStatus for their awayness,
 * and it contains their current idle time.  PurplePresences are
 * never saved to disk.  The information they contain is only relevant
 * for the current PurpleSession.
 *
 * @note When a presence is destroyed with the last g_object_unref(), all
 *       statuses added to this list will be destroyed along with the presence.
 */
struct _PurplePresence
{
	/*< private >*/
	GObject gparent;
};

/** Base class for all #PurplePresence's */
struct _PurplePresenceClass {
	/*< private >*/
	GObjectClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A presence for an account
 */
struct _PurpleAccountPresence
{
	/*< private >*/
	PurplePresence parent;
};

/** Base class for all #PurpleAccountPresence's */
struct _PurpleAccountPresenceClass {
	/*< private >*/
	PurplePresenceClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A presence for a buddy
 */
struct _PurpleBuddyPresence
{
	/*< private >*/
	PurplePresence parent;
};

/** Base class for all #PurpleBuddyPresence's */
struct _PurpleBuddyPresenceClass {
	/*< private >*/
	PurplePresenceClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name PurpleAccountPresence API                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a presence for an account.
 *
 * @param account The account to associate with the presence.
 *
 * @return The new presence.
 */
PurpleAccountPresence *purple_account_presence_new(PurpleAccount *account);

/**
 * Returns an account presence's account.
 *
 * @param presence The presence.
 *
 * @return The presence's account.
 */
PurpleAccount *purple_account_presence_get_account(const PurpleAccountPresence *presence);

/*@}*/

/**************************************************************************/
/** @name PurpleBuddyPresence API                                         */
/**************************************************************************/
/*@{*/

/**
 * Creates a presence for a buddy.
 *
 * @param buddy The buddy to associate with the presence.
 *
 * @return The new presence.
 */
PurpleBuddyPresence *purple_buddy_presence_new(PurpleBuddy *buddy);

/**
 * Returns the buddy presence's buddy.
 *
 * @param presence The presence.
 *
 * @return The presence's buddy.
 */
PurpleBuddy *purple_buddy_presence_get_buddy(const PurpleBuddyPresence *presence);

/*@}*/

/**************************************************************************/
/** @name PurplePresence API                                              */
/**************************************************************************/
/*@{*/

/**
 * Sets the active state of a status in a presence.
 *
 * Only independent statuses can be set unactive. Normal statuses can only
 * be set active, so if you wish to disable a status, set another
 * non-independent status to active, or use purple_presence_switch_status().
 *
 * @param presence  The presence.
 * @param status_id The ID of the status.
 * @param active    The active state.
 */
void purple_presence_set_status_active(PurplePresence *presence,
									 const char *status_id, gboolean active);

/**
 * Switches the active status in a presence.
 *
 * This is similar to purple_presence_set_status_active(), except it won't
 * activate independent statuses.
 *
 * @param presence The presence.
 * @param status_id The status ID to switch to.
 */
void purple_presence_switch_status(PurplePresence *presence,
								 const char *status_id);

/**
 * Sets the idle state and time on a presence.
 *
 * @param presence  The presence.
 * @param idle      The idle state.
 * @param idle_time The idle time, if @a idle is TRUE.  This
 *                  is the time at which the user became idle,
 *                  in seconds since the epoch.  If this value is
 *                  unknown then 0 should be used.
 */
void purple_presence_set_idle(PurplePresence *presence, gboolean idle,
							time_t idle_time);

/**
 * Sets the login time on a presence.
 *
 * @param presence   The presence.
 * @param login_time The login time.
 */
void purple_presence_set_login_time(PurplePresence *presence, time_t login_time);

/**
 * Returns all the statuses in a presence.
 *
 * @param presence The presence.
 *
 * @constreturn The statuses.
 */
GList *purple_presence_get_statuses(const PurplePresence *presence);

/**
 * Returns the status with the specified ID from a presence.
 *
 * @param presence  The presence.
 * @param status_id The ID of the status.
 *
 * @return The status if found, or NULL.
 */
PurpleStatus *purple_presence_get_status(const PurplePresence *presence,
									 const char *status_id);

/**
 * Returns the active exclusive status from a presence.
 *
 * @param presence The presence.
 *
 * @return The active exclusive status.
 */
PurpleStatus *purple_presence_get_active_status(const PurplePresence *presence);

/**
 * Returns whether or not a presence is available.
 *
 * Available presences are online and possibly invisible, but not away or idle.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is available, or FALSE otherwise.
 */
gboolean purple_presence_is_available(const PurplePresence *presence);

/**
 * Returns whether or not a presence is online.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is online, or FALSE otherwise.
 */
gboolean purple_presence_is_online(const PurplePresence *presence);

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
gboolean purple_presence_is_status_active(const PurplePresence *presence,
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
gboolean purple_presence_is_status_primitive_active(
	const PurplePresence *presence, PurpleStatusPrimitive primitive);

/**
 * Returns whether or not a presence is idle.
 *
 * @param presence The presence.
 *
 * @return TRUE if the presence is idle, or FALSE otherwise.
 *         If the presence is offline (purple_presence_is_online()
 *         returns FALSE) then FALSE is returned.
 */
gboolean purple_presence_is_idle(const PurplePresence *presence);

/**
 * Returns the presence's idle time.
 *
 * @param presence The presence.
 *
 * @return The presence's idle time.
 */
time_t purple_presence_get_idle_time(const PurplePresence *presence);

/**
 * Returns the presence's login time.
 *
 * @param presence The presence.
 *
 * @return The presence's login time.
 */
time_t purple_presence_get_login_time(const PurplePresence *presence);

/**
 * Compares two presences for availability.
 *
 * @param presence1 The first presence.
 * @param presence2 The second presence.
 *
 * @return -1 if @a presence1 is more available than @a presence2.
 *          0 if @a presence1 is equal to @a presence2.
 *          1 if @a presence1 is less available than @a presence2.
 */
gint purple_presence_compare(const PurplePresence *presence1,
						   const PurplePresence *presence2);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_PRESENCE_H_ */
