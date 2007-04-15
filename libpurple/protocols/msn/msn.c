/**
 * @file msn.c The MSN protocol plugin
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define PHOTO_SUPPORT 1

#include <glib.h>

#include "msn.h"
#include "accountopt.h"
#include "msg.h"
#include "page.h"
#include "pluginpref.h"
#include "prefs.h"
#include "session.h"
#include "state.h"
#include "util.h"
#include "cmds.h"
#include "core.h"
#include "prpl.h"
#include "msnutils.h"
#include "version.h"

#include "switchboard.h"
#include "notification.h"
#include "sync.h"
#include "slplink.h"

#if PHOTO_SUPPORT
#include "imgstore.h"
#endif

typedef struct
{
	PurpleConnection *gc;
	const char *passport;

} MsnMobileData;

typedef struct
{
	PurpleConnection *gc;
	char *name;

} MsnGetInfoData;

typedef struct
{
	MsnGetInfoData *info_data;
	char *stripped;
	char *url_buffer;
	PurpleNotifyUserInfo *user_info;
	char *photo_url_text;

} MsnGetInfoStepTwoData;

static const char *
msn_normalize(const PurpleAccount *account, const char *str)
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

static PurpleCmdRet
msn_cmd_nudge(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleConnection *gc = purple_account_get_connection(account);
	MsnMessage *msg;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	msg = msn_message_new_nudge();
	session = gc->proto_data;
	swboard = msn_session_get_swboard(session, purple_conversation_get_name(conv), MSN_SB_FLAG_IM);

	if (swboard == NULL)
		return PURPLE_CMD_RET_FAILED;

	msn_switchboard_send_msg(swboard, msg, TRUE);

	purple_conversation_write(conv, NULL, _("You have just sent a Nudge!"), PURPLE_MESSAGE_SYSTEM, time(NULL));

	return PURPLE_CMD_RET_OK;
}

void
msn_act_id(PurpleConnection *gc, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	PurpleAccount *account;
	const char *alias;

	char *soapbody;
	MsnSoapReq *soap_request;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	account = purple_connection_get_account(gc);

	if(entry && strlen(entry))
		alias = purple_url_encode(entry);
	else
		alias = "";

	if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
	{
		purple_notify_error(gc, NULL,
						  _("Your new MSN friendly name is too long."), NULL);
		return;
	}

	if (*alias == '\0') {
		alias = purple_url_encode(purple_account_get_username(account));
	}

	msn_cmdproc_send(cmdproc, "PRP", "MFN %s", alias);

	soapbody = g_strdup_printf(MSN_CONTACT_UPDATE_TEMPLATE, alias);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
										MSN_ADDRESS_BOOK_POST_URL,
										MSN_CONTACT_UPDATE_SOAP_ACTION,
										soapbody,
										NULL,
										NULL);

	session->contact->soapconn->read_cb = NULL;
	msn_soap_post(session->contact->soapconn,
				  soap_request,
				  msn_contact_connect_init);

	g_free(soapbody);
}

static void
msn_set_prp(PurpleConnection *gc, const char *type, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (entry == NULL || *entry == '\0')
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s", type);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s %s", type,
						 purple_url_encode(entry));
	}
}

static void
msn_set_home_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHH", entry);
}

static void
msn_set_work_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHW", entry);
}

static void
msn_set_mobile_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHM", entry);
}

static void
enable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "Y");
}

static void
disable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "N");
}

static void
send_to_mobile(PurpleConnection *gc, const char *who, const char *entry)
{
	MsnTransaction *trans;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnPage *page;
	char *payload;
	size_t payload_len;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	page = msn_page_new();
	msn_page_set_body(page, entry);

	payload = msn_page_gen_payload(page, &payload_len);

	trans = msn_transaction_new(cmdproc, "PGD", "%s 1 %d", who, payload_len);

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
msn_show_set_friendly_name(PurplePluginAction *action)
{
	PurpleConnection *gc;

	gc = (PurpleConnection *) action->context;

	purple_request_input(gc, NULL, _("Set your friendly name."),
					   _("This is the name that other MSN buddies will "
						 "see you as."),
					   purple_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_act_id),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_home_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your home phone number."), NULL,
					   msn_user_get_home_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_home_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_work_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your work phone number."), NULL,
					   msn_user_get_work_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_work_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your mobile phone number."), NULL,
					   msn_user_get_mobile_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_mobile_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_pages(PurplePluginAction *action)
{
	PurpleConnection *gc;

	gc = (PurpleConnection *) action->context;

	purple_request_action(gc, NULL, _("Allow MSN Mobile pages?"),
						_("Do you want to allow or disallow people on "
						  "your buddy list to send you MSN Mobile pages "
						  "to your cell phone or other mobile device?"),
						-1, gc, 3,
						_("Allow"), G_CALLBACK(enable_msn_pages_cb),
						_("Disallow"), G_CALLBACK(disable_msn_pages_cb),
						_("Cancel"), NULL);
}

static void
msn_show_hotmail_inbox(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	if (session->passport_info.file == NULL)
	{
		purple_notify_error(gc, NULL,
						  _("This Hotmail account may not be active."), NULL);
		return;
	}

	purple_notify_uri(gc, session->passport_info.file);
}

static void
show_send_to_mobile_cb(PurpleBlistNode *node, gpointer ignored)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	MsnSession *session;
	MsnMobileData *data;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);

	session = gc->proto_data;

	data = g_new0(MsnMobileData, 1);
	data->gc = gc;
	data->passport = buddy->name;

	purple_request_input(gc, NULL, _("Send a mobile message."), NULL,
					   NULL, TRUE, FALSE, NULL,
					   _("Page"), G_CALLBACK(send_to_mobile_cb),
					   _("Close"), G_CALLBACK(close_mobile_page_cb),
					   data);
}

static void
initiate_chat_cb(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;

	MsnSession *session;
	MsnSwitchBoard *swboard;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);

	session = gc->proto_data;

	swboard = msn_switchboard_new(session);
	msn_switchboard_request(swboard);
	msn_switchboard_request_add_user(swboard, buddy->name);

	/* TODO: This might move somewhere else, after USR might be */
	swboard->chat_id = session->conv_seq++;
	swboard->conv = serv_got_joined_chat(gc, swboard->chat_id, "MSN Chat");
	swboard->flag = MSN_SB_FLAG_IM;

	purple_conv_chat_add_user(PURPLE_CONV_CHAT(swboard->conv),
							purple_account_get_username(buddy->account), NULL, PURPLE_CBFLAGS_NONE, TRUE);
}

