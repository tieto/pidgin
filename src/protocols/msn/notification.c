/**
 * @file notification.c Notification server functions
 *
 * gaim
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
#include "msn.h"
#include "notification.h"
#include "state.h"
#include "error.h"
#include "utils.h"
#include "page.h"

#include "userlist.h"
#include "sync.h"

#define BUDDY_ALIAS_MAXLEN 388

static MsnTable *cbs_table;

/**************************************************************************
 * Login
 **************************************************************************/

void
msn_got_login_params(MsnSession *session, const char *login_params)
{
	MsnCmdProc *cmdproc;

	cmdproc = session->notification->cmdproc;

	msn_cmdproc_send(cmdproc, "USR", "TWN S %s", login_params);
}

static void
cvr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;

	account = cmdproc->session->account;

	msn_cmdproc_send(cmdproc, "USR", "TWN I %s",
					 gaim_account_get_username(account));
}

static void
inf_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	GaimConnection *gc;

	account = cmdproc->session->account;
	gc = gaim_account_get_connection(account);

	if (strcmp(cmd->params[1], "MD5"))
	{
		msn_cmdproc_show_error(cmdproc, MSN_ERROR_MISC);
		return;
	}

	msn_cmdproc_send(cmdproc, "USR", "MD5 I %s",
					 gaim_account_get_username(account));

	if (cmdproc->error)
		return;

	gaim_connection_update_progress(gc, _("Requesting to send password"),
									5, MSN_CONNECT_STEPS);
}

static void
usr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	/*
	 * We're either getting the passport connect info (if we're on
	 * MSNP8 or higher), or a challenge request (MSNP7 and lower).
	 *
	 * Let's find out.
	 */
	if (!g_ascii_strcasecmp(cmd->params[1], "OK"))
	{
		const char *friendly = gaim_url_decode(cmd->params[3]);

		/* OK */

		gaim_connection_set_display_name(gc, friendly);

		msn_cmdproc_send(cmdproc, "SYN", "%s", "0");

		if (cmdproc->error)
			return;

		gaim_connection_update_progress(gc, _("Retrieving buddy list"),
										7, MSN_CONNECT_STEPS);
	}
	else if (!g_ascii_strcasecmp(cmd->params[1], "TWN"))
	{
		char **elems, **cur, **tokens;

		/* Passport authentication */
		session->nexus = msn_nexus_new(session);

		/* Parse the challenge data. */

		elems = g_strsplit(cmd->params[3], ",", 0);

		for (cur = elems; *cur != NULL; cur++)
		{
				tokens = g_strsplit(*cur, "=", 2);
				g_hash_table_insert(session->nexus->challenge_data, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
		}

		g_strfreev(elems);

		msn_nexus_connect(session->nexus);

		gaim_connection_update_progress(gc, _("Password sent"),
										6, MSN_CONNECT_STEPS);
	}
	else if (!g_ascii_strcasecmp(cmd->params[1], "MD5"))
	{
		/* Challenge */
		const char *challenge;
		const char *password;
		char buf[33];
		md5_state_t st;
		md5_byte_t di[16];
		int i;

		challenge = cmd->params[3];
		password = gaim_account_get_password(account);

		md5_init(&st);
		md5_append(&st, (const md5_byte_t *)challenge, strlen(challenge));
		md5_append(&st, (const md5_byte_t *)password, strlen(password));
		md5_finish(&st, di);

		for (i = 0; i < 16; i++)
			g_snprintf(buf + (i*2), 3, "%02x", di[i]);

		msn_cmdproc_send(cmdproc, "USR", "MD5 S %s", buf);

		if (cmdproc->error)
			return;

		gaim_connection_update_progress(gc, _("Password sent"),
										6, MSN_CONNECT_STEPS);
	}
}

