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

#include "internal.h"
#include "account.h"
#include "accountopt.h"
#include "version.h"
#include "debug.h"

#include "bonjour.h"
#include "dns_sd.h"
#include "jabber.h"
#include "buddy.h"

void bonjour_login(GaimAccount* account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimGroup* bonjour_group = NULL;
	BonjourData* bd = NULL;
	
	gc->flags |= GAIM_CONNECTION_HTML;
	gc->proto_data = g_new(BonjourData, 1);
	bd = gc->proto_data;
	
	// Start waiting for jabber connections (iChat style)
	bd->jabber_data = g_new(BonjourJabber, 1);
	bd->jabber_data->name = gc->account->username;
	bd->jabber_data->port = gaim_account_get_int(account, "port", BONJOUR_DEFAULT_PORT_INT);
	bd->jabber_data->account = account;
	
	if (bonjour_jabber_start(bd->jabber_data) == -1) {
		// Send a message about the connection error
		gaim_debug_error("bonjour", "Unable to listen to ichat connections");
		
		// Free the data
		g_free(bd->jabber_data);
		g_free(bd);
		return;
	}
	
	// Connect to the mDNS daemon looking for buddies in the LAN
	bd->dns_sd_data = bonjour_dns_sd_new();
	bd->dns_sd_data->name = (sw_string)gaim_account_get_username(account);
	bd->dns_sd_data->txtvers = g_strdup("1");
	bd->dns_sd_data->version = g_strdup("1");
	bd->dns_sd_data->first = gaim_account_get_string(account, "first", "Juanjo");
	bd->dns_sd_data->last = gaim_account_get_string(account, "last", "");
	bd->dns_sd_data->port_p2pj = gaim_account_get_int(account, "port", BONJOUR_DEFAULT_PORT_INT);
	bd->dns_sd_data->phsh = g_strdup("");
	bd->dns_sd_data->status = g_strdup("avail"); //<-- Check the real status if different from avail
	bd->dns_sd_data->email = gaim_account_get_string(account, "email", "");
	bd->dns_sd_data->vc = g_strdup("");
	bd->dns_sd_data->jid = g_strdup("");
	bd->dns_sd_data->AIM = g_strdup("");
	bd->dns_sd_data->msg = g_strdup(gc->away);
	
	bd->dns_sd_data->account = account;
	bonjour_dns_sd_start(bd->dns_sd_data);
	
	// Create a group for bonjour buddies
	bonjour_group = gaim_group_new(BONJOUR_GROUP_NAME);
	gaim_blist_add_group(bonjour_group, NULL);
	
	// Show the buddy list by telling Gaim we have already connected
	gaim_connection_set_state(gc, GAIM_CONNECTED);
}

void bonjour_close(GaimConnection* connection)
{
	GaimGroup* bonjour_group = gaim_find_group(BONJOUR_GROUP_NAME);
	GSList* buddies;
	GSList* l;
	BonjourData* bd = (BonjourData*)connection->proto_data;
	
	// Stop looking for buddies in the LAN
	if (connection != NULL) {
		bonjour_dns_sd_stop(bd->dns_sd_data);
		if (bd != NULL) {
			bonjour_dns_sd_free(bd->dns_sd_data);
		}
	}
	
	// Stop waiting for conversations
	bonjour_jabber_stop(bd->jabber_data);
	g_free(bd->jabber_data);
	
	// Remove all the bonjour buddies
	if(connection != NULL){
		buddies = gaim_find_buddies(connection->account, connection->account->username);
		for(l = buddies; l; l = l->next){
			bonjour_buddy_delete(((GaimBuddy*)(l->data))->proto_data);
			gaim_blist_remove_buddy(l->data);
		}
		g_slist_free(buddies);
	}
	
	// Delete the bonjour group
	gaim_blist_remove_group(bonjour_group);
	
}

const char* bonjour_list_icon(GaimAccount* account, GaimBuddy* buddy)
{
	return BONJOUR_ICON_NAME;
}

int bonjour_send_im(GaimConnection* connection, const char* to, const char* msg, GaimConvImFlags flags)
{
	if(!to || !msg)
		return 0;
		
	bonjour_jabber_send_message(((BonjourData*)(connection->proto_data))->jabber_data, to, msg);
		
	return 1;
}