static void
t_msn_xfer_init(PurpleXfer *xfer)
{
	MsnSlpLink *slplink;
	const char *filename;
	FILE *fp;

	filename = purple_xfer_get_local_filename(xfer);

	slplink = xfer->data;

	if ((fp = g_fopen(filename, "rb")) == NULL)
	{
		PurpleAccount *account;
		const char *who;
		char *msg;

		account = slplink->session->account;
		who = slplink->remote_user;

		msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
							  filename, strerror(errno));
		purple_xfer_error(purple_xfer_get_type(xfer), account, xfer->who, msg);
		purple_xfer_cancel_local(xfer);
		g_free(msg);

		return;
	}
	fclose(fp);

	msn_slplink_request_ft(slplink, xfer);
}

static PurpleXfer*
msn_new_xfer(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSlpLink *slplink;
	PurpleXfer *xfer;

	session = gc->proto_data;

	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);

	slplink = msn_session_get_slplink(session, who);

	xfer->data = slplink;

	purple_xfer_set_init_fnc(xfer, t_msn_xfer_init);

	return xfer;
}

static void
msn_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	PurpleXfer *xfer = msn_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

static gboolean
msn_can_receive_file(PurpleConnection *gc, const char *who)
{
	PurpleAccount *account;
	char *normal;
	gboolean ret;

	account = purple_connection_get_account(gc);

	normal = g_strdup(msn_normalize(account, purple_account_get_username(account)));

	ret = strcmp(normal, msn_normalize(account, who));

	g_free(normal);

	return ret;
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "msn";
}

/*
 * Set the User status text
 * Add the PSM String Using "Name - PSM String" format
 */
static char *
msn_status_text(PurpleBuddy *buddy)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const char *msg, *name, *cmedia;
	char *psm_str, *tmp2, *text;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	msg = purple_status_get_attr_string(status, "message");
	cmedia=purple_status_get_attr_string(status, "currentmedia");

	if (!purple_presence_is_available(presence) && !purple_presence_is_idle(presence)){
		name = purple_status_get_name(status);
	}else{
		name = NULL;
	}

	if (cmedia != NULL) {
		if(name) {
			tmp2 = g_strdup_printf("%s - %s", name, cmedia);
			text = g_markup_escape_text(tmp2, -1);
		} else {
			text = g_markup_escape_text(cmedia, -1);
		}
		return text;
	} else if (msg != NULL) {
		tmp2 = purple_markup_strip_html(msg);
		if (name){
			psm_str = g_strdup_printf("%s - %s", name, tmp2);
			g_free(tmp2);
		}else{
			psm_str = tmp2;
		}
		text = g_markup_escape_text(psm_str, -1);
		g_free(psm_str);
		return text;
	} else {
		if (!purple_presence_is_available(presence) && !purple_presence_is_idle(presence)){
			psm_str = g_strdup(purple_status_get_name(status));
			text = g_markup_escape_text(psm_str, -1);
			g_free(psm_str);
			return text;
		}
	}

	return NULL;
}

static void
msn_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	MsnUser *user;
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);

	user = buddy->proto_data;

	
	if (purple_presence_is_online(presence))
	{
		const char *psm, *currentmedia;
		char *tmp;

		psm = purple_status_get_attr_string(status, "message");
		currentmedia = purple_status_get_attr_string(status, "currentmedia");

		purple_notify_user_info_add_pair(user_info, _("Status"),
									   (purple_presence_is_idle(presence) ? _("Idle") : purple_status_get_name(status)));
		if (psm) {
			tmp = g_markup_escape_text(psm, -1);
			purple_notify_user_info_add_pair(user_info, _("PSM"), tmp);
			g_free(tmp);
		}
		if (currentmedia) {
			tmp = g_markup_escape_text(currentmedia, -1);
			purple_notify_user_info_add_pair(user_info, _("Current media"), tmp);
			g_free(tmp);
		}
	}
	
	if (full && user)
	{
		purple_notify_user_info_add_pair(user_info, _("Has you"),
									   ((user->list_op & (1 << MSN_LIST_RL)) ? _("Yes") : _("No")));
	}

	/* XXX: This is being shown in non-full tooltips because the
	 * XXX: blocked icon overlay isn't always accurate for MSN.
	 * XXX: This can die as soon as purple_privacy_check() knows that
	 * XXX: this prpl always honors both the allow and deny lists. */
	if (user)
	{
		purple_notify_user_info_add_pair(user_info, _("Blocked"),
									   ((user->list_op & (1 << MSN_LIST_BL)) ? _("Yes") : _("No")));
	}
}

static GList *
msn_status_types(PurpleAccount *account)
{
	PurpleStatusType *status;
	GList *types = NULL;
#if 0
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,
			NULL, NULL, FALSE, TRUE, FALSE);
