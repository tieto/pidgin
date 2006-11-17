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
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "privacy.h"
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
status_attr_to_xmlnode(const GaimStatus *status, const GaimStatusType *type, const GaimStatusAttr *attr)
{
	xmlnode *node;
	const char *id;
	char *value = NULL;
	GaimStatusAttr *default_attr;
	GaimValue *default_value;
	GaimType attr_type;
	GaimValue *attr_value;

	id = gaim_status_attr_get_id(attr);
	g_return_val_if_fail(id, NULL);

	attr_value = gaim_status_get_attr_value(status, id);
	g_return_val_if_fail(attr_value, NULL);
	attr_type = gaim_value_get_type(attr_value);

	/*
	 * If attr_value is a different type than it should be
	 * then don't write it to the file.
	 */
	default_attr = gaim_status_type_get_attr(type, id);
	default_value = gaim_status_attr_get_value(default_attr);
	if (attr_type != gaim_value_get_type(default_value))
		return NULL;

	/*
	 * If attr_value is the same as the default for this status
	 * then there is no need to write it to the file.
	 */
	if (attr_type == GAIM_TYPE_STRING)
	{
		const char *string_value = gaim_value_get_string(attr_value);
		const char *default_string_value = gaim_value_get_string(default_value);
		if (((string_value == NULL) && (default_string_value == NULL)) ||
			((string_value != NULL) && (default_string_value != NULL) &&
			 !strcmp(string_value, default_string_value)))
			return NULL;
		value = g_strdup(gaim_value_get_string(attr_value));
	}
	else if (attr_type == GAIM_TYPE_INT)
	{
		int int_value = gaim_value_get_int(attr_value);
		if (int_value == gaim_value_get_int(default_value))
			return NULL;
		value = g_strdup_printf("%d", int_value);
	}
	else if (attr_type == GAIM_TYPE_BOOLEAN)
	{
		gboolean boolean_value = gaim_value_get_boolean(attr_value);
		if (boolean_value == gaim_value_get_boolean(default_value))
			return NULL;
		value = g_strdup(boolean_value ?
								"true" : "false");
	}
	else
	{
		return NULL;
	}

	g_return_val_if_fail(value, NULL);

	node = xmlnode_new("attribute");

	xmlnode_set_attrib(node, "id", id);
	xmlnode_set_attrib(node, "value", value);

	g_free(value);

	return node;
}

