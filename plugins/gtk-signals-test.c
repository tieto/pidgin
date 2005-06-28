/*
 * Signals test plugin.
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
#define GTK_SIGNAL_TEST_PLUGIN_ID "gtk-signals-test"

#include <gtk/gtk.h>

#include "internal.h"
#include "debug.h"
#include "version.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkplugin.h"

/**************************************************************************
 * Account subsystem signal callbacks
 **************************************************************************/
static void
account_modified_cb(GaimAccount *account, void *data) {
	gaim_debug_info("gtk-signal-test", "account modified cb\n");
}

/**************************************************************************
 * Buddy List subsystem signal callbacks
 **************************************************************************/
static void
blist_created_cb(GaimBuddyList *blist, void *data) {
	gaim_debug_info("gtk-signal-test", "buddy list created\n");
}

static void
blist_drawing_tooltip_cb(GaimBlistNode *node, char **text, void *data) {
	gaim_debug_info("gtk-signal-test", "drawing tooltip cb\n");
}

/**************************************************************************
 * Conversation subsystem signal callbacks
 **************************************************************************/
static void
conversation_drag_end_cb(GaimConvWindow *source, GaimConvWindow *destination) {
	gaim_debug_info("gtk-signal-test", "conversation drag ended cb\n");
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *accounts_handle = gaim_gtk_account_get_handle();
	void *blist_handle = gaim_gtk_blist_get_handle();
	void *conv_handle = gaim_gtk_conversations_get_handle();

	gaim_debug_register_category("gtk-signal-test");

	/* Accounts subsystem signals */
	gaim_signal_connect(accounts_handle, "account-modified",
						plugin, GAIM_CALLBACK(account_modified_cb), NULL);

	/* Buddy List subsystem signals */
	gaim_signal_connect(blist_handle, "gtkblist-created",
						plugin, GAIM_CALLBACK(blist_created_cb), NULL);
	gaim_signal_connect(blist_handle, "drawing-tooltip",
						plugin, GAIM_CALLBACK(blist_drawing_tooltip_cb), NULL);

	/* Conversations subsystem signals */
	gaim_signal_connect(conv_handle, "conversation-drag-ended",
						plugin, GAIM_CALLBACK(conversation_drag_end_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_debug_unregister_category("gtk-signal-test");
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	GTK_SIGNAL_TEST_PLUGIN_ID,                        /**< id             */
	N_("GTK Signals Test"),                             /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Test to see that all ui signals are working properly."),
	                                                  /**  description    */
	N_("Test to see that all ui signals are working properly."),
	"Gary Kramlich <amc_grim@users.sf.net>",              /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
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

GAIM_INIT_PLUGIN(gtksignalstest, init_plugin, info)
