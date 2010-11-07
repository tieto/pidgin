/*
 * purple - Bonjour Protocol Plugin
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#include <glib.h>
#ifndef _WIN32
#include <pwd.h>
#else
#define UNICODE
#include <winsock2.h>
#include <windows.h>
#include <lm.h>
#include "dns_sd_proxy.h"
#endif

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "version.h"

#include "bonjour.h"
#include "mdns_common.h"
#include "jabber.h"
#include "buddy.h"
#include "bonjour_ft.h"

static char *default_firstname;
static char *default_lastname;
static char *default_hostname;

static void
bonjour_removeallfromlocal(PurpleConnection *conn, PurpleGroup *bonjour_group)
{
	PurpleAccount *account = purple_connection_get_account(conn);
	PurpleBlistNode *cnode, *cnodenext, *bnode, *bnodenext;
	PurpleBuddy *buddy;

	if (bonjour_group == NULL)
		return;

	/* Go through and remove all buddies that belong to this account */
	for (cnode = ((PurpleBlistNode *) bonjour_group)->child; cnode; cnode = cnodenext) {
		cnodenext = cnode->next;
		if (!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
			continue;
		for (bnode = cnode->child; bnode; bnode = bnodenext) {
			bnodenext = bnode->next;
			if (!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			buddy = (PurpleBuddy *) bnode;
			if (buddy->account != account)
				continue;
			purple_prpl_got_user_status(account, buddy->name, "offline", NULL);
			purple_account_remove_buddy(account, buddy, NULL);
			purple_blist_remove_buddy(buddy);
		}
	}

}

static void
bonjour_login(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	BonjourData *bd;
	PurpleStatus *status;
	PurplePresence *presence;

#ifdef _WIN32
	if (!dns_sd_available()) {
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_OTHER_ERROR,
			_("The Apple Bonjour For Windows toolkit wasn't found, see the FAQ at: "
			  "http://d.pidgin.im/BonjourWindows"
			  " for more information."));
		return;
	}
#endif

	gc->flags |= PURPLE_CONNECTION_HTML;
	gc->proto_data = bd = g_new0(BonjourData, 1);

	/* Start waiting for jabber connections (iChat style) */
	bd->jabber_data = g_new0(BonjourJabber, 1);
	bd->jabber_data->port = BONJOUR_DEFAULT_PORT_INT;
	bd->jabber_data->account = account;

	if (bonjour_jabber_start(bd->jabber_data) == -1) {
		/* Send a message about the connection error */
		purple_connection_error_reason (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to listen for incoming IM connections\n"));
		return;
	}

	/* Connect to the mDNS daemon looking for buddies in the LAN */
	bd->dns_sd_data = bonjour_dns_sd_new();
	bd->dns_sd_data->first = g_strdup(purple_account_get_string(account, "first", default_firstname));
	bd->dns_sd_data->last = g_strdup(purple_account_get_string(account, "last", default_lastname));
	bd->dns_sd_data->port_p2pj = bd->jabber_data->port;
	/* Not engaged in AV conference */
	bd->dns_sd_data->vc = g_strdup("!");

	status = purple_account_get_active_status(account);
	presence = purple_account_get_presence(account);
	if (purple_presence_is_available(presence))
		bd->dns_sd_data->status = g_strdup("avail");
	else if (purple_presence_is_idle(presence))
		bd->dns_sd_data->status = g_strdup("away");
	else
		bd->dns_sd_data->status = g_strdup("dnd");
	bd->dns_sd_data->msg = g_strdup(purple_status_get_attr_string(status, "message"));

	bd->dns_sd_data->account = account;
	if (!bonjour_dns_sd_start(bd->dns_sd_data))
	{
		purple_connection_error_reason (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to establish connection with the local mDNS server.  Is it running?"));
		return;
	}

	bonjour_dns_sd_update_buddy_icon(bd->dns_sd_data);

	/* Show the buddy list by telling Purple we have already connected */
	purple_connection_set_state(gc, PURPLE_CONNECTED);
}

static void
bonjour_close(PurpleConnection *connection)
{
	PurpleGroup *bonjour_group;
	BonjourData *bd = connection->proto_data;

	bonjour_group = purple_find_group(BONJOUR_GROUP_NAME);

	/* Remove all the bonjour buddies */
	bonjour_removeallfromlocal(connection, bonjour_group);

	/* Stop looking for buddies in the LAN */
	if (bd != NULL && bd->dns_sd_data != NULL)
	{
		bonjour_dns_sd_stop(bd->dns_sd_data);
		bonjour_dns_sd_free(bd->dns_sd_data);
	}

	if (bd != NULL && bd->jabber_data != NULL)
	{
		/* Stop waiting for conversations */
		bonjour_jabber_stop(bd->jabber_data);
		g_free(bd->jabber_data);
	}

	/* Delete the bonjour group */
	if (bonjour_group != NULL)
		purple_blist_remove_group(bonjour_group);

	/* Cancel any file transfers */
	while (bd != NULL && bd->xfer_lists) {
		purple_xfer_cancel_local(bd->xfer_lists->data);
	}

	g_free(bd);
	connection->proto_data = NULL;
}

static const char *
bonjour_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return BONJOUR_ICON_NAME;
}

static int
bonjour_send_im(PurpleConnection *connection, const char *to, const char *msg, PurpleMessageFlags flags)
{
	if(!to || !msg)
		return 0;

	return bonjour_jabber_send_message(((BonjourData*)(connection->proto_data))->jabber_data, to, msg);
}

static void
bonjour_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	BonjourData *bd;
	gboolean disconnected;
	PurpleStatusType *type;
	int primitive;
	PurplePresence *presence;
	const char *message, *bonjour_status;
	gchar *stripped;

	gc = purple_account_get_connection(account);
	bd = gc->proto_data;
	disconnected = purple_account_is_disconnected(account);
	type = purple_status_get_type(status);
	primitive = purple_status_type_get_primitive(type);
	presence = purple_account_get_presence(account);

	message = purple_status_get_attr_string(status, "message");
	if (message == NULL)
		message = "";
	stripped = purple_markup_strip_html(message);

	/*
	 * The three possible status for Bonjour are
	 *   -available ("avail")
	 *   -idle ("away")
	 *   -away ("dnd")
	 * Each of them can have an optional message.
	 */
	if (purple_presence_is_available(presence))
		bonjour_status = "avail";
	else if (purple_presence_is_idle(presence))
		bonjour_status = "away";
	else
		bonjour_status = "dnd";

	bonjour_dns_sd_send_status(bd->dns_sd_data, bonjour_status, stripped);
	g_free(stripped);
}

