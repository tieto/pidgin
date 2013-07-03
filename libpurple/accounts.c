/**
 * @file accounts.c Accounts API
 * @ingroup core
 */

/* purple
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
#include "internal.h"
#include "accounts.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "network.h"
#include "pounce.h"

static PurpleAccountUiOps *account_ui_ops = NULL;

static GList   *accounts = NULL;
static guint    save_timer = 0;
static gboolean accounts_loaded = FALSE;

/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/
static xmlnode *
accounts_to_xmlnode(void)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("account");
	xmlnode_set_attrib(node, "version", "1.0");

	for (cur = purple_accounts_get_all(); cur != NULL; cur = cur->next)
	{
		child = purple_account_to_xmlnode(cur->data);
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
		purple_debug_error("account", "Attempted to save accounts before "
						 "they were read!\n");
		return;
	}

	node = accounts_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	purple_util_write_data_to_file("accounts.xml", data, -1);
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

void
purple_accounts_schedule_save(void)
{
	if (save_timer == 0)
		save_timer = purple_timeout_add_seconds(5, save_cb, NULL);
}

/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/
static void
migrate_yahoo_japan(PurpleAccount *account)
{
	/* detect a Yahoo! JAPAN account that existed prior to 2.6.0 and convert it
	 * to use the new prpl-yahoojp.  Also remove the account-specific settings
	 * we no longer need */

	if(purple_strequal(purple_account_get_protocol_id(account), "prpl-yahoo")) {
		if(purple_account_get_bool(account, "yahoojp", FALSE)) {
			const char *serverjp = purple_account_get_string(account, "serverjp", NULL);
			const char *xferjp_host = purple_account_get_string(account, "xferjp_host", NULL);

			g_return_if_fail(serverjp != NULL);
			g_return_if_fail(xferjp_host != NULL);

			purple_account_set_string(account, "server", serverjp);
			purple_account_set_string(account, "xfer_host", xferjp_host);

			purple_account_set_protocol_id(account, "prpl-yahoojp");
		}

		/* these should always be nuked */
		purple_account_remove_setting(account, "yahoojp");
		purple_account_remove_setting(account, "serverjp");
		purple_account_remove_setting(account, "xferjp_host");

	}
}

static void
migrate_icq_server(PurpleAccount *account)
{
	/* Migrate the login server setting for ICQ accounts.  See
	 * 'mtn log --last 1 --no-graph --from b6d7712e90b68610df3bd2d8cbaf46d94c8b3794'
	 * for details on the change. */

	if(purple_strequal(purple_account_get_protocol_id(account), "prpl-icq")) {
		const char *tmp = purple_account_get_string(account, "server", NULL);

		/* Non-secure server */
		if(purple_strequal(tmp,	"login.messaging.aol.com") ||
				purple_strequal(tmp, "login.oscar.aol.com"))
			purple_account_set_string(account, "server", "login.icq.com");

		/* Secure server */
		if(purple_strequal(tmp, "slogin.oscar.aol.com"))
			purple_account_set_string(account, "server", "slogin.icq.com");
	}
}

static void
migrate_xmpp_encryption(PurpleAccount *account)
{
	/* When this is removed, nuke the "old_ssl" and "require_tls" settings */
	if (g_str_equal(purple_account_get_protocol_id(account), "prpl-jabber")) {
		const char *sec = purple_account_get_string(account, "connection_security", "");

		if (g_str_equal("", sec)) {
			const char *val = "require_tls";
			if (purple_account_get_bool(account, "old_ssl", FALSE))
				val = "old_ssl";
			else if (!purple_account_get_bool(account, "require_tls", TRUE))
				val = "opportunistic_tls";

			purple_account_set_string(account, "connection_security", val);
		}
	}
}

