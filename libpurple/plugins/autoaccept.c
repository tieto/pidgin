/*
 * Autoaccept - Auto-accept file transfers from selected users
 * Copyright (C) 2006
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
#include "internal.h"

#define PLUGIN_ID			"core-plugin_pack-autoaccept"
#define PLUGIN_NAME			N_("Autoaccept")
#define PLUGIN_STATIC_NAME	"Autoaccept"
#define PLUGIN_SUMMARY		N_("Auto-accept file transfer requests from selected users.")
#define PLUGIN_DESCRIPTION	N_("Auto-accept file transfer requests from selected users.")
#define PLUGIN_AUTHOR		"Sadrul H Chowdhury <sadrul@users.sourceforge.net>"

/* System headers */
#include <glib.h>
#if GLIB_CHECK_VERSION(2,6,0)
#	include <glib/gstdio.h>
#else
#	include <sys/types.h>
#	include <sys/stat.h>
#	define	g_mkdir mkdir
#endif

/* Purple headers */
#include <plugin.h>
#include <version.h>

#include <blist.h>
#include <conversation.h>
#include <ft.h>
#include <request.h>
#include <notify.h>
#include <util.h>

#define PREF_PREFIX		"/plugins/core/" PLUGIN_ID
#define PREF_PATH		PREF_PREFIX "/path"
#define PREF_STRANGER	PREF_PREFIX "/reject_stranger"
#define PREF_NOTIFY		PREF_PREFIX "/notify"

typedef enum
{
	FT_ASK,
	FT_ACCEPT,
	FT_REJECT
} AutoAcceptSetting;

static gboolean
ensure_path_exists(const char *dir)
{
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
	{
		if (purple_build_dir(dir, S_IRUSR | S_IWUSR | S_IXUSR))
			return FALSE;
	}

	return TRUE;
}

static void
auto_accept_complete_cb(PurpleXfer *xfer, PurpleXfer *my)
{
	if (xfer == my && purple_prefs_get_bool(PREF_NOTIFY) &&
			!purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, xfer->who, xfer->account))
	{
		char *message = g_strdup_printf(_("Autoaccepted file transfer of \"%s\" from \"%s\" completed."),
					xfer->filename, xfer->who);
		purple_notify_info(NULL, _("Autoaccept complete"), message, NULL);
		g_free(message);
	}
}

static void
file_recv_request_cb(PurpleXfer *xfer, gpointer handle)
{
	PurpleAccount *account;
	PurpleBlistNode *node;
	const char *pref;
	char *filename;
	char *dirname;

	account = xfer->account;
	node = (PurpleBlistNode *)purple_find_buddy(account, xfer->who);

	if (!node)
	{
		if (purple_prefs_get_bool(PREF_STRANGER))
			xfer->status = PURPLE_XFER_STATUS_CANCEL_LOCAL;
		return;
	}

	node = node->parent;
	g_return_if_fail(PURPLE_BLIST_NODE_IS_CONTACT(node));

	pref = purple_prefs_get_string(PREF_PATH);
	switch (purple_blist_node_get_int(node, "autoaccept"))
	{
		case FT_ASK:
			break;
		case FT_ACCEPT:
			if (ensure_path_exists(pref))
			{
				int count = 1;
				const char *escape;
				dirname = g_build_filename(pref, purple_normalize(account, xfer->who), NULL);

				if (!ensure_path_exists(dirname))
				{
					g_free(dirname);
					break;
				}

				escape = purple_escape_filename(xfer->filename);
				filename = g_build_filename(dirname, escape, NULL);

				/* Make sure the file doesn't exist. Do we want some better checking than this? */
				while (g_file_test(filename, G_FILE_TEST_EXISTS)) {
					char *file = g_strdup_printf("%s-%d", escape, count++);
					g_free(filename);
					filename = g_build_filename(dirname, file, NULL);
					g_free(file);
				}

				purple_xfer_request_accepted(xfer, filename);

				g_free(dirname);
				g_free(filename);
			}

			purple_signal_connect(purple_xfers_get_handle(), "file-recv-complete", handle,
								PURPLE_CALLBACK(auto_accept_complete_cb), xfer);
			break;
		case FT_REJECT:
			xfer->status = PURPLE_XFER_STATUS_CANCEL_LOCAL;
			break;
	}
}

