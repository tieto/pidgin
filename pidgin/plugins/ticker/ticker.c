/*
 * Purple Ticker Plugin
 * The line below doesn't apply at all, does it?  It should be Syd, Sean, and
 * maybe Nathan, I believe.
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
 * ticker.c -- Syd Logan, Summer 2000
 * pluginized- Sean Egan, Summer 2002
 */
#include "internal.h"
#include "pidgin.h"

#include "buddylist.h"
#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "signals.h"
#include "version.h"

#include "gtkblist.h"
#include "gtkplugin.h"
#include "gtkutils.h"
#include "pidginstock.h"

#include "gtkticker.h"

#define TICKER_PLUGIN_ID "gtk-ticker"

static GtkWidget *tickerwindow = NULL;
static GtkWidget *ticker;

typedef struct {
	PurpleContact *contact;
	GtkWidget *ebox;
	GtkWidget *label;
	GtkWidget *icon;
	guint timeout;
} TickerData;

static GList *tickerbuds = NULL;

static void buddy_ticker_update_contact(PurpleContact *contact);

static gboolean buddy_ticker_destroy_window(GtkWidget *window,
		GdkEventAny *event, gpointer data) {
	if(window)
		gtk_widget_hide(window);

	return TRUE; /* don't actually destroy the window */
}

static void buddy_ticker_create_window(void) {
	if(tickerwindow) {
		gtk_widget_show(tickerwindow);
		return;
	}

	tickerwindow = pidgin_create_window(_("Buddy Ticker"), 0, "ticker", TRUE);
	gtk_window_set_default_size(GTK_WINDOW(tickerwindow), 500, -1);
	g_signal_connect(G_OBJECT(tickerwindow), "delete_event",
			G_CALLBACK (buddy_ticker_destroy_window), NULL);

	ticker = gtk_ticker_new();
	gtk_ticker_set_spacing(GTK_TICKER(ticker), 20);
	gtk_container_add(GTK_CONTAINER(tickerwindow), ticker);
	gtk_ticker_set_interval(GTK_TICKER(ticker), 500);
	gtk_ticker_set_scootch(GTK_TICKER(ticker), 10);
	gtk_ticker_start_scroll(GTK_TICKER(ticker));
	gtk_widget_set_size_request(ticker, 1, -1);

	gtk_widget_show_all(tickerwindow);
}

static gboolean buddy_click_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	PurpleContact *contact = user_data;
	PurpleBuddy *b = purple_contact_get_priority_buddy(contact);

	PurpleIMConversation *im = purple_im_conversation_new(purple_buddy_get_account(b),
			purple_buddy_get_name(b));
	purple_conversation_present(PURPLE_CONVERSATION(im));
	return TRUE;
}

static TickerData *buddy_ticker_find_contact(PurpleContact *c) {
	GList *tb;
	for(tb = tickerbuds; tb; tb = tb->next) {
		TickerData *td = tb->data;
		if(td->contact == c)
			return td;
	}
	return NULL;
}

static void buddy_ticker_set_pixmap(PurpleContact *c)
{
	TickerData *td = buddy_ticker_find_contact(c);
	PurpleBuddy *buddy;
	PurplePresence *presence;
	const char *stock;

	if(!td)
		return;

	buddy = purple_contact_get_priority_buddy(c);
	presence = purple_buddy_get_presence(buddy);
	stock = pidgin_stock_id_from_presence(presence);
	if(!td->icon) {
		td->icon = gtk_image_new();
		g_object_set(G_OBJECT(td->icon), "stock", stock,
				"icon-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC),
				NULL);
	} else {
		g_object_set(G_OBJECT(td->icon), "stock", stock, NULL);
	}
}

static gboolean buddy_ticker_set_pixmap_cb(gpointer data) {
	TickerData *td = data;

	if (g_list_find(tickerbuds, td) != NULL) {
		buddy_ticker_update_contact(td->contact);
		td->timeout = 0;
	}

	return FALSE;
}

static void buddy_ticker_add_buddy(PurpleBuddy *b) {
	GtkWidget *hbox;
	TickerData *td;
	PurpleContact *contact;

	contact = purple_buddy_get_contact(b);

	buddy_ticker_create_window();

	if (!ticker)
		return;

	if (buddy_ticker_find_contact(contact))
	{
		buddy_ticker_update_contact(contact);
		return;
	}

	td = g_new0(TickerData, 1);
	td->contact = contact;
	tickerbuds = g_list_append(tickerbuds, td);

	td->ebox = gtk_event_box_new();
	gtk_ticker_add(GTK_TICKER(ticker), td->ebox);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(td->ebox), hbox);
	buddy_ticker_set_pixmap(contact);
	gtk_box_pack_start(GTK_BOX(hbox), td->icon, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(td->ebox), "button-press-event",
		G_CALLBACK(buddy_click_cb), contact);

	td->label = gtk_label_new(purple_contact_get_alias(contact));
	gtk_box_pack_start(GTK_BOX(hbox), td->label, FALSE, FALSE, 2);

	gtk_widget_show_all(td->ebox);
	gtk_widget_show(tickerwindow);

	/*
	 * Update the icon in a few seconds (after the open door icon has
	 * changed).  This is somewhat ugly.
	 */
	td->timeout = g_timeout_add(11000, buddy_ticker_set_pixmap_cb, td);
}

