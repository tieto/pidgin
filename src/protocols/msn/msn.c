/**
 * @file msn.c The MSN protocol plugin
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"
#include "msg.h"
#include "session.h"
#include "state.h"
#include "utils.h"

#define BUDDY_ALIAS_MAXLEN 388

static GaimPlugin *my_protocol = NULL;

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;

static char *msn_normalize(const char *str);

static void
msn_act_id(gpointer data, char *entry)
{
	struct gaim_connection *gc = data;
	MsnSession *session = gc->proto_data;
	char outparams[MSN_BUF_LEN];
	char *alias;

	if (entry == NULL || *entry == '\0') 
		alias = g_strdup("");
	else
		alias = g_strdup(entry);

	if (strlen(alias) >= BUDDY_ALIAS_MAXLEN) {
		do_error_dialog(_("Your new MSN friendly name is too long."),
						NULL, GAIM_ERROR);
		return;
	}

	g_snprintf(outparams, sizeof(outparams), "%s %s",
			   gc->username, msn_url_encode(alias));

	g_free(alias);

	if (!msn_servconn_send_command(session->notification_conn,
								   "REA", outparams)) {

		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_show_set_friendly_name(struct gaim_connection *gc)
{
	do_prompt_dialog(_("Set Friendly Name:"), gc->displayname,
					 gc, msn_act_id, NULL);
}


/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(struct gaim_account *a, struct buddy *b)
{
	return "msn";
}

static void
msn_list_emblems(struct buddy *b, char **se, char **sw,
				 char **nw, char **ne)
{
	char *emblems[4] = { NULL, NULL, NULL, NULL };
	int away_type = MSN_AWAY_TYPE(b->uc);
	int i = 0;

	if (b->present == GAIM_BUDDY_OFFLINE)
		emblems[i++] = "offline";
	else if (away_type == MSN_BUSY || away_type == MSN_PHONE)
		emblems[i++] = "occupied";
	else if (away_type != 0)
		emblems[i++] = "away";

	if (MSN_MOBILE(b->uc))
		emblems[i++] = "wireless";

	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *
msn_status_text(struct buddy *b)
{
	if (b->uc & UC_UNAVAILABLE)
		return g_strdup(msn_away_get_text(MSN_AWAY_TYPE(b->uc)));

	return NULL;
}

static char *
msn_tooltip_text(struct buddy *b)
{
	if (GAIM_BUDDY_IS_ONLINE(b)) {
		return g_strdup_printf(_("<b>Status:</b> %s"),
							   msn_away_get_text(MSN_AWAY_TYPE(b->uc)));
	}

	return NULL;
}

static GList *
msn_away_states(struct gaim_connection *gc)
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
msn_actions(struct gaim_connection *gc)
{
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set Friendly Name");
	pam->callback = msn_show_set_friendly_name;
	pam->gc = gc;
	m = g_list_append(m, pam);

	return m;
}

static GList *
msn_buddy_menu(struct gaim_connection *gc, const char *who)
{
	GList *m = NULL;

	return m;
}

static void
msn_login(struct gaim_account *account)
{
	struct gaim_connection *gc;
	MsnSession *session;
	const char *server;
	int port;

	server = (*account->proto_opt[USEROPT_MSNSERVER]
			  ? account->proto_opt[USEROPT_MSNSERVER]
			  : MSN_SERVER);
	port = (*account->proto_opt[USEROPT_MSNPORT]
			? atoi(account->proto_opt[USEROPT_MSNPORT])
			: MSN_PORT);


	gc = new_gaim_conn(account);

	session = msn_session_new(account, server, port);
	session->prpl = my_protocol;

	gc->proto_data = session;

	set_login_progress(gc, 1, _("Connecting"));

	g_snprintf(gc->username, sizeof(gc->username), "%s",
			   msn_normalize(gc->username));

	if (!msn_session_connect(session)) {
		hide_login_progress(gc, _("Unable to connect"));
		signoff(gc);
	}
}

static void
msn_close(struct gaim_connection *gc)
{
	MsnSession *session = gc->proto_data;

	msn_session_destroy(session);

	gc->proto_data = NULL;
}

static int
msn_send_im(struct gaim_connection *gc, const char *who, const char *message,
			int len, int flags)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard;

	swboard = msn_session_find_switch_with_passport(session, who);

	if (g_ascii_strcasecmp(who, gc->username)) {
		MsnMessage *msg;
		MsnUser *user;

		user = msn_user_new(session, who, NULL);

		msg = msn_message_new();
		msn_message_set_receiver(msg, user);
		msn_message_set_attr(msg, "X-MMS-IM-Format",
							 "FN=Arial; EF=; CO=0; PF=0");
		msn_message_set_body(msg, message);

		if (swboard != NULL) {
			if (!msn_switchboard_send_msg(swboard, msg))
				msn_switchboard_destroy(swboard);
		}
		else {
			if ((swboard = msn_session_open_switchboard(session)) == NULL) {
				msn_message_destroy(msg);

				hide_login_progress(gc, _("Write error"));
				signoff(gc);

				return 1;
			}

			msn_switchboard_set_user(swboard, user);
			msn_switchboard_send_msg(swboard, msg);
		}

		msn_user_destroy(user);
		msn_message_destroy(msg);
	}
	else {
		/*
		 * In MSN, you can't send messages to yourself, so
		 * we'll fake like we received it ;)
		 */
		serv_got_typing_stopped(gc, (char *)who);
		serv_got_im(gc, who, message, flags | IM_FLAG_GAIMUSER,
					time(NULL), -1);
	}

	return 1;
}

