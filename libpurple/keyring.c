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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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

typedef struct _PurpleKeyringCbInfo PurpleKeyringCbInfo;
typedef struct _PurpleKeyringChangeTracker PurpleKeyringChangeTracker;

/******************************************/
/** @name PurpleKeyring                   */
/******************************************/
/*@{*/

struct _PurpleKeyring
{
	char *name; /* a user friendly name */
	char *id;   /* same as plugin id    */
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringCancelRequests cancel_requests;
	PurpleKeyringClose close_keyring;
	PurpleKeyringChangeMaster change_master;
	PurpleKeyringImportPassword import_password;
	PurpleKeyringExportPassword export_password;
};

struct _PurpleKeyringChangeTracker
{
	GError *error;
	PurpleKeyringSetInUseCallback cb;
	gpointer data;
	PurpleKeyring *new;
	PurpleKeyring *old; /* we are done when: finished == TRUE && read_outstanding == 0 */
	int read_outstanding;
	gboolean finished;
	gboolean abort;
	gboolean force;
	gboolean succeeded;
};

struct _PurpleKeyringCbInfo
{
	gpointer cb;
	gpointer data;
};

typedef void (*PurpleKeyringDropCallback)(gpointer data);

typedef struct
{
	PurpleKeyringDropCallback cb;
	gpointer cb_data;
	int drop_outstanding;
	gboolean finished;
} PurpleKeyringDropTracker;

static void
purple_keyring_close(PurpleKeyring *keyring);

static void
purple_keyring_change_tracker_free(PurpleKeyringChangeTracker *tracker)
{
	if (tracker->error)
		g_error_free(tracker->error);
	g_free(tracker);
}

/* Constructor */
PurpleKeyring *
purple_keyring_new(void)
{
	return g_new0(PurpleKeyring, 1);
}

/* Destructor */
void
purple_keyring_free(PurpleKeyring *keyring)
{
	g_free(keyring->name);
	g_free(keyring->id);
	g_free(keyring);
}

/* Accessors */
const char *
purple_keyring_get_name(const PurpleKeyring *keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->name;
}

const char *
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
purple_keyring_set_name(PurpleKeyring *keyring, const char *name)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(name != NULL);

	g_free(keyring->name);
	keyring->name = g_strdup(name);
}

void
purple_keyring_set_id(PurpleKeyring *keyring, const char *id)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(id != NULL);

	g_free(keyring->id);
	keyring->id = g_strdup(id);
}

void
purple_keyring_set_read_password(PurpleKeyring *keyring, PurpleKeyringRead read_cb)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(read_cb != NULL);

	keyring->read_password = read_cb;
}

void
purple_keyring_set_save_password(PurpleKeyring *keyring, PurpleKeyringSave save_cb)
{
	g_return_if_fail(keyring != NULL);
	g_return_if_fail(save_cb != NULL);

	keyring->save_password = save_cb;
}

void
purple_keyring_set_cancel_requests(PurpleKeyring *keyring, PurpleKeyringCancelRequests cancel_requests)
{
	g_return_if_fail(keyring != NULL);

	keyring->cancel_requests = cancel_requests;
}

void
purple_keyring_set_close_keyring(PurpleKeyring *keyring, PurpleKeyringClose close_cb)
{
	g_return_if_fail(keyring != NULL);

	keyring->close_keyring = close_cb;
}

void
purple_keyring_set_change_master(PurpleKeyring *keyring, PurpleKeyringChangeMaster change)
{
	g_return_if_fail(keyring != NULL);

	keyring->change_master = change;
}

void
purple_keyring_set_import_password(PurpleKeyring *keyring, PurpleKeyringImportPassword import)
{
	g_return_if_fail(keyring != NULL);

	keyring->import_password = import;
}

void
purple_keyring_set_export_password(PurpleKeyring *keyring, PurpleKeyringExportPassword export)
{
	g_return_if_fail(keyring != NULL);

	keyring->export_password = export;
}

/*@}*/


/***************************************/
/** @name Keyring API                  */
/***************************************/
/*@{*/

static GList *purple_keyring_keyrings;              /* list of available keyrings */
static PurpleKeyring *purple_keyring_inuse; /* keyring being used */
static char *purple_keyring_to_use;
static guint purple_keyring_pref_cb_id;
static GList *purple_keyring_loaded_plugins = NULL;
static PurpleKeyringChangeTracker *current_change_tracker = NULL;
static gboolean purple_keyring_is_quitting = FALSE;

