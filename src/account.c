/**
 * @file account.c Account API
 * @ingroup core
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
#include "internal.h"
#include "account.h"
#include "debug.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "status.h"
#include "util.h"

typedef enum
{
	TAG_NONE = 0,
	TAG_PROTOCOL,
	TAG_NAME,
	TAG_PASSWORD,
	TAG_ALIAS,
	TAG_USERINFO,
	TAG_BUDDYICON,
	TAG_SETTING,
	TAG_TYPE,
	TAG_HOST,
	TAG_PORT

} AccountParserTag;

typedef struct
{
	GaimPrefType type;

	char *ui;

	union
	{
		int integer;
		char *string;
		gboolean bool;

	} value;

} GaimAccountSetting;

typedef struct
{
	AccountParserTag tag;

	GaimAccount *account;
	GaimProxyInfo *proxy_info;
	char *protocol_id;

	GString *buffer;

	GaimPrefType setting_type;
	char *setting_ui;
	char *setting_name;

	gboolean in_proxy;

} AccountParserData;


static GaimAccountUiOps *account_ui_ops = NULL;

static GList   *accounts = NULL;
static guint    accounts_save_timer = 0;
static gboolean accounts_loaded = FALSE;

static void
delete_setting(void *data)
{
	GaimAccountSetting *setting = (GaimAccountSetting *)data;

	if (setting->ui != NULL)
		g_free(setting->ui);

	if (setting->type == GAIM_PREF_STRING)
		g_free(setting->value.string);

	g_free(setting);
}

static gboolean
accounts_save_cb(gpointer unused)
{
	gaim_accounts_sync();
	accounts_save_timer = 0;

	return FALSE;
}

static void
schedule_accounts_save()
{
	if (!accounts_save_timer)
		accounts_save_timer = gaim_timeout_add(5000, accounts_save_cb, NULL);
}

GaimAccount *
gaim_account_new(const char *username, const char *protocol_id)
{
	GaimAccount *account = NULL;
	GaimPlugin *prpl = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	account = gaim_accounts_find(username, protocol_id);

	if (account != NULL)
		return account;

	account = g_new0(GaimAccount, 1);

	gaim_account_set_username(account, username);

	gaim_account_set_protocol_id(account, protocol_id);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
	account->ui_settings = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, (GDestroyNotify)g_hash_table_destroy);
	account->system_log = NULL;

	account->presence = gaim_presence_new_for_account(account);

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	
	if (prpl == NULL)
		return account;
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if ( prpl_info != NULL && prpl_info->status_types != NULL )
		gaim_account_set_status_types(account, prpl_info->status_types(account));

	return account;
}

void
gaim_account_destroy(GaimAccount *account)
{
	GList *l;

	g_return_if_fail(account != NULL);

	gaim_debug_info("account", "Destroying account %p\n", account);

	if (account->gc != NULL)
		gaim_connection_destroy(account->gc);

	gaim_debug_info("account", "Continuing to destroy account %p\n", account);

	for (l = gaim_get_conversations(); l != NULL; l = l->next)
	{
		GaimConversation *conv = (GaimConversation *)l->data;

		if (gaim_conversation_get_account(conv) == account)
			gaim_conversation_set_account(conv, NULL);
	}

	if (account->username    != NULL) g_free(account->username);
	if (account->alias       != NULL) g_free(account->alias);
	if (account->password    != NULL) g_free(account->password);
	if (account->user_info   != NULL) g_free(account->user_info);
	if (account->protocol_id != NULL) g_free(account->protocol_id);

	g_hash_table_destroy(account->settings);
	g_hash_table_destroy(account->ui_settings);

	gaim_account_set_status_types(account, NULL);

	gaim_presence_destroy(account->presence);

	if(account->system_log)
		gaim_log_free(account->system_log);

	g_free(account);
}

GaimConnection *
gaim_account_register(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, NULL);

	if (gaim_account_get_connection(account) != NULL)
		return NULL;

	gc = gaim_connection_new(account);

	gaim_debug_info("account", "Registering account %p. gc = %p\n",
					account, gc);

	gaim_connection_register(gc);

	return gc;
}

GaimConnection *
gaim_account_connect(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, NULL);

	if (gaim_account_get_connection(account) != NULL)
		return NULL;

	gc = gaim_connection_new(account);

	gaim_debug_info("account", "Connecting to account %p. gc = %p\n",
					account, gc);

	gaim_connection_connect(gc);

	return gc;
}

void
gaim_account_disconnect(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gaim_debug_info("account", "Disconnecting account %p\n", account);

	gc = gaim_account_get_connection(account);

	gaim_account_set_connection(account, NULL);

	gaim_connection_disconnect(gc);
}

void
gaim_account_notify_added(GaimAccount *account, const char *id,
						  const char *remote_user, const char *alias,
						  const char *message)
{
	GaimAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = gaim_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->notify_added != NULL)
		ui_ops->notify_added(account, remote_user, id, alias, message);
}

static void
change_password_cb(GaimAccount *account, GaimRequestFields *fields)
{
	const char *orig_pass, *new_pass_1, *new_pass_2;

	orig_pass  = gaim_request_fields_get_string(fields, "password");
	new_pass_1 = gaim_request_fields_get_string(fields, "new_password_1");
	new_pass_2 = gaim_request_fields_get_string(fields, "new_password_2");

	if (g_utf8_collate(new_pass_1, new_pass_2))
	{
		gaim_notify_error(NULL, NULL,
						  _("New passwords do not match."), NULL);

		return;
	}

	if (orig_pass == NULL || new_pass_1 == NULL || new_pass_2 == NULL ||
		*orig_pass == '\0' || *new_pass_1 == '\0' || *new_pass_2 == '\0')
	{
		gaim_notify_error(NULL, NULL,
						  _("Fill out all fields completely."), NULL);
		return;
	}

	serv_change_passwd(gaim_account_get_connection(account),
					   orig_pass, new_pass_1);
	gaim_account_set_password(account, new_pass_1);
}

void
gaim_account_request_change_password(GaimAccount *account)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("password", _("Original password"),
										  NULL, FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("new_password_1",
										  _("New password"),
										  NULL, FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("new_password_2",
										  _("New password (again)"),
										  NULL, FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	g_snprintf(primary, sizeof(primary), _("Change password for %s"),
			   gaim_account_get_username(account));

	/* I'm sticking this somewhere in the code: bologna */

	gaim_request_fields(gaim_account_get_connection(account),
						NULL,
						primary,
						_("Please enter your current password and your "
						  "new password."),
						fields,
						_("OK"), G_CALLBACK(change_password_cb),
						_("Cancel"), NULL,
						account);
}