static void
save_cb(PurpleBlistNode *node, int choice)
{
	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		node = node->parent;
	g_return_if_fail(PURPLE_BLIST_NODE_IS_CONTACT(node));
	purple_blist_node_set_int(node, "autoaccept", choice);
}

static void
set_auto_accept_settings(PurpleBlistNode *node, gpointer plugin)
{
	char *message;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		node = node->parent;
	g_return_if_fail(PURPLE_BLIST_NODE_IS_CONTACT(node));

	message = g_strdup_printf(_("When a file-transfer request arrives from %s"), 
					purple_contact_get_alias((PurpleContact *)node));
	purple_request_choice(plugin, _("Set Autoaccept Setting"), message,
						NULL, purple_blist_node_get_int(node, "autoaccept"),
						_("_Save"), G_CALLBACK(save_cb),
						_("_Cancel"), NULL,
						NULL, NULL, NULL,
						node,
						_("Ask"), FT_ASK,
						_("Auto Accept"), FT_ACCEPT,
						_("Auto Reject"), FT_REJECT,
						NULL, purple_contact_get_alias((PurpleContact *)node), NULL,
						NULL);
	g_free(message);
}

static void
context_menu(PurpleBlistNode *node, GList **menu, gpointer plugin)
{
	PurpleMenuAction *action;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node) && !PURPLE_BLIST_NODE_IS_CONTACT(node) &&
		!(purple_blist_node_get_flags(node) & PURPLE_BLIST_NODE_FLAG_NO_SAVE))
		return;

	action = purple_menu_action_new(_("Autoaccept File Transfers..."),
					PURPLE_CALLBACK(set_auto_accept_settings), plugin, NULL);
	(*menu) = g_list_prepend(*menu, action);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_xfers_get_handle(), "file-recv-request", plugin,
						PURPLE_CALLBACK(file_recv_request_cb), plugin);
	purple_signal_connect(purple_blist_get_handle(), "blist-node-extended-menu", plugin,
						PURPLE_CALLBACK(context_menu), plugin);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	/* XXX: Is there a better way than this? There really should be. */
	pref = purple_plugin_pref_new_with_name_and_label(PREF_PATH, _("Path to save the files in\n"
								"(Please provide the full path)"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_STRANGER,
					_("Automatically reject from users not in buddy list"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_NOTIFY,
					_("Notify with a popup when an autoaccepted file transfer is complete\n"
					  "(only when there's no conversation with the sender)"));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,			/* Magic				*/
	PURPLE_MAJOR_VERSION,			/* Purple Major Version	*/
	PURPLE_MINOR_VERSION,			/* Purple Minor Version	*/
	PURPLE_PLUGIN_STANDARD,			/* plugin type			*/
	NULL,					/* ui requirement		*/
	0,					/* flags				*/
	NULL,					/* dependencies			*/
	PURPLE_PRIORITY_DEFAULT,			/* priority				*/

	PLUGIN_ID,				/* plugin id			*/
	PLUGIN_NAME,				/* name					*/
	DISPLAY_VERSION,			/* version				*/
	PLUGIN_SUMMARY,				/* summary				*/
	PLUGIN_DESCRIPTION,			/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	PURPLE_WEBSITE,				/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,					/* destroy				*/

	NULL,					/* ui_info				*/
	NULL,					/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL,					/* actions				*/

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin) {
	char *dirname;

	dirname = g_build_filename(purple_user_dir(), "autoaccept", NULL);
	purple_prefs_add_none(PREF_PREFIX);
	purple_prefs_add_string(PREF_PATH, dirname);
	purple_prefs_add_bool(PREF_STRANGER, TRUE);
	purple_prefs_add_bool(PREF_NOTIFY, TRUE);
	g_free(dirname);
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
