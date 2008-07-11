/**
 * @file keyring.h Keyring plugin API
 * @ingroup core
 *
 * @todo
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

#ifndef _PURPLE_KEYRING_H_
#define _PURPLE_KEYRING_H_

#include <glib.h>
#include "account.h"

/********************************************************
 * DATA STRUCTURES **************************************
 ********************************************************/

typedef struct _PurpleKeyring PurpleKeyring;				/* public (for now) */
typedef struct _PurpleKeyringPasswordNode PurpleKeyringPasswordNode;	/* opaque struct */

/*
 * XXX maybe strip a couple GError* if they're not used,
 * since they should only be interresting for the callback
 *  --> ability to forward errors ?
 *
 */

/*********************************************************/
/** @name Callbacks for basic keyring operation          */
/*********************************************************/

/**
 * Callback for once a password is read. If there was a problem, the password should
 * be NULL, and the error set.
 *
 * @param account The account of which the password was asked.
 * @param password The password that was read
 * @param error Error that could have occured. Must be freed if non NULL.
 * @param data Data passed to the callback.
 */
typedef void (*PurpleKeyringReadCallback)(const PurpleAccount * account,
					  gchar * password,
					  GError ** error,
					  gpointer data);

/**
 * Callback for once a password has been stored. If there was a problem, the error will be set.
 *
 * @param account The account of which the password was saved.
 * @param error Error that could have occured. Must be freed if non NULL.
 * @param data Data passed to the callback.
 */
typedef void (*PurpleKeyringSaveCallback)(const PurpleAccount * account, 
					  GError ** error,
					  gpointer data);

/**
 * Callback for once the master password for a keyring has been changed.
 *
 * @param result Will be TRUE if the password has been changed, false otherwise.
 * @param error Error that has occured. Must be freed if non NULL.
 * @param data Data passed to the callback.
 */
typedef void (*PurpleKeyringChangeMasterCallback)(gboolean result,
						  GError ** error,
						  gpointer data);

/**
 * Callback for once the master password for a keyring has been changed.
 *
 * @param result Will be TRUE if the password has been changed, false otherwise.
 * @param error Error that has occured. Must be freed if non NULL.
 * @param data Data passed to the callback.
 */
typedef void (*PurpleKeyringImportCallback)(GError ** error, gpointer data);	/* XXX add a gboolean result or just use error ? */
typedef void (*PurpleKeyringExportCallback)(PurpleKeyringPasswordNode * result, GError ** error, gpointer data);

//gboolean purple_keyring_import_password(const PurpleKeyringPasswordNode * passwordnode);


/* pointers to the keyring's functions */
// FIXME : only callback should care about errors
typedef void (*PurpleKeyringRead)(const PurpleAccount * account, GError ** error, PurpleKeyringReadCallback cb, gpointer data);
typedef void (*PurpleKeyringSave)(const PurpleAccount * account, gchar * password, GError ** error, PurpleKeyringSaveCallback cb, gpointer data);
typedef void (*PurpleKeyringClose)(GError ** error);
typedef void (*PurpleKeyringChangeMaster)(GError ** error, PurpleKeyringChangeMasterCallback cb, gpointer data);
typedef void (*PurpleKeyringFree)(gchar * password, GError ** error);

typedef void (*PurpleKeyringImportPassword)(const PurpleKeyringPasswordNode * nodeinfo, GError ** error, PurpleKeyringImportCallback cb, gpointer data);
typedef void (*PurpleKeyringExportPassword)(const PurpleAccount * account,GError ** error, PurpleKeyringExportCallback cb,     gpointer data);

/* information about a keyring */
struct _PurpleKeyring
{
	char *  name;
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringClose close_keyring;
	PurpleKeyringFree free_password;
	PurpleKeyringChangeMaster change_master;
	PurpleKeyringImportPassword import_password;
	PurpleKeyringExportPassword export_password;
	gpointer r1;	/* RESERVED */
	gpointer r2;	/* RESERVED */
	gpointer r3;	/* RESERVED */
};




/***************************************/
/** @name Keyring plugin wrapper API   */
/***************************************/

/* manipulate keyring list, used by config interface */
const GList * purple_keyring_get_keyringlist();
const PurpleKeyring * purple_keyring_get_inuse();

// FIXME : needs to be async so it can detect errors and undo changes
void
purple_keyring_set_inuse_got_pw_cb(const PurpleAccount * account, 
			 gchar * password, 
			 GError ** error, 
			 gpointer data);

/* register a keyring plugin */
// XXX : function to unregister a keyring ?
void purple_plugin_keyring_register(PurpleKeyring * info);

