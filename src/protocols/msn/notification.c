/**
 * @file notification.c Notification server functions
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
#include "msn.h"
#include "notification.h"
#include "state.h"
#include "error.h"
#include "utils.h"

typedef struct
{
	GaimConnection *gc;
	MsnUser *user;

} MsnPermitAdd;

static MsnTable *cbs_table = NULL;

/**************************************************************************
 * Utility functions
 **************************************************************************/
static gboolean
add_buddy(MsnCmdProc *cmdproc, MsnUser *user)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	GaimBuddy *b;
	MsnGroup *group = NULL;
	GaimGroup *g = NULL;
	GList *l, *l2;
	GSList *sl;
	GSList *buddies;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);
	buddies = gaim_find_buddies(account, msn_user_get_passport(user));

	for (l = msn_user_get_group_ids(user); l != NULL; l = l->next)
	{
		int group_id = GPOINTER_TO_INT(l->data);

		if (group_id > -1)
			group = msn_groups_find_with_id(session->groups, group_id);

		if (group == NULL)
		{
			gaim_debug(GAIM_DEBUG_WARNING, "msn",
					   "Group ID %d for user %s was not defined.\n",
					   group_id, msn_user_get_passport(user));

			/* Find a group that we can stick this guy into. Lamer. */
			l2 = msn_groups_get_list(session->groups);

			if (l2 != NULL)
			{
				group = l2->data;

				msn_user_add_group_id(user, msn_group_get_id(group));
			}
		}

		if (group == NULL ||
			(g = gaim_find_group(msn_group_get_name(group))) == NULL)
		{
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Group '%s' appears in server-side "
					   "buddy list, but not here!",
					   msn_group_get_name(group));
		}

		if (group != NULL)
			msn_group_add_user(group, user);

		b = NULL;

		for (sl = buddies; sl != NULL; sl = sl->next)
		{
			b = (GaimBuddy *)sl->data;

			if (gaim_find_buddys_group(b) == g)
				break;

			b = NULL;
		}

		if (b == NULL)
		{
			const char *passport, *friendly;

			passport = msn_user_get_passport(user);

			b = gaim_buddy_new(account, passport, NULL);

			b->proto_data = user;

			gaim_blist_add_buddy(b, NULL, g, NULL);

			if ((friendly = msn_user_get_name(user)) != NULL)
				serv_got_alias(gc, passport, friendly);
		}
		else
			b->proto_data = user;
	}

	g_slist_free(buddies);

	serv_got_alias(gc, (char *)msn_user_get_passport(user),
				   (char *)msn_user_get_name(user));

	return TRUE;
}

/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
msn_accept_add_cb(MsnPermitAdd *pa)
{
	if (g_list_find(gaim_connections_get_all(), pa->gc) != NULL)
	{
		MsnSession *session;
		MsnCmdProc *cmdproc;

		session = pa->gc->proto_data;
		cmdproc = session->notification_conn->cmdproc;

		msn_cmdproc_send(cmdproc, "ADD", "AL %s %s",
						 msn_user_get_passport(pa->user),
						 gaim_url_encode(msn_user_get_name(pa->user)));

		if (cmdproc->error)
			return;

		gaim_privacy_permit_add(pa->gc->account,
								msn_user_get_passport(pa->user), TRUE);
		gaim_account_notify_added(pa->gc->account, NULL,
								  msn_user_get_passport(pa->user),
								  msn_user_get_name(pa->user), NULL);
	}

	msn_user_destroy(pa->user);
	g_free(pa);
}

static void
msn_cancel_add_cb(MsnPermitAdd *pa)
{
	if (g_list_find(gaim_connections_get_all(), pa->gc) != NULL)
	{
		MsnSession *session;
		MsnCmdProc *cmdproc;

		session = pa->gc->proto_data;
		cmdproc = session->notification_conn->cmdproc;

		msn_cmdproc_send(cmdproc, "ADD", "BL %s %s",
						 msn_user_get_passport(pa->user),
						 gaim_url_encode(msn_user_get_name(pa->user)));

		if (cmdproc->error)
			return;

		gaim_privacy_deny_add(pa->gc->account,
							  msn_user_get_passport(pa->user), TRUE);
	}

	msn_user_destroy(pa->user);
	g_free(pa);
}

/**************************************************************************
 * Login
 **************************************************************************/

