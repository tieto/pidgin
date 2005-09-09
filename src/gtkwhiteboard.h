/**
 * @file gtkwhiteboard.h The GtkGaimWhiteboard frontend object
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

#ifndef _GAIM_GTKWHITEBOARD_H_
#define _GAIM_GTKWHITEBOARD_H_

// INCLUDES ============================================================================================
#include "gtkgaim.h"

#include "whiteboard.h"

// DEFINES =============================================================================================

#define FULL_CIRCLE_DEGREES		23040

#define BRUSH_STATE_UP			0
#define BRUSH_STATE_DOWN		1
#define BRUSH_STATE_MOTION		2

#define PALETTE_NUM_COLORS		7

// DATATYPES ===========================================================================================
typedef struct _GaimGtkWhiteboard
{
	GaimWhiteboard	*wb;		// backend data for this whiteboard
	
	GtkWidget	*window;	// Window for the Doodle session
	GtkWidget	*drawing_area;	// Drawing area
	
	GdkPixmap	*pixmap;	// Memory for drawing area
	
	int		width;		// Canvas width
	int		height;		// Canvas height
} GaimGtkWhiteboard;

// PROTOTYPES ==========================================================================================

GaimWhiteboardUiOps	*gaim_gtk_whiteboard_get_ui_ops( void );

void			gaim_gtk_whiteboard_create( GaimWhiteboard *wb );
void			gaim_gtk_whiteboard_destroy( GaimWhiteboard *wb );
void			gaim_gtk_whiteboard_exit( GtkWidget *widget, gpointer data );

//void			gaim_gtkwhiteboard_button_start_press( GtkButton *button, gpointer data );

gboolean		gaim_gtk_whiteboard_configure_event( GtkWidget *widget, GdkEventConfigure *event, gpointer data );
gboolean		gaim_gtk_whiteboard_expose_event( GtkWidget *widget, GdkEventExpose *event, gpointer data );

gboolean		gaim_gtk_whiteboard_brush_down( GtkWidget *widget, GdkEventButton *event, gpointer data );
gboolean		gaim_gtk_whiteboard_brush_motion( GtkWidget *widget, GdkEventMotion *event, gpointer data );
gboolean		gaim_gtk_whiteboard_brush_up( GtkWidget *widget, GdkEventButton *event, gpointer data );

void			gaim_gtk_whiteboard_draw_brush_point( GaimWhiteboard *wb,
							      int x, int y, int color, int size );
void			gaim_gtk_whiteboard_draw_brush_line( GaimWhiteboard *wb,
							     int x0, int y0, int x1, int y1, int color, int size );

void			gaim_gtk_whiteboard_set_dimensions( GaimWhiteboard *wb, int width, int height );
void			gaim_gtk_whiteboard_clear( GaimWhiteboard *wb );

void			gaim_gtk_whiteboard_button_clear_press( GtkWidget *widget, gpointer data );
void			gaim_gtk_whiteboard_button_save_press( GtkWidget *widget, gpointer data );

void			gaim_gtk_whiteboard_set_canvas_as_icon( GaimGtkWhiteboard *gtkwb );

void			gaim_gtk_whiteboard_rgb24_to_rgb48( int color_rgb, GdkColor *color );

#endif // _GAIM_GTKWHITEBOARD_H_
