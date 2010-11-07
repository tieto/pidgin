/*
 * pidgin
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
 *
 */

#include <stdlib.h>

#include "internal.h"
#include "blist.h"
#include "debug.h"

#include "gtkwhiteboard.h"
#include "gtkutils.h"

/******************************************************************************
 * Prototypes
 *****************************************************************************/
static void pidgin_whiteboard_create(PurpleWhiteboard *wb);

static void pidgin_whiteboard_destroy(PurpleWhiteboard *wb);
static gboolean whiteboard_close_cb(GtkWidget *widget, GdkEvent *event, PidginWhiteboard *gtkwb);

/*static void pidginwhiteboard_button_start_press(GtkButton *button, gpointer data); */

static gboolean pidgin_whiteboard_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static gboolean pidgin_whiteboard_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data);

static gboolean pidgin_whiteboard_brush_down(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean pidgin_whiteboard_brush_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean pidgin_whiteboard_brush_up(GtkWidget *widget, GdkEventButton *event, gpointer data);

static void pidgin_whiteboard_draw_brush_point(PurpleWhiteboard *wb,
												  int x, int y, int color, int size);
static void pidgin_whiteboard_draw_brush_line(PurpleWhiteboard *wb, int x0, int y0,
												int x1, int y1, int color, int size);

static void pidgin_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height);
static void pidgin_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color);
static void pidgin_whiteboard_clear(PurpleWhiteboard *wb);

static void pidgin_whiteboard_button_clear_press(GtkWidget *widget, gpointer data);
static void pidgin_whiteboard_button_save_press(GtkWidget *widget, gpointer data);

static void pidgin_whiteboard_set_canvas_as_icon(PidginWhiteboard *gtkwb);

static void pidgin_whiteboard_rgb24_to_rgb48(int color_rgb, GdkColor *color);

static void color_select_dialog(GtkWidget *widget, PidginWhiteboard *gtkwb);

/******************************************************************************
 * Globals
 *****************************************************************************/
/*
GList *buttonList = NULL;
GdkColor DefaultColor[PALETTE_NUM_COLORS];
*/

static int LastX;       /* Tracks last position of the mouse when drawing */
static int LastY;
static int MotionCount; /* Tracks how many brush motions made */
static int BrushState = BRUSH_STATE_UP;

static PurpleWhiteboardUiOps ui_ops =
{
	pidgin_whiteboard_create,
	pidgin_whiteboard_destroy,
	pidgin_whiteboard_set_dimensions,
	pidgin_whiteboard_set_brush,
	pidgin_whiteboard_draw_brush_point,
	pidgin_whiteboard_draw_brush_line,
	pidgin_whiteboard_clear,
	NULL,
	NULL,
	NULL,
	NULL
};

/******************************************************************************
 * API
 *****************************************************************************/
PurpleWhiteboardUiOps *pidgin_whiteboard_get_ui_ops(void)
{
	return &ui_ops;
}

static void pidgin_whiteboard_create(PurpleWhiteboard *wb)
{
	PurpleBuddy *buddy;
	GtkWidget *window;
	GtkWidget *drawing_area;
	GtkWidget *vbox_controls;
	GtkWidget *hbox_canvas_and_controls;

	/*
		--------------------------
		|[][][][palette[][][][][]|
		|------------------------|
		|       canvas     | con |
		|                  | trol|
		|                  | s   |
		|                  |     |
		|                  |     |
		--------------------------
	*/
	GtkWidget *clear_button;
	GtkWidget *save_button;
	GtkWidget *color_button;

	PidginWhiteboard *gtkwb = g_new0(PidginWhiteboard, 1);

	gtkwb->wb = wb;
	wb->ui_data = gtkwb;

	/* Get dimensions (default?) for the whiteboard canvas */
	if (!purple_whiteboard_get_dimensions(wb, &gtkwb->width, &gtkwb->height))
	{
		/* Give some initial board-size */
		gtkwb->width = 300;
		gtkwb->height = 250;
	}

	if (!purple_whiteboard_get_brush(wb, &gtkwb->brush_size, &gtkwb->brush_color))
	{
		/* Give some initial brush-info */
		gtkwb->brush_size = 2;
		gtkwb->brush_color = 0xff0000;
	}

	/* Try and set window title as the name of the buddy, else just use their
	 * username
	 */
	buddy = purple_find_buddy(wb->account, wb->who);

	window = pidgin_create_window(buddy != NULL ? purple_buddy_get_contact_alias(buddy) : wb->who, 0, NULL, FALSE);
	gtkwb->window = window;
	gtk_widget_set_name(window, wb->who);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(whiteboard_close_cb), gtkwb);