static void
cvr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	GaimConnection *gc;

	account = cmdproc->session->account;
	gc = gaim_account_get_connection(account);

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
		gaim_connection_error(gc, _("Unable to login using MD5"));

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
	char outparams[MSN_BUF_LEN];

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

		session->syncing_lists = TRUE;

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

		g_snprintf(outparams, sizeof(outparams), "MD5 S ");

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
	GaimConnection *gc;
	gboolean protocol_supported = FALSE;
	char proto_str[8];
	size_t i;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

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
		gaim_connection_error(gc, _("Protocol not supported"));

		return;
	}

	if (session->protocol_ver >= 8)
	{
		msn_cmdproc_send(cmdproc, "CVR",
						 "0x0409 winnt 5.1 i386 MSNMSGR 6.0.0602 MSMSGS %s",
						 gaim_account_get_username(account));
	}
	else
	{
		msn_cmdproc_send(cmdproc, "INF", NULL, NULL);
	}
}

/**************************************************************************
 * Log out
 **************************************************************************/
static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimConnection *gc;

	gc = cmdproc->session->account->gc;

	if (!g_ascii_strcasecmp(cmd->params[0], "OTH"))
	{
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc,
							  _("You have been disconnected. You have "
								"signed on from another location."));
	}
	else if (!g_ascii_strcasecmp(cmd->params[0], "SSD"))
	{
		gaim_connection_error(gc,
							  _("You have been disconnected. The MSN servers "
								"are going down temporarily."));
	}
}

/**************************************************************************
 * Messages
 **************************************************************************/
static void
msg_cmd_post(MsnCmdProc *cmdproc, char *payload, size_t len)
{
	MsnMessage *msg;

	msg = msn_message_new();

	msn_message_parse_payload(msg, payload, len);

	msg->passport = cmdproc->temp;
	msn_cmdproc_process_msg(cmdproc, msg);
	g_free(cmdproc->temp);
	cmdproc->temp = NULL;

	msn_message_destroy(msg);
}

static void
msg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	gaim_debug(GAIM_DEBUG_INFO, "msn", "Found message. Parsing.\n");

	cmdproc->payload_cb  = msg_cmd_post;
	cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	cmdproc->temp = g_strdup(cmd->params[0]);
}

/**************************************************************************
 * Challenges
 **************************************************************************/
static void
chl_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;
	MsnTransaction *trans;
	char buf[MSN_BUF_LEN];
	const char *challenge_resp;
	const char *challenge_str;
	md5_state_t st;
	md5_byte_t di[16];
	int i;

	session = cmdproc->session;
	gc = session->account->gc;

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)cmd->params[1], strlen(cmd->params[1]));

	if (session->protocol_ver >= 8)
	{
		challenge_resp = "VT6PX?UQTM4WM%YR";
		challenge_str  = "PROD0038W!61ZTF9";
	}
	else
	{
		challenge_resp = "Q1P7W2E4J9R8U3S5";
		challenge_str  = "msmsgs@msnmsgr.com";
	}

	md5_append(&st, (const md5_byte_t *)challenge_resp,
			   strlen(challenge_resp));
	md5_finish(&st, di);

	for (i = 0; i < 16; i++)
		g_snprintf(buf + (i*2), 3, "%02x", di[i]);

	trans = msn_transaction_new("QRY", "%s 32", challenge_str);

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
	GaimAccount *account;
	GaimConnection *gc;
	MsnPermitAdd *pa;
	GSList *sl;
	const char *list, *passport, *group_id = NULL;
	const char *friend;
	char msg[MSN_BUF_LEN];

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	list     = cmd->params[1];
	passport = cmd->params[3];
	friend   = gaim_url_decode(cmd->params[4]);

	if (cmd->param_count >= 6)
		group_id = cmd->params[5];

	if (!g_ascii_strcasecmp(list, "FL"))
	{
		user = msn_user_new(session, passport, NULL);

		if (group_id != NULL)
			msn_user_add_group_id(user, atoi(group_id));

		add_buddy(cmdproc, user);

		return;
	}
	else if (g_ascii_strcasecmp(list, "RL"))
		return;

	for (sl = gc->account->permit; sl != NULL; sl = sl->next)
		if (!gaim_utf8_strcasecmp(sl->data, passport))
			return;

	user = msn_user_new(session, passport, friend);
	msn_user_set_name(user, friend);

	pa       = g_new0(MsnPermitAdd, 1);
	pa->user = user;
	pa->gc   = gc;

	g_snprintf(msg, sizeof(msg),
			   _("The user %s (%s) wants to add %s to his or her buddy list."),
			   passport, friend, gaim_account_get_username(account));

	gaim_request_action(gc, NULL, msg, NULL, 0, pa, 2,
						_("Authorize"), G_CALLBACK(msn_accept_add_cb),
						_("Deny"), G_CALLBACK(msn_cancel_add_cb));
}

