/*
 * Gaim - iChat-like timestamps
 *
 * Copyright (C) 2002-2003, Sean Egan
 * Copyright (C) 2003, Chris J. Friesen <Darth_Sebulba04@yahoo.com>
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



#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"

#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#define TIMESTAMP_PLUGIN_ID "gtk-timestamp"

/* Set the default to 5 minutes. */
static int interval = 5 * 60 * 1000;

static GSList *timestamp_timeouts;

static gboolean do_timestamp (gpointer data)
{
	GaimConversation *c = (GaimConversation *)data;
	char *buf;
	char mdate[6];
	time_t tim = time(NULL);
        gsize len = 0;

	if (!g_list_find(gaim_get_conversations(), c))
		return FALSE;

        len = GPOINTER_TO_INT(
            gaim_conversation_get_data(c, "timestamp-last-size"));
        if((NULL != c->history) && (len != c->history->len)) {

            strftime(mdate, sizeof(mdate), "%02H:%02M", localtime(&tim));
            buf = g_strdup_printf("%s", mdate);
        }
#ifndef DEBUG_TIMESTAMP
        else {
            return TRUE;
        }
#else
        else {
            buf = g_strdup_printf("NC");
        }
#endif

        gaim_conversation_write(c, NULL, buf, GAIM_MESSAGE_NO_LOG, tim);
        g_free(buf);

        gaim_conversation_set_data(c, "timestamp-last-size",
                                   GINT_TO_POINTER(c->history->len));

	return TRUE;
}

static void timestamp_new_convo(GaimConversation *conv)
{
	do_timestamp(conv);

	timestamp_timeouts = g_slist_append(timestamp_timeouts,
			GINT_TO_POINTER(g_timeout_add(interval, do_timestamp, conv)));

}

static void set_timestamp(GtkWidget *button, GtkWidget *spinner) {
	int tm;

	tm = 0;

	tm = CLAMP(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)), 1, G_MAXINT);
	gaim_debug(GAIM_DEBUG_MISC, "timestamp", "setting time to %d mins\n", tm);

	tm = tm * 60 * 1000;

	interval = tm;
	gaim_prefs_set_int("/plugins/gtk/timestamp/interval", interval);
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

	frame = gaim_gtk_make_frame(ret, _("iChat Timestamp"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	label = gtk_label_new(_("Delay"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	adj = (GtkAdjustment *)gtk_adjustment_new(interval/(60*1000), 1, G_MAXINT, 1, 0, 0);
	spinner = gtk_spin_button_new(adj, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, TRUE, TRUE, 0);

	label = gtk_label_new(_("minutes."));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	button = gtk_button_new_with_mnemonic(_("_Apply"));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(set_timestamp), spinner);

	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GList *cnvs;
	GaimConversation *c;

	timestamp_timeouts = NULL;
	for (cnvs = gaim_get_conversations(); cnvs != NULL; cnvs = cnvs->next) {
		c = cnvs->data;
		timestamp_new_convo(c);
	}

	gaim_signal_connect(gaim_conversations_get_handle(),
						"conversation-created",
						plugin, GAIM_CALLBACK(timestamp_new_convo), NULL);

	interval = gaim_prefs_get_int("/plugins/gtk/timestamp/interval");

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
	GAIM_PLUGIN_API_VERSION,                          /**< api_version    */
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
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/timestamp");
	gaim_prefs_add_int("/plugins/gtk/timestamp/interval", interval);
}

GAIM_INIT_PLUGIN(interval, init_plugin, info)
