/**
 * @file internalkeyring.c internal keyring
 * @ingroup plugins
 *
 * @todo
 *   cleanup error handling and reporting
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#define INTERNALKEYRING_NAME        N_("Internal keyring")
#define INTERNALKEYRING_DESCRIPTION N_("This plugin provides the default password storage behaviour for libpurple. Password will be stored unencrypted.")
#define INTERNALKEYRING_AUTHOR      "Scrouaf (scrouaf[at]soc.pidgin.im)"
#define INTERNALKEYRING_ID          PURPLE_DEFAULT_KEYRING

#define ACTIVATE() \
	if (internal_keyring_passwords == NULL) \
		internal_keyring_open();

static GHashTable *internal_keyring_passwords = NULL;
static PurpleKeyring *keyring_handler = NULL;

/***********************************************/
/*     Keyring interface                       */
/***********************************************/
static void
internal_keyring_open(void)
{
	internal_keyring_passwords = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
}

static void
internal_keyring_read(PurpleAccount *account,
                      PurpleKeyringReadCallback cb,
                      gpointer data)
{
	const char *password;
	GError *error;

	ACTIVATE();

	purple_debug_info("keyring-internal",
		"Reading password for account %s (%s).\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account));

	password = g_hash_table_lookup(internal_keyring_passwords, account);

	if (password != NULL) {
		if (cb != NULL)
			cb(account, password, NULL, data);
	} else {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWD, "Password not found.");
		if (cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
	}
}

static void
internal_keyring_save(PurpleAccount *account,
                      const gchar *password,
                      PurpleKeyringSaveCallback cb,
                      gpointer data)
{
	ACTIVATE();

	if (password == NULL || *password == '\0') {
		g_hash_table_remove(internal_keyring_passwords, account);
		purple_debug_info("keyring-internal",
			"Deleted password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else {
		g_hash_table_replace(internal_keyring_passwords, account, g_strdup(password));
		purple_debug_info("keyring-internal",
			"Updated password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

	}

	if (cb != NULL)
		cb(account, NULL, data);
}


static void
internal_keyring_close(GError **error)
{
	if (internal_keyring_passwords)
		g_hash_table_destroy(internal_keyring_passwords);
	internal_keyring_passwords = NULL;
}

static gboolean
internal_keyring_import_password(PurpleAccount *account,
                                 const char *mode,
                                 const char *data,
                                 GError **error)
{
	ACTIVATE();

	purple_debug_info("keyring-internal", "Importing password.\n");

	if (account != NULL &&
	    data != NULL &&
	    (mode == NULL || g_strcmp0(mode, "cleartext") == 0)) {

		g_hash_table_replace(internal_keyring_passwords, account, g_strdup(data));
		return TRUE;

	} else {
		*error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOPASSWD, "No password for account.");
		return FALSE;

	}

	return TRUE;
}

static gboolean
internal_keyring_export_password(PurpleAccount *account,
                                 const char **mode,
                                 char **data,
                                 GError **error,
                                 GDestroyNotify *destroy)
{
	gchar *password;

	ACTIVATE();

	purple_debug_info("keyring-internal", "Exporting password.\n");

	password = g_hash_table_lookup(internal_keyring_passwords, account);

	if (password == NULL) {
		return FALSE;
	} else {
		*mode = "cleartext";
		*data = g_strdup(password);
		*destroy = g_free;
		return TRUE;
	}
}

static void
internal_keyring_init(void)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, INTERNALKEYRING_NAME);
	purple_keyring_set_id(keyring_handler, INTERNALKEYRING_ID);
	purple_keyring_set_read_password(keyring_handler, internal_keyring_read);
	purple_keyring_set_save_password(keyring_handler, internal_keyring_save);
	purple_keyring_set_close_keyring(keyring_handler, internal_keyring_close);
	purple_keyring_set_change_master(keyring_handler, NULL);
	purple_keyring_set_import_password(keyring_handler, internal_keyring_import_password);
	purple_keyring_set_export_password(keyring_handler, internal_keyring_export_password);

	purple_keyring_register(keyring_handler);
}

static void
internal_keyring_uninit(void)
{
	internal_keyring_close(NULL);
	purple_keyring_unregister(keyring_handler);

}

/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
internal_keyring_load(PurplePlugin *plugin)
{
	internal_keyring_init();
	return TRUE;
}

static gboolean
internal_keyring_unload(PurplePlugin *plugin)
{
	internal_keyring_uninit();

	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,				/* magic */
	PURPLE_MAJOR_VERSION,				/* major_version */
	PURPLE_MINOR_VERSION,				/* minor_version */
	PURPLE_PLUGIN_STANDARD,				/* type */
	NULL,								/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE|PURPLE_PLUGIN_FLAG_AUTOLOAD,	/* flags */
	NULL,								/* dependencies */
	PURPLE_PRIORITY_DEFAULT,			/* priority */
	INTERNALKEYRING_ID,					/* id */
	INTERNALKEYRING_NAME,				/* name */
	DISPLAY_VERSION,					/* version */
	"Internal Keyring Plugin",			/* summary */
	INTERNALKEYRING_DESCRIPTION,		/* description */
	INTERNALKEYRING_AUTHOR,				/* author */
	"N/A",								/* homepage */
	internal_keyring_load,				/* load */
	internal_keyring_unload,			/* unload */
	NULL,								/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_debug_info("keyring-internal", "Init plugin called.\n");
}

PURPLE_INIT_PLUGIN(internal_keyring, init_plugin, plugininfo)

