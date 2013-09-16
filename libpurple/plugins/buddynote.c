/*
 * BuddyNote - Store notes on particular buddies
 * Copyright (C) 2004 Stu Tomlinson <stu@nosnilmot.com>
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

#include <debug.h>
#include <notify.h>
#include <request.h>
#include <signals.h>
#include <util.h>
#include <version.h>

static void
dont_do_it_cb(PurpleBlistNode *node, const char *note)
{
}

static void
do_it_cb(PurpleBlistNode *node, const char *note)
{
	purple_blist_node_set_string(node, "notes", note);
}

static void
buddynote_edit_cb(PurpleBlistNode *node, gpointer data)
{
	const char *note;

	note = purple_blist_node_get_string(node, "notes");

	purple_request_input(node, _("Notes"),
					   _("Enter your notes below..."),
					   NULL,
					   note, TRUE, FALSE, "html",
					   _("Save"), G_CALLBACK(do_it_cb),
					   _("Cancel"), G_CALLBACK(dont_do_it_cb),
					   NULL, node);
}

static void
buddynote_extended_menu_cb(PurpleBlistNode *node, GList **m)
{
	PurpleMenuAction *bna = NULL;

	if (purple_blist_node_is_transient(node))
		return;

	*m = g_list_append(*m, bna);
	bna = purple_menu_action_new(_("Edit Notes..."), PURPLE_CALLBACK(buddynote_edit_cb), NULL, NULL);
	*m = g_list_append(*m, bna);
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Stu Tomlinson <stu@nosnilmot.com>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           "core-plugin_pack-buddynote",
		"name",         N_("Buddy Notes"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Utility"),
		"summary",      N_("Store notes on particular buddies."),
		"description",  N_("Adds the option to store notes for buddies on your "
		                   "buddy list."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{

	purple_signal_connect(purple_blist_get_handle(), "blist-node-extended-menu",
						plugin, PURPLE_CALLBACK(buddynote_extended_menu_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(buddynote, plugin_query, plugin_load, plugin_unload);
