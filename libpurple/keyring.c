/**
 * @file keyring.c Keyring API
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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib.h>
#include <string.h>
#include "account.h"
#include "keyring.h"
#include "signals.h"
#include "core.h"
#include "debug.h"
#include "internal.h"
#include "dbus-maybe.h"

struct _PurpleKeyring
{
	gchar *name;
	gchar *id;
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringCancelRequests cancel_requests;
	PurpleKeyringClose close_keyring;
	PurpleKeyringChangeMaster change_master;
	PurpleKeyringImportPassword import_password;
	PurpleKeyringExportPassword export_password;
};

typedef struct
{
	GError *error;
	PurpleKeyringSetInUseCallback cb;
	gpointer cb_data;
	PurpleKeyring *new;
	PurpleKeyring *old;

	/**
	 * We are done when finished is positive and read_outstanding is zero.
	 */
	gboolean finished;
	int read_outstanding;

	gboolean abort;
	gboolean force;
	gboolean succeeded;
} PurpleKeyringChangeTracker;

typedef void (*PurpleKeyringDropCallback)(gpointer data);

typedef struct
{
	PurpleKeyringDropCallback cb;
	gpointer cb_data;

	gboolean finished;
	int drop_outstanding;
} PurpleKeyringDropTracker;

typedef struct
{
	PurpleKeyringSaveCallback cb;
	gpointer cb_data;
} PurpleKeyringSetPasswordData;

typedef struct
{
	gchar *keyring_id;
	gchar *mode;
	gchar *data;
} PurpleKeyringFailedImport;

static void
purple_keyring_change_tracker_free(PurpleKeyringChangeTracker *tracker)
{
	if (tracker->error)
		g_error_free(tracker->error);
	g_free(tracker);
}

static void
purple_keyring_failed_import_free(PurpleKeyringFailedImport *import)
{
	g_return_if_fail(import != NULL);

	g_free(import->keyring_id);
	g_free(import->mode);
	g_free(import->data);
	g_free(import);
}

static void
purple_keyring_close(PurpleKeyring *keyring);

static void
purple_keyring_drop_passwords(PurpleKeyring *keyring,
	PurpleKeyringDropCallback cb, gpointer data);

/* A list of available keyrings */
static GList *purple_keyring_keyrings = NULL;

/* Keyring being used. */
static PurpleKeyring *purple_keyring_inuse = NULL;

/* Keyring id marked to use (may not be loadable). */
static gchar *purple_keyring_to_use = NULL;

static guint purple_keyring_pref_cbid = 0;
static GList *purple_keyring_loaded_plugins = NULL;
static PurpleKeyringChangeTracker *current_change_tracker = NULL;
static gboolean purple_keyring_is_quitting = FALSE;
static GHashTable *purple_keyring_failed_imports = NULL;


/**************************************************************************/
/* Setting used keyrings                                                  */
/**************************************************************************/

PurpleKeyring *
purple_keyring_find_keyring_by_id(const gchar *id)
{
	GList *it;

	for (it = purple_keyring_keyrings; it != NULL; it = it->next) {
		PurpleKeyring *keyring = it->data;
		const gchar *curr_id = purple_keyring_get_id(keyring);

		if (g_strcmp0(id, curr_id) == 0)
			return keyring;
	}

	return NULL;
}

static void
purple_keyring_pref_callback(const gchar *pref, PurplePrefType type,
	gconstpointer id, gpointer data)
{
	PurpleKeyring *new_keyring;

	g_return_if_fail(g_strcmp0(pref, "/purple/keyring/active") == 0);
	g_return_if_fail(type == PURPLE_PREF_STRING);
	g_return_if_fail(id != NULL);

	new_keyring = purple_keyring_find_keyring_by_id(id);
	g_return_if_fail(new_keyring != NULL);

	purple_keyring_set_inuse(new_keyring, FALSE, NULL, NULL);
}

static void
purple_keyring_pref_connect(void)
{
	g_return_if_fail(purple_keyring_pref_cbid == 0);

	purple_keyring_pref_cbid = purple_prefs_connect_callback(NULL,
		"/purple/keyring/active", purple_keyring_pref_callback, NULL);
}

