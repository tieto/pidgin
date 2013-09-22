/*
 * Hello World Plugin
 *
 * Copyright (C) 2004, Gary Kramlich <grim@guifications.org>,
 *               2007, John Bailey <rekkanoryo@cpw.pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* config.h may define PURPLE_PLUGINS; protect the definition here so that we
 * don't get complaints about redefinition when it's not necessary. */
#ifndef PURPLE_PLUGINS
# define PURPLE_PLUGINS
#endif

#include <glib.h>

/* This will prevent compiler errors in some instances and is better explained in the
 * how-to documents on the wiki */
#ifndef G_GNUC_NULL_TERMINATED
# if __GNUC__ >= 4
#  define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define G_GNUC_NULL_TERMINATED
# endif
#endif

#include <notify.h>
#include <plugins.h>

/* This function is the callback for the plugin action we added. All we're
 * doing here is displaying a message. When the user selects the plugin
 * action, this function is called. */
static void
plugin_action_test_cb (PurplePluginAction * action)
{
	purple_notify_message (action->plugin, PURPLE_NOTIFY_MSG_INFO,
		"Plugin Actions Test", "This is a plugin actions test :)", NULL, NULL,
		NULL, NULL);
}

/* we tell libpurple in the PurplePluginInfo struct to call this function to
 * get a list of plugin actions to use for the plugin.  This function gives
 * libpurple that list of actions. */
static GList *
plugin_actions (PurplePlugin * plugin)
{
	/* some C89 (a.k.a. ANSI C) compilers will warn if any variable declaration
	 * includes an initilization that calls a function.  To avoid that, we
	 * generally initialize our variables first with constant values like NULL
	 * or 0 and assign to them with function calls later */
	GList *list = NULL;
	PurplePluginAction *action = NULL;

	/* The action gets created by specifying a name to show in the UI and a
	 * callback function to call. */
	action = purple_plugin_action_new ("Plugin Action Test", plugin_action_test_cb);

	/* libpurple requires a GList of plugin actions, even if there is only one
	 * action in the list.  We append the action to a GList here. */
	list = g_list_append (list, action);

	/* Once the list is complete, we send it to libpurple. */
	return list;
}

static PurplePluginInfo *
plugin_query (GError ** error)
{
	const gchar * const authors[] = {
		"John Bailey <rekkanoryo@cpw.pidgin.im>", /* correct author */
		NULL
	};

	/* For specific notes on the meanings of each of these members, consult the
	   C Plugin Howto on the website. */
	return purple_plugin_info_new (
		"id",           "core-hello_world",
		"name",         "Hello World!",
		"version",      DISPLAY_VERSION, /* This constant is defined in config.h, but you shouldn't use it for your
		                                    own plugins.  We use it here because it's our plugin. And we're lazy. */
		"category",     "Example",
		"summary",      "Hello World Plugin",
		"description",  "Hello World Plugin",
		"authors",      authors,
		"website",      "http://helloworld.tld",
		"abi-version",  PURPLE_ABI_VERSION,
		"get-actions",  plugin_actions, /* this tells libpurple the address of the function to call to get the list
		                                   of plugin actions. */
		NULL
	);
}

static gboolean
plugin_load (PurplePlugin * plugin, GError ** error)
{
	purple_notify_message (plugin, PURPLE_NOTIFY_MSG_INFO, "Hello World!",
		"This is the Hello World! plugin :)", NULL, NULL,
		NULL, NULL);

	return TRUE;
}

static gboolean
plugin_unload (PurplePlugin * plugin, GError ** error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT (hello_world, plugin_query, plugin_load, plugin_unload);
