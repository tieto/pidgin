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
 * pluginized- Sean Egan, Summer 2002
 */


#include <gtk/gtk.h>
#include "gtkticker.h"
#include <string.h>
#include <stdlib.h>
#include "gaim.h"
#include "prpl.h"
#include "pixmaps/no_icon.xpm"
#ifdef _WIN32
#include "win32dep.h"
#endif

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

static GtkWidget *tickerwindow = NULL;
static GtkWidget *ticker;
static GModule *handle;

typedef struct {
	char buddy[ 128 ];
	char alias[ 388 ];
	GtkWidget *hbox;
	GtkWidget *ebox;
	GtkWidget *label;
	GtkWidget *pix;
} TickerData;

GList *tickerbuds = (GList *) NULL;
gboolean userclose = FALSE;
GtkWidget *msgw;

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;
G_MODULE_IMPORT GtkWidget *blist;

void BuddyTickerDestroyWindow( GtkWidget *window );
void BuddyTickerCreateWindow( void );
void BuddyTickerAddUser( char *name, char *alias, GdkPixmap *pm, GdkBitmap *bm );
void BuddyTickerRemoveUser( char *name );
void BuddyTickerSetPixmap( char *name, GdkPixmap *pm, GdkBitmap *bm );
void BuddyTickerClearList( void );
void BuddyTickerSignOff( void );
GList * BuddyTickerFindUser( char *name );
int BuddyTickerMessageRemove( gpointer data );
void BuddyTickerShow();

void
BuddyTickerDestroyWindow( GtkWidget *window )
{
	BuddyTickerClearList();
        gtk_ticker_stop_scroll( GTK_TICKER( ticker ) );
	gtk_widget_destroy( window );	
	ticker = tickerwindow = (GtkWidget *) NULL;
	userclose = TRUE;
}

/* static char *msg = "Welcome to Gaim " VERSION ", brought to you by Rob Flynn (maintainer), Eric Warmenhoven, Mark Spencer, Jeramey Crawford, Jim Duchek, Syd Logan, and Sean Egan"; 
 */

void
BuddyTickerCreateWindow()
{

      	if ( tickerwindow != (GtkWidget *) NULL ) 
		return;
	debug_printf("Making ticker\n");
        tickerwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_signal_connect (GTK_OBJECT(tickerwindow), "destroy",
                GTK_SIGNAL_FUNC (BuddyTickerDestroyWindow), "WM destroy");
        gtk_window_set_title (GTK_WINDOW(tickerwindow), "Gaim - Buddy Ticker");
        gtk_window_set_role (GTK_WINDOW(tickerwindow), "ticker");
	gtk_widget_realize(tickerwindow);

        ticker = gtk_ticker_new();
	if (!ticker)
		return;
        gtk_ticker_set_spacing( GTK_TICKER( ticker ), 20 );
        gtk_widget_set_usize( ticker, 500, -1 );
        gtk_container_add( GTK_CONTAINER( tickerwindow ), ticker );
        gtk_ticker_set_interval( GTK_TICKER( ticker ), 500 );
        gtk_ticker_set_scootch( GTK_TICKER( ticker ), 10 );
	/* Damned egotists
	  msgw = gtk_label_new( msg );
	  gtk_ticker_add( GTK_TICKER( ticker ), msgw );
        */
	gtk_ticker_start_scroll( GTK_TICKER( ticker ) );

	gtk_timeout_add( 60000, BuddyTickerMessageRemove, (gpointer)  NULL );

        gtk_widget_show_all (ticker);

}

gint 
ButtonPressCallback( GtkWidget *widget, GdkEvent *event, gpointer callback_data ) 
{
	TickerData *p = (TickerData *) callback_data;
	new_conversation(p->buddy);
	
	return TRUE;
}