static void
set_user_info_cb(GaimAccount *account, const char *user_info)
{
	GaimConnection *gc;

	gaim_account_set_user_info(account, user_info);

	gc = gaim_account_get_connection(account);

	if (gc != NULL)
		serv_set_info(gaim_account_get_connection(account), user_info);
}

void
gaim_account_request_change_user_info(GaimAccount *account)
{
	GaimConnection *gc;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gc = gaim_account_get_connection(account);

	g_snprintf(primary, sizeof(primary),
			   _("Change user information for %s"),
			   gaim_account_get_username(account));

	gaim_request_input(gaim_account_get_connection(account),
					   NULL, primary, NULL,
					   gaim_account_get_user_info(account),
					   TRUE, FALSE, ((gc != NULL) &&
					   (gc->flags & GAIM_CONNECTION_HTML) ? "html" : NULL),
					   _("Save"), G_CALLBACK(set_user_info_cb),
					   _("Cancel"), NULL, account);
}

void
gaim_account_set_username(GaimAccount *account, const char *username)
{
	g_return_if_fail(account != NULL);

	if (account->username != NULL)
		g_free(account->username);

	account->username = (username == NULL ? NULL : g_strdup(username));

	schedule_accounts_save();
}

void
gaim_account_set_password(GaimAccount *account, const char *password)
{
	g_return_if_fail(account != NULL);

	if (account->password != NULL)
		g_free(account->password);

	account->password = (password == NULL ? NULL : g_strdup(password));

	schedule_accounts_save();
}

void
gaim_account_set_alias(GaimAccount *account, const char *alias)
{
	g_return_if_fail(account != NULL);

	if (account->alias != NULL)
		g_free(account->alias);

	account->alias = (alias == NULL ? NULL : g_strdup(alias));

	schedule_accounts_save();
}

void
gaim_account_set_user_info(GaimAccount *account, const char *user_info)
{
	g_return_if_fail(account != NULL);

	if (account->user_info != NULL)
		g_free(account->user_info);

	account->user_info = (user_info == NULL ? NULL : g_strdup(user_info));

	schedule_accounts_save();
}

