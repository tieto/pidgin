/**
 * @file user.c User functions
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "msn.h"
#include "user.h"
#include "slp.h"

static void free_user_endpoint(MsnUserEndpoint *data)
{
	g_free(data->name);
	g_free(data);
}

/*new a user object*/
MsnUser *
msn_user_new(MsnUserList *userlist, const char *passport,
			 const char *friendly_name)
{
	MsnUser *user;

	user = g_new0(MsnUser, 1);

	user->userlist = userlist;

	msn_user_set_passport(user, passport);
	msn_user_set_friendly_name(user, friendly_name);

	return user;
}

/*destroy a user object*/
void
msn_user_destroy(MsnUser *user)
{
	g_return_if_fail(user != NULL);

	while (user->endpoints != NULL) {
		free_user_endpoint(user->endpoints->data);
		user->endpoints = g_slist_delete_link(user->endpoints, user->endpoints);
	}

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	if (user->group_ids != NULL)
	{
		GList *l;
		for (l = user->group_ids; l != NULL; l = l->next)
		{
			g_free(l->data);
		}
		g_list_free(user->group_ids);
	}

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	g_free(user->passport);
	g_free(user->friendly_name);
	g_free(user->uid);
	if (user->extinfo) {
		g_free(user->extinfo->media_album);
		g_free(user->extinfo->media_artist);
		g_free(user->extinfo->media_title);
		g_free(user->extinfo->phone_home);
		g_free(user->extinfo->phone_mobile);
		g_free(user->extinfo->phone_work);
		g_free(user->extinfo);
	}
	g_free(user->statusline);
	g_free(user->invite_message);

	g_free(user);
}

void
msn_user_update(MsnUser *user)
{
	PurpleAccount *account;
	gboolean offline;

	g_return_if_fail(user != NULL);

	account = user->userlist->session->account;

	offline = (user->status == NULL);

	if (!offline) {
		purple_prpl_got_user_status(account, user->passport, user->status,
				"message", user->statusline, NULL);
	} else {
		if (user->mobile) {
			purple_prpl_got_user_status(account, user->passport, "mobile", NULL);
			purple_prpl_got_user_status(account, user->passport, "available", NULL);
		} else {
			purple_prpl_got_user_status(account, user->passport, "offline", NULL);
		}
	}

	if (!offline || !user->mobile) {
		purple_prpl_got_user_status_deactive(account, user->passport, "mobile");
	}

	if (!offline && user->extinfo && user->extinfo->media_type != CURRENT_MEDIA_UNKNOWN) {
		if (user->extinfo->media_type == CURRENT_MEDIA_MUSIC) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            PURPLE_TUNE_ARTIST, user->extinfo->media_artist,
			                            PURPLE_TUNE_ALBUM, user->extinfo->media_album,
			                            PURPLE_TUNE_TITLE, user->extinfo->media_title,
			                            NULL);
		} else if (user->extinfo->media_type == CURRENT_MEDIA_GAMES) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            "game", user->extinfo->media_title,
			                            NULL);
		} else if (user->extinfo->media_type == CURRENT_MEDIA_OFFICE) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            "office", user->extinfo->media_title,
			                            NULL);
		} else {
			purple_debug_warning("msn", "Got CurrentMedia with unknown type %d.\n",
			                     user->extinfo->media_type);
		}
	} else {
		purple_prpl_got_user_status_deactive(account, user->passport, "tune");
	}

	if (user->idle)
		purple_prpl_got_user_idle(account, user->passport, TRUE, -1);
	else
		purple_prpl_got_user_idle(account, user->passport, FALSE, 0);
}

