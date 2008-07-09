/**
 * @file keyring.h Keyring plugin API
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_KEYRING_H_
#define _PURPLE_KEYRING_H_

#include <glib.h>
#include "account.h"

/********************************************************
 * DATA STRUCTURES **************************************
 ********************************************************/

typedef struct _PurpleKeyring PurpleKeyring;	/* TODO : move back as public struct */
typedef struct _PurpleKeyringPasswordNode PurpleKeyringPasswordNode;	/* opaque struct */


	/* XXX maybe strip a couple GError* if they're not used */
/* callbacks */
typedef void (*PurpleKeyringReadCallback)(const PurpleAccount * account, gchar * password, GError * error, gpointer data);
typedef void (*PurpleKeyringSaveCallback)(const PurpleAccount * account, GError * error, gpointer data);
typedef void (*PurpleKeyringChangeMasterCallback)(int result, Gerror * error, gpointer data);
typedef void (*PurpleKeyringImportCallback)(GError * error, gpointer data);	/* XXX add a gboolean result or just use error ? */
typedef void (*PurpleKeyringExportCallback)(PurpleKeyringPasswordNode * result, GError * error, gpointer data);

//gboolean purple_keyring_import_password(const PurpleKeyringPasswordNode * passwordnode);

/* pointers to the keyring's functions */
typedef void (*PurpleKeyringRead)(const PurpleAccount * account, GError ** error, PurpleKeyringReadCallback cb, gpointer data);
typedef void (*PurpleKeyringSave)(const PurpleAccount * account, gchar * password, GError ** error, PurpleKeyringSaveCallback cb, gpointer data);
typedef void (*PurpleKeyringClose)(GError ** error);
typedef void (*PurpleKeyringChangeMaster)(GError ** error, PurpleKeyringChangeMasterCallback cb, gpointer data);
typedef void (*PurpleKeyringFree)(gchar * password, GError ** error);

/**
 * TODO : 
 *  - add GErrors in there
 *  - add callback, it needs to be async
 *  - typedefs for callbacks
 */
typedef gboolean (*PurpleKeyringImportPassword)(PurpleKeyringPasswordNode * nodeinfo);
typedef PurpleKeyringPasswordNode * (*PurpleKeyringExportPassword)(PurpleAccount * account);


/***************************************/
/** @name Keyring plugin wrapper API   */
/***************************************/

/* manipulate keyring list, used by config interface */
const GList * purple_keyring_get_keyringlist(void);
const PurpleKeyringInfo * purple_keyring_get_inuse(void);

// FIXME : needs to be async
void purple_keyring_set_inuse(PurpleKeyringInfo *);	/* changes keyring to use, lots of code involved */

/* register a keyring plugin */
// TODO : function to unregister a keyring ?
void purple_plugin_keyring_register(PurpleKeyring * info);

/* used by account.c while reading a password from xml */
gboolean purple_keyring_import_password(const PurpleKeyringPasswordNode * passwordnode, 
					GError ** error, 
					PurpleKeyringImportCallback cb, 
					gpointer data);
/**
 * used by account.c while syncing accounts
 *  returned data must be g_free()'d
 */
void purple_keyring_export_password(PurpleAccount * account,
				    GError ** error,
				    PurpleKeyringImportCallback cb,
				    gpointer data);


/* functions called from the code to access passwords (account.h):
	purple_account_get_password()	<- TODO : rewrite these functions :)
	purple_account_set_password()
so these functions will call : */
/* FIXME : callback of course */
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
	/* PurpleKeyringInfo */
/*
 * TODO : constructor/destructor
 *	  move it back as public. It would actually make much more sense
 * 	  since devs could build static structure
 */
const char *  purple_keyring_get_name(PurpleKeyringInfo * info);
const PurpleKeyringRead purple_keyring_get_read_password(PurpleKeyringInfo * info);
const PurpleKeyringSave purple_keyring_get_save_password(PurpleKeyringInfo * info);
const PurpleKeyringClose purple_keyring_get_close_keyring(PurpleKeyringInfo * info);
const PurpleKeyringFree purple_keyring_get_free_password(PurpleKeyringInfo * info);
const PurpleKeyringChangeMaster purple_keyring_get_change_master(PurpleKeyringInfo * info);
const PurpleKeyringImportPassword purple_keyring_get_import_password(PurpleKeyringInfo * info);
const PurpleKeyringExportPassword purple_keyring_get_export_password(PurpleKeyringInfo * info);

void purple_keyring_set_name(PurpleKeyringInfo * info, char * name);		/* must be static, will not be auto-freed upon destruction */
void purple_keyring_set_read_password(PurpleKeyringInfo * info, PurpleKeyringRead read);
void purple_keyring_set_save_password(PurpleKeyringInfo * info, PurpleKeyringSave save);
void purple_keyring_set_close_keyring(PurpleKeyringInfo * info, PurpleKeyringClose close);
void purple_keyring_set_free_password(PurpleKeyringInfo * info, PurpleKeyringFree free);
void purple_keyring_set_change_master(PurpleKeyringInfo * info, PurpleKeyringChangeMaster change_master);
void purple_keyring_set_import_password(PurpleKeyringInfo * info, PurpleKeyringImportPassword import_password);
void purple_keyring_set_export_password(PurpleKeyringInfo * info, PurpleKeyringExportPassword export_password);

	/* PurpleKeyringPasswordNode */

PurpleKeyringPasswordNode * purple_keyring_password_node_new(void);
void purple_keyring_password_node_new(PurpleKeyringPasswordNode * node);

const PurpleAccount * purple_keyring_password_node_get_account(PurpleKeyringPasswordNode * info);
const char * purple_keyring_password_node_get_encryption(PurpleKeyringPasswordNode * info);		/* info to be kept must be copied */
const char * purple_keyring_password_node_get_mode(PurpleKeyringPasswordNode * info);			/* strings will be freed after use*/
const char * purple_keyring_password_node_get_data(PurpleKeyringPasswordNode * info);			/* (in most cases) */

void purple_keyring_password_node_set_account(PurpleKeyringPasswordNode * info, PurpleAccount * account);
void purple_keyring_password_node_set_encryption(PurpleKeyringPasswordNode * info, const char * encryption);
void purple_keyring_password_node_set_mode(PurpleKeyringPasswordNode * info, const char * mode);
void purple_keyring_password_node_set_data(PurpleKeyringPasswordNode * info, const char * data);

/* prepare some stuff at startup */
void purple_keyring_init();

/***************************************/
/** @name Error Codes                  */
/***************************************/

#define ERR_PIDGINKEYRING	/* error domain FIXME: has to be a GQuark, not just a define
				 * this should be done in void purple_keyring_init()
                                 */

enum
{
	ERR_OK = 0		/* no error */
	ERR_NOPASSWD = 1,	/* no stored password */
	ERR_NOACCOUNT,		/* account not found */
	ERR_WRONGPASS,		/* user submitted wrong password when prompted */
	ERR_WRONGFORMAT,	/* data passed is not in suitable format*/
	ERR_NOKEYRING		/* no keyring configured */
} PurpleKeyringError;


#endif /* _PURPLE_KEYRING_H_ */