void
gaim_account_set_buddy_icon(GaimAccount *account, const char *icon)
{
	g_return_if_fail(account != NULL);

	if (account->buddy_icon != NULL)
		g_free(account->buddy_icon);

	account->buddy_icon = (icon == NULL ? NULL : g_strdup(icon));
	if (account->gc)
		serv_set_buddyicon(account->gc, icon);
	
	schedule_accounts_save();
}

void
gaim_account_set_protocol_id(GaimAccount *account, const char *protocol_id)
{
	g_return_if_fail(account     != NULL);
	g_return_if_fail(protocol_id != NULL);

	if (account->protocol_id != NULL)
		g_free(account->protocol_id);

	account->protocol_id = g_strdup(protocol_id);

	schedule_accounts_save();
}

void
gaim_account_set_connection(GaimAccount *account, GaimConnection *gc)
{
	g_return_if_fail(account != NULL);

	account->gc = gc;
}

void
gaim_account_set_remember_password(GaimAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	account->remember_pass = value;

	schedule_accounts_save();
}

void
gaim_account_set_check_mail(GaimAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	gaim_account_set_bool(account, "check-mail", value);
}

void
gaim_account_set_auto_login(GaimAccount *account, const char *ui,
							gboolean value)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);

	gaim_account_set_ui_bool(account, ui, "auto-login", value);
}

void
gaim_account_set_proxy_info(GaimAccount *account, GaimProxyInfo *info)
{
	g_return_if_fail(account != NULL);

	if (account->proxy_info != NULL)
		gaim_proxy_info_destroy(account->proxy_info);

	account->proxy_info = info;

	schedule_accounts_save();
}

void
gaim_account_set_status_types(GaimAccount *account, GList *status_types)
{
	g_return_if_fail(account != NULL);

	/* Old with the old... */
	if (account->status_types != NULL)
	{
		GList *l;

		for (l = account->status_types; l != NULL; l = l->next)
			gaim_status_type_destroy((GaimStatusType *)l->data);

		g_list_free(account->status_types);
	}

	/* In with the new... */
	account->status_types = status_types;
}

void
gaim_account_set_status(GaimAccount *account, const char *status_id,
						gboolean active, ...)
{
	GaimStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_account_get_status(account, status_id);

	if (status == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "accounts",
				   "Invalid status ID %s for account %s (%s)\n",
				   status_id, gaim_account_get_username(account),
				   gaim_account_get_protocol_id(account));
		return;
	}

	if (!active && gaim_status_is_independent(status))
	{
		gaim_debug(GAIM_DEBUG_ERROR, "accounts",
				   "Cannot deactivate an exclusive status.\n");
		return;
	}

	if (gaim_status_is_active(status) == active)
		return;

	gaim_status_set_active(status, active);
}

void
gaim_account_clear_settings(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	g_hash_table_destroy(account->settings);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
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

	schedule_accounts_save();
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

	schedule_accounts_save();
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

	schedule_accounts_save();
}

static GHashTable *
get_ui_settings_table(GaimAccount *account, const char *ui)
{
	GHashTable *table;

	table = g_hash_table_lookup(account->ui_settings, ui);

	if (table == NULL) {
		table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
									  delete_setting);
		g_hash_table_insert(account->ui_settings, g_strdup(ui), table);
	}

	return table;
}