static void
purple_keyring_pref_disconnect(void)
{
	g_return_if_fail(purple_keyring_pref_cbid != 0);

	purple_prefs_disconnect_callback(purple_keyring_pref_cbid);
	purple_keyring_pref_cbid = 0;
}

PurpleKeyring *
purple_keyring_get_inuse(void)
{
	return purple_keyring_inuse;
}

static void
purple_keyring_set_inuse_drop_cb(gpointer _tracker)
{
	PurpleKeyringChangeTracker *tracker = _tracker;

	g_return_if_fail(tracker != NULL);

	if (tracker->succeeded) {
		purple_keyring_close(tracker->old);

		purple_debug_info("keyring", "Successfully changed keyring.\n");

		purple_keyring_inuse = tracker->new;
		current_change_tracker = NULL;

		if (tracker->cb != NULL)
			tracker->cb(NULL, tracker->cb_data);
		purple_keyring_change_tracker_free(tracker);
		return;
	}

	purple_debug_error("keyring", "Failed to change keyring, aborting.\n");

	purple_keyring_close(tracker->new);

	purple_keyring_pref_disconnect();
	purple_prefs_set_string("/purple/keyring/active",
		purple_keyring_get_id(tracker->old));
	purple_keyring_pref_connect();

	current_change_tracker = NULL;

	if (tracker->error == NULL) {
		tracker->error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_UNKNOWN,
			"Unknown error has occured");
	}

	if (tracker->cb != NULL)
		tracker->cb(tracker->error, tracker->cb_data);

	purple_keyring_change_tracker_free(tracker);
}

static void
purple_keyring_set_inuse_save_cb(PurpleAccount *account, GError *error,
	gpointer _tracker)
{
	const gchar *account_name;
	PurpleKeyringChangeTracker *tracker = _tracker;

	g_return_if_fail(account != NULL);
	g_return_if_fail(tracker != NULL);

	tracker->read_outstanding--;

	account_name = purple_account_get_username(account);

	if (g_error_matches(error, PURPLE_KEYRING_ERROR,
		PURPLE_KEYRING_ERROR_NOPASSWORD)) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring", "No password found while "
				"changing keyring for account %s: %s.\n",
				account_name, error->message);
		}
	} else if (g_error_matches(error, PURPLE_KEYRING_ERROR,
		PURPLE_KEYRING_ERROR_ACCESSDENIED)) {
		purple_debug_info("keyring", "Access denied while changing "
			"keyring for account %s: %s.\n",
			account_name, error->message);
		tracker->abort = TRUE;
		if (tracker->error != NULL)
			g_error_free(tracker->error);
		tracker->error = g_error_copy(error);
	} else if (g_error_matches(error, PURPLE_KEYRING_ERROR,
		PURPLE_KEYRING_ERROR_CANCELLED)) {
		purple_debug_info("keyring", "Operation cancelled while "
			"changing keyring for account %s: %s.\n",
			account_name, error->message);
		tracker->abort = TRUE;
		if (tracker->error == NULL)
			tracker->error = g_error_copy(error);
	} else if (g_error_matches(error, PURPLE_KEYRING_ERROR,
		PURPLE_KEYRING_ERROR_BACKENDFAIL)) {
		purple_debug_error("keyring", "Failed to communicate with "
			"backend while changing keyring for account %s: %s. "
			"Aborting changes.\n", account_name, error->message);
		tracker->abort = TRUE;
		if (tracker->error != NULL)
			g_error_free(tracker->error);
		tracker->error = g_error_copy(error);
	} else if (error != NULL) {
		purple_debug_error("keyring", "Unknown error while changing "
			"keyring for account %s: %s. Aborting changes.\n",
			account_name, error->message);
		tracker->abort = TRUE;
		if (tracker->error == NULL)
			tracker->error = g_error_copy(error);
	}

	purple_signal_emit(purple_keyring_get_handle(), "password-migration",
		account);

	if (!tracker->finished || tracker->read_outstanding > 0)
		return;

	/* This was the last one. */
	if (tracker->abort && !tracker->force) {
		tracker->succeeded = FALSE;
		purple_keyring_drop_passwords(tracker->new,
			purple_keyring_set_inuse_drop_cb, tracker);
	} else {
		tracker->succeeded = TRUE;
		purple_keyring_drop_passwords(tracker->old,
			purple_keyring_set_inuse_drop_cb, tracker);
	}
}

