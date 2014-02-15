/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_WHITEBOARD_H_
#define _PURPLE_WHITEBOARD_H_
/**
 * SECTION:whiteboard
 * @section_id: libpurple-whiteboard
 * @short_description: <filename>whiteboard.h</filename>
 * @title: Whiteboard Object
 */

#define PURPLE_TYPE_WHITEBOARD             (purple_whiteboard_get_type())
#define PURPLE_WHITEBOARD(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboard))
#define PURPLE_WHITEBOARD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboardClass))
#define PURPLE_IS_WHITEBOARD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_WHITEBOARD))
#define PURPLE_IS_WHITEBOARD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_WHITEBOARD))
#define PURPLE_WHITEBOARD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboardClass))

#define PURPLE_TYPE_WHITEBOARD_UI_OPS      (purple_whiteboard_ui_ops_get_type())

typedef struct _PurpleWhiteboard PurpleWhiteboard;
typedef struct _PurpleWhiteboardClass PurpleWhiteboardClass;

typedef struct _PurpleWhiteboardUiOps PurpleWhiteboardUiOps;
typedef struct _PurpleWhiteboardOps PurpleWhiteboardOps;

#include "account.h"

/**
 * PurpleWhiteboardUiOps:
 * @create:         create whiteboard
 * @destroy:        destory whiteboard
 * @set_dimensions: set whiteboard dimensions
 * @set_brush:      set the size and color of the brush
 * @draw_point:     draw a point
 * @draw_line:      draw a line
 * @clear:          clear whiteboard
 *
 * The PurpleWhiteboard UI Operations
 */
struct _PurpleWhiteboardUiOps
{
	void (*create)(PurpleWhiteboard *wb);
	void (*destroy)(PurpleWhiteboard *wb);
	void (*set_dimensions)(PurpleWhiteboard *wb, int width, int height);
	void (*set_brush) (PurpleWhiteboard *wb, int size, int color);
	void (*draw_point)(PurpleWhiteboard *wb, int x, int y,
	                   int color, int size);
	void (*draw_line)(PurpleWhiteboard *wb, int x1, int y1,
	                  int x2, int y2,
	                  int color, int size);
	void (*clear)(PurpleWhiteboard *wb);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleWhiteboardOps:
 * @start:          start function
 * @end:            end function
 * @get_dimensions: get whiteboard dimensions
 * @set_dimensions: set whiteboard dimensions
 * @get_brush:      get the brush size and color
 * @set_brush:      set the brush size and color
 * @send_draw_list: send_draw_list function
 * @clear:          clear whiteboard
 *
 * Whiteboard protocol operations
 */
struct _PurpleWhiteboardOps
{
	void (*start)(PurpleWhiteboard *wb);
	void (*end)(PurpleWhiteboard *wb);
	void (*get_dimensions)(const PurpleWhiteboard *wb, int *width, int *height);
	void (*set_dimensions)(PurpleWhiteboard *wb, int width, int height);
	void (*get_brush) (const PurpleWhiteboard *wb, int *size, int *color);
	void (*set_brush) (PurpleWhiteboard *wb, int size, int color);
	void (*send_draw_list)(PurpleWhiteboard *wb, GList *draw_list);
	void (*clear)(PurpleWhiteboard *wb);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleWhiteboard:
 * @ui_data: The UI data associated with this whiteboard. This is a convenience
 *           field provided to the UIs -- it is not used by the libpurple core.
 *
 * A Whiteboard
 */
struct _PurpleWhiteboard
{
	GObject gparent;

