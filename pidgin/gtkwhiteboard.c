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

#include "internal.h"
#include "buddylist.h"
#include "debug.h"
#include "pidgin.h"
#include "whiteboard.h"

#include "gtk3compat.h"
#include "gtkwhiteboard.h"
#include "gtkutils.h"

typedef enum {
	PIDGIN_WHITEBOARD_BRUSH_UP,
	PIDGIN_WHITEBOARD_BRUSH_DOWN,
	PIDGIN_WHITEBOARD_BRUSH_MOTION
} PidginWhiteboardBrushState;

typedef struct _PidginWhiteboard PidginWhiteboard;
typedef struct _PidginWhiteboardPrivate PidginWhiteboardPrivate;

struct _PidginWhiteboardPrivate {
	cairo_t *cr;
	cairo_surface_t *surface;
};

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
struct _PidginWhiteboard
{
	PidginWhiteboardPrivate *priv;

	PurpleWhiteboard *wb;

	GtkWidget *window;
	GtkWidget *drawing_area;

	int width;
	int height;
	int brush_color;
	int brush_size;
};

/******************************************************************************
 * Prototypes
 *****************************************************************************/
static void pidgin_whiteboard_create(PurpleWhiteboard *wb);

static void pidgin_whiteboard_destroy(PurpleWhiteboard *wb);
static gboolean whiteboard_close_cb(GtkWidget *widget, GdkEvent *event, PidginWhiteboard *gtkwb);

/*static void pidginwhiteboard_button_start_press(GtkButton *button, gpointer data); */

static gboolean pidgin_whiteboard_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static gboolean
pidgin_whiteboard_draw_event(GtkWidget *widget, cairo_t *cr,
	gpointer _gtkwb);

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

static void color_selected(GtkColorButton *button, PidginWhiteboard *gtkwb);

/******************************************************************************
 * Globals
 *****************************************************************************/

static int LastX;       /* Tracks last position of the mouse when drawing */
static int LastY;
static int MotionCount; /* Tracks how many brush motions made */
static PidginWhiteboardBrushState brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;

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
	GdkColor color;

	PidginWhiteboard *gtkwb = g_new0(PidginWhiteboard, 1);
	gtkwb->priv = g_new0(PidginWhiteboardPrivate, 1);

	gtkwb->wb = wb;
	purple_whiteboard_set_ui_data(wb, gtkwb);

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
	buddy = purple_blist_find_buddy(purple_whiteboard_get_account(wb), purple_whiteboard_get_who(wb));

	window = pidgin_create_window(buddy != NULL ? purple_buddy_get_contact_alias(buddy) : purple_whiteboard_get_who(wb), 0, NULL, FALSE);
	gtkwb->window = window;
	gtk_widget_set_name(window, purple_whiteboard_get_who(wb));

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(whiteboard_close_cb), gtkwb);

	hbox_canvas_and_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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
	g_signal_connect(G_OBJECT(drawing_area), "draw",
		G_CALLBACK(pidgin_whiteboard_draw_event), gtkwb);

	g_signal_connect(G_OBJECT(drawing_area), "configure-event",
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
	vbox_controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
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
	color_button = gtk_color_button_new();
	gtk_box_pack_start(GTK_BOX(vbox_controls), color_button, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(color_button);

	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(color_button), FALSE);
	pidgin_whiteboard_rgb24_to_rgb48(gtkwb->brush_color, &color);
	pidgin_color_chooser_set_rgb(GTK_COLOR_CHOOSER(color_button), &color);

	g_signal_connect(G_OBJECT(color_button), "color-set",
	                 G_CALLBACK(color_selected), gtkwb);

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
	GtkWidget *colour_dialog;

	g_return_if_fail(wb != NULL);
	gtkwb = purple_whiteboard_get_ui_data(wb);
	g_return_if_fail(gtkwb != NULL);

	/* TODO Ask if user wants to save picture before the session is closed */

	/* Clear graphical memory */
	if (gtkwb->priv->cr) {
		cairo_destroy(gtkwb->priv->cr);
		gtkwb->priv->cr = NULL;
	}
	if (gtkwb->priv->surface) {
		cairo_surface_destroy(gtkwb->priv->surface);
		gtkwb->priv->surface = NULL;
	}

	colour_dialog = g_object_get_data(G_OBJECT(gtkwb->window), "colour-dialog");
	if (colour_dialog) {
		gtk_widget_destroy(colour_dialog);
		g_object_set_data(G_OBJECT(gtkwb->window), "colour-dialog", NULL);
	}

	if(gtkwb->window)
	{
		gtk_widget_destroy(gtkwb->window);
		gtkwb->window = NULL;
	}

	g_free(gtkwb->priv);
	g_free(gtkwb);
	purple_whiteboard_set_ui_data(wb, NULL);
}

