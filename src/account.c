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
#include "xmlnode.h"

/* TODO: Should use GaimValue instead of this?  What about "ui"? */
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


static GaimAccountUiOps *account_ui_ops = NULL;

static GList   *accounts = NULL;
static guint    save_timer = 0;
static gboolean accounts_loaded = FALSE;


/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static void
setting_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *name;
	GaimAccountSetting *setting;
	xmlnode *node, *child;
	char buf[20];

	name    = (const char *)key;
	setting = (GaimAccountSetting *)value;
	node    = (xmlnode *)user_data;

	child = xmlnode_new_child(node, "setting");
	xmlnode_set_attrib(child, "name", name);

	if (setting->type == GAIM_PREF_INT) {
		xmlnode_set_attrib(child, "type", "int");
		snprintf(buf, sizeof(buf), "%d", setting->value.integer);
		xmlnode_insert_data(child, buf, -1);
	}
	else if (setting->type == GAIM_PREF_STRING && setting->value.string != NULL) {
		xmlnode_set_attrib(child, "type", "string");
		xmlnode_insert_data(child, setting->value.string, -1);
	}
	else if (setting->type == GAIM_PREF_BOOLEAN) {
		xmlnode_set_attrib(child, "type", "bool");
		snprintf(buf, sizeof(buf), "%d", setting->value.bool);
		xmlnode_insert_data(child, buf, -1);
	}
}

static void
ui_setting_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *ui;
	GHashTable *table;
	xmlnode *node, *child;

	ui    = (const char *)key;
	table = (GHashTable *)value;
	node  = (xmlnode *)user_data;

	if (g_hash_table_size(table) > 0)
	{
		child = xmlnode_new_child(node, "settings");
		xmlnode_set_attrib(child, "ui", ui);
		g_hash_table_foreach(table, setting_to_xmlnode, child);
	}
}

static xmlnode *
proxy_settings_to_xmlnode(GaimProxyInfo *proxy_info)
{
	xmlnode *node, *child;
	GaimProxyType proxy_type;
	const char *value;
	int int_value;
	char buf[20];

	proxy_type = gaim_proxy_info_get_type(proxy_info);

	node = xmlnode_new("proxy");

	child = xmlnode_new_child(node, "type");
	xmlnode_insert_data(child,
			(proxy_type == GAIM_PROXY_USE_GLOBAL ? "global" :
			 proxy_type == GAIM_PROXY_NONE       ? "none"   :
			 proxy_type == GAIM_PROXY_HTTP       ? "http"   :
			 proxy_type == GAIM_PROXY_SOCKS4     ? "socks4" :
			 proxy_type == GAIM_PROXY_SOCKS5     ? "socks5" :
			 proxy_type == GAIM_PROXY_USE_ENVVAR ? "envvar" : "unknown"), -1);

	if (proxy_type != GAIM_PROXY_USE_GLOBAL &&
		proxy_type != GAIM_PROXY_NONE &&
		proxy_type != GAIM_PROXY_USE_ENVVAR)
	{
		if ((value = gaim_proxy_info_get_host(proxy_info)) != NULL)
		{
			child = xmlnode_new_child(node, "host");
			xmlnode_insert_data(child, value, -1);
		}

		if ((int_value = gaim_proxy_info_get_port(proxy_info)) != 0)
		{
			snprintf(buf, sizeof(buf), "%d", int_value);
			child = xmlnode_new_child(node, "port");
			xmlnode_insert_data(child, buf, -1);
		}

		if ((value = gaim_proxy_info_get_username(proxy_info)) != NULL)
		{
			child = xmlnode_new_child(node, "username");
			xmlnode_insert_data(child, value, -1);
		}

		if ((value = gaim_proxy_info_get_password(proxy_info)) != NULL)
		{
			child = xmlnode_new_child(node, "password");
			xmlnode_insert_data(child, value, -1);
		}
	}

	return node;
}

