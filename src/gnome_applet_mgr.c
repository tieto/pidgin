/**************************************************************
**
** GaimGnomeAppletMgr
** Author - Quinticent (John Palmieri: johnp@martianrock.com)
**
** Purpose - Takes over the task of managing the GNOME applet
**           code and provides a centralized codebase for
**	     GNOME integration for Gaim.
**
**
** gaim
**
** Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#ifdef USE_APPLET
#include <string.h>
#include <gdk_imlib.h>
#include "gaim.h"
#include "gnome_applet_mgr.h"

enum gaim_user_states MRI_user_status; 

gboolean applet_buddy_show = FALSE;
GtkWidget *applet_popup = NULL;

/*
gchar GAIM_GNOME_OFFLINE_ICON[255] = GAIM_GNOME_PENGUIN_OFFLINE;
gchar GAIM_GNOME_CONNECT_ICON[255] = GAIM_GNOME_PENGUIN_CONNECT;
gchar GAIM_GNOME_ONLINE_ICON[255] = GAIM_GNOME_PENGUIN_ONLINE;
*/

GtkWidget *applet;
GtkWidget *appletframe;
GtkWidget *status_label;

GtkWidget *icon;
GdkPixmap *icon_offline_pm=NULL;
GdkPixmap *icon_offline_bm=NULL;

GdkPixmap *icon_online_pm=NULL;
GdkPixmap *icon_online_bm=NULL;

GdkPixmap *icon_connect_pm=NULL;
GdkPixmap *icon_connect_bm=NULL;

GdkPixmap *icon_msg_pending_pm=NULL;
GdkPixmap *icon_msg_pending_bm=NULL;

GdkPixmap *icon_away_pm=NULL;
GdkPixmap *icon_away_bm=NULL;

static GtkAllocation get_applet_pos(gboolean);
gint sizehint=48;

static gboolean load_applet_icon(const char *name, int height, int width,
				 GdkPixmap **pm, GdkBitmap **bm)
{
	gboolean result = TRUE;
	char *path;
	GdkImlibImage *im;

	path = gnome_pixmap_file(name);	

	im=gdk_imlib_load_image( path );
	
	if ((*pm)!=NULL)
		gdk_imlib_free_pixmap((*pm));
	
	if( im!= NULL ){
		gdk_imlib_render(im,width,height);
	
		(*pm) = gdk_imlib_move_image(im);
		(*bm) = gdk_imlib_move_mask(im);	
		
	} else {
		result = FALSE;
		sprintf(debug_buff,_("file not found: %s\n"),path);
		debug_print(debug_buff);
	}
	
	free(path);
	return result;
}

#ifdef HAVE_PANEL_PIXEL_SIZE
static void applet_change_pixel_size(GtkWidget *w, int size, gpointer data)
{
	sizehint = size;
	update_pixmaps();
}
#endif

static gboolean update_applet(){
	char buf[BUF_LONG];
	GSList *c = connections;

	switch( MRI_user_status ){
	case offline:
		gtk_pixmap_set( GTK_PIXMAP(icon),
				icon_offline_pm,
				icon_offline_bm );
		gtk_label_set( GTK_LABEL(status_label), _MSG_OFFLINE_ );
		applet_set_tooltips(_("Offilne. Click to bring up login box."));
		break;
	case signing_on:
		gtk_pixmap_set( GTK_PIXMAP(icon),
				icon_connect_pm,
				icon_connect_bm );   
		gtk_label_set( GTK_LABEL(status_label), _MSG_CONNECT_ );
		applet_set_tooltips(_("Attempting to sign on...."));
		break;
	case online:
		gtk_pixmap_set( GTK_PIXMAP(icon),
				icon_online_pm,
				icon_online_bm );                
		gtk_label_set( GTK_LABEL(status_label), _MSG_ONLINE_ );
		g_snprintf(buf, sizeof buf, "Online: ");
		while (c) {
			strcat(buf, ((struct gaim_connection *)c->data)->username);
			c = g_slist_next(c);
			if (c) strcat(buf, ", ");
		}
		applet_set_tooltips(buf);
		break;
	case away:
		gtk_pixmap_set( GTK_PIXMAP(icon),
				icon_online_pm,
				icon_online_bm );   
		gtk_label_set( GTK_LABEL(status_label), _("Away") );
		break;
	}

	return TRUE;
}