static void
add_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	const char *list, *passport;
	char *reason;
	char *msg = NULL;
	char **params;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);
	params = g_strsplit(trans->params, " ", 0);

	list     = params[0];
	passport = params[1];

	reason = "invalid user";

	if (!strcmp(list, "FL"))
		msg = g_strdup("Unable to add user on MSN");
	else if (!strcmp(list, "BL"))
		msg = g_strdup("Unable to block user on MSN");
	else if (!strcmp(list, "AL"))
		msg = g_strdup("Unable to permit user on MSN");

	if (!strcmp(list, "FL"))
	{
		reason = g_strdup_printf("%s is not a valid passport account.\n\n"
								 "This user will be automatically removed "
								 "from your %s account's buddy list, so this "
								 "won't appear again.",
								 passport, gaim_account_get_username(account));
	}
	else
	{
		reason = g_strdup_printf("%s is not a valid passport account.",
								 passport);
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

	group = msn_group_new(session, group_id, group_name);

	msn_groups_add(session->groups, group);
}

static void
blp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimConnection *gc;
	const char *list_name;

	gc = cmdproc->session->account->gc;

	if (cmdproc->session->protocol_ver >= 8)
		list_name = cmd->params[0];
	else
		list_name = cmd->params[2];

	if (!g_ascii_strcasecmp(list_name, "AL"))
	{
		/*
		 * If the current setting is AL, messages from users who
		 * are not in BL will be delivered.
		 *
		 * In other words, deny some.
		 */
		gc->account->perm_deny = GAIM_PRIVACY_DENY_USERS;
	}
	else
	{
		/* If the current setting is BL, only messages from people
		 * who are in the AL will be delivered.
		 *
		 * In other words, permit some.
		 */
		gc->account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
	}
}

static void
bpr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;
	const char *passport, *type, *value;
	GaimBuddy *b;
	MsnUser *user;

	session = cmdproc->session;
	gc = session->account->gc;

	if (cmd->param_count == 4)
	{
		passport = cmd->params[1];
		type     = cmd->params[2];
		value    = cmd->params[3];
	}
	else if (cmd->param_count == 2)
	{
		passport = msn_user_get_passport(session->last_user_added);
		type     = cmd->params[0];
		value    = cmd->params[1];
	}
	else
		return;

	user = msn_users_find_with_passport(session->users, passport);

	if (value != NULL)
	{
		if (!strcmp(type, "MOB"))
		{
			if (!strcmp(value, "Y") || !strcmp(value, "N"))
			{
				user->mobile = (!strcmp(value, "Y") ? TRUE : FALSE);

				if ((b = gaim_find_buddy(gc->account, passport)) != NULL)
				{
					if (GAIM_BUDDY_IS_ONLINE(b))
					{
						serv_got_update(gc, (char *)passport,
										1, 0, 0, 0, b->uc);
					}
				}
			}
		}
		else if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(user, gaim_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(user, gaim_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(user, gaim_url_decode(value));
	}
}

static void
fln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimConnection *gc;

	gc = cmdproc->session->account->gc;

	serv_got_update(gc, (char *)cmd->params[0], 0, 0, 0, 0, 0);
}

static void
iln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	int status = 0;
	const char *state, *passport, *friend;
	GaimBuddy *b;

	session = cmdproc->session;
	gc = session->account->gc;

	state    = cmd->params[1];
	passport = cmd->params[2];
	friend   = gaim_url_decode(cmd->params[3]);

	user = msn_users_find_with_passport(session->users, passport);

	serv_got_alias(gc, (char *)passport, (char *)friend);

	msn_user_set_name(user, friend);

	if (session->protocol_ver >= 9 && cmd->param_count == 6)
	{
		msnobj = msn_object_new_from_string(gaim_url_decode(cmd->params[5]));
		msn_user_set_object(user, msnobj);
	}

	if ((b = gaim_find_buddy(gc->account, passport)) != NULL)
		status |= ((((b->uc) >> 1) & 0xF0) << 1);

	if (!g_ascii_strcasecmp(state, "BSY"))
		status |= UC_UNAVAILABLE | (MSN_BUSY << 1);
	else if (!g_ascii_strcasecmp(state, "IDL"))
		status |= UC_UNAVAILABLE | (MSN_IDLE << 1);
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status |= UC_UNAVAILABLE | (MSN_BRB << 1);
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status |= UC_UNAVAILABLE | (MSN_AWAY << 1);
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status |= UC_UNAVAILABLE | (MSN_PHONE << 1);
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status |= UC_UNAVAILABLE | (MSN_LUNCH << 1);

	serv_got_update(gc, (char *)passport, 1, 0, 0, 0, status);
}