static void
purple_keyring_pref_cb(const char *pref,
		       PurplePrefType type,
		       gconstpointer id,
		       gpointer data)
{
	PurpleKeyring *new;

	g_return_if_fail(g_strcmp0(pref, "/purple/keyring/active") == 0);
	g_return_if_fail(type == PURPLE_PREF_STRING);
	g_return_if_fail(id != NULL);

	new = purple_keyring_find_keyring_by_id(id);
	g_return_if_fail(new != NULL);

	purple_keyring_set_inuse(new, FALSE, NULL, data);
}

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
	const char *touse;
	GList *it;

	purple_keyring_keyrings = NULL;
	purple_keyring_inuse = NULL;

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

	/* see what keyring we want to use */
	touse = purple_prefs_get_string("/purple/keyring/active");

	if (touse == NULL) {
		purple_prefs_add_none("/purple/keyring");
		purple_prefs_add_string("/purple/keyring/active", PURPLE_DEFAULT_KEYRING);
		purple_keyring_to_use = g_strdup(PURPLE_DEFAULT_KEYRING);
	} else {
		purple_keyring_to_use = g_strdup(touse);
	}

	purple_keyring_pref_cb_id = purple_prefs_connect_callback(NULL,
		"/purple/keyring/active", purple_keyring_pref_cb, NULL);

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
	purple_prefs_disconnect_callback(purple_keyring_pref_cb_id);
	purple_keyring_pref_cb_id = 0;
}

void *
purple_keyring_get_handle(void)
{
	static int handle;

	return &handle;
}

PurpleKeyring *
purple_keyring_find_keyring_by_id(const char *id)
{
	GList *l;
	PurpleKeyring *keyring;
	const char *curr_id;

	for (l = purple_keyring_keyrings; l != NULL; l = l->next) {
		keyring = l->data;
		curr_id = purple_keyring_get_id(keyring);

		if (g_strcmp0(id, curr_id) == 0)
			return keyring;
	}

	return NULL;
}

PurpleKeyring *
purple_keyring_get_inuse(void)
{
	return purple_keyring_inuse;
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
			tracker->cb(tracker->new, NULL, tracker->data);
	} else {
		purple_debug_error("keyring",
			"Failed to change keyring, aborting.\n");

		purple_keyring_close(tracker->new);

		purple_prefs_disconnect_callback(purple_keyring_pref_cb_id);
		purple_prefs_set_string("/purple/keyring/active",
			purple_keyring_get_id(tracker->old));
		purple_keyring_pref_cb_id = purple_prefs_connect_callback(NULL,
			"/purple/keyring/active", purple_keyring_pref_cb, NULL);

		current_change_tracker = NULL;

		if (tracker->error == NULL) {
			tracker->error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_UNKNOWN,
				"Unknown error has occured");
		}
		if (tracker->cb != NULL)
			tracker->cb(tracker->old, tracker->error, tracker->data);
	}

	purple_keyring_change_tracker_free(tracker);
}

static void
purple_keyring_set_inuse_check_error_cb(PurpleAccount *account,
					GError *error,
					gpointer data)
{
	const char *name;
	PurpleKeyringChangeTracker *tracker;

	tracker = (PurpleKeyringChangeTracker *)data;

	tracker->read_outstanding--;

	name = purple_account_get_username(account);

	if ((error != NULL) && (error->domain == PURPLE_KEYRING_ERROR)) {
		switch(error->code) {
			case PURPLE_KEYRING_ERROR_NOPASSWORD:
				if (purple_debug_is_verbose()) {
					purple_debug_misc("keyring",
						"No password found while changing keyring for account %s: %s.\n",
						name, error->message);
				}
				break;

			case PURPLE_KEYRING_ERROR_CANCELLED:
				purple_debug_info("keyring",
					"Operation cancelled while changing keyring for account %s: %s.\n",
					name, error->message);
				tracker->abort = TRUE;
				if (tracker->error != NULL)
					g_error_free(tracker->error);
				tracker->error = g_error_copy(error);
				break;

			case PURPLE_KEYRING_ERROR_BACKENDFAIL:
				purple_debug_error("keyring",
					"Failed to communicate with backend while changing keyring for account %s: %s. Aborting changes.\n",
					name, error->message);
				tracker->abort = TRUE;
				if (tracker->error != NULL)
					g_error_free(tracker->error);
				tracker->error = g_error_copy(error);
				break;

			default:
				purple_debug_error("keyring",
					"Unknown error while changing keyring for account %s: %s. Aborting changes.\n",
					name, error->message);
				tracker->abort = TRUE;
				if (tracker->error == NULL)
					tracker->error = g_error_copy(error);
				break;
		}
	}

	/**
	 * This is kind of hackish. It will schedule an account save.
	 *
	 * Another way to do this would be to expose the
	 * schedule_accounts_save() function, but other such functions
	 * are not exposed. So these was done for consistency.
	 */
	purple_account_set_remember_password(account,
		purple_account_get_remember_password(account));

	/* if this was the last one */
	if (tracker->finished && tracker->read_outstanding == 0) {
		if (tracker->abort && !tracker->force) {
			tracker->succeeded = FALSE;
			purple_keyring_drop_passwords(tracker->new, purple_keyring_set_inuse_drop_cb, tracker);
		} else {
			tracker->succeeded = TRUE;
			purple_keyring_drop_passwords(tracker->old, purple_keyring_set_inuse_drop_cb, tracker);
		}
	}
}

