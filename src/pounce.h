/**
 * @file pounce.h Buddy pounce API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_POUNCE_H_
#define _GAIM_POUNCE_H_

/**
 * Events that trigger buddy pounces.
 */
typedef enum
{
	GAIM_POUNCE_NONE           = 0x00, /**< No events.                    */
	GAIM_POUNCE_SIGNON         = 0x01, /**< The buddy signed on.          */
	GAIM_POUNCE_SIGNOFF        = 0x02, /**< The buddy signed off.         */
	GAIM_POUNCE_AWAY           = 0x04, /**< The buddy went away.          */
	GAIM_POUNCE_AWAY_RETURN    = 0x08, /**< The buddy returned from away. */
	GAIM_POUNCE_IDLE           = 0x10, /**< The buddy became idle.        */
	GAIM_POUNCE_IDLE_RETURN    = 0x20, /**< The buddy is no longer idle.  */
	GAIM_POUNCE_TYPING         = 0x40, /**< The buddy started typing.     */
	GAIM_POUNCE_TYPING_STOPPED = 0x80  /**< The buddy stopped typing.     */

} GaimPounceEvent;

struct gaim_pounce;

/** A pounce callback. */
typedef void (*gaim_pounce_cb)(struct gaim_pounce *, GaimPounceEvent, void *);

/**
 * A buddy pounce structure.
 *
 * Buddy pounces are actions triggered by a buddy-related event. For
 * example, a sound can be played or an IM window opened when a buddy
 * signs on or returns from away. Such responses are handled in the
 * UI. The events themselves are done in the core.
 */
struct gaim_pounce
{
	GaimPounceEvent events;       /**< The event(s) to pounce on. */
	GaimAccount *pouncer; /**< The user who is pouncing.  */

	char *pouncee;                /**< The buddy to pounce on.    */

	gaim_pounce_cb callback;      /**< The callback function to call when the
	                                   event is triggered. */
	void (*free)(void *data);     /**< The data free function. */
	void *data;                   /**< Pounce-specific data. */
};

/**
 * Creates a new buddy pounce.
 *
 * @param pouncer The account that will pounce.
 * @param pouncee The buddy to pounce on.
 * @param event   The event(s) to pounce on.
 * @param cb      The callback function to call when the pounce is triggered.
 * @param data    Pounce-specific data.
 * @param free    The function to free the pounce-specific data.
 *
 * @return The new buddy pounce structure.
 */
struct gaim_pounce *gaim_pounce_new(GaimAccount *pouncer,
									const char *pouncee,
									GaimPounceEvent event,
									gaim_pounce_cb cb, void *data,
									void (*free)(void *));

/**
 * Destroys a buddy pounce.
 *
 * @param pounce The buddy pounce.
 */
void gaim_pounce_destroy(struct gaim_pounce *pounce);

/**
 * Sets the events a pounce should watch for.
 *
 * @param pounce The buddy pounce.
 * @param events The events to watch for.
 */
void gaim_pounce_set_events(struct gaim_pounce *pounce,
							GaimPounceEvent events);

/**
 * Sets the account that will do the pouncing.
 *
 * @param pounce  The buddy pounce.
 * @param pouncer The account that will pounce.
 */
void gaim_pounce_set_pouncer(struct gaim_pounce *pounce,
							 GaimAccount *pouncer);

/**
 * Sets the buddy a pounce should pounce on.
 *
 * @param pounce  The buddy pounce.
 * @param pouncee The buddy to pounce on.
 */
void gaim_pounce_set_pouncee(struct gaim_pounce *pounce, const char *buddy);

/**
 * Sets the callback function to call when the pounce event is triggered.
 *
 * @param pounce The buddy pounce.
 * @param cb     The callback function.
 */
void gaim_pounce_set_callback(struct gaim_pounce *pounce, gaim_pounce_cb cb);

/**
 * Sets the pounce-specific data.
 *
 * @param pounce The buddy pounce.
 * @param data   Data specific to the pounce.
 */
void gaim_pounce_set_data(struct gaim_pounce *pounce, void *data);

/**
 * Returns the events a pounce should watch for.
 *
 * @param pounce The buddy pounce.
 *
 * @return The events the pounce is watching for.
 */
GaimPounceEvent gaim_pounce_get_events(const struct gaim_pounce *pounce);

/**
 * Returns the account that will do the pouncing.
 *
 * @param pounce The buddy pounce.
 *
 * @return The account that will pounce.
 */
GaimAccount *gaim_pounce_get_pouncer(const struct gaim_pounce *pounce);

/**
 * Returns the buddy a pounce should pounce on.
 *
 * @param pounce The buddy pounce.
 *
 * @return The buddy to pounce on.
 */
const char *gaim_pounce_get_pouncee(const struct gaim_pounce *pounce);

/**
 * Returns the pounce-specific data.
 *
 * @param pounce The buddy pounce.
 *
 * @return The data specific to a buddy pounce.
 */
void *gaim_pounce_get_data(const struct gaim_pounce *pounce);

/**
 * Executes a pounce with the specified pouncer, pouncee, and event type.
 *
 * @param pouncer The account that will do the pouncing.
 * @param pouncee The buddy that is being pounced.
 * @param events  The events that triggered the pounce.
 */
void gaim_pounce_execute(const GaimAccount *pouncer,
						 const char *pouncee,
						 GaimPounceEvent events);

/**
 * Finds a pounce with the specified event(s) and buddy.
 *
 * @param pouncer The account to match against.
 * @param buddy   The buddy to match against.
 * @param events  The event(s) to match against.
 *
 * @return The pounce if found, or @c NULL otherwise.
 */
struct gaim_pounce *gaim_find_pounce(const GaimAccount *pouncer,
									 const char *pouncee,
									 GaimPounceEvent events);

/**
 * Returns a list of all registered buddy pounces.
 *
 * @return The list of buddy pounces.
 */
GList *gaim_get_pounces(void);

#endif /* _GAIM_POUNCE_H_ */
