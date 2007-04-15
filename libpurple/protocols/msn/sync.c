/**
 * @file sync.c MSN list synchronization functions
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
#include "msn.h"
#include "sync.h"
#include "state.h"

static MsnTable *cbs_table;

static void
blp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	PurpleConnection *gc = cmdproc->session->account->gc;
	const char *list_name;

	list_name = cmd->params[0];

	if (!g_ascii_strcasecmp(list_name, "AL"))
	{
		/*
		 * If the current setting is AL, messages from users who
		 * are not in BL will be delivered.
		 *
		 * In other words, deny some.
		 */
		gc->account->perm_deny = PURPLE_PRIVACY_DENY_USERS;
	}
	else
	{
		/* If the current setting is BL, only messages from people
		 * who are in the AL will be delivered.
		 *
		 * In other words, permit some.
		 */
		gc->account->perm_deny = PURPLE_PRIVACY_ALLOW_USERS;
	}
}

static void
prp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session = cmdproc->session;
	const char *type, *value;

	type  = cmd->params[0];
	value = cmd->params[1];

	if (cmd->param_count == 2)
	{
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, purple_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, purple_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, purple_url_decode(value));
	}
	else
	{
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, NULL);
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, NULL);
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, NULL);
	}
}

static void
lsg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session = cmdproc->session;
	const char *name;
	const char *group_id;

	group_id = cmd->params[0];
	name = purple_url_decode(cmd->params[1]);

	msn_group_new(session->userlist, group_id, name);

	/* HACK */
	if (group_id == 0)
		/* Group of ungroupped buddies */
		return;

	if ((purple_find_group(name)) == NULL)
	{
		PurpleGroup *g = purple_group_new(name);
		purple_blist_add_group(g, NULL);
	}
}

static void
lst_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session = cmdproc->session;
	char *passport = NULL;
	const char *friend = NULL;
	int list_op;
	MsnUser *user;

	passport = cmd->params[0];
	friend   = purple_url_decode(cmd->params[1]);
	list_op  = atoi(cmd->params[2]);

	user = msn_user_new(session->userlist, passport, friend);

	msn_userlist_add_user(session->userlist, user);

	session->sync->last_user = user;

	/* TODO: This can be improved */

	if (list_op & MSN_LIST_FL_OP)
	{
		char **c;
		char **tokens;
		const char *group_nums;
		GSList *group_ids;

		group_nums = cmd->params[3];

		group_ids = NULL;

		tokens = g_strsplit(group_nums, ",", -1);

		for (c = tokens; *c != NULL; c++)
		{
			int id;

			id = atoi(*c);
			group_ids = g_slist_append(group_ids, GINT_TO_POINTER(id));
		}

		g_strfreev(tokens);

		msn_got_lst_user(session, user, list_op, group_ids);

		g_slist_free(group_ids);
	}
	else
	{
		msn_got_lst_user(session, user, list_op, NULL);
	}

	session->sync->num_users++;

	if (session->sync->num_users == session->sync->total_users)
	{
		cmdproc->cbs_table = session->sync->old_cbs_table;

		msn_session_finish_login(session);

		msn_sync_destroy(session->sync);
		session->sync = NULL;
	}
}

static void
bpr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSync *sync = cmdproc->session->sync;
	const char *type, *value;
	MsnUser *user;

	user = sync->last_user;

	type     = cmd->params[0];
	value    = cmd->params[1];

	if (value)
	{
		if (!strcmp(type, "MOB"))
		{
			if (!strcmp(value, "Y"))
				user->mobile = TRUE;
		}
		else if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(user, purple_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(user, purple_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(user, purple_url_decode(value));
	}
}

void
msn_sync_init(void)
{
	/* TODO: check prp, blp, bpr */

	cbs_table = msn_table_new();

	/* Syncing */
	msn_table_add_cmd(cbs_table, NULL, "GTC", NULL);
	msn_table_add_cmd(cbs_table, NULL, "BLP", blp_cmd);
	msn_table_add_cmd(cbs_table, NULL, "PRP", prp_cmd);
	msn_table_add_cmd(cbs_table, NULL, "LSG", lsg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "LST", lst_cmd);
	msn_table_add_cmd(cbs_table, NULL, "BPR", bpr_cmd);
}

void
msn_sync_end(void)
{
	msn_table_destroy(cbs_table);
}

MsnSync *
msn_sync_new(MsnSession *session)
{
	MsnSync *sync;

	sync = g_new0(MsnSync, 1);

	sync->session = session;
	sync->cbs_table = cbs_table;

	return sync;
}

void
msn_sync_destroy(MsnSync *sync)
{
	g_free(sync);
}
