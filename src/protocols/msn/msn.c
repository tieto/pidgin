/**
 * @file msn.c The MSN protocol plugin
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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

#include "blist.h"
#include "msn.h"
#include "accountopt.h"
#include "msg.h"
#include "page.h"
#include "pluginpref.h"
#include "prefs.h"
#include "session.h"
#include "state.h"
#include "utils.h"
#include "util.h"

#include "notification.h"
#include "switchboard.h"

#define BUDDY_ALIAS_MAXLEN 387

static GaimPlugin *my_protocol = NULL;

static const char *msn_normalize(const GaimAccount *account, const char *str);

typedef struct
{
	GaimConnection *gc;
	const char *passport;

} MsnMobileData;

typedef struct
{
	GaimConnection *gc;
	char *name;

} MsnGetInfoData;

static void
msn_act_id(GaimConnection *gc, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	GaimAccount *account;
	char *alias;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;
	account = gaim_connection_get_account(gc);

	if (entry == NULL || *entry == '\0')
		alias = g_strdup("");
	else
		alias = g_strdup(entry);

	if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
	{
		gaim_notify_error(gc, NULL,
						  _("Your new MSN friendly name is too long."), NULL);

		g_free(alias);

		return;
	}

	msn_cmdproc_send(cmdproc, "REA", "%s %s",
					 gaim_account_get_username(account),
					 gaim_url_encode(alias));

	g_free(alias);
}

static void
msn_set_prp(GaimConnection *gc, const char *type, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (entry == NULL || *entry == '\0')
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s  ", type);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s %s", type,
						 gaim_url_encode(entry));
	}
}

static void
msn_set_home_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHH", entry);
}

static void
msn_set_work_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHW", entry);
}

static void
msn_set_mobile_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHM", entry);
}

static void
enable_msn_pages_cb(GaimConnection *gc)
{
	msn_set_prp(gc, "MOB", "Y");
}

static void
disable_msn_pages_cb(GaimConnection *gc)
{
	msn_set_prp(gc, "MOB", "N");
}

static void
send_to_mobile(GaimConnection *gc, const char *who, const char *entry)
{
	MsnSession *session;
	MsnServConn *servconn;
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	MsnPage *page;
	char *payload;
	size_t payload_len;

	session = gc->proto_data;
	servconn = session->notification_conn;
	cmdproc = servconn->cmdproc;	

	page = msn_page_new();
	msn_page_set_body(page, entry);

	trans = msn_transaction_new("PGD", "%s 1 %d", who, page->size);

	payload = msn_page_gen_payload(page, &payload_len);

	msn_transaction_set_payload(trans, payload, payload_len);

	msn_page_destroy(page);

	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
send_to_mobile_cb(MsnMobileData *data, const char *entry)
{
	send_to_mobile(data->gc, data->passport, entry);
	g_free(data);
}

static void
close_mobile_page_cb(MsnMobileData *data, const char *entry)
{
	g_free(data);
}

/* -- */

static void
msn_show_set_friendly_name(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, NULL, _("Set your friendly name."),
					   _("This is the name that other MSN buddies will "
						 "see you as."),
					   gaim_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_act_id),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_home_phone(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	MsnSession *session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your home phone number."), NULL,
					   msn_user_get_home_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_home_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_work_phone(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	MsnSession *session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your work phone number."), NULL,
					   msn_user_get_work_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_work_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_phone(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	MsnSession *session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your mobile phone number."), NULL,
					   msn_user_get_mobile_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_mobile_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_pages(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_action(gc, NULL, _("Allow MSN Mobile pages?"),
			_("Do you want to allow or disallow people on "
			  "your buddy list to send you MSN Mobile pages "
			  "to your cell phone or other mobile device?"),
			-1, gc, 3,
			_("Allow"), G_CALLBACK(enable_msn_pages_cb),
			_("Disallow"), G_CALLBACK(disable_msn_pages_cb),
			_("Cancel"), NULL);
}

static void
show_send_to_mobile_cb(GaimBlistNode *node, gpointer ignored)
{
	GaimBuddy *buddy;
	GaimConnection *gc;
	MsnUser *user;
	MsnSession *session;
	MsnMobileData *data;
	
	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	session = gc->proto_data;
	user = msn_users_find_with_passport(session->users, buddy->name);

	data = g_new0(MsnMobileData, 1);
	data->gc = gc;
	data->passport = buddy->name;

	gaim_request_input(gc, NULL, _("Send a mobile message."), NULL,
			NULL, TRUE, FALSE, NULL,
			_("Page"), G_CALLBACK(send_to_mobile_cb),
			_("Close"), G_CALLBACK(close_mobile_page_cb),
			data);
}

static void
initiate_chat_cb(GaimBlistNode *node, gpointer data)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnSwitchBoard *swboard;
	MsnUser *user;
	
	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if ((swboard = msn_session_open_switchboard(session)) == NULL)
		return;

	user = msn_user_new(session, buddy->name, NULL);

	msn_switchboard_set_user(swboard, user);

	swboard->total_users = 1;

	session->last_chat_id++;
	swboard->chat_id = session->last_chat_id;
	swboard->chat = serv_got_joined_chat(gc, swboard->chat_id, "MSN Chat");

	gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat),
			gaim_account_get_username(buddy->account), NULL);
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "msn";
}

