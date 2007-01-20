/*
 * IPC test server plugin.
 *
 * Copyright (C) 2003 Christian Hammond.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#define IPC_TEST_SERVER_PLUGIN_ID "core-ipc-test-server"

#include "internal.h"
#include "debug.h"
#include "plugin.h"
#include "version.h"

static int
add_func(int i1, int i2)
{
	gaim_debug_misc("ipc-test-server", "Got %d, %d, returning %d\n",
					i1, i2, i1 + i2);
	return i1 + i2;
}

static int
sub_func(int i1, int i2)
{
	gaim_debug_misc("ipc-test-server", "Got %d, %d, returning %d\n",
					i1, i2, i1 - i2);
	return i1 - i2;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_plugin_ipc_register(plugin, "add", GAIM_CALLBACK(add_func),
							 gaim_marshal_INT__INT_INT,
							 gaim_value_new(GAIM_TYPE_INT), 2,
							 gaim_value_new(GAIM_TYPE_INT),
							 gaim_value_new(GAIM_TYPE_INT));

	gaim_plugin_ipc_register(plugin, "sub", GAIM_CALLBACK(sub_func),
							 gaim_marshal_INT__INT_INT,
							 gaim_value_new(GAIM_TYPE_INT), 2,
							 gaim_value_new(GAIM_TYPE_INT),
							 gaim_value_new(GAIM_TYPE_INT));

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	IPC_TEST_SERVER_PLUGIN_ID,                        /**< id             */
	N_("IPC Test Server"),                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Test plugin IPC support, as a server."),
	                                                  /**  description    */
	N_("Test plugin IPC support, as a server. This registers the IPC "
	   "commands."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(ipctestserver, init_plugin, info)
