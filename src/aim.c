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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#ifdef USE_APPLET
#include "gnome_applet_mgr.h"
#include <gnome.h>
#else
#ifdef USE_GNOME
#include <gnome.h>
#endif /* USE_GNOME */
#endif /* USE_APPLET */
#ifdef GAIM_PLUGINS
#include <dlfcn.h>
#endif /* GAIM_PLUGINS */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "gaim.h"
#ifndef USE_APPLET
#include "pixmaps/logo.xpm"
#endif /* USE_APPLET */
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#include "locale.h"
#include "gtkticker.h"

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
GtkWidget *remember = NULL;

void BuddyTickerCreateWindow( void );

char toc_addy[16];
char *quad_addr = NULL;

gboolean running = FALSE; /* whether or not we're currently trying to sign on */

void cancel_logon(void)
{
#ifdef USE_APPLET
	applet_buddy_show = FALSE;
	signoff();
	running = FALSE;
        gtk_widget_hide(mainwindow);
#else
#ifdef GAIM_PLUGINS
	GList *c;
	struct gaim_plugin *p;
	void (*gaim_plugin_remove)();
	char *error;

	/* first we tell those who have requested it we're quitting */
	plugin_event(event_quit, 0, 0, 0);

	/* then we remove everyone in a mass suicide */
	c = plugins;
	while (c) {
		p = (struct gaim_plugin *)c->data;
		gaim_plugin_remove = dlsym(p->handle, "gaim_plugin_remove");
		if ((error = (char *)dlerror()) == NULL)
			(*gaim_plugin_remove)();
		/* we don't need to worry about removing callbacks since
		 * there won't be any more chance to call them back :) */
		dlclose(p->handle);
		g_free(p->filename); /* why do i bother? */
		g_free(p);
		c = c->next;
	}
#endif /* GAIM_PLUGINS */
#ifdef USE_PERL
	perl_end();
#endif

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

static int snd_tmout;
int logins_not_muted = 1;
static void sound_timeout() {
	logins_not_muted = 1;
	gtk_timeout_remove(snd_tmout);
}

char g_screenname[ 64 ];	/* gotta be enough */

void dologin(GtkWidget *widget, GtkWidget *w)
{
	char *username = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(name)->entry));
        char *password = gtk_entry_get_text(GTK_ENTRY(pass));
	int i;

        if (query_state() != STATE_OFFLINE)
                return;
        
	if (!strlen(username)) {
		hide_login_progress(_("Please enter your logon"));
		return;
	}
	if (!strlen(password)) {
		hide_login_progress(_("You must give your password"));
		return;
	}

	/* save screenname away for cache file use */

	strcpy( g_screenname, username );

	/* fold cache screen name file to upper case to avoid problems
	   finding file later if user uses different case at login time */

	for ( i = 0; i < strlen( g_screenname ); i++ )
		g_screenname[i] = toupper( g_screenname[i] );

#ifdef USE_APPLET
	set_user_state(signing_on);
#endif /* USE_APPLET */

	if (running) return;
	running = TRUE;

        if (serv_login(username, password) < 0) {
		running = FALSE;
                return;
	}

	if (!USE_OSCAR) /* serv_login will set up USE_OSCAR */
		gaim_setup();
}

void auth_failed() {
	running = FALSE;
}

/* we need to do this for Oscar because serv_login only starts the login
 * process, it doesn't end there. gaim_setup will be called later from
 * oscar.c, after the buddy list is made and serv_finish_login is called */
void gaim_setup() {
	if (sound_options & OPT_SOUND_LOGIN &&
		sound_options & OPT_SOUND_SILENT_SIGNON) {
		logins_not_muted = 0;
		snd_tmout = gtk_timeout_add(10000, (GtkFunction)sound_timeout,
				NULL);
	}

#ifdef USE_APPLET
	set_user_state(online);
	applet_widget_unregister_callback(APPLET_WIDGET(applet),"signon");
	applet_widget_register_callback(APPLET_WIDGET(applet),
			"signoff",
			_("Signoff"),
			signoff,
			NULL);
#endif /* USE_APPLET */

	plugin_event(event_signon, 0, 0, 0);

	 running = FALSE;
	 return;
}