static void
ver_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	gboolean protocol_supported = FALSE;
	char proto_str[8];
	size_t i;

	session = cmdproc->session;
	account = session->account;

	g_snprintf(proto_str, sizeof(proto_str), "MSNP%d", session->protocol_ver);

	for (i = 1; i < cmd->param_count; i++)
	{
		if (!strcmp(cmd->params[i], proto_str))
		{
			protocol_supported = TRUE;
			break;
		}
	}

	if (!protocol_supported)
	{
		msn_cmdproc_show_error(cmdproc, MSN_ERROR_MISC);
		return;
	}

	msn_cmdproc_send(cmdproc, "CVR",
					 "0x0409 winnt 5.1 i386 MSNMSGR 6.0.0602 MSMSGS %s",
					 gaim_account_get_username(account));
}

/**************************************************************************
 * Log out
 **************************************************************************/
static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	if (!g_ascii_strcasecmp(cmd->params[0], "OTH"))
		msn_cmdproc_show_error(cmdproc, MSN_ERROR_SIGNOTHER);
	else if (!g_ascii_strcasecmp(cmd->params[0], "SSD"))
		msn_cmdproc_show_error(cmdproc, MSN_ERROR_SERVDOWN);
}

/**************************************************************************
 * Messages
 **************************************************************************/
static void
msg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	MsnMessage *msg;

	msg = msn_message_new_from_cmd(cmdproc->session, cmd);

	msn_message_parse_payload(msg, payload, len);
	/* msn_message_show_readable(msg, "Notification", TRUE); */

	msn_cmdproc_process_msg(cmdproc, msg);

	msn_message_destroy(msg);
}

static void
msg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	/* NOTE: cmd is not always cmdproc->last_cmd, sometimes cmd is a queued
	 * command and we are processing it */

	if (cmd->payload == NULL)
	{
		cmdproc->last_cmd->payload_cb  = msg_cmd_post;
		cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	}
	else
	{
		g_return_if_fail(cmd->payload_cb != NULL);

		cmd->payload_cb(cmdproc, cmd, cmd->payload, cmd->payload_len);
	}
}

/**************************************************************************
 * Challenges
 **************************************************************************/
static void
chl_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnTransaction *trans;
	char buf[33];
	const char *challenge_resp;
	md5_state_t st;
	md5_byte_t di[16];
	int i;

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)cmd->params[1],
			   strlen(cmd->params[1]));

	challenge_resp = "VT6PX?UQTM4WM%YR";

	md5_append(&st, (const md5_byte_t *)challenge_resp,
			   strlen(challenge_resp));
	md5_finish(&st, di);

	for (i = 0; i < 16; i++)
		g_snprintf(buf + (i*2), 3, "%02x", di[i]);

	trans = msn_transaction_new(cmdproc, "QRY", "%s 32", "PROD0038W!61ZTF9");

	msn_transaction_set_payload(trans, buf, 32);

	msn_cmdproc_send_trans(cmdproc, trans);
}

/**************************************************************************
 * Buddy Lists
 **************************************************************************/
static void
add_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnUser *user;
	const char *list;
	const char *passport;
	const char *friendly;
	MsnListId list_id;
	int group_id;

	list     = cmd->params[1];
	passport = cmd->params[3];
	friendly = gaim_url_decode(cmd->params[4]);

	session = cmdproc->session;

	user = msn_userlist_find_user(session->userlist, passport);

	if (user == NULL)
	{
		user = msn_user_new(session->userlist, passport, friendly);
		msn_userlist_add_user(session->userlist, user);
	}
	else
		msn_user_set_friendly_name(user, friendly);

	list_id = msn_get_list_id(list);

	if (cmd->param_count >= 6)
		group_id = atoi(cmd->params[5]);
	else
		group_id = -1;

	msn_got_add_user(session, user, list_id, group_id);
}

