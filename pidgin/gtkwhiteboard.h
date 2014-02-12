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

#ifndef _PIDGINWHITEBOARD_H_
#define _PIDGINWHITEBOARD_H_
/**
 * SECTION:gtkwhiteboard
 * @section_id: pidgin-gtkwhiteboard
 * @short_description: <filename>gtkwhiteboard.h</filename>
 * @title: Whiteboard Frontend
 */

#include "pidgin.h"

G_BEGIN_DECLS

/*****************************************************************************/
/* PidginWhiteboard API                                                      */
/*****************************************************************************/

/**
 * pidgin_whiteboard_get_ui_ops:
 *
 * Gets the GtkWhiteboard UI Operations.
 *
 * Returns: The GtkWhiteboard UI Operations.
 */
PurpleWhiteboardUiOps *pidgin_whiteboard_get_ui_ops(void);

G_END_DECLS

#endif /* _PIDGINWHITEBOARD_H_ */