/*
 * The add_buddy callback removes the buddy from the local list.
 * Bonjour manages buddies for you, and adding someone locally by
 * hand is stupid.  Perhaps we should change libpurple not to allow adding
 * if there is no add_buddy callback.
 */
static void
bonjour_fake_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group) {
	purple_debug_error("bonjour", "Buddy '%s' manually added; removing.  "
				      "Bonjour buddies must be discovered and not manually added.\n",
			   purple_buddy_get_name(buddy));

	/* I suppose we could alert the user here, but it seems unnecessary. */

	/* If this causes problems, it can be moved to an idle callback */
	purple_blist_remove_buddy(buddy);
}


static void bonjour_remove_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group) {
	if (buddy->proto_data) {
		bonjour_buddy_delete(buddy->proto_data);
		buddy->proto_data = NULL;
	}
}

static GList *
bonjour_status_types(PurpleAccount *account)
{
	GList *status_types = NULL;
	PurpleStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
										   BONJOUR_STATUS_ID_AVAILABLE,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   purple_value_new(PURPLE_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY,
										   BONJOUR_STATUS_ID_AWAY,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   purple_value_new(PURPLE_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
									 BONJOUR_STATUS_ID_OFFLINE,
									 NULL, TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	return status_types;
}

static void
bonjour_convo_closed(PurpleConnection *connection, const char *who)
{
	PurpleBuddy *buddy = purple_find_buddy(connection->account, who);
	BonjourBuddy *bb;

	if (buddy == NULL || buddy->proto_data == NULL)
	{
		/*
		 * This buddy is not in our buddy list, and therefore does not really
		 * exist, so we won't have any data about them.
		 */
		return;
	}

	bb = buddy->proto_data;
	bonjour_jabber_close_conversation(bb->conversation);
	bb->conversation = NULL;
}

static
void bonjour_set_buddy_icon(PurpleConnection *conn, PurpleStoredImage *img)
{
	BonjourData *bd = conn->proto_data;
	bonjour_dns_sd_update_buddy_icon(bd->dns_sd_data);
}


static char *
bonjour_status_text(PurpleBuddy *buddy)
{
	const PurplePresence *presence;
	const PurpleStatus *status;
	const char *message;
	gchar *ret = NULL;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	message = purple_status_get_attr_string(status, "message");

	if (message != NULL) {
		ret = g_markup_escape_text(message, -1);
		purple_util_chrreplace(ret, '\n', ' ');
	}

	return ret;
}

static void
bonjour_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;
	BonjourBuddy *bb = buddy->proto_data;
	const char *status_description;
	const char *message;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	message = purple_status_get_attr_string(status, "message");

	if (purple_presence_is_available(presence))
		status_description = purple_status_get_name(status);
	else if (purple_presence_is_idle(presence))
		status_description = _("Idle");
	else
		status_description = purple_status_get_name(status);

	purple_notify_user_info_add_pair(user_info, _("Status"), status_description);
	if (message != NULL)
		purple_notify_user_info_add_pair(user_info, _("Message"), message);

	if (bb == NULL) {
		purple_debug_error("bonjour", "Got tooltip request for a buddy without protocol data.\n");
		return;
	}

	/* Only show first/last name if there is a nickname set (to avoid duplication) */
	if (bb->nick != NULL) {
		if (bb->first != NULL)
			purple_notify_user_info_add_pair(user_info, _("First name"), bb->first);
		if (bb->first != NULL)
			purple_notify_user_info_add_pair(user_info, _("Last name"), bb->last);
	}

	if (bb->email != NULL)
		purple_notify_user_info_add_pair(user_info, _("E-Mail"), bb->email);

	if (bb->AIM != NULL)
		purple_notify_user_info_add_pair(user_info, _("AIM Account"), bb->AIM);

	if (bb->jid!= NULL)
		purple_notify_user_info_add_pair(user_info, _("XMPP Account"), bb->jid);
}

static void
bonjour_group_buddy(PurpleConnection *connection, const char *who, const char *old_group, const char *new_group)
{
	PurpleBlistNodeFlags oldflags;
	PurpleBuddy *buddy = purple_find_buddy(connection->account, who);

	if (buddy == NULL)
		return;

	oldflags = purple_blist_node_get_flags((PurpleBlistNode *)buddy);

	/* If we're moving them out of the bonjour group, make them persistent */
	if (strcmp(new_group, BONJOUR_GROUP_NAME) == 0)
		purple_blist_node_set_flags((PurpleBlistNode *)buddy, oldflags | PURPLE_BLIST_NODE_FLAG_NO_SAVE);
	else
		purple_blist_node_set_flags((PurpleBlistNode *)buddy, oldflags ^ PURPLE_BLIST_NODE_FLAG_NO_SAVE);

}

static gboolean
bonjour_can_receive_file(PurpleConnection *connection, const char *who)
{
	PurpleBuddy *buddy = purple_find_buddy(connection->account, who);

	return (buddy != NULL && buddy->proto_data != NULL);

}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	/* These shouldn't happen here because they are allocated in _init() */

	g_free(default_firstname);
	g_free(default_lastname);
	g_free(default_hostname);

	return TRUE;
}