static void
add_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	const char *list, *passport;
	char *reason = NULL;
	char *msg = NULL;
	char **params;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);
	params = g_strsplit(trans->params, " ", 0);

	list     = params[0];
	passport = params[1];

	if (!strcmp(list, "FL"))
		msg = g_strdup_printf("Unable to add user on %s (%s)",
							  gaim_account_get_username(account),
							  gaim_account_get_protocol_name(account));
	else if (!strcmp(list, "BL"))
		msg = g_strdup_printf("Unable to block user on %s (%s)",
							  gaim_account_get_username(account),
							  gaim_account_get_protocol_name(account));
	else if (!strcmp(list, "AL"))
		msg = g_strdup_printf("Unable to permit user on %s (%s)",
							  gaim_account_get_username(account),
							  gaim_account_get_protocol_name(account));

	if (!strcmp(list, "FL"))
	{
		if (error == 210)
		{
			reason = g_strdup_printf("%s could not be added because "
									 "your buddy list is full.", passport);
		}
	}

	if (reason == NULL)
	{
		if (error == 208)
		{
			reason = g_strdup_printf("%s is not a valid passport account.",
									 passport);
		}
		else
		{
			reason = g_strdup_printf("Unknown error.");
		}
	}

	if (msg != NULL)
	{
		gaim_notify_error(gc, NULL, msg, reason);
		g_free(msg);
	}

	if (!strcmp(list, "FL"))
	{
		GaimBuddy *buddy;

		buddy = gaim_find_buddy(account, passport);

		if (buddy != NULL)
			gaim_blist_remove_buddy(buddy);
	}

	g_free(reason);

	g_strfreev(params);
}

static void
adg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnGroup *group;
	MsnSession *session;
	gint group_id;
	const char *group_name;

	session = cmdproc->session;

	group_id = atoi(cmd->params[3]);

	group_name = gaim_url_decode(cmd->params[2]);

	group = msn_group_new(session->userlist, group_id, group_name);

	/* There is a user that must me moved to this group */
	if (cmd->trans->data)
	{
		/* msn_userlist_move_buddy(); */
		MsnUserList *userlist = cmdproc->session->userlist;
		MsnMoveBuddy *data = cmd->trans->data;

		if (data->old_group_name != NULL)
		{
			msn_userlist_rem_buddy(userlist, data->who, MSN_LIST_FL, data->old_group_name);
			g_free(data->old_group_name);
		}

		msn_userlist_add_buddy(userlist, data->who, MSN_LIST_FL, group_name);
		g_free(data->who);

	}
}

static void
fln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;

	account = cmdproc->session->account;

	gaim_prpl_got_user_status(account, cmd->params[0], "offline", NULL);
}

/*
 * XXX - There is a bit of code duplication between this function
 * and nln_cmd.  Someone should do something about that.
 */
static void
iln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	const char *status, *state, *passport, *friendly;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	state    = cmd->params[1];
	passport = cmd->params[2];
	friendly = gaim_url_decode(cmd->params[3]);

	user = msn_userlist_find_user(session->userlist, passport);

	/* serv_got_nick(gc, passport, friendly); */
	serv_got_alias(gc, passport, friendly);

	msn_user_set_friendly_name(user, friendly);

	if (session->protocol_ver >= 9 && cmd->param_count == 6)
	{
		msnobj = msn_object_new_from_string(gaim_url_decode(cmd->params[5]));
		msn_user_set_object(user, msnobj);
	}

	/* XXX - What does this do?????
	if ((b = gaim_find_buddy(account, passport)) != NULL)
		status |= ((((b->uc) >> 1) & 0xF0) << 1);
	*/

	if (!g_ascii_strcasecmp(state, "BSY"))
		status = "busy";
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status = "brb";
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status = "away";
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status = "phone";
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status = "lunch";
	else
		status = "available";

	gaim_prpl_got_user_status(account, passport, status, NULL);

	if (!g_ascii_strcasecmp(state, "IDL"))
		gaim_prpl_got_user_idle(account, passport, TRUE, -1);
	else
		gaim_prpl_got_user_idle(account, passport, FALSE, 0);
}

static void
ipg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
#if 0
	gaim_debug_misc("msn", "Incoming Page: {%s}\n", payload);
#endif
}

static void
ipg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->servconn->payload_len = atoi(cmd->params[0]);
	cmdproc->last_cmd->payload_cb = ipg_cmd_post;
}

