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
#include "version.h"

#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#define TIMESTAMP_PLUGIN_ID "gtk-timestamp"

/* Set the default to 5 minutes. */
static int interval = 5 * 60 * 1000;

static GSList *timestamp_timeouts = NULL;

static gboolean
do_timestamp(gpointer data)
{
	GaimConversation *c = (GaimConversation *)data;
	GaimGtkConversation *conv = GAIM_GTK_CONVERSATION(c);
	GtkTextIter iter;
	const char *mdate;
	int is_conversation_active;
	time_t tim = time(NULL);

	if (!g_list_find(gaim_get_conversations(), c))
		return FALSE;

	/* is_conversation_active is true if an im has been displayed since the last timestamp */
	is_conversation_active = GPOINTER_TO_INT(gaim_conversation_get_data(c, "timestamp-conv-active"));

	if (is_conversation_active){
		int y, height;
		GdkRectangle rect;
		gboolean scroll = TRUE;
		GtkWidget *imhtml = conv->imhtml;
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(imhtml));
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gaim_conversation_set_data(c, "timestamp-conv-active", GINT_TO_POINTER(FALSE));
		mdate = gaim_utf8_strftime("\n%H:%M", localtime(&tim));
		gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
		gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(imhtml), &iter, &y, &height);
		if(((y + height) - (rect.y + rect.height)) > height
		   && gtk_text_buffer_get_char_count(buffer)){
			scroll = FALSE;
		}
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, mdate, strlen(mdate), "TIMESTAMP", NULL);
		if (scroll)
			gtk_imhtml_scroll_to_end(GTK_IMHTML(imhtml), gaim_prefs_get_bool("/gaim/gtk/conversations/use_smooth_scrolling"));
	}
	else
		gaim_conversation_set_data(c, "timestamp-enabled", GINT_TO_POINTER(FALSE));

	return TRUE;
}


static gboolean
timestamp_displaying_conv_msg(GaimAccount *account, const char *who, char **buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	int is_timestamp_enabled;

	if (!g_list_find(gaim_get_conversations(), conv))
		return FALSE;

	/* set to true, since there has been an im since the last timestamp */
	gaim_conversation_set_data(conv, "timestamp-conv-active", GINT_TO_POINTER(TRUE));

	is_timestamp_enabled = GPOINTER_TO_INT(gaim_conversation_get_data(conv, "timestamp-enabled"));

	if (!is_timestamp_enabled){
		gaim_conversation_set_data(conv, "timestamp-enabled", GINT_TO_POINTER(TRUE));
		do_timestamp((gpointer)conv);
	}

	return FALSE;
}

static void timestamp_new_convo(GaimConversation *conv)
{
	GaimGtkConversation *c = GAIM_GTK_CONVERSATION(conv);

	if (!g_list_find(gaim_get_conversations(), conv))
		return;

	/*
	This if statement stops conversations that have already been initialized for timestamps
	from being reinitialized.  This prevents every active conversation from immediately being spammed
	with a new timestamp when the user modifies the timer interval.
	*/
	if (!gaim_conversation_get_data(conv, "timestamp-initialized")){
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(c->imhtml));
		gaim_conversation_set_data(conv, "timestamp-initialized", GINT_TO_POINTER(TRUE));
		gaim_conversation_set_data(conv, "timestamp-enabled", GINT_TO_POINTER(TRUE));
		gaim_conversation_set_data(conv, "timestamp-conv-active", GINT_TO_POINTER(TRUE));
		gtk_text_buffer_create_tag (buffer, "TIMESTAMP", "foreground", "#888888", "justification", GTK_JUSTIFY_CENTER,
					    "weight", PANGO_WEIGHT_BOLD, NULL);
		do_timestamp(conv);
	}

	timestamp_timeouts = g_slist_append(timestamp_timeouts,
			GINT_TO_POINTER(g_timeout_add(interval, do_timestamp, conv)));
}


static void destroy_timer_list()
{
	GSList *to;

	for (to = timestamp_timeouts; to != NULL; to = to->next)
		g_source_remove(GPOINTER_TO_INT(to->data));

	g_slist_free(timestamp_timeouts);

	timestamp_timeouts = NULL;
}

static void init_timer_list()
{
	GList *cnvs;
	GaimConversation *c;

	if (timestamp_timeouts != NULL)
		destroy_timer_list();

	for (cnvs = gaim_get_conversations(); cnvs != NULL; cnvs = cnvs->next) {
		c = cnvs->data;
		timestamp_new_convo(c);
	}
}



static void set_timestamp(GtkWidget *spinner, void *null) {
	int tm;

	tm = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
	gaim_debug(GAIM_DEBUG_MISC, "timestamp", "setting time to %d mins\n", tm);

	interval = tm * 60 * 1000;
	gaim_prefs_set_int("/plugins/gtk/timestamp/interval", interval);

	destroy_timer_list();
	init_timer_list();
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame, *label;
	GtkWidget *vbox, *hbox;
	GtkAdjustment *adj;
	GtkWidget *spinner;

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
	g_signal_connect(G_OBJECT(spinner), "value-changed", G_CALLBACK(set_timestamp), NULL);
	label = gtk_label_new(_("minutes."));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *conv_handle = gaim_conversations_get_handle();
	void *gtkconv_handle = gaim_gtk_conversations_get_handle();

	init_timer_list();

	gaim_signal_connect(conv_handle, "conversation-created",
					plugin, GAIM_CALLBACK(timestamp_new_convo), NULL);

	gaim_signal_connect(gtkconv_handle, "displaying-chat-msg",
					plugin, GAIM_CALLBACK(timestamp_displaying_conv_msg), NULL);
	gaim_signal_connect(gtkconv_handle, "displaying-im-msg",
					plugin, GAIM_CALLBACK(timestamp_displaying_conv_msg), NULL);

	interval = gaim_prefs_get_int("/plugins/gtk/timestamp/interval");

	return TRUE;
}



static gboolean
plugin_unload(GaimPlugin *plugin)
{
	destroy_timer_list();

	return TRUE;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame,
	0 /* page_num (Reserved) */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
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
	"Sean Egan <seanegan@gmail.com>",                 /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/timestamp");
	gaim_prefs_add_int("/plugins/gtk/timestamp/interval", interval);
}

GAIM_INIT_PLUGIN(interval, init_plugin, info)
