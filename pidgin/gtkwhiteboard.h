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
/**
 * SECTION:gtkwhiteboard
 * @section_id: pidgin-gtkwhiteboard
 * @short_description: <filename>gtkwhiteboard.h</filename>
 * @title: Whiteboard Frontend
 */

#ifndef _PIDGINWHITEBOARD_H_
#define _PIDGINWHITEBOARD_H_

#include "pidgin.h"

#include "whiteboard.h"

#define FULL_CIRCLE_DEGREES 23040

/* TODO: Make into an enum. */
#define BRUSH_STATE_UP      0
#define BRUSH_STATE_DOWN    1
#define BRUSH_STATE_MOTION  2

/* XXX: This seems duplicated with the Yahoo! Doodle protocol code.
 * XXX: How should they work together? */
#define PALETTE_NUM_COLORS  7

typedef struct _PidginWhiteboardPrivate PidginWhiteboardPrivate;

/**
 * PidginWhiteboard:
 * @priv:         Internal data
 * @wb:           Backend data for this whiteboard
 * @window:       Window for the Doodle session
 * @drawing_area: Drawing area
 * @width:        Canvas width
 * @height:       Canvas height
 * @brush_color:  Foreground color
 * @brush_size:   Brush size
 *
 * A PidginWhiteboard
 */
typedef struct _PidginWhiteboard
{
	PidginWhiteboardPrivate *priv;

	PurpleWhiteboard *wb;

	GtkWidget *window;
	GtkWidget *drawing_area;

	int width;
	int height;
	int brush_color;
	int brush_size;
} PidginWhiteboard;

G_BEGIN_DECLS

/*****************************************************************************/
/** @name PidginWhiteboard API                                              */
/*****************************************************************************/
/*@{*/

/**
 * pidgin_whiteboard_get_ui_ops:
 *
 * Gets the GtkWhiteboard UI Operations.
 *
 * Returns: The GtkWhiteboard UI Operations.
 */
PurpleWhiteboardUiOps *pidgin_whiteboard_get_ui_ops( void );

/*@}*/

G_END_DECLS

#endif /* _PIDGINWHITEBOARD_H_ */
