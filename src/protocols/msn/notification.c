/**
 * @file notification.c Notification server functions
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
#include "notification.h"
#include "state.h"
#include "error.h"
#include "utils.h"

typedef struct
{
	struct gaim_connection *gc;
	MsnUser *user;

} MsnPermitAdd;

static GHashTable *notification_commands  = NULL;
static GHashTable *notification_msg_types = NULL;
G_MODULE_IMPORT GSList *connections;

/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
msn_accept_add_cb(MsnPermitAdd *pa)
{
	if (g_slist_find(connections, pa->gc) != NULL) {
		MsnSession *session = pa->gc->proto_data;
		char outparams[MSN_BUF_LEN];

		g_snprintf(outparams, sizeof(outparams), "AL %s %s",
				   msn_user_get_passport(pa->user),
				   msn_url_encode(msn_user_get_name(pa->user)));

		if (msn_servconn_send_command(session->notification_conn,
									  "ADD", outparams) <= 0) {
			hide_login_progress(pa->gc, _("Write error"));
			signoff(pa->gc);
			return;
		}

		gaim_privacy_permit_add(pa->gc->account,
								msn_user_get_passport(pa->user));
		build_allow_list();

		show_got_added(pa->gc, NULL, msn_user_get_passport(pa->user),
					   msn_user_get_name(pa->user), NULL);
	}

	msn_user_destroy(pa->user);
	g_free(pa);
}

static void
msn_cancel_add_cb(MsnPermitAdd *pa)
{
	if (g_slist_find(connections, pa->gc) != NULL) {
		MsnSession *session = pa->gc->proto_data;
		char outparams[MSN_BUF_LEN];

		g_snprintf(outparams, sizeof(outparams), "BL %s %s",
				   msn_user_get_passport(pa->user),
				   msn_url_encode(msn_user_get_name(pa->user)));

		if (msn_servconn_send_command(session->notification_conn,
									  "ADD", outparams) <= 0) {
			hide_login_progress(pa->gc, _("Write error"));
			signoff(pa->gc);
			return;
		}

		gaim_privacy_deny_add(pa->gc->account,
							  msn_user_get_passport(pa->user));
		build_block_list();
	}

	msn_user_destroy(pa->user);
	g_free(pa);
}

/**************************************************************************
 * Catch-all commands
 **************************************************************************/
static gboolean
__blank_cmd(MsnServConn *servconn, const char *command, const char **params,
			size_t param_count)
{
	return TRUE;
}

static gboolean
__unknown_cmd(MsnServConn *servconn, const char *command, const char **params,
			  size_t param_count)
{
	char buf[MSN_BUF_LEN];

	if (isdigit(*command)) {
		int errnum = atoi(command);

		if (errnum == 225) {
			/*
			 * Ignore this. It happens as a result of moving a buddy from
			 * one group that isn't on the server to another that is.
			 * The user doesn't care if the old group was there or not.
			 */
			return TRUE;
		}

		g_snprintf(buf, sizeof(buf), "MSN Error: %s\n",
				   msn_error_get_text(errnum));
	}
	else {
		g_snprintf(buf, sizeof(buf), "MSN Error: Unable to parse message\n");
	}

	do_error_dialog(buf, NULL, GAIM_ERROR);

	return TRUE;
}


/**************************************************************************
 * Login
 **************************************************************************/
static gboolean
__ver_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	size_t i;
	gboolean msnp5_found = FALSE;

	for (i = 1; i < param_count; i++) {
		if (!strcmp(params[i], "MSNP5")) {
			msnp5_found = TRUE;
			break;
		}
	}

	if (!msnp5_found) {
		hide_login_progress(gc, _("Protocol not supported"));
		signoff(gc);

		return FALSE;
	}

	if (!msn_servconn_send_command(servconn, "INF", NULL)) {
		hide_login_progress(gc, _("Unable to request INF"));
		signoff(gc);

		return FALSE;
	}

	return TRUE;
}

