/*
 * win2ktrans
 *
 * copyright (c) 1998-2002, rob flynn <rob@marko.net> 
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 59 temple place, suite 330, boston, ma  02111-1307  usa
 *
 */
#define GAIM_PLUGINS

#include <gtk/gtk.h>
#include "wintransparency.h"
#include "gaim.h"

int alpha = 255;

static void gaim_new_conversation(char *who) {
	struct conversation *c = find_conversation(who);

	if (!c || alpha == 255)
		return;

	gdk_window_add_filter (GTK_WIDGET(c->window)->window, wgaim_window_filter, c);
}


G_MODULE_EXPORT char *gaim_plugin_init(GModule *handle) {


	gaim_signal_connect(handle, event_new_conversation, gaim_new_conversation, NULL); 

	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
}

struct gaim_plugin_description desc; 

G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Transparency"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("This plugin enables variable alpha transparency on conversation windows.\n\n* Note: This plugin requires Win2000 or WinXP.")); 
	desc.authors = g_strdup(_("Rob Flynn &lt;rob@marko.net&gt;"));
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

G_MODULE_EXPORT char *name() {
	return _("Transparency");
}

G_MODULE_EXPORT char *description() {
	return _("This plugin enables variable alpha transparency on conversation windows.\n\n* Note: This plugin requires Win2000 or WinXP.");
}

void alpha_value_changed(GtkWidget *w, gpointer data)
{
	alpha = gtk_range_get_value(GTK_RANGE(w));
}

G_MODULE_EXPORT GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *hbox;
	GtkWidget *label, *slider;

	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), 255);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);

	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(alpha_value_changed), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	return hbox;
}
