#define GAIM_PLUGINS
#include "gaim.h"
#include "prpl.h"
#ifdef MAX
#undef MAX
#undef MIN
#endif
#include "jabber/jabber.h"

static GtkWidget *window = NULL;
static GModule *me = NULL;

/* this is an evil hack. gc->proto_data for Jabber connections can be cast to a jconn *. */

char *name()
{
	return "RawJab";
}

char *description()
{
	return "Lets you send raw XML to Jabber. Not very useful except for debugging. Hit 'enter'"
		" in the entry to send. Watch the debug window.";
}

static int goodbye()
{
	gaim_plugin_unload(me);
	return FALSE;
}

static void send_it(GtkEntry *entry)
{
	char *txt;
	GSList *c = connections;
	struct gaim_connection *gc;
	while (c) {
		gc = c->data;
		if (gc->protocol == PROTO_JABBER)
			break;
		c = c->next;
	}
	if (!c) return;
	txt = gtk_entry_get_text(entry);
	jab_send_raw(*(jconn *)gc->proto_data, txt);
	gtk_entry_set_text(entry, "");
}

char *gaim_plugin_init(GModule *h)
{
	GtkWidget *entry;
	me = h;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(goodbye), NULL);

	entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(window), entry);
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(send_it), NULL);

	gtk_widget_show_all(window);

	return NULL;
}

void gaim_plugin_remove()
{
	if (window)
		gtk_widget_destroy(window);
	window = NULL;
	me = NULL;
}