#endif
	status = purple_status_type_new_with_attrs(
				PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE,
				"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
				"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
				NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "brb", _("Be Right Back"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "busy", _("Busy"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "phone", _("On the Phone"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "lunch", _("Out to Lunch"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			"currentmedia", _("Current media"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_full(PURPLE_STATUS_MOBILE,
			"mobile", NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, status);
	
	return types;
}

static GList *
msn_actions(PurplePlugin *plugin, gpointer context)
{
	PurpleConnection *gc = (PurpleConnection *)context;
	PurpleAccount *account;
	const char *user;

	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Set Friendly Name..."),
								 msn_show_set_friendly_name);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

	act = purple_plugin_action_new(_("Set Home Phone Number..."),
								 msn_show_set_home_phone);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Set Work Phone Number..."),
			msn_show_set_work_phone);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Set Mobile Phone Number..."),
			msn_show_set_mobile_phone);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

#if 0
	act = purple_plugin_action_new(_("Enable/Disable Mobile Devices..."),
			msn_show_set_mobile_support);
	m = g_list_append(m, act);
#endif

	act = purple_plugin_action_new(_("Allow/Disallow Mobile Pages..."),
			msn_show_set_mobile_pages);
	m = g_list_append(m, act);

	account = purple_connection_get_account(gc);
	user = msn_normalize(account, purple_account_get_username(account));

	if (strstr(user, "@hotmail.com") != NULL)
	{
		m = g_list_append(m, NULL);
		act = purple_plugin_action_new(_("Open Hotmail Inbox"),
				msn_show_hotmail_inbox);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_buddy_menu(PurpleBuddy *buddy)
{
	MsnUser *user;

	GList *m = NULL;
	PurpleMenuAction *act;

	g_return_val_if_fail(buddy != NULL, NULL);

	user = buddy->proto_data;

	if (user != NULL)
	{
		if (user->mobile)
		{
			act = purple_menu_action_new(_("Send to Mobile"),
			                           PURPLE_CALLBACK(show_send_to_mobile_cb),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (g_ascii_strcasecmp(buddy->name,
	                       purple_account_get_username(buddy->account)))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
		                           PURPLE_CALLBACK(initiate_chat_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		return msn_buddy_menu((PurpleBuddy *) node);
	}
	else
	{
		return NULL;
	}
}

static void
msn_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	MsnSession *session;
	const char *username;
	const char *host;
	gboolean http_method = FALSE;
	int port;

	gc = purple_account_get_connection(account);

	if (!purple_ssl_is_supported())
	{
		gc->wants_to_die = TRUE;
		purple_connection_error(gc,
			_("SSL support is needed for MSN. Please install a supported "
			  "SSL library."));
		return;
	}

	if (purple_account_get_bool(account, "http_method", FALSE))
		http_method = TRUE;

	host = purple_account_get_string(account, "server", MSN_SERVER);
	port = purple_account_get_int(account, "port", MSN_PORT);

	session = msn_session_new(account);

	gc->proto_data = session;
	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_FORMATTING_WBFO | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_URLDESC;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_START);

	/* Hmm, I don't like this. */
	/* XXX shx: Me neither */
	username = msn_normalize(account, purple_account_get_username(account));

	if (strcmp(username, purple_account_get_username(account)))
		purple_account_set_username(account, username);

	if (!msn_session_connect(session, host, port, http_method))
		purple_connection_error(gc, _("Failed to connect to server."));
}

static void
msn_close(PurpleConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	g_return_if_fail(session != NULL);

	msn_session_destroy(session);

	gc->proto_data = NULL;
}

static int
msn_send_im(PurpleConnection *gc, const char *who, const char *message,
			PurpleMessageFlags flags)
{
	PurpleAccount *account;
	PurpleBuddy *buddy = purple_find_buddy(gc->account, who);
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;

	purple_debug_info("MaYuan","send IM {%s} to %s\n",message,who);
	account = purple_connection_get_account(gc);

	if (buddy) {
		PurplePresence *p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			char *text = purple_markup_strip_html(message);
			send_to_mobile(gc, who, text);
			g_free(text);
			return 1;
		}
	}

	msn_import_html(message, &msgformat, &msgtext);
	if(msn_user_is_online(account, who)||
		msn_user_is_yahoo(account, who)){
		/*User online,then send Online Instant Message*/

		if (strlen(msgtext) + strlen(msgformat) + strlen(VERSION) > 1564)
		{
			g_free(msgformat);
			g_free(msgtext);

			return -E2BIG;
		}

		msg = msn_message_new_plain(msgtext);
		msg->remote_user = g_strdup(who);
		msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);

		g_free(msgformat);
		g_free(msgtext);

		purple_debug_info("MaYuan","prepare to send online Message\n");
		if (g_ascii_strcasecmp(who, purple_account_get_username(account)))
		{
			MsnSession *session;
			MsnSwitchBoard *swboard;

			session = gc->proto_data;
			if(msn_user_is_yahoo(account,who)){
				/*we send the online and offline Message to Yahoo User via UBM*/
				purple_debug_info("MaYuan","send to Yahoo User\n");
				uum_send_msg(session,msg);
			}else{
				purple_debug_info("MaYuan","send via switchboard\n");
				swboard = msn_session_get_swboard(session, who, MSN_SB_FLAG_IM);
				msn_switchboard_send_msg(swboard, msg, TRUE);
			}
		}
		else
		{
			char *body_str, *body_enc, *pre, *post;
			const char *format;
			/*
			 * In MSN, you can't send messages to yourself, so
			 * we'll fake like we received it ;)
			 */
			body_str = msn_message_to_string(msg);
			body_enc = g_markup_escape_text(body_str, -1);
			g_free(body_str);

			format = msn_message_get_attr(msg, "X-MMS-IM-Format");
			msn_parse_format(format, &pre, &post);
			body_str = g_strdup_printf("%s%s%s", pre ? pre :  "",
									   body_enc ? body_enc : "", post ? post : "");
			g_free(body_enc);
			g_free(pre);
			g_free(post);

			serv_got_typing_stopped(gc, who);
			serv_got_im(gc, who, body_str, flags, time(NULL));
			g_free(body_str);
		}

		msn_message_destroy(msg);
	}else	{
		/*send Offline Instant Message,only to MSN Passport User*/
		MsnSession *session;
		MsnOim *oim;
		char *friendname;
		
		purple_debug_info("MaYuan","prepare to send offline Message\n");		
		session = gc->proto_data;
		/* XXX/khc: hack */
		if (!session->oim)
			session->oim = msn_oim_new(session);

		oim = session->oim;
		friendname = msn_encode_mime(account->username);
		msn_oim_prep_send_msg_info(oim, purple_account_get_username(account),
								   friendname, who,	message);
		msn_oim_send_msg(oim);
	}
	return 1;
}

static unsigned int
msn_send_typing(PurpleConnection *gc, const char *who, PurpleTypingState state)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;

	/*
	 * TODO: I feel like this should be "if (state != PURPLE_TYPING)"
	 *       but this is how it was before, and I don't want to break
	 *       anything. --KingAnt
	 */
	if (state == PURPLE_NOT_TYPING)
		return 0;

	if (!g_ascii_strcasecmp(who, purple_account_get_username(account)))
	{
		/* We'll just fake it, since we're sending to ourself. */
		serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, PURPLE_TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_swboard(session, who);

	if (swboard == NULL || !msn_switchboard_can_send(swboard))
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msg = msn_message_new(MSN_MSG_TYPING);
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_flag(msg, 'U');
	msn_message_set_attr(msg, "TypingUser",
						 purple_account_get_username(account));
	msn_message_set_bin_data(msg, "\r\n", 2);

	msn_switchboard_send_msg(swboard, msg, FALSE);

	msn_message_destroy(msg);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = purple_account_get_connection(account);

	if (gc != NULL){
		session = gc->proto_data;
		msn_change_status(session);
	}
}

static void
msn_set_idle(PurpleConnection *gc, int idle)
{
	MsnSession *session;

	session = gc->proto_data;

	msn_change_status(session);
}

#if 0
static void
fake_userlist_add_buddy(MsnUserList *userlist,
					   const char *who, int list_id,
					   const char *group_name)
{
	MsnUser *user;
	static int group_id_c = 1;
	int group_id;

	group_id = -1;

	if (group_name != NULL)
	{
		MsnGroup *group;
		group = msn_group_new(userlist, group_id_c, group_name);
		group_id = group_id_c++;
	}

	user = msn_userlist_find_user(userlist, who);

	if (user == NULL)
	{
		user = msn_user_new(userlist, who, NULL);
		msn_userlist_add_user(userlist, user);
	}
	else
		if (user->list_op & (1 << list_id))
		{
			if (list_id == MSN_LIST_FL)
			{
				if (group_id >= 0)
					if (g_list_find(user->group_ids,
									GINT_TO_POINTER(group_id)))
						return;
			}
			else
				return;
		}

	if (group_id >= 0)
	{
		user->group_ids = g_list_append(user->group_ids,
										GINT_TO_POINTER(group_id));
	}

	user->list_op |= (1 << list_id);
}
#endif

static void
msn_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;
	const char *who;

	session = gc->proto_data;
	userlist = session->userlist;
	who = msn_normalize(gc->account, buddy->name);

	purple_debug_info("MaYuan","add user:{%s} to group:{%s}\n",who,group->name);
	if (!session->logged_in)
	{
#if 0
		fake_userlist_add_buddy(session->sync_userlist, who, MSN_LIST_FL,
								group ? group->name : NULL);
#else
		purple_debug_error("msn", "msn_add_buddy called before connected\n");
#endif

		return;
	}

#if 0
	if (group != NULL && group->name != NULL)
		purple_debug_info("msn", "msn_add_buddy: %s, %s\n", who, group->name);
	else
		purple_debug_info("msn", "msn_add_buddy: %s\n", who);
#endif

#if 0
	/* Which is the max? */
	if (session->fl_users_count >= 150)
	{
		purple_debug_info("msn", "Too many buddies\n");
		/* Buddy list full */
		/* TODO: purple should be notified of this */
		return;
	}
#endif

	/* XXX - Would group ever be NULL here?  I don't think so...
	 * shx: Yes it should; MSN handles non-grouped buddies, and this is only
	 * internal. */
	msn_userlist_add_buddy(userlist, who, MSN_LIST_FL,
						   group ? group->name : NULL);
}

static void
msn_rem_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	/* XXX - Does buddy->name need to be msn_normalize'd here?  --KingAnt */
	msn_userlist_rem_buddy(userlist, buddy->name, MSN_LIST_FL, group->name);
}

static void
msn_add_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_BL_OP)
		msn_userlist_rem_buddy(userlist, who, MSN_LIST_BL, NULL);

	msn_userlist_add_buddy(userlist, who, MSN_LIST_AL, NULL);
}

