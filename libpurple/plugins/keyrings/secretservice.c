/* purple
 * @file secretservice.c Secret Service password storage
 * @ingroup plugins
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

/* TODO
 *
 * This keyring now works (at the time of this writing), but there are
 * some inconvenient edge cases. When looking up passwords, libsecret
 * doesn't error if the keyring is locked. Therefore, it appears to
 * this plugin that there's no stored password. libpurple seems to
 * handle this as gracefully as possible, but it's still inconvenient.
 * This plugin could possibly be ported to use libsecret's "Complete API"
 * to resolve this if desired.
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugins.h"
#include "version.h"

#include <libsecret/secret.h>

/* Translators: Secret Service is a service that runs on the user's computer.
   It is one option for where the user's passwords can be stored. It is a
   project name. It may not be appropriate to translate this string, but
   transliterating to your alphabet is reasonable. More info about the
   project can be found at https://wiki.gnome.org/Projects/Libsecret */
#define SECRETSERVICE_NAME        N_("Secret Service")
#define SECRETSERVICE_ID          "keyring-libsecret"
#define SECRETSERVICE_DOMAIN      (g_quark_from_static_string(SECRETSERVICE_ID))

static PurpleKeyring *keyring_handler = NULL;
static GCancellable *keyring_cancellable = NULL;

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
ss_g_error_to_keyring_error(GError **error, PurpleAccount *account)
{
	GError *old_error;
	GError *new_error = NULL;

	g_return_if_fail(error != NULL);

	old_error = *error;

	if (g_error_matches(old_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		new_error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_CANCELLED,
			_("Operation cancelled."));
	} else if (g_error_matches(old_error, G_DBUS_ERROR,
				G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND) ||
			g_error_matches(old_error, G_DBUS_ERROR,
				G_DBUS_ERROR_IO_ERROR)) {
		purple_debug_info("keyring-libsecret",
			"Failed to communicate with Secret "
			"Service (account: %s (%s)).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		new_error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			"Failed to communicate with Secret "
			"Service (account: %s).",
			purple_account_get_username(account));
	} else if (g_error_matches(old_error, SECRET_ERROR,
			SECRET_ERROR_IS_LOCKED)) {
		purple_debug_info("keyring-libsecret",
			"Secret Service is locked (account: %s (%s)).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		new_error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_ACCESSDENIED,
			"Secret Service is locked (account: %s).",
			purple_account_get_username(account));
	} else {
		purple_debug_error("keyring-libsecret",
			"Unknown error (account: %s (%s), "
			"domain: %s, code: %d): %s.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account),
			g_quark_to_string(old_error->domain),
			old_error->code, old_error->message);
		new_error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			"Unknown error (account: %s).",
			purple_account_get_username(account));
	}

	g_clear_error(error);
	g_propagate_error(error, new_error);
}

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
		ss_g_error_to_keyring_error(&error, account);
	} else if (password == NULL) {
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWORD,
			"No password found for account: %s",
			purple_account_get_username(account));
	}

	if (cb != NULL) {
		cb(account, password, error, storage->user_data);
	}

	g_clear_error(&error);
	g_free(storage);
}

static void
ss_read(PurpleAccount *account, PurpleKeyringReadCallback cb, gpointer data)
{
	InfoStorage *storage = g_new0(InfoStorage, 1);

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	secret_password_lookup(&purple_schema, keyring_cancellable,
		ss_read_continue, storage,
		"user", purple_account_get_username(account),
		"protocol", purple_account_get_protocol_id(account), NULL);
}

static void
ss_save_continue(GError *error, gpointer data)
{
	InfoStorage *storage = data;
	PurpleKeyringSaveCallback cb;
	PurpleAccount *account;

	account = storage->account;
	cb = storage->cb;

	if (error != NULL) {
		ss_g_error_to_keyring_error(&error, account);
	} else {
		purple_debug_info("keyring-libsecret", "Password for %s updated.\n",
			purple_account_get_username(account));
	}

	if (cb != NULL)
		cb(account, error, storage->user_data);

	g_clear_error(&error);
	g_free(storage);
}

static void
ss_store_continue(GObject *object, GAsyncResult *result, gpointer data)
{
	GError *error = NULL;

	secret_password_store_finish(result, &error);

	ss_save_continue(error, data);
}

static void
ss_clear_continue(GObject *object, GAsyncResult *result, gpointer data)
{
	GError *error = NULL;

	secret_password_clear_finish(result, &error);

	ss_save_continue(error, data);
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
			label, password, keyring_cancellable,
			ss_store_continue, storage,
			"user", username,
			"protocol", purple_account_get_protocol_id(account),
			NULL);
		g_free(label);

	} else {	/* password == NULL, delete password. */
		purple_debug_info("keyring-libsecret",
			"Forgetting password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		secret_password_clear(&purple_schema, keyring_cancellable,
			ss_clear_continue, storage,
			"user", purple_account_get_username(account),
			"protocol", purple_account_get_protocol_id(account),
			NULL);
	}
}

static void
ss_cancel(void)
{
	g_cancellable_cancel(keyring_cancellable);

	/* Swap out cancelled cancellable for new one for further operations */
	g_clear_object(&keyring_cancellable);
	keyring_cancellable = g_cancellable_new();
}

static void
ss_close(void)
{
	ss_cancel();
}

static gboolean
ss_init(GError **error)
{
	keyring_cancellable = g_cancellable_new();

	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, _(SECRETSERVICE_NAME));
	purple_keyring_set_id(keyring_handler, SECRETSERVICE_ID);
	purple_keyring_set_read_password(keyring_handler, ss_read);
	purple_keyring_set_save_password(keyring_handler, ss_save);
	purple_keyring_set_cancel_requests(keyring_handler, ss_cancel);
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

	g_clear_object(&keyring_cancellable);
}

/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Elliott Sales de Andrade (qulogic[at]pidgin.im)",
		NULL
	};

	return purple_plugin_info_new(
		"id",           SECRETSERVICE_ID,
		"name",         SECRETSERVICE_NAME,
		"version",      DISPLAY_VERSION,
		"category",     N_("Keyring"),
		"summary",      "Secret Service Plugin",
		"description",  N_("This plugin will store passwords in Secret Service."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	return ss_init(error);
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		g_set_error(error, SECRETSERVICE_DOMAIN, 0, "The keyring is currently "
			"in use.");
		return FALSE;
	}

	ss_uninit();

	return TRUE;
}

PURPLE_PLUGIN_INIT(secret_service, plugin_query, plugin_load, plugin_unload);
