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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * ticker.c -- Syd Logan, Summer 2000
 */

#include <gtk/gtk.h>
#include "gtkticker.h"

GtkWidget *tickerwindow = NULL;
GtkWidget *ticker;

typedef struct {
	char buddy[ 128 ];
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *pix;
} TickerData;

static GList *tickerbuds = (GList *) NULL;

void BuddyTickerDestroyWindow( GtkWidget *window );
void BuddyTickerCreateWindow( void );
void BuddyTickerAddUser( char *name, GdkPixmap *pm, GdkBitmap *bm );
void BuddyTickerRemoveUser( char *name );
void BuddyTickerSetPixmap( char *name, GdkPixmap *pm, GdkBitmap *bm );
GList * BuddyTickerFindUser( char *name );

void
BuddyTickerDestroyWindow( GtkWidget *window )
{
        gtk_ticker_stop_scroll( GTK_TICKER( ticker ) );
	gtk_widget_destroy( window );	
	ticker = tickerwindow = (GtkWidget *) NULL;
}

void
BuddyTickerCreateWindow()
{
	if ( tickerwindow != (GtkWidget *) NULL ) 
		return;
        tickerwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_signal_connect (GTK_OBJECT(tickerwindow), "destroy",
                GTK_SIGNAL_FUNC (BuddyTickerDestroyWindow), "WM destroy");
        gtk_window_set_title (GTK_WINDOW(tickerwindow), "Buddy Ticker");

        ticker = gtk_ticker_new();
        gtk_container_add( GTK_CONTAINER( tickerwindow ), ticker );
        gtk_ticker_set_spacing( GTK_TICKER( ticker ), 20 );
        gtk_widget_set_usize( ticker, 200, -1 );
        gtk_widget_show (ticker);

        gtk_ticker_set_interval( GTK_TICKER( ticker ), 500 );
        gtk_ticker_set_scootch( GTK_TICKER( ticker ), 10 );
        gtk_ticker_start_scroll( GTK_TICKER( ticker ) );
}

void
BuddyTickerAddUser( char *name, GdkPixmap *pm, GdkBitmap *bm )
{
	GtkWidget *hbox, *label, *pmap;
	TickerData *p;

	BuddyTickerCreateWindow();
	p = (TickerData *) malloc( sizeof( TickerData ) );
	p->hbox = (GtkWidget *) NULL;
	p->label = (GtkWidget *) NULL;
	p->pix = (GtkWidget *) NULL;
	strcpy( p->buddy, name );
	tickerbuds = g_list_append( tickerbuds, p );

	p->hbox = gtk_hbox_new( FALSE, 0 );
	gtk_ticker_add( GTK_TICKER( ticker ), p->hbox );
	gtk_widget_show_all( p->hbox );

	BuddyTickerSetPixmap( name, pm, bm );

	p->label = gtk_label_new( name );
	gtk_box_pack_start_defaults( GTK_BOX( p->hbox ), p->label );
	gtk_widget_show( p->label );

        gtk_widget_show( tickerwindow );
}

void 
BuddyTickerRemoveUser( char *name )
{
	GList *p = (GList *) BuddyTickerFindUser( name );
	TickerData *data = (TickerData *) p->data;

	if ( data ) {
		gtk_ticker_remove( GTK_TICKER( ticker ), data->hbox );
		tickerbuds = g_list_remove( tickerbuds, data );
		free( data );	
	}
}

void
BuddyTickerSetPixmap( char *name, GdkPixmap *pm, GdkBitmap *bm )
{
	GList *p = (GList *) BuddyTickerFindUser( name );
	TickerData *data = (TickerData *) p->data;

	if ( data->pix == (GtkWidget *) NULL ) {
		data->pix = gtk_pixmap_new( pm, bm );
		gtk_box_pack_start_defaults( GTK_BOX( data->hbox ), data->pix );
	} else {
		gtk_widget_hide( data->pix );
		gtk_pixmap_set(GTK_PIXMAP(data->pix), pm, bm);
	}
	gtk_widget_show( data->pix );
}

GList *
BuddyTickerFindUser( char *name )
{
	GList *p = tickerbuds;

	while ( p ) {
		TickerData *q = (TickerData *) p->data;
		if ( !strcmp( name, q->buddy ) )
			return( p );
		p = p->next;		
	}
	return (GList *) NULL;
}

int
BuddyTickerLogonTimeout( gpointer data )
{
	char *name = (char *) data;

	GList *p = (GList *) BuddyTickerFindUser( name );
	TickerData *q = (TickerData *) p->data;

	if ( q ) {
		
	}

	return FALSE;
}

int
BuddyTickerLogoutTimeout( gpointer data )
{
	char *name = (char *) data;

	BuddyTickerRemoveUser( name );

	return FALSE;
}