static void
purple_keyring_set_inuse_got_pw_cb(PurpleAccount *account,
                                   const gchar *password,
                                   GError *error,
                                   gpointer data)
{
	PurpleKeyring *new;
	PurpleKeyringSave save_cb;
	PurpleKeyringChangeTracker *tracker;

	tracker = (PurpleKeyringChangeTracker *)data;
	new = tracker->new;

	g_return_if_fail(tracker != NULL);
	if (tracker->abort) {
		purple_keyring_set_inuse_check_error_cb(account, NULL, data);
		return;
	}

	if (error != NULL) {
		if (error->code == PURPLE_KEYRING_ERROR_NOPASSWORD ||
			tracker->force == TRUE) {
			/* don't save password, and ignore it */
		} else {
			/* fatal error, abort all */
			tracker->abort = TRUE;
		}
		purple_keyring_set_inuse_check_error_cb(account, error, data);
	} else {
		save_cb = purple_keyring_get_save_password(new);
		g_return_if_fail(save_cb != NULL);

		save_cb(account, password, purple_keyring_set_inuse_check_error_cb,
			tracker);
	}
}

void
purple_keyring_set_inuse(PurpleKeyring *newkeyring,
                         gboolean force,
                         PurpleKeyringSetInUseCallback cb,
                         gpointer data)
{
	GList *cur;
	PurpleKeyring *oldkeyring;
	PurpleKeyringRead read_cb = NULL;
	PurpleKeyringChangeTracker *tracker;

	oldkeyring = purple_keyring_get_inuse();

	if (current_change_tracker != NULL) {
		GError *error;
		purple_debug_error("keyring", "There is password migration "
			"session already running.\n");
		if (cb == NULL)
			return;
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_INTERNAL,
			"There is a password migration session already running");
		cb(oldkeyring, error, data);
		g_error_free(error);
		return;
	}

	if (oldkeyring == newkeyring) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring",
				"Old and new keyring are the same: %s.\n",
				(newkeyring != NULL) ?
					newkeyring->id : "(null)");
		}
		if (cb != NULL)
			cb(newkeyring, NULL, data);
		return;
	}

	purple_debug_info("keyring", "Attempting to set new keyring: %s.\n",
		(newkeyring != NULL) ? newkeyring->id : "(null)");

	if (oldkeyring != NULL) {
		read_cb = purple_keyring_get_read_password(oldkeyring);
		g_return_if_fail(read_cb != NULL);

		purple_debug_misc("keyring", "Starting migration from: %s.\n",
			oldkeyring->id);

		tracker = g_new(PurpleKeyringChangeTracker, 1);
		current_change_tracker = tracker;

		tracker->cb = cb;
		tracker->data = data;
		tracker->new = newkeyring;
		tracker->old = oldkeyring;
		tracker->read_outstanding = 0;
		tracker->finished = FALSE;
		tracker->abort = FALSE;
		tracker->force = force;
		tracker->error = NULL;

		for (cur = purple_accounts_get_all(); cur != NULL;
			cur = cur->next) {

			if (tracker->abort) {
				tracker->finished = TRUE;
				break;
			}
			tracker->read_outstanding++;

			if (cur->next == NULL)
				tracker->finished = TRUE;

			read_cb(cur->data, purple_keyring_set_inuse_got_pw_cb, tracker);
		}
	} else { /* no keyring was set before. */
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring", "Setting keyring for the "
				"first time: %s.\n", newkeyring->id);
		}

		purple_keyring_inuse = newkeyring;
		g_assert(current_change_tracker == NULL);
		if (cb != NULL)
			cb(newkeyring, NULL, data);
	}
}

GList *
purple_keyring_get_options(void)
{
	const GList *keyrings;
	PurpleKeyring *keyring;
	GList *list = NULL;
	static char currentDisabledName[40];

	if (purple_keyring_get_inuse() == NULL && purple_keyring_to_use != NULL
		&& purple_keyring_to_use[0] != '\0') {
		g_snprintf(currentDisabledName, sizeof(currentDisabledName),
			_("%s (disabled)"), purple_keyring_to_use);
		list = g_list_append(list, currentDisabledName);
		list = g_list_append(list, purple_keyring_to_use);
	}

	for (keyrings = purple_keyring_keyrings; keyrings != NULL;
		keyrings = keyrings->next) {

		keyring = keyrings->data;
		list = g_list_append(list, keyring->name);
		list = g_list_append(list, keyring->id);
	}

	return list;
}