static void
msn_list_emblems(GaimBuddy *b, char **se, char **sw,
				 char **nw, char **ne)
{
	MsnUser *user;
	char *emblems[4] = { NULL, NULL, NULL, NULL };
	int away_type = MSN_AWAY_TYPE(b->uc);
	int i = 0;

	user = b->proto_data;

	if (b->present == GAIM_BUDDY_OFFLINE)
		emblems[i++] = "offline";
	else if (away_type == MSN_BUSY || away_type == MSN_PHONE)
		emblems[i++] = "occupied";
	else if (away_type != 0)
		emblems[i++] = "away";

	if (user == NULL) {
		emblems[0] = "offline";
	}
	else if (user->mobile)
		emblems[i++] = "wireless";

	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *
msn_status_text(GaimBuddy *b)
{
	if (b->uc & UC_UNAVAILABLE)
		return g_strdup(msn_away_get_text(MSN_AWAY_TYPE(b->uc)));

	return NULL;
}

static char *
msn_tooltip_text(GaimBuddy *b)
{
	char *text = NULL;

	if (GAIM_BUDDY_IS_ONLINE(b)) {
		text = g_strdup_printf("\n<b>%s:</b> %s", _("Status"),
							   msn_away_get_text(MSN_AWAY_TYPE(b->uc)));
	}

	return text;
}

static GList *
msn_away_states(GaimConnection *gc)
{
	GList *m = NULL;

	m = g_list_append(m, _("Available"));
	m = g_list_append(m, _("Away From Computer"));
	m = g_list_append(m, _("Be Right Back"));
	m = g_list_append(m, _("Busy"));
	m = g_list_append(m, _("On The Phone"));
	m = g_list_append(m, _("Out To Lunch"));
	m = g_list_append(m, _("Hidden"));

	return m;
}

static GList *
msn_actions(GaimPlugin *plugin, gpointer context)
{
	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Set Friendly Name"),
			msn_show_set_friendly_name);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Set Home Phone Number"),
			msn_show_set_home_phone);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Set Work Phone Number"),
			msn_show_set_work_phone);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Set Mobile Phone Number"),
			msn_show_set_mobile_phone);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

#if 0
	act = gaim_plugin_action_new(_("Enable/Disable Mobile Devices"),
			msn_show_set_mobile_support);
	m = g_list_append(m, act);
#endif

	act = gaim_plugin_action_new(_("Allow/Disallow Mobile Pages"),
			msn_show_set_mobile_pages);
	m = g_list_append(m, act);

	return m;
}

static GList *
msn_buddy_menu(GaimBuddy *buddy)
{
	MsnUser *user;

	GList *m = NULL;
	GaimBlistNodeAction *act;

	user = buddy->proto_data;
	if (user != NULL)
	{
		if (user->mobile)
		{
			act = gaim_blist_node_action_new(_("Send to Mobile"),
					show_send_to_mobile_cb, NULL);
			m = g_list_append(m, act);
		}
	}

	if (g_ascii_strcasecmp(buddy->name, gaim_account_get_username(buddy->account)))
	{
		act = gaim_blist_node_action_new(_("Initiate Chat"),
				initiate_chat_cb, NULL);
		m = g_list_append(m, act);
	}

	return m;
}


static GList *
msn_blist_node_menu(GaimBlistNode *node)
{
	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return msn_buddy_menu((GaimBuddy *) node);
	} else {
		return NULL;
	}
}