static void
parse_settings(xmlnode *node, PurpleAccount *account)
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
		PurplePrefType type;
		char *data;

		name = xmlnode_get_attrib(child, "name");
		if (name == NULL)
			/* Ignore this setting */
			continue;

		str_type = xmlnode_get_attrib(child, "type");
		if (str_type == NULL)
			/* Ignore this setting */
			continue;

		if (purple_strequal(str_type, "string"))
			type = PURPLE_PREF_STRING;
		else if (purple_strequal(str_type, "int"))
			type = PURPLE_PREF_INT;
		else if (purple_strequal(str_type, "bool"))
			type = PURPLE_PREF_BOOLEAN;
		else
			/* Ignore this setting */
			continue;

		data = xmlnode_get_data(child);
		if (data == NULL)
			/* Ignore this setting */
			continue;

		if (ui == NULL)
		{
			if (type == PURPLE_PREF_STRING)
				purple_account_set_string(account, name, data);
			else if (type == PURPLE_PREF_INT)
				purple_account_set_int(account, name, atoi(data));
			else if (type == PURPLE_PREF_BOOLEAN)
				purple_account_set_bool(account, name,
									  (*data == '0' ? FALSE : TRUE));
		} else {
			if (type == PURPLE_PREF_STRING)
				purple_account_set_ui_string(account, ui, name, data);
			else if (type == PURPLE_PREF_INT)
				purple_account_set_ui_int(account, ui, name, atoi(data));
			else if (type == PURPLE_PREF_BOOLEAN)
				purple_account_set_ui_bool(account, ui, name,
										 (*data == '0' ? FALSE : TRUE));
		}

		g_free(data);
	}

	/* we do this here because we need access to account settings to determine
	 * if we can/should migrate an old Yahoo! JAPAN account */
	migrate_yahoo_japan(account);
	/* we do this here because we need access to account settings to determine
	 * if we can/should migrate an ICQ account's server setting */
	migrate_icq_server(account);
	/* we do this here because we need to do it before the user views the
	 * Edit Account dialog. */
	migrate_xmpp_encryption(account);
}