void bonjour_set_status(GaimConnection* connection, const char* state, const char* message)
{
	char* status_dns_sd = NULL;
	char* stripped = NULL;

	if(message) {
		stripped = g_strdup(message);
	} else {
		stripped = g_strdup("");
	}

	if(connection->away){
		g_free(connection->away);
	}
	connection->away = stripped;
	
	if (g_ascii_strcasecmp(state, _("Online")) == 0) {
		status_dns_sd = g_strdup("avail");
	} else if (g_ascii_strcasecmp(state, _("Away")) == 0) {
		status_dns_sd = g_strdup("away");
	} else if (g_ascii_strcasecmp(state, _("Do Not Disturb")) == 0) {
		status_dns_sd = g_strdup("dnd");
	} else if (g_ascii_strcasecmp(state, _("Custom")) == 0) {
		status_dns_sd = g_strdup("away");
	}

	if (status_dns_sd != NULL) {
		bonjour_dns_sd_send_status(((BonjourData*)(connection->proto_data))->dns_sd_data, 
			status_dns_sd, stripped);
	}
}

static GList* bonjour_status_types(GaimConnection* connection)
{
	GList *types = NULL;

	types = g_list_append(types, _("Online"));
	types = g_list_append(types, _("Away"));
	types = g_list_append(types, _("Do Not Disturb"));
	types = g_list_append(types, GAIM_AWAY_CUSTOM);

	return types;
}

static void bonjour_convo_closed(GaimConnection* connection, const char* who)
{
	GaimBuddy* buddy = gaim_find_buddy(connection->account, who);
	
	bonjour_jabber_close_conversation(((BonjourData*)(connection->proto_data))->jabber_data, buddy);
}

static void bonjour_list_emblems(GaimBuddy *b, char **se, char **sw,
		char **nw, char **ne)
{
	switch (b->uc) {
		case BONJOUR_STATE_AWAY:
			*se = "away";
			break;
		case BONJOUR_STATE_DND:
			*se = "dnd";
			break;
		case BONJOUR_STATE_ERROR:
			*se = "error";
			break;
	}
}

static char* bonjour_status_text(GaimBuddy *b)
{
	BonjourBuddy* bb = (BonjourBuddy*)b->proto_data;
	
	if (bb->msg != NULL) {
		return g_strdup(bb->msg);
	} else {
		return g_strdup("");
	}
}

static char* bonjour_tooltip_text(GaimBuddy *b)
{
	char* status = NULL;
	
	switch (b->uc) {
		case BONJOUR_STATE_AVAILABLE:
			status = g_strdup(_("Online"));
			break;
		case BONJOUR_STATE_AWAY:
			status = g_strdup(_("Away"));
			break;
		case BONJOUR_STATE_DND:
			status = g_strdup(_("Do Not Disturb"));
			break;
		case BONJOUR_STATE_ERROR:
			status = g_strdup("Error");
			break;
	}
	
	return g_strconcat("\n<b>Status: </b>", status, NULL);
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_NO_PASSWORD,
	NULL,                                                    /* user_splits */
	NULL,                                                    /* protocol_options */
	{"png", 0, 0, 96, 96, GAIM_ICON_SCALE_DISPLAY},          /* icon_spec */
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
	NULL,                                                    /* warn */
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
	NULL                                                     /* send_file */
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
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;
	char hostname[255];

	if (gethostname(hostname, 255) != 0) {
		gaim_debug_warning("rendezvous", "Error %d when getting host name.  Using \"localhost.\"\n", errno);
		strcpy(hostname, "localhost");
	}

	// Creating the user splits
	split = gaim_account_user_split_new(_("Host name"), hostname, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);
	
	// Creating the options for the protocol
	option = gaim_account_option_int_new(_("Port"), "port", 5298);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("First name"), "first", "Gaim");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Last name"), "last", "User");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Email"), "email", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	
	/*
	option = gaim_account_option_string_new(_("Status Message"), "message", "Available");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	*/
	
	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(bonjour, init_plugin, info);
