/*
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>

#include "account.h"
#include "prefs.h"
#include "prpl.h"

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
	GaimProtocol protocol;
	GaimProxyInfo *proxy_info;
	char *protocol_id;

	GString *buffer;

	GaimPrefType setting_type;
	char *setting_name;

	gboolean in_proxy;

} AccountParserData;

static GList   *accounts = NULL;
static guint    accounts_save_timer = 0;
static gboolean accounts_loaded = FALSE;

static void
__delete_setting(void *data)
{
	GaimAccountSetting *setting = (GaimAccountSetting *)data;

	if (setting->type == GAIM_PREF_STRING)
		g_free(setting->value.string);

	g_free(setting);
}

static gboolean
__accounts_save_cb(gpointer unused)
{
	gaim_accounts_sync();
	accounts_save_timer = 0;

	return FALSE;
}

static void
schedule_accounts_save()
{
	if (!accounts_save_timer)
		accounts_save_timer = g_timeout_add(5000, __accounts_save_cb, NULL);
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

	return account;
}

void
gaim_account_destroy(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	if (account->gc != NULL)
		gaim_connection_destroy(account->gc);

	if (account->username    != NULL) g_free(account->username);
	if (account->alias       != NULL) g_free(account->alias);
	if (account->password    != NULL) g_free(account->password);
	if (account->user_info   != NULL) g_free(account->user_info);
	if (account->protocol_id != NULL) g_free(account->protocol_id);

	g_hash_table_destroy(account->settings);

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

	account->gc = NULL;
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
	g_return_if_fail(account  != NULL);

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
	g_return_if_fail(account   != NULL);

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

	schedule_accounts_save();
}

void
gaim_account_set_protocol(GaimAccount *account, GaimProtocol protocol)
{
	GaimPlugin *plugin;

	g_return_if_fail(account != NULL);

	plugin = gaim_find_prpl(protocol);

	g_return_if_fail(plugin != NULL);

	account->protocol = protocol;

	if (account->protocol_id != NULL)
		g_free(account->protocol_id);

	account->protocol_id = g_strdup(plugin->info->id);

	schedule_accounts_save();
}

void
gaim_account_set_connection(GaimAccount *account, GaimConnection *gc)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(gc      != NULL);

	account->gc = gc;

	schedule_accounts_save();
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

	account->check_mail = value;

	schedule_accounts_save();
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
gaim_account_clear_settings(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	g_hash_table_destroy(account->settings);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, __delete_setting);
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

gboolean
gaim_account_get_check_mail(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return account->check_mail;
}

GaimProxyInfo *
gaim_account_get_proxy_info(const GaimAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->proxy_info;
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

/* XML Stuff */
static void
__free_parser_data(gpointer user_data)
{
	AccountParserData *data = user_data;

	if (data->buffer != NULL)
		g_free(data->buffer);

	if (data->setting_name != NULL)
		g_free(data->setting_name);

	g_free(data);
}

static void
__start_element_handler(GMarkupParseContext *context,
						const gchar *element_name,
						const gchar **attribute_names,
						const gchar **attribute_values,
						gpointer user_data, GError **error)
{
	AccountParserData *data = user_data;
	int i;

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
	else if (!strcmp(element_name, "setting")) {
		data->tag = TAG_SETTING;

		for (i = 0; attribute_names[i] != NULL; i++) {

			if (!strcmp(attribute_names[i], "name"))
				data->setting_name = g_strdup(attribute_values[i]);
			else if (!strcmp(attribute_names[i], "type")) {

				if (!strcmp(attribute_values[i], "string"))
					data->setting_type = GAIM_PREF_STRING;
				else if (!strcmp(attribute_values[i], "int"))
					data->setting_type = GAIM_PREF_INT;
				else if (!strcmp(attribute_values[i], "bool"))
					data->setting_type = GAIM_PREF_BOOLEAN;
			}
		}
	}
}