static xmlnode *
status_attrs_to_xmlnode(const GaimStatus *status)
{
	GaimStatusType *type = gaim_status_get_type(status);
	xmlnode *node, *child;
	const GList *attrs, *attr;

	node = xmlnode_new("attributes");

	attrs = gaim_status_type_get_attrs(type);
	for (attr = attrs; attr != NULL; attr = attr->next)
	{
		child = status_attr_to_xmlnode(status, type, (const GaimStatusAttr *)attr->data);
		if (child)
			xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
status_to_xmlnode(const GaimStatus *status)
{
	xmlnode *node, *child;

	node = xmlnode_new("status");
	xmlnode_set_attrib(node, "type", gaim_status_get_id(status));
	if (gaim_status_get_name(status) != NULL)
		xmlnode_set_attrib(node, "name", gaim_status_get_name(status));
	xmlnode_set_attrib(node, "active", gaim_status_is_active(status) ? "true" : "false");

	child = status_attrs_to_xmlnode(status);
	xmlnode_insert_child(node, child);

	return node;
}

static xmlnode *
statuses_to_xmlnode(const GaimPresence *presence)
{
	xmlnode *node, *child;
	const GList *statuses, *status;

	node = xmlnode_new("statuses");

	statuses = gaim_presence_get_statuses(presence);
	for (status = statuses; status != NULL; status = status->next)
	{
		child = status_to_xmlnode((GaimStatus *)status->data);
		xmlnode_insert_child(node, child);
	}

	return node;
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

	return node;
}

static xmlnode *
account_to_xmlnode(GaimAccount *account)
{
	xmlnode *node, *child;
	const char *tmp;
	GaimPresence *presence;
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

	if ((presence = gaim_account_get_presence(account)) != NULL)
	{
		child = statuses_to_xmlnode(presence);
		xmlnode_insert_child(node, child);
	}

	if ((tmp = gaim_account_get_user_info(account)) != NULL)
	{
		/* TODO: Do we need to call gaim_str_strip_char(tmp, '\r') here? */
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

	node = xmlnode_new("account");
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
		gaim_debug_error("account", "Attempted to save accounts before "
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

static GList *
parse_status_attrs(xmlnode *node, GaimStatus *status)
{
	GList *list = NULL;
	xmlnode *child;
	GaimValue *attr_value;

	for (child = xmlnode_get_child(node, "attribute"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		const char *id = xmlnode_get_attrib(child, "id");
		const char *value = xmlnode_get_attrib(child, "value");

		if (!id || !*id || !value || !*value)
			continue;

		attr_value = gaim_status_get_attr_value(status, id);
		if (!attr_value)
			continue;

		list = g_list_append(list, (char *)id);

		switch (gaim_value_get_type(attr_value))
		{
			case GAIM_TYPE_STRING:
				list = g_list_append(list, (char *)value);
				break;
			case GAIM_TYPE_INT:
			case GAIM_TYPE_BOOLEAN:
			{
				int v;
				if (sscanf(value, "%d", &v) == 1)
					list = g_list_append(list, GINT_TO_POINTER(v));
				else
					list = g_list_remove(list, id);
				break;
			}
			default:
				break;
		}
	}

	return list;
}

static void
parse_status(xmlnode *node, GaimAccount *account)
{
	gboolean active = FALSE;
	const char *data;
	const char *type;
	xmlnode *child;
	GList *attrs = NULL;

	/* Get the active/inactive state */
	data = xmlnode_get_attrib(node, "active");
	if (data == NULL)
		return;
	if (strcasecmp(data, "true") == 0)
		active = TRUE;
	else if (strcasecmp(data, "false") == 0)
		active = FALSE;
	else
		return;

	/* Get the type of the status */
	type = xmlnode_get_attrib(node, "type");
	if (type == NULL)
		return;

	/* Read attributes into a GList */
	child = xmlnode_get_child(node, "attributes");
	if (child != NULL)
	{
		attrs = parse_status_attrs(child,
						gaim_account_get_status(account, type));
	}

	gaim_account_set_status_list(account, type, active, attrs);

	g_list_free(attrs);
}

static void
parse_statuses(xmlnode *node, GaimAccount *account)
{
	xmlnode *child;

	for (child = xmlnode_get_child(node, "status"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		parse_status(child, account);
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

	/* If there are no values set then proxy_info NULL */
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
		g_free(protocol_id);
		g_free(name);
		return NULL;
	}

	ret = gaim_account_new(name, protocol_id);
	g_free(name);
	g_free(protocol_id);

	/* Read the password */
	child = xmlnode_get_child(node, "password");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		gaim_account_set_remember_password(ret, TRUE);
		gaim_account_set_password(ret, data);
		g_free(data);
	}

	/* Read the alias */
	child = xmlnode_get_child(node, "alias");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		if (*data != '\0')
			gaim_account_set_alias(ret, data);
		g_free(data);
	}

	/* Read the statuses */
	child = xmlnode_get_child(node, "statuses");
	if (child != NULL)
	{
		parse_statuses(child, ret);
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
		GaimAccount *new_acct;
		new_acct = parse_account(child);
		gaim_accounts_add(new_acct);
	}

	xmlnode_free(node);
}


static void
delete_setting(void *data)
{
	GaimAccountSetting *setting = (GaimAccountSetting *)data;

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
	GaimStatusType *status_type;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	account = gaim_accounts_find(username, protocol_id);

	if (account != NULL)
		return account;

	account = g_new0(GaimAccount, 1);
	GAIM_DBUS_REGISTER_POINTER(account, GaimAccount);

	gaim_account_set_username(account, username);

	gaim_account_set_protocol_id(account, protocol_id);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
	account->ui_settings = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, (GDestroyNotify)g_hash_table_destroy);
	account->system_log = NULL;
	/* 0 is not a valid privacy setting */
	account->perm_deny = GAIM_PRIVACY_ALLOW_ALL;

	account->presence = gaim_presence_new_for_account(account);

	prpl = gaim_find_prpl(protocol_id);

	if (prpl == NULL)
		return account;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info != NULL && prpl_info->status_types != NULL)
		gaim_account_set_status_types(account, prpl_info->status_types(account));

	status_type = gaim_account_get_status_type_with_primitive(account, GAIM_STATUS_AVAILABLE);
	if (status_type != NULL)
		gaim_presence_set_status_active(account->presence,
										gaim_status_type_get_id(status_type),
										TRUE);
	else
		gaim_presence_set_status_active(account->presence,
										"offline",
										TRUE);

	return account;
}

void
gaim_account_destroy(GaimAccount *account)
{
	GList *l;

	g_return_if_fail(account != NULL);

	gaim_debug_info("account", "Destroying account %p\n", account);

	for (l = gaim_get_conversations(); l != NULL; l = l->next)
	{
		GaimConversation *conv = (GaimConversation *)l->data;

		if (gaim_conversation_get_account(conv) == account)
			gaim_conversation_set_account(conv, NULL);
	}

	g_free(account->username);
	g_free(account->alias);
	g_free(account->password);
	g_free(account->user_info);
	g_free(account->protocol_id);

	g_hash_table_destroy(account->settings);
	g_hash_table_destroy(account->ui_settings);

	gaim_account_set_status_types(account, NULL);

	gaim_presence_destroy(account->presence);

	if(account->system_log)
		gaim_log_free(account->system_log);

	GAIM_DBUS_UNREGISTER_POINTER(account);
	g_free(account);
}

void
gaim_account_register(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	gaim_debug_info("account", "Registering account %s\n",
					gaim_account_get_username(account));

	gaim_connection_new(account, TRUE, NULL);
}

static void
request_password_ok_cb(GaimAccount *account, GaimRequestFields *fields)
{
	const char *entry;
	gboolean remember;

	entry = gaim_request_fields_get_string(fields, "password");
	remember = gaim_request_fields_get_bool(fields, "remember");

	if (!entry || !*entry)
	{
		gaim_notify_error(account, NULL, _("Password is required to sign on."), NULL);
		return;
	}

	if(remember)
	  gaim_account_set_remember_password(account, TRUE);

	gaim_account_set_password(account, entry);

	gaim_connection_new(account, FALSE, entry);
}

static void
request_password(GaimAccount *account)
{
	gchar *primary;
	const gchar *username;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	GaimRequestFields *fields;

	/* Close any previous password request windows */
	gaim_request_close_with_handle(account);

	username = gaim_account_get_username(account);
	primary = g_strdup_printf(_("Enter password for %s (%s)"), username,
								  gaim_account_get_protocol_name(account));

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("password", _("Enter Password"), NULL, FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_bool_new("remember", _("Save password"), FALSE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(account,
                        NULL,
                        primary,
                        NULL,
                        fields,
                        _("OK"), G_CALLBACK(request_password_ok_cb),
                        _("Cancel"), NULL,
                        account);
	g_free(primary);
}

void
gaim_account_connect(GaimAccount *account)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	const char *password;

	g_return_if_fail(account != NULL);

	gaim_debug_info("account", "Connecting to account %s\n",
					gaim_account_get_username(account));

	if (!gaim_account_get_enabled(account, gaim_core_get_ui()))
		return;

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	if (prpl == NULL)
	{
		gchar *message;

		message = g_strdup_printf(_("Missing protocol plugin for %s"),
			gaim_account_get_username(account));
		gaim_notify_error(account, _("Connection Error"), message, NULL);
		g_free(message);
		return;
	}

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	password = gaim_account_get_password(account);
	if ((password == NULL) &&
		!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
		!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		request_password(account);
	else
		gaim_connection_new(account, FALSE, password);
}

void
gaim_account_disconnect(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_if_fail(account != NULL);
	g_return_if_fail(!gaim_account_is_disconnected(account));

	gaim_debug_info("account", "Disconnecting account %p\n", account);

	account->disconnecting = TRUE;

	gc = gaim_account_get_connection(account);
	gaim_connection_destroy(gc);
	if (!gaim_account_get_remember_password(account))
		gaim_account_set_password(account, NULL);
	gaim_account_set_connection(account, NULL);

	account->disconnecting = FALSE;
}

void
gaim_account_notify_added(GaimAccount *account, const char *remote_user,
                          const char *id, const char *alias,
                          const char *message)
{
	GaimAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = gaim_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->notify_added != NULL)
		ui_ops->notify_added(account, remote_user, id, alias, message);
}

void
gaim_account_request_add(GaimAccount *account, const char *remote_user,
                         const char *id, const char *alias,
                         const char *message)
{
	GaimAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = gaim_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add != NULL)
		ui_ops->request_add(account, remote_user, id, alias, message);
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
		gaim_notify_error(account, NULL,
						  _("New passwords do not match."), NULL);

		return;
	}

	if (orig_pass == NULL || new_pass_1 == NULL || new_pass_2 == NULL ||
		*orig_pass == '\0' || *new_pass_1 == '\0' || *new_pass_2 == '\0')
	{
		gaim_notify_error(account, NULL,
						  _("Fill out all fields completely."), NULL);
		return;
	}

	gaim_account_change_password(account, orig_pass, new_pass_1);
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
	serv_set_info(gc, user_info);
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

	gaim_request_input(gc, _("Set User Info"), primary, NULL,
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

	g_free(account->username);
	account->username = g_strdup(username);

	schedule_accounts_save();
}

void
gaim_account_set_password(GaimAccount *account, const char *password)
{
	g_return_if_fail(account != NULL);

	g_free(account->password);
	account->password = g_strdup(password);

	schedule_accounts_save();
}

void
gaim_account_set_alias(GaimAccount *account, const char *alias)
{
	g_return_if_fail(account != NULL);

	/*
	 * Do nothing if alias and account->alias are both NULL.  Or if
	 * they're the exact same string.
	 */
	if (alias == account->alias)
		return;

	if ((!alias && account->alias) || (alias && !account->alias) ||
			g_utf8_collate(account->alias, alias))
	{
		char *old = account->alias;

		account->alias = g_strdup(alias);
		gaim_signal_emit(gaim_accounts_get_handle(), "account-alias-changed",
						 account, old);
		g_free(old);

		schedule_accounts_save();
	}
}

void
gaim_account_set_user_info(GaimAccount *account, const char *user_info)
{
	g_return_if_fail(account != NULL);

	g_free(account->user_info);
	account->user_info = g_strdup(user_info);

	schedule_accounts_save();
}

void
gaim_account_set_buddy_icon(GaimAccount *account, const char *icon)
{
	g_return_if_fail(account != NULL);

	/* Delete an existing icon from the cache. */
	if (account->buddy_icon != NULL && (icon == NULL || strcmp(account->buddy_icon, icon)))
	{
		const char *dirname = gaim_buddy_icons_get_cache_dir();
		struct stat st;

		if (g_stat(account->buddy_icon, &st) == 0)
		{
			/* The file exists. This is a full path. */

			/* XXX: This is a hack so we only delete the file if it's
			 * in the cache dir. Otherwise, people who upgrade (who
			 * may have buddy icon filenames set outside of the cache
			 * dir) could lose files. */
			if (!strncmp(dirname, account->buddy_icon, strlen(dirname)))
				g_unlink(account->buddy_icon);
		}
		else
		{
			char *filename = g_build_filename(dirname, account->buddy_icon, NULL);
			g_unlink(filename);
			g_free(filename);
		}
	}

	g_free(account->buddy_icon);
	account->buddy_icon = g_strdup(icon);
	if (gaim_account_is_connected(account))
	{
		GaimConnection *gc;
		GaimPluginProtocolInfo *prpl_info;

		gc = gaim_account_get_connection(account);
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (prpl_info && prpl_info->set_buddy_icon)
		{
			char *filename = gaim_buddy_icons_get_full_path(icon);
			prpl_info->set_buddy_icon(gc, filename);
			g_free(filename);
		}
	}

	schedule_accounts_save();
}

void
gaim_account_set_protocol_id(GaimAccount *account, const char *protocol_id)
{
	g_return_if_fail(account     != NULL);
	g_return_if_fail(protocol_id != NULL);

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
	GaimConnection *gc;
	gboolean was_enabled = FALSE;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);

	was_enabled = gaim_account_get_enabled(account, ui);

	gaim_account_set_ui_bool(account, ui, "auto-login", value);
	gc = gaim_account_get_connection(account);

	if(was_enabled && !value)
		gaim_signal_emit(gaim_accounts_get_handle(), "account-disabled", account);
	else if(!was_enabled && value)
		gaim_signal_emit(gaim_accounts_get_handle(), "account-enabled", account);

	if ((gc != NULL) && (gc->wants_to_die == TRUE))
		return;

	if (value && gaim_presence_is_online(account->presence))
		gaim_account_connect(account);
	else if (!value && !gaim_account_is_disconnected(account))
		gaim_account_disconnect(account);
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

	/* Out with the old... */
	if (account->status_types != NULL)
	{
		g_list_foreach(account->status_types, (GFunc)gaim_status_type_destroy, NULL);
		g_list_free(account->status_types);
	}

	/* In with the new... */
	account->status_types = status_types;
}

void
gaim_account_set_status(GaimAccount *account, const char *status_id,
						gboolean active, ...)
{
	GList *attrs = NULL;
	const gchar *id;
	gpointer data;
	va_list args;

	va_start(args, active);
	while ((id = va_arg(args, const char *)) != NULL)
	{
		attrs = g_list_append(attrs, (char *)id);
		data = va_arg(args, void *);
		attrs = g_list_append(attrs, data);
	}
	gaim_account_set_status_list(account, status_id, active, attrs);
	g_list_free(attrs);
	va_end(args);
}

void
gaim_account_set_status_list(GaimAccount *account, const char *status_id,
							 gboolean active, GList *attrs)
{
	GaimStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);

	status = gaim_account_get_status(account, status_id);
	if (status == NULL)
	{
		gaim_debug_error("account",
				   "Invalid status ID %s for account %s (%s)\n",
				   status_id, gaim_account_get_username(account),
				   gaim_account_get_protocol_id(account));
		return;
	}

	if (active || gaim_status_is_independent(status))
		gaim_status_set_active_with_attrs_list(status, active, attrs);

	/*
	 * Our current statuses are saved to accounts.xml (so that when we
	 * reconnect, we go back to the previous status).
	 */
	schedule_accounts_save();
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

static GaimConnectionState
gaim_account_get_state(const GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, GAIM_DISCONNECTED);

	gc = gaim_account_get_connection(account);
	if (!gc)
		return GAIM_DISCONNECTED;

	return gaim_connection_get_state(gc);
}

gboolean
gaim_account_is_connected(const GaimAccount *account)
{
	return (gaim_account_get_state(account) == GAIM_CONNECTED);
}

gboolean
gaim_account_is_connecting(const GaimAccount *account)
{
	return (gaim_account_get_state(account) == GAIM_CONNECTING);
}

gboolean
gaim_account_is_disconnected(const GaimAccount *account)
{
	return (gaim_account_get_state(account) == GAIM_DISCONNECTED);
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
	/*
	 * HACK by Seanegan
	 */
	if (!strcmp(account->protocol_id, "prpl-oscar")) {
		if (isdigit(account->username[0]))
			return "prpl-icq";
		else
			return "prpl-aim";
	}
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
gaim_account_get_active_status(const GaimAccount *account)
{
	g_return_val_if_fail(account   != NULL, NULL);

	return gaim_presence_get_active_status(account->presence);
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

GaimStatusType *
gaim_account_get_status_type_with_primitive(const GaimAccount *account, GaimStatusPrimitive primitive)
{
	const GList *l;

	g_return_val_if_fail(account != NULL, NULL);

	for (l = gaim_account_get_status_types(account); l != NULL; l = l->next)
	{
		GaimStatusType *status_type = (GaimStatusType *)l->data;

		if (gaim_status_type_get_primitive(status_type) == primitive)
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
gaim_account_get_log(GaimAccount *account, gboolean create)
{
	g_return_val_if_fail(account != NULL, NULL);

	if(!account->system_log && create){
		GaimPresence *presence;
		int login_time;

		presence = gaim_account_get_presence(account);
		login_time = gaim_presence_get_login_time(presence);

		account->system_log	 = gaim_log_new(GAIM_LOG_SYSTEM,
				gaim_account_get_username(account), account, NULL,
				(login_time != 0) ? login_time : time(NULL), NULL);
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
gaim_account_add_buddy(GaimAccount *account, GaimBuddy *buddy)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info != NULL && prpl_info->add_buddy != NULL)
		prpl_info->add_buddy(gc, buddy, gaim_buddy_get_group(buddy));
}

void
gaim_account_add_buddies(GaimAccount *account, GList *buddies)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info) {
		GList *cur, *groups = NULL;

		/* Make a list of what group each buddy is in */
		for (cur = buddies; cur != NULL; cur = cur->next) {
			GaimBlistNode *node = cur->data;
			groups = g_list_append(groups, node->parent->parent);
		}

		if (prpl_info->add_buddies != NULL)
			prpl_info->add_buddies(gc, buddies, groups);
		else if (prpl_info->add_buddy != NULL) {
			GList *curb = buddies, *curg = groups;

			while ((curb != NULL) && (curg != NULL)) {
				prpl_info->add_buddy(gc, curb->data, curg->data);
				curb = curb->next;
				curg = curg->next;
			}
		}

		g_list_free(groups);
	}
}

void
gaim_account_remove_buddy(GaimAccount *account, GaimBuddy *buddy,
		GaimGroup *group)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->remove_buddy)
		prpl_info->remove_buddy(gc, buddy, group);
}

void
gaim_account_remove_buddies(GaimAccount *account, GList *buddies, GList *groups)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info) {
		if (prpl_info->remove_buddies)
			prpl_info->remove_buddies(gc, buddies, groups);
		else {
			GList *curb = buddies;
			GList *curg = groups;
			while ((curb != NULL) && (curg != NULL)) {
				gaim_account_remove_buddy(account, curb->data, curg->data);
				curb = curb->next;
				curg = curg->next;
			}
		}
	}
}

