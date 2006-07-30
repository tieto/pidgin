/**
 * @file session.c MSN session functions
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
#include "session.h"
#include "notification.h"

#include "dialog.h"

MsnSession *
msn_session_new(GaimAccount *account)
{
	MsnSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(MsnSession, 1);

	session->account = account;
	session->notification = msn_notification_new(session);
	session->userlist = msn_userlist_new(session);

	session->user = msn_user_new(session->userlist,
								 gaim_account_get_username(account), NULL);

	/*if you want to chat with Yahoo Messenger*/
	//session->protocol_ver = WLM_YAHOO_PROT_VER;
	session->protocol_ver = WLM_PROT_VER;
	session->conv_seq = 1;

	return session;
}

void
msn_session_destroy(MsnSession *session)
{
	g_return_if_fail(session != NULL);

	session->destroying = TRUE;

	if (session->connected)
		msn_session_disconnect(session);

	if (session->notification != NULL)
		msn_notification_destroy(session->notification);

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	while (session->slplinks != NULL)
		msn_slplink_destroy(session->slplinks->data);

	msn_userlist_destroy(session->userlist);

	g_free(session->passport_info.t);
	g_free(session->passport_info.p);
	g_free(session->passport_info.kv);
	g_free(session->passport_info.sid);
	g_free(session->passport_info.mspauth);
	g_free(session->passport_info.client_ip);

	if (session->passport_info.file != NULL)
	{
		g_unlink(session->passport_info.file);
		g_free(session->passport_info.file);
	}

	if (session->sync != NULL)
		msn_sync_destroy(session->sync);

	if (session->nexus != NULL)
		msn_nexus_destroy(session->nexus);

	if (session->contact != NULL)
		msn_contact_destroy(session->contact);
	if (session->oim != NULL)
		msn_oim_destroy(session->oim);

	if (session->user != NULL)
		msn_user_destroy(session->user);

	g_free(session);
}

gboolean
msn_session_connect(MsnSession *session, const char *host, int port,
					gboolean http_method)
{
	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(!session->connected, TRUE);

	session->connected = TRUE;
	session->http_method = http_method;

	if (session->notification == NULL)
	{
		gaim_debug_error("msn", "This shouldn't happen\n");
		g_return_val_if_reached(FALSE);
	}

	if (msn_notification_connect(session->notification, host, port))
	{
		return TRUE;
	}

	return FALSE;
}

void
msn_session_disconnect(MsnSession *session)
{
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->connected);

	session->connected = FALSE;

	while (session->switches != NULL)
		msn_switchboard_close(session->switches->data);

	if (session->notification != NULL)
		msn_notification_close(session->notification);
}

/* TODO: This must go away when conversation is redesigned */
MsnSwitchBoard *
msn_session_find_swboard(MsnSession *session, const char *username)
{
	GList *l;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if ((swboard->im_user != NULL) && !strcmp(username, swboard->im_user))
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_swboard_with_conv(MsnSession *session, GaimConversation *conv)
{
	GList *l;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->conv == conv)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_swboard_with_id(const MsnSession *session, int chat_id)
{
	GList *l;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(chat_id >= 0,    NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->chat_id == chat_id)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_get_swboard(MsnSession *session, const char *username,
						MsnSBFlag flag)
{
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);

	swboard = msn_session_find_swboard(session, username);

	if (swboard == NULL)
	{
		swboard = msn_switchboard_new(session);
		swboard->im_user = g_strdup(username);
		msn_switchboard_request(swboard);
		msn_switchboard_request_add_user(swboard, username);
	}

	swboard->flag |= flag;

	return swboard;
}

static void
msn_session_sync_users(MsnSession *session)
{
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimConnection *gc = gaim_account_get_connection(session->account);

	g_return_if_fail(gc != NULL);

	/* The core used to use msn_add_buddy to add all buddies before
	 * being logged in. This no longer happens, so we manually iterate
	 * over the whole buddy list to identify sync issues. */
	for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next){
		GaimGroup *group = (GaimGroup *)gnode;
		const char *group_name = group->name;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		if(!g_strcasecmp(group_name, MSN_INDIVIDUALS_GROUP_NAME)
						||	!g_strcasecmp(group_name,MSN_NON_IM_GROUP_NAME)){
			continue;
		}
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b;
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				b = (GaimBuddy *)bnode;
				if(b->account == gc->account){
					MsnUser *remote_user;
					gboolean found = FALSE;

					gaim_debug_info("MaYuan","buddy name:%s,group name:%s\n",b->name,group_name);
					remote_user = msn_userlist_find_user(session->userlist, b->name);

					if ((remote_user != NULL) && (remote_user->list_op & MSN_LIST_FL_OP)){
						const char *group_id;
						GList *l;

						group_id = msn_userlist_find_group_id(remote_user->userlist,
								group_name);
						if(group_id == NULL){
							continue;
						}

						for (l = remote_user->group_ids; l != NULL; l = l->next){
							if (!g_strcasecmp(group_id ,l->data)){
								found = TRUE;
								break;
							}
						}
					}

					if (!found){
						/* The user was not on the server list or not in that group
						 * on the server list */
						msn_show_sync_issue(session, b->name, group_name);
					}
				}
			}
		}
	}
}