static void
msn_login(GaimAccount *account)
{
	GaimConnection *gc;
	MsnSession *session;
	const char *username;
	const char *server;
	gboolean http_method = FALSE;
	int port;

	gc = gaim_account_get_connection(account);

	if (!gaim_ssl_is_supported())
	{
		gaim_connection_error(gc,
			_("SSL support is needed for MSN. Please install a supported "
			  "SSL library. See http://gaim.sf.net/faq-ssl.php for more "
			  "information."));

		return;
	}

	if (gaim_account_get_bool(account, "http_method", FALSE))
	{
		http_method = TRUE;

		gaim_debug(GAIM_DEBUG_INFO, "msn", "using http method\n");

		server = "gateway.messenger.hotmail.com";
		port   = 80;
	}
	else
	{
		server = gaim_account_get_string(account, "server", MSN_SERVER);
		port   = gaim_account_get_int(account,    "port",   MSN_PORT);
	}

	session = msn_session_new(account, server, port);
	session->http_method = http_method;
	session->prpl = my_protocol;

	if (session->http_method)
		msn_http_session_init(session);

	gc->proto_data = session;
	gc->flags |= GAIM_CONNECTION_HTML | GAIM_CONNECTION_FORMATTING_WBFO | GAIM_CONNECTION_NO_BGCOLOR | GAIM_CONNECTION_NO_FONTSIZE | GAIM_CONNECTION_NO_URLDESC;

	gaim_connection_update_progress(gc, _("Connecting"), 0, MSN_CONNECT_STEPS);

	/* Hmm, I don't like this. */
	username = msn_normalize(account, gaim_account_get_username(account));

	if (strcmp(username, gaim_account_get_username(account)))
		gaim_account_set_username(account, username);

	if (!msn_session_connect(session))
	{
		gaim_connection_error(gc, _("Unable to connect."));

		return;
	}
}

static void
msn_close(GaimConnection *gc)
{
	MsnSession *session = gc->proto_data;

	if (session != NULL)
	{
		if (session->http_method)
			msn_http_session_uninit(session);

		msn_session_destroy(session);
	}

	gc->proto_data = NULL;
}

static int
msn_send_im(GaimConnection *gc, const char *who, const char *message,
			GaimConvImFlags flags)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	swboard = msn_session_find_switch_with_passport(session, who);

	if (g_ascii_strcasecmp(who, gaim_account_get_username(account)))
	{
		MsnMessage *msg;
		MsnUser *user;
		char *msgformat;
		char *msgtext;

		user = msn_user_new(session, who, NULL);

		msn_import_html(message, &msgformat, &msgtext);

		msg = msn_message_new();
		msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);
		msn_message_set_body(msg, msgtext);

		g_free(msgformat);
		g_free(msgtext);

		if (swboard != NULL)
		{
			msn_switchboard_send_msg(swboard, msg);
		}
		else
		{
			if ((swboard = msn_session_open_switchboard(session)) == NULL)
			{
				msn_message_destroy(msg);

				return 1;
			}

			msn_switchboard_set_user(swboard, user);
			msn_switchboard_send_msg(swboard, msg);
		}

		msn_user_destroy(user);
		msn_message_destroy(msg);
	}
	else
	{
		/*
		 * In MSN, you can't send messages to yourself, so
		 * we'll fake like we received it ;)
		 */
		serv_got_typing_stopped(gc, (char *)who);
		serv_got_im(gc, who, message, flags, time(NULL));
	}

	return 1;
}

static int
msn_send_typing(GaimConnection *gc, const char *who, int typing)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	MsnUser *user;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;

	if (!typing)
		return 0;

	if (!g_ascii_strcasecmp(who, gaim_account_get_username(account)))
	{
		/* We'll just fake it, since we're sending to ourself. */
		serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, GAIM_TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_switch_with_passport(session, who);

	if (swboard == NULL)
		return 0;

	user = msn_user_new(session, who, NULL);

	msg = msn_message_new();
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_charset(msg, NULL);
	msn_message_set_flag(msg, 'U');
	msn_message_set_attr(msg, "TypingUser",
						 gaim_account_get_username(account));
	msn_message_set_attr(msg, "User-Agent", NULL);
	msn_message_set_body(msg, "\r\n");

	msn_switchboard_send_msg(swboard, msg);

	msn_user_destroy(user);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	MsnSession *session;
	const char *away;

	session = gc->proto_data;

	if (gc->away != NULL)
	{
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg != NULL)
	{
		gc->away = g_strdup("");
		away = "AWY";
	}
	else if (state)
	{
		gc->away = g_strdup("");

		if (!strcmp(state, _("Away From Computer")))
			away = "AWY";
		else if (!strcmp(state, _("Be Right Back")))
			away = "BRB";
		else if (!strcmp(state, _("Busy")))
			away = "BSY";
		else if (!strcmp(state, _("On The Phone")))
			away = "PHN";
		else if (!strcmp(state, _("Out To Lunch")))
			away = "LUN";
		else if (!strcmp(state, _("Hidden")))
			away = "HDN";
		else
		{
			g_free(gc->away);
			gc->away = NULL;
			away = "NLN";
		}
	}
	else if (gc->is_idle)
		away = "IDL";
	else
		away = "NLN";

	msn_session_change_status(session, away);
}