static void
purple_keyring_set_inuse_read_cb(PurpleAccount *account, const gchar *password,
	GError *error, gpointer _tracker)
{
	PurpleKeyringChangeTracker *tracker = _tracker;
	PurpleKeyringSave save_cb;

	g_return_if_fail(account != NULL);
	g_return_if_fail(tracker != NULL);

	if (tracker->abort) {
		purple_keyring_set_inuse_save_cb(account, NULL, tracker);
		return;
	}

	if (error != NULL) {
		if (tracker->force == TRUE || g_error_matches(error,
			PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWORD)) {
			/* Don't save password, and ignore it. */
		} else {
			tracker->abort = TRUE;
		}
		purple_keyring_set_inuse_save_cb(account, error, tracker);
		return;
	}

	save_cb = purple_keyring_get_save_password(tracker->new);
	g_assert(save_cb != NULL);

	save_cb(account, password, purple_keyring_set_inuse_save_cb, tracker);
}

void
purple_keyring_set_inuse(PurpleKeyring *newkeyring, gboolean force,
	PurpleKeyringSetInUseCallback cb, gpointer data)
{
	PurpleKeyring *oldkeyring;
	PurpleKeyringChangeTracker *tracker;
	GList *it;
	PurpleKeyringRead read_cb;

	if (current_change_tracker != NULL) {
		GError *error;
		purple_debug_error("keyring", "There is password migration "
			"session already running.\n");
		if (cb == NULL)
			return;
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_INTERNAL,
			"There is a password migration session already running");
		cb(error, data);
		g_error_free(error);
		return;
	}

	oldkeyring = purple_keyring_get_inuse();

	if (oldkeyring == newkeyring) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring",
				"Old and new keyring are the same: %s.\n",
				(newkeyring != NULL) ?
					newkeyring->id : "(null)");
		}
		if (cb != NULL)
			cb(NULL, data);
		return;
	}

	purple_debug_info("keyring", "Attempting to set new keyring: %s.\n",
		(newkeyring != NULL) ? newkeyring->id : "(null)");

	if (oldkeyring == NULL) { /* No keyring was set before. */
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring", "Setting keyring for the "
				"first time: %s.\n", newkeyring->id);
		}

		purple_keyring_inuse = newkeyring;
		g_assert(current_change_tracker == NULL);
		if (cb != NULL)
			cb(NULL, data);
		return;
	}

	/* Starting a migration. */

	read_cb = purple_keyring_get_read_password(oldkeyring);
	g_assert(read_cb != NULL);

	purple_debug_misc("keyring", "Starting migration from: %s.\n",
		oldkeyring->id);

	tracker = g_new0(PurpleKeyringChangeTracker, 1);
	current_change_tracker = tracker;

	tracker->cb = cb;
	tracker->cb_data = data;
	tracker->new = newkeyring;
	tracker->old = oldkeyring;
	tracker->force = force;

	for (it = purple_accounts_get_all(); it != NULL; it = it->next) {
		if (tracker->abort) {
			tracker->finished = TRUE;
			break;
		}
		tracker->read_outstanding++;

		if (it->next == NULL)
			tracker->finished = TRUE;

		read_cb(it->data, purple_keyring_set_inuse_read_cb, tracker);
	}
}