/* used by account.c while reading a password from xml */
void purple_keyring_import_password(const PurpleKeyringPasswordNode * passwordnode, 
					GError ** error, 
					PurpleKeyringImportCallback cb, 
					gpointer data);
/**
 * used by account.c while syncing accounts
 *  returned data must be g_free()'d
 */
void purple_keyring_export_password(PurpleAccount * account,
				    GError ** error,
				    PurpleKeyringExportCallback cb,
				    gpointer data);


/* functions called from the code to access passwords (account.h):
	purple_account_get_password()	<- TODO : rewrite these functions :)
	purple_account_set_password()
so these functions will call : */
void purple_keyring_get_password(const PurpleAccount *account,
				 GError ** error,
				 PurpleKeyringReadCallback cb,
				 gpointer data);
void purple_keyring_set_password(const PurpleAccount * account, 
				 gchar * password, 
				 GError ** error, 
				 PurpleKeyringSaveCallback cb,
				 gpointer data);

/* accessors for data structure fields */
	/* PurpleKeyring */
/*
 * TODO : constructor/destructor
 *	  move it back as public. It would actually make much more sense
 * 	  since devs could build static structure
 */
const char * purple_keyring_get_name(const PurpleKeyring * info);
PurpleKeyringRead purple_keyring_get_read_password(const PurpleKeyring * info);
PurpleKeyringSave purple_keyring_get_save_password(const PurpleKeyring * info);
PurpleKeyringClose purple_keyring_get_close_keyring(const PurpleKeyring * info);
PurpleKeyringFree purple_keyring_get_free_password(const PurpleKeyring * info);
PurpleKeyringChangeMaster purple_keyring_get_change_master(const PurpleKeyring * info);
PurpleKeyringImportPassword purple_keyring_get_import_password(const PurpleKeyring * info);
PurpleKeyringExportPassword purple_keyring_get_export_password(const PurpleKeyring * info);

void purple_keyring_set_name(PurpleKeyring * info, char * name);		/* must be static, will not be auto-freed upon destruction */
void purple_keyring_set_read_password(PurpleKeyring * info, PurpleKeyringRead read);
void purple_keyring_set_save_password(PurpleKeyring * info, PurpleKeyringSave save);
void purple_keyring_set_close_keyring(PurpleKeyring * info, PurpleKeyringClose close);
void purple_keyring_set_free_password(PurpleKeyring * info, PurpleKeyringFree free);
void purple_keyring_set_change_master(PurpleKeyring * info, PurpleKeyringChangeMaster change_master);
void purple_keyring_set_import_password(PurpleKeyring * info, PurpleKeyringImportPassword import_password);
void purple_keyring_set_export_password(PurpleKeyring * info, PurpleKeyringExportPassword export_password);

	/* PurpleKeyringPasswordNode */

PurpleKeyringPasswordNode * purple_keyring_password_node_new();
void purple_keyring_password_node_delete(PurpleKeyringPasswordNode * node);

const PurpleAccount * 
purple_keyring_password_node_get_account(const PurpleKeyringPasswordNode * info);

const char * purple_keyring_password_node_get_encryption(const PurpleKeyringPasswordNode * info);		/* info to be kept must be copied */
const char * purple_keyring_password_node_get_mode(const PurpleKeyringPasswordNode * info);			/* strings will be freed after use*/
const char * purple_keyring_password_node_get_data(const PurpleKeyringPasswordNode * info);			/* (in most cases) */

void purple_keyring_password_node_set_account(PurpleKeyringPasswordNode * info, PurpleAccount * account);
void purple_keyring_password_node_set_encryption(PurpleKeyringPasswordNode * info, const char * encryption);
void purple_keyring_password_node_set_mode(PurpleKeyringPasswordNode * info, const char * mode);
void purple_keyring_password_node_set_data(PurpleKeyringPasswordNode * info, const char * data);

/* prepare some stuff at startup */
void purple_keyring_init();

/***************************************/
/** @name Error Codes                  */
/***************************************/

/**
 * Error domain GQuark. 
 * See @ref purple_keyring_error_domain .
 */
#define ERR_PIDGINKEYRING 	purple_keyring_error_domain()
/** stuff here too */
GQuark purple_keyring_error_domain();

/** error codes for keyrings. */
enum PurpleKeyringError
{
	ERR_OK = 0,		/**< no error. */
	ERR_NOPASSWD = 1,	/**< no stored password. */
	ERR_NOACCOUNT,		/**< account not found. */
	ERR_WRONGPASS,		/**< user submitted wrong password when prompted. */
	ERR_WRONGFORMAT,	/**< data passed is not in suitable format. */
	ERR_NOKEYRING,		/**< no keyring configured. */
	ERR_NOCHANNEL
};


#endif /* _PURPLE_KEYRING_H_ */
