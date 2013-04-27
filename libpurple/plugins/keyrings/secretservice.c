/* purple
 * @file secretservice.c Secret Service password storage
 * @ingroup plugins
 * @todo rewrite it with Complete API
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

#include <libsecret/secret.h>

#define SECRETSERVICE_NAME        N_("Secret Service")
#define SECRETSERVICE_ID          "keyring-libsecret"

static PurpleKeyring *keyring_handler = NULL;

static const SecretSchema purple_schema = {
	"im.pidgin.Purple", SECRET_SCHEMA_NONE,
	{
		{"user", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{"protocol", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{"NULL", 0}
	},
	/* Reserved fields */
	0, 0, 0, 0, 0, 0, 0, 0
};

typedef struct _InfoStorage InfoStorage;

struct _InfoStorage
{
	PurpleAccount *account;
	gpointer cb;
	gpointer user_data;
};

/***********************************************/
/*     Keyring interface                       */
/***********************************************/
static void
ss_read_continue(GObject *object, GAsyncResult *result, gpointer data)
{
	InfoStorage *storage = data;
	PurpleAccount *account = storage->account;
	PurpleKeyringReadCallback cb = storage->cb;
	char *password;
	GError *error = NULL;

	password = secret_password_lookup_finish(result, &error);

	if (error != NULL) {
		int code = error->code;

		switch (code) {
			case G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND:
			case G_DBUS_ERROR_IO_ERROR:
				error = g_error_new(PURPLE_KEYRING_ERROR,
				                    PURPLE_KEYRING_ERROR_BACKENDFAIL,
				                    "Failed to communicate with Secret Service (account : %s).",
				                    purple_account_get_username(account));
				if (cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				break;

			default:
				purple_debug_error("keyring-libsecret",
				                  "Unknown error (account: %s (%s), domain: %s, code: %d): %s.\n",
				                  purple_account_get_username(account),
				                  purple_account_get_protocol_id(account),
				                  g_quark_to_string(error->domain), code, error->message);
				error = g_error_new(PURPLE_KEYRING_ERROR,
				                    PURPLE_KEYRING_ERROR_BACKENDFAIL,
				                    "Unknown error (account : %s).",
				                    purple_account_get_username(account));
				if (cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				break;
		}

	} else if (password == NULL) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
		                    PURPLE_KEYRING_ERROR_NOPASSWORD,
		                    "No password found for account: %s",
		                    purple_account_get_username(account));
		if (cb != NULL)
			cb(account, NULL, error, storage->user_data);
		g_error_free(error);

	} else {
		if (cb != NULL)
			cb(account, password, NULL, storage->user_data);
	}

	g_free(storage);
}

static void
ss_read(PurpleAccount *account, PurpleKeyringReadCallback cb, gpointer data)
{
	InfoStorage *storage = g_new0(InfoStorage, 1);

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	secret_password_lookup(&purple_schema,
	                       NULL, ss_read_continue, storage,
	                       "user", purple_account_get_username(account),
	                       "protocol", purple_account_get_protocol_id(account),
	                       NULL);
}

static void
ss_save_continue(GObject *object, GAsyncResult *result, gpointer data)
{
	InfoStorage *storage = data;
	PurpleKeyringSaveCallback cb;
	GError *error = NULL;
	PurpleAccount *account;

	account = storage->account;
	cb = storage->cb;

	secret_password_store_finish(result, &error);

	if (error != NULL) {
		int code = error->code;

		switch (code) {
			case G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND:
			case G_DBUS_ERROR_IO_ERROR:
				purple_debug_info("keyring-libsecret",
				                  "Failed to communicate with Secret Service (account : %s (%s)).\n",
				                  purple_account_get_username(account),
				                  purple_account_get_protocol_id(account));
				error = g_error_new(PURPLE_KEYRING_ERROR,
				                    PURPLE_KEYRING_ERROR_BACKENDFAIL,
				                    "Failed to communicate with Secret Service (account : %s).",
				                    purple_account_get_username(account));
				if (cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				break;

			default:
				purple_debug_error("keyring-libsecret",
				                  "Unknown error (account: %s (%s), domain: %s, code: %d): %s.\n",
				                  purple_account_get_username(account),
				                  purple_account_get_protocol_id(account),
				                  g_quark_to_string(error->domain), code, error->message);
				error = g_error_new(PURPLE_KEYRING_ERROR,
				                    PURPLE_KEYRING_ERROR_BACKENDFAIL,
				                    "Unknown error (account : %s).",
				                    purple_account_get_username(account));
				if (cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				break;
		}

	} else {
		purple_debug_info("keyring-libsecret", "Password for %s updated.\n",
			purple_account_get_username(account));

		if (cb != NULL)
			cb(account, NULL, storage->user_data);
	}

	g_free(storage);
}

static void
ss_save(PurpleAccount *account,
         const gchar *password,
         PurpleKeyringSaveCallback cb,
         gpointer data)
{
	InfoStorage *storage = g_new0(InfoStorage, 1);

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	if (password != NULL && *password != '\0') {
		const char *username = purple_account_get_username(account);
		char *label;

		purple_debug_info("keyring-libsecret",
			"Updating password for account %s (%s).\n",
			username, purple_account_get_protocol_id(account));

		label = g_strdup_printf(_("Pidgin IM password for account %s"), username);
		secret_password_store(&purple_schema, SECRET_COLLECTION_DEFAULT,
		                      label, password,
		                      NULL, ss_save_continue, storage,
		                      "user", username,
		                      "protocol", purple_account_get_protocol_id(account),
		                      NULL);
		g_free(label);

	} else {	/* password == NULL, delete password. */
		purple_debug_info("keyring-libsecret",
			"Forgetting password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		secret_password_clear(&purple_schema, NULL, ss_save_continue, storage,
		                      "user", purple_account_get_username(account),
		                      "protocol", purple_account_get_protocol_id(account),
		                      NULL);
	}
}

static void
ss_close(void)
{
}

static gboolean
ss_init(void)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, SECRETSERVICE_NAME);
	purple_keyring_set_id(keyring_handler, SECRETSERVICE_ID);
	purple_keyring_set_read_password(keyring_handler, ss_read);
	purple_keyring_set_save_password(keyring_handler, ss_save);
	purple_keyring_set_close_keyring(keyring_handler, ss_close);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static void
ss_uninit(void)
{
	ss_close();
	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;
}

/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
ss_load(PurplePlugin *plugin)
{
	return ss_init();
}

static gboolean
ss_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler)
		return FALSE;

	ss_uninit();

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,		/* magic */
	PURPLE_MAJOR_VERSION,		/* major_version */
	PURPLE_MINOR_VERSION,		/* minor_version */
	PURPLE_PLUGIN_STANDARD,		/* type */
	NULL,						/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,	/* flags */
	NULL,						/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	SECRETSERVICE_ID,			/* id */
	SECRETSERVICE_NAME,			/* name */
	DISPLAY_VERSION,			/* version */
	"Secret Service Plugin",		/* summary */
	N_("This plugin will store passwords in Secret Service."),	/* description */
	"Elliott Sales de Andrade (qulogic[at]pidgin.im)",		/* author */
	PURPLE_WEBSITE,				/* homepage */
	ss_load,					/* load */
	ss_unload,					/* unload */
	NULL,						/* destroy */
	NULL,						/* ui_info */
	NULL,						/* extra_info */
	NULL,						/* prefs_info */
	NULL,						/* actions */
	NULL,						/* padding... */
	NULL,
	NULL,
	NULL,
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(secret_service, init_plugin, plugininfo)

