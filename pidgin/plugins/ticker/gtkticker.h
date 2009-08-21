/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02111-1301, USA.
 */

/*
 * Copyright 2000 Syd Logan 
 */

#ifndef __GTK_TICKER_H__
#define __GTK_TICKER_H__


#include <gdk/gdk.h>
#include <gtk/gtk.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TICKER            (gtk_ticker_get_type())
#define GTK_TICKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_TICKER, GtkTicker))
#define GTK_TICKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_TICKER, GtkTickerClass))
#define GTK_IS_TICKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_TICKER))
#define GTK_IS_TICKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_TICKER))


typedef struct _GtkTicker        GtkTicker;
typedef struct _GtkTickerClass   GtkTickerClass;
typedef struct _GtkTickerChild   GtkTickerChild;

/* XXX children move from right to left, should be able to go other way */

struct _GtkTicker
{
  GtkContainer container;
  guint interval;	/* how often to scootch */
  gint spacing;	/* inter-child horizontal spacing */
  guint scootch;	/* how many pixels to move each scootch */
  gint timer;		/* timer object */
  gint total;		/* total width of widgets */
  gint width;		/* width of containing window */
  gboolean dirty;
  GList *children;
};

struct _GtkTickerClass
{
  GtkContainerClass parent_class;
};

struct _GtkTickerChild
{
  GtkWidget *widget;
  gint x;		/* current position */
  gint offset;	/* offset in list */
};


GType      gtk_ticker_get_type          (void);
GtkWidget* gtk_ticker_new               (void);
void       gtk_ticker_add               (GtkTicker       *ticker,
                                        GtkWidget      *widget);
void       gtk_ticker_remove            (GtkTicker       *ticker,
                                        GtkWidget      *widget);
void       gtk_ticker_set_interval     (GtkTicker       *ticker,
					gint		interval);
guint      gtk_ticker_get_interval     (GtkTicker       *ticker);
void       gtk_ticker_set_spacing      (GtkTicker       *ticker,
					gint		spacing);
guint      gtk_ticker_get_spacing      (GtkTicker       *ticker);
void       gtk_ticker_set_scootch      (GtkTicker       *ticker,
					gint		scootch);
guint      gtk_ticker_get_scootch      (GtkTicker       *ticker);
void       gtk_ticker_start_scroll     (GtkTicker       *ticker);
void       gtk_ticker_stop_scroll      (GtkTicker       *ticker);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TICKER_H__ */
