/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 */

#include "internal.h"
#include "plugins.h"
#include "notify.h"
#include "util.h"
#include "version.h"

PurplePlugin *plugin_handle = NULL;

static char *
outgoing_msg_common(const char *message)
{
  char *m;
  char **ms = g_strsplit(message, "<u>", -1);
  m = g_strjoinv("<font face=\"monospace\" color=\"#00b025\">", ms);
  g_strfreev(ms);

  ms = g_strsplit(m, "</u>", -1);
  g_free(m);
  return g_strjoinv("</font>", ms);
}

static gboolean outgoing_msg_cb1(PurpleConversation *conv, PurpleMessage *msg,
	gpointer null)
{
	purple_message_set_contents(msg,
		outgoing_msg_common(purple_message_get_contents(msg)));
	return FALSE;
}

static void
outgoing_msg_cb2(PurpleAccount *account, PurpleMessage *msg,
	PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	purple_message_set_contents(msg,
		outgoing_msg_common(purple_message_get_contents(msg)));
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Sean Egan <seanegan@gmail.com>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           "codeinline",
		"name",         "Code Inline",
		"version",      "1.0",
		"category",     "Formatting",
		"summary",      "Formats text as code",
		"description",  "Changes the formatting of any outgoing text such "
		                "that anything underlined will be received green and "
		                "monospace.",
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
     void *handle = purple_conversations_get_handle();
     plugin_handle = plugin;
     purple_signal_connect(handle, "writing-im-msg", plugin,
                PURPLE_CALLBACK(outgoing_msg_cb1), NULL);
     purple_signal_connect(handle, "sending-im-msg", plugin,
		PURPLE_CALLBACK(outgoing_msg_cb2), NULL);

     return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(codeinline, plugin_query, plugin_load, plugin_unload);
