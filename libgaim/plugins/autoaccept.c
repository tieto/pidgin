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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

/* Gaim headers */
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
		if (gaim_build_dir(dir, S_IRUSR | S_IWUSR | S_IXUSR))
			return FALSE;
	}

	return TRUE;
}

static void
auto_accept_complete_cb(GaimXfer *xfer, GaimXfer *my)
{
	if (xfer == my && gaim_prefs_get_bool(PREF_NOTIFY) &&
			!gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, xfer->who, xfer->account))
	{
		char *message = g_strdup_printf(_("Autoaccepted file transfer of \"%s\" from \"%s\" completed."),
					xfer->filename, xfer->who);
		gaim_notify_info(NULL, _("Autoaccept complete"), message, NULL);
		g_free(message);
	}
}

static void
file_recv_request_cb(GaimXfer *xfer, gpointer handle)
{
	GaimAccount *account;
	GaimBlistNode *node;
	const char *pref;
	char *filename;
	char *dirname;

	account = xfer->account;
	node = (GaimBlistNode *)gaim_find_buddy(account, xfer->who);

	if (!node)
	{
		if (gaim_prefs_get_bool(PREF_STRANGER))
			xfer->status = GAIM_XFER_STATUS_CANCEL_LOCAL;
		return;
	}

	node = node->parent;
	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT(node));

	pref = gaim_prefs_get_string(PREF_PATH);
	switch (gaim_blist_node_get_int(node, "autoaccept"))
	{
		case FT_ASK:
			break;
		case FT_ACCEPT:
			if (ensure_path_exists(pref))
			{
				dirname = g_build_filename(pref, xfer->who, NULL);

				if (!ensure_path_exists(dirname))
				{
					g_free(dirname);
					break;
				}
				
				filename = g_build_filename(dirname, xfer->filename, NULL);

				gaim_xfer_request_accepted(xfer, filename);

				g_free(dirname);
				g_free(filename);
			}
			
			gaim_signal_connect(gaim_xfers_get_handle(), "file-recv-complete", handle,
								GAIM_CALLBACK(auto_accept_complete_cb), xfer);
			break;
		case FT_REJECT:
			xfer->status = GAIM_XFER_STATUS_CANCEL_LOCAL;
			break;
	}
}

static void
save_cb(GaimBlistNode *node, int choice)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		node = node->parent;
	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT(node));
	gaim_blist_node_set_int(node, "autoaccept", choice);
}

static void
set_auto_accept_settings(GaimBlistNode *node, gpointer plugin)
{
	char *message;

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		node = node->parent;
	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT(node));

	message = g_strdup_printf(_("When a file-transfer request arrives from %s"), 
					gaim_contact_get_alias((GaimContact *)node));
	gaim_request_choice(plugin, _("Set Autoaccept Setting"), message,
						NULL, gaim_blist_node_get_int(node, "autoaccept"),
						_("_Save"), G_CALLBACK(save_cb),
						_("_Cancel"), NULL, node,
						_("Ask"), FT_ASK,
						_("Auto Accept"), FT_ACCEPT,
						_("Auto Reject"), FT_REJECT,
						NULL);
	g_free(message);
}

static void
context_menu(GaimBlistNode *node, GList **menu, gpointer plugin)
{
	GaimMenuAction *action;

	if (!GAIM_BLIST_NODE_IS_BUDDY(node) && !GAIM_BLIST_NODE_IS_CONTACT(node))
		return;

	action = gaim_menu_action_new(_("Autoaccept File Transfers..."),
					GAIM_CALLBACK(set_auto_accept_settings), plugin, NULL);
	(*menu) = g_list_prepend(*menu, action);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_xfers_get_handle(), "file-recv-request", plugin,
						GAIM_CALLBACK(file_recv_request_cb), plugin);
	gaim_signal_connect(gaim_blist_get_handle(), "blist-node-extended-menu", plugin,
						GAIM_CALLBACK(context_menu), plugin);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *pref;

	frame = gaim_plugin_pref_frame_new();

	/* XXX: Is there a better way than this? There really should be. */
	pref = gaim_plugin_pref_new_with_name_and_label(PREF_PATH, _("Path to save the files in\n"
								"(Please provide the full path)"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREF_STRANGER,
					_("Automatically reject from users not in buddy list"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREF_NOTIFY,
					_("Notify with a popup when an autoaccepted file transfer is complete\n"
					  "(only when there's no conversation with the sender)"));
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL,
};

static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,			/* Magic				*/
	GAIM_MAJOR_VERSION,			/* Gaim Major Version	*/
	GAIM_MINOR_VERSION,			/* Gaim Minor Version	*/
	GAIM_PLUGIN_STANDARD,			/* plugin type			*/
	NULL,					/* ui requirement		*/
	0,					/* flags				*/
	NULL,					/* dependencies			*/
	GAIM_PRIORITY_DEFAULT,			/* priority				*/

	PLUGIN_ID,				/* plugin id			*/
	PLUGIN_NAME,				/* name					*/
	VERSION,				/* version				*/
	PLUGIN_SUMMARY,				/* summary				*/
	PLUGIN_DESCRIPTION,			/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	GAIM_WEBSITE,				/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,					/* destroy				*/

	NULL,					/* ui_info				*/
	NULL,					/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL					/* actions				*/
};

static void
init_plugin(GaimPlugin *plugin) {
	char *dirname;

	dirname = g_build_filename(gaim_user_dir(), "autoaccept", NULL);
	gaim_prefs_add_none(PREF_PREFIX);
	gaim_prefs_add_string(PREF_PATH, dirname);
	gaim_prefs_add_bool(PREF_STRANGER, TRUE);
	gaim_prefs_add_bool(PREF_NOTIFY, TRUE);
	g_free(dirname);
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