static gboolean
__inf_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	char outparams[MSN_BUF_LEN];

	if (strcmp(params[1], "MD5")) {
		hide_login_progress(gc, _("Unable to login using MD5"));
		signoff(gc);

		return FALSE;
	}

	g_snprintf(outparams, sizeof(outparams), "MD5 I %s", gc->username);

	if (!msn_servconn_send_command(servconn, "USR", outparams)) {
		hide_login_progress(gc, _("Unable to send USR"));
		signoff(gc);

		return FALSE;
	}

	set_login_progress(gc, 4, _("Requesting to send password"));

	return TRUE;
}

static gboolean
__usr_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	char outparams[MSN_BUF_LEN];

	/* We're either getting the challenge or the OK. Let's find out. */
	if (!g_ascii_strcasecmp(params[1], "OK")) {
		/* OK */

		if (!msn_servconn_send_command(servconn, "SYN", "0")) {
			hide_login_progress(gc, _("Unable to write"));
			signoff(gc);

			return FALSE;
		}

		set_login_progress(session->account->gc, 4, _("Retrieving buddy list"));
	}
	else {
		/* Challenge */
		const char *challenge = params[3];
		char buf[MSN_BUF_LEN];
		md5_state_t st;
		md5_byte_t di[16];
		int i;

		g_snprintf(buf, sizeof(buf), "%s%s", challenge, gc->password);

		md5_init(&st);
		md5_append(&st, (const md5_byte_t *)buf, strlen(buf));
		md5_finish(&st, di);

		g_snprintf(outparams, sizeof(outparams), "MD5 S ");

		for (i = 0; i < 16; i++) {
			g_snprintf(buf, sizeof(buf), "%02x", di[i]);
			strcat(outparams, buf);
		}

		if (!msn_servconn_send_command(servconn, "USR", outparams)) {
			hide_login_progress(gc, _("Unable to send password"));
			signoff(gc);

			return FALSE;
		}

		set_login_progress(gc, 4, _("Password sent"));
	}

	return TRUE;
}

/**************************************************************************
 * Log out
 **************************************************************************/
static gboolean
__out_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;

	if (!g_ascii_strcasecmp(params[0], "OTH")) {
		hide_login_progress(gc,
							_("You have been disconnected. You have "
							  "signed on from another location."));
		signoff(gc);
	}
	else if (!g_ascii_strcasecmp(params[0], "SSD")) {
		hide_login_progress(gc,
							_("You have been disconnected. The MSN servers "
							  "are going down temporarily."));
		signoff(gc);
	}

	return FALSE;
}

/**************************************************************************
 * Messages
 **************************************************************************/
static gboolean
__msg_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	gaim_debug(GAIM_DEBUG_INFO, "msn", "Found message. Parsing.\n");

	servconn->parsing_msg = TRUE;
	servconn->msg_passport = g_strdup(params[0]);
	servconn->msg_friendly = g_strdup(params[1]);
	servconn->msg_len      = atoi(params[2]);

	return TRUE;
}

/**************************************************************************
 * Challenges
 **************************************************************************/
static gboolean
__chl_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	char buf[MSN_BUF_LEN];
	char buf2[3];
	md5_state_t st;
	md5_byte_t di[16];
	int i;

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)params[1], strlen(params[1]));
	md5_append(&st, (const md5_byte_t *)"Q1P7W2E4J9R8U3S5",
			   strlen("Q1P7W2E4J9R8U3S5"));
	md5_finish(&st, di);

	g_snprintf(buf, sizeof(buf),
			   "QRY %u msmsgs@msnmsgr.com 32\r\n",
			   servconn->session->trId++);

	for (i = 0; i < 16; i++) {
		g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
		strcat(buf, buf2);
	}

	if (msn_servconn_write(servconn, buf, strlen(buf)) <= 0) {
		hide_login_progress(gc, _("Unable to write to server"));
		signoff(gc);
	}

	return TRUE;
}

/**************************************************************************
 * Buddy Lists
 **************************************************************************/
