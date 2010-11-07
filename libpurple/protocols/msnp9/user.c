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

MsnUser *
msn_user_new(MsnUserList *userlist, const char *passport,
			 const char *friendly_name)
{
	MsnUser *user;

	user = g_new0(MsnUser, 1);

	user->userlist = userlist;

	msn_user_set_passport(user, passport);
	if (friendly_name != NULL)
		msn_user_set_friendly_name(user, friendly_name);

	return user;
}

void
msn_user_destroy(MsnUser *user)
{
	g_return_if_fail(user != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	if (user->group_ids != NULL)
		g_list_free(user->group_ids);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	g_free(user->passport);
	g_free(user->friendly_name);
	g_free(user->phone.home);
	g_free(user->phone.work);
	g_free(user->phone.mobile);

	g_free(user);
}

void
msn_user_update(MsnUser *user)
{
	PurpleAccount *account;

	account = user->userlist->session->account;

	if (user->status != NULL) {
		if (!strcmp(user->status, "offline") && user->mobile) {
			purple_prpl_got_user_status(account, user->passport, "offline", NULL);
			purple_prpl_got_user_status(account, user->passport, "mobile", NULL);
		} else {
			purple_prpl_got_user_status(account, user->passport, user->status, NULL);
			purple_prpl_got_user_status_deactive(account, user->passport, "mobile");
		}
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

void
msn_user_set_friendly_name(MsnUser *user, const char *name)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	const char *encoded;

	g_return_if_fail(user != NULL);

	encoded = purple_url_encode(name);
	session = user->userlist->session;

	if (user->friendly_name && strcmp(user->friendly_name, name)
		&& (strlen(encoded) < 387) && session->passport_info.verified &&
		(user->list_op & MSN_LIST_FL_OP)) {
		/* copy the new name to the server list, but only when new */
		/* should we check this more thoroughly? */
		cmdproc = session->notification->cmdproc;
		msn_cmdproc_send(cmdproc, "REA", "%s %s",
						 user->passport,
						 encoded);
	}

	g_free(user->friendly_name);
	user->friendly_name = g_strdup(name);
}

void
msn_user_set_buddy_icon(MsnUser *user, PurpleStoredImage *img)
{
	MsnObject *msnobj = msn_user_get_object(user);

	g_return_if_fail(user != NULL);

	if (img == NULL)
		msn_user_set_object(user, NULL);
	else
	{
		PurpleCipherContext *ctx;
		char *buf;
		gconstpointer data = purple_imgstore_get_data(img);
		size_t size = purple_imgstore_get_size(img);
		char *base64;
		unsigned char digest[20];

		if (msnobj == NULL)
		{
			msnobj = msn_object_new();
			msn_object_set_local(msnobj);
			msn_object_set_type(msnobj, MSN_OBJECT_USERTILE);
			msn_object_set_location(msnobj, "TFR2C2.tmp");
			msn_object_set_creator(msnobj, msn_user_get_passport(user));

			msn_user_set_object(user, msnobj);
		}

		msn_object_set_image(msnobj, img);

		/* Compute the SHA1D field. */
		memset(digest, 0, sizeof(digest));

		ctx = purple_cipher_context_new_by_name("sha1", NULL);
		purple_cipher_context_append(ctx, data, size);
		purple_cipher_context_digest(ctx, sizeof(digest), digest, NULL);

		base64 = purple_base64_encode(digest, sizeof(digest));
		msn_object_set_sha1d(msnobj, base64);
		g_free(base64);

		msn_object_set_size(msnobj, size);

		/* Compute the SHA1C field. */
		buf = g_strdup_printf(
			"Creator%sSize%dType%dLocation%sFriendly%sSHA1D%s",
			msn_object_get_creator(msnobj),
			msn_object_get_size(msnobj),
			msn_object_get_type(msnobj),
			msn_object_get_location(msnobj),
			msn_object_get_friendly(msnobj),
			msn_object_get_sha1d(msnobj));

		memset(digest, 0, sizeof(digest));

		purple_cipher_context_reset(ctx, NULL);
		purple_cipher_context_append(ctx, (const guchar *)buf, strlen(buf));
		purple_cipher_context_digest(ctx, sizeof(digest), digest, NULL);
		purple_cipher_context_destroy(ctx);
		g_free(buf);

		base64 = purple_base64_encode(digest, sizeof(digest));
		msn_object_set_sha1c(msnobj, base64);
		g_free(base64);
	}
}

void
msn_user_add_group_id(MsnUser *user, int id)
{
	MsnUserList *userlist;
	PurpleAccount *account;
	PurpleBuddy *b;
	PurpleGroup *g;
	const char *passport;
	const char *group_name;

	g_return_if_fail(user != NULL);
	g_return_if_fail(id >= 0);

	user->group_ids = g_list_append(user->group_ids, GINT_TO_POINTER(id));

	userlist = user->userlist;
	account = userlist->session->account;
	passport = msn_user_get_passport(user);

	group_name = msn_userlist_find_group_name(userlist, id);

	g = purple_find_group(group_name);

	if ((id == 0) && (g == NULL))
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

	b->proto_data = user;
}

void
msn_user_remove_group_id(MsnUser *user, int id)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(id >= 0);

	user->group_ids = g_list_remove(user->group_ids, GINT_TO_POINTER(id));
}

void
msn_user_set_home_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.home != NULL)
		g_free(user->phone.home);

	user->phone.home = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_work_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.work != NULL)
		g_free(user->phone.work);

	user->phone.work = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_mobile_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.mobile != NULL)
		g_free(user->phone.mobile);

	user->phone.mobile = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_object(MsnUser *user, MsnObject *obj)
{
	g_return_if_fail(user != NULL);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	user->msnobj = obj;

	if (user->list_op & MSN_LIST_FL_OP)
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

	return user->phone.home;
}

const char *
msn_user_get_work_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.work;
}

const char *
msn_user_get_mobile_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.mobile;
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