void
msn_user_set_state(MsnUser *user, const char *state)
{
	const char *status;

	g_return_if_fail(user != NULL);

	if (state == NULL) {
		user->status = NULL;
		return;
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

	if (!g_ascii_strcasecmp(state, "IDL"))
		user->idle = TRUE;
	else
		user->idle = FALSE;

	user->status = status;
}

void
msn_user_set_passport(MsnUser *user, const char *passport)
{
	g_return_if_fail(user != NULL);

	g_free(user->passport);
	user->passport = g_strdup(passport);
}

gboolean
msn_user_set_friendly_name(MsnUser *user, const char *name)
{
	g_return_val_if_fail(user != NULL, FALSE);

	if (user == user->userlist->session->user)
		return FALSE;

	if (user->friendly_name && name && (!strcmp(user->friendly_name, name) ||
				!strcmp(user->passport, name)))
		return FALSE;

	g_free(user->friendly_name);
	user->friendly_name = g_strdup(name);

	serv_got_alias(purple_account_get_connection(user->userlist->session->account),
			user->passport, name);
	return TRUE;
}

void
msn_user_set_statusline(MsnUser *user, const char *statusline)
{
	g_return_if_fail(user != NULL);

	g_free(user->statusline);
	user->statusline = g_strdup(statusline);
}

void
msn_user_set_uid(MsnUser *user, const char *uid)
{
	g_return_if_fail(user != NULL);

	g_free(user->uid);
	user->uid = g_strdup(uid);
}

void
msn_user_set_endpoint_data(MsnUser *user, const char *input, MsnUserEndpoint *newep)
{
	MsnUserEndpoint *ep;
	char *endpoint;
	GSList *l;

	g_return_if_fail(user != NULL);
	g_return_if_fail(input != NULL);

	endpoint = g_ascii_strdown(input, -1);

	for (l = user->endpoints; l; l = l->next) {
		ep = l->data;
		if (g_str_equal(ep->id, endpoint)) {
			/* We have info about this endpoint! */

			g_free(endpoint);

			if (newep == NULL) {
				/* Delete it and exit */
				user->endpoints = g_slist_delete_link(user->endpoints, l);
				free_user_endpoint(ep);
				return;
			}

			/* Break out of our loop and update it */
			break;
		}
	}
	if (l == NULL) {
		/* Need to add a new endpoint */
		ep = g_new0(MsnUserEndpoint, 1);
		ep->id = endpoint;
		user->endpoints = g_slist_prepend(user->endpoints, ep);
	}

	ep->clientid = newep->clientid;
	ep->extcaps = newep->extcaps;
}

void
msn_user_set_op(MsnUser *user, MsnListOp list_op)
{
	g_return_if_fail(user != NULL);

	user->list_op |= list_op;
}

void
msn_user_unset_op(MsnUser *user, MsnListOp list_op)
{
	g_return_if_fail(user != NULL);

	user->list_op &= ~list_op;
}

void
msn_user_set_buddy_icon(MsnUser *user, PurpleStoredImage *img)
{
	MsnObject *msnobj;

	g_return_if_fail(user != NULL);

	msnobj = msn_object_new_from_image(img, "TFR2C2.tmp",
			user->passport, MSN_OBJECT_USERTILE);

	if (!msnobj)
		purple_debug_error("msn", "Unable to open buddy icon from %s!\n", user->passport);

	msn_user_set_object(user, msnobj);
}

/*add group id to User object*/
void
msn_user_add_group_id(MsnUser *user, const char* group_id)
{
	MsnUserList *userlist;
	PurpleAccount *account;
	PurpleBuddy *b;
	PurpleGroup *g;
	const char *passport;
	const char *group_name;

	g_return_if_fail(user != NULL);
	g_return_if_fail(group_id != NULL);

	user->group_ids = g_list_append(user->group_ids, g_strdup(group_id));

	userlist = user->userlist;
	account = userlist->session->account;
	passport = msn_user_get_passport(user);

	group_name = msn_userlist_find_group_name(userlist, group_id);

	purple_debug_info("msn", "User: group id:%s,name:%s,user:%s\n", group_id, group_name, passport);

	g = purple_find_group(group_name);

	if ((group_id == NULL) && (g == NULL))
	{
		g = purple_group_new(group_name);
		purple_blist_add_group(g, NULL);
	}

	b = purple_find_buddy_in_group(account, passport, g);
	if (b == NULL)
	{
		b = purple_buddy_new(account, passport, NULL);
		purple_blist_add_buddy(b, NULL, g, NULL);
	}
	purple_buddy_set_protocol_data(b, user);
	/*Update the blist Node info*/
}

/*check if the msn user is online*/
gboolean
msn_user_is_online(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;

	buddy = purple_find_buddy(account, name);
	return PURPLE_BUDDY_IS_ONLINE(buddy);
}

gboolean
msn_user_is_yahoo(PurpleAccount *account, const char *name)
{
	MsnSession *session = NULL;
	MsnUser *user;
	PurpleConnection *gc;

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		session = gc->proto_data;

	if ((session != NULL) && (user = msn_userlist_find_user(session->userlist, name)) != NULL)
	{
		return (user->networkid == MSN_NETWORK_YAHOO);
	}
	return (strstr(name,"@yahoo.") != NULL);
}

void
msn_user_remove_group_id(MsnUser *user, const char *id)
{
	GList *l;

	g_return_if_fail(user != NULL);
	g_return_if_fail(id != NULL);

	l = g_list_find_custom(user->group_ids, id, (GCompareFunc)strcmp);

	if (l == NULL)
		return;

	g_free(l->data);
	user->group_ids = g_list_delete_link(user->group_ids, l);
}

void
msn_user_set_pending_group(MsnUser *user, const char *group)
{
	user->pending_group = g_strdup(group);
}

char *
msn_user_remove_pending_group(MsnUser *user)
{
	char *group = user->pending_group;
	user->pending_group = NULL;
	return group;
}

void
msn_user_set_home_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (!number && !user->extinfo)
		return;

	if (user->extinfo)
		g_free(user->extinfo->phone_home);
	else
		user->extinfo = g_new0(MsnUserExtendedInfo, 1);

	user->extinfo->phone_home = g_strdup(number);
}

