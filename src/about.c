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
#include "pixmaps/about_small.xpm"

static GtkWidget *about = NULL;

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
	open_url_nw(NULL, WEBSITE);
}

char *name()
{
	return PACKAGE;
}

char *description()
{
	return WEBSITE;
}

void show_about(GtkWidget *w, void *null)
{
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *a_table;
	GtkWidget *label;
	GtkWidget *pixmap;
	GtkStyle *style;
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *hbox;
	GtkWidget *button;

	char abouttitle[45];

	if (!about) {

		about = gtk_window_new(GTK_WINDOW_POPUP);

		g_snprintf(abouttitle, sizeof(abouttitle), _("About GAIM v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_window_set_wmclass(GTK_WINDOW(about), "about", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(about), FALSE, TRUE, TRUE);

		gtk_widget_realize(about);
		aol_icon(about->window);

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		gtk_container_add(GTK_CONTAINER(about), vbox);

		frame = gtk_frame_new("Gaim " VERSION);
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		/* Left side, TOP */
		style = gtk_widget_get_style(about);
		pm = gdk_pixmap_create_from_xpm_d(about->window, &bm,
						  &style->bg[GTK_STATE_NORMAL], (gchar **) aol_logo);
		pixmap = gtk_pixmap_new(pm, bm);

		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);

		gtk_box_pack_start(GTK_BOX(fbox), pixmap, FALSE, FALSE, 0);

		/* Set up the author table */
		a_table = gtk_table_new(8, 2, TRUE);
		gtk_table_set_row_spacings(GTK_TABLE(a_table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(a_table), 5);

		label =
		    gtk_label_new(_
				  ("GAIM is a client that supports AOL's Instant Messenger protocol. "
				   "It is written using Gtk+ and is licensed under the GPL.\n"
				   "URL: " WEBSITE));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 2, 0, 2);

		/* Rob */
		label = gtk_label_new("Rob Flynn (Maintainer)");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 2, 3);

		label = gtk_label_new("rob@marko.net");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 1, 2, 2, 3);

		/* Eric */
		label = gtk_label_new("Eric Warmenhoven");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 3, 4);

		label = gtk_label_new("warmenhoven@yahoo.com");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 1, 2, 3, 4);
		
		/* Bmiller */
		label = gtk_label_new("Benjamin Miller");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 4, 5);
		
		/* Decklin */
		label = gtk_label_new("Decklin Foster");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 5, 6);

		/* Jim */
		label = gtk_label_new("Jim Duchek");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 6, 7);

		/* Mark */
		label = gtk_label_new("Mark Spencer");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 0, 1, 7, 8);

		label = gtk_label_new("markster@marko.net");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach_defaults(GTK_TABLE(a_table), label, 1, 2, 7, 8);

		gtk_box_pack_start(GTK_BOX(fbox), a_table, TRUE, TRUE, 0);

		/* Close Button */

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		button = picture_button(about, _("Close"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

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

		/* this makes the sizes not work. */
		//GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		//gtk_widget_grab_default(button);

		button = picture_button(about, _("Web Site"), about_small_xpm);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(about_click), NULL);

		if (display_options & OPT_DISP_COOL_LOOK)
			gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	}

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show_all(about);

}
