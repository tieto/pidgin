/**
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
gaim_privacy_permit_add(GaimAccount *account, const char *who)
{
	GSList *l;
	char *name;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);
	g_return_val_if_fail(gaim_account_is_connected(account), FALSE);

	name = g_strdup(normalize(who));

	for (l = account->permit; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, normalize((char *)l->data)))
			break;
	}

	g_free(name);

	if (l != NULL)
		return FALSE;

	account->permit = g_slist_append(account->permit, g_strdup(who));

	serv_add_permit(gaim_account_get_connection(account), who);
	gaim_blist_save();

	if (privacy_ops != NULL && privacy_ops->permit_added != NULL)
		privacy_ops->permit_added(account, who);

	return TRUE;
}

gboolean
gaim_privacy_permit_remove(GaimAccount *account, const char *who)
{
	GSList *l;
	char *name;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);
	g_return_val_if_fail(gaim_account_is_connected(account), FALSE);

	name = g_strdup(normalize(who));

	for (l = account->permit; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, normalize((char *)l->data)))
			break;
	}

	g_free(name);

	if (l == NULL)
		return FALSE;

	account->permit = g_slist_remove(account->permit, l->data);
	g_free(l->data);

	serv_rem_deny(gaim_account_get_connection(account), who);
	gaim_blist_save();

	if (privacy_ops != NULL && privacy_ops->permit_removed != NULL)
		privacy_ops->permit_removed(account, who);

	return TRUE;
}

gboolean
gaim_privacy_deny_add(GaimAccount *account, const char *who)
{
	GSList *l;
	char *name;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);
	g_return_val_if_fail(gaim_account_is_connected(account), FALSE);

	name = g_strdup(normalize(who));

	for (l = account->deny; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, normalize((char *)l->data)))
			break;
	}

	g_free(name);

	if (l != NULL)
		return FALSE;

	account->deny = g_slist_append(account->deny, g_strdup(who));

	serv_add_deny(gaim_account_get_connection(account), who);
	gaim_blist_save();

	if (privacy_ops != NULL && privacy_ops->deny_added != NULL)
		privacy_ops->deny_added(account, who);

	return TRUE;
}

gboolean
gaim_privacy_deny_remove(GaimAccount *account, const char *who)
{
	GSList *l;
	char *name;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);
	g_return_val_if_fail(gaim_account_is_connected(account), FALSE);

	name = g_strdup(normalize(who));

	for (l = account->deny; l != NULL; l = l->next) {
		if (!gaim_utf8_strcasecmp(name, normalize((char *)l->data)))
			break;
	}

	g_free(name);

	if (l == NULL)
		return FALSE;

	account->deny = g_slist_remove(account->deny, l->data);
	g_free(l->data);

	serv_rem_deny(gaim_account_get_connection(account), who);
	gaim_blist_save();

	if (privacy_ops != NULL && privacy_ops->deny_removed != NULL)
		privacy_ops->deny_removed(account, who);

	return TRUE;
}

void
gaim_set_privacy_ui_ops(GaimPrivacyUiOps *ops)
{
	privacy_ops = ops;
}

GaimPrivacyUiOps *
gaim_get_privacy_ui_ops(void)
{
	return privacy_ops;
}

void
gaim_privacy_init(void)
{
}
