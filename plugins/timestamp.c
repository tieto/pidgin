/* iChat-like timestamps by Sean Egan.
 *
 * Modified by: Chris J. Friesen <Darth_Sebulba04@yahoo.com> Jan 05, 2003.
 * <INSERT GPL HERE> */

#include "config.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include <time.h>
#include "gaim.h"
#include "gtkimhtml.h"

//Set the default to 5 minutes.
int timestamp = 5 * 60 * 1000;

GModule *handle;
GSList *timestamp_timeouts;

gboolean do_timestamp (gpointer data)
{
	struct gaim_conversation *c = (struct gaim_conversation *)data;
	char *buf;
	char mdate[6];
	time_t tim = time(NULL);
	
	if (!g_list_find(conversations, c))
		return FALSE;

	strftime(mdate, sizeof(mdate), "%H:%M", localtime(&tim));
	buf = g_strdup_printf("            %s", mdate);
	gaim_conversation_write(c, NULL, buf, WFLAG_NOLOG, tim, -1);
	g_free(buf);
	return TRUE;
}

void timestamp_new_convo(char *name)
{
	struct gaim_conversation *c = gaim_find_conversation(name);
	do_timestamp(c);

	timestamp_timeouts = g_slist_append(timestamp_timeouts,
			GINT_TO_POINTER(g_timeout_add(timestamp, do_timestamp, c)));

}

static void set_timestamp(GtkWidget *button, GtkWidget *spinner) {
	int tm;
       
	tm = 0;
	
	tm = CLAMP(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)), 1, G_MAXINT);
	debug_printf("setting  time to %d mins\n", tm);
	
	tm = tm * 60 * 1000;

	timestamp = tm;
}

GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *ret;
	GtkWidget *frame, *label;
	GtkWidget *vbox, *hbox;
	GtkAdjustment *adj;
	GtkWidget *spinner, *button;
	
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	frame = make_frame(ret, _("iChat Timestamp"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	label = gtk_label_new("Delay");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	
	adj = (GtkAdjustment *)gtk_adjustment_new(timestamp/(60*1000), 1, G_MAXINT, 1, 0, 0);
	spinner = gtk_spin_button_new(adj, 0, 0);
        gtk_box_pack_start(GTK_BOX(hbox), spinner, TRUE, TRUE, 0);

        label = gtk_label_new("minutes.");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

        hbox = gtk_hbox_new(TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

        button = gtk_button_new_with_mnemonic(_("_Apply"));
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(set_timestamp), spinner);
	
	gtk_widget_show_all(ret);
	return ret;
}


char *gaim_plugin_init(GModule *h) {
	GList *cnvs = conversations;
	struct gaim_conversation *c;
	handle = h;

	while (cnvs) {
		c = cnvs->data;
		timestamp_new_convo(c->name);
		cnvs = cnvs->next;
	}
	gaim_signal_connect(handle, event_new_conversation, timestamp_new_convo, NULL);

	return NULL;
}

void gaim_plugin_remove() {
	GSList *to;
	to = timestamp_timeouts;
	while (to) {
		g_source_remove(GPOINTER_TO_INT(to->data));
		to = to->next;
	}
	g_slist_free(timestamp_timeouts);
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Timestamp"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Adds iChat-style timestamps to conversations every N minutes."));
	desc.authors = g_strdup("Sean Egan &lt;bj91704@binghamton.edu>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
