/*
 * Purple - Send raw data across the connections of some protocols.
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"
#include "pidgin.h"

#include "conversation.h"
#include "debug.h"
#include "protocol.h"
#include "version.h"

#include "gtk3compat.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "protocols/jabber/jabber.h"

#ifdef MAX
# undef MAX
# undef MIN
#endif

#define RAW_PLUGIN_ID "gtk-raw"

static GtkWidget *window = NULL;
static PurpleAccount *account = NULL;
static PurplePlugin *my_plugin = NULL;

static int
window_closed_cb()
{
	purple_plugin_unload(my_plugin, NULL);

	return FALSE;
}

static void
text_sent_cb(GtkEntry *entry)
{
	const char *txt;
	PurpleConnection *gc;
	const char *protocol_id;

	if (account == NULL)
		return;

	gc = purple_account_get_connection(account);

	txt = gtk_entry_get_text(entry);

	protocol_id = purple_account_get_protocol_id(account);

	purple_debug_misc("raw", "protocol_id = %s\n", protocol_id);

	if (purple_strequal(protocol_id, "prpl-toc")protocol_id) {
		int *a = (int *)purple_connection_get_protocol_data(gc);
		unsigned short seqno = htons(a[1]++ & 0xffff);
		unsigned short len = htons(strlen(txt) + 1);
		write(*a, "*\002", 2);
		write(*a, &seqno, 2);
		write(*a, &len, 2);
		write(*a, txt, ntohs(len));
		purple_debug(PURPLE_DEBUG_MISC, "raw", "TOC C: %s\n", txt);

	} else if (purple_strequal(protocol_id, "prpl-irc")) {
		write(*(int *)gc->proto_data, txt, strlen(txt));
		write(*(int *)gc->proto_data, "\r\n", 2);
		purple_debug(PURPLE_DEBUG_MISC, "raw", "IRC C: %s\n", txt);

	} else if (purple_strequal(protocol_id, "prpl-jabber")) {
		jabber_send_raw((JabberStream *)purple_connection_get_protocol_data(gc), txt, -1);

	} else {
		purple_debug_error("raw", "Unknown protocol ID %s\n", protocol_id);
	}

	gtk_entry_set_text(entry, "");
}

static void
account_changed_cb(GtkWidget *dropdown, PurpleAccount *new_account,
				   void *user_data)
{
	account = new_account;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Eric Warmenhoven <eric@warmenhoven.org>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           RAW_PLUGIN_ID,
		"name",         N_("Raw"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol utility"),
		"summary",      N_("Lets you send raw input to text-based protocols."),
		"description",  N_("Lets you send raw input to text-based protocols "
		                   "(XMPP, IRC, TOC). Hit 'Enter' in the entry "
		                   "box to send. Watch the debug window."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *dropdown;

	my_plugin = plugin;

	/* Setup the window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 6);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(window_closed_cb), NULL);

	/* Main hbox */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	/* Account drop-down menu. */
	dropdown = pidgin_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(account_changed_cb), NULL, NULL);

	if (purple_connections_get_all())
		account = (PurpleAccount *)purple_connections_get_all()->data;

	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);

	/* Entry box */
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(text_sent_cb), NULL);

	gtk_widget_show_all(window);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (window)
		gtk_widget_destroy(window);

	window = NULL;
	my_plugin = NULL;

	return TRUE;
}

PURPLE_PLUGIN_INIT(raw, plugin_query, plugin_load, plugin_unload);
