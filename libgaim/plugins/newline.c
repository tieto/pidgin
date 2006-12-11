/*
 * Displays messages on a new line, below the nick
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

#include <string.h>

#include <conversation.h>
#include <debug.h>
#include <plugin.h>
#include <signals.h>
#include <util.h>
#include <version.h>

static gboolean
addnewline_msg_cb(GaimAccount *account, char *sender, char **message,
					 GaimConversation *conv, int *flags, void *data)
{
	if (g_ascii_strncasecmp(*message, "/me ", strlen("/me "))) {
		char *tmp = g_strdup_printf("\n%s", *message);
		g_free(*message);
		*message = tmp;
	}

	return FALSE;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *conversation = gaim_conversations_get_handle();

	gaim_signal_connect(conversation, "displaying-im-msg",
						plugin, GAIM_CALLBACK(addnewline_msg_cb), NULL);
	gaim_signal_connect(conversation, "displaying-chat-msg",
						plugin, GAIM_CALLBACK(addnewline_msg_cb), NULL);
	gaim_signal_connect(conversation, "receiving-im-msg",
						plugin, GAIM_CALLBACK(addnewline_msg_cb), NULL);
	gaim_signal_connect(conversation, "receiving-chat-msg",
						plugin, GAIM_CALLBACK(addnewline_msg_cb), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,								/**< magic			*/
	GAIM_MAJOR_VERSION,								/**< major version	*/
	GAIM_MINOR_VERSION,								/**< minor version	*/
	GAIM_PLUGIN_STANDARD,							/**< type			*/
	NULL,											/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	GAIM_PRIORITY_DEFAULT,							/**< priority		*/

	"core-plugin_pack-newline",						/**< id				*/
	N_("New Line"),									/**< name			*/
	VERSION,										/**< version		*/
	N_("Prepends a newline to displayed message."),	/**  summary		*/
	N_("Prepends a newline to messages so that the "
	   "test of the message appears below the "
	   "screen name in the conversation window."),	/**  description	*/
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

GAIM_INIT_PLUGIN(lastseen, init_plugin, info)
