/*
 * win2ktrans
 *
 * copyright (c) 1998-2002, rob flynn <rob@marko.net>
 *
 * Contributions by Herman Bloggs <hermanator12002@yahoo.com>
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
#define GAIM_PLUGINS

#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>
#include "gaim.h"
#include "win32dep.h"

/*
 *  MACROS & DEFINES
 */
#define WINTRANS_VERSION 1

/* These defines aren't found in mingw's winuser.h */
#ifndef LWA_ALPHA
#define LWA_ALPHA               0x00000002
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED           0x00080000
#endif

/* Transparency plugin configuration */
#define OPT_WGAIM_IMTRANS               0x00000001
#define OPT_WGAIM_SHOW_IMTRANS          0x00000002
#define OPT_WGAIM_BLTRANS               0x00000004
#define OPT_WGAIM_BUDDYWIN_ONTOP        0x00000008

/*
 *  GLOBALS
 */
G_MODULE_IMPORT guint im_options;
G_MODULE_IMPORT GtkWidget *all_convos;
G_MODULE_IMPORT GtkWidget *blist;

/*
 *  LOCALS
 */
static GtkWidget *slider_box=NULL;
static int imalpha = 255;
static int blalpha = 255;
guint trans_options=0;

/*
 *  PROTOS
 */
BOOL (*MySetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)=NULL;
extern GtkWidget *gaim_button(const char*, guint*, int, GtkWidget*);
static void save_trans_prefs();

/*
 *  CODE
 */
/* Set window transparency level */
void set_wintrans(GtkWidget *window, int trans) {
	if(MySetLayeredWindowAttributes) {
		HWND hWnd = GDK_WINDOW_HWND(window->window);
		SetWindowLong(hWnd,GWL_EXSTYLE,GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);
		MySetLayeredWindowAttributes(hWnd,0,trans,LWA_ALPHA);
	}
}

void set_wintrans_off(GtkWidget *window) {
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

int has_transparency() {
	return MySetLayeredWindowAttributes ? TRUE : FALSE;
}

GtkWidget *wintrans_slider(GtkWidget *win) {
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *frame;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
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

static void gaim_del_conversation(char *who) {
	if(!all_convos)
		slider_box = NULL;
}

static void gaim_new_conversation(char *who) {
	GList *wl, *wl1;
	GtkWidget *vbox=NULL;

	struct conversation *c = find_conversation(who);

	/* Single convo window */
	if((im_options & OPT_IM_ONE_WINDOW)) {
		/* check prefs to see if we want trans */
		if ((trans_options & OPT_WGAIM_IMTRANS) &&
		    (trans_options & OPT_WGAIM_SHOW_IMTRANS)) {
			if(!slider_box) {
				/* Get vbox which to add slider to.. */
				for ( wl1 = wl = gtk_container_children(GTK_CONTAINER(c->window));
				      wl != NULL;
				      wl = wl->next ) {
					if ( GTK_IS_VBOX(GTK_OBJECT(wl->data)) )
						vbox = GTK_WIDGET(wl->data);
					else
						debug_printf("no vbox found\n");
				}
				g_list_free(wl1);
				
				slider_box = wintrans_slider(c->window);
				gtk_box_pack_start(GTK_BOX(vbox),
						   slider_box,
						   FALSE, FALSE, 0);
			}
		}
	}
	/* One window per convo */
	else {
		if ((trans_options & OPT_WGAIM_IMTRANS) &&
		    (trans_options & OPT_WGAIM_SHOW_IMTRANS)) {
			GtkWidget *paned;

			/* Get container which to add slider to.. */
			wl = gtk_container_children(GTK_CONTAINER(c->window));
			paned = GTK_WIDGET(wl->data);
			g_list_free(wl);
			wl = gtk_container_children(GTK_CONTAINER(paned));
			/* 2nd child */
			vbox = GTK_WIDGET((wl->next)->data);
			g_list_free(wl);
			
			if(!GTK_IS_VBOX(vbox))
				debug_printf("vbox isn't a vbox widget\n");
			
			gtk_box_pack_start(GTK_BOX(vbox),
					   wintrans_slider(c->window),
					   FALSE, FALSE, 0);
		}
	}

	if((trans_options & OPT_WGAIM_IMTRANS) &&
	   !(trans_options & OPT_WGAIM_SHOW_IMTRANS)) {
		set_wintrans(c->window, imalpha);
	}
}

static void blist_created() {
	if(blist) {
		if(trans_options & OPT_WGAIM_BUDDYWIN_ONTOP)
			SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		else
			SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		if(trans_options & OPT_WGAIM_BLTRANS)
			set_wintrans(blist, blalpha);
		else
			set_wintrans_off(blist);
	}
}

static void alpha_change(GtkWidget *w, gpointer data) {
	int *alpha = (int*)data;
	*alpha = gtk_range_get_value(GTK_RANGE(w));
}

static void bl_alpha_change(GtkWidget *w, gpointer data) {
	alpha_change(w, data);
	if(blist)
		change_alpha(w, blist);
}

/* Load options */
static void load_trans_prefs() {
	FILE *f;
	char buf[1024];
	int ver=0;
	char tag[256];

	if (gaim_home_dir())
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S ".gaim" G_DIR_SEPARATOR_S "wintransrc", gaim_home_dir());
	else
		return;

	if ((f = fopen(buf, "r"))) {
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "# wintransrc v%d", &ver);
		if ((ver > 1) || (buf[0] != '#'))
			return;

		while (!feof(f)) {
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "%s {", tag);
			if (strcmp(tag, "options")==0) {
				fgets(buf, sizeof(buf), f);
				sscanf(buf, "\ttrans_options { %d", &trans_options);
				continue;
			} else if (strcmp(tag, "trans")==0) {
				int num;
				for(fgets(buf, sizeof(buf), f);buf[0] != '}' && !feof(f);fgets(buf, sizeof(buf), f)) {
					sscanf(buf, "\t%s { %d", tag, &num);
					if(strcmp(tag, "imalpha")==0) {
						imalpha = num;
					}
					else if(strcmp(tag, "blalpha")==0) {
						blalpha = num;
					}
				}
			}
		}
	}
	else
		save_trans_prefs();
	blist_created();
}