static void
nln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	const char *state;
	const char *passport;
	const char *friendly;
	const char *status;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	state    = cmd->params[0];
	passport = cmd->params[1];
	friendly = gaim_url_decode(cmd->params[2]);

	user = msn_userlist_find_user(session->userlist, passport);

	/* serv_got_nick(gc, passport, friendly); */
	serv_got_alias(gc, passport, friendly);

	msn_user_set_friendly_name(user, friendly);

	if (session->protocol_ver >= 9)
	{
		if (cmd->param_count == 5)
		{
			msnobj =
				msn_object_new_from_string(gaim_url_decode(cmd->params[4]));
			msn_user_set_object(user, msnobj);
		}
		else
		{
			msn_user_set_object(user, NULL);
		}
	}

	if (!g_ascii_strcasecmp(state, "BSY"))
		status = "busy";
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status = "brb";
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status = "away";
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status = "phone";
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status = "lunch";
	else
		status = "available";

	gaim_prpl_got_user_status(account, passport, status, NULL);

	if (!g_ascii_strcasecmp(state, "IDL"))
		gaim_prpl_got_user_idle(account, passport, TRUE, -1);
	else
		gaim_prpl_got_user_idle(account, passport, FALSE, 0);
}

static void
chg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	char *state = cmd->params[1];
	int state_id = 0;

	if (!strcmp(state, "NLN"))
		state_id = MSN_ONLINE;
	else if (!strcmp(state, "BSY"))
		state_id = MSN_BUSY;
	else if (!strcmp(state, "IDL"))
		state_id = MSN_IDLE;
	else if (!strcmp(state, "BRB"))
		state_id = MSN_BRB;
	else if (!strcmp(state, "AWY"))
		state_id = MSN_AWAY;
	else if (!strcmp(state, "PHN"))
		state_id = MSN_PHONE;
	else if (!strcmp(state, "LUN"))
		state_id = MSN_LUNCH;
	else if (!strcmp(state, "HDN"))
		state_id = MSN_HIDDEN;

	cmdproc->session->state = state_id;
}


static void
not_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
#if 0
	MSN_SET_PARAMS("NOT %d\r\n%s", cmdproc->servconn->payload, payload);
	gaim_debug_misc("msn", "Notification: {%s}\n", payload);
#endif
}

static void
not_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->servconn->payload_len = atoi(cmd->params[0]);
	cmdproc->last_cmd->payload_cb = not_cmd_post;
}

static void
rea_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	/* TODO: This might be with us too */

	MsnSession *session;
	GaimConnection *gc;
	const char *friendly;

	session = cmdproc->session;
	gc = session->account->gc;
	friendly = gaim_url_decode(cmd->params[3]);

	gaim_connection_set_display_name(gc, friendly);
}

static void
reg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	int group_id;
	const char *group_name;

	session = cmdproc->session;
	group_id = atoi(cmd->params[2]);
	group_name = gaim_url_decode(cmd->params[3]);

	msn_userlist_rename_group_id(session->userlist, group_id, group_name);
}

static void
rem_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnUser *user;
	const char *list;
	const char *passport;
	MsnListId list_id;
	int group_id;

	session = cmdproc->session;
	list = cmd->params[1];
	passport = cmd->params[3];
	user = msn_userlist_find_user(session->userlist, passport);

	g_return_if_fail(user != NULL);

	list_id = msn_get_list_id(list);

	if (cmd->param_count == 5)
		group_id = atoi(cmd->params[4]);
	else
		group_id = -1;

	msn_got_rem_user(session, user, list_id, group_id);
}

static void
rmg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	int group_id;

	session = cmdproc->session;
	group_id = atoi(cmd->params[2]);

	msn_userlist_remove_group_id(session->userlist, group_id);
}

static void
syn_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	int total_users;

	session = cmdproc->session;

	if (cmd->param_count == 2)
	{
		char *buf;
		/*
		 * This can happen if we sent a SYN with an up-to-date
		 * buddy list revision, but we send 0 to get a full list.
		 * So, error out.
		 */
		buf = g_strdup_printf(
				_("Your MSN buddy list for %s is temporarily unavailable. "
				  "Please wait and try again."),
				gaim_account_get_username(session->account));

		gaim_connection_error(gaim_account_get_connection(session->account),
							  buf);

		g_free(buf);

		return;
	}

	total_users  = atoi(cmd->params[2]);

	if (total_users == 0)
	{
		msn_session_finish_login(session);
	}
	else
	{
		/* syn_table */
		MsnSync *sync;

		sync = msn_sync_new(session);
		sync->total_users = total_users;
		sync->old_cbs_table = cmdproc->cbs_table;

		session->sync = sync;
		cmdproc->cbs_table = sync->cbs_table;
	}
}