static void
ipg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->payload_cb = NULL;
	cmdproc->servconn->payload_len = atoi(cmd->params[2]);
}

static void
lsg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnGroup *group;
	GaimGroup *g;
	const char *name;
	int num_groups, group_id;

	session = cmdproc->session;

	if (session->protocol_ver >= 8)
	{
		group_id = atoi(cmd->params[0]);
		name = gaim_url_decode(cmd->params[1]);
	}
	else
	{
		num_groups = atoi(cmd->params[3]);
		group_id   = atoi(cmd->params[4]);
		name       = gaim_url_decode(cmd->params[5]);

		if (num_groups == 0)
			return;

		if (!strcmp(name, "~"))
			name = _("Buddies");
	}

	group = msn_group_new(session, group_id, name);

	msn_groups_add(session->groups, group);

	if ((g = gaim_find_group(name)) == NULL)
	{
		g = gaim_group_new(name);
		gaim_blist_add_group(g, NULL);
	}
}

static void
lst_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	const char *passport = NULL;
	const char *friend = NULL;

	session = cmdproc->session;
	account = session->account;
	gc = gaim_account_get_connection(account);

	if (session->protocol_ver >= 8)
	{
		const char *group_nums;
		int list_op;

		passport   = cmd->params[0];
		friend     = gaim_url_decode(cmd->params[1]);
		list_op    = atoi(cmd->params[2]);
		group_nums = cmd->params[3];

		if (list_op & MSN_LIST_FL_OP)
		{
			MsnUser *user;
			char **c;
			char **tokens;

			user = msn_user_new(session, passport, friend);

			/* Ensure we have a friendly name set. */
			msn_user_set_name(user, friend);

			tokens = g_strsplit((group_nums ? group_nums : ""), ",", -1);

			gaim_debug_misc("msn", "Fetching group IDs from '%s'\n",
							group_nums);
			for (c = tokens; *c != NULL; c++)
			{
				gaim_debug_misc("msn", "Appending group ID %d\n", atoi(*c));
				msn_user_add_group_id(user, atoi(*c));
			}

			g_strfreev(tokens);

			session->lists.forward =
				g_slist_append(session->lists.forward, user);

			session->last_user_added = user;
		}

		if (list_op & MSN_LIST_AL_OP)
		{
			/* These are users who are allowed to see our status. */

			if (g_slist_find_custom(account->deny, passport,
									(GCompareFunc)strcmp))
			{
				gaim_privacy_deny_remove(gc->account, passport, TRUE);
			}

			gaim_privacy_permit_add(account, passport, TRUE);
		}

		if (list_op & MSN_LIST_BL_OP)
		{
			/* These are users who are not allowed to see our status. */
			gaim_privacy_deny_add(account, passport, TRUE);
		}

		if (list_op & MSN_LIST_RL_OP)
		{
			/* These are users who have us on their contact list. */

			gboolean new_entry = TRUE;

			if (g_slist_find_custom(account->permit, passport,
									(GCompareFunc)g_ascii_strcasecmp) ||
				g_slist_find_custom(account->deny, passport,
									(GCompareFunc)g_ascii_strcasecmp))
			{
				new_entry = FALSE;
			}

			if (new_entry)
			{
				MsnPermitAdd *pa;
				char msg[MSN_BUF_LEN];

				pa       = g_new0(MsnPermitAdd, 1);
				pa->user = msn_user_new(session, passport, friend);
				pa->gc   = gc;

				/* Ensure we have a friendly name set. */
				msn_user_set_name(pa->user, friend);

				g_snprintf(msg, sizeof(msg),
						   _("The user %s (%s) wants to add you to their "
							 "buddy list."),
						   msn_user_get_passport(pa->user),
						   msn_user_get_name(pa->user));

				gaim_request_action(gc, NULL, msg, NULL, 0, pa, 2,
									_("Authorize"),
									G_CALLBACK(msn_accept_add_cb),
									_("Deny"),
									G_CALLBACK(msn_cancel_add_cb));
			}
		}

		session->num_users++;

		if (session->num_users == session->total_users)
		{
			msn_user_set_buddy_icon(session->user,
				gaim_account_get_buddy_icon(session->account));

			if (!msn_session_change_status(session, "NLN"))
				return;

			gaim_connection_set_state(gc, GAIM_CONNECTED);
			serv_finish_login(gc);

			if (session->lists.allow == NULL)
				session->lists.allow = g_slist_copy(account->permit);
			else
				session->lists.allow = g_slist_concat(session->lists.allow,
													  account->permit);

			if (session->lists.block == NULL)
				session->lists.block = g_slist_copy(account->permit);
			else
				session->lists.block = g_slist_concat(session->lists.block,
													  account->deny);

			while (session->lists.forward != NULL)
			{
				MsnUser *user = session->lists.forward->data;
				GSList *buddies;
				GSList *sl;

				session->lists.forward =
					g_slist_remove(session->lists.forward, user);

				add_buddy(cmdproc, user);

				buddies = gaim_find_buddies(account,
											msn_user_get_passport(user));

				/* Find all occurrences of this buddy in the wrong place. */
				for (sl = buddies; sl != NULL; sl = sl->next)
				{
					GaimBuddy *b = sl->data;

					if (b->proto_data == NULL)
					{
						gaim_debug_warning("msn",
							"Deleting misplaced user %s (%s) during sync "
							"with server.\n",
							b->name, gaim_find_buddys_group(b)->name);

						gaim_blist_remove_buddy(b);
					}
				}

				g_slist_free(buddies);
			}

			session->syncing_lists = FALSE;
			session->lists_synced  = TRUE;
		}
	}
	else
	{
		const char *list_name;
		int user_num;
		int num_users;

		list_name = cmd->params[1];
		user_num  = atoi(cmd->params[3]);
		num_users = atoi(cmd->params[4]);

		if (g_ascii_strcasecmp(list_name, "RL") &&
			user_num == 0 && num_users == 0)
		{
			return; /* There are no users on this list. */
		}

		if (num_users > 0)
		{
			passport  = cmd->params[5];
			friend    = gaim_url_decode(cmd->params[6]);
		}

		if (session->syncing_lists && session->lists_synced)
			return;

		if (!g_ascii_strcasecmp(list_name, "FL") && user_num != 0)
		{
			/* These are users on our contact list. */
			MsnUser *user;

			user = msn_user_new(session, passport, friend);

			if (cmd->param_count == 8)
				msn_user_add_group_id(user, atoi(cmd->params[7]));

			session->lists.forward =
				g_slist_append(session->lists.forward, user);
		}
		else if (!g_ascii_strcasecmp(list_name, "AL") && user_num != 0)
		{
			/* These are users who are allowed to see our status. */
			if (g_slist_find_custom(gc->account->deny, passport,
									(GCompareFunc)strcmp))
			{
				gaim_debug(GAIM_DEBUG_INFO, "msn",
						   "Moving user from deny list to permit: %s (%s)\n",
						   passport, friend);

				gaim_privacy_deny_remove(gc->account, passport, TRUE);
			}

			gaim_privacy_permit_add(gc->account, passport, TRUE);
		}
		else if (!g_ascii_strcasecmp(list_name, "BL") && user_num != 0)
		{
			/* These are users who are not allowed to see our status. */
			gaim_privacy_deny_add(gc->account, passport, TRUE);
		}
		else if (!g_ascii_strcasecmp(list_name, "RL"))
		{
			/* These are users who have us on their contact list. */
			if (user_num > 0)
			{
				gboolean new_entry = TRUE;

				if (g_slist_find_custom(gc->account->permit, passport,
										(GCompareFunc)g_ascii_strcasecmp))
				{
					new_entry = FALSE;
				}

				if (g_slist_find_custom(gc->account->deny, passport,
										(GCompareFunc)g_ascii_strcasecmp))
				{
					new_entry = FALSE;
				}

				if (new_entry)
				{
					MsnPermitAdd *pa;
					char msg[MSN_BUF_LEN];

					gaim_debug(GAIM_DEBUG_WARNING, "msn",
							   "Unresolved MSN RL entry: %s\n", passport);

					pa       = g_new0(MsnPermitAdd, 1);
					pa->user = msn_user_new(session, passport, friend);
					pa->gc   = gc;

					msn_user_set_name(pa->user, friend);

					g_snprintf(msg, sizeof(msg),
							   _("The user %s (%s) wants to add you to their "
								 "buddy list."),
							   msn_user_get_passport(pa->user),
							   msn_user_get_name(pa->user));

					gaim_request_action(gc, NULL, msg, NULL, 0, pa, 2,
										_("Authorize"),
										G_CALLBACK(msn_accept_add_cb),
										_("Deny"),
										G_CALLBACK(msn_cancel_add_cb));
				}
			}

			if (user_num != num_users)
				return; /* This isn't the last one in the RL. */

			/* Now we're at the last one, so we can do final work. */
			if (!session->lists_synced)
			{
				if (!msn_session_change_status(session, "NLN"))
					return;

				gaim_connection_set_state(gc, GAIM_CONNECTED);
				serv_finish_login(gc);
			}

			if (session->lists.allow == NULL)
				session->lists.allow = g_slist_copy(gc->account->permit);
			else
				session->lists.allow = g_slist_concat(session->lists.allow,
													  gc->account->permit);

			if (session->lists.block == NULL)
				session->lists.block = g_slist_copy(gc->account->deny);
			else
				session->lists.block = g_slist_concat(session->lists.block,
													  gc->account->deny);

			while (session->lists.forward != NULL)
			{
				MsnUser *user = session->lists.forward->data;

				session->lists.forward =
					g_slist_remove(session->lists.forward, user);

				add_buddy(cmdproc, user);
			}

			session->syncing_lists = FALSE;
			session->lists_synced  = TRUE;
		}
	}
}

