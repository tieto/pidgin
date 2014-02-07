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

#ifndef _FINCH_H_
#define _FINCH_H_
/**
 * SECTION:finch
 * @section_id: finch-finch
 * @short_description: <filename>finch.h</filename>
 * @title: UI Definitions and Includes
 */

#include <glib.h>

#define FINCH_UI "gnt-purple"

#define FINCH_PREFS_ROOT "/finch"

#define FINCH_GET_DATA(obj)        (obj)->ui_data
#define FINCH_SET_DATA(obj, data)  (obj)->ui_data = data

/**
 * gnt_start:
 *
 * Start finch with the given command line arguments.
 */
gboolean gnt_start(int *argc, char ***argv);

#endif
