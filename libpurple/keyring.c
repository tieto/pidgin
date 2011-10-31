/**
 * @file keyring.c Keyring plugin API
 * @ingroup core
 * @todo ? : add dummy callback to all calls to prevent poorly written
 * plugins from segfaulting on NULL callback ?
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

typedef struct _PurpleKeyringCbInfo PurpleKeyringCbInfo;
typedef struct _PurpleKeyringChangeTracker PurpleKeyringChangeTracker;

/******************************************/
/** @name PurpleKeyring                   */
/******************************************/
/*@{*/

struct _PurpleKeyring
{
	char * name;    /* a user friendly name */
	char * id;      /* same as plugin id    */
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringClose close_keyring;
	PurpleKeyringChangeMaster change_master;
	PurpleKeyringImportPassword import_password;
	PurpleKeyringExportPassword export_password;
};

struct _PurpleKeyringChangeTracker
{
	GError * error;			/* could probably be dropped */
	PurpleKeyringSetInUseCallback cb;
	gpointer data;
	const PurpleKeyring * new;
	const PurpleKeyring * old;	/* we are done when : finished == TRUE && read_outstanding == 0 */
	int read_outstanding;
	gboolean finished;
	gboolean abort;
	gboolean force;
};

struct _PurpleKeyringCbInfo
{
	gpointer cb;
	gpointer data;
};

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
	g_free(keyring);
}

/* Accessors */
const char * 
purple_keyring_get_name(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->name;
}

const char * 
purple_keyring_get_id(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->id;
}

PurpleKeyringRead 
purple_keyring_get_read_password(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->read_password;
}

PurpleKeyringSave 
purple_keyring_get_save_password(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->save_password;
}

PurpleKeyringClose 
purple_keyring_get_close_keyring(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->close_keyring;
}

PurpleKeyringChangeMaster 
purple_keyring_get_change_master(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->change_master;
}

PurpleKeyringImportPassword 
purple_keyring_get_import_password(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->import_password;
}

PurpleKeyringExportPassword 
purple_keyring_get_export_password(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->export_password;
}

void 
purple_keyring_set_name(PurpleKeyring * keyring, char * name)
{
	g_return_if_fail(keyring != NULL);

	g_free(keyring->name);
	keyring->name = g_strdup(name);
}

void 
purple_keyring_set_id(PurpleKeyring * keyring, char * id)
{
	g_return_if_fail(keyring != NULL);

	g_free(keyring->id);
	keyring->id = g_strdup(id);
}

void
purple_keyring_set_read_password(PurpleKeyring * keyring, PurpleKeyringRead read)
{
	g_return_if_fail(keyring != NULL);

	keyring->read_password = read;
}

void
purple_keyring_set_save_password(PurpleKeyring * keyring, PurpleKeyringSave save)
{
	g_return_if_fail(keyring != NULL);

	keyring->save_password = save;
}

void
purple_keyring_set_close_keyring(PurpleKeyring * keyring, PurpleKeyringClose close)
{
	g_return_if_fail(keyring != NULL);

	keyring->close_keyring = close;
}

void
purple_keyring_set_change_master(PurpleKeyring * keyring, PurpleKeyringChangeMaster change)
{
	g_return_if_fail(keyring != NULL);

	keyring->change_master = change;
}

void
purple_keyring_set_import_password(PurpleKeyring * keyring, PurpleKeyringImportPassword import)
{
	g_return_if_fail(keyring != NULL);

	keyring->import_password = import;
}

void
purple_keyring_set_export_password(PurpleKeyring * keyring, PurpleKeyringExportPassword export)
{
	g_return_if_fail(keyring != NULL);

	keyring->export_password = export;
}

/*@}*/


/***************************************/
/** @name Keyring API                  */
/***************************************/
/*@{*/

static GList * purple_keyring_keyrings;		/* list of available keyrings   */
static const PurpleKeyring * purple_keyring_inuse;	/* keyring being used	        */
static char * purple_keyring_to_use;
static guint purple_keyring_pref_cb_id;

static void 
purple_keyring_pref_cb(const char *pref,
		       PurplePrefType type,
		       gconstpointer id,
		       gpointer data)
{
	PurpleKeyring * new;

	g_return_if_fail(g_strcmp0(pref, "/purple/keyring/active") == 0);
	g_return_if_fail(type == PURPLE_PREF_STRING);
	g_return_if_fail(id != NULL);

	new = purple_keyring_get_keyring_by_id(id);
	g_return_if_fail(new != NULL);

	purple_keyring_set_inuse(new, FALSE, NULL, data);
}

