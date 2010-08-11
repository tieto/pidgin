/**
 * @file gtkpounce.h GTK+ Buddy Pounce API
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#ifndef _PIDGINPOUNCE_H_
#define _PIDGINPOUNCE_H_

#include "pounce.h"

/**
 * Displays a New Buddy Pounce or Edit Buddy Pounce dialog.
 *
 * @param account    The optional account to use.
 * @param name       The optional name to pounce on.
 * @param cur_pounce The current buddy pounce, if editing an existing one.
 */
void pidgin_pounce_editor_show(PurpleAccount *account, const char *name,
								PurplePounce *cur_pounce);

/**
 * Shows the pounces manager window.
 */
void pidgin_pounces_manager_show(void);

/**
 * Hides the pounces manager window.
 */
void pidgin_pounces_manager_hide(void);

/**
 * Returns the gtkpounces handle
 *
 * @return The handle to the GTK+ pounces system
 */
void *pidgin_pounces_get_handle(void);

/**
 * Initializes the GTK+ pounces subsystem.
 */
void pidgin_pounces_init(void);

#endif /* _PIDGINPOUNCE_H_ */
