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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "pixmaps/logo.xpm"
#include "pixmaps/cancel.xpm"

static GtkWidget *about=NULL;

static void destroy_about()
{
	if (about)
		gtk_widget_destroy(about);
	about = NULL;
}


static void version_exit()
{
	gtk_main_quit();
}


static void about_click(GtkWidget *w, gpointer m)
{
	open_url_nw(NULL, "http://www.marko.net/gaim/");
}

void show_about(GtkWidget *w, void *null)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *a_table;
	GtkWidget *label;
	GtkWidget *pixmap;
	GtkStyle *style;
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *hbox;
	GtkWidget *eventbox;
	GtkWidget *button;

	char abouttitle[45];
	
	if (!about) {
	
		about = gtk_window_new(GTK_WINDOW_DIALOG);

		g_snprintf(abouttitle, sizeof(abouttitle), _("About GAIM v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
                gtk_window_set_wmclass(GTK_WINDOW(about), "about", "Gaim" );
		gtk_container_border_width(GTK_CONTAINER(about), 2);
		gtk_widget_set_usize(about, 535, 250);
		gtk_window_set_policy(GTK_WINDOW(about), FALSE, FALSE, TRUE);

		gtk_widget_show(about);
        	aol_icon(about->window);


		vbox = gtk_vbox_new(FALSE, 5);
		
		table = gtk_table_new(3, 2, FALSE);

		/* Left side, TOP */
		style = gtk_widget_get_style(about);
		pm = gdk_pixmap_create_from_xpm_d(about->window, &bm, 
			&style->bg[GTK_STATE_NORMAL], (gchar **)aol_logo);
		pixmap = gtk_pixmap_new(pm, bm);

		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);

		gtk_table_attach(GTK_TABLE(table), pixmap, 0, 1, 0, 1, 0, 0, 5, 5);
		gtk_widget_show(pixmap);

		
		/* Right side, TOP*/
		hbox = gtk_vbox_new(FALSE, 5);

		label = gtk_label_new(_("GAIM is a client that supports AOL's Instant Messanger protocol.  It is " 
				"written using Gtk+ and is licensed under the GPL."));
		gtk_widget_show(label);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, GTK_EXPAND, FALSE, 0);

		/* Set up the author table */
		a_table = gtk_table_new(2, 5, FALSE);
		
		/* Rob */
		label = gtk_label_new("Rob Flynn (Maintainer)");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 0, 1, 1, 2, GTK_FILL, 0, 5, 5);
		
		label = gtk_label_new("rob@tgflinux.com");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 1, 2, 1, 2, GTK_FILL, 0, 5, 5);

		/* Eric */
		label = gtk_label_new("Eric Warmenhoven");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 0, 1, 2, 3, GTK_FILL, 0, 5, 5);
		
		label = gtk_label_new("warmenhoven@yahoo.com");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 1, 2, 2, 3, GTK_FILL, 0, 5, 5);
		
		/* Jim */
		label = gtk_label_new("Jim Duchek");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 0, 1, 3, 4, GTK_FILL, 0, 5, 5);
		
		/* Mark */
		label = gtk_label_new("Mark Spencer");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 0, 1, 4, 5, GTK_FILL, 0, 5, 5);
		
		label = gtk_label_new("markster@marko.net");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(a_table), label, 1, 2, 4, 5, GTK_FILL, 0, 5, 5);
		
		gtk_box_pack_start(GTK_BOX(hbox), a_table, TRUE, FALSE, 0);	

		gtk_widget_show_all(a_table);

		/* End Author List */

		gtk_table_attach(GTK_TABLE(table), hbox, 1, 3, 0, 1, 0, 0, 5, 5);
		gtk_widget_show(hbox);

		/* Clickable URL */
		eventbox = gtk_event_box_new();
		gtk_table_attach(GTK_TABLE(table), eventbox, 0, 3, 1, 2, GTK_FILL, FALSE, 5, 5);
		gtk_widget_show(eventbox);
		
		label = gtk_label_new("Gaim " VERSION " - http://www.marko.net/gaim/\n");
		gtk_container_add(GTK_CONTAINER(eventbox), label);

		gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",
				   GTK_SIGNAL_FUNC(about_click), NULL);
		gdk_window_set_cursor(eventbox->window, gdk_cursor_new(GDK_HAND2));
		gtk_widget_show(label);

		/* End Clickable URL */

		/* Close Button */

		button = picture_button(about, _("Close"), cancel_xpm);

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		/* End Button */

		gtk_widget_show(vbox);
		gtk_widget_show(table);


		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

		gtk_container_add(GTK_CONTAINER(about), vbox);

		if (null != (void *)2) {
			/* 2 can be as sad as 1, it's the loneliest number since the number 1 */ 
			gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
        	                                  GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(about));
			gtk_signal_connect(GTK_OBJECT(about), "destroy",
					   GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(about));
		} else {
			gtk_signal_connect(GTK_OBJECT(button), "clicked",
					   GTK_SIGNAL_FUNC(version_exit), NULL);
			gtk_signal_connect(GTK_OBJECT(about), "destroy",
					   GTK_SIGNAL_FUNC(version_exit), NULL);
		}
 		
 		if (display_options & OPT_DISP_COOL_LOOK)
 			gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
 			
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		gtk_widget_grab_default(button);


	}
	else
		/* Let's give'em something to talk about -- woah woah woah */
		gtk_widget_show(about);
	
}