void
purple_keyring_register(PurpleKeyring *keyring)
{
	const gchar *keyring_id;

	g_return_if_fail(keyring != NULL);

	keyring_id = purple_keyring_get_id(keyring);

	purple_debug_info("keyring", "Registering keyring: %s\n",
		keyring_id ? keyring_id : "(null)");

	if (purple_keyring_get_id(keyring) == NULL ||
		purple_keyring_get_name(keyring) == NULL ||
		purple_keyring_get_read_password(keyring) == NULL ||
		purple_keyring_get_save_password(keyring) == NULL) {
		purple_debug_error("keyring", "Cannot register %s, some "
			"required fields are missing.\n",
			keyring_id ? keyring_id : "(null)");
		return;
	}

	/* If this is the configured keyring, use it. */
	if (purple_keyring_inuse == NULL &&
		g_strcmp0(keyring_id, purple_keyring_to_use) == 0) {
		purple_debug_info("keyring", "Keyring %s matches keyring to "
			"use, using it.\n", keyring_id);
		purple_keyring_set_inuse(keyring, TRUE, NULL, NULL);
	}

	PURPLE_DBUS_REGISTER_POINTER(keyring, PurpleKeyring);
	purple_signal_emit(purple_keyring_get_handle(), "keyring-register",
		keyring_id, keyring);
	if (purple_debug_is_verbose()) {
		purple_debug_info("keyring", "Registered keyring: %s.\n",
			keyring_id);
	}

	purple_keyring_keyrings = g_list_prepend(purple_keyring_keyrings,
		keyring);
}

void
purple_keyring_unregister(PurpleKeyring *keyring)
{
	PurpleKeyring *inuse;
	PurpleKeyring *fallback;
	const gchar *keyring_id;

	g_return_if_fail(keyring != NULL);

	keyring_id = purple_keyring_get_id(keyring);

	purple_debug_info("keyring", "Unregistering keyring: %s.\n",
		keyring_id);

	purple_signal_emit(purple_keyring_get_handle(), "keyring-unregister",
		keyring_id, keyring);
	PURPLE_DBUS_UNREGISTER_POINTER(keyring);

	inuse = purple_keyring_get_inuse();
	fallback = purple_keyring_find_keyring_by_id(PURPLE_DEFAULT_KEYRING);

	if (inuse == keyring) {
		if (inuse != fallback) {
			purple_keyring_set_inuse(fallback, TRUE, NULL, NULL);
		} else {
			fallback = NULL;
			purple_keyring_set_inuse(NULL, TRUE, NULL, NULL);
		}
	}

	purple_keyring_keyrings = g_list_remove(purple_keyring_keyrings,
		keyring);
}

GList *
purple_keyring_get_options(void)
{
	GList *options = NULL;
	GList *it;
	static gchar currentDisabledName[40];

	if (purple_keyring_get_inuse() == NULL && purple_keyring_to_use != NULL
		&& purple_keyring_to_use[0] != '\0') {
		g_snprintf(currentDisabledName, sizeof(currentDisabledName),
			_("%s (disabled)"), purple_keyring_to_use);

		options = g_list_append(options, currentDisabledName);
		options = g_list_append(options, purple_keyring_to_use);
	}

	for (it = purple_keyring_keyrings; it != NULL; it = it->next) {
		PurpleKeyring *keyring = it->data;

		options = g_list_append(options,
			(gpointer)purple_keyring_get_name(keyring));
		options = g_list_append(options,
			(gpointer)purple_keyring_get_id(keyring));
	}

	return options;
}


/**************************************************************************/
/* Keyring plugin wrappers                                                */
/**************************************************************************/

static void
purple_keyring_close(PurpleKeyring *keyring)
{
	PurpleKeyringClose close_cb;

	g_return_if_fail(keyring != NULL);

	close_cb = purple_keyring_get_close_keyring(keyring);

	if (close_cb != NULL)
		close_cb();
}

static void
purple_keyring_drop_passwords_cb(PurpleAccount *account, GError *error,
	gpointer _tracker)
{
	PurpleKeyringDropTracker *tracker = _tracker;

	tracker->drop_outstanding--;

	if (!tracker->finished || tracker->drop_outstanding > 0)
		return;

	if (tracker->cb)
		tracker->cb(tracker->cb_data);
	g_free(tracker);
}

static void
purple_keyring_drop_passwords(PurpleKeyring *keyring,
	PurpleKeyringDropCallback cb, gpointer data)
{
	GList *cur;
	PurpleKeyringSave save_cb;
	PurpleKeyringDropTracker *tracker;

	g_return_if_fail(keyring != NULL);

	save_cb = purple_keyring_get_save_password(keyring);
	g_return_if_fail(save_cb != NULL);

	tracker = g_new0(PurpleKeyringDropTracker, 1);
	tracker->cb = cb;
	tracker->cb_data = data;

	for (cur = purple_accounts_get_all(); cur != NULL; cur = cur->next) {
		tracker->drop_outstanding++;
		if (cur->next == NULL)
			tracker->finished = TRUE;

		save_cb(cur->data, NULL, purple_keyring_drop_passwords_cb,
			tracker);
	}
}