/**************************************************************************
 * Misc commands
 **************************************************************************/
static void
url_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	const char *rru;
	const char *url;
	md5_state_t st;
	md5_byte_t di[16];
	FILE *fd;
	char buf[2048];
	char buf2[3];
	char sendbuf[64];
	int i;

	session = cmdproc->session;
	account = session->account;

	rru = cmd->params[1];
	url = cmd->params[2];

	g_snprintf(buf, sizeof(buf), "%s%lu%s",
			   session->passport_info.mspauth,
			   time(NULL) - session->passport_info.sl,
			   gaim_account_get_password(account));

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)buf, strlen(buf));
	md5_finish(&st, di);

	memset(sendbuf, 0, sizeof(sendbuf));

	for (i = 0; i < 16; i++)
	{
		g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
		strcat(sendbuf, buf2);
	}

	if (session->passport_info.file != NULL)
	{
		unlink(session->passport_info.file);
		g_free(session->passport_info.file);
	}

	if ((fd = gaim_mkstemp(&session->passport_info.file, FALSE)) == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Error opening temp passport file: %s\n",
				   strerror(errno));
	}
	else
	{
		fputs("<html>\n"
			  "<head>\n"
			  "<noscript>\n"
			  "<meta http-equiv=\"Refresh\" content=\"0; "
			  "url=http://www.hotmail.com\">\n"
			  "</noscript>\n"
			  "</head>\n\n",
			  fd);

		fprintf(fd, "<body onload=\"document.pform.submit(); \">\n");
		fprintf(fd, "<form name=\"pform\" action=\"%s\" method=\"POST\">\n\n",
				url);
		fprintf(fd, "<input type=\"hidden\" name=\"mode\" value=\"ttl\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"login\" value=\"%s\">\n",
				gaim_account_get_username(account));
		fprintf(fd, "<input type=\"hidden\" name=\"username\" value=\"%s\">\n",
				gaim_account_get_username(account));
		fprintf(fd, "<input type=\"hidden\" name=\"sid\" value=\"%s\">\n",
				session->passport_info.sid);
		fprintf(fd, "<input type=\"hidden\" name=\"kv\" value=\"%s\">\n",
				session->passport_info.kv);
		fprintf(fd, "<input type=\"hidden\" name=\"id\" value=\"2\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"sl\" value=\"%ld\">\n",
				time(NULL) - session->passport_info.sl);
		fprintf(fd, "<input type=\"hidden\" name=\"rru\" value=\"%s\">\n",
				rru);
		fprintf(fd, "<input type=\"hidden\" name=\"auth\" value=\"%s\">\n",
				session->passport_info.mspauth);
		fprintf(fd, "<input type=\"hidden\" name=\"creds\" value=\"%s\">\n",
				sendbuf); /* TODO Digest me (huh? -- ChipX86) */
		fprintf(fd, "<input type=\"hidden\" name=\"svc\" value=\"mail\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"js\" value=\"yes\">\n");
		fprintf(fd, "</form></body>\n");
		fprintf(fd, "</html>\n");

		if (fclose(fd))
		{
			gaim_debug_error("msn",
							 "Error closing temp passport file: %s\n",
							 strerror(errno));

			unlink(session->passport_info.file);
			g_free(session->passport_info.file);
			session->passport_info.file = NULL;
		}
	}
}
/**************************************************************************
 * Switchboards
 **************************************************************************/