void
purple_keyring_register(PurpleKeyring *keyring)
{
	const char *keyring_id;

	g_return_if_fail(keyring != NULL);

	keyring_id = purple_keyring_get_id(keyring);

	purple_debug_info("keyring", "Registering keyring: %s.\n",
		keyring->id ? keyring->id : "(null)");

	if (purple_keyring_get_id(keyring) == NULL ||
		purple_keyring_get_name(keyring) == NULL ||
		purple_keyring_get_read_password(keyring) == NULL ||
		purple_keyring_get_save_password(keyring) == NULL) {
		purple_debug_error("keyring", "Invalid keyring %s, some "
			"required fields are missing.\n",
			keyring->id ? keyring->id : "(null)");
		return;
	}

	/* If this is the configured keyring, use it. */
	if (purple_keyring_inuse == NULL &&
	    g_strcmp0(keyring_id, purple_keyring_to_use) == 0) {

		purple_debug_info("keyring", "Keyring %s matches keyring to use, using it.\n",
			keyring->id);
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
	const char *keyring_id;

	g_return_if_fail(keyring != NULL);

	purple_debug_info("keyring", "Unregistering keyring: %s.\n",
		purple_keyring_get_id(keyring));

	keyring_id = purple_keyring_get_id(keyring);
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

/*@}*/


/***************************************/
/** @name Keyring plugin wrappers      */
/***************************************/
/*@{*/

gboolean
purple_keyring_import_password(PurpleAccount *account,
                               const char *keyringid,
                               const char *mode,
                               const char *data,
                               GError **error)
{
	PurpleKeyring *inuse;
	PurpleKeyringImportPassword import;
	const char *realid;

	purple_debug_misc("keyring", "Importing password for account %s (%s) to keyring %s.\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account),
		keyringid);

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_NOKEYRING,
				"No keyring configured, cannot import password "
				"info");
		}
		purple_debug_error("Keyring",
			"No keyring configured, cannot import password info for account %s (%s).\n",
			purple_account_get_username(account), purple_account_get_protocol_id(account));
		return FALSE;
	}

	realid = purple_keyring_get_id(inuse);
	/*
	 * we want to be sure that either :
	 *  - there is a keyringid specified and it matches the one configured
	 *  - or the configured keyring is the fallback, compatible one.
	 */
	if ((keyringid != NULL && g_strcmp0(realid, keyringid) != 0) ||
	    (keyringid == NULL && g_strcmp0(PURPLE_DEFAULT_KEYRING, realid))) {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_INTERNAL,
				"Specified keyring id does not match the "
				"configured one.");
		}
		purple_debug_info("keyring",
			"Specified keyring id does not match the configured one (%s vs. %s). Data will be lost.\n",
			keyringid, realid);
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
                               const char **keyringid,
                               const char **mode,
                               char **data,
                               GError **error,
                               GDestroyNotify *destroy)
{
	PurpleKeyring *inuse;
	PurpleKeyringExportPassword export;

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		*error = g_error_new(PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_NOKEYRING,
			"No keyring configured, cannot export password info");
		purple_debug_error("keyring",
			"No keyring configured, cannot export password info.\n");
		return FALSE;
	}

	*keyringid = purple_keyring_get_id(inuse);

	if (purple_debug_is_verbose()) {
		purple_debug_misc("keyring",
			"Exporting password for account %s (%s) from keyring "
			"%s...\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account), *keyringid);
	}

	if (*keyringid == NULL) {
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
                                     gpointer data)
{
	PurpleKeyringCbInfo *cbinfo;
	PurpleKeyringSaveCallback cb;

	g_return_if_fail(data != NULL);
	g_return_if_fail(account != NULL);

	cbinfo = data;
	cb = cbinfo->cb;

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

	if (cb != NULL)
		cb(account, error, cbinfo->data);
	g_free(data);
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
	PurpleKeyringCbInfo *cbinfo;

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
		save_cb = purple_keyring_get_save_password(inuse);
		g_return_if_fail(save_cb != NULL);

		cbinfo = g_new(PurpleKeyringCbInfo, 1);
		cbinfo->cb = cb;
		cbinfo->data = data;
		purple_debug_info("keyring", "%s password for account %s (%s)...\n",
			(password ? "Saving" : "Removing"),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		save_cb(account, password, purple_keyring_set_password_async_cb, cbinfo);
	}
}

static void
purple_keyring_close(PurpleKeyring *keyring)
{
	PurpleKeyringClose close_cb;

	g_return_if_fail(keyring != NULL);

	close_cb = purple_keyring_get_close_keyring(keyring);

	if (close_cb != NULL)
		close_cb();
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

/*@}*/


/***************************************/
/** @name Error Codes                  */
/***************************************/
/*@{*/

GQuark purple_keyring_error_domain(void)
{
	return g_quark_from_static_string("libpurple keyring");
}

/*}@*/