void
gaim_account_set_ui_int(GaimAccount *account, const char *ui,
						const char *name, int value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type          = GAIM_PREF_INT;
	setting->ui            = g_strdup(ui);
	setting->value.integer = value;

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

void
gaim_account_set_ui_string(GaimAccount *account, const char *ui,
						   const char *name, const char *value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type         = GAIM_PREF_STRING;
	setting->ui           = g_strdup(ui);
	setting->value.string = g_strdup(value);

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

void
gaim_account_set_ui_bool(GaimAccount *account, const char *ui,
						 const char *name, gboolean value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(GaimAccountSetting, 1);

	setting->type       = GAIM_PREF_BOOLEAN;
	setting->ui         = g_strdup(ui);
	setting->value.bool = value;

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

gboolean
gaim_account_is_connected(const GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, FALSE);

	gc = gaim_account_get_connection(account);

	/* XXX - The first way is better... but it doesn't work quite right yet */
	/* return ((gc != NULL) && GAIM_CONNECTION_IS_CONNECTED(gc)); */
	return ((gc != NULL) && gaim_connection_get_state(gc) != GAIM_DISCONNECTED);
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

const char *
gaim_account_get_protocol_id(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->protocol_id;
}

const char *
gaim_account_get_protocol_name(const GaimAccount *account)
{
	GaimPlugin *p;

	g_return_val_if_fail(account != NULL, NULL);

	p = gaim_find_prpl(gaim_account_get_protocol_id(account));

	return ((p && p->info->name) ? _(p->info->name) : _("Unknown"));
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

gboolean
gaim_account_get_check_mail(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return gaim_account_get_bool(account, "check-mail", FALSE);
}

gboolean
gaim_account_get_auto_login(const GaimAccount *account, const char *ui)
{
	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(ui      != NULL, FALSE);

	return gaim_account_get_ui_bool(account, ui, "auto-login", FALSE);
}

GaimProxyInfo *
gaim_account_get_proxy_info(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->proxy_info;
}

GaimStatus *
gaim_account_get_status(const GaimAccount *account, const char *status_id)
{
	g_return_val_if_fail(account   != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	return gaim_presence_get_status(account->presence, status_id);
}

GaimStatusType *
gaim_account_get_status_type(const GaimAccount *account, const char *id)
{
	const GList *l;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(id      != NULL, NULL);

	for (l = gaim_account_get_status_types(account); l != NULL; l = l->next)
	{
		GaimStatusType *status_type = (GaimStatusType *)l->data;

		if (!strcmp(gaim_status_type_get_id(status_type), id))
			return status_type;
	}

	return NULL;
}

GaimPresence *
gaim_account_get_presence(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->presence;
}

gboolean
gaim_account_is_status_active(const GaimAccount *account,
							  const char *status_id)
{
	g_return_val_if_fail(account   != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	return gaim_presence_is_status_active(account->presence, status_id);
}

const GList *
gaim_account_get_status_types(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->status_types;
}

int
gaim_account_get_int(const GaimAccount *account, const char *name,
					 int default_value)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_INT, default_value);

	return setting->value.integer;
}

const char *
gaim_account_get_string(const GaimAccount *account, const char *name,
						const char *default_value)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_STRING, default_value);

	return setting->value.string;
}

gboolean
gaim_account_get_bool(const GaimAccount *account, const char *name,
					  gboolean default_value)
{
	GaimAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_BOOLEAN, default_value);

	return setting->value.bool;
}

int
gaim_account_get_ui_int(const GaimAccount *account, const char *ui,
						const char *name, int default_value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_INT, default_value);

	return setting->value.integer;
}

const char *
gaim_account_get_ui_string(const GaimAccount *account, const char *ui,
						   const char *name, const char *default_value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_STRING, default_value);

	return setting->value.string;
}

gboolean
gaim_account_get_ui_bool(const GaimAccount *account, const char *ui,
						 const char *name, gboolean default_value)
{
	GaimAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == GAIM_PREF_BOOLEAN, default_value);

	return setting->value.bool;
}

GaimLog *
gaim_account_get_log(GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	if(!account->system_log){
		GaimConnection *gc;

		gc = gaim_account_get_connection(account);

		account->system_log	 = gaim_log_new(GAIM_LOG_SYSTEM,
				gaim_account_get_username(account), account,
				gc != NULL ? gc->login_time : time(NULL));
	}

	return account->system_log;
}

void
gaim_account_destroy_log(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	if(account->system_log){
		gaim_log_free(account->system_log);
		account->system_log = NULL;
	}
}

/* XML Stuff */
static void
free_parser_data(gpointer user_data)
{
	AccountParserData *data = user_data;

	if (data->buffer != NULL)
		g_string_free(data->buffer, TRUE);

	if (data->setting_name != NULL)
		g_free(data->setting_name);

	g_free(data);
}