static gboolean
__add_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	MsnPermitAdd *pa;
	GSList *sl;
	const char *list, *passport;
	char *friend;
	char msg[MSN_BUF_LEN];

	list = params[1];
	passport = params[3];

	friend = msn_url_decode(params[4]);

	if (g_ascii_strcasecmp(list, "RL"))
		return TRUE;

	for (sl = gc->account->permit; sl != NULL; sl = sl->next) {
		if (!gaim_utf8_strcasecmp(sl->data, passport))
			return TRUE;
	}

	pa = g_new0(MsnPermitAdd, 1);
	pa->user = msn_user_new(session, passport, friend);
	pa->gc = gc;

	g_snprintf(msg, sizeof(msg),
			   _("The user %s (%s) wants to add %s to his or her buddy list."),
			   passport, friend, gc->username);

	do_ask_dialog(msg, NULL, pa,
				  _("Authorize"), msn_accept_add_cb,
				  _("Deny"), msn_cancel_add_cb,
				  session->prpl->handle, FALSE);

	return TRUE;
}

static gboolean
__adg_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	gint *group_id;
	char *group_name;

	group_id = g_new(gint, 1);
	*group_id = atoi(params[3]);

	group_name = msn_url_decode(params[2]);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Added group %s (id %d)\n",
			   group_name, group_id);

	g_hash_table_insert(session->group_ids, group_name, group_id);
	g_hash_table_insert(session->group_names, group_id, g_strdup(group_name));

	return TRUE;
}

static gboolean
__blp_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;

	if (!g_ascii_strcasecmp(params[2], "AL")) {
		/*
		 * If the current setting is AL, messages from users who
		 * are not in BL will be delivered.
		 *
		 * In other words, deny some.
		 */
		gc->account->permdeny = DENY_SOME;
	}
	else {
		/* If the current setting is BL, only messages from people
		 * who are in the AL will be delivered.
		 *
		 * In other words, permit some.
		 */
		gc->account->permdeny = PERMIT_SOME;
	}

	return TRUE;
}

static gboolean
__bpr_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	const char *passport, *type, *value;
	struct buddy *b;
	MsnUser *user;

	passport = params[1];
	type     = params[2];
	value    = params[3];

	user = msn_users_find_with_passport(session->users, passport);

	if (value != NULL) {
		if (!strcmp(type, "MOB")) {
			if (!strcmp(value, "Y")) {
				user->mobile = TRUE;

				if ((b = gaim_find_buddy(gc->account, passport)) != NULL) {
					if (GAIM_BUDDY_IS_ONLINE(b)) {
						serv_got_update(gc, (char *)passport,
										1, 0, 0, 0, b->uc);
					}
				}
			}
		}
		else if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(user, msn_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(user, msn_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(user, msn_url_decode(value));
	}

	return TRUE;
}

static gboolean
__fln_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;

	serv_got_update(gc, (char *)params[0], 0, 0, 0, 0, 0);

	return TRUE;
}

static gboolean
__iln_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	int status = 0;
	const char *state, *passport, *friend;
	struct buddy *b;

	state    = params[1];
	passport = params[2];
	friend   = msn_url_decode(params[3]);

	serv_got_alias(gc, (char *)passport, (char *)friend);

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

	return TRUE;
}

static gboolean
__lsg_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct group *g;
	const char *name;
	int group_num, num_groups, group_id;
	gint *group_id_1, *group_id_2;

	group_num  = atoi(params[2]);
	num_groups = atoi(params[3]);
	group_id   = atoi(params[4]);
	name       = msn_url_decode(params[5]);

	if (num_groups == 0)
		return TRUE;

	if (group_num == 1) {
		session->group_names = g_hash_table_new_full(g_int_hash, g_int_equal,
													 g_free, g_free);
		session->group_ids = g_hash_table_new_full(g_str_hash, g_str_equal,
												   g_free, g_free);
	}

	group_id_1 = g_new(gint, 1);
	group_id_2 = g_new(gint, 1);

	*group_id_1 = group_id;
	*group_id_2 = group_id;

	if (!strcmp(name, "~"))
		name = _("Buddies");

	g_hash_table_insert(session->group_names, group_id_1, g_strdup(name));
	g_hash_table_insert(session->group_ids, g_strdup(name), group_id_2);

	if ((g = gaim_find_group(name)) == NULL) {
		g = gaim_group_new(name);
		gaim_blist_add_group(g, NULL);
	}

	return TRUE;
}