void
gaim_account_remove_group(GaimAccount *account, GaimGroup *group)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->remove_group)
		prpl_info->remove_group(gc, group);
}

void
gaim_account_change_password(GaimAccount *account, const char *orig_pw,
		const char *new_pw)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = gaim_account_get_connection(account);

	gaim_account_set_password(account, new_pw);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->change_passwd)
		prpl_info->change_passwd(gc, orig_pw, new_pw);
}

gboolean gaim_account_supports_offline_message(GaimAccount *account, GaimBuddy *buddy)
{
	GaimConnection *gc;
	GaimPluginProtocolInfo *prpl_info;

	g_return_val_if_fail(account, FALSE);
	g_return_val_if_fail(buddy, FALSE);

	gc = gaim_account_get_connection(account);
	if (gc == NULL)
		return FALSE;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (!prpl_info || !prpl_info->offline_message)
		return FALSE;
	return prpl_info->offline_message(buddy);
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

	/*
	 * Disable the account before blowing it out of the water.
	 * Conceptually it probably makes more sense to disable the
	 * account for all UIs rather than the just the current UI,
	 * but it doesn't really matter.
	 */
	gaim_account_set_enabled(account, gaim_core_get_ui(), FALSE);

	gaim_notify_close_with_handle(account);
	gaim_request_close_with_handle(account);

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

	/* This will cause the deletion of an old buddy icon. */
	gaim_account_set_buddy_icon(account, NULL);

	gaim_account_destroy(account);
}

