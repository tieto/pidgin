/* iChat-like timestamps by Sean Egan.
 *
 * Modified by: Chris J. Friesen <Darth_Sebulba04@yahoo.com> Jan 05, 2003.
 * <INSERT GPL HERE> */

#include "config.h"

#include <time.h>
#include "gaim.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define TIMESTAMP_PLUGIN_ID "gtk-timestamp"

//Set the default to 5 minutes.
static int timestamp = 5 * 60 * 1000;

static GSList *timestamp_timeouts;

gboolean do_timestamp (gpointer data)
{
	struct gaim_conversation *c = (struct gaim_conversation *)data;
	char *buf;
	char mdate[6];
	time_t tim = time(NULL);
	
	if (!g_list_find(gaim_get_conversations(), c))
		return FALSE;

	strftime(mdate, sizeof(mdate), "%H:%M", localtime(&tim));
	buf = g_strdup_printf("            %s", mdate);
	gaim_conversation_write(c, NULL, buf, -1, WFLAG_NOLOG, tim);
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
	gaim_debug(GAIM_DEBUG_MISC, "timestamp", "setting  time to %d mins\n", tm);
	
	tm = tm * 60 * 1000;

	timestamp = tm;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
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

	label = gtk_label_new(_("Delay"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	
	adj = (GtkAdjustment *)gtk_adjustment_new(timestamp/(60*1000), 1, G_MAXINT, 1, 0, 0);
	spinner = gtk_spin_button_new(adj, 0, 0);
        gtk_box_pack_start(GTK_BOX(hbox), spinner, TRUE, TRUE, 0);

        label = gtk_label_new(_("minutes."));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

        hbox = gtk_hbox_new(TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

        button = gtk_button_new_with_mnemonic(_("_Apply"));
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(set_timestamp), spinner);
	
	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GList *cnvs;
	struct gaim_conversation *c;

	for (cnvs = gaim_get_conversations(); cnvs != NULL; cnvs = cnvs->next) {
		c = cnvs->data;
		timestamp_new_convo(c->name);
	}

	gaim_signal_connect(plugin, event_new_conversation,
						timestamp_new_convo, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	GSList *to;

	for (to = timestamp_timeouts; to != NULL; to = to->next)
		g_source_remove(GPOINTER_TO_INT(to->data));

	g_slist_free(timestamp_timeouts);

	return TRUE;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	TIMESTAMP_PLUGIN_ID,                              /**< id             */
	N_("Timestamp"),                                  /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Adds iChat-style timestamps to conversations every N minutes."),
	                                                  /**  description    */
	N_("Adds iChat-style timestamps to conversations every N minutes."),
	"Sean Egan <bj91704@binghamton.edu>",             /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(timestamp, __init_plugin, info);