static gboolean
__lst_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	int user_num;
	int num_users;
	const char *type;
	const char *passport;
	const char *friend;

	user_num  = atoi(params[3]);
	num_users = atoi(params[4]);

	if (user_num == 0 && num_users == 0)
		return TRUE; /* There are no users on this list. */

	type      = params[1];
	passport  = params[5];
	friend    = msn_url_decode(params[6]);

	if (!g_ascii_strcasecmp(type, "FL") && user_num != 0) {
		/* These are users on our contact list. */
		MsnUser *user;

		user = msn_user_new(session, passport, friend);

		if (param_count == 8)
			msn_user_set_group_id(user, atoi(params[7]));

		session->lists.forward = g_slist_append(session->lists.forward, user);
	}
	else if (!g_ascii_strcasecmp(type, "AL") && user_num != 0) {
		/* These are users who are allowed to see our status. */
		if (g_slist_find_custom(gc->account->deny, passport,
								(GCompareFunc)strcmp)) {

			gaim_debug(GAIM_DEBUG_INFO, "msn",
					   "Moving user from deny list to permit: %s (%s)\n",
					   passport, friend);

			gaim_privacy_deny_remove(gc->account, passport);
		}

		gaim_privacy_permit_add(gc->account, passport);
	}
	else if (!g_ascii_strcasecmp(type, "BL") && user_num != 0) {
		/* These are users who are not allowed to see our status. */
		gaim_privacy_deny_add(gc->account, passport);
	}
	else if (!g_ascii_strcasecmp(type, "RL")) {
		/* These are users who have us on their contact list. */
		if (user_num > 0) {
			gboolean new_entry = TRUE;

			if (g_slist_find_custom(gc->account->permit, passport,
									(GCompareFunc)g_ascii_strcasecmp)) {
				new_entry = FALSE;
			}
			
			if (g_slist_find_custom(gc->account->deny, passport,
									(GCompareFunc)g_ascii_strcasecmp)) {
				new_entry = FALSE;
			}

			if (new_entry) {
				MsnPermitAdd *pa;
				char msg[MSN_BUF_LEN];

				gaim_debug(GAIM_DEBUG_WARNING, "msn",
						   "Unresolved MSN RL entry: %s\n", passport);

				pa       = g_new0(MsnPermitAdd, 1);
				pa->user = msn_user_new(session, passport, friend);
				pa->gc   = gc;

				g_snprintf(msg, sizeof(msg),
						   _("The user %s (%s) wants to add you to their "
							 "buddy list."),
						   msn_user_get_passport(pa->user),
						   msn_user_get_name(pa->user));

				do_ask_dialog(msg, NULL, pa,
							  _("Authorize"), msn_accept_add_cb,
							  _("Deny"), msn_cancel_add_cb,
							  session->prpl->handle, FALSE);
			}
		}

		if (user_num != num_users)
			return TRUE; /* This isn't the last one in the RL. */

		if (!msn_servconn_send_command(servconn, "CHG", "NLN")) {
			hide_login_progress(gc, _("Unable to write"));
			signoff(gc);

			return FALSE;
		}

		account_online(gc);
		serv_finish_login(gc);

		session->lists.allow = g_slist_copy(gc->account->permit);
		session->lists.block = g_slist_copy(gc->account->deny);

		while (session->lists.forward != NULL) {
			MsnUser *user = session->lists.forward->data;
			struct buddy *b;

			b = gaim_find_buddy(gc->account, msn_user_get_passport(user));

			session->lists.forward = g_slist_remove(session->lists.forward,
													user);

			if (b == NULL) {
				struct group *g = NULL;
				const char *group_name = NULL;
				int group_id;

				group_id = msn_user_get_group_id(user);

				if (group_id > -1) {
					group_name = g_hash_table_lookup(session->group_names,
													 &group_id);
				}

				if (group_name == NULL) {
					gaim_debug(GAIM_DEBUG_WARNING, "msn",
							   "Group ID %d for user %s was not defined.\n",
							   group_id, passport);
				}
				else if ((g = gaim_find_group(group_name)) == NULL) {
					gaim_debug(GAIM_DEBUG_ERROR, "msn",
							   "Group '%s' appears in server-side "
							   "buddy list, but not here!",
							   group_name);
				}

				if (g == NULL) {
					if ((g = gaim_find_group(_("Buddies"))) == NULL) {
						g = gaim_group_new(_("Buddies"));
						gaim_blist_add_group(g, NULL);
					}
				}

				b = gaim_buddy_new(gc->account,
								   msn_user_get_passport(user), NULL);

				gaim_blist_add_buddy(b, g, NULL);
			}

			b->proto_data = user;

			serv_got_alias(gc, (char *)msn_user_get_passport(user),
						   (char *)msn_user_get_name(user));
		}
	}

	return TRUE;
}

