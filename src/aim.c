/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef USE_THEMES
#include <gnome.h>
#else
#ifdef USE_APPLET
#include "gnome_applet_mgr.h"
#include <gnome.h>
#endif /* USE_APPLET */
#endif /* USE_THEMES */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "gaim.h"
#ifndef USE_APPLET
#include "pixmaps/logo.xpm"
#endif /* USE_APPLET */

static GtkWidget *name;
static GtkWidget *pass;
static GtkWidget *signon;
static GtkWidget *cancel;
static GtkWidget *progress;
static GtkWidget *notice;

GList *permit = NULL;
GList *deny = NULL;
GList *log_conversations = NULL;
GList *buddy_pounces = NULL;
GList *away_messages = NULL;
GList *groups = NULL;
GList *buddy_chats = NULL;
GList *conversations = NULL;
GList *chat_rooms = NULL;

GtkWidget *mainwindow = NULL;

char toc_addy[16];
char *quad_addr = NULL;


void cancel_logon(void)
{
#ifdef USE_APPLET
        set_applet_draw_closed();
        AppletCancelLogon();
        gtk_widget_hide(mainwindow);
#else
	exit(0);
#endif /* USE_APPLET */
}

void set_login_progress(int howfar, char *whattosay)
{
	gtk_progress_bar_update(GTK_PROGRESS_BAR(progress),
				((float)howfar / (float)LOGIN_STEPS));
	gtk_statusbar_pop(GTK_STATUSBAR(notice), 1);
	gtk_statusbar_push(GTK_STATUSBAR(notice), 1, whattosay);	

        while (gtk_events_pending())
               gtk_main_iteration();

	/* Why do I need these? */
	usleep(10);
	while (gtk_events_pending())
               gtk_main_iteration();
	       
}

void hide_login_progress(char *why)
{
	gtk_progress_bar_update(GTK_PROGRESS_BAR(progress),
				0);
	gtk_statusbar_pop(GTK_STATUSBAR(notice), 1);
	gtk_statusbar_push(GTK_STATUSBAR(notice), 1, why);	

        while (gtk_events_pending())
               gtk_main_iteration();

	/* Why do I need these? */
	usleep(10);
	while (gtk_events_pending())
               gtk_main_iteration();
}

void dologin(GtkWidget *widget, GtkWidget *w)
{
	char *username = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(name)->entry));
        char *password = gtk_entry_get_text(GTK_ENTRY(pass));

        if (query_state() != STATE_OFFLINE)
                return;
        
	if (!strlen(username)) {
		hide_login_progress("Please enter your logon");
		return;
	}
	if (!strlen(password)) {
		hide_login_progress("You must give your password");
		return;
	}

#ifdef USE_APPLET
	set_applet_draw_closed();
	setUserState(signing_on);
#endif /* USE_APPLET */


        if (serv_login(username, password) < 0)
                return;

        return;
}




void doenter(GtkWidget *widget, GtkWidget *w)
{
	if (widget == name) {
		gtk_entry_set_text(GTK_ENTRY(pass),"");
		gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(name)->entry), 0, 0);
		gtk_window_set_focus(GTK_WINDOW(mainwindow), pass);
	} else if (widget == pass) {
		gtk_window_set_focus(GTK_WINDOW(mainwindow), signon);
	} else {
		g_print("what did you press enter on?\n");
	}

}


static void combo_changed(GtkWidget *w, GtkWidget *combo)
{
        char *txt = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(combo)->entry), 0, -1);
        struct aim_user *u;
        
        if (!(general_options & OPT_GEN_REMEMBER_PASS)) {
		g_free(txt);
                return;
        }

        u = find_user(txt);

        if (u != NULL) {
                gtk_entry_set_text(GTK_ENTRY(pass), u->password);
        } else {
                gtk_entry_set_text(GTK_ENTRY(pass), "");
        }
       
	g_free(txt); 
        return;
}