void update_pixmaps() {
	/*
	if (display_options & OPT_DISP_DEVIL_PIXMAPS) {
		sprintf(GAIM_GNOME_OFFLINE_ICON, "%s",  GAIM_GNOME_DEVIL_OFFLINE);
		sprintf(GAIM_GNOME_CONNECT_ICON, "%s",  GAIM_GNOME_DEVIL_CONNECT);
		sprintf(GAIM_GNOME_ONLINE_ICON, "%s",  GAIM_GNOME_DEVIL_ONLINE);
	} else {
		sprintf(GAIM_GNOME_OFFLINE_ICON, "%s",  GAIM_GNOME_PENGUIN_OFFLINE);
		sprintf(GAIM_GNOME_CONNECT_ICON, "%s",  GAIM_GNOME_PENGUIN_CONNECT);
		sprintf(GAIM_GNOME_ONLINE_ICON, "%s",  GAIM_GNOME_PENGUIN_ONLINE);
	}
	*/
	load_applet_icon( GAIM_GNOME_OFFLINE_ICON, (sizehint-16), (sizehint-12),
			&icon_offline_pm, &icon_offline_bm );
	load_applet_icon( GAIM_GNOME_CONNECT_ICON, (sizehint-16), (sizehint-12),
			&icon_connect_pm, &icon_connect_bm );
	load_applet_icon( GAIM_GNOME_ONLINE_ICON, (sizehint-16), (sizehint-12),
			&icon_online_pm, &icon_online_bm );
	update_applet();
	gtk_widget_set_usize(appletframe, sizehint, sizehint);
}


extern GtkWidget *mainwindow;
void applet_show_login(AppletWidget *widget, gpointer data) {
        show_login();
	if (general_options & OPT_GEN_NEAR_APPLET) {
		GtkAllocation a = get_applet_pos(FALSE);
		gtk_widget_set_uposition(mainwindow, a.x, a.y);
	}
}

void applet_do_signon(AppletWidget *widget, gpointer data) {
	applet_show_login(NULL, 0);
}

void insert_applet_away() {
	GSList *awy = away_messages;
	struct away_message *a;
	char  *awayname;

	applet_widget_register_callback_dir(APPLET_WIDGET(applet),
		"away/",
		_("Away"));
	applet_widget_register_callback(APPLET_WIDGET(applet),
		"away/new",
		_("New Away Message"),
		(AppletCallbackFunc)create_away_mess,
		NULL);

	while(awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_register_callback(APPLET_WIDGET(applet),
			awayname,
			a->name,
			(AppletCallbackFunc)do_away_message,
			a);

		awy = g_slist_next(awy);
		free(awayname);
	}
}

void remove_applet_away() {
	GSList *awy = away_messages;
	struct away_message *a;
	char  *awayname;

	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away/new");

	while (awy) {
		a = (struct away_message *)awy->data;

		awayname = g_malloc(sizeof *awayname * (6 + strlen(a->name)));
		awayname[0] = '\0';
		strcat(awayname, "away/");
		strcat(awayname, a->name);
		applet_widget_unregister_callback(APPLET_WIDGET(applet), awayname);

		awy = g_slist_next(awy);
		free(awayname);
	}
	applet_widget_unregister_callback_dir(APPLET_WIDGET(applet), "away/");
	applet_widget_unregister_callback(APPLET_WIDGET(applet), "away");
}

static void applet_show_about(AppletWidget *widget, gpointer data) {
  
  const gchar *authors[] = {"Mark Spencer <markster@marko.net>",
                            "Jim Duchek <jimduchek@ou.edu>",
                            "Rob Flynn <rflynn@blueridge.net>",
			    "Eric Warmenhoven <warmenhoven@yahoo.com>",
			    "Syd Logan",
                            NULL};

  GtkWidget *about=gnome_about_new(_("GAIM"),
				   _(VERSION),
				   _(""),
				   authors,
				   "",
				   NULL);
  gtk_widget_show(about);
}

static GtkAllocation get_applet_pos(gboolean for_blist) {
	gint x,y,pad;
	GtkRequisition buddy_req, applet_req;
	GtkAllocation result;
	GNOME_Panel_OrientType orient = applet_widget_get_panel_orient( APPLET_WIDGET(applet) );
	pad = 5;

	gdk_window_get_position(gtk_widget_get_parent_window(appletframe), &x, &y);
	if (for_blist) {
	        if (general_options & OPT_GEN_SAVED_WINDOWS) {
			buddy_req.width = blist_pos.width;
			buddy_req.height = blist_pos.height;
		} else {
			buddy_req = blist->requisition;
		}
	} else {
		buddy_req = mainwindow->requisition;
	}
	applet_req = appletframe->requisition;

	/* FIXME : we need to be smarter here */
	switch( orient ){
	case ORIENT_UP:
		result.x=x;
		result.y=y-(buddy_req.height+pad);
		break;
	case ORIENT_DOWN:
		result.x=x; 
		result.y=y+applet_req.height+pad; 
		break;
	case ORIENT_LEFT:
		result.x=x-(buddy_req.width + pad );
		result.y=y;
		break;
	case ORIENT_RIGHT:
		result.x=x+applet_req.width+pad;
		result.y=y;
		break;
	} 
	return result;
}