static GList *
parse_status_attrs(xmlnode *node, PurpleStatus *status)
{
	GList *list = NULL;
	xmlnode *child;
	PurpleValue *attr_value;

	for (child = xmlnode_get_child(node, "attribute"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		const char *id = xmlnode_get_attrib(child, "id");
		const char *value = xmlnode_get_attrib(child, "value");

		if (!id || !*id || !value || !*value)
			continue;

		attr_value = purple_status_get_attr_value(status, id);
		if (!attr_value)
			continue;

		list = g_list_append(list, (char *)id);

		switch (purple_value_get_type(attr_value))
		{
			case PURPLE_TYPE_STRING:
				list = g_list_append(list, (char *)value);
				break;
			case PURPLE_TYPE_INT:
			case PURPLE_TYPE_BOOLEAN:
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
parse_status(xmlnode *node, PurpleAccount *account)
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
	if (g_ascii_strcasecmp(data, "true") == 0)
		active = TRUE;
	else if (g_ascii_strcasecmp(data, "false") == 0)
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
						purple_account_get_status(account, type));
	}

	purple_account_set_status_list(account, type, active, attrs);

	g_list_free(attrs);
}

static void
parse_statuses(xmlnode *node, PurpleAccount *account)
{
	xmlnode *child;

	for (child = xmlnode_get_child(node, "status"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		parse_status(child, account);
	}
}

static void
parse_proxy_info(xmlnode *node, PurpleAccount *account)
{
	PurpleProxyInfo *proxy_info;
	xmlnode *child;
	char *data;

	proxy_info = purple_proxy_info_new();

	/* Use the global proxy settings, by default */
	purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_USE_GLOBAL);

	/* Read proxy type */
	child = xmlnode_get_child(node, "type");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		if (purple_strequal(data, "global"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_USE_GLOBAL);
		else if (purple_strequal(data, "none"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_NONE);
		else if (purple_strequal(data, "http"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_HTTP);
		else if (purple_strequal(data, "socks4"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_SOCKS4);
		else if (purple_strequal(data, "socks5"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_SOCKS5);
		else if (purple_strequal(data, "tor"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_TOR);
		else if (purple_strequal(data, "envvar"))
			purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_USE_ENVVAR);
		else
		{
			purple_debug_error("account", "Invalid proxy type found when "
							 "loading account information for %s\n",
							 purple_account_get_username(account));
		}
		g_free(data);
	}

	/* Read proxy host */
	child = xmlnode_get_child(node, "host");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		purple_proxy_info_set_host(proxy_info, data);
		g_free(data);
	}

	/* Read proxy port */
	child = xmlnode_get_child(node, "port");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		purple_proxy_info_set_port(proxy_info, atoi(data));
		g_free(data);
	}

	/* Read proxy username */
	child = xmlnode_get_child(node, "username");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		purple_proxy_info_set_username(proxy_info, data);
		g_free(data);
	}

	/* Read proxy password */
	child = xmlnode_get_child(node, "password");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		purple_proxy_info_set_password(proxy_info, data);
		g_free(data);
	}

	/* If there are no values set then proxy_info NULL */
	if ((purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL) &&
		(purple_proxy_info_get_host(proxy_info) == NULL) &&
		(purple_proxy_info_get_port(proxy_info) == 0) &&
		(purple_proxy_info_get_username(proxy_info) == NULL) &&
		(purple_proxy_info_get_password(proxy_info) == NULL))
	{
		purple_proxy_info_destroy(proxy_info);
		return;
	}

	purple_account_set_proxy_info(account, proxy_info);
}

static void
parse_current_error(xmlnode *node, PurpleAccount *account)
{
	guint type;
	char *type_str = NULL, *description = NULL;
	xmlnode *child;
	PurpleConnectionErrorInfo *current_error = NULL;

	child = xmlnode_get_child(node, "type");
	if (child == NULL || (type_str = xmlnode_get_data(child)) == NULL)
		return;
	type = atoi(type_str);
	g_free(type_str);

	if (type > PURPLE_CONNECTION_ERROR_OTHER_ERROR)
	{
		purple_debug_error("account",
			"Invalid PurpleConnectionError value %d found when "
			"loading account information for %s\n",
			type, purple_account_get_username(account));
		type = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
	}

	child = xmlnode_get_child(node, "description");
	if (child)
		description = xmlnode_get_data(child);
	if (description == NULL)
		description = g_strdup("");

	current_error = g_new0(PurpleConnectionErrorInfo, 1);
	PURPLE_DBUS_REGISTER_POINTER(current_error, PurpleConnectionErrorInfo);
	current_error->type = type;
	current_error->description = description;

	purple_account_set_current_error(account, current_error);
}

static PurpleAccount *
parse_account(xmlnode *node)
{
	PurpleAccount *ret;
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

	ret = purple_account_new(name, protocol_id);
	g_free(name);
	g_free(protocol_id);

	/* Read the alias */
	child = xmlnode_get_child(node, "alias");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		if (*data != '\0')
			purple_account_set_private_alias(ret, data);
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
		purple_account_set_user_info(ret, data);
		g_free(data);
	}

	/* Read an old buddyicon */
	child = xmlnode_get_child(node, "buddyicon");
	if ((child != NULL) && ((data = xmlnode_get_data(child)) != NULL))
	{
		const char *dirname = purple_buddy_icons_get_cache_dir();
		char *filename = g_build_filename(dirname, data, NULL);
		gchar *contents;
		gsize len;

		if (g_file_get_contents(filename, &contents, &len, NULL))
		{
			purple_buddy_icons_set_account_icon(ret, (guchar *)contents, len);
		}

		g_free(filename);
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

	/* Read current error */
	child = xmlnode_get_child(node, "current_error");
	if (child != NULL)
	{
		parse_current_error(child, ret);
	}

	/* Read the password */
	child = xmlnode_get_child(node, "password");
	if (child != NULL)
	{
		const char *keyring_id = xmlnode_get_attrib(child, "keyring_id");
		const char *mode = xmlnode_get_attrib(child, "mode");
		gboolean result;

		data = xmlnode_get_data(child);
		result = purple_keyring_import_password(ret, keyring_id, mode, data, NULL);

		if (result == TRUE || purple_keyring_get_inuse() == NULL) {
			purple_account_set_remember_password(ret, TRUE);
		} else {
			purple_debug_error("account", "Failed to import password.\n");
		} 
		purple_str_wipe(data);
	}

	return ret;
}

static void
load_accounts(void)
{
	xmlnode *node, *child;

	accounts_loaded = TRUE;

	node = purple_util_read_xml_from_file("accounts.xml", _("accounts"));

	if (node == NULL)
		return;

	for (child = xmlnode_get_child(node, "account"); child != NULL;
			child = xmlnode_get_next_twin(child))
	{
		PurpleAccount *new_acct;
		new_acct = parse_account(child);
		purple_accounts_add(new_acct);
	}

	xmlnode_free(node);

	_purple_buddy_icons_account_loaded_cb();
}

void
purple_accounts_add(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	if (g_list_find(accounts, account) != NULL)
		return;

	accounts = g_list_append(accounts, account);

	purple_accounts_schedule_save();

	purple_signal_emit(purple_accounts_get_handle(), "account-added", account);
}

void
purple_accounts_remove(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	accounts = g_list_remove(accounts, account);

	purple_accounts_schedule_save();

	/* Clearing the error ensures that account-error-changed is emitted,
	 * which is the end of the guarantee that the the error's pointer is
	 * valid.
	 */
	purple_account_clear_current_error(account);
	purple_signal_emit(purple_accounts_get_handle(), "account-removed", account);
}

static void
purple_accounts_delete_set(PurpleAccount *account, GError *error, gpointer data)
{
	g_object_unref(G_OBJECT(account));
}

void
purple_accounts_delete(PurpleAccount *account)
{
	PurpleBListNode *gnode, *cnode, *bnode;
	GList *iter;

	g_return_if_fail(account != NULL);

	/*
	 * Disable the account before blowing it out of the water.
	 * Conceptually it probably makes more sense to disable the
	 * account for all UIs rather than the just the current UI,
	 * but it doesn't really matter.
	 */
	purple_account_set_enabled(account, purple_core_get_ui(), FALSE);

	purple_notify_close_with_handle(account);
	purple_request_close_with_handle(account);

	purple_accounts_remove(account);

	/* Remove this account's buddies */
	for (gnode = purple_blist_get_root();
	     gnode != NULL;
		 gnode = purple_blist_node_get_sibling_next(gnode))
	{
		if (!PURPLE_IS_GROUP(gnode))
			continue;

		cnode = purple_blist_node_get_first_child(gnode);
		while (cnode) {
			PurpleBListNode *cnode_next = purple_blist_node_get_sibling_next(cnode);

			if(PURPLE_IS_CONTACT(cnode)) {
				bnode = purple_blist_node_get_first_child(cnode);
				while (bnode) {
					PurpleBListNode *bnode_next = purple_blist_node_get_sibling_next(bnode);

					if (PURPLE_IS_BUDDY(bnode)) {
						PurpleBuddy *b = (PurpleBuddy *)bnode;

						if (purple_buddy_get_account(b) == account)
							purple_blist_remove_buddy(b);
					}
					bnode = bnode_next;
				}
			} else if (PURPLE_IS_CHAT(cnode)) {
				PurpleChat *c = (PurpleChat *)cnode;

				if (purple_chat_get_account(c) == account)
					purple_blist_remove_chat(c);
			}
			cnode = cnode_next;
		}
	}

	/* Remove any open conversation for this account */
	for (iter = purple_conversations_get_all(); iter; ) {
		PurpleConversation *conv = iter->data;
		iter = iter->next;
		if (purple_conversation_get_account(conv) == account)
			g_object_unref(conv);
	}

	/* Remove this account's pounces */
	purple_pounce_destroy_all_by_account(account);

	/* This will cause the deletion of an old buddy icon. */
	purple_buddy_icons_set_account_icon(account, NULL, 0);

	/* This is async because we do not want the
	 * account being overwritten before we are done.
	 */
	purple_keyring_set_password(account, NULL,
		purple_accounts_delete_set, NULL);
}

void
purple_accounts_reorder(PurpleAccount *account, gint new_index)
{
	gint index;
	GList *l;

	g_return_if_fail(account != NULL);
	g_return_if_fail(new_index <= g_list_length(accounts));

	index = g_list_index(accounts, account);

	if (index == -1) {
		purple_debug_error("account",
				   "Unregistered account (%s) discovered during reorder!\n",
				   purple_account_get_username(account));
		return;
	}

	l = g_list_nth(accounts, index);

	if (new_index > index)
		new_index--;

	/* Remove the old one. */
	accounts = g_list_delete_link(accounts, l);

	/* Insert it where it should go. */
	accounts = g_list_insert(accounts, account, new_index);

	purple_accounts_schedule_save();
}

GList *
purple_accounts_get_all(void)
{
	return accounts;
}

GList *
purple_accounts_get_all_active(void)
{
	GList *list = NULL;
	GList *all = purple_accounts_get_all();

	while (all != NULL) {
		PurpleAccount *account = all->data;

		if (purple_account_get_enabled(account, purple_core_get_ui()))
			list = g_list_append(list, account);

		all = all->next;
	}

	return list;
}

PurpleAccount *
purple_accounts_find(const char *name, const char *protocol_id)
{
	PurpleAccount *account = NULL;
	GList *l;
	char *who;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
		account = (PurpleAccount *)l->data;

		if (!purple_strequal(purple_account_get_protocol_id(account), protocol_id))
			continue;

		who = g_strdup(purple_normalize(account, name));
		if (purple_strequal(purple_normalize(account, purple_account_get_username(account)), who)) {
			g_free(who);
			return account;
		}
		g_free(who);
	}

	return NULL;
}

