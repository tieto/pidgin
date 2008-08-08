/**
 * @file keyring.c Keyring plugin API
 * @todo
 *  - purple_keyring_set_inuse()
 *  - loading : find a way to fallback
 *  - purple_keyring_drop()
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
#include <string.h>>
#include "account.h"
#include "keyring.h"
#include "signals.h"
#include "core.h"
#include "debug.h"

/******************************************/
/** @name PurpleKeyring                   */
/******************************************/
/*@{*/

struct _PurpleKeyring
{
	char * name;		// same as plugin id
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringClose close_keyring;
	PurpleKeyringChangeMaster change_master;
	PurpleKeyringImportPassword import_password;
	PurpleKeyringExportPassword export_password;
	PurpleKeyringReadSync read_sync;
	PurpleKeyringSaveSync save_sync;
	gpointer r1;	/* RESERVED */
	gpointer r2;	/* RESERVED */
	gpointer r3;	/* RESERVED */
};


/* Constructor */
PurpleKeyring * 
purple_keyring_new()
{
	return g_malloc0(sizeof(PurpleKeyring));
}

/* Destructor */
void 
purple_keyring_free(PurpleKeyring * keyring)
{
	g_free(keyring);
	return;
}

/* Accessors */
const char * 
purple_keyring_get_name(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->name;
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

PurpleKeyringReadSync
purple_keyring_get_read_sync(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->read_sync;
}

PurpleKeyringSaveSync
purple_keyring_get_save_sync(const PurpleKeyring * keyring)
{
	g_return_val_if_fail(keyring != NULL, NULL);

	return keyring->save_sync;
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

	return;
}

void
purple_keyring_set_read_password(PurpleKeyring * keyring, PurpleKeyringRead read)
{
	g_return_if_fail(keyring != NULL);

	keyring->read_password = read;

	return;
}

void
purple_keyring_set_save_password(PurpleKeyring * keyring, PurpleKeyringSave save)
{
	g_return_if_fail(keyring != NULL);

	keyring->save_password = save;

	return;
}

void 
purple_keyring_set_read_sync(PurpleKeyring * keyring, PurpleKeyringReadSync read)
{
	g_return_if_fail(keyring != NULL);

	keyring->read_sync = read;

	return;
}

void 
purple_keyring_set_save_sync(PurpleKeyring * keyring, PurpleKeyringSaveSync save)
{
	g_return_if_fail(keyring != NULL);

	keyring->save_sync = save;

	return;
}

void
purple_keyring_set_close_keyring(PurpleKeyring * keyring, PurpleKeyringClose close)
{
	g_return_if_fail(keyring != NULL);

	keyring->close_keyring = close;

	return;
}

void
purple_keyring_set_change_master(PurpleKeyring * keyring, PurpleKeyringChangeMaster change)
{
	g_return_if_fail(keyring != NULL);

	keyring->change_master = change;

	return;
}

void
purple_keyring_set_import_password(PurpleKeyring * keyring, PurpleKeyringImportPassword import)
{
	g_return_if_fail(keyring != NULL);

	keyring->import_password = import;

	return;
}

void
purple_keyring_set_export_password(PurpleKeyring * keyring, PurpleKeyringExportPassword export)
{
	g_return_if_fail(keyring != NULL);

	keyring->export_password = export;

	return;
}

/*@}*/


/***************************************/
/** @name Keyring API                  */
/** @todo (maybe)                      */
/**  - rename as purple_keyrings       */
/***************************************/
/*@{*/

static GList * purple_keyring_keyrings;		/* list of available keyrings   */
static const PurpleKeyring * purple_keyring_inuse;	/* keyring being used	        */
static char * purple_keyring_to_use;


void 
purple_keyring_init()
{
	PurpleCore * core;
	const char * touse;

	purple_debug_info("keyring", "keyring_it");

	/* Make sure we don't have junk */
	purple_keyring_keyrings = NULL;
	purple_keyring_inuse = NULL;

	/* register signals */
	core = purple_get_core();

	purple_signal_register(core, "keyring-register",
		purple_marshal_VOID__POINTER_POINTER, 
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),			/* keyring name */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	purple_signal_register(core, "keyring-unregister",
		purple_marshal_VOID__POINTER_POINTER, 
		NULL, 2,
		purple_value_new(PURPLE_TYPE_STRING),			/* keyring name */
		purple_value_new(PURPLE_TYPE_BOXED, "PurpleKeyring *")); /* a pointer to the keyring */

	/* see what keyring we want to use */
	touse = purple_prefs_get_string("active-keyring");

	if (touse == NULL) {

		purple_keyring_to_use = g_strdup(FALLBACK_KEYRING);

	} else {

		purple_keyring_to_use = g_strdup(touse);
	}

	return;
}

void
purple_keyring_uninit()
{
	g_free(purple_keyring_to_use);
}

static PurpleKeyring *
purple_keyring_find_keyring_by_id(char * id)
{
	GList * l;
	PurpleKeyring * keyring;
	const char * name;

	for (l = purple_keyring_keyrings; l != NULL; l = l->next) {
		keyring = l->data;
		name = purple_keyring_get_name(keyring);

		if (g_strcmp0(id, name) == 0)
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
/** @todo ? : add dummy callback ? */
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
		save(cur->data, "", NULL, NULL, NULL);

	return;
}

struct _PurpleKeyringChangeTracker
{
	GError * error;			// could probably be dropped
	PurpleKeyringSetInUseCallback cb;
	gpointer data;
	const PurpleKeyring * new;
	const PurpleKeyring * old;		// we are done when : finished == TRUE && read_outstanding == 0
	int read_outstanding;
	gboolean finished;
	gboolean abort;
	gboolean force;
};

static void
purple_keyring_set_inuse_check_error_cb(const PurpleAccount * account,
					GError * error,
					gpointer data)
{

	const char * name;
	PurpleKeyringClose close;
	struct _PurpleKeyringChangeTracker * tracker;

	tracker = (struct _PurpleKeyringChangeTracker *)data;

	g_return_if_fail(tracker->abort == FALSE);

	tracker->read_outstanding--;

	name = purple_account_get_username(account);;


	if ((error != NULL) && (error->domain == ERR_PIDGINKEYRING)) {

		tracker->error = error;

		switch(error->code) {

			case ERR_NOCAP :
				g_debug("Keyring could not save password for account %s : %s", name, error->message);
				break;

			case ERR_NOPASSWD :
				g_debug("No password found while changing keyring for account %s : %s",
					 name, error->message);
				break;

			case ERR_NOCHANNEL :
				g_debug("Failed to communicate with backend while changing keyring for account %s : %s Aborting changes.",
					 name, error->message);
				tracker->abort = TRUE;
				break;

			default :
				g_debug("Unknown error while changing keyring for account %s : %s", name, error->message);
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

		} else {
			close = purple_keyring_get_close_keyring(tracker->old);
			close(&error);

			tracker->cb(tracker->new, TRUE, error, tracker->data);
		}

		g_free(tracker);
	}
	return;
}


static void
purple_keyring_set_inuse_got_pw_cb(const PurpleAccount * account, 
				  gchar * password, 
				  GError * error, 
				  gpointer data)
{
	const PurpleKeyring * new;
	PurpleKeyringSave save;
	struct _PurpleKeyringChangeTracker * tracker;

	tracker = (struct _PurpleKeyringChangeTracker *)data;
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
			save(account, password, NULL, 
			     purple_keyring_set_inuse_check_error_cb, tracker);

		} else {
			error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
				"cannot store passwords in new keyring");
			purple_keyring_set_inuse_check_error_cb(account, error, data);
			g_error_free(error);
		}
	}

	return;
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
	struct _PurpleKeyringChangeTracker * tracker;
	GError * error = NULL; 

	purple_debug_info("keyring", "changing in use keyring\n");

	oldkeyring = purple_keyring_get_inuse();

	if (oldkeyring != NULL)
		read = purple_keyring_get_read_password(oldkeyring);

	if (read == NULL) {
		error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
			"Existing keyring cannot read passwords");
		g_debug("Existing keyring cannot read passwords");

		/* at this point, we know the keyring won't let us
		 * read passwords, so there no point in copying them.
		 * therefore we just cleanup the old and setup the new 
		 * one later.
		 */

		purple_keyring_drop_passwords(newkeyring);

		close = purple_keyring_get_close_keyring(oldkeyring);

		if (close != NULL)
			close(NULL);	/* we can't do much about errors at this point*/

	}

	if (purple_keyring_inuse != NULL && read != NULL) {

		tracker = g_malloc(sizeof(struct _PurpleKeyringChangeTracker));
		oldkeyring = purple_keyring_get_inuse();

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
		    cur = cur->next)
		{
			tracker->read_outstanding++;

			if (cur->next == NULL)
				tracker->finished = TRUE;

			read(cur->data, purple_keyring_set_inuse_got_pw_cb, tracker);
		}

	} else { /* no keyring was set before. */
		purple_debug_info("keyring", "setting keyring for the first time\n");
		purple_keyring_inuse = newkeyring;

		if (cb != NULL)
			cb(newkeyring, TRUE, NULL, data);

		return;
	}
}


