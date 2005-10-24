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
	int			state;		/**< State of whiteboard session */

	GaimAccount		*account;	/**< Account associated with this session */
	char			*who;		/**< Name of the remote user */

	void			*ui_data;	/**< Graphical user-interface data */
	void			*proto_data;	/**< Protocol specific data */
	GaimWhiteboardPrplOps	*prpl_ops;	/**< Protocol-plugin operations */

	GList			*draw_list;	/**< List of drawing elements/deltas to send */
} GaimWhiteboard;

/**
 * The GaimWhiteboard UI Operations
 */
typedef struct _GaimWhiteboardUiOps
{
	void ( *create )( GaimWhiteboard *wb ); /**< create function */
	void ( *destroy )( GaimWhiteboard *wb ); /**< destory function */
	void ( *set_dimensions)( GaimWhiteboard *wb, int width, int height ); /**< set_dimensions function */
	void ( *draw_point )( GaimWhiteboard *wb, int x, int y, int color, int size ); /**< draw_point function */
	void ( *draw_line )( GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size ); /**< draw_line function */
	void ( *clear )( GaimWhiteboard *wb ); /**< clear function */
} GaimWhiteboardUiOps;

/**
 * GaimWhiteboard PRPL Operations
 */
struct _GaimWhiteboardPrplOps
{
	void ( *start )( GaimWhiteboard *wb ); /**< start function */
	void ( *end )( GaimWhiteboard *wb ); /**< end function */
	void ( *get_dimensions )( GaimWhiteboard *wb, int *width, int *height ); /**< get_dimensions function */
	void ( *set_dimensions )( GaimWhiteboard *wb, int width, int height );  /**< set_dimensions function */
	void ( *send_draw_list )( GaimWhiteboard *wb, GList *draw_list ); /**< send_draw_list function */
	void ( *clear )( GaimWhiteboard *wb ); /**< clear function */
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/******************************************************************************/
/** @name GaimWhiteboard API                                                  */
/******************************************************************************/
/*@{*/

/**
 * Sets the UI Operations
 *
 * @param ops The UI Operations to set
 */
void		gaim_whiteboard_set_ui_ops( GaimWhiteboardUiOps *ops );

/**
 * Creates a whiteboard
 *
 * @param account The account.
 * @param who     Who you're drawing with.
 * @param state   The state.
 *
 * @return The new whiteboard
 */
GaimWhiteboard	*gaim_whiteboard_create( GaimAccount *account, char *who, int state );

/**
 * Destroys a whiteboard
 *
 * @param wb The whiteboard.
 */
void		gaim_whiteboard_destroy( GaimWhiteboard *wb );

/**
 * Starts a whiteboard
 *
 * @param wb The whiteboard.
 */
void		gaim_whiteboard_start( GaimWhiteboard *wb );

/**
 * Finds a whiteboard from an account and user.
 *
 * @param account The account.
 * @param who     The user.
 *
 * @return The whiteboard if found, otherwise @c NULL.
 */
GaimWhiteboard	*gaim_whiteboard_get_session( GaimAccount *account, char *who );

/**
 * Destorys a drawing list for a whiteboard
 *
 * @param draw_list The drawing list.
 *
 * @return The start of the new drawing list (?)
 */
GList		*gaim_whiteboard_draw_list_destroy( GList *draw_list );

/**
 * Sets the dimensions for a whiteboard.
 *
 * @param wb     The whiteboard.
 * @param width  The width.
 * @param height The height.
 */
void		gaim_whiteboard_set_dimensions( GaimWhiteboard *wb, int width, int height );

/**
 * Draws a point on a whiteboard.
 *
 * @param wb    The whiteboard.
 * @param x     The x coordinate.
 * @param y     The y coordinate.
 * @param color The color to use.
 * @param size  The brush size.
 */
void		gaim_whiteboard_draw_point( GaimWhiteboard *wb, int x, int y, int color, int size );

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
void		gaim_whiteboard_draw_line( GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size );

/**
 * Clears a whiteboard
 *
 * @param wb The whiteboard.
 */
void		gaim_whiteboard_clear( GaimWhiteboard *wb );

/*@}*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAIM_WHITEBOARD_H_ */