static void
msn_set_idle(GaimConnection *gc, int idle)
{
	MsnSession *session;

	session = gc->proto_data;

	if (gc->away != NULL)
		return;

	msn_session_change_status(session, (idle ? "IDL" : "NLN"));
}

static void
msn_add_buddy(GaimConnection *gc, const char *name, GaimGroup *group)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnGroup *msn_group = NULL;
	const char *who;
	GSList *l;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;
	who = msn_normalize(gc->account, name);

	if (strchr(who, ' '))
	{
		/* This is a broken blist entry. */
		return;
	}

	for (l = session->lists.forward; l != NULL; l = l->next)
	{
		MsnUser *user = l->data;

		if (!gaim_utf8_strcasecmp(who, msn_user_get_passport(user)))
			break;
	}

	if (l != NULL)
		return;

	if (group != NULL)
		msn_group = msn_groups_find_with_name(session->groups, group->name);

	if (msn_group != NULL)
	{
		msn_cmdproc_send(cmdproc, "ADD", "FL %s %s %d", who, who,
						 msn_group_get_id(msn_group));
	}
	else
	{
		msn_cmdproc_send(cmdproc, "ADD", "FL %s %s", who, who);
	}
}

static void
msn_rem_buddy(GaimConnection *gc, const char *who, const char *group_name)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnGroup *group;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (strchr(who, ' '))
	{
		/* This is a broken blist entry. */
		return;
	}

	group = msn_groups_find_with_name(session->groups, group_name);

	if (group == NULL)
	{
		msn_cmdproc_send(cmdproc, "REM", "FL %s", who);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "REM", "FL %s %d", who,
						 msn_group_get_id(group));
	}
}

static void
msn_add_permit(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (!strchr(who, '@'))
	{
		char buf[MSN_BUF_LEN];

		g_snprintf(buf, sizeof(buf),
				   _("An MSN screen name must be in the form \"user@server.com\". "
					 "Perhaps you meant %s@hotmail.com. No changes were made "
					 "to your allow list."), who);

		gaim_notify_error(gc, NULL, _("Invalid MSN screen name"), buf);
		gaim_privacy_permit_remove(gc->account, who, TRUE);

		return;
	}

	if (g_slist_find_custom(gc->account->deny, who, (GCompareFunc)strcasecmp))
	{
		gaim_debug_info("msn", "Moving %s from BL to AL\n", who);
		gaim_privacy_deny_remove(gc->account, who, TRUE);

		msn_cmdproc_send(cmdproc, "REM", "BL %s", who);

		if (cmdproc->error)
			return;
	}

	msn_cmdproc_send(cmdproc, "ADD", "AL %s %s", who, who);
}

static void
msn_add_deny(GaimConnection *gc, const char *who)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (!strchr(who, '@'))
	{
		char buf[MSN_BUF_LEN];
		g_snprintf(buf, sizeof(buf),
				   _("An MSN screen name must be in the form \"user@server.com\". "
					 "Perhaps you meant %s@hotmail.com. No changes were made "
					 "to your block list."), who);

		gaim_notify_error(gc, NULL, _("Invalid MSN screen name"), buf);

		gaim_privacy_deny_remove(gc->account, who, TRUE);

		return;
	}

	if (g_slist_find_custom(gc->account->permit, who, (GCompareFunc)strcmp))
	{
		gaim_debug(GAIM_DEBUG_INFO, "msn", "Moving %s from AL to BL\n", who);
		gaim_privacy_permit_remove(gc->account, who, TRUE);

		msn_cmdproc_send(cmdproc, "REM", "AL %s", who);

		if (cmdproc->error)
			return;
	}

	msn_cmdproc_send(cmdproc, "ADD", "BL %s %s", who, who);
}

static void
msn_rem_permit(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	msn_cmdproc_send(cmdproc, "REM", "AL %s", who);

	if (cmdproc->error)
		return;

	gaim_privacy_deny_add(gc->account, who, TRUE);

	msn_cmdproc_send(cmdproc, "ADD", "BL %s %s", who, who);
}