static gboolean whiteboard_close_cb(GtkWidget *widget, GdkEvent *event, PidginWhiteboard *gtkwb)
{
	PurpleWhiteboard *wb;

	g_return_val_if_fail(gtkwb != NULL, FALSE);
	wb = gtkwb->wb;
	g_return_val_if_fail(wb != NULL, FALSE);

	g_object_unref(wb);

	return FALSE;
}

/*
 * Whiteboard start button on conversation window (move this code to gtkconv?
 * and use new protocol member?)
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
	purple_im_conversation_write_message(PURPLE_CONV_IM(conv), "", XXXX_("Sent Doodle request."),
					   PURPLE_MESSAGE_NICK | PURPLE_MESSAGE_RECV, time(NULL));

	yahoo_doodle_command_send_request(gc, to);
	yahoo_doodle_command_send_ready(gc, to);

	/* Insert this 'session' in the list.  At this point, it's only a requested
	 * session.
	 */
	wb = purple_whiteboard_new(account, to, DOODLE_STATE_REQUESTING);
}
#endif

static gboolean pidgin_whiteboard_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;
	cairo_t *cr;
	GtkAllocation allocation;
	GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

	if (gtkwb->priv->cr)
		cairo_destroy(gtkwb->priv->cr);
	if (gtkwb->priv->surface)
		cairo_surface_destroy(gtkwb->priv->surface);

	gtk_widget_get_allocation(widget, &allocation);

	gtkwb->priv->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
		allocation.width, allocation.height);
	gtkwb->priv->cr = cr = cairo_create(gtkwb->priv->surface);
	gdk_cairo_set_source_rgba(cr, &white);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	return TRUE;
}

static gboolean
pidgin_whiteboard_draw_event(GtkWidget *widget, cairo_t *cr,
	gpointer _gtkwb)
{
	PidginWhiteboard *gtkwb = _gtkwb;

	cairo_set_source_surface(cr, gtkwb->priv->surface, 0, 0);
	cairo_paint(cr);

	return FALSE;
}

static gboolean pidgin_whiteboard_brush_down(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if (brush_state != PIDGIN_WHITEBOARD_BRUSH_UP) {
		/* Potential double-click DOWN to DOWN? */
		brush_state = PIDGIN_WHITEBOARD_BRUSH_DOWN;

		/* return FALSE; */
	}

	brush_state = PIDGIN_WHITEBOARD_BRUSH_DOWN;

	if(event->button == 1 && gtkwb->priv->cr != NULL)
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

	purple_whiteboard_set_draw_list(wb, draw_list);

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

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if(event->is_hint)
		gdk_window_get_device_position(event->window, event->device, &x, &y,
		                               &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if(state & GDK_BUTTON1_MASK && gtkwb->priv->cr != NULL)
	{
		if ((brush_state != PIDGIN_WHITEBOARD_BRUSH_DOWN) &&
			(brush_state != PIDGIN_WHITEBOARD_BRUSH_MOTION))
		{
			purple_debug_error("gtkwhiteboard",
				"***Bad brush state transition %d to MOTION\n",
				brush_state);

			brush_state = PIDGIN_WHITEBOARD_BRUSH_MOTION;

			return FALSE;
		}
		brush_state = PIDGIN_WHITEBOARD_BRUSH_MOTION;

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

	purple_whiteboard_set_draw_list(wb, draw_list);

	return TRUE;
}

static gboolean pidgin_whiteboard_brush_up(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if ((brush_state != PIDGIN_WHITEBOARD_BRUSH_DOWN) &&
		(brush_state != PIDGIN_WHITEBOARD_BRUSH_MOTION))
	{
		purple_debug_error("gtkwhiteboard",
			"***Bad brush state transition %d to UP\n",
			brush_state);

		brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;

		return FALSE;
	}
	brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;

	if(event->button == 1 && gtkwb->priv->cr != NULL)
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

		/* Send draw list to protocol draw_list handler */
		purple_whiteboard_send_draw_list(gtkwb->wb, draw_list);

		pidgin_whiteboard_set_canvas_as_icon(gtkwb);

		/* The brush stroke is finished, clear the list for another one */
		if(draw_list)
			purple_whiteboard_draw_list_destroy(draw_list);

		purple_whiteboard_set_draw_list(wb, NULL);
	}

	return TRUE;
}

static void pidgin_whiteboard_draw_brush_point(PurpleWhiteboard *wb, int x, int y, int color, int size)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);
	GtkWidget *widget = gtkwb->drawing_area;
	cairo_t *gfx_con = gtkwb->priv->cr;
	GdkColor col;
	GdkRGBA rgba;

	/* Interpret and convert color */
	pidgin_whiteboard_rgb24_to_rgb48(color, &col);

	rgba.red = col.red / 0xffff;
	rgba.green = col.green / 0xffff;
	rgba.blue = col.blue / 0xffff;
	rgba.alpha = 1.0;
	gdk_cairo_set_source_rgba(gfx_con, &rgba);

	/* Draw a circle */
	cairo_arc(gfx_con,
	          x, y,
	          size / 2.0,
	          0.0, 2.0 * M_PI);
	cairo_fill(gfx_con);

	gtk_widget_queue_draw_area(widget,
							   x - size / 2, y - size / 2,
							   size, size);
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
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);

	gtkwb->width = width;
	gtkwb->height = height;
}

