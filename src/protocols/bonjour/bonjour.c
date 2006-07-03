/*
 * gaim - Bonjour Protocol Plugin
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <glib.h>
#ifndef _WIN32
#include <pwd.h>
#else
#define UNICODE
#include <windows.h>
#include <lm.h>
#endif

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "version.h"

#include "bonjour.h"
#include "dns_sd.h"
#include "jabber.h"
#include "buddy.h"

/*
 * TODO: Should implement an add_buddy callback that removes the buddy
 *       from the local list.  Bonjour manages buddies for you, and
 *       adding someone locally by hand is stupid.  Or, maybe even better,
 *       if a PRPL does not have an add_buddy callback then do not allow
 *       users to add buddies.
 */

static char *default_firstname;
static char *default_lastname;
static char *default_hostname;

static void
bonjour_removeallfromlocal(GaimConnection *gc)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;

	blist = gaim_get_blist();
	if (blist == NULL)
		return;

	/* Go through and remove all buddies that belong to this account */
	for (gnode = blist->root; gnode; gnode = gnode->next)
	{
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for (bnode = cnode->child; bnode; bnode = bnode->next)
			{
				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				buddy = (GaimBuddy *)bnode;
				if (buddy->account != account)
					continue;
				gaim_prpl_got_user_status(account, buddy->name, "offline", NULL);
				gaim_blist_remove_buddy(buddy);
			}
		}
	}
}

static void
bonjour_login(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimGroup *bonjour_group = NULL;
	BonjourData *bd = NULL;
	GaimStatus *status;
	GaimPresence *presence;

	gc->flags |= GAIM_CONNECTION_HTML;
	gc->proto_data = g_new0(BonjourData, 1);
	bd = gc->proto_data;

	/* Start waiting for jabber connections (iChat style) */
	bd->jabber_data = g_new(BonjourJabber, 1);
	bd->jabber_data->port = BONJOUR_DEFAULT_PORT_INT;
	bd->jabber_data->account = account;

	if (bonjour_jabber_start(bd->jabber_data) == -1) {
		/* Send a message about the connection error */
		gaim_connection_error(gc, _("Unable to listen for incoming IM connections\n"));

		/* Free the data */
		g_free(bd->jabber_data);
		bd->jabber_data = NULL;
		return;
	}

	/* Connect to the mDNS daemon looking for buddies in the LAN */
	bd->dns_sd_data = bonjour_dns_sd_new();
	bd->dns_sd_data->name = (sw_string)gaim_account_get_username(account);
	bd->dns_sd_data->txtvers = g_strdup("1");
	bd->dns_sd_data->version = g_strdup("1");
	bd->dns_sd_data->first = g_strdup(gaim_account_get_string(account, "first", default_firstname));
	bd->dns_sd_data->last = g_strdup(gaim_account_get_string(account, "last", default_lastname));
	bd->dns_sd_data->port_p2pj = bd->jabber_data->port;
	bd->dns_sd_data->phsh = g_strdup("");
	bd->dns_sd_data->email = g_strdup(gaim_account_get_string(account, "email", ""));
	bd->dns_sd_data->vc = g_strdup("");
	bd->dns_sd_data->jid = g_strdup(gaim_account_get_string(account, "jid", ""));
	bd->dns_sd_data->AIM = g_strdup(gaim_account_get_string(account, "AIM", ""));

	status = gaim_account_get_active_status(account);
	presence = gaim_account_get_presence(account);
	if (gaim_presence_is_available(presence))
		bd->dns_sd_data->status = g_strdup("avail");
	else if (gaim_presence_is_idle(presence))
		bd->dns_sd_data->status = g_strdup("away");
	else
		bd->dns_sd_data->status = g_strdup("dnd");
	bd->dns_sd_data->msg = g_strdup(gaim_status_get_attr_string(status, "message"));

	bd->dns_sd_data->account = account;
	if (!bonjour_dns_sd_start(bd->dns_sd_data))
	{
		gaim_connection_error(gc, _("Unable to establish connection with the local mDNS server.  Is it running?"));
		return;
	}

	/* Create a group for bonjour buddies */
	bonjour_group = gaim_group_new(BONJOUR_GROUP_NAME);
	gaim_blist_add_group(bonjour_group, NULL);

	/* Show the buddy list by telling Gaim we have already connected */
	gaim_connection_set_state(gc, GAIM_CONNECTED);
}