static void
nln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	const char *state;
	const char *passport;
	const char *friend;
	int status = 0;

	session = cmdproc->session;
	gc = session->account->gc;

	state    = cmd->params[0];
	passport = cmd->params[1];
	friend   = gaim_url_decode(cmd->params[2]);

	user = msn_users_find_with_passport(session->users, passport);

	serv_got_alias(gc, (char *)passport, (char *)friend);

	msn_user_set_name(user, friend);

	if (session->protocol_ver >= 9 && cmd->param_count == 5)
	{
		msnobj = msn_object_new_from_string(gaim_url_decode(cmd->params[4]));
		msn_user_set_object(user, msnobj);
	}

	if (!g_ascii_strcasecmp(state, "BSY"))
		status |= UC_UNAVAILABLE | (MSN_BUSY << 1);
	else if (!g_ascii_strcasecmp(state, "IDL"))
		status |= UC_UNAVAILABLE | (MSN_IDLE << 1);
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status |= UC_UNAVAILABLE | (MSN_BRB << 1);
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status |= UC_UNAVAILABLE | (MSN_AWAY << 1);
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status |= UC_UNAVAILABLE | (MSN_PHONE << 1);
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status |= UC_UNAVAILABLE | (MSN_LUNCH << 1);

	serv_got_update(gc, (char *)passport, 1, 0, 0, 0, status);
}

