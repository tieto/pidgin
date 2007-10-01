/**
 * @file gntprefs.h GNT Preferences API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#ifndef _GNT_PREFS_H
#define _GNT_PREFS_H

/**********************************************************************
 * @name GNT Preferences API
 **********************************************************************/
/*@{*/

/**
 * Perform necessary initializations.
 */
void finch_prefs_init(void);

/**
 * Show the preferences dialog.
 */
void finch_prefs_show_all(void);

/**
 * You don't need to know about this.
 */
void finch_prefs_update_old(void);
/*@}*/

#endif
