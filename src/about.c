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

#ifdef USE_APPLET
#include <gnome.h>
#include <applet-widget.h>
#endif /* USE_APPLET */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "pixmaps/logo.xpm"

static GtkWidget *about=NULL;

static void destroy_about()
{
	if (about)
		gtk_widget_destroy(about);
	about = NULL;
}



void show_about(GtkWidget *w, void *null)
{
	GtkWidget *button;
	GtkWidget *vbox;
	GtkWidget *pixmap;
	GtkWidget *label;
	GtkStyle *style;
	GdkPixmap *pm;
	GdkBitmap *bm;
	char abouttitle[45]; 

	if (!about) {
		about = gtk_window_new(GTK_WINDOW_DIALOG);
	             
		g_snprintf(abouttitle, sizeof(abouttitle), "About GAIM v%s", VERSION);
		gtk_window_set_title(GTK_WINDOW(about), abouttitle);
		gtk_container_border_width(GTK_CONTAINER(about), 2);
		gtk_widget_set_usize(about, 455, 370);

		gtk_widget_show(about);
        	aol_icon(about->window);

		
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(about), vbox);
		gtk_widget_show(vbox);
		
		style = gtk_widget_get_style(about);
		pm = gdk_pixmap_create_from_xpm_d(about->window, &bm, 
			&style->bg[GTK_STATE_NORMAL], (gchar **)aol_logo);
		pixmap = gtk_pixmap_new(pm, bm);
		gtk_box_pack_start(GTK_BOX(vbox), pixmap, TRUE, TRUE, 0);
		gtk_widget_show(pixmap);
		
		label = gtk_label_new(
"GAIM is a client that supports AOL's Instant Messenger protocol\nwritten under the GTK\n" 
"It is developed by:\n"
"Rob Flynn <rflynn@blueridge.net> <IM: RobFlynn> (Maintainer)\n"
"Jim Duchek <jimduchek@ou.edu> <IM:Zilding> (Former Maintainer)\n" 
"Mark Spencer <markster@marko.net> <IM: Markster97> (Original Author)\n" 
"\n"
"A special thanks to Eric Warmenhoven for his gnome applet-goodness.\n"
"\n"
"Gaim is brought to you by a team of penguin pimps, the letter G, and beer.\n");

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);
		
		button = gtk_button_new_with_label("Close");
		gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                                          GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(about));
                gtk_signal_connect(GTK_OBJECT(about), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(about));
 
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, FALSE, 0);
		gtk_widget_grab_default(button);
		gtk_widget_show(button);
	
	} else
                gtk_widget_show(about);

}