void 
purple_keyring_register(PurpleKeyring * keyring)
{
	const char * keyring_id;
	PurpleCore * core;

	purple_debug_info("keyring", "registering keyring.\n");
	g_return_if_fail(keyring != NULL);
	
	keyring_id = purple_keyring_get_name(keyring);

	/* keyring with no name. Add error handling ? */
	g_return_if_fail(keyring_id != NULL);

	/* If this is the configured keyring, use it. */
	if (purple_keyring_inuse == NULL &&
	    g_strcmp0(keyring_id, purple_keyring_to_use) == 0) {

		/** @todo add callback to make sure all is ok */
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

	core = purple_get_core();
	keyring_id = purple_keyring_get_name(keyring);
	purple_signal_emit(core, "keyring-unregister", keyring_id, keyring);

	inuse = purple_keyring_get_inuse();

	if (inuse == keyring) {
		fallback = purple_keyring_find_keyring_by_id(FALLBACK_KEYRING);

		/* this is problematic. If it fails, we won't detect it */
		purple_keyring_set_inuse(fallback, TRUE, NULL, NULL);
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
purple_keyring_import_password(PurpleAccount * account, 
			       char * keyringid,
			       char * mode,
			       char * data,
			       GError ** error)
{
	const PurpleKeyring * inuse;
	PurpleKeyringImportPassword import;
	const char * realid;

	purple_debug_info("keyring", "importing password.\n");

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOKEYRING,
			"No keyring configured, cannot import password info");
		g_debug("No keyring configured, cannot import password info");
		return FALSE;
	}

	realid = purple_keyring_get_name(inuse);
	/*
	 * we want to be sure that either :
	 *  - there is a keyringid specified and it matches the one configured
	 *  - or the configured keyring is the fallback, compatible one.
	 */
	if ((keyringid != NULL && g_strcmp0(realid, keyringid) != 0) ||
	    g_strcmp0(FALLBACK_KEYRING, realid)) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_INVALID,
			"Specified keyring id does not match the configured one.");
		g_debug("Specified keyring id does not match the configured one.");
		return FALSE;
	}

	import = purple_keyring_get_import_password(inuse);
	if (import == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
			"Keyring cannot import password info.");
		g_debug("Keyring cannot import password info. This might be normal");
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

	purple_debug_info("keyring", "exporting password.\n");

	inuse = purple_keyring_get_inuse();

	if (inuse == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOKEYRING,
			"No keyring configured, cannot import password info");
		g_debug("No keyring configured, cannot import password info");
		return;
	}

	*keyringid = purple_keyring_get_name(inuse);

	if (*keyringid == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_INVALID,
			"Plugin does not have a keyring id");
		g_debug("Plugin does not have a keyring id");
		return FALSE;
	}

	export = purple_keyring_get_export_password(inuse);

	if (export == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING , ERR_NOCAP,
			"Keyring cannot export password info.");
		g_debug("Keyring cannot export password info. This might be normal");
		return FALSE;
	}

	return export(account, mode, data, error, destroy);
}