gboolean
purple_keyring_import_password(PurpleAccount *account,
                               const gchar *keyring_id,
                               const gchar *mode,
                               const gchar *data,
                               GError **error)
{
	PurpleKeyring *inuse;
	PurpleKeyringImportPassword import;
	const gchar *realid;

	purple_debug_misc("keyring", "Importing password for account %s (%s) to keyring %s.\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account),
		keyring_id);

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		PurpleKeyringFailedImport *import;
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_NOKEYRING,
				"No keyring configured, cannot import password "
				"info");
		}
		purple_debug_warning("Keyring",
			"No keyring configured, cannot import password info for account %s (%s).\n",
			purple_account_get_username(account), purple_account_get_protocol_id(account));

		import = g_new0(PurpleKeyringFailedImport, 1);
		import->keyring_id = g_strdup(keyring_id);
		import->mode = g_strdup(mode);
		import->data = g_strdup(data);
		g_hash_table_insert(purple_keyring_failed_imports, account, import);
		return FALSE;
	}

	realid = purple_keyring_get_id(inuse);
	/*
	 * we want to be sure that either :
	 *  - there is a keyring_id specified and it matches the one configured
	 *  - or the configured keyring is the fallback, compatible one.
	 */
	if ((keyring_id != NULL && g_strcmp0(realid, keyring_id) != 0) ||
	    (keyring_id == NULL && g_strcmp0(PURPLE_DEFAULT_KEYRING, realid))) {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_INTERNAL,
				"Specified keyring id does not match the "
				"configured one.");
		}
		purple_debug_info("keyring",
			"Specified keyring id does not match the configured one (%s vs. %s). Data will be lost.\n",
			keyring_id, realid);
		return FALSE;
	}

	import = purple_keyring_get_import_password(inuse);
	if (import == NULL) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("Keyring", "Configured keyring "
				"cannot import password info. This might be "
				"normal.\n");
		}
		return TRUE;
	}

	return import(account, mode, data, error);
}

gboolean
purple_keyring_export_password(PurpleAccount *account,
                               const gchar **keyring_id,
                               const gchar **mode,
                               gchar **data,
                               GError **error,
                               GDestroyNotify *destroy)
{
	PurpleKeyring *inuse;
	PurpleKeyringExportPassword export;

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		PurpleKeyringFailedImport *import = g_hash_table_lookup(
			purple_keyring_failed_imports, account);

		if (import == NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOKEYRING,
				"No keyring configured, cannot export password info");
			purple_debug_warning("keyring",
				"No keyring configured, cannot export password info.\n");
			return FALSE;
		} else {
			purple_debug_info("keyring", "No keyring configured, getting fallback export data for %s (%s).\n",
				purple_account_get_username(account),
				purple_account_get_protocol_id(account));

			*keyring_id = import->keyring_id;
			*mode = import->mode;
			*data = g_strdup(import->data);
			*destroy = g_free;
			return TRUE;
		}
	}

	*keyring_id = purple_keyring_get_id(inuse);

	if (purple_debug_is_verbose()) {
		purple_debug_misc("keyring",
			"Exporting password for account %s (%s) from keyring "
			"%s...\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account), *keyring_id);
	}

	if (*keyring_id == NULL) {
		*error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"Plugin does not have a keyring id");
		purple_debug_info("keyring",
			"Configured keyring does not have a keyring id, cannot export password.\n");
		return FALSE;
	}

	export = purple_keyring_get_export_password(inuse);

	if (export == NULL) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("Keyring", "Configured keyring "
				"cannot export password info. This might be "
				"normal.\n");
		}
		*mode = NULL;
		*data = NULL;
		*destroy = NULL;
		return TRUE;
	}

	return export(account, mode, data, error, destroy);
}