/* Save options */

static void write_options(FILE *f) {
	fprintf(f, "options {\n");
	fprintf(f, "\ttrans_options { %u }\n", trans_options);
	fprintf(f, "}\n");
}

static void write_trans(FILE *f) {
	fprintf(f, "trans {\n");
	fprintf(f, "\timalpha { %d }\n", imalpha);
	fprintf(f, "\tblalpha { %d }\n", blalpha);
	fprintf(f, "}\n");
}

static void save_trans_prefs() {
	FILE *f;
	char buf[1024];

	if (gaim_home_dir()) {
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S ".gaim" G_DIR_SEPARATOR_S "wintransrc", gaim_home_dir());
	}
	else
		return;

	if ((f = fopen(buf, "w"))) {
		fprintf(f, "# wintransrc v%d\n", WINTRANS_VERSION);
		write_trans(f);
		write_options(f);
		fclose(f);
	}
	else
		debug_printf("Error opening wintransrc\n");
}

static void set_trans_option(GtkWidget *w, int option) {
	trans_options ^= option;
	save_trans_prefs();

	if(option == OPT_WGAIM_BUDDYWIN_ONTOP) {
		if(blist) {
			if(trans_options & OPT_WGAIM_BUDDYWIN_ONTOP)
				SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			else
				SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	} else if(option == OPT_WGAIM_BLTRANS) {
		if(blist) {
			if(trans_options & OPT_WGAIM_BLTRANS)
				set_wintrans(blist, blalpha);
			else
				set_wintrans_off(blist);
		}
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *gaim_plugin_init(GModule *handle) {
	gaim_signal_connect(handle, event_new_conversation, gaim_new_conversation, NULL); 
	gaim_signal_connect(handle, event_del_conversation, gaim_del_conversation, NULL); 
	gaim_signal_connect(handle, event_signon, blist_created, NULL);
	MySetLayeredWindowAttributes = (void*)wgaim_find_and_loadproc("user32.dll", "SetLayeredWindowAttributes" );
	load_trans_prefs();

	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
}

struct gaim_plugin_description desc; 

G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Transparency"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("This plugin enables variable alpha transparency on conversation windows.\n\n* Note: This plugin requires Win2000 or WinXP.")); 
	desc.authors = g_strdup(_("Rob Flynn &lt;rob@marko.net&gt;"));
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

G_MODULE_EXPORT char *name() {
	return _("Transparency");
}

G_MODULE_EXPORT char *description() {
	return _("This plugin enables variable alpha transparency on conversation windows.\n\n* Note: This plugin requires Win2000 or WinXP.");
}

G_MODULE_EXPORT GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *ret;
	GtkWidget *imtransbox, *bltransbox;
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *button;
	GtkWidget *trans_box;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	/* IM Convo trans options */
	imtransbox = make_frame (ret, _("IM Conversation Windows"));
	button = gaim_button(_("_IM window transparency"), &trans_options, OPT_WGAIM_IMTRANS, imtransbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_trans_option), (int *)OPT_WGAIM_IMTRANS);

	trans_box =  gtk_vbox_new(FALSE, 18);
	if (!(trans_options & OPT_WGAIM_IMTRANS))
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	gtk_widget_show(trans_box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),  trans_box);
	
	button = gaim_button(_("_Show slider bar in IM window"), &trans_options, OPT_WGAIM_SHOW_IMTRANS, trans_box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_trans_option), (int *)OPT_WGAIM_SHOW_IMTRANS);

	gtk_box_pack_start(GTK_BOX(imtransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), imalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);
	
	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(alpha_change), (void*)&imalpha);
	gtk_signal_connect(GTK_OBJECT(slider), "focus-out-event", GTK_SIGNAL_FUNC(save_trans_prefs), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);
	
	/* Buddy List trans options */
	bltransbox = make_frame (ret, _("Buddy List Window"));
	button = gaim_button(_("_Keep Buddy List window on top"), &trans_options, OPT_WGAIM_BUDDYWIN_ONTOP, bltransbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_trans_option), (int *)OPT_WGAIM_BUDDYWIN_ONTOP);

	button = gaim_button(_("_Buddy List window transparency"), &trans_options, OPT_WGAIM_BLTRANS, bltransbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_trans_option), (int *)OPT_WGAIM_BLTRANS);

	trans_box =  gtk_vbox_new(FALSE, 18);
	if (!(trans_options & OPT_WGAIM_BLTRANS))
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	gtk_widget_show(trans_box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),  trans_box);
	gtk_box_pack_start(GTK_BOX(bltransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), blalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);
	
	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(bl_alpha_change), (void*)&blalpha);
	gtk_signal_connect(GTK_OBJECT(slider), "focus-out-event", GTK_SIGNAL_FUNC(save_trans_prefs), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);

	/* If this version of Windows dosn't support Transparency, grey out options */
	if(!has_transparency()) {
		debug_printf("This version of windows dosn't support transparency\n");
		gtk_widget_set_sensitive(GTK_WIDGET(imtransbox), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(trans_box), FALSE);
	}

	gtk_widget_show_all(ret);
	return ret;
}