static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data, GError **error)
{
	const char *value;
	AccountParserData *data = user_data;
	GHashTable *atts;
	int i;

	atts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (i = 0; attribute_names[i] != NULL; i++) {
		g_hash_table_insert(atts, g_strdup(attribute_names[i]),
							g_strdup(attribute_values[i]));
	}

	if (data->buffer != NULL) {
		g_string_free(data->buffer, TRUE);
		data->buffer = NULL;
	}

	if (!strcmp(element_name, "protocol"))
		data->tag = TAG_PROTOCOL;
	else if (!strcmp(element_name, "name") || !strcmp(element_name, "username"))
		data->tag = TAG_NAME;
	else if (!strcmp(element_name, "password"))
		data->tag = TAG_PASSWORD;
	else if (!strcmp(element_name, "alias"))
		data->tag = TAG_ALIAS;
	else if (!strcmp(element_name, "userinfo"))
		data->tag = TAG_USERINFO;
	else if (!strcmp(element_name, "buddyicon"))
		data->tag = TAG_BUDDYICON;
	else if (!strcmp(element_name, "proxy")) {
		data->in_proxy = TRUE;

		data->proxy_info = gaim_proxy_info_new();
	}
	else if (!strcmp(element_name, "type"))
		data->tag = TAG_TYPE;
	else if (!strcmp(element_name, "host"))
		data->tag = TAG_HOST;
	else if (!strcmp(element_name, "port"))
		data->tag = TAG_PORT;
	else if (!strcmp(element_name, "settings")) {
		if ((value = g_hash_table_lookup(atts, "ui")) != NULL) {
			data->setting_ui = g_strdup(value);
		}
	}
	else if (!strcmp(element_name, "setting")) {
		data->tag = TAG_SETTING;

		if ((value = g_hash_table_lookup(atts, "name")) != NULL)
			data->setting_name = g_strdup(value);

		if ((value = g_hash_table_lookup(atts, "type")) != NULL) {
			if (!strcmp(value, "string"))
				data->setting_type = GAIM_PREF_STRING;
			else if (!strcmp(value, "int"))
				data->setting_type = GAIM_PREF_INT;
			else if (!strcmp(value, "bool"))
				data->setting_type = GAIM_PREF_BOOLEAN;
		}
	}

	g_hash_table_destroy(atts);
}