static int
msn_send_typing(struct gaim_connection *gc, char *who, int typing)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	MsnUser *user;

	if (!typing)
		return 0;

	if (!g_ascii_strcasecmp(who, gc->username)) {
		/* We'll just fake it, since we're sending to ourself. */
		serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_switch_with_passport(session, who);

	if (swboard == NULL)
		return 0;

	user = msn_user_new(session, who, NULL);

	msg = msn_message_new();
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_receiver(msg, user);
	msn_message_set_charset(msg, NULL);
	msn_message_set_attr(msg, "TypingUser", gc->username);
	msn_message_set_attr(msg, "User-Agent", NULL);
	msn_message_set_body(msg, "\r\n");

	if (!msn_switchboard_send_msg(swboard, msg))
		msn_switchboard_destroy(swboard);

	msn_user_destroy(user);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	MsnSession *session = gc->proto_data;
	const char *away;

	if (gc->away != NULL) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg != NULL) {
		gc->away = g_strdup("");
		away = "AWY";
	}
	else if (state) {
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
		else {
			g_free(gc->away);
			gc->away = NULL;
			away = "NLN";
		}
	}
	else if (gc->is_idle)
		away = "IDL";
	else
		away = "NLN";

	if (!msn_servconn_send_command(session->notification_conn, "CHG", away)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_set_idle(struct gaim_connection *gc, int idle)
{
	MsnSession *session = gc->proto_data;

	if (gc->away != NULL)
		return;

	if (!msn_servconn_send_command(session->notification_conn, "CHG",
								   (idle ? "IDL" : "NLN"))) {

		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_add_buddy(struct gaim_connection *gc, const char *name)
{
	MsnSession *session = gc->proto_data;
	char *who;
	char outparams[MSN_BUF_LEN];
	GSList *l;

	who = msn_normalize(name);

	if (strchr(who, ' ')) {
		/* This is a broken blist entry. */
		return;
	}

	for (l = session->lists.forward; l != NULL; l = l->next) {
		MsnUser *user = l->data;

		if (!gaim_utf8_strcasecmp(who, msn_user_get_passport(user)))
			break;
	}

	if (l != NULL)
		return;

	g_snprintf(outparams, sizeof(outparams),
			   "FL %s %s", who, who);

	if (!msn_servconn_send_command(session->notification_conn,
								   "ADD", outparams)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_rem_buddy(struct gaim_connection *gc, char *who, char *group)
{
	MsnSession *session = gc->proto_data;
	char outparams[MSN_BUF_LEN];

	g_snprintf(outparams, sizeof(outparams), "FL %s", who);

	if (!msn_servconn_send_command(session->notification_conn,
								   "REM", outparams)) {

		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_add_permit(struct gaim_connection *gc, const char *who)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!strchr(who, '@')) {
		g_snprintf(buf, sizeof(buf), 
			   _("An MSN screenname must be in the form \"user@server.com\". "
			     "Perhaps you meant %s@hotmail.com. No changes were made "
				 "to your allow list."), who);

		do_error_dialog(_("Invalid MSN screenname"), buf, GAIM_ERROR);
		gaim_privacy_permit_remove(gc->account, who);

		return;
	}

	if (g_slist_find_custom(gc->account->deny, who, (GCompareFunc)strcmp)) {
		gaim_debug(GAIM_DEBUG_INFO, "msn", "Moving %s from BL to AL\n", who);
		gaim_privacy_deny_remove(gc->account, who);

		g_snprintf(buf, sizeof(buf), "BL %s", who);

		if (!msn_servconn_send_command(session->notification_conn,
									   "REM", buf)) {

			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}
	}

	g_snprintf(buf, sizeof(buf), "AL %s %s", who, who);

	if (!msn_servconn_send_command(session->notification_conn, "ADD", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
	}
}

static void
msn_add_deny(struct gaim_connection *gc, const char *who)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!strchr(who, '@')) {
		g_snprintf(buf, sizeof(buf), 
			   _("An MSN screenname must be in the form \"user@server.com\". "
			     "Perhaps you meant %s@hotmail.com. No changes were made "
				 "to your block list."), who);

		do_error_dialog(_("Invalid MSN screenname"), buf, GAIM_ERROR);

		gaim_privacy_deny_remove(gc->account, who);

		return;
	}

	if (g_slist_find_custom(gc->account->permit, who, (GCompareFunc)strcmp)) {
		gaim_debug(GAIM_DEBUG_INFO, "msn", "Moving %s from AL to BL\n", who);
		gaim_privacy_permit_remove(gc->account, who);

		g_snprintf(buf, sizeof(buf), "AL %s", who);

		if (!msn_servconn_send_command(session->notification_conn,
									   "REM", buf)) {

			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}
	}

	g_snprintf(buf, sizeof(buf), "BL %s %s", who, who);

	if (!msn_servconn_send_command(session->notification_conn, "ADD", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void
msn_rem_permit(struct gaim_connection *gc, const char *who)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "AL %s", who);

	if (!msn_servconn_send_command(session->notification_conn, "REM", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	gaim_privacy_deny_add(gc->account, who);

	g_snprintf(buf, sizeof(buf), "BL %s %s", who, who);

	if (!msn_servconn_send_command(session->notification_conn, "ADD", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void
msn_rem_deny(struct gaim_connection *gc, const char *who)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "BL %s", who);

	if (!msn_servconn_send_command(session->notification_conn, "REM", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	gaim_privacy_permit_add(gc->account, who);

	g_snprintf(buf, sizeof(buf), "AL %s %s", who, who);

	if (!msn_servconn_send_command(session->notification_conn, "ADD", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void
msn_set_permit_deny(struct gaim_connection *gc)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];
	GSList *s, *t = NULL;

	if (gc->account->permdeny == PERMIT_ALL ||
		gc->account->permdeny == DENY_SOME) {

		strcpy(buf, "AL");
	}
	else
		strcpy(buf, "BL");

	if (!msn_servconn_send_command(session->notification_conn, "BLP", buf)) {
		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}

	/*
	 * This is safe because we'll always come here after we've gotten
	 * the list off the server, and data is never removed. So if the
	 * lengths are equal we don't know about anyone locally and so
	 * there's no sense in going through them all.
	 */
	if (g_slist_length(gc->account->permit) ==
		g_slist_length(session->lists.allow)) {

		g_slist_free(session->lists.allow);
		session->lists.allow = NULL;
	}

	if (g_slist_length(gc->account->deny) ==
		g_slist_length(session->lists.block)) {

		g_slist_free(session->lists.block);
		session->lists.block = NULL;
	}

	if (session->lists.allow == NULL && session->lists.block == NULL)
		return;

	if (session->lists.allow != NULL) {

		for (s = g_slist_nth(gc->account->permit,
							 g_slist_length(session->lists.allow));
			 s != NULL;
			 s = s->next) {

			char *who = s->data;

			if (!strchr(who, '@')) {
				t = g_slist_append(t, who);
				continue;
			}

			if (g_slist_find(session->lists.block, who)) {
				t = g_slist_append(t, who);
				continue;
			}

			g_snprintf(buf, sizeof(buf), "AL %s %s", who, who);

			if (!msn_servconn_send_command(session->notification_conn,
										   "ADD", buf)) {
				hide_login_progress(gc, _("Write error"));
				signoff(gc);
				return;
			}
		}

		for (; t != NULL; t = t->next)
			gaim_privacy_permit_remove(gc->account, t->data);

		if (t != NULL)
			g_slist_free(t);

		t = NULL;
		g_slist_free(session->lists.allow);
		session->lists.allow = NULL;
	}

	if (session->lists.block) {
		for (s = g_slist_nth(gc->account->deny,
							 g_slist_length(session->lists.block));
			 s != NULL;
			 s = s->next) {

			char *who = s->data;

			if (!strchr(who, '@')) {
				t = g_slist_append(t, who);
				continue;
			}

			if (g_slist_find(session->lists.block, who)) {
				t = g_slist_append(t, who);
				continue;
			}

			g_snprintf(buf, sizeof(buf), "BL %s %s", who, who);

			if (!msn_servconn_send_command(session->notification_conn,
										   "ADD", buf)) {
				hide_login_progress(gc, _("Write error"));
				signoff(gc);
				return;
			}
		}

		for (; t != NULL; t = t->next)
			gaim_privacy_deny_remove(gc->account, t->data);

		if (t != NULL)
			g_slist_free(t);

		g_slist_free(session->lists.block);
		session->lists.block = NULL;
	}
}

static void
msn_chat_invite(struct gaim_connection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard = msn_session_find_switch_with_id(session, id);

	if (swboard == NULL)
		return;

	if (!msn_switchboard_send_command(swboard, "CAL", who))
		msn_switchboard_destroy(swboard);
}

static void
msn_chat_leave(struct gaim_connection *gc, int id)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard = msn_session_find_switch_with_id(session, id);
	char buf[6];

	if (swboard == NULL)
		return;

	strcpy(buf, "OUT\r\n");

	if (!msn_servconn_write(swboard->servconn, buf, strlen(buf)))
		msn_switchboard_destroy(swboard);
}

static int
msn_chat_send(struct gaim_connection *gc, int id, char *message)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard = msn_session_find_switch_with_id(session, id);
	MsnMessage *msg;
	char *send;

	if (swboard == NULL)
		return -EINVAL;

	send = add_cr(message);

	msg = msn_message_new();
	msn_message_set_attr(msg, "X-MMS-IM-Format", "FN=Arial; EF=; CO=0; PF=0");
	msn_message_set_body(msg, send);

	g_free(send);

	if (!msn_switchboard_send_msg(swboard, msg)) {
		msn_switchboard_destroy(swboard);

		msn_message_destroy(msg);

		return 0;
	}

	msn_message_destroy(msg);

	serv_got_chat_in(gc, id, gc->username, 0, message, time(NULL));

	return 0;
}

static void
msn_keepalive(struct gaim_connection *gc)
{
	MsnSession *session = gc->proto_data;
	char buf[MSN_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "PNG\r\n");

	if (msn_servconn_write(session->notification_conn,
						   buf, strlen(buf)) < 0) {

		hide_login_progress(gc, _("Write error"));
		signoff(gc);
		return;
	}
}

static void
msn_group_buddy(struct gaim_connection *gc, const char *who,
				const char *old_group, const char *new_group)
{
	MsnSession *session = gc->proto_data;
	char outparams[MSN_BUF_LEN];
	gint *old_group_id, *new_group_id;

	old_group_id = g_hash_table_lookup(session->group_ids, old_group);
	new_group_id = g_hash_table_lookup(session->group_ids, new_group);

	if (new_group_id == NULL) {
		g_snprintf(outparams, sizeof(outparams), "%s 0",
				   msn_url_encode(new_group));

		if (!msn_servconn_send_command(session->notification_conn,
									   "ADG", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}

		/* I hate this. So much. */
		session->moving_buddy = TRUE;
		session->dest_group_name = g_strdup(new_group);
	}
	else {
		g_snprintf(outparams, sizeof(outparams), "FL %s %s %d",
				   who, who, *new_group_id);

		if (!msn_servconn_send_command(session->notification_conn,
									   "ADD", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}
	}

	if (old_group_id != NULL) {
		g_snprintf(outparams, sizeof(outparams), "FL %s %d",
				   who, *old_group_id);

		if (!msn_servconn_send_command(session->notification_conn,
									   "REM", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
			return;
		}
	}
}

static void
msn_rename_group(struct gaim_connection *gc, const char *old_group,
				 const char *new_group, GList *members)
{
	MsnSession *session = gc->proto_data;
	char outparams[MSN_BUF_LEN];
	int *group_id;

	if (g_hash_table_lookup_extended(session->group_ids, old_group,
									 NULL, (gpointer)&group_id)) {
		g_snprintf(outparams, sizeof(outparams), "%d %s 0",
				   *group_id, msn_url_encode(new_group));

		if (!msn_servconn_send_command(session->notification_conn,
									   "REG", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
		}
	}
	else {
		g_snprintf(outparams, sizeof(outparams), "%s 0",
				   msn_url_encode(new_group));

		if (!msn_servconn_send_command(session->notification_conn,
									   "ADG", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
		}
	}
}

static void
msn_buddy_free(struct buddy *b)
{
	if (b->proto_data != NULL)
		g_free(b->proto_data);
}

static void
msn_convo_closed(struct gaim_connection *gc, char *who)
{
	MsnSession *session = gc->proto_data;
	MsnSwitchBoard *swboard;
	
	swboard = msn_session_find_switch_with_passport(session, who);

	if (swboard != NULL) {
		char sendbuf[256];

		g_snprintf(sendbuf, sizeof(sendbuf), "BYE %s\r\n", gc->username);

		msn_servconn_write(swboard->servconn, sendbuf, strlen(sendbuf));

		msn_switchboard_destroy(swboard);
	}
}

static char *
msn_normalize(const char *str)
{
	static char buf[BUF_LEN];

	g_return_val_if_fail(str != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", str,
			   (strchr(str, '@') ? "" : "@hotmail.com"));

	return buf;
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_MSN,
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_BUDDY_ICON,
	NULL,
	NULL,
	msn_list_icon,
	msn_list_emblems,
	msn_status_text,
	msn_tooltip_text,
	msn_away_states,
	msn_actions,
	msn_buddy_menu,
	NULL,
	msn_login,
	msn_close,
	msn_send_im,
	NULL,
	msn_send_typing,
	NULL,
	msn_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
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
	msn_normalize
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
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
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Login Server:"));
	puo->def = g_strdup(MSN_SERVER);
	puo->pos = USEROPT_MSNSERVER;
	prpl_info.user_opts = g_list_append(prpl_info.user_opts, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Port:"));
	puo->def = g_strdup("1863");
	puo->pos = USEROPT_MSNPORT;
	prpl_info.user_opts = g_list_append(prpl_info.user_opts, puo);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(msn, __init_plugin, info);
