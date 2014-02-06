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
 * SECTION:idle
 * @section_id: libpurple-idle
 * @short_description: <filename>idle.h</filename>
 * @title: Idle API
 */

#ifndef _PURPLE_IDLE_H_
#define _PURPLE_IDLE_H_

#include <time.h>

typedef struct _PurpleIdleUiOps PurpleIdleUiOps;

/**
 * PurpleIdleUiOps:
 *
 * Idle UI operations.
 */
struct _PurpleIdleUiOps
{
	time_t (*get_time_idle)(void);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Idle API                                                        */
/**************************************************************************/
/*@{*/

/**
 * purple_idle_touch:
 *
 * Touch our idle tracker.  This signifies that the user is
 * 'active'.  The conversation code calls this when the
 * user sends an IM, for example.
 */
void purple_idle_touch(void);

/**
 * purple_idle_set:
 *
 * Fake our idle time by setting the time at which our
 * accounts purportedly became idle.  This is used by
 * the I'dle Mak'er plugin.
 */
void purple_idle_set(time_t time);

/*@}*/

/**************************************************************************/
/** @name Idle Subsystem                                                  */
/**************************************************************************/
/*@{*/

/**
 * purple_idle_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used for idle reporting.
 */
void purple_idle_set_ui_ops(PurpleIdleUiOps *ops);

/**
 * purple_idle_get_ui_ops:
 *
 * Returns the UI operations structure used for idle reporting.
 *
 * Returns: The UI operations structure in use.
 */
PurpleIdleUiOps *purple_idle_get_ui_ops(void);

/**
 * purple_idle_init:
 *
 * Initializes the idle system.
 */
void purple_idle_init(void);

/**
 * purple_idle_uninit:
 *
 * Uninitializes the idle system.
 */
void purple_idle_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_IDLE_H_ */