#if 0
	int i;

	GtkWidget *hbox_palette;
	GtkWidget *vbox_palette_above_canvas_and_controls;
	GtkWidget *palette_color_box[PALETTE_NUM_COLORS];

	/* Create vertical box to place palette above the canvas and controls */
	vbox_palette_above_canvas_and_controls = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox_palette_above_canvas_and_controls);
	gtk_widget_show(vbox_palette_above_canvas_and_controls);

	/* Create horizontal box for the palette and all its entries */
	hbox_palette = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_palette_above_canvas_and_controls),
			hbox_palette, FALSE, FALSE, PIDGIN_HIG_BORDER);
	gtk_widget_show(hbox_palette);

	/* Create horizontal box to seperate the canvas from the controls */
	hbox_canvas_and_controls = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_palette_above_canvas_and_controls),
			hbox_canvas_and_controls, FALSE, FALSE, PIDGIN_HIG_BORDER);
	gtk_widget_show(hbox_canvas_and_controls);

	for(i = 0; i < PALETTE_NUM_COLORS; i++)
	{
		palette_color_box[i] = gtk_image_new_from_pixbuf(NULL);
		gtk_widget_set_size_request(palette_color_box[i], gtkwb->width / PALETTE_NUM_COLORS ,32);
		gtk_container_add(GTK_CONTAINER(hbox_palette), palette_color_box[i]);

		gtk_widget_show(palette_color_box[i]);
	}
