/*
  GNOME stroke implementation
  Copyright (c) 2000, 2001 Dan Nicolaescu
  See the file COPYING for distribution information.
*/

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gstroke.h"
#include "gstroke-internal.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if !GTK_CHECK_VERSION(2,14,0)
#define gtk_widget_get_window(x) x->window
#endif

static void gstroke_invisible_window_init (GtkWidget *widget);
/*FIXME: Maybe these should be put in a structure, and not static...*/
static Display * gstroke_disp = NULL;
static Window gstroke_window;
static GC gstroke_gc;
static int mouse_button = 2;
static gboolean draw_strokes = FALSE;

#define GSTROKE_TIMEOUT_DURATION 10

#define GSTROKE_SIGNALS "gstroke_signals"

struct gstroke_func_and_data {
	void (*func)(GtkWidget *, void *);
	gpointer data;
};


/*FIXME: maybe it's better to just make 2 static variables, not a
  structure */
struct mouse_position {
	struct s_point last_point;
	gboolean invalid;
};


static struct mouse_position last_mouse_position;
static guint timer_id;

static void gstroke_execute (GtkWidget *widget, const gchar *name);

static void
record_stroke_segment (GtkWidget *widget)
{
  gint x, y;
  struct gstroke_metrics *metrics;

  g_return_if_fail( widget != NULL );

  gtk_widget_get_pointer (widget, &x, &y);

  if (last_mouse_position.invalid)
    last_mouse_position.invalid = FALSE;
  else if (gstroke_draw_strokes())
    {
#if 1
      XDrawLine (gstroke_disp, gstroke_window, gstroke_gc,
                 last_mouse_position.last_point.x,
                 last_mouse_position.last_point.y,
                 x, y);
      /* XFlush (gstroke_disp); */
#else
      /* FIXME: this does not work. It will only work if we create a
         corresponding GDK window for stroke_window and draw on
         that... */
      gdk_draw_line (gtk_widget_get_window(widget),
                     widget->style->fg_gc[GTK_STATE_NORMAL],
                     last_mouse_position.last_point.x,
                     last_mouse_position.last_point.y,
                     x,
                     y);
#endif
    }

  if (last_mouse_position.last_point.x != x
      || last_mouse_position.last_point.y != y)
    {
      last_mouse_position.last_point.x = x;
      last_mouse_position.last_point.y = y;
      metrics = (struct gstroke_metrics *)g_object_get_data(G_OBJECT(widget),
															GSTROKE_METRICS);
      _gstroke_record (x, y, metrics);
    }
}

static gint
gstroke_timeout (gpointer data)
{
	GtkWidget *widget;

	g_return_val_if_fail(data != NULL, FALSE);

	widget = GTK_WIDGET (data);
	record_stroke_segment (widget);

	return TRUE;
}

static void gstroke_cancel(GdkEvent *event) 
{
	last_mouse_position.invalid = TRUE;

	if (timer_id > 0)
	    g_source_remove (timer_id);

	timer_id = 0;

	if( event != NULL )
		gdk_pointer_ungrab (event->button.time);


	if (gstroke_draw_strokes() && gstroke_disp != NULL) {
	    /* get rid of the invisible stroke window */
	    XUnmapWindow (gstroke_disp, gstroke_window);
	    XFlush (gstroke_disp);
	}

}