static void
end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					gpointer user_data,  GError **error)
{
	AccountParserData *data = user_data;
	gchar *buffer;

	if (data->buffer == NULL)
		return;

	buffer = g_string_free(data->buffer, FALSE);
	data->buffer = NULL;

	if (data->tag == TAG_PROTOCOL) {
		data->protocol_id = g_strdup(buffer);
	}
	else if (data->tag == TAG_NAME) {
		if (data->in_proxy) {
			gaim_proxy_info_set_username(data->proxy_info, buffer);
		}
		else {
			data->account = gaim_account_new(buffer, data->protocol_id);

			gaim_accounts_add(data->account);

			g_free(data->protocol_id);

			data->protocol_id = NULL;
		}
	}
	else if (data->tag == TAG_PASSWORD) {
		if (*buffer != '\0') {
			if (data->in_proxy) {
				gaim_proxy_info_set_password(data->proxy_info, buffer);
			}
			else {
				gaim_account_set_password(data->account, buffer);
				gaim_account_set_remember_password(data->account, TRUE);
			}
		}
	}
	else if (data->tag == TAG_ALIAS) {
		if (*buffer != '\0')
			gaim_account_set_alias(data->account, buffer);
	}
	else if (data->tag == TAG_USERINFO) {
		if (*buffer != '\0')
			gaim_account_set_user_info(data->account, buffer);
	}
	else if (data->tag == TAG_BUDDYICON) {
		if (*buffer != '\0')
			gaim_account_set_buddy_icon(data->account, buffer);
	}
	else if (data->tag == TAG_TYPE) {
		if (data->in_proxy) {
			if (!strcmp(buffer, "global"))
				gaim_proxy_info_set_type(data->proxy_info,
										 GAIM_PROXY_USE_GLOBAL);
			else if (!strcmp(buffer, "none"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_NONE);
			else if (!strcmp(buffer, "http"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_HTTP);
			else if (!strcmp(buffer, "socks4"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_SOCKS4);
			else if (!strcmp(buffer, "socks5"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_SOCKS5);
			else if (!strcmp(buffer, "envvar"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_USE_ENVVAR);
			else
				gaim_debug(GAIM_DEBUG_ERROR, "account",
						   "Invalid proxy type found when loading account "
						   "information for %s\n",
						   gaim_account_get_username(data->account));
		}
	}
	else if (data->tag == TAG_HOST) {
		if (data->in_proxy && *buffer != '\0')
			gaim_proxy_info_set_host(data->proxy_info, buffer);
	}
	else if (data->tag == TAG_PORT) {
		if (data->in_proxy && *buffer != '\0')
			gaim_proxy_info_set_port(data->proxy_info, atoi(buffer));
	}
	else if (data->tag == TAG_SETTING) {
		if (*buffer != '\0') {
			if (data->setting_ui != NULL) {
				if (data->setting_type == GAIM_PREF_STRING)
					gaim_account_set_ui_string(data->account, data->setting_ui,
											   data->setting_name, buffer);
				else if (data->setting_type == GAIM_PREF_INT)
					gaim_account_set_ui_int(data->account, data->setting_ui,
											data->setting_name, atoi(buffer));
				else if (data->setting_type == GAIM_PREF_BOOLEAN)
					gaim_account_set_ui_bool(data->account, data->setting_ui,
											 data->setting_name,
											 (*buffer == '0' ? FALSE : TRUE));
			}
			else {
				if (data->setting_type == GAIM_PREF_STRING)
					gaim_account_set_string(data->account, data->setting_name,
											buffer);
				else if (data->setting_type == GAIM_PREF_INT)
					gaim_account_set_int(data->account, data->setting_name,
										 atoi(buffer));
				else if (data->setting_type == GAIM_PREF_BOOLEAN)
					gaim_account_set_bool(data->account, data->setting_name,
										  (*buffer == '0' ? FALSE : TRUE));
			}
		}

		g_free(data->setting_name);
		data->setting_name = NULL;
	}
	else if (!strcmp(element_name, "proxy")) {
		data->in_proxy = FALSE;

		if (gaim_proxy_info_get_type(data->proxy_info) == GAIM_PROXY_USE_GLOBAL) {
			gaim_proxy_info_destroy(data->proxy_info);
			data->proxy_info = NULL;
		}
		else if (*buffer != '\0') {
			gaim_account_set_proxy_info(data->account, data->proxy_info);
		}
	}
	else if (!strcmp(element_name, "settings")) {
		if (data->setting_ui != NULL) {
			g_free(data->setting_ui);
			data->setting_ui = NULL;
		}
	}

	data->tag = TAG_NONE;

	g_free(buffer);
}

static void
text_handler(GMarkupParseContext *context, const gchar *text,
			 gsize text_len, gpointer user_data, GError **error)
{
	AccountParserData *data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}

static GMarkupParser accounts_parser =
{
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	NULL
};

gboolean
gaim_accounts_load()
{
	gchar *filename = g_build_filename(gaim_user_dir(), "accounts.xml", NULL);
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;
	AccountParserData *parser_data;

	if (filename == NULL) {
		accounts_loaded = TRUE;
		return FALSE;
	}

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts",
				   "Error reading accounts: %s\n", error->message);
		g_error_free(error);
		g_free(filename);
		accounts_loaded = TRUE;

		return FALSE;
	}

	parser_data = g_new0(AccountParserData, 1);

	context = g_markup_parse_context_new(&accounts_parser, 0,
										 parser_data, free_parser_data);

	if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		accounts_loaded = TRUE;

		return TRUE;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts", "Error parsing %s\n",
				   filename);
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		accounts_loaded = TRUE;

		return TRUE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);
	g_free(filename);
	accounts_loaded = TRUE;

	return TRUE;
}

static void
write_setting(gpointer key, gpointer value, gpointer user_data)
{
	GaimAccountSetting *setting;
	const char *name;
	FILE *fp;

	setting = (GaimAccountSetting *)value;
	name    = (const char *)key;
	fp      = (FILE *)user_data;

	if (setting->type == GAIM_PREF_INT) {
		fprintf(fp, "   <setting name='%s' type='int'>%d</setting>\n",
				name, setting->value.integer);
	}
	else if (setting->type == GAIM_PREF_STRING && setting->value.string != NULL) {
		fprintf(fp, "   <setting name='%s' type='string'>%s</setting>\n",
				name, setting->value.string);
	}
	else if (setting->type == GAIM_PREF_BOOLEAN) {
		fprintf(fp, "   <setting name='%s' type='bool'>%d</setting>\n",
				name, setting->value.bool);
	}
}

static void
write_ui_setting_list(gpointer key, gpointer value, gpointer user_data)
{
	GHashTable *table;
	const char *ui;
	FILE *fp;

	table = (GHashTable *)value;
	ui    = (const char *)key;
	fp    = (FILE *)user_data;

	fprintf(fp, "  <settings ui='%s'>\n", ui);
	g_hash_table_foreach(table, write_setting, fp);
	fprintf(fp, "  </settings>\n");
}

static void
gaim_accounts_write(FILE *fp, GaimAccount *account)
{
	GaimProxyInfo *proxy_info;
	GaimProxyType proxy_type;
	const char *password, *alias, *user_info, *buddy_icon;
	char *esc;

	fprintf(fp, " <account>\n");
	fprintf(fp, "  <protocol>%s</protocol>\n", account->protocol_id);
	esc = g_markup_escape_text(gaim_account_get_username(account), -1);
	fprintf(fp, "  <name>%s</name>\n", esc);
	g_free(esc);

	if (gaim_account_get_remember_password(account) &&
		(password = gaim_account_get_password(account)) != NULL) {
		esc = g_markup_escape_text(password, -1);
		fprintf(fp, "  <password>%s</password>\n", esc);
		g_free(esc);
	}

	if ((alias = gaim_account_get_alias(account)) != NULL) {
		esc = g_markup_escape_text(alias, -1);
		fprintf(fp, "  <alias>%s</alias>\n", esc);
		g_free(esc);
	}

	if ((user_info = gaim_account_get_user_info(account)) != NULL) {
		esc = g_markup_escape_text(user_info, -1);
                gaim_str_strip_cr(esc);
		fprintf(fp, "  <userinfo>%s</userinfo>\n", esc);
		g_free(esc);
	}

	if ((buddy_icon = gaim_account_get_buddy_icon(account)) != NULL) {
		esc = g_markup_escape_text(buddy_icon, -1);
		fprintf(fp, "  <buddyicon>%s</buddyicon>\n", esc);
		g_free(esc);
	}

	fprintf(fp, "  <settings>\n");
	g_hash_table_foreach(account->settings, write_setting, fp);
	fprintf(fp, "  </settings>\n");

	g_hash_table_foreach(account->ui_settings, write_ui_setting_list, fp);

	if ((proxy_info = gaim_account_get_proxy_info(account)) != NULL)
	{
		const char *value;
		int int_value;

		proxy_type = gaim_proxy_info_get_type(proxy_info);

		fprintf(fp, "  <proxy>\n");
		fprintf(fp, "   <type>%s</type>\n",
				(proxy_type == GAIM_PROXY_USE_GLOBAL ? "global" :
				 proxy_type == GAIM_PROXY_NONE       ? "none"   :
				 proxy_type == GAIM_PROXY_HTTP       ? "http"   :
				 proxy_type == GAIM_PROXY_SOCKS4     ? "socks4" :
				 proxy_type == GAIM_PROXY_SOCKS5     ? "socks5" :
				 proxy_type == GAIM_PROXY_USE_ENVVAR ? "envvar" : "unknown"));

		if (proxy_type != GAIM_PROXY_USE_GLOBAL &&
			proxy_type != GAIM_PROXY_NONE &&
			proxy_type != GAIM_PROXY_USE_ENVVAR) {
			if ((value = gaim_proxy_info_get_host(proxy_info)) != NULL)
				fprintf(fp, "   <host>%s</host>\n", value);

			if ((int_value = gaim_proxy_info_get_port(proxy_info)) != 0)
				fprintf(fp, "   <port>%d</port>\n", int_value);

			if ((value = gaim_proxy_info_get_username(proxy_info)) != NULL)
				fprintf(fp, "   <username>%s</username>\n", value);

			if ((value = gaim_proxy_info_get_password(proxy_info)) != NULL)
				fprintf(fp, "   <password>%s</password>\n", value);
		}

		fprintf(fp, "  </proxy>\n");
	}

	fprintf(fp, " </account>\n");
}

void
gaim_accounts_sync(void)
{
	FILE *fp;
	const char *user_dir = gaim_user_dir();
	char *filename;
	char *filename_real;

	if (!accounts_loaded) {
		gaim_debug(GAIM_DEBUG_WARNING, "accounts",
				   "Writing accounts to disk.\n");
		schedule_accounts_save();
		return;
	}

	if (user_dir == NULL)
		return;

	gaim_debug(GAIM_DEBUG_INFO, "accounts", "Writing accounts to disk.\n");

	fp = fopen(user_dir, "r");

	if (fp == NULL)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(fp);

	filename = g_build_filename(user_dir, "accounts.xml.save", NULL);

	if ((fp = fopen(filename, "w")) != NULL) {
		GList *l;

		fprintf(fp, "<?xml version='1.0' encoding='UTF-8' ?>\n\n");
		fprintf(fp, "<accounts>\n");

		for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
			gaim_accounts_write(fp, l->data);

		fprintf(fp, "</accounts>\n");

		fclose(fp);
		chmod(filename, S_IRUSR | S_IWUSR);
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts", "Unable to write %s\n",
				   filename);
		g_free(filename);
		return;
	}

	filename_real = g_build_filename(user_dir, "accounts.xml", NULL);

	if (rename(filename, filename_real) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts", "Error renaming %s to %s\n",
				   filename, filename_real);
	}

	g_free(filename);
	g_free(filename_real);
}