#endif

	hbox_canvas_and_controls = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_canvas_and_controls);

	gtk_container_add(GTK_CONTAINER(window), hbox_canvas_and_controls);
	gtk_container_set_border_width(GTK_CONTAINER(window), PIDGIN_HIG_BORDER);

	/* Create the drawing area */
	drawing_area = gtk_drawing_area_new();
	gtkwb->drawing_area = drawing_area;
	gtk_widget_set_size_request(GTK_WIDGET(drawing_area), gtkwb->width, gtkwb->height);
	gtk_box_pack_start(GTK_BOX(hbox_canvas_and_controls), drawing_area, TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);

	gtk_widget_show(drawing_area);

	/* Signals used to handle backing pixmap */
	g_signal_connect(G_OBJECT(drawing_area), "expose_event",
					 G_CALLBACK(pidgin_whiteboard_expose_event), gtkwb);

	g_signal_connect(G_OBJECT(drawing_area), "configure_event",
					 G_CALLBACK(pidgin_whiteboard_configure_event), gtkwb);

	/* Event signals */
	g_signal_connect(G_OBJECT(drawing_area), "button_press_event",
					 G_CALLBACK(pidgin_whiteboard_brush_down), gtkwb);

	g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event",
					 G_CALLBACK(pidgin_whiteboard_brush_motion), gtkwb);

	g_signal_connect(G_OBJECT(drawing_area), "button_release_event",
					 G_CALLBACK(pidgin_whiteboard_brush_up), gtkwb);

	gtk_widget_set_events(drawing_area,
						  GDK_EXPOSURE_MASK |
						  GDK_LEAVE_NOTIFY_MASK |
						  GDK_BUTTON_PRESS_MASK |
						  GDK_POINTER_MOTION_MASK |
						  GDK_BUTTON_RELEASE_MASK |
						  GDK_POINTER_MOTION_HINT_MASK);

	/* Create vertical box to contain the controls */
	vbox_controls = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_canvas_and_controls),
					vbox_controls, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(vbox_controls);

	/* Add a clear button */
	clear_button = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	gtk_box_pack_start(GTK_BOX(vbox_controls), clear_button, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(clear_button);
	g_signal_connect(G_OBJECT(clear_button), "clicked",
					 G_CALLBACK(pidgin_whiteboard_button_clear_press), gtkwb);

	/* Add a save button */
	save_button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_box_pack_start(GTK_BOX(vbox_controls), save_button, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(save_button);

	g_signal_connect(G_OBJECT(save_button), "clicked",
					 G_CALLBACK(pidgin_whiteboard_button_save_press), gtkwb);

	/* Add a color selector */
	color_button = gtk_button_new_from_stock(GTK_STOCK_SELECT_COLOR);
	gtk_box_pack_start(GTK_BOX(vbox_controls), color_button, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(color_button);
	g_signal_connect(G_OBJECT(color_button), "clicked",
					 G_CALLBACK(color_select_dialog), gtkwb);

	/* Make all this (window) visible */
	gtk_widget_show(window);

	pidgin_whiteboard_set_canvas_as_icon(gtkwb);

	/* TODO Specific protocol/whiteboard assignment here? Needs a UI Op? */
	/* Set default brush size and color */
	/*
	ds->brush_size = DOODLE_BRUSH_MEDIUM;
	ds->brush_color = 0;
	*/
}

static void pidgin_whiteboard_destroy(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb;

	g_return_if_fail(wb != NULL);
	gtkwb = wb->ui_data;
	g_return_if_fail(gtkwb != NULL);

	/* TODO Ask if user wants to save picture before the session is closed */

	/* Clear graphical memory */
	if(gtkwb->pixmap)
	{
		g_object_unref(gtkwb->pixmap);
		gtkwb->pixmap = NULL;
	}

	if(gtkwb->window)
	{
		gtk_widget_destroy(gtkwb->window);
		gtkwb->window = NULL;
	}
	g_free(gtkwb);
	wb->ui_data = NULL;
}

static gboolean whiteboard_close_cb(GtkWidget *widget, GdkEvent *event, PidginWhiteboard *gtkwb)
{
	PurpleWhiteboard *wb;

	g_return_val_if_fail(gtkwb != NULL, FALSE);
	wb = gtkwb->wb;
	g_return_val_if_fail(wb != NULL, FALSE);

	purple_whiteboard_destroy(wb);

	return FALSE;
}

/*
 * Whiteboard start button on conversation window (move this code to gtkconv?
 * and use new prpl_info member?)
 */
#if 0
static void pidginwhiteboard_button_start_press(GtkButton *button, gpointer data)
{
	PurpleConversation *conv = data;
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleConnection *gc = purple_account_get_connection(account);
	char *to = (char*)(purple_conversation_get_name(conv));

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	PurpleWhiteboard *wb = purple_whiteboard_get(account, to);

	/* Write a local message to this conversation showing that a request for a
	 * Doodle session has been made
	 */
	/* XXXX because otherwise gettext will see this string, even though it's
	 * in an #if 0 block. Remove the XXXX if you want to use this code.
	 * But, it really shouldn't be a Yahoo-specific string. ;) */
	purple_conv_im_write(PURPLE_CONV_IM(conv), "", XXXX_("Sent Doodle request."),
					   PURPLE_MESSAGE_NICK | PURPLE_MESSAGE_RECV, time(NULL));

	yahoo_doodle_command_send_request(gc, to);
	yahoo_doodle_command_send_ready(gc, to);

	/* Insert this 'session' in the list.  At this point, it's only a requested
	 * session.
	 */
	wb = purple_whiteboard_create(account, to, DOODLE_STATE_REQUESTING);
}
#endif

static gboolean pidgin_whiteboard_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	GdkPixmap *pixmap = gtkwb->pixmap;

	if(pixmap)
		g_object_unref(pixmap);

	pixmap = gdk_pixmap_new(widget->window,
							widget->allocation.width,
							widget->allocation.height,
							-1);

	gtkwb->pixmap = pixmap;

	gdk_draw_rectangle(pixmap,
					   widget->style->white_gc,
					   TRUE,
					   0, 0,
					   widget->allocation.width,
					   widget->allocation.height);

	return TRUE;
}

static gboolean pidgin_whiteboard_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)(data);
	GdkPixmap *pixmap = gtkwb->pixmap;

	gdk_draw_drawable(widget->window,
					  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
					  pixmap,
					  event->area.x, event->area.y,
					  event->area.x, event->area.y,
					  event->area.width, event->area.height);

	return FALSE;
}