static gint
process_event (GtkWidget *widget, GdkEvent *event, gpointer data G_GNUC_UNUSED)
{
  static GtkWidget *original_widget = NULL;
  static GdkCursor *cursor = NULL;

  switch (event->type) {
    case GDK_BUTTON_PRESS:
		if (event->button.button != gstroke_get_mouse_button()) {
			/* Similar to the bug below catch when any other button is
			 * clicked after the middle button is clicked (but possibly
			 * not released)
			 */
			gstroke_cancel(event);	
			original_widget = NULL;
			break;
		}

      original_widget = widget; /* remeber the widget where
                                   the stroke started */

      gstroke_invisible_window_init (widget);

      record_stroke_segment (widget);

	  if (cursor == NULL)
		  cursor = gdk_cursor_new(GDK_PENCIL);

      gdk_pointer_grab (gtk_widget_get_window(widget), FALSE,
			GDK_BUTTON_RELEASE_MASK, NULL, cursor,
			event->button.time);
      timer_id = g_timeout_add (GSTROKE_TIMEOUT_DURATION,
				  gstroke_timeout, widget);
      return TRUE;

    case GDK_BUTTON_RELEASE:
      if ((event->button.button != gstroke_get_mouse_button())
	  || (original_widget == NULL)) {

		/* Nice bug when you hold down one button and press another. */
		/* We'll just cancel the gesture instead. */
		gstroke_cancel(event);
		original_widget = NULL;
		break;
	  }

      last_mouse_position.invalid = TRUE;
      original_widget = NULL;
      g_source_remove (timer_id);
      gdk_pointer_ungrab (event->button.time);
      timer_id = 0;

      {
	char result[GSTROKE_MAX_SEQUENCE];
	struct gstroke_metrics *metrics;

	metrics = (struct gstroke_metrics *)g_object_get_data(G_OBJECT (widget),
														  GSTROKE_METRICS);
		if (gstroke_draw_strokes()) {
			/* get rid of the invisible stroke window */
			XUnmapWindow (gstroke_disp, gstroke_window);
			XFlush (gstroke_disp);
		}

	_gstroke_canonical (result, metrics);
	gstroke_execute (widget, result);
      }
      return FALSE;

    default:
      break;
  }

  return FALSE;
}

void
gstroke_set_draw_strokes(gboolean draw)
{
	draw_strokes = draw;
}

gboolean
gstroke_draw_strokes(void)
{
	return draw_strokes;
}

void
gstroke_set_mouse_button(gint button)
{
	mouse_button = button;
}

guint
gstroke_get_mouse_button(void)
{
	return mouse_button;
}

void
gstroke_enable (GtkWidget *widget)
{
  struct gstroke_metrics*
    metrics = (struct gstroke_metrics *)g_object_get_data(G_OBJECT(widget),
														  GSTROKE_METRICS);
  if (metrics == NULL)
    {
      metrics = (struct gstroke_metrics *)g_malloc (sizeof
                                                    (struct gstroke_metrics));
      metrics->pointList = NULL;
      metrics->min_x = 10000;
      metrics->min_y = 10000;
      metrics->max_x = 0;
      metrics->max_y = 0;
      metrics->point_count = 0;

      g_object_set_data(G_OBJECT(widget), GSTROKE_METRICS, metrics);

      g_signal_connect(G_OBJECT(widget), "event",
					   G_CALLBACK(process_event), NULL);
    }
  else
    _gstroke_init (metrics);

  last_mouse_position.invalid = TRUE;
}

void
gstroke_disable(GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func(G_OBJECT(widget), G_CALLBACK(process_event), NULL);
}

guint
gstroke_signal_connect (GtkWidget *widget,
                        const gchar *name,
                        void (*func)(GtkWidget *widget, void *data),
                        gpointer data)
{
  struct gstroke_func_and_data *func_and_data;
  GHashTable *hash_table =
    (GHashTable*)g_object_get_data(G_OBJECT(widget), GSTROKE_SIGNALS);

  if (!hash_table)
    {
      hash_table = g_hash_table_new (g_str_hash, g_str_equal);
      g_object_set_data(G_OBJECT(widget), GSTROKE_SIGNALS,
						(gpointer)hash_table);
    }
  func_and_data = g_new (struct gstroke_func_and_data, 1);
  func_and_data->func = func;
  func_and_data->data = data;
  g_hash_table_insert (hash_table, (gpointer)name, (gpointer)func_and_data);
  return TRUE;
}

