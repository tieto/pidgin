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
#include <config.h>
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
#include "pixmaps/panic.xpm"

static GtkWidget *about = NULL;
static GtkWidget *help = NULL;

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
	open_url(NULL, WEBSITE);
}

char *name()
{
	return PACKAGE;
}

char *description()
{
	return WEBSITE;
}

char *version()
{
	return VERSION;
}

void show_about(GtkWidget *w, void *null)
{
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *label;
	GtkWidget *pixmap;
	GtkStyle *style;
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *text;

	char abouttitle[45];

	if (!about) {

		GAIM_DIALOG(about);
		gtk_widget_set_usize(GTK_WIDGET(about), 450, -1);
		g_snprintf(abouttitle, sizeof(abouttitle), _("About Gaim v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_window_set_wmclass(GTK_WINDOW(about), "about", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(about), FALSE, TRUE, TRUE);
		gtk_widget_realize(about);
		aol_icon(about->window);

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		gtk_container_add(GTK_CONTAINER(about), vbox);
		gtk_widget_show(vbox);

		frame = gtk_frame_new("Gaim v" VERSION);
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show(frame);

		fbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);
		gtk_widget_show(fbox);

		/* Left side, TOP */
		style = gtk_widget_get_style(about);
		pm = gdk_pixmap_create_from_xpm_d(about->window, &bm,
						  &style->bg[GTK_STATE_NORMAL], (gchar **)gaim_logo_xpm);
		pixmap = gtk_pixmap_new(pm, bm);
		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);
		gtk_box_pack_start(GTK_BOX(fbox), pixmap, FALSE, FALSE, 0);
		gtk_widget_show(pixmap);

		label =
		    gtk_label_new(
				  _("Gaim is a client that supports AOL's Instant Messenger protocol. It is written\n"
				   "using Gtk+ and is licensed under the GPL.\n\n" "URL: " WEBSITE "\n\n"
				   "IRC: #gaim on irc.openprojects.net"));

		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_box_pack_start(GTK_BOX(fbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		text = gtk_text_new(NULL, NULL);
		gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
				_("Rob Flynn (maintainer)               rob@marko.net\nEric Warmenhoven (lead coder)  warmenhoven@yahoo.com\n\nBenjamin Miller\nDecklin Foster\nSean Egan\nJim Duchek\nMark Spencer (original author)   markster@marko.net"),
				208);
		gtk_box_pack_start(GTK_BOX(fbox), text, TRUE, TRUE, 0);
		gtk_widget_show(text);

		/* Close Button */

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

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
		/* GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT); */
		/* gtk_widget_grab_default(button); */

		button = picture_button(about, _("Web Site"), about_small_xpm);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(about_click), NULL);
	}

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show(about);

	gdk_window_raise(about->window);
}

static void destroy_help()
{
	if (help)
		gtk_widget_destroy(help);
	help = NULL;
}

void gaim_help(GtkWidget *w, void *x)
{
	GtkWidget *vbox;
	GdkPixmap *pm;
	GdkBitmap *bm;
	GtkWidget *pix;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *close;

	if (!help) {

		help = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(help), "Gaim Help");
		gtk_window_set_wmclass(GTK_WINDOW(help), "help", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(help), FALSE, TRUE, TRUE);
		gtk_widget_realize(help);
		aol_icon(help->window);
		gtk_signal_connect(GTK_OBJECT(help), "destroy",
				   GTK_SIGNAL_FUNC(destroy_help), GTK_OBJECT(help));
		gtk_widget_set_usize(help, 315, -1);

		vbox = gtk_vbox_new(FALSE, 10);
		gtk_container_add(GTK_CONTAINER(help), vbox);

		hbox = gtk_hbox_new(FALSE, 10);
		gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

		close = picture_button(help, _("Close"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 5);
		gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(destroy_help), NULL);

		pm = gdk_pixmap_create_from_xpm_d(help->window, &bm, NULL, panic_xpm);
		pix = gtk_pixmap_new(pm, bm);
		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(bm);
		gtk_box_pack_start(GTK_BOX(vbox), pix, FALSE, FALSE, 5);

		label = gtk_label_new(_("Relax, help is just around the corner. "
					"The first thing you'll need to do is get an AIM "
					"account; you can get one from http://aim.aol.com/. "
					"Just click the button that says \"New Users\" and "
					"you can create an account that way. Once you have "
					"your account, enter the username and password into "
					"the login window that comes up when you start Gaim, "
					"and click the Signon button. Once you're online, "
					"you can talk to one of the Gaim developers for more "
					"assistance; their contact information is in the "
					"AUTHORS file in the Gaim source, or at "
					WEBSITE "contactinfo.php. If you can't get online and "
					"still need more assistance, feel free to email us "
					"at gaim@marko.net. Thanks for using Gaim!"));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_box_pack_end(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	}

	gtk_widget_show_all(help);
}