static void
not_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->payload_cb  = NULL;
	cmdproc->servconn->payload_len = atoi(cmd->params[1]);
}

static void
prp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	const char *type, *value;

	session = cmdproc->session;

	type  = cmd->params[2];
	value = cmd->params[3];

	if (cmd->param_count == 4)
	{
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, gaim_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, gaim_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, gaim_url_decode(value));
	}
}

static void
rea_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;
	const char *friend;

	session = cmdproc->session;
	gc = session->account->gc;
	friend = gaim_url_decode(cmd->params[3]);

	gaim_connection_set_display_name(gc, friend);
}

static void
reg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnGroup *group;
	int group_id;
	const char *group_name;

	session = cmdproc->session;
	group_id = atoi(cmd->params[2]);

	group_name = gaim_url_decode(cmd->params[3]);

	group = msn_groups_find_with_id(session->groups, group_id);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Renamed group %s to %s\n",
			   msn_group_get_name(group), group_name);

	if (group != NULL)
		msn_group_set_name(group, group_name);
}

static void
rem_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	const char *passport;

	session = cmdproc->session;
	passport = cmd->params[3];

	if (cmd->param_count == 5)
	{
		MsnUser *user;
		int group_id = atoi(cmd->params[4]);

		user = msn_users_find_with_passport(session->users, passport);

		msn_user_remove_group_id(user, group_id);
	}

	/* I hate this. */
	/* shx: it won't be here for long. */
	if (session->moving_buddy)
	{
		MsnGroup *group, *old_group;
		const char *friendly;

		group = msn_groups_find_with_name(session->groups,
										  session->dest_group_name);

		old_group = session->old_group;

		session->moving_buddy = FALSE;
		session->old_group    = NULL;

		if (group == NULL)
		{
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Still don't have a group ID for %s while moving %s!\n",
					   session->dest_group_name, passport);

			g_free(session->dest_group_name);
			session->dest_group_name = NULL;
		}

		g_free(session->dest_group_name);
		session->dest_group_name = NULL;

		if ((friendly = msn_user_get_name(session->moving_user)) == NULL)
			friendly = passport;

		msn_cmdproc_send(cmdproc, "ADD", "FL %s %s %d",
						 passport, gaim_url_encode(friendly),
						 msn_group_get_id(group));

		if (cmdproc->error)
			return;

		if (old_group != NULL)
			msn_group_remove_user(old_group, session->moving_user);

		msn_user_unref(session->moving_user);

		session->moving_user = NULL;

		if (old_group != NULL &&
			msn_users_get_count(msn_group_get_users(old_group)) <= 0)
		{
			msn_cmdproc_send(cmdproc, "REM", "%s", "%d",
							 msn_group_get_id(old_group));
		}
	}
}