static void
__end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					  gpointer user_data,  GError **error)
{
	AccountParserData *data = user_data;
	gchar *buffer;

	if (data->buffer == NULL)
		return;

	buffer = g_string_free(data->buffer, FALSE);
	data->buffer = NULL;

	if (data->tag == TAG_PROTOCOL) {
		GList *l;
		GaimPlugin *plugin;

		data->protocol_id = g_strdup(buffer);
		data->protocol = -1;

		for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
			plugin = (GaimPlugin *)l->data;

			if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {
				if (!strcmp(plugin->info->id, buffer)) {
					data->protocol =
						GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol;

					break;
				}
			}
		}
	}
	else if (data->tag == TAG_NAME) {
		if (data->in_proxy) {
			gaim_proxy_info_set_username(data->proxy_info, buffer);
		}
		else {
			data->account = gaim_account_new(buffer, data->protocol);
			data->account->protocol_id = data->protocol_id;

			gaim_accounts_add(data->account);

			data->protocol_id = NULL;
		}
	}
	else if (data->tag == TAG_PASSWORD) {
		if (data->in_proxy) {
			gaim_proxy_info_set_password(data->proxy_info, buffer);
		}
		else {
			gaim_account_set_password(data->account, buffer);
			gaim_account_set_remember_password(data->account, TRUE);
		}
	}
	else if (data->tag == TAG_ALIAS)
		gaim_account_set_alias(data->account, buffer);
	else if (data->tag == TAG_USERINFO)
		gaim_account_set_user_info(data->account, buffer);
	else if (data->tag == TAG_BUDDYICON)
		gaim_account_set_buddy_icon(data->account, buffer);
	else if (data->tag == TAG_TYPE) {
		if (data->in_proxy) {
			if (!strcmp(buffer, "global"))
				gaim_proxy_info_set_type(data->proxy_info,
										 GAIM_PROXY_USE_GLOBAL);
			else if (!strcmp(buffer, "http"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_HTTP);
			else if (!strcmp(buffer, "socks4"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_SOCKS4);
			else if (!strcmp(buffer, "socks5"))
				gaim_proxy_info_set_type(data->proxy_info, GAIM_PROXY_SOCKS5);
			else
				gaim_debug(GAIM_DEBUG_ERROR, "account",
						   "Invalid proxy type found when loading account "
						   "information for %s\n",
						   gaim_account_get_username(data->account));
		}
	}
	else if (data->tag == TAG_HOST) {
		if (data->in_proxy) {
			gaim_proxy_info_set_host(data->proxy_info, buffer);
		}
	}
	else if (data->tag == TAG_PORT) {
		if (data->in_proxy) {
			gaim_proxy_info_set_port(data->proxy_info, atoi(buffer));
		}
	}
	else if (data->tag == TAG_SETTING) {
		if (data->setting_type == GAIM_PREF_STRING)
			gaim_account_set_string(data->account, data->setting_name,
									buffer);
		else if (data->setting_type == GAIM_PREF_INT)
			gaim_account_set_int(data->account, data->setting_name,
								 atoi(buffer));
		else if (data->setting_type == GAIM_PREF_BOOLEAN)
			gaim_account_set_bool(data->account, data->setting_name,
				(*buffer == '0' ? FALSE : TRUE));

		g_free(data->setting_name);
		data->setting_name = NULL;
	}
	else if (!strcmp(element_name, "proxy")) {
		data->in_proxy = FALSE;

		if (gaim_proxy_info_get_type(data->proxy_info) == GAIM_PROXY_NONE) {
			gaim_proxy_info_destroy(data->proxy_info);
			data->proxy_info = NULL;
		}
		else {
			gaim_account_set_proxy_info(data->account, data->proxy_info);
		}
	}

	data->tag = TAG_NONE;

	g_free(buffer);
}

static void
__text_handler(GMarkupParseContext *context, const gchar *text,
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
	__start_element_handler,
	__end_element_handler,
	__text_handler,
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

	gaim_debug(GAIM_DEBUG_INFO, "accounts", "Reading %s\n", filename);

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts",
				   "Error reading accounts: %s\n", error->message);
		
		g_error_free(error);

		accounts_loaded = TRUE;
		return FALSE;
	}

	parser_data = g_new0(AccountParserData, 1);

	context = g_markup_parse_context_new(&accounts_parser, 0,
										 parser_data, __free_parser_data);

	if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);

		accounts_loaded = TRUE;

		return FALSE;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "accounts", "Error parsing %s\n",
				   filename);

		g_markup_parse_context_free(context);
		g_free(contents);
		accounts_loaded = TRUE;

		return FALSE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);

	gaim_debug(GAIM_DEBUG_INFO, "accounts", "Finished reading %s\n",
			   filename);
	g_free(filename);

	accounts_loaded = TRUE;

	return TRUE;
}

static void
__write_setting(gpointer key, gpointer value, gpointer user_data)
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
	else if (setting->type == GAIM_PREF_STRING) {
		fprintf(fp, "   <setting name='%s' type='string'>%s</setting>\n",
				name, setting->value.string);
	}
	else if (setting->type == GAIM_PREF_BOOLEAN) {
		fprintf(fp, "   <setting name='%s' type='bool'>%d</setting>\n",
				name, setting->value.bool);
	}
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
		fprintf(fp, "  <userinfo>%s</userinfo>\n", esc);
		g_free(esc);
	}

	if ((buddy_icon = gaim_account_get_buddy_icon(account)) != NULL) {
		esc = g_markup_escape_text(buddy_icon, -1);
		fprintf(fp, "  <buddyicon>%s</buddyicon>\n", esc);
		g_free(esc);
	}

	fprintf(fp, "  <settings>\n");
	g_hash_table_foreach(account->settings, __write_setting, fp);
	fprintf(fp, "  </settings>\n");

	if ((proxy_info = gaim_account_get_proxy_info(account)) != NULL &&
		(proxy_type = gaim_proxy_info_get_type(proxy_info)) != GAIM_PROXY_NONE)
	{
		const char *value;
		int int_value;

		fprintf(fp, "  <proxy>\n");
		fprintf(fp, "   <type>%s</type>\n",
				(proxy_type == GAIM_PROXY_USE_GLOBAL ? "global" :
				 proxy_type == GAIM_PROXY_HTTP       ? "http"   :
				 proxy_type == GAIM_PROXY_SOCKS4     ? "socks4" :
				 proxy_type == GAIM_PROXY_SOCKS5     ? "socks5" : "unknown"));

		if (proxy_type != GAIM_PROXY_USE_GLOBAL) {
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

	accounts = g_list_append(accounts, account);

	schedule_accounts_save();
}

void
gaim_accounts_remove(GaimAccount *account)
{
	g_return_if_fail(account != NULL);

	accounts = g_list_remove(accounts, account);

	schedule_accounts_save();
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