static void
msn_rem_deny(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	msn_cmdproc_send(cmdproc, "REM", "BL %s", who);

	if (cmdproc->error)
		return;

	gaim_privacy_permit_add(gc->account, who, TRUE);

	msn_cmdproc_send(cmdproc, "ADD", "AL %s %s", who, who);
}

static void
msn_set_permit_deny(GaimConnection *gc)
{
	GaimAccount *account;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	GSList *s, *t = NULL;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (account->perm_deny == GAIM_PRIVACY_ALLOW_ALL ||
		account->perm_deny == GAIM_PRIVACY_DENY_USERS)
	{
		msn_cmdproc_send(cmdproc, "BLP", "%s", "AL");
	}
	else
	{
		msn_cmdproc_send(cmdproc, "BLP", "%s", "BL");
	}

	if (cmdproc->error)
		return;

	/*
	 * This is safe because we'll always come here after we've gotten
	 * the list off the server, and data is never removed. So if the
	 * lengths are equal we don't know about anyone locally and so
	 * there's no sense in going through them all.
	 */
	if (g_slist_length(gc->account->permit) ==
		g_slist_length(session->lists.allow))
	{
		g_slist_free(session->lists.allow);
		session->lists.allow = NULL;
	}

	if (g_slist_length(gc->account->deny) ==
		g_slist_length(session->lists.block))
	{
		g_slist_free(session->lists.block);
		session->lists.block = NULL;
	}

	if (session->lists.allow == NULL && session->lists.block == NULL)
		return;

	if (session->lists.allow != NULL)
	{
		for (s = g_slist_nth(gc->account->permit,
							 g_slist_length(session->lists.allow));
			 s != NULL;
			 s = s->next)
		{
			char *who = s->data;

			if (!strchr(who, '@'))
			{
				t = g_slist_append(t, who);
				continue;
			}

			if (g_slist_find(session->lists.block, who))
			{
				t = g_slist_append(t, who);
				continue;
			}

			msn_cmdproc_send(cmdproc, "ADD", "AL %s %s", who, who);

			if (cmdproc->error)
				return;
		}

		for (; t != NULL; t = t->next)
			gaim_privacy_permit_remove(gc->account, t->data, TRUE);

		if (t != NULL)
			g_slist_free(t);

		t = NULL;
		g_slist_free(session->lists.allow);
		session->lists.allow = NULL;
	}

	if (session->lists.block)
	{
		for (s = g_slist_nth(gc->account->deny,
							 g_slist_length(session->lists.block));
			 s != NULL;
			 s = s->next)
		{
			char *who = s->data;

			if (!strchr(who, '@'))
			{
				t = g_slist_append(t, who);
				continue;
			}

			if (g_slist_find(session->lists.block, who))
			{
				t = g_slist_append(t, who);
				continue;
			}

			msn_cmdproc_send(cmdproc, "ADD", "BL %s %s", who, who);

			if (cmdproc->error)
				return;
		}

		for (; t != NULL; t = t->next)
			gaim_privacy_deny_remove(gc->account, t->data, TRUE);

		if (t != NULL)
			g_slist_free(t);

		g_slist_free(session->lists.block);
		session->lists.block = NULL;
	}
}

static void
msn_chat_invite(GaimConnection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = gc->proto_data;
	swboard = msn_session_find_switch_with_id(session, id);

	if (swboard == NULL)
		return;

	msn_cmdproc_send(swboard->cmdproc, "CAL", "%s", who);
}

static void
msn_chat_leave(GaimConnection *gc, int id)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = gc->proto_data;
	account = gaim_connection_get_account(gc);
	swboard = msn_session_find_switch_with_id(session, id);

	if (swboard == NULL)
	{
		serv_got_chat_left(gc, id);
		return;
	}

	msn_cmdproc_send_quick(swboard->cmdproc, "OUT", NULL, NULL);
	msn_switchboard_destroy(swboard);

	serv_got_chat_left(gc, id);
}

static int
msn_chat_send(GaimConnection *gc, int id, const char *message)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	swboard = msn_session_find_switch_with_id(session, id);

	if (swboard == NULL)
		return -EINVAL;

	msn_import_html(message, &msgformat, &msgtext);

	msg = msn_message_new();
	msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);
	msn_message_set_body(msg, msgtext);

	g_free(msgformat);
	g_free(msgtext);

	msn_switchboard_send_msg(swboard, msg);

	msn_message_destroy(msg);

	serv_got_chat_in(gc, id, gaim_account_get_username(account),
					 0, message, time(NULL));

	return 0;
}