static void
rmg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnGroup *group;

	session = cmdproc->session;
	group = msn_groups_find_with_id(session->groups, atoi(cmd->params[2]));

	if (group != NULL)
		msn_groups_remove(session->groups, group);
}

static void
syn_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimConnection *gc;

	session = cmdproc->session;
	gc = gaim_account_get_connection(session->account);

	if (session->protocol_ver >= 8)
	{
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

			gaim_connection_error(gc, buf);

			g_free(buf);

			return;
		}

		session->total_users  = atoi(cmd->params[2]);
		session->total_groups = atoi(cmd->params[3]);

		if (session->total_users == 0)
		{
			gaim_connection_set_state(gc, GAIM_CONNECTED);
			serv_finish_login(gc);

			session->syncing_lists = FALSE;
			session->lists_synced  = TRUE;
		}
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

	if ((fd = gaim_mkstemp(&session->passport_info.file)) == NULL)
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
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Error closing temp passport file: %s\n",
					   strerror(errno));

			unlink(session->passport_info.file);
			g_free(session->passport_info.file);
		}
		else
		{
			/*
			 * Renaming file with .html extension, so that the
			 * win32 open_url will work.
			 */
			char *tmp;

			if ((tmp = g_strdup_printf("%s.html",
					session->passport_info.file)) != NULL)
			{
				if (rename(session->passport_info.file, tmp) == 0)
				{
					g_free(session->passport_info.file);
					session->passport_info.file = tmp;
				}
				else
					g_free(tmp);
			}
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
	MsnUser *user;
	const char *session_id;
	char *host, *c;
	int port;

	session = cmdproc->session;
	session_id = cmd->params[0];

	host = g_strdup(cmd->params[1]);

	if ((c = strchr(host, ':')) != NULL)
	{
		*c = '\0';
		port = atoi(c + 1);
	}
	else
		port = 1863;

	if (session->http_method)
		port = 80;

	swboard = msn_switchboard_new(session);

	user = msn_user_new(session, cmd->params[4], NULL);

	msn_switchboard_set_invited(swboard, TRUE);
	msn_switchboard_set_session_id(swboard, cmd->params[0]);
	msn_switchboard_set_auth_key(swboard, cmd->params[3]);
	msn_switchboard_set_user(swboard, user);

	if (!msn_switchboard_connect(swboard, host, port))
	{
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Unable to connect to switchboard on %s, port %d\n",
				   host, port);

		g_free(host);

		return;
	}

	g_free(host);
}

static void
xfr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	GaimConnection *gc;
	char *host;
	char *c;
	int port;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(cmd->params[1], "SB") && strcmp(cmd->params[1], "NS"))
	{
		gaim_connection_error(gc, _("Got invalid XFR"));

		return;
	}

	host = g_strdup(cmd->params[2]);

	if ((c = strchr(host, ':')) != NULL)
	{
		*c = '\0';
		port = atoi(c + 1);
	}
	else
		port = 1863;

	if (!strcmp(cmd->params[1], "SB"))
	{
		swboard = msn_session_find_unused_switch(session);

		if (swboard == NULL)
		{
			gaim_debug_error("msn",
							 "Received an XFR SB request when there's no unused "
							 "switchboards!\n");
			return;
		}

		msn_switchboard_set_auth_key(swboard, cmd->params[4]);

		if (session->http_method)
			port = 80;

		if (!msn_switchboard_connect(swboard, host, port))
		{
			gaim_debug_error("msn",
							 "Unable to connect to switchboard on %s, port %d\n",
							 host, port);

			g_free(host);

			return;
		}
	}
	else if (!strcmp(cmd->params[1], "NS"))
	{
		if (!msn_notification_connect(session->notification_conn, host,
									  port))
		{
			gaim_connection_error(gc, _("Unable to transfer to "
										"notification server"));

			g_free(host);

			return;
		}
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
	const char *passport;

	session = cmdproc->session;
	passport = msg->passport;

	if (strcmp(passport, "Hotmail"))
	{
		/* This isn't an official message. */
		return;
	}

	if ((value = msn_message_get_attr(msg, "kv")) != NULL)
		session->passport_info.kv = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "sid")) != NULL)
		session->passport_info.sid = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "MSPAuth")) != NULL)
		session->passport_info.mspauth = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "ClientIP")) != NULL)
		session->passport_info.client_ip = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "ClientPort")) != NULL)
		session->passport_info.client_port = ntohs(atoi(value));
}