/**
 * This should be renamed purple_keyring_get_password() when getting
 * to 3.0, while dropping purple_keyring_get_password_sync().
 */
void 
purple_keyring_get_password_async(const PurpleAccount * account,
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

	return;
}

/**
 * This should be renamed purple_keyring_set_password() when getting
 * to 3.0, while dropping purple_keyring_set_password_sync().
 */
void 
purple_keyring_set_password_async(const PurpleAccount * account, 
				  gchar * password,
				  GDestroyNotify destroypassword,
				  PurpleKeyringSaveCallback cb,
				  gpointer data)
{
	GError * error = NULL;
	const PurpleKeyring * inuse;
	PurpleKeyringSave save;

	if (account == NULL) {
		error = g_error_new(ERR_PIDGINKEYRING, ERR_INVALID,
			"No account passed to the function.");

		if (cb != NULL)
			cb(account, error, data);

		g_error_free(error);

	} else {
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
				save(account, password, destroypassword, cb, data);
			}
		}
	}

	return;
}


/**
 * This should be dropped at 3.0 (it's here for compatibility)
 */
const char * 
purple_keyring_get_password_sync(const PurpleAccount * account)
{
	PurpleKeyringReadSync read;
	const PurpleKeyring * inuse;

	if (account == NULL) {
		return NULL;

	} else {

		inuse = purple_keyring_get_inuse();

		if (inuse == NULL) {

			return NULL;

		} else {

			read = purple_keyring_get_read_sync(inuse);

			if (read == NULL){

				return NULL;

			} else {

				return read(account);

			}
		}
	}
}