static void
msn_add_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_AL_OP)
		msn_userlist_rem_buddy(userlist, who, MSN_LIST_AL, NULL);

	msn_userlist_add_buddy(userlist, who, MSN_LIST_BL, NULL);
}

static void
msn_rem_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy(userlist, who, MSN_LIST_AL, NULL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy(userlist, who, MSN_LIST_BL, NULL);
}

static void
msn_rem_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy(userlist, who, MSN_LIST_BL, NULL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy(userlist, who, MSN_LIST_AL, NULL);
}

static void
msn_set_permit_deny(PurpleConnection *gc)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnCmdProc *cmdproc;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (account->perm_deny == PURPLE_PRIVACY_ALLOW_ALL ||
		account->perm_deny == PURPLE_PRIVACY_DENY_USERS){
		msn_cmdproc_send(cmdproc, "BLP", "%s", "AL");
	}else{
		msn_cmdproc_send(cmdproc, "BLP", "%s", "BL");
	}
}

static void
msn_chat_invite(PurpleConnection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = gc->proto_data;

	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
	{
		/* if we have no switchboard, everyone else left the chat already */
		swboard = msn_switchboard_new(session);
		msn_switchboard_request(swboard);
		swboard->chat_id = id;
		swboard->conv = purple_find_chat(gc, id);
	}

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_switchboard_request_add_user(swboard, who);
}

