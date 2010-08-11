/*
 * SendButton - Add a Send button to the conversation window entry area.
 * Copyright (C) 2008 Etan Reisner <deryni@pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "internal.h"

#include "version.h"

#include "pidgin.h"

#include "gtkconv.h"
#include "gtkplugin.h"

static void
send_button_cb(GtkButton *button, PidginConversation *gtkconv)
{
	g_signal_emit_by_name(gtkconv->entry, "message_send");
}

static void
create_send_button_pidgin(PidginConversation *gtkconv)
{
	GtkWidget *send_button;

	send_button = gtk_button_new_with_mnemonic(_("_Send"));
	g_signal_connect(G_OBJECT(send_button), "clicked",
	                 G_CALLBACK(send_button_cb), gtkconv);
	gtk_box_pack_end(GTK_BOX(gtkconv->lower_hbox), send_button, FALSE,
	                 FALSE, 0);
	gtk_widget_show(send_button);

	g_object_set_data(G_OBJECT(gtkconv->lower_hbox), "send_button",
	                  send_button);
}

static void
remove_send_button_pidgin(PidginConversation *gtkconv)
{
	GtkWidget *send_button = NULL;

	send_button = g_object_get_data(G_OBJECT(gtkconv->lower_hbox),
	                                "send_button");
	if (send_button != NULL) {
		gtk_widget_destroy(send_button);
	}
}

static void
conversation_displayed_cb(PidginConversation *gtkconv)
{
	GtkWidget *send_button = NULL;

	send_button = g_object_get_data(G_OBJECT(gtkconv->lower_hbox),
	                                "send_button");
	if (send_button == NULL) {
		create_send_button_pidgin(gtkconv);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();
	void *gtk_conv_handle = pidgin_conversations_get_handle();

	purple_signal_connect(gtk_conv_handle, "conversation-displayed", plugin,
	                      PURPLE_CALLBACK(conversation_displayed_cb), NULL);
	/*
	purple_signal_connect(gtk_conv_handle, "conversation-hiding", plugin,
	                      PURPLE_CALLBACK(conversation_hiding_cb), NULL);
	 */

	while (convs) {
		
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Setup Send button */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			create_send_button_pidgin(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Remove Send button */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			remove_send_button_pidgin(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtksendbutton",                                /**< id */
	N_("Send Button"),                              /**< name */
	DISPLAY_VERSION,                                /**< version */
	N_("Conversation Window Send Button."),         /**< summary */
	N_("Adds a Send button to the entry area of "
	   "the conversation window. Intended for when "
	   "no physical keyboard is present."),         /**< description */
	"Etan Reisner <deryni@pidgin.im>",              /**< author */
	PURPLE_WEBSITE,                                 /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(sendbutton, init_plugin, info)