/**
 * This should be dropped at 3.0 (it's here for compatibility)
 */
void 
purple_keyring_set_password_sync(PurpleAccount * account,
				 const char *password)
{
	PurpleKeyringSaveSync save;
	const PurpleKeyring * inuse;

	if (account != NULL) {

		inuse = purple_keyring_get_inuse();

		if (inuse != NULL) {

			save = purple_keyring_get_save_sync(inuse);

			if (save != NULL){

				return save(account, password);

			}
		}
	}

	return;
}

void
purple_keyring_close(PurpleKeyring * keyring,
		     GError ** error)
{
	PurpleKeyringClose close;

	if (keyring == NULL) {
		*error = g_error_new(ERR_PIDGINKEYRING, ERR_INVALID,
			"No keyring passed to the function.");

		return;

	} else {
		close = purple_keyring_get_close_keyring(keyring);

		if (close == NULL) {
			*error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
				"Keyring doesn't support being closed.");

		} else {
			close(error);

		}
	}

	return;
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

		cb(FALSE, error, data);

		g_error_free(error);

	} else {

		change = purple_keyring_get_change_master(inuse);

		if (change == NULL) {
			error = g_error_new(ERR_PIDGINKEYRING, ERR_NOCAP,
				"Keyring doesn't support master passwords.");

			cb(FALSE, error, data);

			g_error_free(error);

		} else {
			change(cb, data);

		}
	}
	return;
}

/*@}*/


/***************************************/
/** @name Error Codes                  */
/***************************************/
/*@{*/

GQuark purple_keyring_error_domain(void)
{
	return g_quark_from_static_string("Libpurple keyring");
}

/*}@*/