static void
bonjour_close(GaimConnection *connection)
{
	GaimGroup *bonjour_group;
	BonjourData *bd = (BonjourData*)connection->proto_data;

	/* Stop looking for buddies in the LAN */
	if (bd->dns_sd_data != NULL)
	{
		bonjour_dns_sd_stop(bd->dns_sd_data);
		bonjour_dns_sd_free(bd->dns_sd_data);
	}

	if (bd->jabber_data != NULL)
	{
		/* Stop waiting for conversations */
		bonjour_jabber_stop(bd->jabber_data);
		g_free(bd->jabber_data);
	}

	/* Remove all the bonjour buddies */
	bonjour_removeallfromlocal(connection);

	/* Delete the bonjour group */
	bonjour_group = gaim_find_group(BONJOUR_GROUP_NAME);
	if (bonjour_group != NULL)
		gaim_blist_remove_group(bonjour_group);

}

static const char *
bonjour_list_icon(GaimAccount *account, GaimBuddy *buddy)
{
	return BONJOUR_ICON_NAME;
}

static int
bonjour_send_im(GaimConnection *connection, const char *to, const char *msg, GaimMessageFlags flags)
{
	if(!to || !msg)
		return 0;

	return bonjour_jabber_send_message(((BonjourData*)(connection->proto_data))->jabber_data, to, msg);
}

static void
bonjour_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc;
	BonjourData *bd;
	gboolean disconnected;
	GaimStatusType *type;
	int primitive;
	GaimPresence *presence;
	const char *message, *bonjour_status;
	gchar *stripped;

	gc = gaim_account_get_connection(account);
	bd = gc->proto_data;
	disconnected = gaim_account_is_disconnected(account);
	type = gaim_status_get_type(status);
	primitive = gaim_status_type_get_primitive(type);
	presence = gaim_account_get_presence(account);

	message = gaim_status_get_attr_string(status, "message");
	if (message == NULL)
		message = "";
	stripped = gaim_markup_strip_html(message);

	/*
	 * The three possible status for Bonjour are
	 *   -available ("avail")
	 *   -idle ("away")
	 *   -away ("dnd")
	 * Each of them can have an optional message.
	 */
	if (gaim_presence_is_available(presence))
		bonjour_status = "avail";
	else if (gaim_presence_is_idle(presence))
		bonjour_status = "away";
	else
		bonjour_status = "dnd";

	bonjour_dns_sd_send_status(bd->dns_sd_data, bonjour_status, stripped);
	g_free(stripped);
}

static GList *
bonjour_status_types(GaimAccount *account)
{
	GList *status_types = NULL;
	GaimStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AVAILABLE,
										   BONJOUR_STATUS_ID_AVAILABLE,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AWAY,
										   BONJOUR_STATUS_ID_AWAY,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
									 BONJOUR_STATUS_ID_OFFLINE,
									 NULL, TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	return status_types;
}

static void
bonjour_convo_closed(GaimConnection *connection, const char *who)
{
	GaimBuddy *buddy = gaim_find_buddy(connection->account, who);

	if (buddy == NULL)
	{
		/*
		 * This buddy is not in our buddy list, and therefore does not really
		 * exist, so we won't have any data about them.
		 */
		return;
	}

	bonjour_jabber_close_conversation(((BonjourData*)(connection->proto_data))->jabber_data, buddy);
}

static void
bonjour_list_emblems(GaimBuddy *buddy,
								 const char **se, const char **sw,
								 const char **nw, const char **ne)
{
	GaimPresence *presence;

	presence = gaim_buddy_get_presence(buddy);

	if (gaim_presence_is_online(presence) && !gaim_presence_is_available(presence))
		*se = "away";
}

static char *
bonjour_status_text(GaimBuddy *buddy)
{
	GaimPresence *presence;

	presence = gaim_buddy_get_presence(buddy);

	if (gaim_presence_is_online(presence) && !gaim_presence_is_available(presence))
		return g_strdup("Away");

	return NULL;
}