void doenter(GtkWidget *widget, GtkWidget *w)
{
	if (widget == name) {
		gtk_entry_set_text(GTK_ENTRY(pass),"");
		gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(name)->entry), 0, 0);
		gtk_widget_grab_focus(pass);
	} else if (widget == pass) {
		dologin(widget, w);
	}

}


static void combo_changed(GtkWidget *w, GtkWidget *combo)
{
        char *txt = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
        struct aim_user *u;
        
        if (!(general_options & OPT_GEN_REMEMBER_PASS)) {
                return;
        }

        u = find_user(txt);

        if (u != NULL) {
                gtk_entry_set_text(GTK_ENTRY(pass), u->password);
        } else {
                gtk_entry_set_text(GTK_ENTRY(pass), "");
        }
       
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
#ifdef GAIM_PLUGINS
	GtkWidget *plugs;
#endif
	GtkWidget *reg;
	GtkWidget *bbox;
	GtkWidget *hbox;
	GtkWidget *sbox;
	GtkWidget *label;
	GtkWidget *table;

#ifndef USE_APPLET
	GtkWidget *pmw;
	GdkPixmap *pm;
	GtkStyle *style;
	GdkBitmap *mask;
#endif /* USE_APPLET */

        if (mainwindow) {
                gtk_widget_show(mainwindow);
		if (!(general_options & OPT_GEN_REMEMBER_PASS))
			gtk_entry_set_text(GTK_ENTRY(pass), "");
                return;
        }
       
	mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(mainwindow);
	signon   = gtk_button_new_with_label(_("Signon"));
	cancel   = gtk_button_new_with_label(_("Cancel"));
	reg      = gtk_button_new_with_label(_("Register"));
	options  = gtk_button_new_with_label(_("Options"));
#ifdef GAIM_PLUGINS
	plugs    = gtk_button_new_with_label(_("Plugins")); 
#endif
	table    = gtk_table_new(8, 2, FALSE);
	name     = gtk_combo_new();
	pass     = gtk_entry_new();
	notice   = gtk_statusbar_new();
	progress = gtk_progress_bar_new();

	gtk_combo_set_popdown_strings(GTK_COMBO(name), combo_user_names());

	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(signon), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(reg), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(options), GTK_RELIEF_NONE);
#ifdef GAIM_PLUGINS
		gtk_button_set_relief(GTK_BUTTON(plugs), GTK_RELIEF_NONE);
#endif
	}

	/* Make the buttons do stuff */
	/* Clicking the button initiates a login */
	gtk_signal_connect(GTK_OBJECT(signon), "clicked",
			   GTK_SIGNAL_FUNC(dologin), mainwindow);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
	/* Allow user to change prefs before logging in */
	gtk_signal_connect(GTK_OBJECT(options), "clicked",
			   GTK_SIGNAL_FUNC(show_prefs), NULL);
#ifdef GAIM_PLUGINS
	/* Allow user to control plugins before logging in */
	gtk_signal_connect(GTK_OBJECT(plugs), "clicked",
			   GTK_SIGNAL_FUNC(show_plugins), NULL);
#endif

	/* Register opens the right URL */
	gtk_signal_connect(GTK_OBJECT(reg), "clicked",
			   GTK_SIGNAL_FUNC(open_url), "http://aim.aol.com/aimnew/Aim/register.adp?promo=106723&pageset=Aim&client=no");
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
	hbox = gtk_hbox_new(TRUE, 10);
	sbox = gtk_vbox_new(TRUE, 5);
	
	gtk_box_pack_start(GTK_BOX(bbox), reg, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), signon, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), options, TRUE, TRUE, 0);
#ifdef GAIM_PLUGINS
	gtk_box_pack_start(GTK_BOX(hbox), plugs, TRUE, TRUE, 0);
#endif

	gtk_box_pack_start(GTK_BOX(sbox), bbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sbox), hbox, TRUE, TRUE, 0);

	/* Labels for selectors and text boxes */
