/**
 * @file account.c Account API
 * @ingroup core
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
#include "account.h"
#include "prefs.h"

typedef struct
{
	GaimPrefType type;

	union
	{
		int integer;
		char *string;
		gboolean bool;

	} value;

} GaimAccountSetting;

static GList *accounts = NULL;

static void
__delete_setting(void *data)
{
	GaimAccountSetting *setting = (GaimAccountSetting *)data;

	if (setting->type == GAIM_PREF_STRING)
		g_free(setting->value.string);

	g_free(setting);
}

GaimAccount *
gaim_account_new(const char *username, GaimProtocol protocol)
{
	GaimAccount *account;

	g_return_val_if_fail(username != NULL, NULL);

	account = g_new0(GaimAccount, 1);

	gaim_account_set_username(account, username);
	gaim_account_set_protocol(account, protocol);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, __delete_setting);

	accounts = g_list_append(accounts, account);

	return account;
}

void
gaim_account_destroy(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	if (account->gc != NULL)
		gaim_connection_destroy(account->gc);

	if (account->username  != NULL) g_free(account->username);
	if (account->alias     != NULL) g_free(account->alias);
	if (account->password  != NULL) g_free(account->password);
	if (account->user_info != NULL) g_free(account->user_info);

	g_hash_table_destroy(account->settings);

	accounts = g_list_remove(accounts, account);

	g_free(account);
}

GaimConnection *
gaim_account_connect(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(!gaim_account_is_connected(account), NULL);

	gc = gaim_connection_new(account);

	gaim_connection_connect(gc);

	return gc;
}

void
gaim_account_disconnect(GaimAccount *account)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gaim_connection_disconnect(account->gc);
	gaim_connection_destroy(account->gc);

	account->gc = NULL;
}

void
gaim_account_set_username(GaimAccount *account, const char *username)
{
	g_return_if_fail(account  != NULL);
	g_return_if_fail(username != NULL);

	if (account->username != NULL)
		g_free(account->username);

	account->username = (username == NULL ? NULL : g_strdup(username));
}

void
gaim_account_set_password(GaimAccount *account, const char *password)
{
	g_return_if_fail(account  != NULL);
	g_return_if_fail(password != NULL);

	if (account->password != NULL)
		g_free(account->password);

	account->password = (password == NULL ? NULL : g_strdup(password));
}

void
gaim_account_set_alias(GaimAccount *account, const char *alias)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(alias   != NULL);

	if (account->alias != NULL)
		g_free(account->alias);

	account->alias = (alias == NULL ? NULL : g_strdup(alias));
}

void
gaim_account_set_user_info(GaimAccount *account, const char *user_info)
{
	g_return_if_fail(account   != NULL);
	g_return_if_fail(user_info != NULL);

	if (account->user_info != NULL)
		g_free(account->user_info);

	account->user_info = (user_info == NULL ? NULL : g_strdup(user_info));
}

void
gaim_account_set_buddy_icon(GaimAccount *account, const char *icon)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(icon    != NULL);

	if (account->buddy_icon != NULL)
		g_free(account->buddy_icon);

	account->buddy_icon = (icon == NULL ? NULL : g_strdup(icon));
}

void
gaim_account_set_protocol(GaimAccount *account, GaimProtocol protocol)
{
	g_return_if_fail(account != NULL);

	account->protocol = protocol;
}

void
gaim_account_set_connection(GaimAccount *account, GaimConnection *gc)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(gc      != NULL);

	account->gc = gc;
}

void
gaim_account_set_remember_password(GaimAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	account->remember_pass = value;
}

void
gaim_account_set_int(GaimAccount *account, const char *name, int value)
{
	GaimAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type          = GAIM_PREF_INT;
	setting->value.integer = value;

	g_hash_table_insert(account->settings, g_strdup(name), setting);
}

void
gaim_account_set_string(GaimAccount *account, const char *name,
						const char *value)
{
	GaimAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type         = GAIM_PREF_STRING;
	setting->value.string = g_strdup(value);

	g_hash_table_insert(account->settings, g_strdup(name), setting);
}

void
gaim_account_set_bool(GaimAccount *account, const char *name, gboolean value)
{
	GaimAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type       = GAIM_PREF_BOOLEAN;
	setting->value.bool = value;

	g_hash_table_insert(account->settings, g_strdup(name), setting);
}

gboolean
gaim_account_is_connected(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return (account->gc != NULL &&
			gaim_connection_get_state(account->gc) == GAIM_CONNECTED);
}

const char *
gaim_account_get_username(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->username;
}

const char *
gaim_account_get_password(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->password;
}

const char *
gaim_account_get_alias(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->alias;
}

const char *
gaim_account_get_user_info(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->user_info;
}

const char *
gaim_account_get_buddy_icon(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->buddy_icon;
}

GaimProtocol
gaim_account_get_protocol(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, -1);

	return account->protocol;
}

GaimConnection *
gaim_account_get_connection(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->gc;
}

gboolean
gaim_account_get_remember_password(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return account->remember_pass;
}

int
gaim_account_get_int(const GaimAccount *account, const char *name)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, -1);
	g_return_val_if_fail(name    != NULL, -1);

	setting = g_hash_table_lookup(account->settings, name);

	g_return_val_if_fail(setting->type == GAIM_PREF_INT, -1);

	return setting->value.integer;
}

const char *
gaim_account_get_string(const GaimAccount *account, const char *name)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	setting = g_hash_table_lookup(account->settings, name);

	g_return_val_if_fail(setting->type == GAIM_PREF_STRING, NULL);

	return setting->value.string;
}

gboolean
gaim_account_get_bool(const GaimAccount *account, const char *name)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(name    != NULL, FALSE);

	setting = g_hash_table_lookup(account->settings, name);

	g_return_val_if_fail(setting->type == GAIM_PREF_BOOLEAN, FALSE);

	return setting->value.bool;
}

GList *
gaim_accounts_get_all(void)
{
	return accounts;
}