static gboolean
__nln_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	const char *state;
	const char *passport;
	const char *friend;
	int status = 0;

	state    = params[0];
	passport = params[1];
	friend   = msn_url_decode(params[2]);

	serv_got_alias(gc, (char *)passport, (char *)friend);

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

	return TRUE;
}

static gboolean
__prp_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	const char *type, *value;

	type  = params[2];
	value = params[3];

	if (param_count == 4) {
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, msn_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, msn_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, msn_url_decode(value));
	}

	return TRUE;
}

static gboolean
__rea_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	char *friend;

	friend = msn_url_decode(params[2]);

	g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", friend);

	return TRUE;
}

static gboolean
__reg_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	gint *group_id;
	char *group_name;

	group_id = g_new(gint, 1);
	*group_id = atoi(params[2]);

	group_name = msn_url_decode(params[3]);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Renamed group %s to %s\n",
			   g_hash_table_lookup(session->group_names, group_id),
			   group_name);

	g_hash_table_replace(session->group_names, group_id, g_strdup(group_name));

	g_hash_table_remove(session->group_ids, group_name);
	g_hash_table_insert(session->group_ids, group_name, group_id);

	return TRUE;
}

static gboolean
__rem_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;

	/* I hate this. */
	if (session->moving_buddy) {
		struct gaim_connection *gc = session->account->gc;
		const char *passport = params[3];
		char outparams[MSN_BUF_LEN];
		int *group_id;

		group_id = g_hash_table_lookup(session->group_ids,
									   session->dest_group_name);

		g_free(session->dest_group_name);
		session->dest_group_name = NULL;
		session->moving_buddy = FALSE;

		if (group_id == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Still don't have a group ID for %s while moving %s!\n",
					   session->dest_group_name, passport);
			return TRUE;
		}

		g_snprintf(outparams, sizeof(outparams), "FL %s %s %d",
				   passport, passport, *group_id);

		if (!msn_servconn_send_command(session->notification_conn,
									   "ADD", outparams)) {
			hide_login_progress(gc, _("Write error"));
			signoff(gc);
		}
	}

	return TRUE;
}

/**************************************************************************
 * Misc commands
 **************************************************************************/