#if 0	
	label = gtk_label_new("TOC: ");
	gtk_table_attach(GTK_TABLE(table), label, 0,1,1,2,0,0, 5, 5);
	gtk_widget_show(label);
#endif
	label = gtk_label_new(_("Screen Name: "));
	gtk_table_attach(GTK_TABLE(table), label, 0,1,2,3,0,0, 5, 5);
	gtk_widget_show(label);
	label = gtk_label_new(_("Password: "));
	gtk_table_attach(GTK_TABLE(table), label, 0,1,3,4,0,0, 5, 5);
	gtk_widget_show(label);
	remember = gtk_check_button_new_with_label(_("Remember Password"));
	gtk_table_attach(GTK_TABLE(table), remember, 0,2,4,5,0,0, 5, 5);
	gtk_widget_show(remember);

	gtk_widget_show(options);
#ifdef GAIM_PLUGINS
	gtk_widget_show(plugs);
#endif
	
        /* Adjust sizes of inputs */
	gtk_widget_set_usize(name,100,0);
	gtk_widget_set_usize(pass,100,0);


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
	gtk_widget_show(hbox);
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
	gtk_window_set_title(GTK_WINDOW(mainwindow),_("Gaim - Login"));


        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(remember), (general_options & OPT_GEN_REMEMBER_PASS));

	if (current_user) {
		sprintf(debug_buff, "Current user is %s\n", current_user->username);
		debug_print(debug_buff);
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(name)->entry), current_user->username);
		if ((general_options & OPT_GEN_REMEMBER_PASS)) {
			combo_changed(NULL, name);
			gtk_widget_grab_focus(signon);
		} else {
			gtk_widget_grab_focus(pass);
		}
	} else {
		gtk_widget_grab_focus(name);
	}


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
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);
#endif /* USE_APPLET */

        
        aol_icon(mainwindow->window);
#ifndef _WIN32
        gdk_window_set_group(mainwindow->window, mainwindow->window);
#endif

        
        gtk_widget_show(mainwindow);

	SetTickerPrefs();
	
        if((general_options & OPT_GEN_AUTO_LOGIN) &&
           (general_options & OPT_GEN_REMEMBER_PASS)) {
		dologin(signon, NULL);
	}
}

extern void show_debug(GtkObject *);

#if HAVE_SIGNAL_H
void sighandler(int sig)
{
	fprintf(stderr, "God damn, I tripped.\n");
	exit(11); /* signal 11 */
}
#endif

int main(int argc, char *argv[])
{
#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

#if HAVE_SIGNAL_H
	/* Let's not violate any PLA's!!!! */
	/* signal(SIGSEGV, sighandler); */
#endif

	if (argc > 1 && !strcmp(argv[1], "--version")) {
		gtk_init(&argc, &argv);
		set_defaults(); /* needed for open_url_nw */
		load_prefs();
		show_about(0, (void *)1);
		gtk_main();
		return 0;
	}

#ifdef USE_APPLET
        init_applet_mgr(argc, argv);
#elif defined USE_GNOME
        gnome_init(PACKAGE,VERSION,argc,argv);
#else
        gtk_init(&argc, &argv);
#endif /* USE_GNOME */

        set_defaults();
        load_prefs();

	if (general_options & OPT_GEN_DEBUG)
		show_debug(NULL);

	gdk_threads_enter();

#ifdef USE_PERL
	perl_init();
	perl_autoload();
#endif

#ifdef USE_APPLET
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"prefs",
					_("Preferences"),
					show_prefs,
					NULL);
        applet_widget_register_callback(APPLET_WIDGET(applet),
					"signon",
					_("Signon"),
					applet_do_signon,
					NULL);
#ifdef GAIM_PLUGINS
        applet_widget_register_callback(APPLET_WIDGET(applet),
					"plugins",
					_("Plugins"),
					GTK_SIGNAL_FUNC(show_plugins),
					NULL);
#endif /* GAIM_PLUGINS */

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
	gdk_threads_leave();
        
	return 0;
	
}