static void
msn_chat_leave(PurpleConnection *gc, int id)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = gc->proto_data;

	swboard = msn_session_find_swboard_with_id(session, id);

	/* if swboard is NULL we were the only person left anyway */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	msn_switchboard_release(swboard, MSN_SB_FLAG_IM);

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static int
msn_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;
	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
		return -EINVAL;

	if (!swboard->ready)
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_import_html(message, &msgformat, &msgtext);

	if (strlen(msgtext) + strlen(msgformat) + strlen(VERSION) > 1564)
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);
	msn_switchboard_send_msg(swboard, msg, FALSE);
	msn_message_destroy(msg);

	g_free(msgformat);
	g_free(msgtext);

	serv_got_chat_in(gc, id, purple_account_get_username(account), 0,
					 message, time(NULL));

	return 0;
}

static void
msn_keepalive(PurpleConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	if (!session->http_method)
	{
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		msn_cmdproc_send_quick(cmdproc, "PNG", NULL, NULL);
	}
}

static void
msn_group_buddy(PurpleConnection *gc, const char *who,
				const char *old_group_name, const char *new_group_name)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	msn_userlist_move_buddy(userlist, who, old_group_name, new_group_name);
}

static void
msn_rename_group(PurpleConnection *gc, const char *old_name,
				 PurpleGroup *group, GList *moved_buddies)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	const char *old_gid;
	const char *enc_new_group_name;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	enc_new_group_name = purple_url_encode(group->name);

	purple_debug_info("MaYuan","rename group:old{%s},new{%s}",old_name,enc_new_group_name);
	old_gid = msn_userlist_find_group_id(session->userlist, old_name);

	if (old_gid != NULL){
		/*find a Group*/
		msn_cmdproc_send(cmdproc, "REG", "%d %s 0", old_gid,
						 enc_new_group_name);
	}else{
		/*not found*/
		msn_cmdproc_send(cmdproc, "ADG", "%s 0", enc_new_group_name);
	}
}

static void
msn_convo_closed(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = gc->proto_data;

	swboard = msn_session_find_swboard(session, who);

	/*
	 * Don't perform an assertion here. If swboard is NULL, then the
	 * switchboard was either closed by the other party, or the person
	 * is talking to himself.
	 */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	msn_switchboard_release(swboard, MSN_SB_FLAG_IM);

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static void
msn_set_buddy_icon(PurpleConnection *gc, const char *filename)
{
	MsnSession *session;
	MsnUser *user;

	session = gc->proto_data;
	user = session->user;

	msn_user_set_buddy_icon(user, filename);

	msn_change_status(session);
}

static void
msn_remove_group(PurpleConnection *gc, PurpleGroup *group)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	const char *group_id;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	/*we can't delete the default group*/
	if(!strcmp(group->name,MSN_INDIVIDUALS_GROUP_NAME)||
		!strcmp(group->name,MSN_NON_IM_GROUP_NAME)){
		return ;
	}
	group_id = msn_userlist_find_group_id(session->userlist, group->name);
	if (group_id != NULL){
		msn_del_group(session,group_id);
	}
}

/**
 * Extract info text from info_data and add it to user_info
 */
static gboolean
msn_tooltip_extract_info_text(PurpleNotifyUserInfo *user_info, MsnGetInfoData *info_data)
{
	PurpleBuddy *b;

	b = purple_find_buddy(purple_connection_get_account(info_data->gc),
						info_data->name);

	if (b){
		char *tmp;

		if (b->alias && b->alias[0]){
			char *aliastext = g_markup_escape_text(b->alias, -1);
			purple_notify_user_info_add_pair(user_info, _("Alias"), aliastext);
			g_free(aliastext);
		}

		if (b->server_alias){
			char *nicktext = g_markup_escape_text(b->server_alias, -1);
			tmp = g_strdup_printf("<font sml=\"msn\">%s</font><br>", nicktext);
			purple_notify_user_info_add_pair(user_info, _("Nickname"), tmp);
			g_free(tmp);
			g_free(nicktext);
		}

		/* Add the tooltip information */
		msn_tooltip_text(b, user_info, TRUE);

		return TRUE;
	}

	return FALSE;
}

#if PHOTO_SUPPORT

static char *
msn_get_photo_url(const char *url_text)
{
	char *p, *q;

	if ((p = strstr(url_text, PHOTO_URL)) != NULL){
		p += strlen(PHOTO_URL);
	}
	if (p && (strncmp(p, "http://",strlen("http://")) == 0) && ((q = strchr(p, '"')) != NULL))
			return g_strndup(p, q - p);

	return NULL;
}

static void msn_got_photo(PurpleUtilFetchUrlData *url_data, gpointer data,
		const gchar *url_text, size_t len, const gchar *error_message);

#endif

#if 0
static char *msn_info_date_reformat(const char *field, size_t len)
{
	char *tmp = g_strndup(field, len);
	time_t t = purple_str_to_time(tmp, FALSE, NULL, NULL, NULL);

	g_free(tmp);
	return g_strdup(purple_date_format_short(localtime(&t)));
}
#endif

#define MSN_GOT_INFO_GET_FIELD(a, b) \
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info, \
			"\n" a ":", 0, "\n", 0, "Undisclosed", b, 0, NULL, NULL); \
	if (found) \
		sect_info = TRUE;

