/**
 * @file idle.h Idle API
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
#ifndef _GAIM_IDLE_H_
#define _GAIM_IDLE_H_

/**
 * Idle UI operations.
 */
typedef struct
{
	time_t (*get_time_idle)(void);
} GaimIdleUiOps;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Idle API                                                        */
/**************************************************************************/
/*@{*/

/**
 * Touch our idle tracker.  This signifies that the user is
 * 'active'.  The conversation code calls this when the
 * user sends an IM, for example.
 */
void gaim_idle_touch(void);

/**
 * Fake our idle time by setting the time at which our
 * accounts purportedly became idle.  This is used by
 * the I'dle Mak'er plugin.
 */
void gaim_idle_set(time_t time);

/*@}*/

/**************************************************************************/
/** @name Idle Subsystem                                                  */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for idle reporting.
 *
 * @param ops The UI operations structure.
 */
void gaim_idle_set_ui_ops(GaimIdleUiOps *ops);

/**
 * Returns the UI operations structure used for idle reporting.
 *
 * @return The UI operations structure in use.
 */
GaimIdleUiOps *gaim_idle_get_ui_ops(void);

/**
 * Initializes the idle system.
 */
void gaim_idle_init(void);

/**
 * Uninitializes the idle system.
 */
void gaim_idle_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_IDLE_H_ */