static void
initial_email_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	GaimConnection *gc;
	GHashTable *table;
	const char *unread;
	const char *passport;

	session = cmdproc->session;
	gc = session->account->gc;
	passport = msg->passport;

	if (strcmp(passport, "Hotmail"))
	{
		/* This isn't an official message. */
		return;
	}

	if (!gaim_account_get_check_mail(session->account))
		return;

	if (session->passport_info.file == NULL)
	{
		msn_cmdproc_send(cmdproc, "URL", "%s", "INBOX");

		msn_cmdproc_queue_message(cmdproc, "URL", msg);

		return;
	}

	table = msn_message_get_hashtable_from_body(msg);

	unread = g_hash_table_lookup(table, "Inbox-Unread");

	if (unread != NULL)
	{
		int count = atoi(unread);

		if (count != 0)
		{
			const char *passport = msn_user_get_passport(session->user);
			const char *file = session->passport_info.file;
			gchar *url;
			while (*file && *file == '/')
				++file;
			url = g_strconcat ("file:///", file, 0);
			gaim_notify_emails(gc, count, FALSE, NULL, NULL,
							   &passport, (const char **)&url, NULL, NULL);
			g_free (url);
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
	const char *passport;

	session = cmdproc->session;
	gc = session->account->gc;
	passport = msg->passport;

	from = subject = NULL;

	if (strcmp(passport, "Hotmail"))
	{
		/* This isn't an official message. */
		return;
	}

	if (!gaim_account_get_check_mail(session->account))
		return;

	if (session->passport_info.file == NULL)
	{
		msn_cmdproc_send(cmdproc, "URL", "%s", "INBOX");

		msn_cmdproc_queue_message(cmdproc, "URL", msg);

		return;
	}

	table = msn_message_get_hashtable_from_body(msg);

	tmp = g_hash_table_lookup(table, "From");
	if (tmp != NULL)
		from = gaim_mime_decode_field(tmp);

	tmp = g_hash_table_lookup(table, "Subject");
	if (tmp != NULL)
		subject = gaim_mime_decode_field(tmp);

	if (from != NULL && subject != NULL)	
		gaim_notify_email(gc, subject, from, msn_user_get_passport(session->user),
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
	const char *passport;

	passport = msg->passport;

	if (strcmp(passport, "Hotmail"))
	{
		/* This isn't an official message. */
		return;
	}

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

static gboolean
connect_cb(MsnServConn *servconn)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	char **a, **c, *vers;
	size_t i;

	g_return_val_if_fail(servconn != NULL, FALSE);

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
		return FALSE;

	session->user = msn_user_new(session,
								 gaim_account_get_username(account), NULL);

	gaim_connection_update_progress(gc, _("Syncing with server"),
									4, MSN_CONNECT_STEPS);

	return TRUE;
}

void
msn_notification_init(void)
{
	cbs_table = msn_table_new();

	/* Register the command callbacks. */

	/* Syncing */
	msn_table_add_cmd(cbs_table, NULL, "GTC", NULL);
	msn_table_add_cmd(cbs_table, NULL, "BLP", blp_cmd);
	msn_table_add_cmd(cbs_table, NULL, "PRP", prp_cmd);
	msn_table_add_cmd(cbs_table, NULL, "LSG", lsg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "LST", lst_cmd);
	msn_table_add_cmd(cbs_table, NULL, "BPR", bpr_cmd);

	/* Syncronous */
	/* msn_table_add_cmd(cbs_table, "CHG", "CHG", chg_cmd); */
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

	msn_table_add_error(cbs_table, "ADD", add_error);

	/* Register the message type callbacks. */
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

MsnServConn *
msn_notification_new(MsnSession *session)
{
	MsnServConn *notification;
	MsnCmdProc *cmdproc;

	notification = msn_servconn_new(session, MSN_SERVER_NS);
	cmdproc = notification->cmdproc;

	msn_servconn_set_connect_cb(notification, connect_cb);

	if (session->http_method)
		notification->http_data->server_type = "NS";

	cmdproc->cbs_table  = cbs_table;

	return notification;
}

gboolean
msn_notification_connect(MsnServConn *notification, const char *host, int port)
{
	g_return_val_if_fail(notification != NULL, FALSE);

	return msn_servconn_connect(notification, host, port);
}