static gboolean
__url_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	const char *rru;
	const char *url;
	md5_state_t st;
	md5_byte_t di[16];
	FILE *fd;
	char buf[2048];
	char buf2[3];
	char sendbuf[64];
	int i;

	rru = params[1];
	url = params[2];

	g_snprintf(buf, sizeof(buf), "%s%lu%s",
			   session->passport_info.mspauth,
			   time(NULL) - session->passport_info.sl, gc->password);

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)buf, strlen(buf));
	md5_finish(&st, di);

	memset(sendbuf, 0, sizeof(sendbuf));

	for (i = 0; i < 16; i++) {
		g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
		strcat(sendbuf, buf2);
	}

	if (session->passport_info.file != NULL) {
		unlink(session->passport_info.file);
		g_free(session->passport_info.file);
	}

	if ((fd = gaim_mkstemp(&session->passport_info.file)) == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Error opening temp passport file: %s\n",
				   strerror(errno));
	}
	else {
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
				gc->username);
		fprintf(fd, "<input type=\"hidden\" name=\"username\" value=\"%s\">\n",
				gc->username);
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
		fprintf(fd, "<input type=\"hiden\" name=\"js\" value=\"yes\">\n");
		fprintf(fd, "</form></body>\n");
		fprintf(fd, "</html>\n");

		if (fclose(fd)) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Error closing temp passport file: %s\n",
					   strerror(errno));

			unlink(session->passport_info.file);
			g_free(session->passport_info.file);
		}
		else {
			/*
			 * Renaming file with .html extension, so that the
			 * win32 open_url will work.
			 */
			char *tmp;

			if ((tmp = g_strdup_printf("%s.html",
					session->passport_info.file)) != NULL) {

				if (rename(session->passport_info.file, tmp) == 0) {
					g_free(session->passport_info.file);
					session->passport_info.file = tmp;
				}
				else
					g_free(tmp);
			}
		}
	}

	return TRUE;
}
/**************************************************************************
 * Switchboards
 **************************************************************************/
static gboolean
__rng_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	MsnSwitchBoard *swboard;
	MsnUser *user;
	const char *session_id;
	char *host, *c;
	int port;

	session_id = params[0];

	host = g_strdup(params[1]);

	if ((c = strchr(host, ':')) != NULL) {
		*c = '\0';
		port = atoi(c + 1);
	}
	else
		port = 1863;

	swboard = msn_switchboard_new(session);

	user = msn_user_new(session, params[4], NULL);

	msn_switchboard_set_invited(swboard, TRUE);
	msn_switchboard_set_session_id(swboard, params[0]);
	msn_switchboard_set_auth_key(swboard, params[3]);
	msn_switchboard_set_user(swboard, user);

	if (!msn_switchboard_connect(swboard, host, port)) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Unable to connect to switchboard on %s, port %d\n",
				   host, port);

		g_free(host);

		return FALSE;
	}

	g_free(host);

	return TRUE;
}

static gboolean
__xfr_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	MsnSwitchBoard *swboard;
	struct gaim_connection *gc = session->account->gc;
	char *host;
	char *c;
	int port;

	if (strcmp(params[1], "SB") && strcmp(params[1], "NS")) {
		hide_login_progress(gc, _("Got invalid XFR"));
		signoff(gc);
		
		return FALSE;
	}

	host = g_strdup(params[2]);

	if ((c = strchr(host, ':')) != NULL) {
		*c = '\0';
		port = atoi(c + 1);
	}
	else
		port = 1863;

	if (!strcmp(params[1], "SB")) {
		swboard = msn_session_find_unused_switch(session);

		if (swboard == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Received an XFR SB request when there's no unused "
					   "switchboards!\n");
			return FALSE;
		}

		msn_switchboard_set_auth_key(swboard, params[4]);

		if (!msn_switchboard_connect(swboard, host, port)) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Unable to connect to switchboard on %s, port %d\n",
					   host, port);

			g_free(host);

			return FALSE;
		}
	}
	else if (!strcmp(params[1], "NS")) {
		msn_servconn_destroy(session->notification_conn);

		session->notification_conn = msn_notification_new(session, host, port);

		if (!msn_servconn_connect(session->notification_conn)) {
			hide_login_progress(gc, _("Unable to transfer to "
									  "notification server"));
			signoff(gc);

			return FALSE;
		}
	}

	g_free(host);

	return TRUE;
}