static void
msn_got_info(PurpleUtilFetchUrlData *url_data, gpointer data,
		const gchar *url_text, size_t len, const gchar *error_message)
{
	MsnGetInfoData *info_data = (MsnGetInfoData *)data;
	PurpleNotifyUserInfo *user_info;
	char *stripped, *p, *q, *tmp;
	char *user_url = NULL;
	gboolean found;
	gboolean has_tooltip_text = FALSE;
	gboolean has_info = FALSE;
	gboolean sect_info = FALSE;
	gboolean has_contact_info = FALSE;
	char *url_buffer;
	GString *s, *s2;
	int stripped_len;
#if PHOTO_SUPPORT
	char *photo_url_text = NULL;
	MsnGetInfoStepTwoData *info2_data = NULL;
#endif

	purple_debug_info("msn", "In msn_got_info,url_text:{%s}\n",url_text);

	/* Make sure the connection is still valid */
	if (g_list_find(purple_connections_get_all(), info_data->gc) == NULL)
	{
		purple_debug_warning("msn", "invalid connection. ignoring buddy info.\n");
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	user_info = purple_notify_user_info_new();
	has_tooltip_text = msn_tooltip_extract_info_text(user_info, info_data);

	if (error_message != NULL || url_text == NULL || strcmp(url_text, "") == 0)
	{
		tmp = g_strdup_printf("<b>%s</b>", _("Error retrieving profile"));
		purple_notify_user_info_add_pair(user_info, NULL, tmp);
		g_free(tmp);

		purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);
		purple_notify_user_info_destroy(user_info);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	url_buffer = g_strdup(url_text);

	/* If they have a homepage link, MSN masks it such that we need to
	 * fetch the url out before purple_markup_strip_html() nukes it */
	/* I don't think this works with the new spaces profiles - Stu 3/2/06 */
	if ((p = strstr(url_text,
			"Take a look at my </font><A class=viewDesc title=\"")) != NULL)
	{
		p += 50;

		if ((q = strchr(p, '"')) != NULL)
			user_url = g_strndup(p, q - p);
	}

	/*
	 * purple_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	 */
	while ((p = strstr(url_buffer, "&nbsp;")) != NULL)
	{
		*p = ' '; /* Turn &nbsp;'s into ordinary blanks */
		p += 1;
		memmove(p, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 5] = '\0';
	}

	while ((p = strstr(url_buffer, "&#183;")) != NULL)
	{
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}

	/* Nuke the nasty \r's that just get in the way */
	purple_str_strip_char(url_buffer, '\r');

	/* MSN always puts in &#39; for apostrophes...replace them */
	while ((p = strstr(url_buffer, "&#39;")) != NULL)
	{
		*p = '\'';
		memmove(p + 1, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 4] = '\0';
	}

	/* Nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = purple_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	purple_debug_misc("msn", "stripped = %p\n", stripped);
	purple_debug_misc("msn", "url_buffer = %p\n", url_buffer);

	/* Gonna re-use the memory we've already got for url_buffer */
	/* No we're not. */
	s = g_string_sized_new(strlen(url_buffer));
	s2 = g_string_sized_new(strlen(url_buffer));
	
	/* General section header */
	if (has_tooltip_text)
		purple_notify_user_info_add_section_break(user_info);
	
	purple_notify_user_info_add_section_header(user_info, _("General"));
	
	/* Extract their Name and put it in */
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	
	/* General */
	MSN_GOT_INFO_GET_FIELD("Nickname", _("Nickname"));
	MSN_GOT_INFO_GET_FIELD("Age", _("Age"));
	MSN_GOT_INFO_GET_FIELD("Gender", _("Gender"));
	MSN_GOT_INFO_GET_FIELD("Occupation", _("Occupation"));
	MSN_GOT_INFO_GET_FIELD("Location", _("Location"));

	/* Extract their Interests and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info,
			"\nInterests\t", 0, " (/default.aspx?page=searchresults", 0,
			"Undisclosed", _("Hobbies and Interests") /* _("Interests") */,
			0, NULL, NULL);

	if (found)
		sect_info = TRUE;

	MSN_GOT_INFO_GET_FIELD("More about me", _("A Little About Me"));
	
	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else 
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		if (has_tooltip_text)
			purple_notify_user_info_remove_last_item(user_info);
	}
											   
	/* Social */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Social"));
										   
	MSN_GOT_INFO_GET_FIELD("Marital status", _("Marital Status"));
	MSN_GOT_INFO_GET_FIELD("Interested in", _("Interests"));
	MSN_GOT_INFO_GET_FIELD("Pets", _("Pets"));
	MSN_GOT_INFO_GET_FIELD("Hometown", _("Hometown"));
	MSN_GOT_INFO_GET_FIELD("Places lived", _("Places Lived"));
	MSN_GOT_INFO_GET_FIELD("Fashion", _("Fashion"));
	MSN_GOT_INFO_GET_FIELD("Humor", _("Humor"));
	MSN_GOT_INFO_GET_FIELD("Music", _("Music"));
	MSN_GOT_INFO_GET_FIELD("Favorite quote", _("Favorite Quote"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else 
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Contact Info */
	/* Personal */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
	purple_notify_user_info_add_section_header(user_info, _("Personal"));

	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Significant other", _("Significant Other"));
	MSN_GOT_INFO_GET_FIELD("Home phone", _("Home Phone"));
	MSN_GOT_INFO_GET_FIELD("Home phone 2", _("Home Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Home address", _("Home Address"));
	MSN_GOT_INFO_GET_FIELD("Personal Mobile", _("Personal Mobile"));
	MSN_GOT_INFO_GET_FIELD("Home fax", _("Home Fax"));
	MSN_GOT_INFO_GET_FIELD("Personal e-mail", _("Personal E-Mail"));
	MSN_GOT_INFO_GET_FIELD("Personal IM", _("Personal IM"));
	MSN_GOT_INFO_GET_FIELD("Birthday", _("Birthday"));
	MSN_GOT_INFO_GET_FIELD("Anniversary", _("Anniversary"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
		has_contact_info = TRUE;
	}
    else 
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Business */
	purple_notify_user_info_add_section_header(user_info, _("Work"));
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Job title", _("Job Title"));
	MSN_GOT_INFO_GET_FIELD("Company", _("Company"));
	MSN_GOT_INFO_GET_FIELD("Department", _("Department"));
	MSN_GOT_INFO_GET_FIELD("Profession", _("Profession"));
	MSN_GOT_INFO_GET_FIELD("Work phone 1", _("Work Phone"));
	MSN_GOT_INFO_GET_FIELD("Work phone 2", _("Work Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Work address", _("Work Address"));
	MSN_GOT_INFO_GET_FIELD("Work mobile", _("Work Mobile"));
	MSN_GOT_INFO_GET_FIELD("Work pager", _("Work Pager"));
	MSN_GOT_INFO_GET_FIELD("Work fax", _("Work Fax"));
	MSN_GOT_INFO_GET_FIELD("Work e-mail", _("Work E-Mail"));
	MSN_GOT_INFO_GET_FIELD("Work IM", _("Work IM"));
	MSN_GOT_INFO_GET_FIELD("Start date", _("Start Date"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
		has_contact_info = TRUE;
	}
    else 
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	if (!has_contact_info)
	{
		/* Remove the Contact Info section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

#if 0 /* these probably don't show up any more */
	/*
	 * The fields, 'A Little About Me', 'Favorite Things', 'Hobbies
	 * and Interests', 'Favorite Quote', and 'My Homepage' may or may
	 * not appear, in any combination. However, they do appear in
	 * certain order, so we can successively search to pin down the
	 * distinct values.
	 */

	/* Check if they have A Little About Me */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" A Little About Me \n\n", 0, "Favorite Things", '\n', NULL,
			_("A Little About Me"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Hobbies and Interests", '\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Favorite Quote", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "My Homepage \n\nTake a look",
				'\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "last updated", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Things */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Favorite Things \n\n", 0, "Hobbies and Interests", '\n', NULL,
			_("Favorite Things"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "Favorite Quote", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "My Homepage \n\nTake a look", '\n',
				NULL, _("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Hobbies and Interests */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Hobbies and Interests \n\n", 0, "Favorite Quote", '\n', NULL,
			_("Hobbies and Interests"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "My Homepage \n\nTake a look",
				'\n', NULL, _("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "last updated", '\n', NULL,
				_("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Quote */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			"Favorite Quote \n\n", 0, "My Homepage \n\nTake a look", '\n', NULL,
			_("Favorite Quote"), 0, NULL, NULL);

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Quote \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Quote"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Extract the last updated date and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" last updated:", 1, "\n", 0, NULL, _("Last Updated"), 0,
			NULL, msn_info_date_reformat);

	if (found)
		has_info = TRUE;
#endif

	/* If we were able to fetch a homepage url earlier, stick it in there */
	if (user_url != NULL)
	{
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", user_url, user_url);
		purple_notify_user_info_add_pair(user_info, _("Homepage"), tmp);
		g_free(tmp);
		g_free(user_url);

		has_info = TRUE;
	}

	if (!has_info)
	{
		/* MSN doesn't actually distinguish between "unknown member" and
		 * a known member with an empty profile. Try to explain this fact.
		 * Note that if we have a nonempty tooltip_text, we know the user
		 * exists.
		 */
		/* This doesn't work with the new spaces profiles - Stu 3/2/06
		char *p = strstr(url_buffer, "Unknown Member </TITLE>");
		 * This might not work for long either ... */
		char *p = strstr(url_buffer, "form id=\"SpacesSearch\" name=\"SpacesSearch\"");
		PurpleBuddy *b = purple_find_buddy
				(purple_connection_get_account(info_data->gc), info_data->name);
		purple_notify_user_info_add_pair(user_info, _("Error retrieving profile"),
									   ((p && b) ? _("The user has not created a public profile.") :
										(p ? _("MSN reported not being able to find the user's profile. "
											   "This either means that the user does not exist, "
											   "or that the user exists "
											   "but has not created a public profile.") :
										 _("Could not find "	/* This should never happen */
										   "any information in the user's profile. "
										   "The user most likely does not exist."))));
	}

	/* put a link to the actual profile URL */
	tmp = g_strdup_printf("<a href=\"%s%s\">%s%s</a>",
					PROFILE_URL, info_data->name, PROFILE_URL, info_data->name);
	purple_notify_user_info_add_pair(user_info, _("Profile URL"), tmp);
	g_free(tmp);									   

#if PHOTO_SUPPORT
	/* Find the URL to the photo; must be before the marshalling [Bug 994207] */
	photo_url_text = msn_get_photo_url(url_text);
	purple_debug_info("Ma Yuan","photo url:{%s}\n",photo_url_text);

	/* Marshall the existing state */
	info2_data = g_malloc0(sizeof(MsnGetInfoStepTwoData));
	info2_data->info_data = info_data;
	info2_data->stripped = stripped;
	info2_data->url_buffer = url_buffer;
	info2_data->user_info = user_info;
	info2_data->photo_url_text = photo_url_text;

	/* Try to put the photo in there too, if there's one */
	if (photo_url_text)
	{
		purple_util_fetch_url(photo_url_text, FALSE, NULL, FALSE, msn_got_photo,
					   info2_data);
	}
	else
	{
		/* Emulate a callback */
		/* TODO: Huh? */
		msn_got_photo(NULL, info2_data, NULL, 0, NULL);
	}
}

static void
msn_got_photo(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *url_text, size_t len, const gchar *error_message)
{
	MsnGetInfoStepTwoData *info2_data = (MsnGetInfoStepTwoData *)user_data;
	int id = -1;

	/* Unmarshall the saved state */
	MsnGetInfoData *info_data = info2_data->info_data;
	char *stripped = info2_data->stripped;
	char *url_buffer = info2_data->url_buffer;
	PurpleNotifyUserInfo *user_info = info2_data->user_info;
	char *photo_url_text = info2_data->photo_url_text;

	/* Make sure the connection is still valid if we got here by fetching a photo url */
	if (url_text && (error_message != NULL ||
					 g_list_find(purple_connections_get_all(), info_data->gc) == NULL))
	{
		purple_debug_warning("msn", "invalid connection. ignoring buddy photo info.\n");
		g_free(stripped);
		g_free(url_buffer);
		g_free(user_info);
		g_free(info_data->name);
		g_free(info_data);
		g_free(photo_url_text);
		g_free(info2_data);

		return;
	}

	/* Try to put the photo in there too, if there's one and is readable */
	if (user_data && url_text && len != 0)
	{
		if (strstr(url_text, "400 Bad Request")
			|| strstr(url_text, "403 Forbidden")
			|| strstr(url_text, "404 Not Found"))
		{

			purple_debug_info("msn", "Error getting %s: %s\n",
					photo_url_text, url_text);
		}
		else
		{
			char buf[1024];
			purple_debug_info("msn", "%s is %d bytes\n", photo_url_text, len);
			id = purple_imgstore_add(url_text, len, NULL);
			g_snprintf(buf, sizeof(buf), "<img id=\"%d\"><br>", id);
			purple_notify_user_info_prepend_pair(user_info, NULL, buf);
		}
	}

	/* We continue here from msn_got_info, as if nothing has happened */
#endif
	purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);

	g_free(stripped);
	g_free(url_buffer);
	purple_notify_user_info_destroy(user_info);
	g_free(info_data->name);
	g_free(info_data);
#if PHOTO_SUPPORT
	g_free(photo_url_text);
	g_free(info2_data);
	if (id != -1)
		purple_imgstore_unref(id);
#endif
}

static void
msn_get_info(PurpleConnection *gc, const char *name)
{
	MsnGetInfoData *data;
	char *url;

	data       = g_new0(MsnGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s", PROFILE_URL, name);

	purple_util_fetch_url(url, FALSE,
				   "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)",
				   TRUE, msn_got_info, data);

	g_free(url);
}

static gboolean msn_load(PurplePlugin *plugin)
{
	msn_notification_init();
	msn_switchboard_init();
	msn_sync_init();

	return TRUE;
}

static gboolean msn_unload(PurplePlugin *plugin)
{
	msn_notification_end();
	msn_switchboard_end();
	msn_sync_end();

	return TRUE;
}

static PurpleAccount *find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean msn_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	char *acct_id = g_hash_table_lookup(params, "account");
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "msnim"))
		return FALSE;

	acct = find_acct("prpl-msn", acct_id);

	if (!acct)
		return FALSE;

	/* msnim:chat?contact=user@domain.tld */
	if (!g_ascii_strcasecmp(cmd, "Chat")) {
		char *sname = g_hash_table_lookup(params, "contact");
		if (sname) {
			PurpleConversation *conv = purple_find_conversation_with_account(
				PURPLE_CONV_TYPE_IM, sname, acct);
			if (conv == NULL)
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, acct, sname);
			purple_conversation_present(conv);
		}
		/*else
			**If pidgindialogs_im() was in the core, we could use it here.
			 * It is all purple_request_* based, but I'm not sure it really belongs in the core
			pidgindialogs_im();*/

		return TRUE;
	}
	/* msnim:add?contact=user@domain.tld */
	else if (!g_ascii_strcasecmp(cmd, "Add")) {
		char *name = g_hash_table_lookup(params, "contact");
		purple_blist_request_add_buddy(acct, name, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}


static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"png", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_SEND},	/* icon_spec */
	msn_list_icon,			/* list_icon */
	NULL,				/* list_emblems */
	msn_status_text,		/* status_text */
	msn_tooltip_text,		/* tooltip_text */
	msn_status_types,		/* away_states */
	msn_blist_node_menu,		/* blist_node_menu */
	NULL,					/* chat_info */
	NULL,					/* chat_info_defaults */
	msn_login,			/* login */
	msn_close,			/* close */
	msn_send_im,			/* send_im */
	NULL,					/* set_info */
	msn_send_typing,		/* send_typing */
	msn_get_info,			/* get_info */
	msn_set_status,			/* set_away */
	msn_set_idle,			/* set_idle */
	NULL,					/* change_passwd */
	msn_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	msn_rem_buddy,			/* remove_buddy */
	NULL,					/* remove_buddies */
	msn_add_permit,			/* add_permit */
	msn_add_deny,			/* add_deny */
	msn_rem_permit,			/* rem_permit */
	msn_rem_deny,			/* rem_deny */
	msn_set_permit_deny,	/* set_permit_deny */
	NULL,					/* join_chat */
	NULL,					/* reject chat invite */
	NULL,					/* get_chat_name */
	msn_chat_invite,		/* chat_invite */
	msn_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	msn_chat_send,			/* chat_send */
	msn_keepalive,			/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	msn_group_buddy,		/* group_buddy */
	msn_rename_group,		/* rename_group */
	NULL,					/* buddy_free */
	msn_convo_closed,		/* convo_closed */
	msn_normalize,			/* normalize */
	msn_set_buddy_icon,		/* set_buddy_icon */
	msn_remove_group,		/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	msn_can_receive_file,	/* can_receive_file */
	msn_send_file,			/* send_file */
	msn_new_xfer,			/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,					/* send_raw */
	NULL,					/* roomlist_room_serialize */
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-msn",                                       /**< id             */
	"MSN",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Windows Live Messenger Protocol Plugin"),
	                                                  /**  description    */
	N_("Windows Live Messenger Protocol Plugin"),
	"MaYuan <mayuan2006@gmail.com>",				/**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	msn_load,                                         /**< load           */
	msn_unload,                                       /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	msn_actions
};

static void
init_plugin(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

	option = purple_account_option_string_new(_("Server"), "server",
											WLM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_int_new(_("Port"), "port", WLM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_bool_new(_("Use HTTP Method"),
										  "http_method", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_bool_new(_("Show custom smileys"),
										  "custom_smileys", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	purple_cmd_register("nudge", "", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY,
	                 "prpl-msn", msn_cmd_nudge,
	                  _("nudge: nudge a user to get their attention"), NULL);

	purple_prefs_remove("/plugins/prpl/msn");

	purple_signal_connect(purple_get_core(), "uri-handler", plugin,
		PURPLE_CALLBACK(msn_uri_handler), NULL);
}

PURPLE_INIT_PLUGIN(msn, init_plugin, info);
