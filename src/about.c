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
	GtkWidget *view;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextTag *tag;
	GtkWidget *sw;

	char abouttitle[45];

	if (!about) {

		GAIM_DIALOG(about);
		gtk_window_set_default_size(GTK_WINDOW(about), 450, -1);
		g_snprintf(abouttitle, sizeof(abouttitle), _("About Gaim v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_window_set_wmclass(GTK_WINDOW(about), "about", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(about), FALSE, TRUE, TRUE);
		gtk_widget_realize(about);

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

		view = gtk_text_view_new ();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
		gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		gtk_text_buffer_set_text(buffer, "", -1);
		gtk_text_buffer_get_start_iter(buffer, &iter);

		tag = gtk_text_buffer_create_tag(buffer, "title", "weight", PANGO_WEIGHT_BOLD, "scale", PANGO_SCALE_LARGE, NULL);
		tag = gtk_text_buffer_create_tag(buffer, "email", "scale", PANGO_SCALE_SMALL, "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL);
		tag = gtk_text_buffer_create_tag(buffer, "url", "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL);

		gtk_text_buffer_insert(buffer, &iter,
				  _("Gaim is a modular Instant Messaging client capable of using AIM, ICQ,\n"
				   "Yahoo!, MSN, IRC, Jabber, Napster, Zephyr, and Gadu-Gadu all at once.\n"
				    "It is written using Gtk+ and is licensed under the GPL.\n\n"), -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "URL: ", -1, "title", NULL);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, WEBSITE "\n\n", -1, "url", NULL);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "IRC: ", -1, "title", NULL);
		gtk_text_buffer_insert(buffer, &iter, "IRC: #gaim on irc.freenode.net\n", -1);

		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, _("\nActive Developers:\n"), -1, "title", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Rob Flynn (maintainer) ", -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "rob@marko.net\n", -1, "email", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Sean Egan (coder) ",-1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "bj91704@binghamton.edu\n", -1, "email", NULL);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, _("\nCrazy Patch Writers:\n"), -1, "title", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Benjamin Miller\n", -1);
		gtk_text_buffer_insert(buffer, &iter, "  Decklin Foster\n", -1);
		gtk_text_buffer_insert(buffer, &iter, "  Nathan Walp\n", -1);
		gtk_text_buffer_insert(buffer, &iter, "  Mark Doliner\n", -1);

		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, _("\nWin32 Port:\n"), -1, "title", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Herman Bloggs ", -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "hermanator12002@yahoo.com\n", -1, "email", NULL);

		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, _("\nRetired Developers:\n"), -1, "title", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Jim Duchek\n", -1);
		gtk_text_buffer_insert(buffer, &iter, "  Eric Warmenhoven ", -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "warmenhoven@yahoo.com\n", -1, "email", NULL);
		gtk_text_buffer_insert(buffer, &iter, "  Mark Spencer (original author) ", -1);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "markster@marko.net\n", -1, "email", NULL);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);


		gtk_container_add(GTK_CONTAINER(sw), view);
		gtk_widget_set_usize(GTK_WIDGET(sw), -1, 350);
		gtk_widget_show(sw);

		gtk_box_pack_start(GTK_BOX(fbox), sw, TRUE, TRUE, 0);
		gtk_widget_show(view);

		/* Close Button */

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		button = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
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
		gtk_widget_show(button);

		/* this makes the sizes not work. */
		/* GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT); */
		/* gtk_widget_grab_default(button); */

		button = gaim_pixbuf_button_from_stock(_("Web Site"), GTK_STOCK_HOME, GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(about_click), NULL);
		gtk_widget_show(button);
	}

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show(about);

	gdk_window_raise(about->window);
}