void
msn_user_set_work_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (!number && !user->extinfo)
		return;

	if (user->extinfo)
		g_free(user->extinfo->phone_work);
	else
		user->extinfo = g_new0(MsnUserExtendedInfo, 1);

	user->extinfo->phone_work = g_strdup(number);
}

void
msn_user_set_mobile_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (!number && !user->extinfo)
		return;

	if (user->extinfo)
		g_free(user->extinfo->phone_mobile);
	else
		user->extinfo = g_new0(MsnUserExtendedInfo, 1);

	user->extinfo->phone_mobile = g_strdup(number);
}

void
msn_user_set_clientid(MsnUser *user, guint clientid)
{
	g_return_if_fail(user != NULL);

	user->clientid = clientid;
}

void
msn_user_set_extcaps(MsnUser *user, guint extcaps)
{
	g_return_if_fail(user != NULL);

	user->extcaps = extcaps;
}

void
msn_user_set_network(MsnUser *user, MsnNetwork network)
{
	g_return_if_fail(user != NULL);

	user->networkid = network;
}

void
msn_user_set_object(MsnUser *user, MsnObject *obj)
{
	g_return_if_fail(user != NULL);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	user->msnobj = obj;

	if (user != user->userlist->session->user && user->list_op & MSN_LIST_FL_OP)
		msn_queue_buddy_icon_request(user);
}

void
msn_user_set_client_caps(MsnUser *user, GHashTable *info)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(info != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	user->clientcaps = info;
}

void
msn_user_set_invite_message(MsnUser *user, const char *message)
{
	g_return_if_fail(user != NULL);

	g_free(user->invite_message);
	user->invite_message = g_strdup(message);
}

const char *
msn_user_get_passport(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->passport;
}

const char *
msn_user_get_friendly_name(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->friendly_name;
}

const char *
msn_user_get_home_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->extinfo ? user->extinfo->phone_home : NULL;
}

const char *
msn_user_get_work_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->extinfo ? user->extinfo->phone_work : NULL;
}

const char *
msn_user_get_mobile_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->extinfo ? user->extinfo->phone_mobile : NULL;
}

guint
msn_user_get_clientid(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, 0);

	return user->clientid;
}

guint
msn_user_get_extcaps(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, 0);

	return user->extcaps;
}

MsnUserEndpoint *
msn_user_get_endpoint_data(MsnUser *user, const char *input)
{
	char *endpoint;
	GSList *l;
	MsnUserEndpoint *ep;

	g_return_val_if_fail(user != NULL, NULL);
	g_return_val_if_fail(input != NULL, NULL);

	endpoint = g_ascii_strdown(input, -1);

	for (l = user->endpoints; l; l = l->next) {
		ep = l->data;
		if (g_str_equal(ep->id, endpoint)) {
			g_free(endpoint);
			return ep;
		}
	}

	g_free(endpoint);

	return NULL;
}

MsnObject *
msn_user_get_object(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->msnobj;
}

GHashTable *
msn_user_get_client_caps(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->clientcaps;
}

const char *
msn_user_get_invite_message(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->invite_message;
}

gboolean
msn_user_is_capable(MsnUser *user, char *endpoint, guint capability, guint extcap)
{
	g_return_val_if_fail(user != NULL, FALSE);

	if (endpoint != NULL) {
		MsnUserEndpoint *ep = msn_user_get_endpoint_data(user, endpoint);
		if (ep != NULL)
			return (ep->clientid & capability) && (ep->extcaps & extcap);
		else
			return FALSE;
	}

	return (user->clientid & capability) && (user->extcaps & extcap);
}