void createOnlinePopup(){
	GtkAllocation al;
	if (blist) gtk_widget_show(blist);
	al  = get_applet_pos(TRUE);  
        if (general_options & OPT_GEN_NEAR_APPLET)
                gtk_widget_set_uposition ( blist, al.x, al.y );
        else if (general_options & OPT_GEN_SAVED_WINDOWS)
                gtk_widget_set_uposition(blist, blist_pos.x - blist_pos.xoff, blist_pos.y - blist_pos.yoff);
}

void AppletClicked( GtkWidget *sender, GdkEventButton *ev, gpointer data ){
	if (!ev || ev->button != 1 || ev->type != GDK_BUTTON_PRESS)
		return;
        
	if(applet_buddy_show) {
		applet_buddy_show = FALSE;
	  	switch( MRI_user_status ){
			case offline:
				if (mainwindow)
					gtk_widget_hide(mainwindow);
				break;
			case online:
			case away:
				applet_destroy_buddy(0, 0, 0);
				break;
		}     
	} else {
		applet_buddy_show = TRUE;
		switch( MRI_user_status ){
			case offline:
				applet_show_login( APPLET_WIDGET(applet), NULL );
				break;
			case online:
			case away:
				createOnlinePopup();
				break;
		}
	}
}


/***************************************************************
**
** Initialize GNOME stuff
**
****************************************************************/

gint init_applet_mgr(int argc, char *argv[]) {
	GtkWidget *vbox;
	
	GtkStyle *label_style;
	GdkFont *label_font = NULL;

        applet_widget_init("GAIM",VERSION,argc,argv,NULL,0,NULL);
        
        /*init imlib for graphics*/ 
        gdk_imlib_init();
        gtk_widget_push_visual(gdk_imlib_get_visual());
        gtk_widget_push_colormap(gdk_imlib_get_colormap());

        applet=applet_widget_new("gaim_applet");
        if(!applet) g_error(_("Can't create GAIM applet!"));
	gtk_widget_set_events(applet, gtk_widget_get_events(applet) |
				GDK_BUTTON_PRESS_MASK);

        appletframe = gtk_frame_new(NULL);
#ifdef HAVE_PANEL_PIXEL_SIZE
	gtk_widget_set_usize(appletframe, 5, 5);
#else
        gtk_widget_set_usize(appletframe, 48, 48);
#endif   
	
	/*load offline icon*/
	load_applet_icon( GAIM_GNOME_OFFLINE_ICON, 32, 32,
			&icon_offline_pm, &icon_offline_bm );
 
 	/*load connecting icon*/
 	load_applet_icon( GAIM_GNOME_CONNECT_ICON, 32, 32,
			&icon_connect_pm, &icon_connect_bm );
							 
	/*load online icon*/
	load_applet_icon( GAIM_GNOME_ONLINE_ICON, 32, 32,
			&icon_online_pm, &icon_online_bm );
 	
 	/*icon_away and icon_msg_pennding need to be implemented*/		
		
	icon=gtk_pixmap_new(icon_offline_pm,icon_offline_bm);
	
	vbox = gtk_vbox_new(FALSE,0);
	
	gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, TRUE, 0);
	
	status_label = gtk_label_new(_("Offline"));

	update_applet();
	
	/*set this label's font*/
	label_style = gtk_widget_get_style( status_label );
	
	label_font = gdk_font_load( _MSG_FONT_ );
	         
	
	if( label_font != NULL ){
		label_style->font = label_font; 
		gtk_widget_set_style( status_label, label_style );
	} else {
		sprintf(debug_buff, _("Font does not exist") );
		debug_print(debug_buff);
	}
	
	gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, TRUE, 0);
	
	gtk_container_add( GTK_CONTAINER(appletframe), vbox );
	applet_widget_add(APPLET_WIDGET(applet), appletframe);
	
	gtk_widget_show( status_label );
	gtk_widget_show( vbox );
	gtk_widget_show( appletframe );
	        
	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      applet_show_about,
					      NULL);
					      
	gtk_signal_connect( GTK_OBJECT(applet), "button_press_event", GTK_SIGNAL_FUNC( AppletClicked), NULL);

	gtk_signal_connect( GTK_OBJECT(applet), "destroy", GTK_SIGNAL_FUNC( do_quit), NULL);

#ifdef HAVE_PANEL_PIXEL_SIZE
	gtk_signal_connect(GTK_OBJECT(applet), "change_pixel_size",
			GTK_SIGNAL_FUNC(applet_change_pixel_size), NULL);
#endif
	
        gtk_widget_show(icon);
        gtk_widget_show(applet);
        return 0;
}

void set_user_state( enum gaim_user_states state ){
	MRI_user_status = state; 
	update_applet();
}

void applet_set_tooltips(char *msg) {
	applet_widget_set_tooltip(APPLET_WIDGET(applet), msg);
}

#endif /*USE_APPLET*/