static xmlnode *
account_to_xmlnode(GaimAccount *account)
{
	xmlnode *node, *child;
	const char *tmp;
	GaimProxyInfo *proxy_info;

	node = xmlnode_new("account");

	child = xmlnode_new_child(node, "protocol");
	xmlnode_insert_data(child, gaim_account_get_protocol_id(account), -1);

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, gaim_account_get_username(account), -1);

	if (gaim_account_get_remember_password(account) &&
		((tmp = gaim_account_get_password(account)) != NULL))
	{
		child = xmlnode_new_child(node, "password");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((tmp = gaim_account_get_alias(account)) != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((tmp = gaim_account_get_user_info(account)) != NULL)
	{
		/* TODO: Do we need to call gaim_str_strip_cr(tmp) here? */
		child = xmlnode_new_child(node, "userinfo");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((tmp = gaim_account_get_buddy_icon(account)) != NULL)
	{
		child = xmlnode_new_child(node, "buddyicon");
		xmlnode_insert_data(child, tmp, -1);
	}

	if (g_hash_table_size(account->settings) > 0)
	{
		child = xmlnode_new_child(node, "settings");
		g_hash_table_foreach(account->settings, setting_to_xmlnode, child);
	}

	if (g_hash_table_size(account->ui_settings) > 0)
	{
		g_hash_table_foreach(account->ui_settings, ui_setting_to_xmlnode, node);
	}

	if ((proxy_info = gaim_account_get_proxy_info(account)) != NULL)
	{
		child = proxy_settings_to_xmlnode(proxy_info);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
accounts_to_xmlnode(void)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("accounts");
	xmlnode_set_attrib(node, "version", "1.0");

	for (cur = gaim_accounts_get_all(); cur != NULL; cur = cur->next)
	{
		child = account_to_xmlnode(cur->data);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static void
sync_accounts(void)
{
	xmlnode *node;
	char *data;

	if (!accounts_loaded)
	{
		gaim_debug_error("accounts", "Attempted to save accounts before "
						 "they were read!\n");
		return;
	}

	node = accounts_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	gaim_util_write_data_to_file("accounts.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	sync_accounts();
	save_timer = 0;
	return FALSE;
}

static void
schedule_accounts_save()
{
	if (save_timer == 0)
		save_timer = gaim_timeout_add(5000, save_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static void
parse_settings(xmlnode *node, GaimAccount *account)
{
	const char *ui;
	xmlnode *child;

	/* Get the UI string, if these are UI settings */
	ui = xmlnode_get_attrib(node, "ui");

	/* Read settings, one by one */
	for (child = xmlnode_get_child(node, "setting"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		const char *name, *str_type;
		GaimPrefType type;
		char *data;

		name = xmlnode_get_attrib(child, "name");
		if (name == NULL)
			/* Ignore this setting */
			continue;

		str_type = xmlnode_get_attrib(child, "type");
		if (str_type == NULL)
			/* Ignore this setting */
			continue;

		if (!strcmp(str_type, "string"))
			type = GAIM_PREF_STRING;
		else if (!strcmp(str_type, "int"))
			type = GAIM_PREF_INT;
		else if (!strcmp(str_type, "bool"))
			type = GAIM_PREF_BOOLEAN;
		else
			/* Ignore this setting */
			continue;

		data = xmlnode_get_data(child);
		if (data == NULL)
			/* Ignore this setting */
			continue;

		if (ui == NULL)
		{
			if (type == GAIM_PREF_STRING)
				gaim_account_set_string(account, name, data);
			else if (type == GAIM_PREF_INT)
				gaim_account_set_int(account, name, atoi(data));
			else if (type == GAIM_PREF_BOOLEAN)
				gaim_account_set_bool(account, name,
									  (*data == '0' ? FALSE : TRUE));
		} else {
			if (type == GAIM_PREF_STRING)
				gaim_account_set_ui_string(account, ui, name, data);
			else if (type == GAIM_PREF_INT)
				gaim_account_set_ui_int(account, ui, name, atoi(data));
			else if (type == GAIM_PREF_BOOLEAN)
				gaim_account_set_ui_bool(account, ui, name,
										 (*data == '0' ? FALSE : TRUE));
		}

		g_free(data);
	}
}

static void
parse_proxy_info(xmlnode *node, GaimAccount *account)
{
	GaimProxyInfo *proxy_info;
	xmlnode *child;
	char *data;

	proxy_info = gaim_proxy_info_new();

	/* Use the global proxy settings, by default */
	gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_USE_GLOBAL);

	/* Read proxy type */
	child = xmlnode_get_child(node, "type");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		if (!strcmp(data, "global"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_USE_GLOBAL);
		else if (!strcmp(data, "none"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_NONE);
		else if (!strcmp(data, "http"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_HTTP);
		else if (!strcmp(data, "socks4"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_SOCKS4);
		else if (!strcmp(data, "socks5"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_SOCKS5);
		else if (!strcmp(data, "envvar"))
			gaim_proxy_info_set_type(proxy_info, GAIM_PROXY_USE_ENVVAR);
		else
		{
			gaim_debug_error("account", "Invalid proxy type found when "
							 "loading account information for %s\n",
							 gaim_account_get_username(account));
		}
		g_free(data);
	}

	/* Read proxy host */
	child = xmlnode_get_child(node, "host");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_proxy_info_set_host(proxy_info, data);
		g_free(data);
	}

	/* Read proxy port */
	child = xmlnode_get_child(node, "port");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_proxy_info_set_port(proxy_info, atoi(data));
		g_free(data);
	}

	/* Read proxy username */
	child = xmlnode_get_child(node, "username");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_proxy_info_set_username(proxy_info, data);
		g_free(data);
	}

	/* Read proxy password */
	child = xmlnode_get_child(node, "password");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_proxy_info_set_password(proxy_info, data);
		g_free(data);
	}

	/* If there are no values set then proxy_infourn NULL */
	if ((gaim_proxy_info_get_type(proxy_info) == GAIM_PROXY_USE_GLOBAL) &&
		(gaim_proxy_info_get_host(proxy_info) == NULL) &&
		(gaim_proxy_info_get_port(proxy_info) == 0) &&
		(gaim_proxy_info_get_username(proxy_info) == NULL) &&
		(gaim_proxy_info_get_password(proxy_info) == NULL))
	{
		gaim_proxy_info_destroy(proxy_info);
		return;
	}

	gaim_account_set_proxy_info(account, proxy_info);
}

static GaimAccount *
parse_account(xmlnode *node)
{
	GaimAccount *ret;
	xmlnode *child;
	char *protocol_id = NULL;
	char *name = NULL;
	char *data;

	child = xmlnode_get_child(node, "protocol");
	if (child != NULL)
		protocol_id = xmlnode_get_data(child);

	child = xmlnode_get_child(node, "name");
	if (child != NULL)
		name = xmlnode_get_data(child);
	if (name == NULL)
	{
		/* Do we really need to do this? */
		child = xmlnode_get_child(node, "username");
		if (child != NULL)
			name = xmlnode_get_data(child);
	}

	if ((protocol_id == NULL) || (name == NULL))
	{
		free(protocol_id);
		free(name);
		return NULL;
	}

	ret = gaim_account_new(name, protocol_id);
	free(name);
	free(protocol_id);

	/* Read the password */
	child = xmlnode_get_child(node, "password");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_account_set_password(ret, data);
		gaim_account_set_remember_password(ret, TRUE);
		g_free(data);
	}

	/* Read the alias */
	child = xmlnode_get_child(node, "alias");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_account_set_alias(ret, data);
		g_free(data);
	}

	/* Read the userinfo */
	child = xmlnode_get_child(node, "userinfo");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_account_set_user_info(ret, data);
		g_free(data);
	}

	/* Read the buddyicon */
	child = xmlnode_get_child(node, "buddyicon");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_account_set_buddy_icon(ret, data);
		g_free(data);
	}

	/* Read settings (both core and UI) */
	for (child = xmlnode_get_child(node, "settings"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		parse_settings(child, ret);
	}

	/* Read proxy */
	child = xmlnode_get_child(node, "proxy");
	if (child != NULL)
	{
		parse_proxy_info(child, ret);
	}

	return ret;
}

static void
load_accounts(void)
{
	xmlnode *node, *child;

	accounts_loaded = TRUE;

	node = gaim_util_read_xml_from_file("accounts.xml", _("accounts"));

	if (node == NULL)
		return;

	for (child = xmlnode_get_child(node, "account"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		GaimAccount *new;
		new = parse_account(child);
		gaim_accounts_add(new);
	}
}


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

	prpl = gaim_find_prpl(protocol_id);

	if (prpl == NULL)
		return account;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info != NULL && prpl_info->status_types != NULL )
		gaim_account_set_status_types(account, prpl_info->status_types(account));

	gaim_presence_set_status_active(account->presence, "offline", TRUE);

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
gaim_account_connect(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, NULL);

	if (gaim_account_get_connection(account) != NULL)
		return NULL;

	gc = gaim_connection_new(account);

	gaim_debug_info("account", "Connecting to account %p. gc = %p\n",
					account, gc);

	gaim_connection_connect(gc, status);

	return gc;
}

void
gaim_account_disconnect(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gaim_debug_info("account", "Disconnecting account %p\n", account);

	account->disconnecting = TRUE;
	gc = gaim_account_get_connection(account);

	gaim_connection_disconnect(gc);

	gaim_account_set_connection(account, NULL);
	account->disconnecting = FALSE;
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
gaim_account_set_enabled(GaimAccount *account, const char *ui,
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
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_account_get_status(account, status_id);

	if (status == NULL)
	{
		gaim_debug_error("accounts",
				   "Invalid status ID %s for account %s (%s)\n",
				   status_id, gaim_account_get_username(account),
				   gaim_account_get_protocol_id(account));
		return;
	}

	va_start(args, active);
	gaim_status_set_active_with_attrs(status, active, args);
	va_end(args);
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

	/* TODO: The first way is better... but it doesn't work quite right yet */
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
gaim_account_get_enabled(const GaimAccount *account, const char *ui)
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

		cnode = gnode->child;
		while (cnode) {
			GaimBlistNode *cnode_next = cnode->next;

			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				bnode = cnode->child;
				while (bnode) {
					GaimBlistNode *bnode_next = bnode->next;

					if (GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
						GaimBuddy *b = (GaimBuddy *)bnode;

						if (b->account == account)
							gaim_blist_remove_buddy(b);
					}
					bnode = bnode_next;
				}
			} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
				GaimChat *c = (GaimChat *)cnode;

				if (c->account == account)
					gaim_blist_remove_chat(c);
			}
			cnode = cnode_next;
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

		/* TODO: Shouldn't be be using some sort of saved status here? */
		if (gaim_account_get_enabled(account, ui))
			gaim_account_connect(account, gaim_account_get_status(account, "online"));
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
		gaim_debug_error("accounts",
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

	load_accounts();

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
	if (save_timer != 0)
	{
		gaim_timeout_remove(save_timer);
		save_timer = 0;
		sync_accounts();
	}

	gaim_signals_unregister_by_instance(gaim_accounts_get_handle());
}
