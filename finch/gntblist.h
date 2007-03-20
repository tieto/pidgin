/**
 * @file gntblist.h GNT BuddyList API
 * @ingroup gntui
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GNT_BLIST_H
#define _GNT_BLIST_H

#include "blist.h"

/**********************************************************************
 * @name GNT BuddyList API
 **********************************************************************/
/*@{*/

/**
 * Get the ui-functions.
 *
 * @return The PurpleBlistUiOps structure populated with the appropriate functions.
 */
PurpleBlistUiOps * finch_blist_get_ui_ops(void);

/**
 * Perform necessary initializations.
 */
void finch_blist_init(void);

/**
 * Perform necessary uninitializations.
 */
void finch_blist_uninit(void);

/**
 * Show the buddy list.
 */
void finch_blist_show(void);

/**
 * Get the position of the buddy list.
 *
 * @param x The x-coordinate is set here if not @ NULL.
 * @param y The y-coordinate is set here if not @c NULL.
 *
 * @return Returns @c TRUE if the values were set, @c FALSE otherwise.
 */
gboolean finch_blist_get_position(int *x, int *y);

/**
 * Set the position of the buddy list.
 *
 * @param x The x-coordinate of the buddy list.
 * @param y The y-coordinate of the buddy list.
 */
void finch_blist_set_position(int x, int y);

/**
 * Get the size of the buddy list.
 *
 * @param width  The width is set here if not @ NULL.
 * @param height The height is set here if not @c NULL.
 *
 * @return Returns @c TRUE if the values were set, @c FALSE otherwise.
 */
gboolean finch_blist_get_size(int *width, int *height);

/**
 * Set the size of the buddy list.
 *
 * @param width  The width of the buddy list.
 * @param height The height of the buddy list.
 */
void finch_blist_set_size(int width, int height);

/*@}*/

#endif