static void
gstroke_execute (GtkWidget *widget, const gchar *name)
{

  GHashTable *hash_table =
    (GHashTable*)g_object_get_data(G_OBJECT(widget), GSTROKE_SIGNALS);

#if 0
  purple_debug(PURPLE_DEBUG_MISC, "gestures", "gstroke %s\n", name);
#endif
  
  if (hash_table)
    {
      struct gstroke_func_and_data *fd =
	(struct gstroke_func_and_data*)g_hash_table_lookup (hash_table, name);
      if (fd)
	(*fd->func)(widget, fd->data);
    }
}

void
gstroke_cleanup (GtkWidget *widget)
{
  struct gstroke_metrics *metrics;
  GHashTable *hash_table =
    (GHashTable*)g_object_get_data(G_OBJECT(widget), GSTROKE_SIGNALS);
  if (hash_table)
    /*  FIXME: does this delete the elements too?  */
    g_hash_table_destroy (hash_table);

  g_object_steal_data(G_OBJECT(widget), GSTROKE_SIGNALS);

  metrics = (struct gstroke_metrics*)g_object_get_data(G_OBJECT(widget),
													   GSTROKE_METRICS);
  if (metrics)
    g_free (metrics);
  g_object_steal_data(G_OBJECT(widget), GSTROKE_METRICS);
}


/* This function should be written using GTK+ primitives*/
static void
gstroke_invisible_window_init (GtkWidget *widget)
{
  XSetWindowAttributes w_attr;
  XWindowAttributes orig_w_attr;
  unsigned long mask, col_border, col_background;
  unsigned int border_width;
  XSizeHints hints;
  Display *disp = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(widget));
  Window wind = gdk_x11_window_get_xid(gtk_widget_get_window(widget));
  int screen = DefaultScreen (disp);

	if (!gstroke_draw_strokes())
		return;

  gstroke_disp = disp;

  /* X server should save what's underneath */
  XGetWindowAttributes (gstroke_disp, wind, &orig_w_attr);
  hints.x = orig_w_attr.x;
  hints.y = orig_w_attr.y;
  hints.width = orig_w_attr.width;
  hints.height = orig_w_attr.height;
  mask = CWSaveUnder;
  w_attr.save_under = True;

  /* inhibit all the decorations */
  mask |= CWOverrideRedirect;
  w_attr.override_redirect = True;

  /* Don't set a background, transparent window */
  mask |= CWBackPixmap;
  w_attr.background_pixmap = None;

  /* Default input window look */
  col_background = WhitePixel (gstroke_disp, screen);

  /* no border for the window */
#if 0
  border_width = 5;
#endif
  border_width = 0;

  col_border = BlackPixel (gstroke_disp, screen);

  gstroke_window = XCreateSimpleWindow (gstroke_disp, wind,
                                        0, 0,
                                        hints.width - 2 * border_width,
                                        hints.height - 2 * border_width,
                                        border_width,
                                        col_border, col_background);

  gstroke_gc = XCreateGC (gstroke_disp, gstroke_window, 0, NULL);

  XSetFunction (gstroke_disp, gstroke_gc, GXinvert);

  XChangeWindowAttributes (gstroke_disp, gstroke_window, mask, &w_attr);

  XSetLineAttributes (gstroke_disp, gstroke_gc, 2, LineSolid,
                      CapButt, JoinMiter);
  XMapRaised (gstroke_disp, gstroke_window);

#if 0
  /*FIXME: is this call really needed? If yes, does it need the real
    argc and argv? */
  hints.flags = PPosition | PSize;
  XSetStandardProperties (gstroke_disp, gstroke_window, "gstroke_test", NULL,
                          (Pixmap)NULL, NULL, 0, &hints);


  /* Receive the close window client message */
  {
    /* FIXME: is this really needed? If yes, something should be done
       with wmdelete...*/
    Atom wmdelete = XInternAtom (gstroke_disp, "WM_DELETE_WINDOW",
                                 False);
    XSetWMProtocols (gstroke_disp, gstroke_window, &wmdelete, True);
  }
#endif
}