void
purple_keyring_get_password(PurpleAccount *account,
                            PurpleKeyringReadCallback cb,
                            gpointer data)
{
	GError *error = NULL;
	PurpleKeyring *inuse;
	PurpleKeyringRead read_cb;

	if (purple_keyring_is_quitting) {
		purple_debug_error("keyring", "Cannot request a password while quitting.\n");
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"Cannot request a password while quitting.");
		if (cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
		return;
	}

	if (account == NULL) {
		purple_debug_error("keyring", "No account passed to the function.\n");
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"No account passed to the function.");

		if (cb != NULL)
			cb(account, NULL, error, data);

		g_error_free(error);

	} else {
		inuse = purple_keyring_get_inuse();

		if (inuse == NULL) {
			purple_debug_error("keyring", "No keyring configured.\n");
			error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOKEYRING,
				"No keyring configured.");

			if (cb != NULL)
				cb(account, NULL, error, data);

			g_error_free(error);

		} else {
			read_cb = purple_keyring_get_read_password(inuse);
			g_return_if_fail(read_cb != NULL);

			purple_debug_info("keyring", "Reading password for account %s (%s)...\n",
				purple_account_get_username(account),
				purple_account_get_protocol_id(account));
			read_cb(account, cb, data);
		}
	}
}

static void
purple_keyring_set_password_async_cb(PurpleAccount *account,
                                     GError *error,
                                     gpointer _sp_data)
{
	PurpleKeyringSetPasswordData *sp_data = _sp_data;

	g_return_if_fail(account != NULL);
	g_return_if_fail(sp_data != NULL);

	if (error == NULL && purple_debug_is_verbose()) {
		purple_debug_misc("keyring", "Password for account %s (%s) "
			"saved successfully.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else if (purple_debug_is_verbose()) {
		purple_debug_warning("keyring", "Password for account %s (%s) "
			"not saved successfully.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	}

	if (error != NULL) {
		purple_notify_error(NULL, _("Keyrings"),
			_("Failed to save password in keyring."),
			error->message);
	}

	if (sp_data->cb != NULL)
		sp_data->cb(account, error, sp_data->cb_data);
	g_free(sp_data);
}

void
purple_keyring_set_password(PurpleAccount *account,
                            const gchar *password,
                            PurpleKeyringSaveCallback cb,
                            gpointer data)
{
	GError *error = NULL;
	PurpleKeyring *inuse;
	PurpleKeyringSave save_cb;

	g_return_if_fail(account != NULL);

	if (purple_keyring_is_quitting) {
		purple_debug_error("keyring", "Cannot save a password while quitting.\n");
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"Cannot save a password while quitting.");
		if (cb != NULL)
			cb(account, error, data);
		g_error_free(error);
		return;
	}

	if (current_change_tracker != NULL) {
		purple_debug_error("keyring", "Cannot save a password during password migration.\n");
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"Cannot save a password during password migration.");
		if (cb != NULL)
			cb(account, error, data);
		g_error_free(error);
		return;
	}

	inuse = purple_keyring_get_inuse();
	if (inuse == NULL) {
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOKEYRING,
			"No keyring configured.");
		if (cb != NULL)
			cb(account, error, data);
		g_error_free(error);

	} else {
		PurpleKeyringSetPasswordData *sp_data;

		save_cb = purple_keyring_get_save_password(inuse);
		g_return_if_fail(save_cb != NULL);

		sp_data = g_new(PurpleKeyringSetPasswordData, 1);
		sp_data->cb = cb;
		sp_data->cb_data = data;
		purple_debug_info("keyring", "%s password for account %s (%s)...\n",
			(password ? "Saving" : "Removing"),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		save_cb(account, password, purple_keyring_set_password_async_cb, sp_data);
	}
}

void
purple_keyring_change_master(PurpleKeyringChangeMasterCallback cb,
                             gpointer data)
{
	GError *error = NULL;
	PurpleKeyringChangeMaster change;
	PurpleKeyring *inuse;

	inuse = purple_keyring_get_inuse();

	if (purple_keyring_is_quitting || current_change_tracker != NULL) {
		purple_debug_error("keyring", "Cannot change a master password at the moment.\n");
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_INTERNAL,
			"Cannot change a master password at the moment.");
		if (cb != NULL)
			cb(error, data);
		g_error_free(error);
		return;
	}

	if (inuse == NULL) {
		error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOKEYRING,
			"No keyring configured, cannot change master password.");
		purple_debug_error("keyring", "No keyring configured, cannot change master password.\n");
		if (cb)
			cb(error, data);
		g_error_free(error);

	} else {
		change = purple_keyring_get_change_master(inuse);

		if (change == NULL) {
			error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_BACKENDFAIL,
				"Keyring doesn't support master passwords.");
			if (cb)
				cb(error, data);

			g_error_free(error);

		} else {
			change(cb, data);

		}
	}
}