void 
purple_keyring_init()
{
	PurpleCore * core;
	const char * touse;

	/* Make sure we don't have junk */
	purple_keyring_keyrings = NULL;
	purple_keyring_inuse = NULL;

	/* register signals */
	core = purple_get_core();

	purple_signal_register(core, "keyring-register",
		purple_marshal_VOID__POINTER_POINTER, 
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),			/* keyring ID */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	purple_signal_register(core, "keyring-unregister",
		purple_marshal_VOID__POINTER_POINTER, 
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),			/* keyring ID */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	/* see what keyring we want to use */
	touse = purple_prefs_get_string("/purple/keyring/active");

	if (touse == NULL) {
		purple_prefs_add_none("/purple/keyring");
		purple_prefs_add_string("/purple/keyring/active", FALLBACK_KEYRING);
		purple_keyring_to_use = g_strdup(FALLBACK_KEYRING);

	} else {

		purple_keyring_to_use = g_strdup(touse);
	}

	purple_keyring_pref_cb_id = purple_prefs_connect_callback(NULL, 
		"/purple/keyring/active", purple_keyring_pref_cb, NULL);

	purple_debug_info("keyring", "purple_keyring_init() done, selected keyring is : %s.\n",
		purple_keyring_to_use);
}

void
purple_keyring_uninit()
{
	g_free(purple_keyring_to_use);
	purple_debug_info("keyring", "purple_keyring_uninit() done.\n");
}

static PurpleKeyring *
purple_keyring_find_keyring_by_id(char * id)
{
	GList * l;
	PurpleKeyring * keyring;
	const char * curr_id;

	for (l = purple_keyring_keyrings; l != NULL; l = l->next) {
		keyring = l->data;
		curr_id = purple_keyring_get_id(keyring);

		if (g_strcmp0(id, curr_id) == 0)
			return keyring;
	}

	return NULL;
}

const GList * 
purple_keyring_get_keyrings()
{
	return purple_keyring_keyrings;
}

const PurpleKeyring * 
purple_keyring_get_inuse()
{
	return purple_keyring_inuse;
}


/* fire and forget */
static void
purple_keyring_drop_passwords(const PurpleKeyring * keyring)
{
	GList * cur;
	PurpleKeyringSave save;

	save = purple_keyring_get_save_password(keyring);

	g_return_if_fail(save != NULL);

	for (cur = purple_accounts_get_all();
	     cur != NULL;
	     cur = cur->next)
		save(cur->data, NULL, NULL, NULL);
}



static void
purple_keyring_set_inuse_check_error_cb(PurpleAccount * account,
					GError * error,
					gpointer data)
{

	const char * name;
	PurpleKeyringClose close;
	struct _PurpleKeyringChangeTracker * tracker;

	tracker = (PurpleKeyringChangeTracker *)data;

	g_return_if_fail(tracker->abort == FALSE);

	tracker->read_outstanding--;

	name = purple_account_get_username(account);;


	if ((error != NULL) && (error->domain == ERR_PIDGINKEYRING)) {

		tracker->error = error;

		switch(error->code) {

			case ERR_NOCAP :
				purple_debug_info("keyring",
					"Keyring could not save password for account %s : %s\n",
					name, error->message);
				break;

			case ERR_NOPASSWD :
				purple_debug_info("keyring",
					"No password found while changing keyring for account %s : %s\n",
					name, error->message);
				break;

			case ERR_NOCHANNEL :
				purple_debug_info("keyring",
					"Failed to communicate with backend while changing keyring for account %s : %s Aborting changes.\n",
					name, error->message);
				tracker->abort = TRUE;
				break;

			default :
				purple_debug_info("keyring",
					"Unknown error while changing keyring for account %s : %s\n",
					name, error->message);
				break;
		}
	}
	/* if this was the last one */
	if (tracker->finished == TRUE && tracker->read_outstanding == 0) {
	
		if (tracker->abort == TRUE && tracker->force == FALSE) {

			if (tracker->cb != NULL)
				tracker->cb(tracker->old, FALSE, tracker->error, tracker->data);

			purple_keyring_drop_passwords(tracker->new);

			close = purple_keyring_get_close_keyring(tracker->new);
			if (close != NULL)
				close(NULL);

			purple_debug_info("keyring",
				"Failed to change keyring, aborting");

			purple_notify_error(NULL, _("Keyrings"), _("Failed to change the keyring."),
				_("Aborting changes."));
			purple_keyring_inuse = tracker->old;
			purple_prefs_disconnect_callback(purple_keyring_pref_cb_id);
			purple_prefs_set_string("/purple/keyring/active",
				purple_keyring_get_id(tracker->old));
			purple_keyring_pref_cb_id = purple_prefs_connect_callback(NULL, 
				"/purple/keyring/active", purple_keyring_pref_cb, NULL);


		} else {
			close = purple_keyring_get_close_keyring(tracker->old);
			if (close != NULL)
				close(&error);

			purple_keyring_drop_passwords(tracker->old);

			purple_debug_info("keyring",
				"Successfully changed keyring.\n");

			if (tracker->cb != NULL)
				tracker->cb(tracker->new, TRUE, error, tracker->data);
		}

		g_free(tracker);
	}
	/**
	 * This is kind of hackish. It will schedule an account save,
	 * and then return because account == NULL.
	 * Another way to do this would be to expose the
	 * schedule_accounts_save() function, but other such functions
	 * are not exposed. So these was done for consistency.
	 */
	purple_account_set_password(NULL, NULL, NULL, NULL);
}