static void buddy_ticker_remove(TickerData *td) {
	gtk_ticker_remove(GTK_TICKER(ticker), td->ebox);
	tickerbuds = g_list_remove(tickerbuds, td);
	if (td->timeout != 0)
		g_source_remove(td->timeout);
	g_free(td);
}

static void buddy_ticker_update_contact(PurpleContact *contact) {
	TickerData *td = buddy_ticker_find_contact(contact);

	if (!td)
		return;

	/* pop up the ticker window again */
	buddy_ticker_create_window();
	if (purple_contact_get_priority_buddy(contact) == NULL)
		buddy_ticker_remove(td);
	else {
		buddy_ticker_set_pixmap(contact);
		gtk_label_set_text(GTK_LABEL(td->label), purple_contact_get_alias(contact));
	}
}

static void buddy_ticker_remove_buddy(PurpleBuddy *b) {
	PurpleContact *c = purple_buddy_get_contact(b);
	TickerData *td = buddy_ticker_find_contact(c);

	if (!td)
		return;

	purple_contact_invalidate_priority_buddy(c);

	/* pop up the ticker window again */
	buddy_ticker_create_window();
	buddy_ticker_update_contact(c);
}

static void buddy_ticker_show(void)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleBuddy *b;

	for(gnode = purple_blist_get_root();
	    gnode;
	    gnode = purple_blist_node_get_sibling_next(gnode))
	{
		if(!PURPLE_IS_GROUP(gnode))
			continue;
		for(cnode = purple_blist_node_get_first_child(gnode);
		    cnode;
		    cnode = purple_blist_node_get_sibling_next(cnode))
		{
			if(!PURPLE_IS_CONTACT(cnode))
				continue;
			for(bnode = purple_blist_node_get_first_child(cnode);
			    bnode;
			    bnode = purple_blist_node_get_sibling_next(bnode))
			{
				if(!PURPLE_IS_BUDDY(bnode))
					continue;
				b = PURPLE_BUDDY(bnode);
				if(PURPLE_BUDDY_IS_ONLINE(b))
					buddy_ticker_add_buddy(b);
			}
		}
	}
}

static void
buddy_signon_cb(PurpleBuddy *b)
{
	PurpleContact *c = purple_buddy_get_contact(b);
	purple_contact_invalidate_priority_buddy(c);
	if(buddy_ticker_find_contact(c))
		buddy_ticker_update_contact(c);
	else
		buddy_ticker_add_buddy(b);
}

static void
buddy_signoff_cb(PurpleBuddy *b)
{
	buddy_ticker_remove_buddy(b);
	if(!tickerbuds)
		gtk_widget_hide(tickerwindow);
}

static void
status_changed_cb(PurpleBuddy *b, PurpleStatus *os, PurpleStatus *s)
{
	PurpleContact *c = purple_buddy_get_contact(b);
	if(buddy_ticker_find_contact(c))
		buddy_ticker_set_pixmap(c);
	else
		buddy_ticker_add_buddy(b);
}

static void
signoff_cb(PurpleConnection *gc)
{
	TickerData *td;
	if (!purple_connections_get_all()) {
		while (tickerbuds) {
			td = tickerbuds->data;
			tickerbuds = g_list_delete_link(tickerbuds, tickerbuds);
			if (td->timeout != 0)
				g_source_remove(td->timeout);
			g_free(td);
		}
		gtk_widget_destroy(tickerwindow);
		tickerwindow = NULL;
		ticker = NULL;
	} else {
		GList *t = tickerbuds;
		while (t) {
			td = t->data;
			t = t->next;
			buddy_ticker_update_contact(td->contact);
		}
	}
}


/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *blist_handle = purple_blist_get_handle();

	purple_signal_connect(purple_connections_get_handle(), "signed-off",
						plugin, PURPLE_CALLBACK(signoff_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-on",
						plugin, PURPLE_CALLBACK(buddy_signon_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-off",
						plugin, PURPLE_CALLBACK(buddy_signoff_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-status-changed",
						plugin, PURPLE_CALLBACK(status_changed_cb), NULL);

	if (purple_connections_get_all())
		buddy_ticker_show();

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	TickerData *td;

	while (tickerbuds) {
		td = tickerbuds->data;
		tickerbuds = g_list_delete_link(tickerbuds, tickerbuds);
		if (td->timeout != 0)
			g_source_remove(td->timeout);
		g_free(td);
	}

	if (tickerwindow != NULL) {
		gtk_widget_destroy(tickerwindow);
		tickerwindow = NULL;
	}

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	TICKER_PLUGIN_ID,                                 /**< id             */
	N_("Buddy Ticker"),                               /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("A horizontal scrolling version of the buddy list."),
	                                                  /**  description    */
	N_("A horizontal scrolling version of the buddy list."),
	"Syd Logan",                                      /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(ticker, init_plugin, info)