/**************************************************************************/
/* PurpleKeyring accessors                                                */
/**************************************************************************/

PurpleKeyring *
purple_keyring_new(void)
{
	return g_new0(PurpleKeyring, 1);
}

void
purple_keyring_free(PurpleKeyring *keyring)
{
	g_return_if_fail(keyring != NULL);

	g_free(keyring->name);
	g_free(keyring->id);
	g_free(keyring);
}

const gchar *
purple_keyring_get_name(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->name;
}

const gchar *
purple_keyring_get_id(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->id;
}

PurpleKeyringRead
purple_keyring_get_read_password(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->read_password;
}

PurpleKeyringSave
purple_keyring_get_save_password(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->save_password;
}

PurpleKeyringCancelRequests
purple_keyring_get_cancel_requests(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->cancel_requests;
}

PurpleKeyringClose
purple_keyring_get_close_keyring(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->close_keyring;
}

PurpleKeyringChangeMaster
purple_keyring_get_change_master(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->change_master;
}

PurpleKeyringImportPassword
purple_keyring_get_import_password(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->import_password;
}

PurpleKeyringExportPassword
purple_keyring_get_export_password(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->export_password;
}

void
purple_keyring_set_name(PurpleKeyring *keyring, const gchar *name)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(name != NULL);

	g_free(keyring->name);
	keyring->name = g_strdup(name);
}

void
purple_keyring_set_id(PurpleKeyring *keyring, const gchar *id)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(id != NULL);

	g_free(keyring->id);
	keyring->id = g_strdup(id);
}

void
purple_keyring_set_read_password(PurpleKeyring *keyring,
	PurpleKeyringRead read_cb)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(read_cb != NULL);

	keyring->read_password = read_cb;
}

void
purple_keyring_set_save_password(PurpleKeyring *keyring,
	PurpleKeyringSave save_cb)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(save_cb != NULL);

	keyring->save_password = save_cb;
}

void
purple_keyring_set_cancel_requests(PurpleKeyring *keyring,
	PurpleKeyringCancelRequests cancel_requests)
{
	g_return_if_fail(keyring != NULL);

	keyring->cancel_requests = cancel_requests;
}

void
purple_keyring_set_close_keyring(PurpleKeyring *keyring,
	PurpleKeyringClose close_cb)
{
	g_return_if_fail(keyring != NULL);

	keyring->close_keyring = close_cb;
}

void
purple_keyring_set_change_master(PurpleKeyring *keyring,
	PurpleKeyringChangeMaster change_master)
{
	g_return_if_fail(keyring != NULL);

	keyring->change_master = change_master;
}

void
purple_keyring_set_import_password(PurpleKeyring *keyring,
	PurpleKeyringImportPassword import_password)
{
	g_return_if_fail(keyring != NULL);

	keyring->import_password = import_password;
}

void
purple_keyring_set_export_password(PurpleKeyring *keyring,
	PurpleKeyringExportPassword export_password)
{
	g_return_if_fail(keyring != NULL);

	keyring->export_password = export_password;
}


/**************************************************************************/
/* Error Codes                                                            */
/**************************************************************************/

GQuark purple_keyring_error_domain(void)
{
	return g_quark_from_static_string("libpurple keyring");
}

/**************************************************************************/
/* Keyring Subsystem                                                      */
/**************************************************************************/

static void purple_keyring_core_initialized_cb(void)
{
	if (purple_keyring_inuse == NULL) {
		purple_notify_error(NULL, _("Keyrings"),
			_("Failed to load selected keyring."),
			_("Check your system configuration or select another "
			"one in Preferences dialog."));
	}
}