void
gaim_accounts_add(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	if (g_list_find(accounts, account) != NULL)
		return;

	accounts = g_list_append(accounts, account);

	schedule_accounts_save();

	gaim_signal_emit(gaim_accounts_get_handle(), "account-added", account);
}

void
gaim_accounts_remove(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	accounts = g_list_remove(accounts, account);

	schedule_accounts_save();

	gaim_signal_emit(gaim_accounts_get_handle(), "account-removed", account);
}

void
gaim_accounts_delete(GaimAccount *account)
{
	GaimBlistNode *gnode, *cnode, *bnode;

	g_return_if_fail(account != NULL);

	gaim_accounts_remove(account);

	/* Remove this account's buddies */
	for (gnode = gaim_get_blist()->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				for (bnode = cnode->child; bnode; bnode = bnode->next) {
					if (GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
						GaimBuddy *b = (GaimBuddy *)bnode;

						if (b->account == account)
							gaim_blist_remove_buddy(b);
					}
				}
			} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
				GaimChat *c = (GaimChat *)cnode;

				if (c->account == account)
					gaim_blist_remove_chat(c);
			}
		}
	}

	/* Remove this account's pounces */
	gaim_pounce_destroy_all_by_account(account);

	gaim_account_destroy(account);
}

void
gaim_accounts_auto_login(const char *ui)
{
	GaimAccount *account;
	GList *l;

	g_return_if_fail(ui != NULL);

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		account = l->data;

		if (gaim_account_get_auto_login(account, ui))
			gaim_account_connect(account);
	}
}

