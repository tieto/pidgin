/*
   libgstroke - a GNOME stroke interface library
   Copyright (c) 1996,1997,1998,1999,2000,2001  Mark F. Willey, ETLA Technical

   See the file COPYING for distribution information.
*/

/* largest number of points allowed to be sampled */
#ifndef _GSTROKE_H_
#define _GSTROKE_H_

#define GSTROKE_MAX_POINTS 10000

/* number of sample points required to have a valid stroke */
#define GSTROKE_MIN_POINTS 50

/* maximum number of numbers in stroke */
#define GSTROKE_MAX_SEQUENCE 32

/* threshold of size of smaller axis needed for it to define its own
   bin size */
#define GSTROKE_SCALE_RATIO 4

/* minimum percentage of points in bin needed to add to sequence */
#define GSTROKE_BIN_COUNT_PERCENT 0.07

void gstroke_set_draw_strokes(gboolean draw);
gboolean gstroke_draw_strokes(void);

void gstroke_set_mouse_button(gint button);
guint gstroke_get_mouse_button(void);

/* enable strokes for the widget */
void gstroke_enable (GtkWidget *widget);

/* disable strokes for the widget */
void gstroke_disable(GtkWidget *widget);

guint gstroke_signal_connect (GtkWidget *widget,
                              const gchar *name,
                              void (*func)(GtkWidget *widget, void *data),
                              gpointer data);

/* frees all the memory allocated for stroke, should be called when
   the widget is destroyed*/
void gstroke_cleanup (GtkWidget *widget);

#endif