static void purple_keyring_core_quitting_cb()
{
	if (current_change_tracker != NULL) {
		PurpleKeyringChangeTracker *tracker;
		PurpleKeyringCancelRequests cancel = NULL;

		tracker = current_change_tracker;
		tracker->abort = TRUE;
		if (tracker->old) {
			cancel = purple_keyring_get_cancel_requests(
				tracker->old);
			if (cancel)
				cancel();
		}
		if (current_change_tracker == tracker && tracker->new) {
			cancel = purple_keyring_get_cancel_requests(
				tracker->new);
			if (cancel)
				cancel();
		}
	}
	purple_keyring_is_quitting = TRUE;
	if (purple_keyring_inuse != NULL) {
		PurpleKeyringCancelRequests cancel;
		cancel = purple_keyring_get_cancel_requests(
			purple_keyring_inuse);
		if (cancel)
			cancel();
	}
}

void
purple_keyring_init(void)
{
	const gchar *touse;
	GList *it;

	purple_keyring_keyrings = NULL;
	purple_keyring_inuse = NULL;

	purple_keyring_failed_imports = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,
		(GDestroyNotify)purple_keyring_failed_import_free);

	purple_signal_register(purple_keyring_get_handle(),
		"keyring-register",
		purple_marshal_VOID__POINTER_POINTER,
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),                    /* keyring ID */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	purple_signal_register(purple_keyring_get_handle(),
		"keyring-unregister",
		purple_marshal_VOID__POINTER_POINTER,
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),                    /* keyring ID */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	purple_signal_register(purple_keyring_get_handle(),
		"password-migration",
		purple_marshal_VOID__POINTER,
		NULL, 1,
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleAccount *")); /* a pointer to the account */

	/* see what keyring we want to use */
	touse = purple_prefs_get_string("/purple/keyring/active");

	if (touse == NULL) {
		purple_prefs_add_none("/purple/keyring");
		purple_prefs_add_string("/purple/keyring/active", PURPLE_DEFAULT_KEYRING);
		purple_keyring_to_use = g_strdup(PURPLE_DEFAULT_KEYRING);
	} else {
		purple_keyring_to_use = g_strdup(touse);
	}

	purple_keyring_pref_connect();

	for (it = purple_plugins_get_all(); it != NULL; it = it->next)
	{
		PurplePlugin *plugin = (PurplePlugin *)it->data;

		if (plugin->info == NULL || plugin->info->id == NULL)
			continue;
		if (strncmp(plugin->info->id, "keyring-", 8) != 0)
			continue;

		if (purple_plugin_is_loaded(plugin))
			continue;

		if (purple_plugin_load(plugin))
		{
			purple_keyring_loaded_plugins = g_list_append(
				purple_keyring_loaded_plugins, plugin);
		}
	}

	if (purple_keyring_inuse == NULL)
		purple_debug_error("keyring", "selected keyring failed to load\n");

	purple_signal_connect(purple_get_core(), "core-initialized",
		purple_keyring_get_handle(),
		PURPLE_CALLBACK(purple_keyring_core_initialized_cb), NULL);
	purple_signal_connect(purple_get_core(), "quitting",
		purple_keyring_get_handle(),
		PURPLE_CALLBACK(purple_keyring_core_quitting_cb), NULL);
}

void
purple_keyring_uninit(void)
{
	GList *it;

	g_free(purple_keyring_to_use);
	purple_keyring_inuse = NULL;

	for (it = g_list_first(purple_keyring_loaded_plugins); it != NULL;
		it = g_list_next(it))
	{
		PurplePlugin *plugin = (PurplePlugin *)it->data;
		if (g_list_find(purple_plugins_get_loaded(), plugin) == NULL)
			continue;
		purple_plugin_unload(plugin);
	}
	g_list_free(purple_keyring_loaded_plugins);
	purple_keyring_loaded_plugins = NULL;

	purple_signals_unregister_by_instance(purple_keyring_get_handle());
	purple_signals_disconnect_by_handle(purple_keyring_get_handle());
	purple_keyring_pref_disconnect();

	g_hash_table_destroy(purple_keyring_failed_imports);
	purple_keyring_failed_imports = NULL;
}

void *
purple_keyring_get_handle(void)
{
	static int handle;

	return &handle;
}
