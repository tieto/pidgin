/*
 * gaim - Transparency plugin
 *
 * copyright (c) 1998-2002, rob flynn <rob@marko.net>
 * copyright (c) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 59 temple place, suite 330, boston, ma  02111-1307  usa
 *
 */
#include <gdk/gdkwin32.h>
#include "internal.h"

#include "core.h"
#include "prefs.h"
#include "debug.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "signals.h"
#include "version.h"

/*
 *  MACROS & DEFINES
 */
#define WINTRANS_PLUGIN_ID              "gtk-win-trans"
#define WINTRANS_VERSION                1

/* These defines aren't found in mingw's winuser.h */
#ifndef LWA_ALPHA
#define LWA_ALPHA                       0x00000002
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED                   0x00080000
#endif

#define blist (gaim_get_blist()?(GAIM_GTK_BLIST(gaim_get_blist())?((GAIM_GTK_BLIST(gaim_get_blist()))->window):NULL):NULL)

/*
 *  DATA STRUCTS
 */
typedef struct {
	GtkWidget *win;
	GtkWidget *slider;
} slider_win;


/*
 *  GLOBALS
 */

/*
 *  LOCALS
 */
static const char *OPT_WINTRANS_IM_ENABLED="/plugins/gtk/win32/wintrans/im_enabled";
static const char *OPT_WINTRANS_IM_ALPHA  ="/plugins/gtk/win32/wintrans/im_alpha";
static const char *OPT_WINTRANS_IM_SLIDER ="/plugins/gtk/win32/wintrans/im_slider";
static const char *OPT_WINTRANS_BL_ENABLED="/plugins/gtk/win32/wintrans/bl_enabled";
static const char *OPT_WINTRANS_BL_ALPHA  ="/plugins/gtk/win32/wintrans/bl_alpha";
static int imalpha = 255;
static int blalpha = 255;
static GList *window_list = NULL;

/*
 *  PROTOS
 */
BOOL (*MySetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)=NULL;

/*
 *  CODE
 */
static GtkWidget *wgaim_button(const char *text, const char *pref, GtkWidget *page) {
        GtkWidget *button;
	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(pref));
        gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
        return button;
}

/* Set window transparency level */
static void set_wintrans(GtkWidget *window, int trans) {
	if(MySetLayeredWindowAttributes) {
		HWND hWnd = GDK_WINDOW_HWND(window->window);
		SetWindowLong(hWnd,GWL_EXSTYLE,GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);
		MySetLayeredWindowAttributes(hWnd,0,trans,LWA_ALPHA);
	}
}

static void set_wintrans_off(GtkWidget *window) {
	if(MySetLayeredWindowAttributes) {
		HWND hWnd = GDK_WINDOW_HWND(window->window);
		SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);

		/* Ask the window and its children to repaint */
		RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
	}
}

static void change_alpha(GtkWidget *w, gpointer data) {
	set_wintrans(GTK_WIDGET(data), gtk_range_get_value(GTK_RANGE(w)));
}

static int has_transparency() {
	return MySetLayeredWindowAttributes ? TRUE : FALSE;
}

static GtkWidget *wintrans_slider(GtkWidget *win) {
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *frame;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), imalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);

	/* On slider val change, update window's transparency level */
	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(change_alpha), win);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	/* Set the initial transparency level */
	set_wintrans(win, imalpha);

	gtk_widget_show_all(hbox);

	return frame;
}

static slider_win* find_slidwin( GtkWidget *win ) {
	GList *tmp = window_list;

	while(tmp) {
		if( ((slider_win*)(tmp->data))->win == win)
			return (slider_win*)tmp->data;
		tmp = tmp->next;
	}
	return NULL;
}

static gboolean win_destroy_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	slider_win *slidwin=NULL;
	/* Remove window from the window list */
	gaim_debug_info(WINTRANS_PLUGIN_ID, "Conv window destoyed.. removing from list\n");

	if((slidwin=find_slidwin(widget))) {
		window_list = g_list_remove(window_list, (gpointer)slidwin);
		g_free(slidwin);
	}
	return FALSE;
}

