/*
 * This is a plugin to load perl scripts. If you don't enter
 * in a name of a script to load it will unload all perl
 * scripts. This is just to test that perl is working in gaim
 * before the UI comes in. You can use this to start building
 * perl scripts, but don't use this for anything real yet.
 *
 */

#define GAIM_PLUGINS
#include "gaim.h"
#include "pixmaps/add.xpm"
#include "pixmaps/cancel.xpm"

char *name() {
	return "Perl Plug";
}

char *description() {
	return "Interface for loading perl scripts";
}

int gaim_plugin_init(void *h) {
	perl_init();
}

static GtkWidget *config = NULL;
static GtkWidget *entry = NULL;

static void cfdes(GtkWidget *m, gpointer n) {
	if (config) gtk_widget_destroy(config);
	config = NULL;
}

static void do_load(GtkWidget *m, gpointer n) {
	char *file = gtk_entry_get_text(GTK_ENTRY(entry));
	if (!file || !strlen(file)) {
		perl_end();
		perl_init();
		return;
	}
	perl_load_file(file);
	gtk_widget_destroy(config);
}

void gaim_plugin_config() {
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *ok;
	GtkWidget *cancel;

	if (config) {
		gtk_widget_show(config);
		return;
	}

	config = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_policy(GTK_WINDOW(config), 0, 0, 1);
	gtk_window_set_title(GTK_WINDOW(config), "Gaim - Add Perl Script");
	gtk_container_set_border_width(GTK_CONTAINER(config), 5);
	gtk_signal_connect(GTK_OBJECT(config), "destroy", GTK_SIGNAL_FUNC(cfdes), 0);
	gtk_widget_realize(config);
	aol_icon(config->window);

	frame = gtk_frame_new("Load Script");
	gtk_container_add(GTK_CONTAINER(config), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("File Name:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(do_load), 0);
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
	gtk_widget_show(hbox);

	ok = picture_button(config, "Load", add_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), ok, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(do_load), 0);

	cancel = picture_button(config, "Cancel", cancel_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), cancel, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(cfdes), 0);

	gtk_widget_show(config);
}

void gaim_plugin_remove() {
	if (config) gtk_widget_destroy(config);
	perl_end();
}