void
BuddyTickerAddUser( char *name, char *alias, GdkPixmap *pm, GdkBitmap *bm )
{
	TickerData *p;
	GList *q;

	if ( userclose == TRUE )
		return;
	
	debug_printf("Adding %s\n", name);

	BuddyTickerCreateWindow();
	if (!ticker)
		return;

	q = (GList *) BuddyTickerFindUser( name );
	if ( q != (GList *) NULL )
		return;

	p = (TickerData *) malloc( sizeof( TickerData ) );
	p->hbox = (GtkWidget *) NULL;
	p->label = (GtkWidget *) NULL;
	p->pix = (GtkWidget *) NULL;
	strcpy( p->buddy, name );
	strcpy( p->alias, alias);
	tickerbuds = g_list_append( tickerbuds, p );

	p->hbox = gtk_hbox_new( FALSE, 0 );
	gtk_ticker_add( GTK_TICKER( ticker ), p->hbox );
	gtk_widget_show_all( p->hbox );

	BuddyTickerSetPixmap( name, pm, bm );

	p->ebox = gtk_event_box_new();

	/* click detection */

        gtk_widget_set_events (p->ebox, GDK_BUTTON_PRESS_MASK);
        gtk_signal_connect (GTK_OBJECT (p->ebox), "button_press_event",
                GTK_SIGNAL_FUNC(ButtonPressCallback), (gpointer) p);

	gtk_box_pack_start_defaults( GTK_BOX( p->hbox ), p->ebox );
	gtk_widget_show( p->ebox );

	if (im_options & OPT_IM_ALIAS_TAB)
		p->label = gtk_label_new( alias );
	else
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

void
BuddyTickerSetAlias( char *name, char *alias) {
	GList *p;
	TickerData *data;

	if ( userclose == TRUE )
		return;
	p = (GList *) BuddyTickerFindUser( name );
	if ( p )
		data = (TickerData *) p->data;
	else
		return;
	if (alias) {
		g_snprintf(data->alias, sizeof(data->alias), alias);
		
		if (im_options & OPT_IM_ALIAS_TAB) 
			gtk_label_set_text(GTK_LABEL(data->label), alias);
	}
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

void
BuddyTickerSetNames()
{
	GList *p = tickerbuds;
	while ( p ) {
		TickerData *q = (TickerData *) p->data;
		if (im_options & OPT_IM_ALIAS_TAB)
			gtk_label_set_text(GTK_LABEL(q->label), q->alias);
		else
			gtk_label_set_text(GTK_LABEL(q->label), q->buddy);
	p = p->next;
	}
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

void BuddyTickerShow()
{
	GdkPixmap *pm;
	GdkBitmap *bm;
	struct gaim_connection *gc;
	struct group *g;
	struct buddy *b;
	GSList *gcons, *grps, *buds;
	char **xpm;
	
	for( gcons = connections; gcons; gcons = gcons->next ) {
		gc = (struct gaim_connection *)gcons->data;
		for( grps = gc->groups; grps; grps = grps->next ) {
			g = (struct group *)grps->data;
			for( buds = g->members; buds; buds = buds->next ) {
				b = (struct buddy *)buds->data;
				if( b->present ) {
					xpm = NULL;
					if (gc->prpl->list_icon)
						xpm = gc->prpl->list_icon(b->uc);
					if (xpm == NULL)
						xpm = (char **)no_icon_xpm;
					pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
					BuddyTickerAddUser( b->name, b->show, pm, bm );
					gdk_pixmap_unref(pm);
					if (bm)
						gdk_bitmap_unref(bm);
				}
			}
		}
	}
}

void signon_cb(struct gaim_connection *gc, char *who) {
	struct buddy *b  = find_buddy(gc, who);
	char **xpm = NULL;
	
	GdkPixmap *pm;
	GdkBitmap *bm;
	
	if (gc->prpl->list_icon)
		xpm = gc->prpl->list_icon(b->uc);
	if (xpm == NULL)
		xpm = (char **)no_icon_xpm;
	pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
		
	BuddyTickerAddUser(who, b->show, pm, bm);
	gdk_pixmap_unref(pm);
	if (bm)
		gdk_bitmap_unref(bm);
}

void signoff_cb(struct gaim_connection *gc) {
	if (connections && !connections->next) {
		gtk_widget_destroy(tickerwindow);
		tickerwindow = NULL;
		ticker = NULL;
	}
}

void buddy_signoff_cb(struct gaim_connection *gc, char *who) {
	BuddyTickerRemoveUser(who);
}

void away_cb(struct gaim_connection *gc, char *who) {
	struct buddy *b  = find_buddy(gc, who);
	char **xpm = NULL;
	
	GdkPixmap *pm;
	GdkBitmap *bm;
	
	if (gc->prpl->list_icon)
		xpm = gc->prpl->list_icon(b->uc);
	if (xpm == NULL)
		xpm = (char **)no_icon_xpm;
	pm = gdk_pixmap_create_from_xpm_d(blist->window, &bm, NULL, xpm);
	BuddyTickerSetPixmap(who, pm, bm);
	gdk_pixmap_unref(pm);
	if (bm)
		gdk_bitmap_unref(bm);
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *name() {
	return "Buddy Ticker";
}

G_MODULE_EXPORT char *description() {
	return "Scrolls online buddies from your buddy list.";
}

G_MODULE_EXPORT char *gaim_plugin_init(GModule *h) {
	handle = h;
	
	gaim_signal_connect(h, event_buddy_signon, signon_cb, NULL);
	gaim_signal_connect(h, event_signoff, signoff_cb, NULL);
	gaim_signal_connect(h, event_buddy_signoff, buddy_signoff_cb, NULL);
	gaim_signal_connect(h, event_buddy_away, away_cb, NULL);

	if (connections)
		BuddyTickerShow();
	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
	BuddyTickerDestroyWindow(tickerwindow);
}
struct gaim_plugin_description desc; 
G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Ticker");
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup("A horizontal scrolling version of the buddy list.");
	desc.authors = g_strdup("Syd Logan");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
