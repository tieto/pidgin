#define GAIM_PLUGINS
#include "gaim.h"

#include <gtk/gtk.h>

void enter_callback(GtkWidget *widget, GtkWidget *entry) {
	gchar *entry_text;
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry));
	/* this is bad, but you really shouldn't be using this plugin */
	sflap_send(connections->data, entry_text, strlen(entry_text), TYPE_DATA);
}

void destroy_callback(GtkWidget *widget, void *handle) {
	gtk_widget_destroy(widget);
	gaim_plugin_unload(handle);
}

GtkWidget *window;
char *gaim_plugin_init(GModule *h) {
	GtkWidget *entry;

	window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_title(GTK_WINDOW(window), "Gaim - SFLAP interface");

	entry = gtk_entry_new();
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   (GtkSignalFunc)enter_callback,
			   entry);
	gtk_container_add(GTK_CONTAINER(window), entry);
	gtk_widget_show(entry);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   (GtkSignalFunc)destroy_callback,
			   h);

	gtk_widget_show(window);

	return NULL;
}

void gaim_plugin_remove() {
	gtk_widget_destroy(window);
}

char *name() {
	return "TOC Interface";
}

char *description() {
	return "Allows you to talk directly to the TOC server, bypassing gaim.";
}