static void
msn_keepalive(GaimConnection *gc)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if (!session->http_method)
		msn_cmdproc_send_quick(cmdproc, "PNG", NULL, NULL);
}

static void
msn_group_buddy(GaimConnection *gc, const char *who,
				const char *old_group_name, const char *new_group_name)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnGroup *old_group, *new_group;
	MsnUser *user;
	const char *friendly;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;
	old_group = msn_groups_find_with_name(session->groups, old_group_name);
	new_group = msn_groups_find_with_name(session->groups, new_group_name);

	user = msn_users_find_with_passport(session->users, who);

	if ((friendly = msn_user_get_name(user)) == NULL)
		friendly = msn_user_get_passport(user);

	if (old_group != NULL)
		msn_user_remove_group_id(user, msn_group_get_id(old_group));

	if (new_group == NULL)
	{
		msn_cmdproc_send(cmdproc, "ADG", "%s 0",
						 gaim_url_encode(new_group_name));

		if (cmdproc->error)
			return;

		/* I hate this. So much. */
		session->moving_buddy    = TRUE;
		session->dest_group_name = g_strdup(new_group_name);
		session->old_group       = old_group;

		session->moving_user =
			msn_users_find_with_passport(session->users, who);

		msn_user_ref(session->moving_user);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "ADD", "FL %s %s %d",
						 who, gaim_url_encode(friendly),
						 msn_group_get_id(new_group));

		if (cmdproc->error)
			return;
	}

	if (old_group != NULL)
	{
		msn_cmdproc_send(cmdproc, "REM", "FL %s %d", who,
						 msn_group_get_id(old_group));

		if (cmdproc->error)
			return;

		if (msn_users_get_count(msn_group_get_users(old_group)) <= 0)
		{
			msn_cmdproc_send(cmdproc, "RMG", "%d",
							 msn_group_get_id(old_group));
		}
	}
}

static void
msn_rename_group(GaimConnection *gc, const char *old_group_name,
				 const char *new_group_name, GList *members)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnGroup *old_group;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if ((old_group = msn_groups_find_with_name(session->groups,
											   old_group_name)) != NULL)
	{
		msn_cmdproc_send(cmdproc, "REG", "%d %s 0",
						 msn_group_get_id(old_group),
						 gaim_url_encode(new_group_name));

		if (cmdproc->error)
			return;

		msn_group_set_name(old_group, new_group_name);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "ADG", "%s 0",
						 gaim_url_encode(new_group_name));
	}
}

static void
msn_buddy_free(GaimBuddy *b)
{
	if (b->proto_data != NULL)
	{
		msn_user_destroy(b->proto_data);
		b->proto_data = NULL;
	}
}

static void
msn_convo_closed(GaimConnection *gc, const char *who)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	swboard = msn_session_find_switch_with_passport(session, who);

	if (swboard != NULL && swboard->chat == NULL)
	{
		msn_cmdproc_send_quick(swboard->cmdproc, "BYE", "%s",
							   gaim_account_get_username(account));

		msn_switchboard_destroy(swboard);
	}
}

static const char *
msn_normalize(const GaimAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp;

	g_return_val_if_fail(str != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", str,
			   (strchr(str, '@') ? "" : "@hotmail.com"));

	tmp = g_utf8_strdown(buf, -1);
	strncpy(buf, tmp, sizeof(buf));
	g_free(tmp);

	return buf;
}

static void
msn_set_buddy_icon(GaimConnection *gc, const char *filename)
{
	MsnSession *session = (MsnSession *)gc->proto_data;
	MsnUser *user = session->user;

	msn_user_set_buddy_icon(user, filename);

	msn_session_change_status(session, session->away_state);
}

static void
msn_remove_group(GaimConnection *gc, const char *name)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnGroup *group;

	session = gc->proto_data;
	cmdproc = session->notification_conn->cmdproc;

	if ((group = msn_groups_find_with_name(session->groups, name)) != NULL)
	{
		msn_cmdproc_send(cmdproc, "RMG", "%d", msn_group_get_id(group));
	}
}

