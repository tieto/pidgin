/**
 * @file gtkidle.h GTK+ Idle API
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
#ifndef _GAIM_GTK_IDLE_H_
#define _GAIM_GTK_IDLE_H_

/**************************************************************************/
/** @name GTK+ Idle API                                                  */
/**************************************************************************/
/*@{*/


/**
 * Initializes the GTK+ idle system.
 */
void gaim_gtk_idle_init(void);

/**
 * Uninitializes the GTK+ idle system.
 */
void gaim_gtk_idle_uninit(void);

/**
 * Check the current idle time, reporting to the server or going auto-away as
 * appropriate.
 *
 * @param gc The GaimConnection* to check
 */
gint gaim_gtk_idle_check(gpointer data);

/*@}*/

#endif /* _GAIM_GTK_IDLE_H_ */
