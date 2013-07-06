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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#define GTK_SIGNAL_TEST_PLUGIN_ID "gtk-signals-test"

#include "internal.h"

#include <gtk/gtk.h>

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
account_modified_cb(PurpleAccount *account, void *data) {
	purple_debug_info("gtk-signal-test", "account modified cb\n");
}

/**************************************************************************
 * Buddy List subsystem signal callbacks
 **************************************************************************/
static void
blist_created_cb(PurpleBuddyList *blist, void *data) {
	purple_debug_info("gtk-signal-test", "buddy list created\n");
}

static void
blist_drawing_tooltip_cb(PurpleBListNode *node, GString *str, gboolean full, void *data) {
	purple_debug_info("gtk-signal-test", "drawing tooltip cb\n");
}

/**************************************************************************
 * Conversation subsystem signal callbacks
 **************************************************************************/
static void
conversation_dragging_cb(PidginWindow *source, PidginWindow *destination) {
	purple_debug_info("gtk-signal-test", "conversation dragging cb\n");
}

static gboolean
displaying_im_msg_cb(PurpleAccount *account, const char *who, char **buffer,
				PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	purple_debug_misc("gtk-signals test", "displaying-im-msg (%s, %s)\n",
					purple_conversation_get_name(conv), *buffer);

	return FALSE;
}

static void
displayed_im_msg_cb(PurpleAccount *account, const char *who, const char *buffer,
				PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	purple_debug_misc("gtk-signals test", "displayed-im-msg (%s, %s)\n",
					purple_conversation_get_name(conv), buffer);
}

static gboolean
displaying_chat_msg_cb(PurpleAccount *account, const char *who, char **buffer,
				PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	purple_debug_misc("gtk-signals test", "displaying-chat-msg (%s, %s)\n",
					purple_conversation_get_name(conv), *buffer);

	return FALSE;
}

static void
displayed_chat_msg_cb(PurpleAccount *account, const char *who, const char *buffer,
				PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	purple_debug_misc("gtk-signals test", "displayed-chat-msg (%s, %s)\n",
					purple_conversation_get_name(conv), buffer);
}

static void
conversation_switched_cb(PurpleConversation *conv, void *data)
{
	purple_debug_misc("gtk-signals test", "conversation-switched (%s)\n",
					purple_conversation_get_name(conv));
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *accounts_handle = pidgin_accounts_get_handle();
	void *blist_handle = pidgin_blist_get_handle();
	void *conv_handle = pidgin_conversations_get_handle();

	/* Accounts subsystem signals */
	purple_signal_connect(accounts_handle, "account-modified",
						plugin, PURPLE_CALLBACK(account_modified_cb), NULL);

	/* Buddy List subsystem signals */
	purple_signal_connect(blist_handle, "gtkblist-created",
						plugin, PURPLE_CALLBACK(blist_created_cb), NULL);
	purple_signal_connect(blist_handle, "drawing-tooltip",
						plugin, PURPLE_CALLBACK(blist_drawing_tooltip_cb), NULL);

	/* Conversations subsystem signals */
	purple_signal_connect(conv_handle, "conversation-dragging",
						plugin, PURPLE_CALLBACK(conversation_dragging_cb), NULL);
	purple_signal_connect(conv_handle, "displaying-im-msg",
						plugin, PURPLE_CALLBACK(displaying_im_msg_cb), NULL);
	purple_signal_connect(conv_handle, "displayed-im-msg",
						plugin, PURPLE_CALLBACK(displayed_im_msg_cb), NULL);
	purple_signal_connect(conv_handle, "displaying-chat-msg",
						plugin, PURPLE_CALLBACK(displaying_chat_msg_cb), NULL);
	purple_signal_connect(conv_handle, "displayed-chat-msg",
						plugin, PURPLE_CALLBACK(displayed_chat_msg_cb), NULL);
	purple_signal_connect(conv_handle, "conversation-switched",
						plugin, PURPLE_CALLBACK(conversation_switched_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin) {
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	GTK_SIGNAL_TEST_PLUGIN_ID,                        /**< id             */
	N_("GTK Signals Test"),                             /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Test to see that all ui signals are working properly."),
	                                                  /**  description    */
	N_("Test to see that all ui signals are working properly."),
	"Gary Kramlich <amc_grim@users.sf.net>",              /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(gtksignalstest, init_plugin, info)
