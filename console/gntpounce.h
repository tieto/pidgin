/**
 * @file gntpounce.h GNT Buddy Pounce API
 * @ingroup gntui
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
#ifndef _GAIM_GNTPOUNCE_H_
#define _GAIM_GNTPOUNCE_H_

#include "pounce.h"

/**
 * Displays a New Buddy Pounce or Edit Buddy Pounce dialog.
 *
 * @param account    The optional account to use.
 * @param name       The optional name to pounce on.
 * @param cur_pounce The current buddy pounce, if editing an existing one.
 */
void gg_pounce_editor_show(GaimAccount *account, const char *name,
								GaimPounce *cur_pounce);

/**
 * Shows the pounces manager window.
 */
void gg_pounces_manager_show(void);

/**
 * Hides the pounces manager window.
 */
void gg_pounces_manager_hide(void);

/**
 * Returns the gtkpounces handle
 *
 * @return The handle to the GTK+ pounces system
 */
void *gg_pounces_get_handle(void);

/**
 * Initializes the GNT pounces subsystem.
 */
void gg_pounces_init(void);

/**
 * Uninitializes the GNT pounces subsystem.
 */
void gg_pounces_uninit(void);

#endif /* _GAIM_GTKPOUNCE_H_ */