static void
bonjour_tooltip_text(GaimBuddy *buddy, GString *str, gboolean full)
{
	GaimPresence *presence;
	GaimStatus *status;
	const char *status_description;
	const char *message;

	presence = gaim_buddy_get_presence(buddy);
	status = gaim_presence_get_active_status(presence);
	message = gaim_status_get_attr_string(status, "message");

	if (gaim_presence_is_available(presence))
		status_description = gaim_status_get_name(status);
	else if (gaim_presence_is_idle(presence))
		status_description = _("Idle");
	else
		status_description = gaim_status_get_name(status);

	g_string_append_printf(str, _("\n<b>Status:</b> %s"), status_description);
	if (message != NULL)
		g_string_append_printf(str, _("\n<b>Message:</b> %s"), message);
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	g_free(default_firstname);
	g_free(default_lastname);
	g_free(default_hostname);

	return TRUE;
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_NO_PASSWORD,
	NULL,                                                    /* user_splits */
	NULL,                                                    /* protocol_options */
	/* {"png", 0, 0, 96, 96, GAIM_ICON_SCALE_DISPLAY}, */    /* icon_spec */
	NO_BUDDY_ICONS, /* not yet */                            /* icon_spec */
	bonjour_list_icon,                                       /* list_icon */
	bonjour_list_emblems,                                    /* list_emblems */
	bonjour_status_text,                                     /* status_text */
	bonjour_tooltip_text,                                    /* tooltip_text */
	bonjour_status_types,                                    /* status_types */
	NULL,                                                    /* blist_node_menu */
	NULL,                                                    /* chat_info */
	NULL,                                                    /* chat_info_defaults */
	bonjour_login,                                           /* login */
	bonjour_close,                                           /* close */
	bonjour_send_im,                                         /* send_im */
	NULL,                                                    /* set_info */
	NULL,                                                    /* send_typing */
	NULL,                                                    /* get_info */
	bonjour_set_status,                                      /* set_status */
	NULL,                                                    /* set_idle */
	NULL,                                                    /* change_passwd */
	NULL,                                                    /* add_buddy */
	NULL,                                                    /* add_buddies */
	NULL,                                                    /* remove_buddy */
	NULL,                                                    /* remove_buddies */
	NULL,                                                    /* add_permit */
	NULL,                                                    /* add_deny */
	NULL,                                                    /* rem_permit */
	NULL,                                                    /* rem_deny */
	NULL,                                                    /* set_permit_deny */
	NULL,                                                    /* join_chat */
	NULL,                                                    /* reject_chat */
	NULL,                                                    /* get_chat_name */
	NULL,                                                    /* chat_invite */
	NULL,                                                    /* chat_leave */
	NULL,                                                    /* chat_whisper */
	NULL,                                                    /* chat_send */
	NULL,                                                    /* keepalive */
	NULL,                                                    /* register_user */
	NULL,                                                    /* get_cb_info */
	NULL,                                                    /* get_cb_away */
	NULL,                                                    /* alias_buddy */
	NULL,                                                    /* group_buddy */
	NULL,                                                    /* rename_group */
	NULL,                                                    /* buddy_free */
	bonjour_convo_closed,                                    /* convo_closed */
	NULL,                                                    /* normalize */
	NULL,                                                    /* set_buddy_icon */
	NULL,                                                    /* remove_group */
	NULL,                                                    /* get_cb_real_name */
	NULL,                                                    /* set_chat_topic */
	NULL,                                                    /* find_blist_chat */
	NULL,                                                    /* roomlist_get_list */
	NULL,                                                    /* roomlist_cancel */
	NULL,                                                    /* roomlist_expand_category */
	NULL,                                                    /* can_receive_file */
	NULL,                                                    /* send_file */
	NULL,                                                    /* new_xfer */
	NULL,                                                    /* offline_message */
	NULL,                                                    /* whiteboard_prpl_ops */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-bonjour",                                   /**< id             */
	"Bonjour",                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Bonjour Protocol Plugin"),
	                                                  /**  description    */
	N_("Bonjour Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	plugin_unload,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL
};

static void
initialize_default_account_values()
{
#ifdef _WIN32
	char *fullname = NULL;
#else
	struct passwd *info;
	const char *fullname = NULL;
#endif
	char *splitpoint = NULL;
	char hostname[255];

#ifndef _WIN32
	/* Try to figure out the user's real name */
	info = getpwuid(getuid());
	if ((info != NULL) && (info->pw_gecos != NULL) && (info->pw_gecos[0] != '\0'))
		fullname = info->pw_gecos;
	else if ((info != NULL) && (info->pw_name != NULL) && (info->pw_name[0] != '\0'))
		fullname = info->pw_name;
	else if (((fullname = getlogin()) != NULL) && (fullname[0] != '\0'))
		;
	else
		fullname = _("Gaim User");

	/* Make sure fullname is valid UTF-8.  If not, try to convert it. */
	if (!g_utf8_validate(fullname, -1, NULL))
	{
		gchar *tmp;
		tmp = g_locale_to_utf8(fullname, -1, NULL, NULL, NULL);
		if ((tmp == NULL) || (*tmp == '\0'))
			fullname = _("Gaim User");
	}

#else
	FARPROC myNetUserGetInfo = wgaim_find_and_loadproc("Netapi32.dll",
		"NetUserGetInfo");

	if (myNetUserGetInfo) {
		LPUSER_INFO_10 user_info = NULL;
		LPSERVER_INFO_100 server_info = NULL;
		wchar_t *servername = NULL;
		wchar_t username[UNLEN + 1] = {'\0'};
		DWORD dwLenUsername = sizeof(username);
		FARPROC myNetServerEnum = wgaim_find_and_loadproc(
			"Netapi32.dll", "NetServerEnum");
		FARPROC myNetApiBufferFree = wgaim_find_and_loadproc(
			"Netapi32.dll", "NetApiBufferFree");

		if (myNetServerEnum && myNetApiBufferFree) {
			DWORD dwEntriesRead = 0;
			DWORD dwTotalEntries = 0;
			DWORD dwResumeHandle = 0;

			NET_API_STATUS nStatus = (myNetServerEnum)(NULL, 100,
				&server_info, MAX_PREFERRED_LENGTH,
				&dwEntriesRead, &dwTotalEntries,
				SV_TYPE_DOMAIN_CTRL, NULL, &dwResumeHandle);

			if ((nStatus == NERR_Success
					|| nStatus == ERROR_MORE_DATA)
					&& dwEntriesRead > 0) {
				servername = server_info->sv100_name;
			} else {
				gaim_debug_warning("bonjour", "Unable to look up domain controller. NET_API_STATUS = %d, Entries Read = %d, Total Entries = %d\n", nStatus, dwEntriesRead, dwTotalEntries);
			}
		}

		if (!GetUserNameW(&username, &dwLenUsername)) {
			gaim_debug_warning("bonjour",
				"Unable to look up username\n");
		}

		if (username != NULL && *username != '\0'
				&& (myNetUserGetInfo)(servername, username, 10,
					&user_info) == NERR_Success) {
			if (user_info != NULL) {
				fullname = g_utf16_to_utf8(
					user_info->usri10_full_name,
					-1, NULL, NULL, NULL);
			}
		}
		if (user_info != NULL)
			(myNetApiBufferFree)(user_info);
		if (server_info != NULL)
			(myNetApiBufferFree)(server_info);
	}

	if (!fullname)
		fullname = g_strdup(_("Gaim User"));
#endif

	/* Split the real name into a first and last name */
	splitpoint = strchr(fullname, ' ');
	if (splitpoint != NULL)
	{
		default_firstname = g_strndup(fullname, splitpoint - fullname);
		default_lastname = g_strdup(&splitpoint[1]);
	}
	else
	{
		default_firstname = g_strdup(fullname);
		default_lastname = g_strdup("");
	}

#ifdef _WIN32
	g_free(fullname);
#endif

	/* Try to figure out a good host name to use */
	/* TODO: Avoiding 'localhost,' if possible */
	if (gethostname(hostname, 255) != 0) {
		gaim_debug_warning("bonjour", "Error %d when getting host name.  Using \"localhost.\"\n", errno);
		strcpy(hostname, "localhost");
	}
	default_hostname = g_strdup(hostname);
}

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	initialize_default_account_values();

	/* Creating the user splits */
	split = gaim_account_user_split_new(_("Hostname"), default_hostname, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	/* Creating the options for the protocol */
	option = gaim_account_option_string_new(_("First name"), "first", default_firstname);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Last name"), "last", default_lastname);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("E-mail"), "email", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("AIM Account"), "AIM", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Jabber Account"), "jid", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(bonjour, init_plugin, info);