static void
rng_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	const char *session_id;
	char *host;
	int port;

	session = cmdproc->session;
	session_id = cmd->params[0];

	msn_parse_socket(cmd->params[1], &host, &port);

	if (session->http_method)
		port = 80;

	swboard = msn_switchboard_new(session);

	msn_switchboard_set_invited(swboard, TRUE);
	msn_switchboard_set_session_id(swboard, cmd->params[0]);
	msn_switchboard_set_auth_key(swboard, cmd->params[3]);
	swboard->im_user = g_strdup(cmd->params[4]);
	/* msn_switchboard_add_user(swboard, cmd->params[4]); */

	msn_switchboard_connect(swboard, host, port);

	g_free(host);
}

static void
xfr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	char *host;
	int port;

	if (strcmp(cmd->params[1], "SB") && strcmp(cmd->params[1], "NS"))
	{
		msn_cmdproc_show_error(cmdproc, MSN_ERROR_MISC);
		return;
	}

	msn_parse_socket(cmd->params[2], &host, &port);

	if (!strcmp(cmd->params[1], "SB"))
	{
		gaim_debug_error("msn", "This shouldn't be handled here.\n");
#if 0
		swboard = cmd->trans->data;

		if (swboard != NULL)
		{
			msn_switchboard_set_auth_key(swboard, cmd->params[4]);

			if (session->http_method)
				port = 80;

			msn_switchboard_connect(swboard, host, port);
		}
#endif
	}
	else if (!strcmp(cmd->params[1], "NS"))
	{
		MsnSession *session;

		session = cmdproc->session;

		msn_notification_connect(session->notification, host, port);
	}

	g_free(host);
}

/**************************************************************************
 * Message Types
 **************************************************************************/
static void
profile_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	const char *value;

	session = cmdproc->session;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if ((value = msn_message_get_attr(msg, "kv")) != NULL)
	{
		if (session->passport_info.kv != NULL)
			g_free(session->passport_info.kv);

		session->passport_info.kv = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "sid")) != NULL)
	{
		if (session->passport_info.sid != NULL)
			g_free(session->passport_info.sid);

		session->passport_info.sid = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "MSPAuth")) != NULL)
	{
		if (session->passport_info.mspauth != NULL)
			g_free(session->passport_info.mspauth);

		session->passport_info.mspauth = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "ClientIP")) != NULL)
	{
		if (session->passport_info.client_ip != NULL)
			g_free(session->passport_info.client_ip);

		session->passport_info.client_ip = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "ClientPort")) != NULL)
		session->passport_info.client_port = ntohs(atoi(value));

	if ((value = msn_message_get_attr(msg, "LoginTime")) != NULL)
		session->passport_info.sl = atol(value);
}

static void
initial_email_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	GaimConnection *gc;
	GHashTable *table;
	const char *unread;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if (!gaim_account_get_check_mail(session->account))
		return;

	if (session->passport_info.file == NULL)
	{
		MsnTransaction *trans;
		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_queue_cmd(trans, msg->cmd);

		msn_cmdproc_send_trans(cmdproc, trans);

		return;
	}

	table = msn_message_get_hashtable_from_body(msg);

	unread = g_hash_table_lookup(table, "Inbox-Unread");

	if (unread != NULL)
	{
		int count = atoi(unread);

		if (count > 0)
		{
			const char *passport;
			const char *url;

			passport = msn_user_get_passport(session->user);
			url = session->passport_info.file;

			gaim_notify_emails(gc, atoi(unread), FALSE, NULL, NULL,
							   &passport, &url, NULL, NULL);
		}
	}

	g_hash_table_destroy(table);
}

static void
email_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	GaimConnection *gc;
	GHashTable *table;
	char *from, *subject, *tmp;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if (!gaim_account_get_check_mail(session->account))
		return;

	if (session->passport_info.file == NULL)
	{
		MsnTransaction *trans;
		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_queue_cmd(trans, msg->cmd);

		msn_cmdproc_send_trans(cmdproc, trans);

		return;
	}

	table = msn_message_get_hashtable_from_body(msg);

	from = subject = NULL;

	tmp = g_hash_table_lookup(table, "From");
	if (tmp != NULL)
		from = gaim_mime_decode_field(tmp);

	tmp = g_hash_table_lookup(table, "Subject");
	if (tmp != NULL)
		subject = gaim_mime_decode_field(tmp);

	gaim_notify_email(gc,
					  (subject != NULL ? subject : ""),
					  (from != NULL ?  from : ""),
					  msn_user_get_passport(session->user),
					  session->passport_info.file, NULL, NULL);

	if (from != NULL)
		g_free(from);

	if (subject != NULL)
		g_free(subject);

	g_hash_table_destroy(table);
}

