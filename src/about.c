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
#include "gtkimhtml.h"

static GtkWidget *about = NULL;

static void destroy_about()
{
	if (about)
		gtk_widget_destroy(about);
	about = NULL;
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

void show_about(GtkWidget *w, void *data)
{
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *text;
	GtkWidget *sw;
	GtkWidget *logo;

	char abouttitle[45];

	if (!about) {

		GAIM_DIALOG(about);
		gtk_window_set_default_size(GTK_WINDOW(about), 450, -1);
		g_snprintf(abouttitle, sizeof(abouttitle), _("About Gaim v%s"), VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_window_set_role(GTK_WINDOW(about), "about");
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

		logo = gaim_pixmap(NULL, "logo.png");
		gtk_box_pack_start(GTK_BOX(fbox), logo, FALSE, FALSE, 0);
		gtk_widget_show(logo);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(fbox), sw, TRUE, TRUE, 0);

		text = gtk_imhtml_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(sw), text);
		gtk_widget_set_usize(sw, -1, 350);
		gaim_setup_imhtml(text);


		gtk_imhtml_append_text(GTK_IMHTML(text),
				  _("Gaim is a modular Instant Messaging client capable of "
					  "using AIM, ICQ, Yahoo!, MSN, IRC, Jabber, Napster, "
					  "Zephyr, and Gadu-Gadu all at once.  It is written using "
					  "Gtk+ and is licensed under the GPL.<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				"<FONT SIZE=\"3\">URL:</FONT> <A HREF=\"" WEBSITE "\">"
				WEBSITE "</A><BR><BR>", -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">IRC:</FONT> #gaim on irc.freenode.net"
				"<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("<FONT SIZE=\"3\">Active Developers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("  Rob Flynn (maintainer) "
			"&lt;<A HREF=\"mailto:rob@marko.net\">rob@marko.net</A>&gt;<BR>"
			"  Sean Egan (coder) "
			"&lt;<A HREF=\"mailto:bj91704@binghamton.edu\">"
			"bj91704@binghamton.edu</A>&gt;"
			"<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
			_("<FONT SIZE=\"3\">Crazy Patch Writers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
			"  Benjamin Miller<BR>"
			"  Decklin Foster<BR>"
			"  Nathan Walp<BR>"
			"  Mark Doliner<BR><BR>", -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">Win32 Port:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
				"  Herman Bloggs "
				"&lt;<A HREF=\"mailto:hermanator12002@yahoo.com\">"
				"hermanator12002@yahoo.com</A>&gt;<BR><BR>", -1, GTK_IMHTML_NO_SCROLL);

		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("<FONT SIZE=\"3\">Retired Developers:</FONT><BR>"), -1, GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(text),
				_("  Jim Duchek<BR>"
				"  Eric Warmenhoven "
				"&lt;<A HREF=\"mailto:warmenhoven@yahoo.com\">"
				"warmenhoven@yahoo.com</A>&gt;<BR>"
				"  Mark Spencer (original author) "
				"&lt;<A HREF=\"mailto:markster@marko.net\">"
				"markster@marko.net</A>&gt;"
				"<BR><BR>"), -1, GTK_IMHTML_NO_SCROLL);

		gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)), 0);

		gtk_widget_show(text);
		gtk_widget_show(sw);


		/* Close Button */

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		button = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

		g_signal_connect_swapped(G_OBJECT(button), "clicked",
								 G_CALLBACK(destroy_about), G_OBJECT(about));
		g_signal_connect(G_OBJECT(about), "destroy",
						 G_CALLBACK(destroy_about), G_OBJECT(about));
		gtk_widget_show(button);

		/* this makes the sizes not work. */
		/* GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT); */
		/* gtk_widget_grab_default(button); */

		gtk_widget_show(button);
	}

	/* Let's give'em something to talk about -- woah woah woah */
	gtk_widget_show(about);
	gtk_window_present(GTK_WINDOW(about));
}