void
gaim_accounts_reorder(GaimAccount *account, size_t new_index)
{
	size_t index;
	GList *l;

	g_return_if_fail(account != NULL);
	g_return_if_fail(new_index >= 0 && new_index <= g_list_length(accounts));

	index = g_list_index(accounts, account);

	if (index == -1) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts",
				   "Unregistered account (%s) discovered during reorder!\n",
				   gaim_account_get_username(account));
		return;
	}

	l = g_list_nth(accounts, index);

	if (new_index > index)
		new_index--;

	/* Remove the old one. */
	accounts = g_list_delete_link(accounts, l);

	/* Insert it where it should go. */
	accounts = g_list_insert(accounts, account, new_index);

	schedule_accounts_save();
}

GList *
gaim_accounts_get_all(void)
{
	return accounts;
}

GaimAccount *
gaim_accounts_find(const char *name, const char *protocol_id)
{
	GaimAccount *account = NULL;
	GList *l;
	char *who;

	g_return_val_if_fail(name != NULL, NULL);

	who = g_strdup(gaim_normalize(NULL, name));

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		account = (GaimAccount *)l->data;

		if (!strcmp(gaim_normalize(NULL, gaim_account_get_username(account)), who) &&
			(!protocol_id || !strcmp(account->protocol_id, protocol_id))) {

			break;
		}

		account = NULL;
	}

	g_free(who);

	return account;
}

void
gaim_accounts_set_ui_ops(GaimAccountUiOps *ops)
{
	account_ui_ops = ops;
}

GaimAccountUiOps *
gaim_accounts_get_ui_ops(void)
{
	return account_ui_ops;
}

void *
gaim_accounts_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_accounts_init(void)
{
	void *handle = gaim_accounts_get_handle();

	gaim_signal_register(handle, "account-connecting",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT));

	gaim_signal_register(handle, "account-away",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "account-setting-info",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "account-set-info",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "account-warned",
						 gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "account-added",
			gaim_marshal_VOID__POINTER, NULL, 1,
			gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_ACCOUNT));

	gaim_signal_register(handle, "account-removed",
			gaim_marshal_VOID__POINTER, NULL, 1,
			gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_ACCOUNT));
}

void
gaim_accounts_uninit(void)
{
	if (accounts_save_timer != 0) {
		gaim_timeout_remove(accounts_save_timer);
		accounts_save_timer = 0;
		gaim_accounts_sync();
	}

	gaim_signals_unregister_by_instance(gaim_accounts_get_handle());
}
