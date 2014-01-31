/**
 * @file whiteboard.h The PurpleWhiteboard core object
 */
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

#define PURPLE_TYPE_WHITEBOARD             (purple_whiteboard_get_type())
#define PURPLE_WHITEBOARD(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboard))
#define PURPLE_WHITEBOARD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboardClass))
#define PURPLE_IS_WHITEBOARD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_WHITEBOARD))
#define PURPLE_IS_WHITEBOARD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_WHITEBOARD))
#define PURPLE_WHITEBOARD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboardClass))

/** @copydoc _PurpleWhiteboard */
typedef struct _PurpleWhiteboard PurpleWhiteboard;
/** @copydoc _PurpleWhiteboardClass */
typedef struct _PurpleWhiteboardClass PurpleWhiteboardClass;

/**
 * Whiteboard protocol operations
 */
typedef struct _PurpleWhiteboardOps PurpleWhiteboardOps;

#include "account.h"

/**
 * The PurpleWhiteboard UI Operations
 */
typedef struct _PurpleWhiteboardUiOps
{
	void (*create)(PurpleWhiteboard *wb);                                /**< create function */
	void (*destroy)(PurpleWhiteboard *wb);                               /**< destory function */
	void (*set_dimensions)(PurpleWhiteboard *wb, int width, int height); /**< set_dimensions function */
	void (*set_brush) (PurpleWhiteboard *wb, int size, int color);       /**< set the size and color of the brush */
	void (*draw_point)(PurpleWhiteboard *wb, int x, int y,
	                   int color, int size);                             /**< draw_point function */
	void (*draw_line)(PurpleWhiteboard *wb, int x1, int y1,
	                  int x2, int y2,
	                  int color, int size);                              /**< draw_line function */
	void (*clear)(PurpleWhiteboard *wb);                                 /**< clear function */

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleWhiteboardUiOps;

/**
 * PurpleWhiteboard Protocol Operations
 */
struct _PurpleWhiteboardOps
{
	void (*start)(PurpleWhiteboard *wb);                                   /**< start function */
	void (*end)(PurpleWhiteboard *wb);                                     /**< end function */
	void (*get_dimensions)(const PurpleWhiteboard *wb, int *width, int *height); /**< get_dimensions function */
	void (*set_dimensions)(PurpleWhiteboard *wb, int width, int height);   /**< set_dimensions function */
	void (*get_brush) (const PurpleWhiteboard *wb, int *size, int *color); /**< get the brush size and color */
	void (*set_brush) (PurpleWhiteboard *wb, int size, int color);         /**< set the brush size and color */
	void (*send_draw_list)(PurpleWhiteboard *wb, GList *draw_list);        /**< send_draw_list function */
	void (*clear)(PurpleWhiteboard *wb);                                   /**< clear function */

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A PurpleWhiteboard
 */
struct _PurpleWhiteboard
{
	GObject gparent;

	/** The UI data associated with this whiteboard. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/** Base class for all #PurpleWhiteboard's */
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
/** @name PurpleWhiteboard API                                                  */
/******************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleWhiteboard object.
 */
GType purple_whiteboard_get_type(void);

/**
 * Sets the UI operations
 *
 * @ops: The UI operations to set
 */
void purple_whiteboard_set_ui_ops(PurpleWhiteboardUiOps *ops);

/**
 * Sets the protocol operations for a whiteboard
 *
 * @wb:  The whiteboard for which to set the protocol operations
 * @ops: The protocol operations to set
 */
void purple_whiteboard_set_protocol_ops(PurpleWhiteboard *wb, PurpleWhiteboardOps *ops);

/**
 * Creates a new whiteboard
 *
 * @account: The account.
 * @who:     Who you're drawing with.
 * @state:   The state.
 *
 * Returns: The new whiteboard
 */
PurpleWhiteboard *purple_whiteboard_new(PurpleAccount *account, const char *who, int state);

/**
 * Returns the whiteboard's account.
 *
 * @wb:		The whiteboard.
 *
 * Returns: The whiteboard's account.
 */
PurpleAccount *purple_whiteboard_get_account(const PurpleWhiteboard *wb);

/**
 * Return who you're drawing with.
 *
 * @wb:		The whiteboard
 *
 * Returns: Who you're drawing with.
 */
const char *purple_whiteboard_get_who(const PurpleWhiteboard *wb);

/**
 * Set the state of the whiteboard.
 *
 * @wb:		The whiteboard.
 * @state:		The state
 */
void purple_whiteboard_set_state(PurpleWhiteboard *wb, int state);

/**
 * Return the state of the whiteboard.
 *
 * @wb:		The whiteboard.
 *
 * Returns: The state of the whiteboard.
 */
int purple_whiteboard_get_state(const PurpleWhiteboard *wb);

/**
 * Starts a whiteboard
 *
 * @wb: The whiteboard.
 */
void purple_whiteboard_start(PurpleWhiteboard *wb);

/**
 * Finds a whiteboard from an account and user.
 *
 * @account: The account.
 * @who:     The user.
 *
 * Returns: The whiteboard if found, otherwise %NULL.
 */
PurpleWhiteboard *purple_whiteboard_get_session(const PurpleAccount *account, const char *who);

/**
 * Destorys a drawing list for a whiteboard
 *
 * @draw_list: The drawing list.
 */
void purple_whiteboard_draw_list_destroy(GList *draw_list);

/**
 * Gets the dimension of a whiteboard.
 *
 * @wb:		The whiteboard.
 * @width:		The width to be set.
 * @height:	The height to be set.
 *
 * Returns: TRUE if the values of width and height were set.
 */
gboolean purple_whiteboard_get_dimensions(const PurpleWhiteboard *wb, int *width, int *height);

/**
 * Sets the dimensions for a whiteboard.
 *
 * @wb:     The whiteboard.
 * @width:  The width.
 * @height: The height.
 */
void purple_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height);

/**
 * Draws a point on a whiteboard.
 *
 * @wb:    The whiteboard.
 * @x:     The x coordinate.
 * @y:     The y coordinate.
 * @color: The color to use.
 * @size:  The brush size.
 */
void purple_whiteboard_draw_point(PurpleWhiteboard *wb, int x, int y, int color, int size);

/**
 * Send a list of points to draw to the buddy.
 *
 * @wb:	The whiteboard
 * @list:	A GList of points
 */
void purple_whiteboard_send_draw_list(PurpleWhiteboard *wb, GList *list);

/**
 * Draws a line on a whiteboard
 *
 * @wb:    The whiteboard.
 * @x1:    The top-left x coordinate.
 * @y1:    The top-left y coordinate.
 * @x2:    The bottom-right x coordinate.
 * @y2:    The bottom-right y coordinate.
 * @color: The color to use.
 * @size:  The brush size.
 */
void purple_whiteboard_draw_line(PurpleWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size);

/**
 * Clears a whiteboard
 *
 * @wb: The whiteboard.
 */
void purple_whiteboard_clear(PurpleWhiteboard *wb);

/**
 * Sends a request to the buddy to clear the whiteboard.
 *
 * @wb: The whiteboard
 */
void purple_whiteboard_send_clear(PurpleWhiteboard *wb);

/**
 * Sends a request to change the size and color of the brush.
 *
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 */
void purple_whiteboard_send_brush(PurpleWhiteboard *wb, int size, int color);

/**
 * Gets the size and color of the brush.
 *
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 *
 * Returns:	TRUE if the size and color were set.
 */
gboolean purple_whiteboard_get_brush(const PurpleWhiteboard *wb, int *size, int *color);

/**
 * Sets the size and color of the brush.
 *
 * @wb:	The whiteboard
 * @size:	The size of the brush
 * @color:	The color of the brush
 */
void purple_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color);

/**
 * Return the drawing list.
 *
 * @wb:			The whiteboard.
 *
 * Returns: The drawing list
 */
GList *purple_whiteboard_get_draw_list(const PurpleWhiteboard *wb);

/**
 * Set the drawing list.
 *
 * @wb:			The whiteboard
 * @draw_list:		The drawing list.
 */
void purple_whiteboard_set_draw_list(PurpleWhiteboard *wb, GList* draw_list);

/**
 * Sets the protocol data for a whiteboard.
 *
 * @wb:			The whiteboard.
 * @proto_data:	The protocol data to set for the whiteboard.
 */
void purple_whiteboard_set_protocol_data(PurpleWhiteboard *wb, gpointer proto_data);

/**
 * Gets the protocol data for a whiteboard.
 *
 * @wb:			The whiteboard.
 *
 * Returns: The protocol data for the whiteboard.
 */
gpointer purple_whiteboard_get_protocol_data(const PurpleWhiteboard *wb);

/**
 * Set the UI data associated with this whiteboard.
 *
 * @wb:			The whiteboard.
 * @ui_data:		A pointer to associate with this whiteboard.
 */
void purple_whiteboard_set_ui_data(PurpleWhiteboard *wb, gpointer ui_data);

/**
 * Get the UI data associated with this whiteboard.
 *
 * @wb:			The whiteboard..
 *
 * Returns: The UI data associated with this whiteboard.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_whiteboard_get_ui_data(const PurpleWhiteboard *wb);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_WHITEBOARD_H_ */