static void
msn_got_info(void *data, const char *url_text, size_t len)
{
	MsnGetInfoData *info_data = (MsnGetInfoData *)data;
	char *stripped, *p, *q;
	char buf[1024];
	char *user_url = NULL;
	gboolean found;
	gboolean has_info = FALSE;
	char *url_buffer;
	GString *s;
	int stripped_len;

	gaim_debug_info("msn", "In msn_got_info\n");

	if (url_text == NULL || strcmp(url_text, "") == 0)
	{
		gaim_notify_formatted(info_data->gc, NULL,
			_("Buddy Information"), NULL,
			_("<html><body><b>Error retrieving profile</b></body></html>"),
			  NULL, NULL);

		return;
	}

	url_buffer = g_strdup(url_text);

	/* If they have a homepage link, MSN masks it such that we need to
	 * fetch the url out before gaim_markup_strip_html() nukes it */
	if ((p = strstr(url_text,
			"Take a look at my </font><A class=viewDesc title=\"")) != NULL)
	{
		p += 50;

		if ((q = strchr(p, '"')) != NULL)
			user_url = g_strndup(p, q - p);
	}

	/*
	 * gaim_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	 */
	while ((p = strstr(url_buffer, "&nbsp;")) != NULL)
	{
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_text) - 6] = '\0';
	}

	while ((p = strstr(url_buffer, "&#183;")) != NULL)
	{
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}

	/* Nuke the nasty \r's that just get in the way */
	while ((p = strchr(url_buffer, '\r')) != NULL)
	{
		memmove(p, p + 1, strlen(p + 1));
		url_buffer[strlen(url_buffer) - 1] = '\0';
	}

	/* MSN always puts in &#39; for apostrophes...replace them */
	while ((p = strstr(url_buffer, "&#39;")) != NULL)
	{
		*p = '\'';
		memmove(p + 1, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 4] = '\0';
	}

	/* Nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = gaim_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	gaim_debug_misc("msn", "stripped = %p\n", stripped);
	gaim_debug_misc("msn", "url_buffer = %p\n", url_buffer);

	/* Gonna re-use the memory we've already got for url_buffer */
	/* No we're not. */
	s = g_string_sized_new(strlen(url_buffer));

	/* Extract their Name and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\tName", 0, "\t", '\n', "Undisclosed", _("Name"), 0, NULL);

	if (found)
		has_info = TRUE;

	/* Extract their Age and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,"\tAge",
			0, "\t", '\n', "Undisclosed", _("Age"), 0, NULL);

	if (found)
		has_info = TRUE;

	/* Extract their Gender and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,"\tGender",
			6, "\t", '\n', "Undisclosed", _("Gender"), 0, NULL);

	if (found)
		has_info = TRUE;

	/* Extract their MaritalStatus and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\tMaritalStatus", 0, "\t", '\n', "Undisclosed",
			_("Marital Status"), 0, NULL);

	if (found)
		has_info = TRUE;

	/* Extract their Location and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\tLocation", 0, "\t", '\n', "Undisclosed", _("Location"),
			0, NULL);

	if (found)
		has_info = TRUE;

	/* Extract their Occupation and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\t Occupation", 6, "\t", '\n', "Undisclosed", _("Occupation"),
			0, NULL);

	if (found)
		has_info = TRUE;

	/*
	 * The fields, 'A Little About Me', 'Favorite Things', 'Hobbies
	 * and Interests', 'Favorite Quote', and 'My Homepage' may or may
	 * not appear, in any combination. However, they do appear in
	 * certain order, so we can successively search to pin down the
	 * distinct values.
	 */

	/* Check if they have A Little About Me */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\tA Little About Me", 1, "Favorite Things", '\n', NULL,
			_("A Little About Me"), 0, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"\tA Little About Me", 1, "Hobbies and Interests", '\n',
				NULL, _("A Little About Me"), 0, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"\tA Little About Me", 1, "Favorite Quote", '\n', NULL,
				_("A Little About Me"), 0, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"\tA Little About Me", 1, "My Homepage\tTake a look", '\n',
				NULL, _("A Little About Me"), 0, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				"\tA Little About Me", 1, "last updated", '\n', NULL,
				_("A Little About Me"), 0, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Things */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Things", 1, "Hobbies and Interests", '\n', NULL,
				_("Favorite Things"), 0, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Things", 1, "Favorite Quote", '\n', NULL,
				"Favorite Things", 0, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Things", 1, "My Homepage\tTake a look", '\n', NULL,
				_("Favorite Things"), 0, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Things", 1, "last updated", '\n', NULL,
				_("Favorite Things"), 0, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Hobbies and Interests */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"Hobbies and Interests", 1, "Favorite Quote", '\n', NULL,
			_("Hobbies and Interests"), 0, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Hobbies and Interests", 1, "My Homepage\tTake a look",
				'\n', NULL, _("Hobbies and Interests"), 0, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Hobbies and Interests", 1, "last updated", '\n', NULL,
				_("Hobbies and Interests"), 0, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Quote */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"Favorite Quote", 1, "My Homepage\tTake a look", '\n', NULL,
			_("Favorite Quote"), 0, NULL);

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Quote", 1, "last updated", '\n', NULL,
				_("Favorite Quote"), 0, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Extract the last updated date and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\tlast updated:", 1, "\n", '\n', NULL, _("Last Updated"),
			0, NULL);

	if (found)
		has_info = TRUE;

	/* If we were able to fetch a homepage url earlier, stick it in there */
	if (user_url != NULL)
	{
		g_snprintf(buf, sizeof(buf),
				   "<b>%s:</b><br><a href=\"%s\">%s</a><br>\n",
				   _("Homepage"), user_url, user_url);

		g_string_append(s, buf);

		g_free(user_url);

		has_info = TRUE;
	}

	/* Finish it off, and show it to them */
	g_string_append(s, "</body></html>\n");

	if (has_info)
	{
		gaim_notify_formatted(info_data->gc, NULL, _("Buddy Information"),
							  NULL, s->str, NULL, NULL);
	}
	else
	{
		char primary[256];

		g_snprintf(primary, sizeof(primary),
				   _("User information for %s unavailable"), info_data->name);
		gaim_notify_error(info_data->gc, NULL, primary,
						  _("The user's profile is empty."));
	}

	g_free(stripped);
	g_free(url_buffer);
	g_string_free(s, TRUE);
	g_free(info_data->name);
	g_free(info_data);
}