static void
purple_keyring_set_inuse_got_pw_cb(PurpleAccount * account, 
				  gchar * password, 
				  GError * error, 
				  gpointer data)
{
	const PurpleKeyring * new;
	PurpleKeyringSave save;
	PurpleKeyringChangeTracker * tracker;

	tracker = (PurpleKeyringChangeTracker *)data;
	new = tracker->new;

	g_return_if_fail(tracker->abort == FALSE);

	if (error != NULL) {

		if (error->code == ERR_NOPASSWD ||
		    error->code == ERR_NOACCOUNT ||
		    tracker->force == TRUE) {

			/* don't save password, and directly trigger callback */
			purple_keyring_set_inuse_check_error_cb(account, error, data);

		} else {

			/* fatal error, abort all */
			tracker->abort = TRUE;
		}

	} else {


		save = purple_keyring_get_save_password(new);

		if (save != NULL) {
			/* this test is probably totally useless, since there's no point
			 * in having a keyring that can't store passwords, but it
			 * will prevent crash with invalid keyrings
			 */
			save(account, password, 
			     purple_keyring_set_inuse_check_error_cb, tracker);

		} else {
			error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
				"cannot store passwords in new keyring");
			purple_keyring_set_inuse_check_error_cb(account, error, data);
			g_error_free(error);
		}
	}
}




void
purple_keyring_set_inuse(const PurpleKeyring * newkeyring,
			 gboolean force,
			 PurpleKeyringSetInUseCallback cb,
			 gpointer data)
{
	GList * cur;
	const PurpleKeyring * oldkeyring;
	PurpleKeyringRead read = NULL;
	PurpleKeyringClose close;
	PurpleKeyringChangeTracker * tracker;

	if (newkeyring != NULL)
		purple_debug_info("keyring", "Attempting to set new keyring : %s.\n",
			newkeyring->id);
	else
		purple_debug_info("keyring", "Attempting to set new keyring : NULL.\n");

	oldkeyring = purple_keyring_get_inuse();

	if (oldkeyring != NULL) {
		read = purple_keyring_get_read_password(oldkeyring);

		if (read == NULL) {
			/*
			error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
				"Existing keyring cannot read passwords");
			*/
			purple_debug_info("keyring", "Existing keyring cannot read passwords");

			/* at this point, we know the keyring won't let us
			 * read passwords, so there no point in copying them.
			 * therefore we just cleanup the old and setup the new 
			 * one later.
			 */

			purple_keyring_drop_passwords(oldkeyring);

			close = purple_keyring_get_close_keyring(oldkeyring);

			if (close != NULL)
				close(NULL);	/* we can't do much about errors at this point*/

		} else {
			tracker = g_malloc(sizeof(PurpleKeyringChangeTracker));
			oldkeyring = purple_keyring_get_inuse();

			purple_keyring_inuse = newkeyring;

			tracker->cb = cb;
			tracker->data = data;
			tracker->new = newkeyring;
			tracker->old = oldkeyring;
			tracker->read_outstanding = 0;
			tracker->finished = FALSE;
			tracker->abort = FALSE;
			tracker->force = force;
			tracker->error = NULL;

			for (cur = purple_accounts_get_all(); 
			    (cur != NULL) && (tracker->abort == FALSE);
			    cur = cur->next) {

				tracker->read_outstanding++;

				if (cur->next == NULL)
					tracker->finished = TRUE;

				read(cur->data, purple_keyring_set_inuse_got_pw_cb, tracker);
			}
		}

	} else { /* no keyring was set before. */
		purple_debug_info("keyring", "Setting keyring for the first time : %s.\n",
			newkeyring->id);
		purple_keyring_inuse = newkeyring;

		if (cb != NULL)
			cb(newkeyring, TRUE, NULL, data);
	}
}