static void pidgin_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);

	gtkwb->brush_size = size;
	gtkwb->brush_color = color;
}

static void pidgin_whiteboard_clear(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);
	GtkWidget *drawing_area = gtkwb->drawing_area;
	cairo_t *cr = gtkwb->priv->cr;
	GtkAllocation allocation;
	GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

	gtk_widget_get_allocation(drawing_area, &allocation);

	gdk_cairo_set_source_rgba(cr, &white);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	gtk_widget_queue_draw_area(drawing_area, 0, 0,
		allocation.width, allocation.height);
}

static void pidgin_whiteboard_button_clear_press(GtkWidget *widget, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)(data);

	/* Confirm whether the user really wants to clear */
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtkwb->window),
											   GTK_DIALOG_DESTROY_WITH_PARENT,
											   GTK_MESSAGE_QUESTION,
											   GTK_BUTTONS_YES_NO,
											   _("Do you really want to clear?"));
	gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (response == GTK_RESPONSE_YES)
	{
		pidgin_whiteboard_clear(gtkwb->wb);

		pidgin_whiteboard_set_canvas_as_icon(gtkwb);

		/* Do protocol specific clearing procedures */
		purple_whiteboard_send_clear(gtkwb->wb);
	}
}

static void
pidgin_whiteboard_button_save_press(GtkWidget *widget, gpointer _gtkwb)
{
	PidginWhiteboard *gtkwb = _gtkwb;
	GdkPixbuf *pixbuf;
	GtkWidget *dialog;
	int result;

	dialog = gtk_file_chooser_dialog_new(_("Save File"),
		GTK_WINDOW(gtkwb->window), GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
		GTK_RESPONSE_ACCEPT, NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(
		GTK_FILE_CHOOSER(dialog), TRUE);

	gtk_file_chooser_set_current_name(
		GTK_FILE_CHOOSER(dialog), "whiteboard.png");

	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_ACCEPT) {
		gboolean success;
		gchar *filename =
			gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		gtk_widget_destroy(dialog);

		pixbuf = gdk_pixbuf_get_from_surface(gtkwb->priv->surface, 0, 0,
			gtkwb->width, gtkwb->height);

		success = gdk_pixbuf_save(pixbuf, filename, "png", NULL,
			"compression", "9", NULL);
		g_object_unref(pixbuf);
		if (success) {
			purple_debug_info("gtkwhiteboard",
				"whiteboard saved to \"%s\"", filename);
		} else {
			purple_notify_error(NULL, _("Whiteboard"),
				_("Unable to save the file"), NULL, NULL);
			purple_debug_error("gtkwhiteboard", "whiteboard "
				"couldn't be saved to \"%s\"", filename);
		}
		g_free(filename);
	} else if (result == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy(dialog);
}

static void pidgin_whiteboard_set_canvas_as_icon(PidginWhiteboard *gtkwb)
{
	GdkPixbuf *pixbuf;

	/* Makes an icon from the whiteboard's canvas 'image' */
	pixbuf = gdk_pixbuf_get_from_surface(gtkwb->priv->surface,
		0, 0, gtkwb->width, gtkwb->height);
	gtk_window_set_icon(GTK_WINDOW(gtkwb->window), pixbuf);
	g_object_unref(pixbuf);
}

static void pidgin_whiteboard_rgb24_to_rgb48(int color_rgb, GdkColor *color)
{
	color->red   = (color_rgb >> 8) | 0xFF;
	color->green = (color_rgb & 0xFF00) | 0xFF;
	color->blue  = ((color_rgb & 0xFF) << 8) | 0xFF;
}

static void
color_selected(GtkColorButton *button, PidginWhiteboard *gtkwb)
{
	GdkColor color;
	PurpleWhiteboard *wb = gtkwb->wb;
	int old_size, old_color;
	int new_color;

	pidgin_color_chooser_get_rgb(GTK_COLOR_CHOOSER(button), &color);

	new_color = (color.red & 0xFF00) << 8;
	new_color |= (color.green & 0xFF00);
	new_color |= (color.blue & 0xFF00) >> 8;

	purple_whiteboard_get_brush(wb, &old_size, &old_color);
	purple_whiteboard_send_brush(wb, old_size, new_color);
}