static GList *combo_user_names()
{
        GList *usr = aim_users;
        GList *tmp = NULL;
        struct aim_user *u;

        
        if (!usr)
                return g_list_append(NULL, "<unknown>");
        
        while(usr) {
                u = (struct aim_user *)usr->data;
                tmp = g_list_append(tmp, g_strdup(u->username));
                usr = usr->next;

        }

        return tmp;
}



void show_login()
{
	GtkWidget *options;
	GtkWidget *reg;
	GtkWidget *bbox;
	GtkWidget *sbox;
	GtkWidget *label;
	GtkWidget *table;
        GtkWidget *remember;

#ifndef USE_APPLET
	GtkWidget *pmw;
	GdkPixmap *pm;
	GtkStyle *style;
	GdkBitmap *mask;
#endif /* USE_APPLET */

        if (mainwindow) {
                gtk_widget_show(mainwindow);
                return;
        }
        
	mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        signon   = gtk_button_new_with_label("Signon");
	cancel   = gtk_button_new_with_label("Cancel");
	reg      = gtk_button_new_with_label("Register");
	options  = gtk_button_new_with_label("Options");
	table    = gtk_table_new(8, 2, FALSE);
	name     = gtk_combo_new();
	pass     = gtk_entry_new();
	notice   = gtk_statusbar_new();
        progress = gtk_progress_bar_new();

        gtk_combo_set_popdown_strings(GTK_COMBO(name), combo_user_names());

	/* Make the buttons do stuff */
	/* Clicking the button initiates a login */
	gtk_signal_connect(GTK_OBJECT(signon), "clicked",
			   GTK_SIGNAL_FUNC(dologin), mainwindow);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
	/* Allow user to change prefs before logging in */
	gtk_signal_connect(GTK_OBJECT(options), "clicked",
			   GTK_SIGNAL_FUNC(show_prefs), NULL);

	/* Register opens the right URL */
	gtk_signal_connect(GTK_OBJECT(reg), "clicked",
			   GTK_SIGNAL_FUNC(open_url), "http://www.aol.com/aim");
	/* Enter in the username clears the password and sets
	   the pointer in the password field */
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "activate",
                           GTK_SIGNAL_FUNC(doenter), mainwindow);

        gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "changed",
                           GTK_SIGNAL_FUNC(combo_changed), name);
			   
	gtk_signal_connect(GTK_OBJECT(pass), "activate",
			   GTK_SIGNAL_FUNC(doenter), mainwindow);
	gtk_signal_connect(GTK_OBJECT(mainwindow), "delete_event",
			   GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
	/* Homogenous spacing, 10 padding */
	bbox = gtk_hbox_new(TRUE, 10);
	sbox = gtk_vbox_new(TRUE, 10);
	
	gtk_box_pack_start(GTK_BOX(bbox), reg, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), signon, TRUE, TRUE, 0);
	
	gtk_box_pack_start(GTK_BOX(sbox), bbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sbox), options, TRUE, TRUE, 0);

	/* Labels for selectors and text boxes */
#if 0	
	label = gtk_label_new("TOC: ");
	gtk_table_attach(GTK_TABLE(table), label, 0,1,1,2,0,0, 5, 5);
	gtk_widget_show(label);
