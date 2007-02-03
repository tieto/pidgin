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
blist_drawing_tooltip_cb(GaimBlistNode *node, GString *str, gboolean full, void *data) {
	gaim_debug_info("gtk-signal-test", "drawing tooltip cb\n");
}

/**************************************************************************
 * Conversation subsystem signal callbacks
 **************************************************************************/
static void
conversation_dragging_cb(PidginWindow *source, PidginWindow *destination) {
	gaim_debug_info("gtk-signal-test", "conversation dragging cb\n");
}

static gboolean
displaying_im_msg_cb(GaimAccount *account, const char *who, char **buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("gtk-signals test", "displaying-im-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), *buffer);

	return FALSE;
}

static void
displayed_im_msg_cb(GaimAccount *account, const char *who, const char *buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("gtk-signals test", "displayed-im-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), buffer);
}

static gboolean
displaying_chat_msg_cb(GaimAccount *account, const char *who, char **buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("gtk-signals test", "displaying-chat-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), *buffer);

	return FALSE;
}

static void
displayed_chat_msg_cb(GaimAccount *account, const char *who, const char *buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("gtk-signals test", "displayed-chat-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), buffer);
}

static void
conversation_switched_cb(GaimConversation *conv, void *data)
{
	gaim_debug_misc("gtk-signals test", "conversation-switched (%s)\n",
					gaim_conversation_get_name(conv));
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *accounts_handle = pidgin_account_get_handle();
	void *blist_handle = pidgin_blist_get_handle();
	void *conv_handle = pidgin_conversations_get_handle();

	/* Accounts subsystem signals */
	gaim_signal_connect(accounts_handle, "account-modified",
						plugin, GAIM_CALLBACK(account_modified_cb), NULL);

	/* Buddy List subsystem signals */
	gaim_signal_connect(blist_handle, "gtkblist-created",
						plugin, GAIM_CALLBACK(blist_created_cb), NULL);
	gaim_signal_connect(blist_handle, "drawing-tooltip",
						plugin, GAIM_CALLBACK(blist_drawing_tooltip_cb), NULL);

	/* Conversations subsystem signals */
	gaim_signal_connect(conv_handle, "conversation-dragging",
						plugin, GAIM_CALLBACK(conversation_dragging_cb), NULL);
	gaim_signal_connect(conv_handle, "displaying-im-msg",
						plugin, GAIM_CALLBACK(displaying_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "displayed-im-msg",
						plugin, GAIM_CALLBACK(displayed_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "displaying-chat-msg",
						plugin, GAIM_CALLBACK(displaying_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "displayed-chat-msg",
						plugin, GAIM_CALLBACK(displayed_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "conversation-switched",
						plugin, GAIM_CALLBACK(conversation_switched_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin) {
	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
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
