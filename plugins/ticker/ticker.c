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
#include "list.h"
#include "gtklist.h"
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
	struct buddy *buddy;
	GtkWidget *ebox;
	GtkWidget *label;
	GtkWidget *icon;
} TickerData;

GList *tickerbuds = NULL;

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;

static gboolean buddy_ticker_destroy_window(GtkWidget *window,
		GdkEventAny *event, gpointer data) {
	if(window)
		gtk_widget_hide(window);

	return TRUE; /* don't actually destroy the window */
}

static void buddy_ticker_create_window() {
	if(tickerwindow) {
		gtk_widget_show(tickerwindow);
		return;
	}

	tickerwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(tickerwindow), "delete_event",
			G_CALLBACK (buddy_ticker_destroy_window), NULL);
	gtk_window_set_title (GTK_WINDOW(tickerwindow), _("Buddy Ticker"));
	gtk_window_set_role (GTK_WINDOW(tickerwindow), "ticker");

	ticker = gtk_ticker_new();
	gtk_ticker_set_spacing(GTK_TICKER(ticker), 20);
	gtk_container_add(GTK_CONTAINER(tickerwindow), ticker);
	gtk_ticker_set_interval(GTK_TICKER(ticker), 500);
	gtk_ticker_set_scootch(GTK_TICKER(ticker), 10);
	gtk_ticker_start_scroll(GTK_TICKER(ticker));
	gtk_widget_set_size_request(ticker, 500, -1);

	gtk_widget_show_all(tickerwindow);
}

static gboolean buddy_click_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	struct buddy *b = user_data;

	gaim_conversation_new(GAIM_CONV_IM, b->account, b->name);
	return TRUE;
}

static TickerData *buddy_ticker_find_buddy(struct buddy *b) {
	GList *tb;
	for(tb = tickerbuds; tb; tb = tb->next) {
		TickerData *td = tb->data;
		if(td->buddy == b)
			return td;
	}
	return NULL;
}

static void buddy_ticker_set_pixmap(struct buddy *b) {
	TickerData *td = buddy_ticker_find_buddy(b);
	GdkPixbuf *pixbuf;

	if(!td)
		return;

	if(!td->icon)
		td->icon = gtk_image_new();

	pixbuf = gaim_gtk_blist_get_status_icon(b, GAIM_STATUS_ICON_SMALL);
	gtk_image_set_from_pixbuf(GTK_IMAGE(td->icon), pixbuf);
	g_object_unref(G_OBJECT(pixbuf));
}

static void buddy_ticker_add_buddy(struct buddy *b) {
	GtkWidget *hbox;
	TickerData *td;

	buddy_ticker_create_window();

	if (!ticker)
		return;

	if(buddy_ticker_find_buddy(b))
		return;

	td = g_new0(TickerData, 1);
	td->buddy = b;
	tickerbuds = g_list_append(tickerbuds, td);

	td->ebox = gtk_event_box_new();
	gtk_ticker_add(GTK_TICKER(ticker), td->ebox);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(td->ebox), hbox);
	buddy_ticker_set_pixmap(b);
	gtk_box_pack_start(GTK_BOX(hbox), td->icon, FALSE, FALSE, 5);

	g_signal_connect(G_OBJECT(td->ebox), "button-press-event",
		G_CALLBACK(buddy_click_cb), b);

	td->label = gtk_label_new(gaim_get_buddy_alias(b));
	gtk_box_pack_start(GTK_BOX(hbox), td->label, FALSE, FALSE, 5);

	gtk_widget_show_all(td->ebox);
	gtk_widget_show(tickerwindow);
}

static void buddy_ticker_remove_buddy(struct buddy *b) {
	TickerData *td = buddy_ticker_find_buddy(b);

	if (!td)
		return;

	/* pop up the ticker window again */
	buddy_ticker_create_window();

	gtk_ticker_remove(GTK_TICKER(ticker), td->ebox);
	tickerbuds = g_list_remove(tickerbuds, td);
	g_free(td);
}

static void buddy_ticker_show()
{
	struct gaim_buddy_list *list = gaim_get_blist();
	GaimBlistNode *gnode, *bnode;
	struct buddy *b;

	if(!list)
		return;

	for(gnode = list->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			b = (struct buddy *)bnode;
			if(GAIM_BUDDY_IS_ONLINE(b))
				buddy_ticker_add_buddy(b);
		}
	}
}

void signon_cb(struct gaim_connection *gc, char *who) {
	struct buddy *b = gaim_find_buddy(gc->account, who);
	if(buddy_ticker_find_buddy(b))
		buddy_ticker_set_pixmap(b);
	else
		buddy_ticker_add_buddy(b);
}

void signoff_cb(struct gaim_connection *gc) {
	if (!connections) {
		while(tickerbuds) {
			g_free(tickerbuds->data);
			tickerbuds = g_list_delete_link(tickerbuds, tickerbuds);
		}
		gtk_widget_destroy(tickerwindow);
		tickerwindow = NULL;
		ticker = NULL;
	} else {
		GList *t = tickerbuds;
		while(t) {
			TickerData *td = t->data;
			t = t->next;
			if(td->buddy->account == gc->account) {
				g_free(td);
				tickerbuds = g_list_remove(tickerbuds, td);
			}
		}
	}
}

void buddy_signoff_cb(struct gaim_connection *gc, char *who) {
	struct buddy *b = gaim_find_buddy(gc->account, who);

	buddy_ticker_remove_buddy(b);
	if(!tickerbuds)
		gtk_widget_hide(tickerwindow);
}

void away_cb(struct gaim_connection *gc, char *who) {
	struct buddy *b = gaim_find_buddy(gc->account, who);
	if(buddy_ticker_find_buddy(b))
		buddy_ticker_set_pixmap(b);
	else
		buddy_ticker_add_buddy(b);
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *name() {
	return _("Buddy Ticker");
}

G_MODULE_EXPORT char *description() {
	return _("A horizontal scrolling version of the buddy list.");
}

G_MODULE_EXPORT char *gaim_plugin_init(GModule *h) {
	handle = h;

	gaim_signal_connect(h, event_buddy_signon, signon_cb, NULL);
	gaim_signal_connect(h, event_signoff, signoff_cb, NULL);
	gaim_signal_connect(h, event_buddy_signoff, buddy_signoff_cb, NULL);
	gaim_signal_connect(h, event_buddy_away, away_cb, NULL);
	gaim_signal_connect(h, event_buddy_back, away_cb, NULL);

	if (connections)
		buddy_ticker_show();
	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
	while(tickerbuds) {
		g_free(tickerbuds->data);
		g_list_delete_link(tickerbuds, tickerbuds);
	}
	gtk_widget_destroy(tickerwindow);
}

struct gaim_plugin_description desc;
G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Buddy Ticker"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("A horizontal scrolling version of the buddy list."));
	desc.authors = g_strdup("Syd Logan");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