static gboolean pidgin_whiteboard_brush_down(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;
	GdkPixmap *pixmap = gtkwb->pixmap;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = wb->draw_list;

	if(BrushState != BRUSH_STATE_UP)
	{
		/* Potential double-click DOWN to DOWN? */
		BrushState = BRUSH_STATE_DOWN;

		/* return FALSE; */
	}

	BrushState = BRUSH_STATE_DOWN;

	if(event->button == 1 && pixmap != NULL)
	{
		/* Check if draw_list has contents; if so, clear it */
		if(draw_list)
		{
			purple_whiteboard_draw_list_destroy(draw_list);
			draw_list = NULL;
		}

		/* Set tracking variables */
		LastX = event->x;
		LastY = event->y;

		MotionCount = 0;

		draw_list = g_list_append(draw_list, GINT_TO_POINTER(LastX));
		draw_list = g_list_append(draw_list, GINT_TO_POINTER(LastY));

		pidgin_whiteboard_draw_brush_point(gtkwb->wb,
											 event->x, event->y,
											 gtkwb->brush_color, gtkwb->brush_size);
	}

	wb->draw_list = draw_list;

	return TRUE;
}

static gboolean pidgin_whiteboard_brush_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	int x;
	int y;
	int dx;
	int dy;

	GdkModifierType state;

	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;
	GdkPixmap *pixmap = gtkwb->pixmap;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = wb->draw_list;

	if(event->is_hint)
		gdk_window_get_pointer(event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if(state & GDK_BUTTON1_MASK && pixmap != NULL)
	{
		if((BrushState != BRUSH_STATE_DOWN) && (BrushState != BRUSH_STATE_MOTION))
		{
			purple_debug_error("gtkwhiteboard", "***Bad brush state transition %d to MOTION\n", BrushState);

			BrushState = BRUSH_STATE_MOTION;

			return FALSE;
		}
		BrushState = BRUSH_STATE_MOTION;

		dx = x - LastX;
		dy = y - LastY;

		MotionCount++;

		/* NOTE 100 is a temporary constant for how many deltas/motions in a
		 * stroke (needs UI Ops?)
		 */
		if(MotionCount == 100)
		{
			draw_list = g_list_append(draw_list, GINT_TO_POINTER(dx));
			draw_list = g_list_append(draw_list, GINT_TO_POINTER(dy));

			/* Send draw list to the draw_list handler */
			purple_whiteboard_send_draw_list(gtkwb->wb, draw_list);

			/* The brush stroke is finished, clear the list for another one */
			if(draw_list)
			{
				purple_whiteboard_draw_list_destroy(draw_list);
				draw_list = NULL;
			}

			/* Reset motion tracking */
			MotionCount = 0;

			draw_list = g_list_append(draw_list, GINT_TO_POINTER(LastX));
			draw_list = g_list_append(draw_list, GINT_TO_POINTER(LastY));

			dx = x - LastX;
			dy = y - LastY;
		}

		draw_list = g_list_append(draw_list, GINT_TO_POINTER(dx));
		draw_list = g_list_append(draw_list, GINT_TO_POINTER(dy));

		pidgin_whiteboard_draw_brush_line(gtkwb->wb,
											LastX, LastY,
											x, y,
											gtkwb->brush_color, gtkwb->brush_size);

		/* Set tracking variables */
		LastX = x;
		LastY = y;
	}

	wb->draw_list = draw_list;

	return TRUE;
}

static gboolean pidgin_whiteboard_brush_up(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;
	GdkPixmap *pixmap = gtkwb->pixmap;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = wb->draw_list;

	if((BrushState != BRUSH_STATE_DOWN) && (BrushState != BRUSH_STATE_MOTION))
	{
		purple_debug_error("gtkwhiteboard", "***Bad brush state transition %d to UP\n", BrushState);

		BrushState = BRUSH_STATE_UP;

		return FALSE;
	}
	BrushState = BRUSH_STATE_UP;

	if(event->button == 1 && pixmap != NULL)
	{
		/* If the brush was never moved, express two sets of two deltas That's a
		 * 'point,' but not for Yahoo!
		 */
		/* if((event->x == LastX) && (event->y == LastY)) */
		if(MotionCount == 0)
		{
			int index;

			/* For Yahoo!, a (0 0) indicates the end of drawing */
			/* FIXME: Yahoo Doodle specific! */
			for(index = 0; index < 2; index++)
			{
				draw_list = g_list_append(draw_list, 0);
				draw_list = g_list_append(draw_list, 0);
			}
		}
		/*
		else
			MotionCount = 0;
		*/

		/* Send draw list to prpl draw_list handler */
		purple_whiteboard_send_draw_list(gtkwb->wb, draw_list);

		pidgin_whiteboard_set_canvas_as_icon(gtkwb);

		/* The brush stroke is finished, clear the list for another one */
		if(draw_list)
			purple_whiteboard_draw_list_destroy(draw_list);

		wb->draw_list = NULL;
	}

	return TRUE;
}