void
msn_session_set_error(MsnSession *session, MsnErrorType error,
					  const char *info)
{
	GaimConnection *gc;
	char *msg;

	gc = gaim_account_get_connection(session->account);

	switch (error)
	{
		case MSN_ERROR_SERVCONN:
			msg = g_strdup(info);
			break;
		case MSN_ERROR_UNSUPPORTED_PROTOCOL:
			msg = g_strdup(_("Our protocol is not supported by the "
							 "server."));
			break;
		case MSN_ERROR_HTTP_MALFORMED:
			msg = g_strdup(_("Error parsing HTTP."));
			break;
		case MSN_ERROR_SIGN_OTHER:
			gc->wants_to_die = TRUE;
			msg = g_strdup(_("You have signed on from another location."));
			break;
		case MSN_ERROR_SERV_UNAVAILABLE:
			msg = g_strdup(_("The MSN servers are temporarily "
							 "unavailable. Please wait and try "
							 "again."));
			break;
		case MSN_ERROR_SERV_DOWN:
			msg = g_strdup(_("The MSN servers are going down "
							 "temporarily."));
			break;
		case MSN_ERROR_AUTH:
			msg = g_strdup_printf(_("Unable to authenticate: %s"),
								  (info == NULL ) ?
								  _("Unknown error") : info);
			break;
		case MSN_ERROR_BAD_BLIST:
			msg = g_strdup(_("Your MSN buddy list is temporarily "
							 "unavailable. Please wait and try "
							 "again."));
			break;
		default:
			msg = g_strdup(_("Unknown error."));
			break;
	}

	msn_session_disconnect(session);

	gaim_connection_error(gc, msg);

	g_free(msg);
}

static const char *
get_login_step_text(MsnSession *session)
{
	const char *steps_text[] = {
		_("Connecting"),
		_("Handshaking"),
		_("Transferring"),
		_("Handshaking"),
		_("Starting authentication"),
		_("Getting cookie"),
		_("Authenticating"),
		_("Sending cookie"),
		_("Retrieving buddy list")
	};

	return steps_text[session->login_step];
}

void
msn_session_set_login_step(MsnSession *session, MsnLoginStep step)
{
	GaimConnection *gc;

	/* Prevent the connection progress going backwards, eg. if we get
	 * transferred several times during login */
	if (session->login_step > step)
		return;

	/* If we're already logged in, we're probably here because of a
	 * mid-session XFR from the notification server, so we don't want to
	 * popup the connection progress dialog */
	if (session->logged_in)
		return;

	gc = session->account->gc;

	session->login_step = step;

	gaim_connection_update_progress(gc, get_login_step_text(session), step,
									MSN_LOGIN_STEPS);
}

void
msn_session_finish_login(MsnSession *session)
{
	GaimAccount *account;
	GaimConnection *gc;
	char *icon;

	if (session->logged_in)
		return;

	account = session->account;
	gc = gaim_account_get_connection(account);

	icon = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(session->account));
	msn_user_set_buddy_icon(session->user, icon);
	g_free(icon);

	session->logged_in = TRUE;

	msn_change_status(session);

	gaim_connection_set_state(gc, GAIM_CONNECTED);

	/* Sync users */
	msn_session_sync_users(session);
}