GList *
purple_keyring_get_options()
{
	const GList * keyrings;
	PurpleKeyring * keyring;
	GList * list = NULL;

	for (keyrings = purple_keyring_get_keyrings();
	     keyrings != NULL;
	     keyrings = keyrings->next) {

		keyring = keyrings->data;
		list = g_list_append(list, keyring->name);
		list = g_list_append(list, keyring->id);
		purple_debug_info("keyring", "adding pair : %s, %s.\n",
			keyring->name, keyring->id);
	}

	return list;
}

PurpleKeyring *
purple_keyring_get_keyring_by_id(const char * id)
{
	const GList * keyrings;
	PurpleKeyring * keyring;

	for (keyrings = purple_keyring_get_keyrings();
	     keyrings != NULL;
	     keyrings = keyrings->next) {

		keyring = keyrings->data;
		if (g_strcmp0(id, keyring->id) == 0)
			return keyring;

	}
	return NULL;
}



void 
purple_keyring_register(PurpleKeyring * keyring)
{
	const char * keyring_id;
	PurpleCore * core;

	g_return_if_fail(keyring != NULL);
	
	keyring_id = purple_keyring_get_id(keyring);

	/* keyring with no ID. Add error handling ? */
	g_return_if_fail(keyring_id != NULL);


	purple_debug_info("keyring", "Registering keyring : %s\n",
		keyring->id);

	/* If this is the configured keyring, use it. */
	if (purple_keyring_inuse == NULL &&
	    g_strcmp0(keyring_id, purple_keyring_to_use) == 0) {

		purple_debug_info("keyring", "Keyring %s matches keyring to use, using it.\n",
			keyring->id);
		purple_keyring_set_inuse(keyring, TRUE, NULL, NULL);

	}

	core = purple_get_core();

	purple_signal_emit(core, "keyring-register", keyring_id, keyring);
	purple_debug_info("keyring", "registered keyring : %s.\n", keyring_id);

	purple_keyring_keyrings = g_list_prepend(purple_keyring_keyrings,
		keyring);
}


void 
purple_keyring_unregister(PurpleKeyring * keyring)
{
	PurpleCore * core;
	const PurpleKeyring * inuse;
	PurpleKeyring * fallback;
	const char * keyring_id;

	g_return_if_fail(keyring != NULL);
	
	purple_debug_info("keyring", 
		"Unregistering keyring %s",
		purple_keyring_get_id(keyring));

	core = purple_get_core();
	keyring_id = purple_keyring_get_id(keyring);
	purple_signal_emit(core, "keyring-unregister", keyring_id, keyring);

	inuse = purple_keyring_get_inuse();
	fallback = purple_keyring_find_keyring_by_id(FALLBACK_KEYRING);

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

	purple_debug_info("keyring", "Keyring %s unregistered", keyring->id);
}


/*@}*/


/***************************************/
/** @name Keyring plugin wrappers      */
/***************************************/
/*@{*/


gboolean
purple_keyring_import_password(PurpleAccount * account, 
			       const char * keyringid,
			       const char * mode,
			       const char * data,
			       GError ** error)
{
	const PurpleKeyring * inuse;
	PurpleKeyringImportPassword import;
	const char * realid;

	purple_debug_info("keyring", "Importing password for account %s (%s) to keyring %s.\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account),
		keyringid);

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOKEYRING,
			"No keyring configured, cannot import password info");
		purple_debug_info("Keyring", 
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
	    (keyringid == NULL && g_strcmp0(FALLBACK_KEYRING, realid))) {

		*error = g_error_new(ERR_PIDGINKEYRING , ERR_INVALID,
			"Specified keyring id does not match the configured one.");
		purple_debug_info("keyring",
			"Specified keyring id does not match the configured one (%s vs. %s). Data will be lost.\n",
			keyringid, realid);
		return FALSE;
	}

	import = purple_keyring_get_import_password(inuse);
	if (import == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
			"Keyring cannot import password info.");
		purple_debug_info("Keyring", "Configured keyring cannot import password info. This might be normal.");
		return FALSE;
	}
	
	return import(account, mode, data, error);
}

