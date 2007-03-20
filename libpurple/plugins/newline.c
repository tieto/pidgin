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
addnewline_msg_cb(PurpleAccount *account, char *sender, char **message,
					 PurpleConversation *conv, int *flags, void *data)
{
	if (g_ascii_strncasecmp(*message, "/me ", strlen("/me "))) {
		char *tmp = g_strdup_printf("\n%s", *message);
		g_free(*message);
		*message = tmp;
	}

	return FALSE;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *conversation = purple_conversations_get_handle();

	purple_signal_connect(conversation, "writing-im-msg",
						plugin, PURPLE_CALLBACK(addnewline_msg_cb), NULL);
	purple_signal_connect(conversation, "writing-chat-msg",
						plugin, PURPLE_CALLBACK(addnewline_msg_cb), NULL);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,								/**< magic			*/
	PURPLE_MAJOR_VERSION,								/**< major version	*/
	PURPLE_MINOR_VERSION,								/**< minor version	*/
	PURPLE_PLUGIN_STANDARD,							/**< type			*/
	NULL,											/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	PURPLE_PRIORITY_DEFAULT,							/**< priority		*/

	"core-plugin_pack-newline",						/**< id				*/
	N_("New Line"),									/**< name			*/
	VERSION,										/**< version		*/
	N_("Prepends a newline to displayed message."),	/**  summary		*/
	N_("Prepends a newline to messages so that the "
	   "rest of the message appears below the "
	   "screen name in the conversation window."),	/**  description	*/
	"Stu Tomlinson <stu@nosnilmot.com>",			/**< author			*/
	PURPLE_WEBSITE,									/**< homepage		*/

	plugin_load,									/**< load			*/
	NULL,											/**< unload			*/
	NULL,											/**< destroy		*/

	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	NULL,											/**< prefs_info		*/
	NULL											/**< actions		*/
};

static void
init_plugin(PurplePlugin *plugin) {
}

PURPLE_INIT_PLUGIN(lastseen, init_plugin, info)
