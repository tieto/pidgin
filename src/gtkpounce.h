/**
 * @file gtkpounce.h GTK+ Buddy Pounce API
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_GTK_POUNCE_H_
#define _GAIM_GTK_POUNCE_H_

#include "pounce.h"

/**
 * Displays a New Buddy Pounce or Edit Buddy Pounce dialog.
 *
 * @param buddy      The optional buddy to pounce on.
 * @param cur_pounce The current buddy pounce, if editting an existing one.
 */
void gaim_gtkpounce_dialog_show(struct buddy *buddy, GaimPounce *cur_pounce);

/**
 * Displays all registered buddy pounces in a menu.
 *
 * @param menu The menu to add to.
 */
void gaim_gtkpounce_menu_build(GtkWidget *menu);

/**
 * Initializes the GTK+ pounces subsystem.
 */
void gaim_gtk_pounces_init(void);

#endif /* _GAIM_GTK_POUNCE_H_ */
