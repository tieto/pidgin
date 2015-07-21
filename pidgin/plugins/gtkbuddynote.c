/*
 * GtkBuddyNote - Store notes on particular buddies
 * Copyright (C) 2007 Etan Reisner <deryni@pidgin.im>
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
#include "internal.h"

#include <gtkblist.h>
#include <gtkplugin.h>

#include <debug.h>
#include <version.h>

static void
append_to_tooltip(PurpleBlistNode *node, GString *text, gboolean full)
{
	if (full) {
		const gchar *note = purple_blist_node_get_string(node, "notes");

		if ((note != NULL) && (*note != '\0')) {
			char *tmp, *esc;
			purple_markup_html_to_xhtml(note, NULL, &tmp);
			esc = g_markup_escape_text(tmp, -1);
			g_free(tmp);
			g_string_append_printf(text, _("\n<b>Buddy Note</b>: %s"),
			                       esc);
			g_free(esc);
		}
	}
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Etan Reisner <deryni@pidgin.im>",
		NULL
	};

	const gchar * const dependencies[] = {
		"core-plugin_pack-buddynote",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",            "gtkbuddynote",
		"name",          N_("Buddy Note Tooltips"),
		"version",       DISPLAY_VERSION,
		"category",      N_("User interface"),
		"summary",       N_("Shows stored buddy notes on the buddy's tooltip."),
		"description",   N_("Shows stored buddy notes on the buddy's tooltip."),
		"authors",       authors,
		"website",       PURPLE_WEBSITE,
		"abi-version",   PURPLE_ABI_VERSION,
		"dependencies",  dependencies,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_signal_connect(pidgin_blist_get_handle(), "drawing-tooltip",
	                      plugin, PURPLE_CALLBACK(append_to_tooltip), NULL);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(gtkbuddynote, plugin_query, plugin_load, plugin_unload);
