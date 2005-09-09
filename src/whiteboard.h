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

// DEFINES =============================================================================================

typedef struct _GaimWhiteboardPrplOps GaimWhiteboardPrplOps;	// NOTE A nasty compiler dependency fix

#include "account.h"

// INCLUDES ============================================================================================

// DATATYPES ===========================================================================================
typedef struct _GaimWhiteboard
{
	int			state;		// State of whiteboard session
	
	GaimAccount		*account;	// Account associated with this session
	char			*who;		// Name of the remote user
	
	void			*ui_data;	// Graphical user-interface data
	void			*proto_data;	// Protocol specific data
	GaimWhiteboardPrplOps	*prpl_ops;	// Protocol-plugin operations
	
	GList			*draw_list;	// List of drawing elements/deltas to send
} GaimWhiteboard;

typedef struct _GaimWhiteboardUiOps
{
	void ( *create )( GaimWhiteboard *wb );
	void ( *destroy )( GaimWhiteboard *wb );
	void ( *set_dimensions)( GaimWhiteboard *wb, int width, int height );
	void ( *draw_point )( GaimWhiteboard *wb, int x, int y, int color, int size );
	void ( *draw_line )( GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size );
	void ( *clear )( GaimWhiteboard *wb );
} GaimWhiteboardUiOps;

struct _GaimWhiteboardPrplOps
{
	void ( *start )( GaimWhiteboard *wb );
	void ( *end )( GaimWhiteboard *wb );
	void ( *get_dimensions )( GaimWhiteboard *wb, int *width, int *height );
	void ( *set_dimensions )( GaimWhiteboard *wb, int width, int height );
	void ( *send_draw_list )( GaimWhiteboard *wb, GList *draw_list );
	void ( *clear )( GaimWhiteboard *wb );
};

// PROTOTYPES ==========================================================================================

void		gaim_whiteboard_set_ui_ops( GaimWhiteboardUiOps *ops );

GaimWhiteboard	*gaim_whiteboard_create( GaimAccount *account, char *who, int state );
void		gaim_whiteboard_destroy( GaimWhiteboard *wb );
void		gaim_whiteboard_start( GaimWhiteboard *wb );

GaimWhiteboard	*gaim_whiteboard_get_session( GaimAccount *account, char *who );

GList		*gaim_whiteboard_draw_list_destroy( GList *draw_list );

void		gaim_whiteboard_set_dimensions( GaimWhiteboard *wb, int width, int height );
void		gaim_whiteboard_draw_point( GaimWhiteboard *wb, int x, int y, int color, int size );
void		gaim_whiteboard_draw_line( GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size );
void		gaim_whiteboard_clear( GaimWhiteboard *wb );

#endif // _GAIM_WHITEBOARD_H_