/**************************************************************************
 * Message Types
 **************************************************************************/
static gboolean
__profile_msg(MsnServConn *servconn, const MsnMessage *msg)
{
	MsnSession *session = servconn->session;
	const char *value;

	if (strcmp(servconn->msg_passport, "Hotmail")) {
		/* This isn't an official message. */
		return TRUE;
	}

	if ((value = msn_message_get_attr(msg, "kv")) != NULL)
		session->passport_info.kv = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "sid")) != NULL)
		session->passport_info.sid = g_strdup(value);

	if ((value = msn_message_get_attr(msg, "MSPAuth")) != NULL)
		session->passport_info.mspauth = g_strdup(value);

	return TRUE;
}

static gboolean
__initial_email_msg(MsnServConn *servconn, const MsnMessage *msg)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	GHashTable *table;
	const char *unread;

	if (strcmp(servconn->msg_passport, "Hotmail")) {
		/* This isn't an official message. */
		return TRUE;
	}

	if (session->passport_info.file == NULL) {
		msn_servconn_send_command(servconn, "URL", "INBOX");
		return TRUE;
	}

	table = msn_message_get_hashtable_from_body(msg);

	unread = g_hash_table_lookup(table, "Inbox-Unread");

	if (unread != NULL)
		connection_has_mail(gc, atoi(unread), NULL, NULL,
							session->passport_info.file);

	g_hash_table_destroy(table);

	return TRUE;
}

static gboolean
__email_msg(MsnServConn *servconn, const MsnMessage *msg)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = session->account->gc;
	GHashTable *table;
	const char *from, *subject;

	if (strcmp(servconn->msg_passport, "Hotmail")) {
		/* This isn't an official message. */
		return TRUE;
	}

	if (session->passport_info.file == NULL) {
		msn_servconn_send_command(servconn, "URL", "INBOX");
		return TRUE;
	}

	table = msn_message_get_hashtable_from_body(msg);

	from    = g_hash_table_lookup(table, "From");
	subject = g_hash_table_lookup(table, "Subject");

	if (from == NULL || subject == NULL) {
		connection_has_mail(gc, 1, NULL, NULL, session->passport_info.file);
	}
	else {
		connection_has_mail(gc, -1, from, subject,
							session->passport_info.file);
	}

	g_hash_table_destroy(table);

	return TRUE;
}

static gboolean
__system_msg(MsnServConn *servconn, const MsnMessage *msg)
{
	GHashTable *table;
	const char *type_s;

	if (strcmp(servconn->msg_passport, "Hotmail")) {
		/* This isn't an official message. */
		return TRUE;
	}

	table = msn_message_get_hashtable_from_body(msg);

	if ((type_s = g_hash_table_lookup(table, "Type")) != NULL) {
		int type = atoi(type_s);
		char buf[MSN_BUF_LEN];

		switch (type) {
			case 1:
				g_snprintf(buf, sizeof(buf),
						   _("The MSN server will shut down for maintenance "
							 "in %d minute(s). You will automatically be "
							 "signed out at that time. Please finish any "
						     "conversations in progress.\n\n"
							 "After the maintenance has been completed, you "
							 "will be able to successfully sign in."),
						   atoi(g_hash_table_lookup(table, "Arg1")));

			default:
				break;
		}

		if (*buf != '\0') {
			do_error_dialog(buf, NULL, GAIM_INFO);
		}
	}

	g_hash_table_destroy(table);

	return TRUE;
}
static gboolean
__connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *notification = data;
	MsnSession *session = notification->session;
	struct gaim_connection *gc = session->account->gc;

	if (source == -1) {
		hide_login_progress(session->account->gc, _("Unable to connect"));
		signoff(session->account->gc);
		return FALSE;
	}

	if (notification->fd != source)
		notification->fd = source;

	if (!msn_servconn_send_command(notification, "VER",
								   "MSNP7 MSNP6 MSNP5 MSNP4 CVR0")) {
		hide_login_progress(gc, _("Unable to write to server"));
		signoff(gc);
		return FALSE;
	}

	session->user = msn_user_new(session, gc->username, NULL);

	set_login_progress(session->account->gc, 4, _("Syncing with server"));

	return TRUE;
}