	/*< public >*/
	gpointer ui_data;
};

/**
 * PurpleWhiteboardClass:
 *
 * Base class for all #PurpleWhiteboard's
 */
struct _PurpleWhiteboardClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/******************************************************************************/
/* PurpleWhiteboard API                                                       */
/******************************************************************************/

/**
 * purple_whiteboard_get_type:
 *
 * Returns: The #GType for the #PurpleWhiteboard object.
 */
GType purple_whiteboard_get_type(void);

/**
 * purple_whiteboard_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleWhiteboardUiOps boxed structure.
 */
GType purple_whiteboard_ui_ops_get_type(void);

/**
 * purple_whiteboard_set_ui_ops:
 * @ops: The UI operations to set
 *
 * Sets the UI operations
 */
void purple_whiteboard_set_ui_ops(PurpleWhiteboardUiOps *ops);

/**
 * purple_whiteboard_set_protocol_ops:
 * @wb:  The whiteboard for which to set the protocol operations
 * @ops: The protocol operations to set
 *
 * Sets the protocol operations for a whiteboard
 */
void purple_whiteboard_set_protocol_ops(PurpleWhiteboard *wb, PurpleWhiteboardOps *ops);

/**
 * purple_whiteboard_new:
 * @account: The account.
 * @who:     Who you're drawing with.
 * @state:   The state.
 *
 * Creates a new whiteboard
 *
 * Returns: The new whiteboard
 */
PurpleWhiteboard *purple_whiteboard_new(PurpleAccount *account, const char *who, int state);

/**
 * purple_whiteboard_get_account:
 * @wb:		The whiteboard.
 *
 * Returns the whiteboard's account.
 *
 * Returns: The whiteboard's account.
 */
PurpleAccount *purple_whiteboard_get_account(const PurpleWhiteboard *wb);

/**
 * purple_whiteboard_get_who:
 * @wb:		The whiteboard
 *
 * Return who you're drawing with.
 *
 * Returns: Who you're drawing with.
 */
const char *purple_whiteboard_get_who(const PurpleWhiteboard *wb);

/**
 * purple_whiteboard_set_state:
 * @wb:		The whiteboard.
 * @state:		The state
 *
 * Set the state of the whiteboard.
 */
void purple_whiteboard_set_state(PurpleWhiteboard *wb, int state);

/**
 * purple_whiteboard_get_state:
 * @wb:		The whiteboard.
 *
 * Return the state of the whiteboard.
 *
 * Returns: The state of the whiteboard.
 */
int purple_whiteboard_get_state(const PurpleWhiteboard *wb);

/**
 * purple_whiteboard_start:
 * @wb: The whiteboard.
 *
 * Starts a whiteboard
 */
void purple_whiteboard_start(PurpleWhiteboard *wb);

/**
 * purple_whiteboard_get_session:
 * @account: The account.
 * @who:     The user.
 *
 * Finds a whiteboard from an account and user.
 *
 * Returns: The whiteboard if found, otherwise %NULL.
 */
PurpleWhiteboard *purple_whiteboard_get_session(const PurpleAccount *account, const char *who);

/**
 * purple_whiteboard_draw_list_destroy:
 * @draw_list: The drawing list.
 *
 * Destorys a drawing list for a whiteboard
 */
void purple_whiteboard_draw_list_destroy(GList *draw_list);

/**
 * purple_whiteboard_get_dimensions:
 * @wb:		The whiteboard.
 * @width:		The width to be set.
 * @height:	The height to be set.
 *
 * Gets the dimension of a whiteboard.
 *
 * Returns: TRUE if the values of width and height were set.
 */
gboolean purple_whiteboard_get_dimensions(const PurpleWhiteboard *wb, int *width, int *height);

/**
 * purple_whiteboard_set_dimensions:
 * @wb:     The whiteboard.
 * @width:  The width.
 * @height: The height.
 *
 * Sets the dimensions for a whiteboard.
 */
void purple_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height);

/**
 * purple_whiteboard_draw_point:
 * @wb:    The whiteboard.
 * @x:     The x coordinate.
 * @y:     The y coordinate.
 * @color: The color to use.
 * @size:  The brush size.
 *
 * Draws a point on a whiteboard.
 */
void purple_whiteboard_draw_point(PurpleWhiteboard *wb, int x, int y, int color, int size);

/**
 * purple_whiteboard_send_draw_list:
 * @wb:	The whiteboard
 * @list:	A GList of points
 *
 * Send a list of points to draw to the buddy.
 */
void purple_whiteboard_send_draw_list(PurpleWhiteboard *wb, GList *list);

/**
 * purple_whiteboard_draw_line:
 * @wb:    The whiteboard.
 * @x1:    The top-left x coordinate.
 * @y1:    The top-left y coordinate.
 * @x2:    The bottom-right x coordinate.
 * @y2:    The bottom-right y coordinate.
 * @color: The color to use.
 * @size:  The brush size.
 *
 * Draws a line on a whiteboard
 */
void purple_whiteboard_draw_line(PurpleWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size);

/**
 * purple_whiteboard_clear:
 * @wb: The whiteboard.
 *
 * Clears a whiteboard
 */
void purple_whiteboard_clear(PurpleWhiteboard *wb);

/**
 * purple_whiteboard_send_clear:
 * @wb: The whiteboard
 *
 * Sends a request to the buddy to clear the whiteboard.
 */
void purple_whiteboard_send_clear(PurpleWhiteboard *wb);

/**
 * purple_whiteboard_send_brush:
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 *
 * Sends a request to change the size and color of the brush.
 */
void purple_whiteboard_send_brush(PurpleWhiteboard *wb, int size, int color);

/**
 * purple_whiteboard_get_brush:
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 *
 * Gets the size and color of the brush.
 *
 * Returns:	TRUE if the size and color were set.
 */
gboolean purple_whiteboard_get_brush(const PurpleWhiteboard *wb, int *size, int *color);

/**
 * purple_whiteboard_set_brush:
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 *
 * Sets the size and color of the brush.
 */
void purple_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color);

/**
 * purple_whiteboard_get_draw_list:
 * @wb:			The whiteboard.
 *
 * Return the drawing list.
 *
 * Returns: The drawing list
 */
GList *purple_whiteboard_get_draw_list(const PurpleWhiteboard *wb);

/**
 * purple_whiteboard_set_draw_list:
 * @wb:			The whiteboard
 * @draw_list:		The drawing list.
 *
 * Set the drawing list.
 */
void purple_whiteboard_set_draw_list(PurpleWhiteboard *wb, GList* draw_list);

/**
 * purple_whiteboard_set_protocol_data:
 * @wb:			The whiteboard.
 * @proto_data:	The protocol data to set for the whiteboard.
 *
 * Sets the protocol data for a whiteboard.
 */
void purple_whiteboard_set_protocol_data(PurpleWhiteboard *wb, gpointer proto_data);

/**
 * purple_whiteboard_get_protocol_data:
 * @wb:			The whiteboard.
 *
 * Gets the protocol data for a whiteboard.
 *
 * Returns: The protocol data for the whiteboard.
 */
gpointer purple_whiteboard_get_protocol_data(const PurpleWhiteboard *wb);

/**
 * purple_whiteboard_set_ui_data:
 * @wb:			The whiteboard.
 * @ui_data:		A pointer to associate with this whiteboard.
 *
 * Set the UI data associated with this whiteboard.
 */
void purple_whiteboard_set_ui_data(PurpleWhiteboard *wb, gpointer ui_data);

/**
 * purple_whiteboard_get_ui_data:
 * @wb:			The whiteboard..
 *
 * Get the UI data associated with this whiteboard.
 *
 * Returns: The UI data associated with this whiteboard.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_whiteboard_get_ui_data(const PurpleWhiteboard *wb);

G_END_DECLS

#endif /* _PURPLE_WHITEBOARD_H_ */
