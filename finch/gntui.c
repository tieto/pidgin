/**
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "finch.h"
#include <internal.h>

#include "gntui.h"

#include "gntaccount.h"
#include "gntbuddylist.h"
#include "gntcertmgr.h"
#include "gntconn.h"
#include "gntconv.h"
#include "gntdebug.h"
#include "gntft.h"
#include "gntlog.h"
#include "gntmedia.h"
#include "gntnotify.h"
#include "gntplugin.h"
#include "gntpounce.h"
#include "gntprefs.h"
#include "gntrequest.h"
#include "gntroomlist.h"
#include "gntstatus.h"
#include "gntsound.h"

#include <prefs.h>

void gnt_ui_init()
{
#ifdef STANDALONE
	gnt_init();
#endif

	purple_prefs_add_none("/purple/gnt");

	/* Accounts */
	finch_accounts_init();
	purple_accounts_set_ui_ops(finch_accounts_get_ui_ops());

	/* Connections */
	finch_connections_init();
	purple_connections_set_ui_ops(finch_connections_get_ui_ops());

	/* Initialize the buddy list */
	finch_blist_init();
	purple_blist_set_ui_ops(finch_blist_get_ui_ops());

	/* Initialize sound */
	purple_sound_set_ui_ops(finch_sound_get_ui_ops());

	/* Now the conversations */
	finch_conversation_init();
	purple_conversations_set_ui_ops(finch_conv_get_ui_ops());

	/* Notify */
	finch_notify_init();
	purple_notify_set_ui_ops(finch_notify_get_ui_ops());

	/* Request */
	finch_request_init();
	purple_request_set_ui_ops(finch_request_get_ui_ops());

	/* Pounce */
	finch_pounces_init();

	/* Log */
	finch_log_init();

	/* File transfer */
	finch_xfers_init();
	purple_xfers_set_ui_ops(finch_xfers_get_ui_ops());

	/* Roomlist */
	finch_roomlist_init();
	purple_roomlist_set_ui_ops(finch_roomlist_get_ui_ops());

	/* Media */
	finch_media_manager_init();

	gnt_register_action(_("Accounts"), finch_accounts_show_all);
	gnt_register_action(_("Buddy List"), finch_blist_show);
	gnt_register_action(_("Buddy Pounces"), finch_pounces_manager_show);
	gnt_register_action(_("Certificates"), finch_certmgr_show);
	gnt_register_action(_("Debug Window"), finch_debug_window_show);
	gnt_register_action(_("File Transfers"), finch_xfer_dialog_show);
	gnt_register_action(_("Plugins"), finch_plugins_show_all);
	gnt_register_action(_("Room List"), finch_roomlist_show_all);
	gnt_register_action(_("Sounds"), finch_sounds_show_all);
	gnt_register_action(_("Preferences"), finch_prefs_show_all);
	gnt_register_action(_("Keyring settings"), finch_prefs_show_keyring);
	gnt_register_action(_("Statuses"), finch_savedstatus_show_all);

#ifdef STANDALONE
}

void gnt_ui_uninit()
{
	purple_accounts_set_ui_ops(NULL);
	finch_accounts_uninit();

	purple_connections_set_ui_ops(NULL);
	finch_connections_uninit();

	purple_blist_set_ui_ops(NULL);
	finch_blist_uninit();

	purple_conversations_set_ui_ops(NULL);
	finch_conversation_uninit();

	purple_notify_set_ui_ops(NULL);
	finch_notify_uninit();

	purple_request_set_ui_ops(NULL);
	finch_request_uninit();

	finch_pounces_uninit();

	finch_log_uninit();

	finch_xfers_uninit();
	purple_xfers_set_ui_ops(NULL);

	finch_roomlist_uninit();
	purple_roomlist_set_ui_ops(NULL);

#ifdef USE_VV
	finch_media_manager_uninit();
#endif

	gnt_quit();
#endif
}