static void set_trans_option(GtkWidget *w, const char *pref) {
        gaim_prefs_set_bool(pref, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
	if(pref == OPT_WINTRANS_BL_ENABLED) {
		if(blist) {
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				set_wintrans(blist, blalpha);
			else
				set_wintrans_off(blist);
		}
	}
}

static void add_slider(GtkWidget *win) {
	GList *wl, *wl1;
	GtkWidget *vbox=NULL;

	/* Look up this window to see if it already has a slider */
	if(!find_slidwin(win)) {
		GtkWidget *slider_box=NULL;
		slider_win *slidwin=NULL;
		GtkRequisition slidereq;
		gint width, height;

		/* Get top vbox */
		for ( wl1 = wl = gtk_container_get_children(GTK_CONTAINER(win));
			  wl != NULL;
			  wl = wl->next ) {
			if ( GTK_IS_VBOX(GTK_OBJECT(wl->data)) )
				vbox = GTK_WIDGET(wl->data);
			else {
				gaim_debug_error(WINTRANS_PLUGIN_ID, "no vbox found\n");
				return;
			}
		}
		g_list_free(wl1);

		slider_box = wintrans_slider(win);
		/* Figure out how tall the slider wants to be */
		gtk_widget_size_request(slider_box, &slidereq);
		gtk_window_get_size(GTK_WINDOW(win), &width, &height);
		gtk_box_pack_start(GTK_BOX(vbox),
						   slider_box,
						   FALSE, FALSE, 0);
		/* Make window taller so we don't slowly collapse its message area */
		gtk_window_resize(GTK_WINDOW(win), width, (height + slidereq.height));
		/* Add window to list, to track that it has a slider */
		slidwin = g_new0( slider_win, 1 );
		slidwin->win = win;
		slidwin->slider = slider_box;
		window_list = g_list_append(window_list, (gpointer)slidwin);
		/* Set callback to remove window from the list, if the window is destroyed */
		g_signal_connect(GTK_OBJECT(win), "destroy_event", G_CALLBACK(win_destroy_cb), NULL);
	}
}

static void remove_sliders() {
	if(window_list) {
		GList *tmp=window_list;
		while(tmp) {
			slider_win *slidwin = (slider_win*)tmp->data;
			if (slidwin != NULL && GTK_IS_WINDOW(slidwin->win)) {
				GtkRequisition slidereq;
				gint width, height;
				/* Figure out how tall the slider was */
				gtk_widget_size_request(slidwin->slider, &slidereq);
				gtk_window_get_size(GTK_WINDOW(slidwin->win), &width, &height);

				gtk_widget_destroy(slidwin->slider);

				gtk_window_resize(GTK_WINDOW(slidwin->win), width, (height - slidereq.height));
			}
			g_free(slidwin);
			tmp=tmp->next;
		}
		g_list_free(window_list);
		window_list = NULL;
	}
}

static void remove_convs_wintrans() {
	GList *conv;

	for(conv = gaim_get_conversations(); conv != NULL; conv = conv->next)
		set_wintrans_off(GAIM_GTK_WINDOW(gaim_conversation_get_window(conv->data))->window);
}

static void update_convs_wintrans(GtkWidget *w, const char *pref) {
	GList *conv;

	if (w != NULL)
		set_trans_option(w, pref);

	if(gaim_prefs_get_bool(OPT_WINTRANS_IM_ENABLED)) {
		for(conv = gaim_get_conversations(); conv != NULL; conv = conv->next)
			set_wintrans(GAIM_GTK_WINDOW(gaim_conversation_get_window(conv->data))->window, gaim_prefs_get_int(OPT_WINTRANS_IM_ALPHA));
	}
	else
		remove_convs_wintrans();

	if(gaim_prefs_get_bool(OPT_WINTRANS_IM_SLIDER)
	   && gaim_prefs_get_bool(OPT_WINTRANS_IM_ENABLED)) {
		for(conv = gaim_get_conversations(); conv != NULL; conv = conv->next)
			add_slider(GAIM_GTK_WINDOW(gaim_conversation_get_window(conv->data))->window);
	}
	else
		remove_sliders();
}

static void set_window_trans(GaimConvWindow *oldwin, GaimConvWindow *newwin) {
	GtkWidget *win = GAIM_GTK_WINDOW(newwin)->window;

	/* check prefs to see if we want trans */
	if (gaim_prefs_get_bool(OPT_WINTRANS_IM_ENABLED) && gaim_prefs_get_bool(OPT_WINTRANS_IM_SLIDER)) {
		add_slider(win);
	}

	if(gaim_prefs_get_bool(OPT_WINTRANS_IM_ENABLED) &&
	   !gaim_prefs_get_bool(OPT_WINTRANS_IM_SLIDER)) {
		set_wintrans(win, imalpha);
	}
}

static void gaim_new_conversation(GaimConversation *c) {
	GaimConvWindow *win = gaim_conversation_get_window(c);
	set_window_trans(NULL, win);
}

static void blist_created() {
	if(blist) {
		if(gaim_prefs_get_bool(OPT_WINTRANS_BL_ENABLED))
			set_wintrans(blist, blalpha);
		else
			set_wintrans_off(blist);
	}
}

static void alpha_change(GtkWidget *w, gpointer data) {
	GList *conv;
	imalpha = gtk_range_get_value(GTK_RANGE(w));

	for(conv = gaim_get_conversations(); conv != NULL; conv = conv->next)
		set_wintrans(GAIM_GTK_WINDOW(gaim_conversation_get_window(conv->data))->window, imalpha);
}

static void alpha_pref_set_int(GtkWidget *w, GdkEventFocus *e, const char *pref)
{
	if (pref == OPT_WINTRANS_IM_ALPHA)
		gaim_prefs_set_int(pref, imalpha);
	else if (pref == OPT_WINTRANS_BL_ALPHA)
		gaim_prefs_set_int(pref, blalpha);
}

static void bl_alpha_change(GtkWidget *w, gpointer data) {
	blalpha = gtk_range_get_value(GTK_RANGE(w));
	if(blist)
		change_alpha(w, blist);
}

/*
 *  EXPORTED FUNCTIONS
 */
gboolean plugin_load(GaimPlugin *plugin) {
	imalpha = gaim_prefs_get_int(OPT_WINTRANS_IM_ALPHA);
	blalpha = gaim_prefs_get_int(OPT_WINTRANS_BL_ALPHA);

	gaim_signal_connect((void*)gaim_conversations_get_handle(),
						"conversation-created",
						plugin,
						GAIM_CALLBACK(gaim_new_conversation),
						NULL);
	gaim_signal_connect((void*)gaim_connections_get_handle(), "signed-on", plugin, GAIM_CALLBACK(blist_created), NULL);
	MySetLayeredWindowAttributes = (void*)wgaim_find_and_loadproc("user32.dll", "SetLayeredWindowAttributes" );

	gaim_signal_connect((void*)gaim_gtk_conversations_get_handle(), "conversation-drag-ended", plugin, GAIM_CALLBACK(set_window_trans), NULL);

	update_convs_wintrans(NULL, NULL);

	if(blist)
		blist_created();

	return TRUE;
}

gboolean plugin_unload(GaimPlugin *plugin) {
	gaim_debug_info(WINTRANS_PLUGIN_ID, "Removing win2ktrans.dll plugin\n");

	remove_convs_wintrans();
	remove_sliders();

	if(blist)
		set_wintrans_off(blist);

	return TRUE;
}

GtkWidget *get_config_frame(GaimPlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *imtransbox, *bltransbox;
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *button;
	GtkWidget *trans_box;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	/* IM Convo trans options */
	imtransbox = gaim_gtk_make_frame (ret, _("IM Conversation Windows"));
	button = wgaim_button(_("_IM window transparency"), OPT_WINTRANS_IM_ENABLED, imtransbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_convs_wintrans), (void *)OPT_WINTRANS_IM_ENABLED);

	trans_box =  gtk_vbox_new(FALSE, 18);
	if (!gaim_prefs_get_bool(OPT_WINTRANS_IM_ENABLED))
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	gtk_widget_show(trans_box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(gaim_gtk_toggle_sensitive),  trans_box);

	button = wgaim_button(_("_Show slider bar in IM window"), OPT_WINTRANS_IM_SLIDER, trans_box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_convs_wintrans), (void *)OPT_WINTRANS_IM_SLIDER);

	gtk_box_pack_start(GTK_BOX(imtransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), imalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);

	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(alpha_change), NULL);
	gtk_signal_connect(GTK_OBJECT(slider), "focus-out-event", GTK_SIGNAL_FUNC(alpha_pref_set_int), (void *)OPT_WINTRANS_IM_ALPHA);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);

	/* Buddy List trans options */
	bltransbox = gaim_gtk_make_frame (ret, _("Buddy List Window"));
	button = wgaim_button(_("_Buddy List window transparency"), OPT_WINTRANS_BL_ENABLED, bltransbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_trans_option), (void *)OPT_WINTRANS_BL_ENABLED);

	trans_box =  gtk_vbox_new(FALSE, 18);
	if (!gaim_prefs_get_bool(OPT_WINTRANS_BL_ENABLED))
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	gtk_widget_show(trans_box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(gaim_gtk_toggle_sensitive),  trans_box);
	gtk_box_pack_start(GTK_BOX(bltransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), blalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);

	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(bl_alpha_change), NULL);
	gtk_signal_connect(GTK_OBJECT(slider), "focus-out-event", GTK_SIGNAL_FUNC(alpha_pref_set_int), (void *)OPT_WINTRANS_BL_ALPHA);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);

	/* If this version of Windows doesn't support Transparency, grey out options */
	if(!has_transparency()) {
		gaim_debug_warning(WINTRANS_PLUGIN_ID, "This version of Windows doesn't support transparency\n");
		gtk_widget_set_sensitive(GTK_WIDGET(imtransbox), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	}

	gtk_widget_show_all(ret);
	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	WINTRANS_PLUGIN_ID,                               /**< id             */
	N_("Transparency"),                               /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Variable Transparency for the buddy list and conversations."),
	                                                  /**  description    */
	N_("This plugin enables variable alpha transparency on conversation windows and the buddy list.\n\n"
           "* Note: This plugin requires Win2000 or WinXP."),
	"Herman Bloggs <hermanator12002@yahoo.com>",      /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/win32");
	gaim_prefs_add_none("/plugins/gtk/win32/wintrans");
	gaim_prefs_add_bool("/plugins/gtk/win32/wintrans/im_enabled", FALSE);
	gaim_prefs_add_int("/plugins/gtk/win32/wintrans/im_alpha", 255);
	gaim_prefs_add_bool("/plugins/gtk/win32/wintrans/im_slider", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/win32/wintrans/bl_enabled", FALSE);
	gaim_prefs_add_int("/plugins/gtk/win32/wintrans/bl_alpha", 255);
}

GAIM_INIT_PLUGIN(wintrans, init_plugin, info)
