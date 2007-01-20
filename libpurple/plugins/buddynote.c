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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "internal.h"

#include <debug.h>
#include <notify.h>
#include <request.h>
#include <signals.h>
#include <util.h>
#include <version.h>

static void
dont_do_it_cb(GaimBlistNode *node, const char *note)
{
}

static void
do_it_cb(GaimBlistNode *node, const char *note)
{
	gaim_blist_node_set_string(node, "notes", note);
}

static void
buddynote_edit_cb(GaimBlistNode *node, gpointer data)
{
	const char *note;

	note = gaim_blist_node_get_string(node, "notes");

	gaim_request_input(node, _("Notes"),
					   _("Enter your notes below..."),
					   NULL,
					   note, TRUE, FALSE, "html",
					   _("Save"), G_CALLBACK(do_it_cb),
					   _("Cancel"), G_CALLBACK(dont_do_it_cb),
					   node);
}

static void
buddynote_extended_menu_cb(GaimBlistNode *node, GList **m)
{
	GaimMenuAction *bna = NULL;

	*m = g_list_append(*m, bna);
	bna = gaim_menu_action_new(_("Edit Notes..."), GAIM_CALLBACK(buddynote_edit_cb), NULL, NULL);
	*m = g_list_append(*m, bna);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{

	gaim_signal_connect(gaim_blist_get_handle(), "blist-node-extended-menu",
						plugin, GAIM_CALLBACK(buddynote_extended_menu_cb), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,								/**< major version	*/
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,							/**< type			*/
	NULL,											/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	GAIM_PRIORITY_DEFAULT,							/**< priority		*/

	"core-plugin_pack-buddynote",					/**< id				*/
	N_("Buddy Notes"),								/**< name			*/
	VERSION,										/**< version		*/
	N_("Store notes on particular buddies."),		/**  summary		*/
	N_("Adds the option to store notes for buddies "
	   "on your buddy list."),						/**  description	*/
	"Stu Tomlinson <stu@nosnilmot.com>",			/**< author			*/
	GAIM_WEBSITE,									/**< homepage		*/

	plugin_load,									/**< load			*/
	NULL,											/**< unload			*/
	NULL,											/**< destroy		*/

	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	NULL,											/**< prefs_info		*/
	NULL											/**< actions		*/
};


static void
init_plugin(GaimPlugin *plugin) {
}

GAIM_INIT_PLUGIN(buddynote, init_plugin, info)