static void
system_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GHashTable *table;
	const char *type_s;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	table = msn_message_get_hashtable_from_body(msg);

	if ((type_s = g_hash_table_lookup(table, "Type")) != NULL)
	{
		int type = atoi(type_s);
		char buf[MSN_BUF_LEN];
		int minutes;

		switch (type)
		{
			case 1:
				minutes = atoi(g_hash_table_lookup(table, "Arg1"));
				g_snprintf(buf, sizeof(buf), ngettext(
							"The MSN server will shut down for maintenance "
							"in %d minute. You will automatically be "
							"signed out at that time.  Please finish any "
							"conversations in progress.\n\nAfter the "
							"maintenance has been completed, you will be "
							"able to successfully sign in.",
							"The MSN server will shut down for maintenance "
							"in %d minutes. You will automatically be "
							"signed out at that time.  Please finish any "
							"conversations in progress.\n\nAfter the "
							"maintenance has been completed, you will be "
							"able to successfully sign in.", minutes),
						minutes);
			default:
				break;
		}

		if (*buf != '\0')
			gaim_notify_info(cmdproc->session->account->gc, NULL, buf, NULL);
	}

	g_hash_table_destroy(table);
}

static void
connect_cb(MsnServConn *servconn)
{
	MsnNotification *notification;
	MsnCmdProc *cmdproc;
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	char **a, **c, *vers;
	int i;

	g_return_if_fail(servconn != NULL);

	notification = servconn->data;
	cmdproc = servconn->cmdproc;
	session = servconn->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	/* Allocate an array for CVR0, NULL, and all the versions */
	a = c = g_new0(char *, session->protocol_ver - 8 + 3);

	for (i = session->protocol_ver; i >= 8; i--)
		*c++ = g_strdup_printf("MSNP%d", i);

	*c++ = g_strdup("CVR0");

	vers = g_strjoinv(" ", a);

	msn_cmdproc_send(cmdproc, "VER", "%s", vers);

	g_strfreev(a);
	g_free(vers);

	if (cmdproc->error)
		return;

	if (session->user == NULL)
		session->user = msn_user_new(session->userlist,
									 gaim_account_get_username(account), NULL);

#if 0
	gaim_connection_update_progress(gc, _("Syncing with server"),
									4, MSN_CONNECT_STEPS);
#endif
}

void
msn_notification_add_buddy(MsnNotification *notification, const char *list,
						   const char *who, const char *store_name,
						   int group_id)
{
	MsnCmdProc *cmdproc;
	cmdproc = notification->servconn->cmdproc;

	if (group_id < 0 && !strcmp(list, "FL"))
		group_id = 0;

	if (group_id >= 0)
	{
		msn_cmdproc_send(cmdproc, "ADD", "%s %s %s %d",
						 list, who, store_name, group_id);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "ADD", "%s %s %s", list, who, store_name);
	}
}

void
msn_notification_rem_buddy(MsnNotification *notification, const char *list,
						   const char *who, int group_id)
{
	MsnCmdProc *cmdproc;
	cmdproc = notification->servconn->cmdproc;

	if (group_id >= 0)
	{
		msn_cmdproc_send(cmdproc, "REM", "%s %s %d", list, who, group_id);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "REM", "%s %s", list, who);
	}
}

