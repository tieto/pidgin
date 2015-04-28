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

#if 0
static gboolean
check_for_buddynote(gpointer data)
{
	PurplePlugin *buddynote = NULL;
	PurplePlugin *plugin = (PurplePlugin *)data;

	buddynote = purple_plugins_find_with_id("core-plugin_pack-buddynote");

	if (buddynote == NULL) {
		buddynote = purple_plugins_find_with_basename("buddynote");
	}

	if (buddynote != NULL) {
		PurplePluginInfo *bninfo = buddynote->info;

		bninfo->flags = PURPLE_PLUGIN_FLAG_INVISIBLE;


		/* If non-gtk buddy note plugin is loaded, but we are not, then load
		 * ourselves, otherwise people upgrading from pre-gtkbuddynote days
		 * will not have 'Buddy Notes' showing as loaded in the plugins list.
		 * We also trigger a save on the list of plugins because it's not been
		 * loaded through the UI. */
		if (purple_plugin_is_loaded(buddynote) &&
		    !purple_plugin_is_loaded(plugin)) {
			purple_plugin_load(plugin);
			pidgin_plugins_save();
		}

	} else {
		info.flags = PURPLE_PLUGIN_FLAG_INVISIBLE;
	}

	return FALSE;
}
#endif

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
#if 0
	PurplePlugin *buddynote = NULL;

	buddynote = purple_plugins_find_with_id("core-plugin_pack-buddynote");
	purple_plugin_unload(buddynote);
#endif

	return TRUE;
}

PURPLE_PLUGIN_INIT(gtkbuddynote, plugin_query, plugin_load, plugin_unload);