static void
msn_get_info(GaimConnection *gc, const char *name)
{
	MsnGetInfoData *data;
	char *url;

	data       = g_new0(MsnGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s", PROFILE_URL, name);

	gaim_url_fetch(url, FALSE,
				   "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)", TRUE,
				   msn_got_info, data);

	g_free(url);
}

static gboolean msn_load(GaimPlugin *plugin)
{
	msn_notification_init();
	msn_switchboard_init();

	return TRUE;
}

static gboolean msn_unload(GaimPlugin *plugin)
{
	msn_notification_end();
	msn_switchboard_end();

	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label(_("Conversations"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
								"/plugins/prpl/msn/conv_close_notice",
								_("Display conversation closed notices"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
								"/plugins/prpl/msn/conv_timeout_notice",
								_("Display timeout notices"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame
};

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PRPL_API_VERSION,
	OPT_PROTO_MAIL_CHECK /* | OPT_PROTO_BUDDY_ICON */,
	NULL,
	NULL,
	msn_list_icon,
	msn_list_emblems,
	msn_status_text,
	msn_tooltip_text,
	msn_away_states,
	msn_blist_node_menu,
	NULL,
	msn_login,
	msn_close,
	msn_send_im,
	NULL,
	msn_send_typing,
	msn_get_info,
	msn_set_away,
	msn_set_idle,
	NULL,
	msn_add_buddy,
	NULL,
	msn_rem_buddy,
	NULL,
	msn_add_permit,
	msn_add_deny,
	msn_rem_permit,
	msn_rem_deny,
	msn_set_permit_deny,
	NULL,
	NULL,
	NULL, /* reject chat invite */
	msn_chat_invite,
	msn_chat_leave,
	NULL,
	msn_chat_send,
	msn_keepalive,
	NULL,
	NULL,
	NULL,
	NULL,
	msn_group_buddy,
	msn_rename_group,
	msn_buddy_free,
	msn_convo_closed,
	msn_normalize,
	msn_set_buddy_icon,
	msn_remove_group,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_API_VERSION,                          /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-msn",                                       /**< id             */
	"MSN",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("MSN Protocol Plugin"),
	                                                  /**  description    */
	N_("MSN Protocol Plugin"),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	msn_load,                                         /**< load           */
	msn_unload,                                       /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	msn_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Login server"), "server",
											MSN_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("Port"), "port", 1863);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_bool_new(_("Use HTTP Method"), "http_method",
										  FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;

	gaim_prefs_add_none("/plugins/prpl/msn");
	gaim_prefs_add_bool("/plugins/prpl/msn/conv_close_notice",   TRUE);
	gaim_prefs_add_bool("/plugins/prpl/msn/conv_timeout_notice", TRUE);
}

GAIM_INIT_PLUGIN(msn, init_plugin, info);