static PurplePlugin *my_protocol = NULL;

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_NO_PASSWORD,
	NULL,                                                    /* user_splits */
	NULL,                                                    /* protocol_options */
	{"png,gif,jpeg", 0, 0, 96, 96, 65535, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	bonjour_list_icon,                                       /* list_icon */
	NULL,                                                    /* list_emblem */
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
	bonjour_fake_add_buddy,                                  /* add_buddy */
	NULL,                                                    /* add_buddies */
	bonjour_remove_buddy,                                    /* remove_buddy */
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
	bonjour_group_buddy,                                     /* group_buddy */
	NULL,                                                    /* rename_group */
	NULL,                                                    /* buddy_free */
	bonjour_convo_closed,                                    /* convo_closed */
	NULL,                                                    /* normalize */
	bonjour_set_buddy_icon,                                  /* set_buddy_icon */
	NULL,                                                    /* remove_group */
	NULL,                                                    /* get_cb_real_name */
	NULL,                                                    /* set_chat_topic */
	NULL,                                                    /* find_blist_chat */
	NULL,                                                    /* roomlist_get_list */
	NULL,                                                    /* roomlist_cancel */
	NULL,                                                    /* roomlist_expand_category */
	bonjour_can_receive_file,                                /* can_receive_file */
	bonjour_send_file,                                       /* send_file */
	bonjour_new_xfer,                                        /* new_xfer */
	NULL,                                                    /* offline_message */
	NULL,                                                    /* whiteboard_prpl_ops */
	NULL,                                                    /* send_raw */
	NULL,                                                    /* roomlist_room_serialize */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                           /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */

	"prpl-bonjour",                                   /**< id             */
	"Bonjour",                                        /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Bonjour Protocol Plugin"),
	                                                  /**  description    */
	N_("Bonjour Protocol Plugin"),
	NULL,                                             /**< author         */
	PURPLE_WEBSITE,                                   /**< homepage       */

	NULL,                                             /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

#ifdef WIN32
static gboolean _set_default_name_cb(gpointer data) {
	gchar *fullname = data;
	const char *splitpoint;
	GList *tmp = prpl_info.protocol_options;
	PurpleAccountOption *option;

	if (!fullname) {
		purple_debug_info("bonjour", "Unable to look up First and Last name or Username from system; using defaults.\n");
		return FALSE;
	}

	g_free(default_firstname);
	g_free(default_lastname);

	/* Split the real name into a first and last name */
	splitpoint = strchr(fullname, ' ');
	if (splitpoint != NULL) {
		default_firstname = g_strndup(fullname, splitpoint - fullname);
		default_lastname = g_strdup(&splitpoint[1]);
	} else {
		default_firstname = g_strdup(fullname);
		default_lastname = g_strdup("");
	}
	g_free(fullname);


	for(; tmp != NULL; tmp = tmp->next) {
		option = tmp->data;
		if (strcmp("first", purple_account_option_get_setting(option)) == 0)
			purple_account_option_set_default_string(option, default_firstname);
		else if (strcmp("last", purple_account_option_get_setting(option)) == 0)
			purple_account_option_set_default_string(option, default_lastname);
	}

	return FALSE;
}

static gpointer _win32_name_lookup_thread(gpointer data) {
	gchar *fullname = NULL;
	wchar_t username[UNLEN + 1];
	DWORD dwLenUsername = UNLEN + 1;

	GetUserNameW((LPWSTR) &username, &dwLenUsername);

	if (username != NULL && *username != '\0') {
		LPBYTE servername = NULL;
		LPBYTE info = NULL;

		NetGetDCName(NULL, NULL, &servername);

		/* purple_debug_info("bonjour", "Looking up the full name from the %s.\n", (servername ? "domain controller" : "local machine")); */

		if (NetUserGetInfo((LPCWSTR) servername, username, 10, &info) == NERR_Success
				&& info != NULL && ((LPUSER_INFO_10) info)->usri10_full_name != NULL
				&& *(((LPUSER_INFO_10) info)->usri10_full_name) != '\0') {
			fullname = g_utf16_to_utf8(
				((LPUSER_INFO_10) info)->usri10_full_name,
				-1, NULL, NULL, NULL);
		}
		/* Fall back to the local machine if we didn't get the full name from the domain controller */
		else if (servername != NULL) {
			/* purple_debug_info("bonjour", "Looking up the full name from the local machine"); */

			if (info != NULL) NetApiBufferFree(info);
			info = NULL;

			if (NetUserGetInfo(NULL, username, 10, &info) == NERR_Success
					&& info != NULL && ((LPUSER_INFO_10) info)->usri10_full_name != NULL
					&& *(((LPUSER_INFO_10) info)->usri10_full_name) != '\0') {
				fullname = g_utf16_to_utf8(
					((LPUSER_INFO_10) info)->usri10_full_name,
					-1, NULL, NULL, NULL);
			}
		}

		if (info != NULL) NetApiBufferFree(info);
		if (servername != NULL) NetApiBufferFree(servername);

		if (!fullname)
			fullname = g_utf16_to_utf8(username, -1, NULL, NULL, NULL);
	}

	purple_timeout_add(0, _set_default_name_cb, fullname);

	return NULL;
}
#endif

static void
initialize_default_account_values(void)
{
#ifndef _WIN32
	struct passwd *info;
#endif
	const char *fullname = NULL, *splitpoint, *tmp;
	char hostname[255];
	gchar *conv = NULL;

#ifndef _WIN32
	/* Try to figure out the user's real name */
	info = getpwuid(getuid());
	if ((info != NULL) && (info->pw_gecos != NULL) && (info->pw_gecos[0] != '\0'))
		fullname = info->pw_gecos;
	else if ((info != NULL) && (info->pw_name != NULL) && (info->pw_name[0] != '\0'))
		fullname = info->pw_name;
	else if (((fullname = getlogin()) != NULL) && (fullname[0] == '\0'))
		fullname = NULL;
#else
	/* The Win32 username lookup functions are synchronous so we do it in a thread */
	g_thread_create(_win32_name_lookup_thread, NULL, FALSE, NULL);
#endif

	/* Make sure fullname is valid UTF-8.  If not, try to convert it. */
	if (fullname != NULL && !g_utf8_validate(fullname, -1, NULL)) {
		fullname = conv = g_locale_to_utf8(fullname, -1, NULL, NULL, NULL);
		if (conv == NULL || *conv == '\0')
			fullname = NULL;
	}

	if (fullname == NULL)
		fullname = _("Purple Person");

	/* Split the real name into a first and last name */
	splitpoint = strchr(fullname, ' ');
	if (splitpoint != NULL) {
		default_firstname = g_strndup(fullname, splitpoint - fullname);
		tmp = &splitpoint[1];

		/* The last name may be followed by a comma and additional data.
		 * Only use the last name itself.
		 */
		splitpoint = strchr(tmp, ',');
		if (splitpoint != NULL)
			default_lastname = g_strndup(tmp, splitpoint - tmp);
		else
			default_lastname = g_strdup(tmp);
	} else {
		default_firstname = g_strdup(fullname);
		default_lastname = g_strdup("");
	}

	g_free(conv);

	/* Try to figure out a good host name to use */
	/* TODO: Avoid 'localhost,' if possible */
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		purple_debug_warning("bonjour", "Error when getting host name: %s.  Using \"localhost.\"\n",
				g_strerror(errno));
		strcpy(hostname, "localhost");
	}
	hostname[sizeof(hostname) - 1] = '\0';
	default_hostname = g_strdup(hostname);
}

static void
init_plugin(PurplePlugin *plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;

	initialize_default_account_values();

	/* Creating the user splits */
	split = purple_account_user_split_new(_("Hostname"), default_hostname, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	/* Creating the options for the protocol */
	option = purple_account_option_string_new(_("First name"), "first", default_firstname);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Last name"), "last", default_lastname);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("E-mail"), "email", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("AIM Account"), "AIM", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("XMPP Account"), "jid", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;
}

PURPLE_INIT_PLUGIN(bonjour, init_plugin, info);