static void
__failed_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *notification = data;
	struct gaim_connection *gc;

	gc = notification->session->account->gc;

	hide_login_progress(gc, _("Error reading from server"));
	signoff(gc);
}

MsnServConn *
msn_notification_new(MsnSession *session, const char *server, int port)
{
	MsnServConn *notification;

	notification = msn_servconn_new(session);

	msn_servconn_set_server(notification, server, port);
	msn_servconn_set_connect_cb(notification, __connect_cb);
	msn_servconn_set_failed_read_cb(notification, __failed_read_cb);

	if (notification_commands == NULL) {
		/* Register the command callbacks. */
		msn_servconn_register_command(notification, "ADD",       __add_cmd);
		msn_servconn_register_command(notification, "ADG",       __adg_cmd);
		msn_servconn_register_command(notification, "BLP",       __blp_cmd);
		msn_servconn_register_command(notification, "BPR",       __bpr_cmd);
		msn_servconn_register_command(notification, "CHG",       __blank_cmd);
		msn_servconn_register_command(notification, "CHL",       __chl_cmd);
		msn_servconn_register_command(notification, "FLN",       __fln_cmd);
		msn_servconn_register_command(notification, "GTC",       __blank_cmd);
		msn_servconn_register_command(notification, "ILN",       __iln_cmd);
		msn_servconn_register_command(notification, "INF",       __inf_cmd);
		msn_servconn_register_command(notification, "LSG",       __lsg_cmd);
		msn_servconn_register_command(notification, "LST",       __lst_cmd);
		msn_servconn_register_command(notification, "MSG",       __msg_cmd);
		msn_servconn_register_command(notification, "NLN",       __nln_cmd);
		msn_servconn_register_command(notification, "OUT",       __out_cmd);
		msn_servconn_register_command(notification, "PRP",       __prp_cmd);
		msn_servconn_register_command(notification, "QNG",       __blank_cmd);
		msn_servconn_register_command(notification, "QRY",       __blank_cmd);
		msn_servconn_register_command(notification, "REA",       __rea_cmd);
		msn_servconn_register_command(notification, "REG",       __reg_cmd);
		msn_servconn_register_command(notification, "REM",       __rem_cmd);
		msn_servconn_register_command(notification, "RMG",       __blank_cmd);
		msn_servconn_register_command(notification, "RNG",       __rng_cmd);
		msn_servconn_register_command(notification, "SYN",       __blank_cmd);
		msn_servconn_register_command(notification, "URL",       __url_cmd);
		msn_servconn_register_command(notification, "USR",       __usr_cmd);
		msn_servconn_register_command(notification, "VER",       __ver_cmd);
		msn_servconn_register_command(notification, "XFR",       __xfr_cmd);
		msn_servconn_register_command(notification, "_unknown_", __unknown_cmd);

		/* Register the message type callbacks. */
		msn_servconn_register_msg_type(notification, "text/x-msmsgsprofile",
									   __profile_msg);
		msn_servconn_register_msg_type(notification,
									   "text/x-msmsgsinitialemailnotification",
									   __initial_email_msg);
		msn_servconn_register_msg_type(notification,
									   "text/x-msmsgsemailnotification",
									   __email_msg);
		msn_servconn_register_msg_type(notification,
									   "application/x-msmsgssystemmessage",
									   __system_msg);

		/* Save these for future use. */
		notification_commands  = notification->commands;
		notification_msg_types = notification->msg_types;
	}
	else {
		g_hash_table_destroy(notification->commands);
		g_hash_table_destroy(notification->msg_types);

		notification->commands  = notification_commands;
		notification->msg_types = notification_msg_types;
	}

	return notification;
}