gboolean
purple_keyring_export_password(PurpleAccount * account,
			       const char ** keyringid,
			       const char ** mode,
			       char ** data,
			       GError ** error,
			       GDestroyNotify * destroy)
{
	const PurpleKeyring * inuse;
	PurpleKeyringExportPassword export;

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOKEYRING,
			"No keyring configured, cannot export password info");
		purple_debug_info("keyring",
			"No keyring configured, cannot export password info");
		return FALSE;
	}

	*keyringid = purple_keyring_get_id(inuse);

	purple_debug_info("keyring",
		"Exporting password for account %s (%s) from keyring %s.\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account),
		*keyringid);

	if (*keyringid == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_INVALID,
			"Plugin does not have a keyring id");
		purple_debug_info("keyring",
			"Configured keyring does not have a keyring id, cannot export password");
		return FALSE;
	}

	export = purple_keyring_get_export_password(inuse);

	if (export == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
			"Keyring cannot export password info.");
		purple_debug_info("keyring",
			"Keyring cannot export password info. This might be normal");
		return FALSE;
	}

	return export(account, mode, data, error, destroy);
}

void 
purple_keyring_get_password(PurpleAccount *account,
                            PurpleKeyringReadCallback cb,
                            gpointer data)
{
	GError * error = NULL;
	const PurpleKeyring * inuse;
	PurpleKeyringRead read;

	if (account == NULL) {
		error = g_error_new(ERR_PIDGINKEYRING, ERR_INVALID,
			"No account passed to the function.");

		if (cb != NULL)
			cb(account, NULL, error, data);

		g_error_free(error);

	} else {
		inuse = purple_keyring_get_inuse();

		if (inuse == NULL) {
			error = g_error_new(ERR_PIDGINKEYRING, ERR_NOKEYRING,
				"No keyring configured.");

			if (cb != NULL)
				cb(account, NULL, error, data);

			g_error_free(error);

		} else {
			read = purple_keyring_get_read_password(inuse);

			if (read == NULL) {
				error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
					"Keyring cannot read password.");

				if (cb != NULL)
					cb(account, NULL, error, data);

				g_error_free(error);

			} else {
				read(account, cb, data);
			}
		}
	}
}

static void 
purple_keyring_set_password_async_cb(PurpleAccount * account, 
				     GError * error,
				     gpointer data)
{
	PurpleKeyringCbInfo * cbinfo;
	PurpleKeyringSaveCallback cb;

	g_return_if_fail(data != NULL);
	g_return_if_fail(account != NULL);

	cbinfo = data;
	cb = cbinfo->cb;

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
purple_keyring_set_password(PurpleAccount * account,
                            const gchar *password,
                            PurpleKeyringSaveCallback cb,
                            gpointer data)
{
	GError * error = NULL;
	const PurpleKeyring * inuse;
	PurpleKeyringSave save;
	PurpleKeyringCbInfo * cbinfo;

	g_return_if_fail(account != NULL);

	inuse = purple_keyring_get_inuse();
	if (inuse == NULL) {
		error = g_error_new(ERR_PIDGINKEYRING, ERR_NOKEYRING,
			"No keyring configured.");
		if (cb != NULL)
			cb(account, error, data);
		g_error_free(error);

	} else {
		save = purple_keyring_get_save_password(inuse);
		if (save == NULL) {
			error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
				"Keyring cannot save password.");
			if (cb != NULL)
				cb(account, error, data);
			g_error_free(error);

		} else {
			cbinfo = g_malloc(sizeof(PurpleKeyringCbInfo));
			cbinfo->cb = cb;
			cbinfo->data = data;
			save(account, password, purple_keyring_set_password_async_cb, data);
		}
	}
}

void
purple_keyring_close(PurpleKeyring * keyring,
		     GError ** error)
{
	PurpleKeyringClose close;

	if (keyring == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING, ERR_INVALID,
			"No keyring passed to the function.");

	} else {
		close = purple_keyring_get_close_keyring(keyring);

		if (close == NULL) {
			*error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
				"Keyring doesn't support being closed.");

		} else {
			close(error);

		}
	}
}


void 
purple_keyring_change_master(PurpleKeyringChangeMasterCallback cb,
			     gpointer data)
{
	GError * error = NULL;
	PurpleKeyringChangeMaster change;
	const PurpleKeyring * inuse;

	inuse = purple_keyring_get_inuse();
	
	if (inuse == NULL) {
		error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
			"Keyring doesn't support master passwords.");
		if (cb)
			cb(FALSE, error, data);
		g_error_free(error);

	} else {

		change = purple_keyring_get_change_master(inuse);

		if (change == NULL) {
			error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
				"Keyring doesn't support master passwords.");
			if (cb)
				cb(FALSE, error, data);

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

