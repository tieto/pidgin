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
#include <string.h>
#include <stdlib.h>
#include "gaim.h"

GtkWidget *tickerwindow = NULL;
GtkWidget *ticker;

typedef struct {
	char buddy[ 128 ];
	GtkWidget *hbox;
	GtkWidget *ebox;
	GtkWidget *label;
	GtkWidget *pix;
} TickerData;

static GList *tickerbuds = (GList *) NULL;
static gboolean userclose = FALSE;
static GtkWidget *msgw;

void BuddyTickerDestroyWindow( GtkWidget *window );
void BuddyTickerCreateWindow( void );
void BuddyTickerAddUser( char *name, GdkPixmap *pm, GdkBitmap *bm );
void BuddyTickerRemoveUser( char *name );
void BuddyTickerSetPixmap( char *name, GdkPixmap *pm, GdkBitmap *bm );
void BuddyTickerClearList( void );
void BuddyTickerSignOff( void );
GList * BuddyTickerFindUser( char *name );
int BuddyTickerMessageRemove( gpointer data );
extern void pressed_ticker(char *);

// this pref is startup only, so make a shadow here of settings at startup
// code uses this variable, not display_prefs

extern int display_options;
int ticker_prefs;

void
SetTickerPrefs( void ) 
{
	ticker_prefs = display_options;
}

void
BuddyTickerDestroyWindow( GtkWidget *window )
{
	BuddyTickerClearList();
        gtk_ticker_stop_scroll( GTK_TICKER( ticker ) );
	gtk_widget_destroy( window );	
	ticker = tickerwindow = (GtkWidget *) NULL;
	userclose = TRUE;
}

static char *msg = "Welcome to GAIM " VERSION ", brought to you by Rob Flynn (maintainer), Eric Warmenhoven, Mark Spencer, Jeramey Crawford, Jim Duchek, and Syd Logan"; 

void
BuddyTickerCreateWindow()
{
	if ( tickerwindow != (GtkWidget *) NULL ) 
		return;
        tickerwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_signal_connect (GTK_OBJECT(tickerwindow), "destroy",
                GTK_SIGNAL_FUNC (BuddyTickerDestroyWindow), "WM destroy");
        gtk_window_set_title (GTK_WINDOW(tickerwindow), "GAIM - Buddy Ticker");
        gtk_window_set_wmclass (GTK_WINDOW(tickerwindow), 
                                "ticker", "Gaim");

        ticker = gtk_ticker_new();
        gtk_ticker_set_spacing( GTK_TICKER( ticker ), 20 );
        gtk_widget_set_usize( ticker, 500, -1 );
        gtk_container_add( GTK_CONTAINER( tickerwindow ), ticker );
        gtk_ticker_set_interval( GTK_TICKER( ticker ), 500 );
        gtk_ticker_set_scootch( GTK_TICKER( ticker ), 10 );
	msgw = gtk_label_new( msg );
	gtk_ticker_add( GTK_TICKER( ticker ), msgw );
        gtk_ticker_start_scroll( GTK_TICKER( ticker ) );

	gtk_timeout_add( 60000, BuddyTickerMessageRemove, (gpointer)  NULL );

        gtk_widget_show_all (ticker);

}

gint 
ButtonPressCallback( GtkWidget *widget, GdkEvent *event, gpointer callback_data ) 
{
	TickerData *p = (TickerData *) callback_data;

	pressed_ticker( p->buddy );

	return TRUE;
}

void
BuddyTickerAddUser( char *name, GdkPixmap *pm, GdkBitmap *bm )
{
	TickerData *p;
	GList *q;

	if ( userclose == TRUE )
		return;

	BuddyTickerCreateWindow();

	q = (GList *) BuddyTickerFindUser( name );
	if ( q != (GList *) NULL )
		return;

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

	p->ebox = gtk_event_box_new();

	// click detection

        gtk_widget_set_events (p->ebox, GDK_BUTTON_PRESS_MASK);
        gtk_signal_connect (GTK_OBJECT (p->ebox), "button_press_event",
                GTK_SIGNAL_FUNC(ButtonPressCallback), (gpointer) p);

	gtk_box_pack_start_defaults( GTK_BOX( p->hbox ), p->ebox );
	gtk_widget_show( p->ebox );

	p->label = gtk_label_new( name );
	gtk_container_add( GTK_CONTAINER(p->ebox), p->label ); 

	gtk_widget_show( p->label );

        gtk_widget_show( tickerwindow );
}

void 
BuddyTickerRemoveUser( char *name )
{
	GList *p = (GList *) BuddyTickerFindUser( name );
	TickerData *data;

	if ( !p )
		return;

	data = (TickerData *) p->data;

	if ( userclose == TRUE )
		return;
	if ( data ) {
		gtk_ticker_remove( GTK_TICKER( ticker ), data->hbox );
		tickerbuds = g_list_remove( tickerbuds, data );
		free( data );	
	}
}

void
BuddyTickerSetPixmap( char *name, GdkPixmap *pm, GdkBitmap *bm )
{
	GList *p; 
	TickerData *data;

	if ( userclose == TRUE )
		return;
	p = (GList *) BuddyTickerFindUser( name );
	if ( p )
		data = (TickerData *) p->data;
	else
		return;
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
BuddyTickerMessageRemove( gpointer data )
{
	if ( userclose == TRUE )
		return FALSE;
	if ( tickerwindow == NULL )
		return FALSE;
	gtk_ticker_remove( GTK_TICKER( ticker ), msgw );
	return FALSE;
}

int
BuddyTickerLogonTimeout( gpointer data )
{
	return FALSE;
}

int
BuddyTickerLogoutTimeout( gpointer data )
{
	char *name = (char *) data;

	if ( userclose == TRUE )
		return FALSE;
	BuddyTickerRemoveUser( name );

	return FALSE;
}

void
BuddyTickerSignoff( void )
{
	GList *p = tickerbuds;
	TickerData *q;

	while ( p ) {
		q = (TickerData *) p->data;
		if ( q )
			BuddyTickerRemoveUser( q->buddy ); 
		p = tickerbuds;
	}
	userclose = FALSE;
	if ( tickerwindow )
		gtk_widget_hide( tickerwindow );
}

void
BuddyTickerClearList( void )
{
	GList *p = tickerbuds;

	while ( p ) 
		p = g_list_remove( p, p->data );
	tickerbuds = (GList *) NULL;
}


