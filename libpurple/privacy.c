/**
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
#include "internal.h"

#include "account.h"
#include "privacy.h"
#include "server.h"
#include "util.h"

static GaimPrivacyUiOps *privacy_ops = NULL;

gboolean
gaim_privacy_permit_add(GaimAccount *account, const char *who,
						gboolean local_only)
{
	GSList *l;
	char *name;
	GaimBuddy *buddy;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = g_strdup(gaim_normalize(account, who));

	for (l = account->permit; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, (char *)l->data))
			break;
	}

	if (l != NULL)
	{
		g_free(name);
		return FALSE;
	}

	account->permit = g_slist_append(account->permit, name);

	if (!local_only && gaim_account_is_connected(account))
		serv_add_permit(gaim_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->permit_added != NULL)
		privacy_ops->permit_added(account, who);

	gaim_blist_schedule_save();

	/* This lets the UI know a buddy has had its privacy setting changed */
	buddy = gaim_find_buddy(account, name);
	if (buddy != NULL) {
		gaim_signal_emit(gaim_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
gaim_privacy_permit_remove(GaimAccount *account, const char *who,
						   gboolean local_only)
{
	GSList *l;
	const char *name;
	GaimBuddy *buddy;
	char *del;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = gaim_normalize(account, who);

	for (l = account->permit; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, (char *)l->data))
			break;
	}

	if (l == NULL)
		return FALSE;

	/* We should not free l->data just yet. There can be occasions where
	 * l->data == who. In such cases, freeing l->data here can cause crashes
	 * later when who is used. */
	del = l->data;
	account->permit = g_slist_delete_link(account->permit, l);

	if (!local_only && gaim_account_is_connected(account))
		serv_rem_permit(gaim_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->permit_removed != NULL)
		privacy_ops->permit_removed(account, who);

	gaim_blist_schedule_save();

	buddy = gaim_find_buddy(account, name);
	if (buddy != NULL) {
		gaim_signal_emit(gaim_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	g_free(del);
	return TRUE;
}

gboolean
gaim_privacy_deny_add(GaimAccount *account, const char *who,
					  gboolean local_only)
{
	GSList *l;
	char *name;
	GaimBuddy *buddy;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = g_strdup(gaim_normalize(account, who));

	for (l = account->deny; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, gaim_normalize(account, (char *)l->data)))
			break;
	}

	if (l != NULL)
	{
		g_free(name);
		return FALSE;
	}

	account->deny = g_slist_append(account->deny, name);

	if (!local_only && gaim_account_is_connected(account))
		serv_add_deny(gaim_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->deny_added != NULL)
		privacy_ops->deny_added(account, who);

	gaim_blist_schedule_save();

	buddy = gaim_find_buddy(account, name);
	if (buddy != NULL) {
		gaim_signal_emit(gaim_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
gaim_privacy_deny_remove(GaimAccount *account, const char *who,
						 gboolean local_only)
{
	GSList *l;
	const char *normalized;
	char *name;
	GaimBuddy *buddy;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	normalized = gaim_normalize(account, who);

	for (l = account->deny; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(normalized, (char *)l->data))
			break;
	}

	buddy = gaim_find_buddy(account, normalized);

	if (l == NULL)
		return FALSE;

	name = l->data;
	account->deny = g_slist_delete_link(account->deny, l);

	if (!local_only && gaim_account_is_connected(account))
		serv_rem_deny(gaim_account_get_connection(account), name);

	if (privacy_ops != NULL && privacy_ops->deny_removed != NULL)
		privacy_ops->deny_removed(account, who);

	if (buddy != NULL) {
		gaim_signal_emit(gaim_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}

	g_free(name);
	gaim_blist_schedule_save();

	return TRUE;
}

/* This makes sure that only all the buddies are in the permit list. */
static void
add_buddies_in_permit(GaimAccount *account, gboolean local)
{
	GSList *list, *iter;
	/* Remove anyone in the permit list who is not in the buddylist */
	for (list = account->permit; list != NULL; ) {
		char *person = list->data;
		list = list->next;
		if (!gaim_find_buddy(account, person))
			gaim_privacy_permit_remove(account, person, local);
	}
	/* Now make sure everyone in the buddylist is in the permit list */
	for (iter = list = gaim_find_buddies(account, NULL); iter; iter = iter->next) {
		GaimBuddy *buddy = iter->data;
		if (!g_slist_find_custom(account->permit, buddy->name, (GCompareFunc)g_utf8_collate))
			gaim_privacy_permit_add(account, buddy->name, local);
	}
	g_slist_free(list);
}

void
gaim_privacy_allow(GaimAccount *account, const char *who, gboolean local,
						gboolean restore)
{
	GSList *list;

	switch (account->perm_deny) {
		case GAIM_PRIVACY_ALLOW_ALL:
			return;
		case GAIM_PRIVACY_ALLOW_USERS:
			gaim_privacy_permit_add(account, who, local);
			break;
		case GAIM_PRIVACY_DENY_USERS:
			gaim_privacy_deny_remove(account, who, local);
			break;
		case GAIM_PRIVACY_DENY_ALL:
			if (!restore) {
				/* Empty the allow-list. */
				for (list = account->permit; list != NULL;) {
					char *who = list->data;
					list = list->next;
					gaim_privacy_permit_remove(account, who, local);
				}
			}
			gaim_privacy_permit_add(account, who, local);
			account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
			break;
		case GAIM_PRIVACY_ALLOW_BUDDYLIST:
			if (!gaim_find_buddy(account, who)) {
				add_buddies_in_permit(account, local);
				gaim_privacy_permit_add(account, who, local);
				account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
			}
			break;
		default:
			g_return_if_reached();
	}
}

void
gaim_privacy_deny(GaimAccount *account, const char *who, gboolean local,
					gboolean restore)
{
	GSList *list;

	switch (account->perm_deny) {
		case GAIM_PRIVACY_ALLOW_ALL:
			if (!restore) {
				/* Empty the deny-list. */
				for (list = account->deny; list != NULL; ) {
					char *person = list->data;
					list = list->next;
					gaim_privacy_deny_remove(account, person, local);
				}
			}
			gaim_privacy_deny_add(account, who, local);
			account->perm_deny = GAIM_PRIVACY_DENY_USERS;
			break;
		case GAIM_PRIVACY_ALLOW_USERS:
			gaim_privacy_permit_remove(account, who, local);
			break;
		case GAIM_PRIVACY_DENY_USERS:
			gaim_privacy_deny_add(account, who, local);
			break;
		case GAIM_PRIVACY_DENY_ALL:
			break;
		case GAIM_PRIVACY_ALLOW_BUDDYLIST:
			if (gaim_find_buddy(account, who)) {
				add_buddies_in_permit(account, local);
				gaim_privacy_permit_remove(account, who, local);
				account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
			}
			break;
		default:
			g_return_if_reached();
	}
}

gboolean
gaim_privacy_check(GaimAccount *account, const char *who)
{
	GSList *list;

	switch (account->perm_deny) {
		case GAIM_PRIVACY_ALLOW_ALL:
			return TRUE;

		case GAIM_PRIVACY_DENY_ALL:
			return FALSE;

		case GAIM_PRIVACY_ALLOW_USERS:
			who = gaim_normalize(account, who);
			for (list=account->permit; list!=NULL; list=list->next) {
				if (!gaim_utf8_strcasecmp(who, (char *)list->data))
					return TRUE;
			}
			return FALSE;

		case GAIM_PRIVACY_DENY_USERS:
			who = gaim_normalize(account, who);
			for (list=account->deny; list!=NULL; list=list->next) {
				if (!gaim_utf8_strcasecmp(who, (char *)list->data ))
					return FALSE;
			}
			return TRUE;

		case GAIM_PRIVACY_ALLOW_BUDDYLIST:
			return (gaim_find_buddy(account, who) != NULL);

		default:
			g_return_val_if_reached(TRUE);
	}
}

void
gaim_privacy_set_ui_ops(GaimPrivacyUiOps *ops)
{
	privacy_ops = ops;
}

GaimPrivacyUiOps *
gaim_privacy_get_ui_ops(void)
{
	return privacy_ops;
}

void
gaim_privacy_init(void)
{
}
