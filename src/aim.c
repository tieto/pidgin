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
#include "pixmaps/logo.xpm"
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#include "locale.h"
#include "gtkticker.h"

GList *permit = NULL;
GList *deny = NULL;
GList *log_conversations = NULL;
GList *buddy_pounces = NULL;
GSList *away_messages = NULL;
GSList *groups = NULL;
GList *conversations = NULL;
GList *chat_rooms = NULL;

GtkWidget *mainwindow = NULL;

void BuddyTickerCreateWindow( void );

char toc_addy[16];
char *quad_addr = NULL;

void cancel_logon(void)
{
#ifdef USE_APPLET
	applet_buddy_show = FALSE;
	if (mainwindow)
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
	/* FIXME: we should do this on a per-connection basis
	gtk_progress_bar_update(GTK_PROGRESS_BAR(progress),
				((float)howfar / (float)LOGIN_STEPS));
	gtk_statusbar_pop(GTK_STATUSBAR(notice), 1);
	gtk_statusbar_push(GTK_STATUSBAR(notice), 1, whattosay);	

        while (gtk_events_pending())
               gtk_main_iteration();
	*/
}

void hide_login_progress(char *why)
{
	/* FIXME: we should do this on a per-connection basis
	gtk_progress_bar_update(GTK_PROGRESS_BAR(progress),
				0);
	gtk_statusbar_pop(GTK_STATUSBAR(notice), 1);
	gtk_statusbar_push(GTK_STATUSBAR(notice), 1, why);	

        while (gtk_events_pending())
               gtk_main_iteration();
	*/
}

static int snd_tmout;
int logins_not_muted = 1;
static void sound_timeout() {
	logins_not_muted = 1;
	gtk_timeout_remove(snd_tmout);
}

/* we need to do this for Oscar because serv_login only starts the login
 * process, it doesn't end there. gaim_setup will be called later from
 * oscar.c, after the buddy list is made and serv_finish_login is called */
void gaim_setup(struct gaim_connection *gc) {
	if (sound_options & OPT_SOUND_LOGIN &&
		sound_options & OPT_SOUND_SILENT_SIGNON) {
		logins_not_muted = 0;
		snd_tmout = gtk_timeout_add(10000, (GtkFunction)sound_timeout,
				NULL);
	}

#ifdef USE_APPLET
	set_user_state(online);
	applet_widget_register_callback(APPLET_WIDGET(applet),
			"signoff",
			_("Signoff"),
			(AppletCallbackFunc)signoff_all,
			NULL);
#endif /* USE_APPLET */

	account_online(gc);

	plugin_event(event_signon, 0, 0, 0);

	 return;
}




void show_login()
{
	GtkWidget *options;
#ifdef GAIM_PLUGINS
	GtkWidget *plugs;
#endif
	GtkWidget *accts;
	GtkWidget *cancel;
	GtkWidget *reg;
	GtkWidget *bbox;
	GtkWidget *hbox;
	GtkWidget *sbox;
	GtkWidget *table;

	GtkWidget *pmw;
	GdkPixmap *pm;
	GtkStyle *style;
	GdkBitmap *mask;

        if (mainwindow) {
                gtk_widget_show(mainwindow);
                return;
        }
       
	mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        /* Set the WM name and class */
        gtk_window_set_wmclass(GTK_WINDOW(mainwindow), "login",
                               "Gaim");
        /* Disallow resizing */
        gtk_window_set_policy(GTK_WINDOW(mainwindow), FALSE, FALSE, TRUE);
	gtk_widget_realize(mainwindow);
	accts    = gtk_button_new_with_label(_("Accounts"));
	cancel   = gtk_button_new_with_label(_("Cancel"));
	reg      = gtk_button_new_with_label(_("Register"));
	options  = gtk_button_new_with_label(_("Options"));
#ifdef GAIM_PLUGINS
	plugs    = gtk_button_new_with_label(_("Plugins")); 
#endif
	table    = gtk_table_new(8, 2, FALSE);

	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(accts), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(reg), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(options), GTK_RELIEF_NONE);
#ifdef GAIM_PLUGINS
		gtk_button_set_relief(GTK_BUTTON(plugs), GTK_RELIEF_NONE);
#endif
	}

	/* Make the buttons do stuff */
	/* Clicking the button initiates a login */
	gtk_signal_connect(GTK_OBJECT(accts), "clicked",
			   GTK_SIGNAL_FUNC(account_editor), mainwindow);
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
	gtk_signal_connect(GTK_OBJECT(mainwindow), "delete_event",
			   GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
	/* Homogenous spacing, 10 padding */
	bbox = gtk_hbox_new(TRUE, 10);
	hbox = gtk_hbox_new(TRUE, 10);
	sbox = gtk_vbox_new(TRUE, 5);
	
	gtk_box_pack_start(GTK_BOX(bbox), reg, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), options, TRUE, TRUE, 0);
#ifdef GAIM_PLUGINS
	gtk_box_pack_start(GTK_BOX(hbox), plugs, TRUE, TRUE, 0);
#endif

	gtk_box_pack_start(GTK_BOX(sbox), hbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sbox), bbox, TRUE, TRUE, 0);

	/* Labels for selectors and text boxes */

	gtk_widget_show(options);
#ifdef GAIM_PLUGINS
	gtk_widget_show(plugs);
#endif
	
	/* Attach the buttons at the bottom */
	gtk_widget_show(cancel);
	gtk_widget_show(reg);
	gtk_widget_show(accts);
	gtk_widget_show(bbox);
	gtk_widget_show(hbox);
	gtk_widget_show(sbox);
	gtk_table_attach(GTK_TABLE(table), accts, 0,2,6,7,0,0, 5, 5);
	gtk_table_attach(GTK_TABLE(table), sbox, 0,2,7,8,0,0, 5, 5);
	
	/* Text fields */
	
	gtk_container_border_width(GTK_CONTAINER(sbox), 10);	 
	
	gtk_container_add(GTK_CONTAINER(mainwindow),table );
	
	gtk_widget_show(table);
        gtk_window_set_title(GTK_WINDOW(mainwindow),_("Gaim - Login"));


        gtk_widget_realize(mainwindow);

	/* Logo at the top */
	style = gtk_widget_get_style(mainwindow);
	pm = gdk_pixmap_create_from_xpm_d(mainwindow->window, &mask,
		&style->bg[GTK_STATE_NORMAL], (gchar **)aol_logo);
	pmw = gtk_pixmap_new( pm, mask);
	gtk_table_attach(GTK_TABLE(table), pmw, 0,2,0,1,0,0,5,5);
	gtk_widget_show(pmw);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);

        
        aol_icon(mainwindow->window);
#ifndef _WIN32
        gdk_window_set_group(mainwindow->window, mainwindow->window);
#endif

        
        gtk_widget_show(mainwindow);

	SetTickerPrefs();
	
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
		set_defaults(FALSE); /* needed for open_url_nw */
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

        set_defaults(FALSE);
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
					"accounts",
					_("Accounts"),
					(AppletCallbackFunc)account_editor,
					NULL);
#ifdef GAIM_PLUGINS
        applet_widget_register_callback(APPLET_WIDGET(applet),
					"plugins",
					_("Plugins"),
					GTK_SIGNAL_FUNC(show_plugins),
					NULL);
#endif /* GAIM_PLUGINS */

	update_pixmaps();
	
	applet_widget_gtk_main();
#else
        

        show_login();
	auto_login();
        gtk_main();
        
#endif /* USE_APPLET */
	gdk_threads_leave();
        
	return 0;
	
}