static void pidgin_whiteboard_draw_brush_point(PurpleWhiteboard *wb, int x, int y, int color, int size)
{
	PidginWhiteboard *gtkwb = wb->ui_data;
	GtkWidget *widget = gtkwb->drawing_area;
	GdkPixmap *pixmap = gtkwb->pixmap;

	GdkRectangle update_rect;

	GdkGC *gfx_con = gdk_gc_new(pixmap);
	GdkColor col;

	update_rect.x      = x - size / 2;
	update_rect.y      = y - size / 2;
	update_rect.width  = size;
	update_rect.height = size;

	/* Interpret and convert color */
	pidgin_whiteboard_rgb24_to_rgb48(color, &col);

	gdk_gc_set_rgb_fg_color(gfx_con, &col);
	/* gdk_gc_set_rgb_bg_color(gfx_con, &col); */

	/* NOTE 5 is a size constant for now... this is because of how poorly the
	 * gdk_draw_arc draws small circles
	 */
	if(size < 5)
	{
		/* Draw a rectangle/square */
		gdk_draw_rectangle(pixmap,
						   gfx_con,
						   TRUE,
						   update_rect.x, update_rect.y,
						   update_rect.width, update_rect.height);
	}
	else
	{
		/* Draw a circle */
		gdk_draw_arc(pixmap,
					 gfx_con,
					 TRUE,
					 update_rect.x, update_rect.y,
					 update_rect.width, update_rect.height,
					 0, FULL_CIRCLE_DEGREES);
	}

	gtk_widget_queue_draw_area(widget,
							   update_rect.x, update_rect.y,
							   update_rect.width, update_rect.height);

	gdk_gc_unref(gfx_con);
}

/* Uses Bresenham's algorithm (as provided by Wikipedia) */
static void pidgin_whiteboard_draw_brush_line(PurpleWhiteboard *wb, int x0, int y0, int x1, int y1, int color, int size)
{
	int temp;

	int xstep;
	int ystep;

	int dx;
	int dy;

	int error;
	int derror;

	int x;
	int y;

	gboolean steep = abs(y1 - y0) > abs(x1 - x0);

	if(steep)
	{
		temp = x0; x0 = y0; y0 = temp;
		temp = x1; x1 = y1; y1 = temp;
	}

	dx = abs(x1 - x0);
	dy = abs(y1 - y0);

	error = 0;
	derror = dy;

	x = x0;
	y = y0;

	if(x0 < x1)
		xstep = 1;
	else
		xstep = -1;

	if(y0 < y1)
		ystep = 1;
	else
		ystep = -1;

	if(steep)
		pidgin_whiteboard_draw_brush_point(wb, y, x, color, size);
	else
		pidgin_whiteboard_draw_brush_point(wb, x, y, color, size);

	while(x != x1)
	{
		x += xstep;
		error += derror;

		if((error * 2) >= dx)
		{
			y += ystep;
			error -= dx;
		}

		if(steep)
			pidgin_whiteboard_draw_brush_point(wb, y, x, color, size);
		else
			pidgin_whiteboard_draw_brush_point(wb, x, y, color, size);
	}
}

static void pidgin_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height)
{
	PidginWhiteboard *gtkwb = wb->ui_data;

	gtkwb->width = width;
	gtkwb->height = height;
}

static void pidgin_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color)
{
	PidginWhiteboard *gtkwb = wb->ui_data;

	gtkwb->brush_size = size;
	gtkwb->brush_color = color;
}

static void pidgin_whiteboard_clear(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb = wb->ui_data;
	GdkPixmap *pixmap = gtkwb->pixmap;
	GtkWidget *drawing_area = gtkwb->drawing_area;

	gdk_draw_rectangle(pixmap,
					   drawing_area->style->white_gc,
					   TRUE,
					   0, 0,
					   drawing_area->allocation.width,
					   drawing_area->allocation.height);

	gtk_widget_queue_draw_area(drawing_area,
							   0, 0,
							   drawing_area->allocation.width,
							   drawing_area->allocation.height);
}

static void pidgin_whiteboard_button_clear_press(GtkWidget *widget, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)(data);

	pidgin_whiteboard_clear(gtkwb->wb);

	pidgin_whiteboard_set_canvas_as_icon(gtkwb);

	/* Do protocol specific clearing procedures */
	purple_whiteboard_send_clear(gtkwb->wb);
}