void
gaim_accounts_reorder(GaimAccount *account, gint new_index)
{
	gint index;
	GList *l;

	g_return_if_fail(account != NULL);
	g_return_if_fail(new_index <= g_list_length(accounts));

	index = g_list_index(accounts, account);

	if (index == -1) {
		gaim_debug_error("account",
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

GList *
gaim_accounts_get_all_active(void)
{
	GList *list = NULL;
	GList *all = gaim_accounts_get_all();

	while (all != NULL) {
		GaimAccount *account = all->data;

		if (gaim_account_get_enabled(account, gaim_core_get_ui()))
			list = g_list_append(list, account);

		all = all->next;
	}

	return list;
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
gaim_accounts_restore_current_statuses()
{
	GList *l;
	GaimAccount *account;

	/* If we're not connected to the Internet right now, we bail on this */
	if (!gaim_network_is_available())
	{
		gaim_debug_info("account", "Network not connected; skipping reconnect\n");
		return;
	}

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
	{
		account = (GaimAccount *)l->data;
		if (gaim_account_get_enabled(account, gaim_core_get_ui()) &&
			(gaim_presence_is_online(account->presence)))
		{
			gaim_account_connect(account);
		}
	}
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

	gaim_signal_register(handle, "account-disabled",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT));

	gaim_signal_register(handle, "account-enabled",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT));

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

	gaim_signal_register(handle, "account-added",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_ACCOUNT));

	gaim_signal_register(handle, "account-removed",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_ACCOUNT));

	gaim_signal_register(handle, "account-status-changed",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_STATUS),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_STATUS));

	gaim_signal_register(handle, "account-alias-changed",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
							 			GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));
	
	load_accounts();

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