void
purple_accounts_restore_current_statuses()
{
	GList *l;
	PurpleAccount *account;

	/* If we're not connected to the Internet right now, we bail on this */
	if (!purple_network_is_available())
	{
		purple_debug_warning("account", "Network not connected; skipping reconnect\n");
		return;
	}

	for (l = purple_accounts_get_all(); l != NULL; l = l->next)
	{
		account = (PurpleAccount *)l->data;

		if (purple_account_get_enabled(account, purple_core_get_ui()) &&
			(purple_presence_is_online(purple_account_get_presence(account))))
		{
			purple_account_connect(account);
		}
	}
}

void
purple_accounts_set_ui_ops(PurpleAccountUiOps *ops)
{
	account_ui_ops = ops;
}

PurpleAccountUiOps *
purple_accounts_get_ui_ops(void)
{
	return account_ui_ops;
}

void *
purple_accounts_get_handle(void)
{
	static int handle;

	return &handle;
}

static void
signed_on_cb(PurpleConnection *gc,
             gpointer unused)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	purple_account_clear_current_error(account);

	purple_signal_emit(purple_accounts_get_handle(), "account-signed-on",
	                   account);
}

static void
signed_off_cb(PurpleConnection *gc,
              gpointer unused)
{
	PurpleAccount *account = purple_connection_get_account(gc);

	purple_signal_emit(purple_accounts_get_handle(), "account-signed-off",
	                   account);
}