static void pidgin_whiteboard_button_save_press(GtkWidget *widget, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)(data);
	GdkPixbuf *pixbuf;

	GtkWidget *dialog;

	int result;

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	dialog = gtk_file_chooser_dialog_new (_("Save File"),
										  GTK_WINDOW(gtkwb->window),
										  GTK_FILE_CHOOSER_ACTION_SAVE,
										  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
										  NULL);

	/* gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), (gboolean)(TRUE)); */

	/* if(user_edited_a_new_document) */
	{
	/* gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_folder_for_saving); */
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "whiteboard.jpg");
	}
	/*
	else
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename_for_existing_document);
	*/
#else
	dialog = gtk_file_selection_new(_("Save File"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), "whiteboard.jpg");
#endif
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if(result == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
#else
		filename = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog)));
#endif
		gtk_widget_destroy(dialog);

		/* Makes an icon from the whiteboard's canvas 'image' */
		pixbuf = gdk_pixbuf_get_from_drawable(NULL,
											  (GdkDrawable*)(gtkwb->pixmap),
											  gdk_drawable_get_colormap(gtkwb->pixmap),
											  0, 0,
											  0, 0,
											  gtkwb->width, gtkwb->height);

		if(gdk_pixbuf_save(pixbuf, filename, "jpeg", NULL, "quality", "100", NULL))
			purple_debug_info("gtkwhiteboard", "File Saved...\n");
		else
			purple_debug_info("gtkwhiteboard", "File not Saved... Error\n");
		g_free(filename);
	}
	else if(result == GTK_RESPONSE_CANCEL)
	{
		gtk_widget_destroy(dialog);

		purple_debug_info("gtkwhiteboard", "File not Saved... Canceled\n");
	}
}

static void pidgin_whiteboard_set_canvas_as_icon(PidginWhiteboard *gtkwb)
{
	GdkPixbuf *pixbuf;

	/* Makes an icon from the whiteboard's canvas 'image' */
	pixbuf = gdk_pixbuf_get_from_drawable(NULL,
										  (GdkDrawable*)(gtkwb->pixmap),
										  gdk_drawable_get_colormap(gtkwb->pixmap),
										  0, 0,
										  0, 0,
										  gtkwb->width, gtkwb->height);

	gtk_window_set_icon((GtkWindow*)(gtkwb->window), pixbuf);
}

static void pidgin_whiteboard_rgb24_to_rgb48(int color_rgb, GdkColor *color)
{
	color->red   = (color_rgb >> 8) | 0xFF;
	color->green = (color_rgb & 0xFF00) | 0xFF;
	color->blue  = ((color_rgb & 0xFF) << 8) | 0xFF;
}

static void
change_color_cb(GtkColorSelection *selection, PidginWhiteboard *gtkwb)
{
	GdkColor color;
	int old_size = 5;
	int old_color = 0;
	int new_color;
	PurpleWhiteboard *wb = gtkwb->wb;

	gtk_color_selection_get_current_color(selection, &color);
	new_color = (color.red & 0xFF00) << 8;
	new_color |= (color.green & 0xFF00);
	new_color |= (color.blue & 0xFF00) >> 8;

	purple_whiteboard_get_brush(wb, &old_size, &old_color);
	purple_whiteboard_send_brush(wb, old_size, new_color);
}

static void color_selection_dialog_destroy(GtkWidget *w, GtkWidget *destroy)
{
	gtk_widget_destroy(destroy);
}

static void color_select_dialog(GtkWidget *widget, PidginWhiteboard *gtkwb)
{
	GdkColor color;
	GtkColorSelectionDialog *dialog;
	
	dialog = (GtkColorSelectionDialog *)gtk_color_selection_dialog_new(_("Select color"));

	g_signal_connect(G_OBJECT(dialog->colorsel), "color-changed",
					G_CALLBACK(change_color_cb), gtkwb);

	gtk_widget_destroy(dialog->cancel_button);
	gtk_widget_destroy(dialog->help_button);

	g_signal_connect(G_OBJECT(dialog->ok_button), "clicked",
					G_CALLBACK(color_selection_dialog_destroy), dialog);

	gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(dialog->colorsel), TRUE);

	pidgin_whiteboard_rgb24_to_rgb48(gtkwb->brush_color, &color);
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(dialog->colorsel), &color);

	gtk_widget_show_all(GTK_WIDGET(dialog));
}