void
msn_notification_init(void)
{
	/* TODO: check prp, blp */

	cbs_table = msn_table_new();

	/* Syncronous */
	msn_table_add_cmd(cbs_table, "CHG", "CHG", chg_cmd);
	msn_table_add_cmd(cbs_table, "CHG", "ILN", iln_cmd);
	msn_table_add_cmd(cbs_table, "ADD", "ADD", add_cmd);
	msn_table_add_cmd(cbs_table, "ADD", "ILN", iln_cmd);
	msn_table_add_cmd(cbs_table, "REM", "REM", rem_cmd);
	msn_table_add_cmd(cbs_table, "USR", "USR", usr_cmd);
	msn_table_add_cmd(cbs_table, "USR", "XFR", xfr_cmd);
	msn_table_add_cmd(cbs_table, "SYN", "SYN", syn_cmd);
	msn_table_add_cmd(cbs_table, "CVR", "CVR", cvr_cmd);
	msn_table_add_cmd(cbs_table, "INF", "INF", inf_cmd);
	msn_table_add_cmd(cbs_table, "VER", "VER", ver_cmd);
	msn_table_add_cmd(cbs_table, "REA", "REA", rea_cmd);
	/* msn_table_add_cmd(cbs_table, "PRP", "PRP", prp_cmd); */
	/* msn_table_add_cmd(cbs_table, "BLP", "BLP", blp_cmd); */
	msn_table_add_cmd(cbs_table, "BLP", "BLP", NULL);
	msn_table_add_cmd(cbs_table, "REG", "REG", reg_cmd);
	msn_table_add_cmd(cbs_table, "ADG", "ADG", adg_cmd);
	msn_table_add_cmd(cbs_table, "RMG", "RMG", rmg_cmd);
	msn_table_add_cmd(cbs_table, "XFR", "XFR", xfr_cmd);

	/* Asyncronous */
	msn_table_add_cmd(cbs_table, NULL, "IPG", ipg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "MSG", msg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "NOT", not_cmd);

	msn_table_add_cmd(cbs_table, NULL, "CHL", chl_cmd);
	msn_table_add_cmd(cbs_table, NULL, "REM", rem_cmd);
	msn_table_add_cmd(cbs_table, NULL, "ADD", add_cmd);

	msn_table_add_cmd(cbs_table, NULL, "QRY", NULL);
	msn_table_add_cmd(cbs_table, NULL, "QNG", NULL);
	msn_table_add_cmd(cbs_table, NULL, "FLN", fln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "NLN", nln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "ILN", iln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "OUT", out_cmd);
	msn_table_add_cmd(cbs_table, NULL, "RNG", rng_cmd);

	msn_table_add_cmd(cbs_table, NULL, "URL", url_cmd);

	msn_table_add_cmd(cbs_table, "fallback", "XFR", xfr_cmd);

	msn_table_add_error(cbs_table, "ADD", add_error);
	/* msn_table_add_error(cbs_table, "REA", rea_error); */

	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsprofile",
						   profile_msg);
	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsinitialemailnotification",
						   initial_email_msg);
	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsemailnotification",
						   email_msg);
	msn_table_add_msg_type(cbs_table,
						   "application/x-msmsgssystemmessage",
						   system_msg);
}

void
msn_notification_end(void)
{
	msn_table_destroy(cbs_table);
}

MsnNotification *
msn_notification_new(MsnSession *session)
{
	MsnNotification *notification;
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	notification = g_new0(MsnNotification, 1);

	notification->session = session;
	notification->servconn = servconn = msn_servconn_new(session, MSN_SERVER_NS);
	notification->cmdproc = servconn->cmdproc;
	msn_servconn_set_connect_cb(servconn, connect_cb);

	if (session->http_method)
		servconn->http_data->server_type = "NS";

	servconn->cmdproc->cbs_table = cbs_table;

	return notification;
}

void
msn_notification_destroy(MsnNotification *notification)
{
	msn_servconn_destroy(notification->servconn);

	g_free(notification);
}

gboolean
msn_notification_connect(MsnNotification *notification, const char *host, int port)
{
	MsnServConn *servconn;

	g_return_val_if_fail(notification != NULL, FALSE);

	servconn = notification->servconn;

	return (notification->in_use = msn_servconn_connect(servconn, host, port));
}

void
msn_notification_disconnect(MsnNotification *notification)
{
	g_return_if_fail(notification != NULL);

	notification->in_use = FALSE;

	if (notification->servconn->connected)
		msn_servconn_disconnect(notification->servconn);
}
