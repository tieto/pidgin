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
/**
 * SECTION:gntstatus
 * @section_id: finch-gntstatus
 * @short_description: <filename>gntstatus.h</filename>
 * @title: Status API
 */

#ifndef _GNT_STATUS_H
#define _GNT_STATUS_H

#include <status.h>
#include <savedstatuses.h>

/**********************************************************************
 * GNT BuddyList API
 **********************************************************************/

/**
 * finch_savedstatus_show_all:
 *
 * Show a dialog with all the saved statuses.
 */
void finch_savedstatus_show_all(void);

/**
 * finch_savedstatus_edit:
 * @saved: The saved status to edit. Set it to %NULL to create a new status.
 *
 * Show a dialog to edit a status.
 */
void finch_savedstatus_edit(PurpleSavedStatus *saved);

#endif