static void
connection_error_cb(PurpleConnection *gc,
                    PurpleConnectionError type,
                    const gchar *description,
                    gpointer unused)
{
	PurpleAccount *account;
	PurpleConnectionErrorInfo *err;

	account = purple_connection_get_account(gc);

	g_return_if_fail(account != NULL);

	err = g_new0(PurpleConnectionErrorInfo, 1);
	PURPLE_DBUS_REGISTER_POINTER(err, PurpleConnectionErrorInfo);

	err->type = type;
	err->description = g_strdup(description);

	purple_account_set_current_error(account, err);

	purple_signal_emit(purple_accounts_get_handle(), "account-connection-error",
	                   account, type, description);
}

static void
password_migration_cb(PurpleAccount *account)
{
	/* account may be NULL (means: all) */

	purple_accounts_schedule_save();
}

void
purple_accounts_init(void)
{
	void *handle = purple_accounts_get_handle();
	void *conn_handle = purple_connections_get_handle();

	purple_signal_register(handle, "account-connecting",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-disabled",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-enabled",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-setting-info",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-set-info",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-created",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-destroying",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-added",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-removed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-status-changed",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS));

	purple_signal_register(handle, "account-actions-changed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-alias-changed",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
							 			PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-requested",
						purple_marshal_INT__POINTER_POINTER_POINTER,
						purple_value_new(PURPLE_TYPE_INT), 4,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-denied",
						purple_marshal_VOID__POINTER_POINTER, NULL, 3,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-authorization-granted",
						purple_marshal_VOID__POINTER_POINTER, NULL, 3,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						purple_value_new(PURPLE_TYPE_STRING),
						purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "account-error-changed",
	                       purple_marshal_VOID__POINTER_POINTER_POINTER,
	                       NULL, 3,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT),
	                       purple_value_new(PURPLE_TYPE_POINTER),
	                       purple_value_new(PURPLE_TYPE_POINTER));

	purple_signal_register(handle, "account-signed-on",
	                       purple_marshal_VOID__POINTER, NULL, 1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-signed-off",
	                       purple_marshal_VOID__POINTER, NULL, 1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT));

	purple_signal_register(handle, "account-connection-error",
	                       purple_marshal_VOID__POINTER_INT_POINTER, NULL, 3,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_ACCOUNT),
	                       purple_value_new(PURPLE_TYPE_ENUM),
	                       purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_connect(conn_handle, "signed-on", handle,
	                      PURPLE_CALLBACK(signed_on_cb), NULL);
	purple_signal_connect(conn_handle, "signed-off", handle,
	                      PURPLE_CALLBACK(signed_off_cb), NULL);
	purple_signal_connect(conn_handle, "connection-error", handle,
	                      PURPLE_CALLBACK(connection_error_cb), NULL);
	purple_signal_connect(purple_keyring_get_handle(), "password-migration", handle,
	                      PURPLE_CALLBACK(password_migration_cb), NULL);

	load_accounts();

}

void
purple_accounts_uninit(void)
{
	gpointer handle = purple_accounts_get_handle();
	if (save_timer != 0)
	{
		purple_timeout_remove(save_timer);
		save_timer = 0;
		sync_accounts();
	}

	for (; accounts; accounts = g_list_delete_link(accounts, accounts))
		g_object_unref(G_OBJECT(accounts->data));

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);
}
