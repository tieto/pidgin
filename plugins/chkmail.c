#define GAIM_PLUGINS

#include <stdio.h>
#include "gaim.h"

static void *handle = NULL;
extern GtkWidget *blist;
GtkWidget *maily;
GtkWidget *vbox2;
GList *tmp;

void gaim_plugin_init(void *h) {
	handle = h;
	printf("Wahoo\n");
	tmp = gtk_container_children(GTK_CONTAINER(blist));

	maily = gtk_label_new("TESTING!!!");
	vbox2 = (GtkWidget *)tmp->data;

	gtk_box_pack_start(GTK_BOX(vbox2), maily, FALSE, FALSE, 5);
	gtk_box_reorder_child(GTK_BOX(vbox2), maily, 2);
	gtk_widget_show(maily);
}

void gaim_plugin_remove() {
	handle = NULL;
	gtk_widget_hide(maily);
	gtk_widget_destroy(maily);
}

char *name() {
	return "Check Mail";
}

char *description() {
	return "Check email every X seconds.\n";
}
