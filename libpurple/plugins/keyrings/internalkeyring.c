/**
 * @file internalkeyring.c internal keyring
 * @ingroup plugins
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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#define INTERNALKEYRING_NAME        N_("Internal keyring")
#define INTERNALKEYRING_DESCRIPTION N_("This plugin provides the default" \
	" password storage behaviour for libpurple. Password will be stored" \
	" unencrypted.")
#define INTERNALKEYRING_AUTHOR      "Scrouaf (scrouaf[at]soc.pidgin.im)"
#define INTERNALKEYRING_ID          PURPLE_DEFAULT_KEYRING

static gboolean internal_keyring_opened = FALSE;
static GHashTable *internal_keyring_passwords = NULL;
static PurpleKeyring *keyring_handler = NULL;

/***********************************************/
/*     Keyring interface                       */
/***********************************************/

static void
internal_keyring_open(void)
{
	if (internal_keyring_opened)
		return;
	internal_keyring_opened = TRUE;

	internal_keyring_passwords = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify)purple_str_wipe);
}

static void
internal_keyring_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	const char *password;
	GError *error;

	internal_keyring_open();

	password = g_hash_table_lookup(internal_keyring_passwords, account);

	if (password != NULL) {
		purple_debug_misc("keyring-internal",
			"Got password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		if (cb != NULL)
			cb(account, password, NULL, data);
	} else {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring-internal",
				"No password for account %s (%s).\n",
				purple_account_get_username(account),
				purple_account_get_protocol_id(account));
		}
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWORD, "Password not found.");
		if (cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
	}
}

static void
internal_keyring_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	void *old_password;
	internal_keyring_open();

	old_password = g_hash_table_lookup(internal_keyring_passwords, account);

	if (password == NULL)
		g_hash_table_remove(internal_keyring_passwords, account);
	else {
		g_hash_table_replace(internal_keyring_passwords, account,
			g_strdup(password));
	}

	if (!(password == NULL && old_password == NULL)) {
		purple_debug_misc("keyring-internal",
			"Password %s for account %s (%s).\n",
			(password == NULL ? "removed" : (old_password == NULL ? "saved" : "updated")),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else if (purple_debug_is_verbose()) {
		purple_debug_misc("keyring-internal",
			"Password for account %s (%s) was already removed.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	}

	if (cb != NULL)
		cb(account, NULL, data);
}

static void
internal_keyring_close(void)
{
	if (!internal_keyring_opened)
		return;
	internal_keyring_opened = FALSE;

	g_hash_table_destroy(internal_keyring_passwords);
	internal_keyring_passwords = NULL;
}

static gboolean
internal_keyring_import_password(PurpleAccount *account, const char *mode,
	const char *data, GError **error)
{
	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(data != NULL, FALSE);

	internal_keyring_open();

	if (mode == NULL)
		mode = "cleartext";

	if (g_strcmp0(mode, "cleartext") == 0) {
		g_hash_table_replace(internal_keyring_passwords, account,
			g_strdup(data));
		return TRUE;
	} else {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				"Invalid password storage mode");
		}
		return FALSE;
	}
}

static gboolean
internal_keyring_export_password(PurpleAccount *account, const char **mode,
	char **data, GError **error, GDestroyNotify *destroy)
{
	gchar *password;

	internal_keyring_open();

	password = g_hash_table_lookup(internal_keyring_passwords, account);

	if (password == NULL) {
		return FALSE;
	} else {
		*mode = "cleartext";
		*data = g_strdup(password);
		*destroy = (GDestroyNotify)purple_str_wipe;
		return TRUE;
	}
}

/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
internal_keyring_load(PurplePlugin *plugin)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, INTERNALKEYRING_NAME);
	purple_keyring_set_id(keyring_handler, INTERNALKEYRING_ID);
	purple_keyring_set_read_password(keyring_handler, internal_keyring_read);
	purple_keyring_set_save_password(keyring_handler, internal_keyring_save);
	purple_keyring_set_close_keyring(keyring_handler, internal_keyring_close);
	purple_keyring_set_import_password(keyring_handler, internal_keyring_import_password);
	purple_keyring_set_export_password(keyring_handler, internal_keyring_export_password);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
internal_keyring_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		purple_debug_warning("keyring-internal",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	internal_keyring_close();

	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,		/* magic */
	PURPLE_MAJOR_VERSION,		/* major_version */
	PURPLE_MINOR_VERSION,		/* minor_version */
	PURPLE_PLUGIN_STANDARD,		/* type */
	NULL,				/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,	/* flags */
	NULL,				/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	INTERNALKEYRING_ID,		/* id */
	INTERNALKEYRING_NAME,		/* name */
	DISPLAY_VERSION,		/* version */
	"Internal Keyring Plugin",	/* summary */
	INTERNALKEYRING_DESCRIPTION,	/* description */
	INTERNALKEYRING_AUTHOR,		/* author */
	PURPLE_WEBSITE,			/* homepage */
	internal_keyring_load,		/* load */
	internal_keyring_unload,	/* unload */
	NULL,				/* destroy */
	NULL,				/* ui_info */
	NULL,				/* extra_info */
	NULL,				/* prefs_info */
	NULL,				/* actions */
	NULL, NULL, NULL, NULL		/* padding */
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(internal_keyring, init_plugin, plugininfo)
