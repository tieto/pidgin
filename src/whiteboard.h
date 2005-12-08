/**
 * @file whiteboard.h The GaimWhiteboard core object
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

#ifndef _GAIM_WHITEBOARD_H_
#define _GAIM_WHITEBOARD_H_

/**
 * Whiteboard PRPL Operations
 */
typedef struct _GaimWhiteboardPrplOps GaimWhiteboardPrplOps;

#include "account.h"

/**
 * A GaimWhiteboard
 */
typedef struct _GaimWhiteboard
{
	int state;                       /**< State of whiteboard session */

	GaimAccount *account;            /**< Account associated with this session */
	char *who;                       /**< Name of the remote user */

	void *ui_data;                   /**< Graphical user-interface data */
	void *proto_data;                /**< Protocol specific data */
	GaimWhiteboardPrplOps *prpl_ops; /**< Protocol-plugin operations */

	GList *draw_list;                /**< List of drawing elements/deltas to send */
} GaimWhiteboard;

/**
 * The GaimWhiteboard UI Operations
 */
typedef struct _GaimWhiteboardUiOps
{
	void (*create)(GaimWhiteboard *wb);                                 /**< create function */
	void (*destroy)(GaimWhiteboard *wb);                               /**< destory function */
	void (*set_dimensions)(GaimWhiteboard *wb, int width, int height); /**< set_dimensions function */
	void (*set_brush) (GaimWhiteboard *wb, int size, int color);		/**< set the size and color of the brush */
	void (*draw_point)(GaimWhiteboard *wb, int x, int y,
					   int color, int size);                           /**< draw_point function */
	void (*draw_line)(GaimWhiteboard *wb, int x1, int y1,
					  int x2, int y2,
					  int color, int size);                            /**< draw_line function */
	void (*clear)(GaimWhiteboard *wb);                                 /**< clear function */
} GaimWhiteboardUiOps;

/**
 * GaimWhiteboard PRPL Operations
 */
struct _GaimWhiteboardPrplOps
{
	void (*start)(GaimWhiteboard *wb);                                   /**< start function */
	void (*end)(GaimWhiteboard *wb);                                     /**< end function */
	void (*get_dimensions)(GaimWhiteboard *wb, int *width, int *height); /**< get_dimensions function */
	void (*set_dimensions)(GaimWhiteboard *wb, int width, int height);   /**< set_dimensions function */
	void (*get_brush) (GaimWhiteboard *wb, int *size, int *color);       /**< get the brush size and color */
	void (*set_brush) (GaimWhiteboard *wb, int size, int color);         /**< set the brush size and color */
	void (*send_draw_list)(GaimWhiteboard *wb, GList *draw_list);        /**< send_draw_list function */
	void (*clear)(GaimWhiteboard *wb);                                   /**< clear function */
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/******************************************************************************/
/** @name GaimWhiteboard API                                                  */
/******************************************************************************/
/*@{*/

/**
 * Sets the UI operations
 *
 * @param ops The UI operations to set
 */
void gaim_whiteboard_set_ui_ops(GaimWhiteboardUiOps *ops);

/**
 * Sets the prpl operations for a whiteboard
 *
 * @param wb  The whiteboard for which to set the prpl operations
 * @param ops The prpl operations to set
 */
void gaim_whiteboard_set_prpl_ops(GaimWhiteboard *wb, GaimWhiteboardPrplOps *ops);

/**
 * Creates a whiteboard
 *
 * @param account The account.
 * @param who     Who you're drawing with.
 * @param state   The state.
 *
 * @return The new whiteboard
 */
GaimWhiteboard *gaim_whiteboard_create(GaimAccount *account, const char *who, int state);

/**
 * Destroys a whiteboard
 *
 * @param wb The whiteboard.
 */
void gaim_whiteboard_destroy(GaimWhiteboard *wb);

/**
 * Starts a whiteboard
 *
 * @param wb The whiteboard.
 */
void gaim_whiteboard_start(GaimWhiteboard *wb);

/**
 * Finds a whiteboard from an account and user.
 *
 * @param account The account.
 * @param who     The user.
 *
 * @return The whiteboard if found, otherwise @c NULL.
 */
GaimWhiteboard *gaim_whiteboard_get_session(GaimAccount *account, const char *who);

/**
 * Destorys a drawing list for a whiteboard
 *
 * @param draw_list The drawing list.
 */
void gaim_whiteboard_draw_list_destroy(GList *draw_list);

/**
 * Gets the dimension of a whiteboard.
 *
 * @param wb		The whiteboard.
 * @param width		The width to be set.
 * @param height	The height to be set.
 *
 * @return TRUE if the values of width and height were set.
 */
gboolean gaim_whiteboard_get_dimensions(GaimWhiteboard *wb, int *width, int *height);

/**
 * Sets the dimensions for a whiteboard.
 *
 * @param wb     The whiteboard.
 * @param width  The width.
 * @param height The height.
 */
void gaim_whiteboard_set_dimensions(GaimWhiteboard *wb, int width, int height);

/**
 * Draws a point on a whiteboard.
 *
 * @param wb    The whiteboard.
 * @param x     The x coordinate.
 * @param y     The y coordinate.
 * @param color The color to use.
 * @param size  The brush size.
 */
void gaim_whiteboard_draw_point(GaimWhiteboard *wb, int x, int y, int color, int size);

/**
 * Send a list of points to draw to the buddy.
 *
 * @param wb	The whiteboard
 * @param list	A GList of points
 */
void gaim_whiteboard_send_draw_list(GaimWhiteboard *wb, GList *list);

/**
 * Draws a line on a whiteboard
 *
 * @param wb    The whiteboard.
 * @param x1    The top-left x coordinate.
 * @param y1    The top-left y coordinate.
 * @param x2    The bottom-right x coordinate.
 * @param y2    The bottom-right y coordinate.
 * @param color The color to use.
 * @param size  The brush size.
 */
void gaim_whiteboard_draw_line(GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size);

/**
 * Clears a whiteboard
 *
 * @param wb The whiteboard.
 */
void gaim_whiteboard_clear(GaimWhiteboard *wb);

/**
 * Sends a request to the buddy to clear the whiteboard.
 *
 * @param wb The whiteboard
 */
void gaim_whiteboard_send_clear(GaimWhiteboard *wb);

/**
 * Sends a request to change the size and color of the brush.
 *
 * @param wb	The whiteboard
 * @param size	The size of the brush
 * @param color	The color of the brush
 */
void gaim_whiteboard_send_brush(GaimWhiteboard *wb, int size, int color);

/**
 * Gets the size and color of the brush.
 *
 * @param wb	The whiteboard
 * @param size	The size of the brush
 * @param color	The color of the brush
 *
 * @return	TRUE if the size and color were set.
 */
gboolean gaim_whiteboard_get_brush(GaimWhiteboard *wb, int *size, int *color);

/**
 * Sets the size and color of the brush.
 *
 * @param wb	The whiteboard
 * @param size	The size of the brush
 * @param color	The color of the brush
 */
void gaim_whiteboard_set_brush(GaimWhiteboard *wb, int size, int color);

/*@}*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAIM_WHITEBOARD_H_ */