#endif
	label = gtk_label_new("Screen Name: ");
	gtk_table_attach(GTK_TABLE(table), label, 0,1,2,3,0,0, 5, 5);
	gtk_widget_show(label);
	label = gtk_label_new("Password: ");
	gtk_table_attach(GTK_TABLE(table), label, 0,1,3,4,0,0, 5, 5);
	gtk_widget_show(label);
	remember = gtk_check_button_new_with_label("Remember Password");
	gtk_table_attach(GTK_TABLE(table), remember, 0,2,4,5,0,0, 5, 5);
	gtk_widget_show(remember);

	gtk_widget_show(options);
	
        /* Adjust sizes of inputs */
	gtk_widget_set_usize(name,95,0);
	gtk_widget_set_usize(pass,95,0);


	/* Status and label */
	gtk_widget_show(notice);

	gtk_widget_set_usize(progress,150,0);

        gtk_widget_show(progress);

        gtk_table_attach(GTK_TABLE(table), progress, 0, 2, 6, 7, 0, 0, 5, 5);
	gtk_widget_set_usize(GTK_STATUSBAR(notice)->label, 150, 0);
	gtk_table_attach(GTK_TABLE(table), notice, 0, 2, 8, 9, 0, 0, 5, 5);

	/* Attach the buttons at the bottom */
	gtk_widget_show(signon);
	gtk_widget_show(cancel);
	gtk_widget_show(reg);
	gtk_widget_show(bbox);
	gtk_widget_show(sbox);
	gtk_table_attach(GTK_TABLE(table), sbox, 0,2,7,8,0,0, 5, 5);
	
	/* Text fields */
	
	gtk_table_attach(GTK_TABLE(table),name,1,2,2,3,0,0,5,5);
	gtk_widget_show(name);
	gtk_table_attach(GTK_TABLE(table),pass,1,2,3,4,0,0,5,5);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_show(pass);
	
	gtk_container_border_width(GTK_CONTAINER(sbox), 10);	 
	
	gtk_container_add(GTK_CONTAINER(mainwindow),table );
	
	gtk_widget_show(table);
	gtk_window_set_title(GTK_WINDOW(mainwindow),"Gaim - Login");


        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(remember), (general_options & OPT_GEN_REMEMBER_PASS));
        
	if (current_user) {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(name)->entry), current_user->username);
                if ((general_options & OPT_GEN_REMEMBER_PASS)) {
                        gtk_entry_set_text(GTK_ENTRY(pass), current_user->password);
                }


		gtk_widget_grab_focus(signon);
	} else
		gtk_widget_grab_focus(name);



	gtk_signal_connect(GTK_OBJECT(remember), "clicked", GTK_SIGNAL_FUNC(set_general_option), (int *)OPT_GEN_REMEMBER_PASS);


        gtk_widget_realize(mainwindow);

#ifndef USE_APPLET
	/* Logo at the top */
	style = gtk_widget_get_style(mainwindow);
	pm = gdk_pixmap_create_from_xpm_d(mainwindow->window, &mask,
		&style->bg[GTK_STATE_NORMAL], (gchar **)aol_logo);
	pmw = gtk_pixmap_new( pm, mask);
	gtk_table_attach(GTK_TABLE(table), pmw, 0,2,0,1,0,0,5,5);
	gtk_widget_show(pmw);
#endif /* USE_APPLET */

        
        aol_icon(mainwindow->window);
#ifndef _WIN32
        gdk_window_set_group(mainwindow->window, mainwindow->window);
#endif

        
        gtk_widget_show(mainwindow);
	
        if((general_options & OPT_GEN_AUTO_LOGIN) &&
           (general_options & OPT_GEN_REMEMBER_PASS)) {
		dologin(signon, NULL);
	}
}


int main(int argc, char *argv[])
{
#ifdef USE_APPLET
        InitAppletMgr( argc, argv );
#elif defined USE_THEMES         
        gnome_init("GAIM",NULL,argc,argv);
#else
        gtk_init(&argc, &argv);
#endif /* USE_THEMES */


        set_defaults();
        load_prefs();

#ifdef USE_APPLET
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"prefs",
					_("Preferences"),
					show_prefs,
					NULL);
        applet_widget_register_callback(APPLET_WIDGET(applet),
					"signon",
					_("Signon"),
					applet_show_login,
					NULL);

        if((general_options & OPT_GEN_AUTO_LOGIN) &&
           (general_options & OPT_GEN_REMEMBER_PASS)) {

                applet_show_login(APPLET_WIDGET(applet), NULL);
        }

	update_pixmaps();
	
	applet_widget_gtk_main();
#else
        

        show_login();
        gtk_main();
        
#endif /* USE_APPLET */
        
	return 0;
	
}
