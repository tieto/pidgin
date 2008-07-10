/**
 * @file keyring.c Keyring plugin API
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


/**
 * Most functions in this files are :
 *  - wrappers that will call the plugin
 *  - keyring managment stuff
 *  - accessors
 *
 * TODO :
 *  - use accessors
 *  - purple_keyring_init()
 *  - purple_keyring_set_inuse() needs to be async for error checking an reversability.
 *
 * Questions :
 *  - cleanup
 *  - warning
 *  - breaking API
 */

#include <glib.h>
#include "keyring.h"
#include "account.h"

/*******************************/
/* opaque structures           */
/*******************************/

/* used to import and export password info */
struct _PurpleKeyringPasswordNode
{
	PurpleAccount * account;
	const char * encryption;
	const char * mode;
	const char * data;
/**
 * strings cannot be modified.
 * all are static excpet data
 */
};


/*******************************/
/* globals                     */
/*******************************/

GList * purple_keyring_keyringlist = NULL;	/* list of available keyrings 	*/
PurpleKeyring * purple_keyring_inuse = NULL;	/* keyring being used		*/


/*******************************/
/* functions                   */
/*******************************/

/* manipulate keyring list, used by config interface */

const GList * 
purple_keyring_get_keyringlist(void)
{
	return purple_keyring_keyringlist;
}

const PurpleKeyring * 
purple_keyring_get_inuse(void)
{
	return purple_keyring_inuse;
}


//typedef void (*PurpleKeyringSaveCallback)(const PurpleAccount * account, GError ** error, gpointer data);
void
purple_keyring_set_inuse_check_error_cb(const PurpleAccount * account,
					GError ** error,
					gpointer data)
{

	const char * name;

	name = purple_account_get_username(account);

	if ((error != NULL) && ((**error).domain == ERR_PIDGINKEYRING)) {

		switch((**error).code) {

			case ERR_NOPASSWD :
				g_debug("No password found while changing keyring for account %s", name);
				break;

			case ERR_NOACCOUNT :
				g_debug("No info on account %s found while changing keyring", name);
				break;

			case ERR_NOCHANNEL :
				g_debug("Failed to communicate with backend while changing keyring for account %s", name);
				break;
				/* FIXME : this should somehow abort the whole procedure */

			default :
				g_debug("Unknown error while changing keyring for account %s", name);
				break;
		}
	}

	return;
}

//typedef void (*PurpleKeyringReadCallback)(const PurpleAccount * account, gchar * password, GError * error, gpointer data);
void
purple_keyring_set_inuse_got_pw_cb(const PurpleAccount * account, 
			 gchar * password, 
			 GError ** error, 
			 gpointer data)
{
	PurpleKeyring * new;
	PurpleKeyringSave save;
	new = (PurpleKeyring *)data;
	/* XXX check for read error or just forward ? */

	/* XXX change to use accessor */

	//typedef void (*PurpleKeyringSave)(const PurpleAccount * account, gchar * password, GError ** error, PurpleKeyringSaveCallback cb, gpointer data);

	save = purple_keyring_get_save_password(new);
	save(account, password, error, purple_keyring_set_inuse_check_error_cb, 
		NULL);

	return;
}

/* FIXME : needs to be async and cancelable */
void 
purple_keyring_set_inuse(PurpleKeyring * new,
			 GError ** error)
{

	GList * cur;
	const PurpleKeyring * old;
	PurpleKeyringClose close;
	PurpleKeyringRead read;

	if (purple_keyring_inuse != NULL) {

		old = purple_keyring_get_inuse();

		for (cur = purple_accounts_get_all(); cur != NULL; cur = cur->next)
		{
			read = purple_keyring_get_read_password(old);
			read(cur->data, NULL, purple_keyring_set_inuse_got_pw_cb, (void*)new);
		}

		/* FIXME :
		 * What happens if safe is closed before passwords have been successfully stored ? 
		 */

		close = purple_keyring_get_close_keyring(old);
		close(error);	/* should automatically free all passwords */
	}

	purple_keyring_inuse = new;
	return;
}

/* register a keyring plugin */
/**
 * XXX : function to unregister a keyring ?
 *	 validate input ? add magix field ?
 */
void 
purple_plugin_keyring_register(PurpleKeyring * info)
{
	purple_keyring_keyringlist = g_list_prepend(purple_keyring_keyringlist,
		info);
}


/**
 * wrappers to import and export passwords
 */


/** 
 * used by account.c while reading a password from xml
 * might not really need to be async.
 * TODO : rewrite InternalKeyring as async
 */
void
purple_keyring_import_password(const PurpleKeyringPasswordNode * passwordnode,
				GError ** error,
				PurpleKeyringImportCallback cb,
				gpointer data)
{
	PurpleKeyringImportPassword import;

	if (purple_keyring_inuse == NULL) {

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOKEYRING, 
			"No Keyring configured.");
		cb(error, data);

	} else {
		import = purple_keyring_get_import_password(purple_keyring_inuse);
		import(passwordnode, error, cb, data);
	}
	return;
}

/* 
 * used by account.c while syncing accounts
 *  returned data must be g_free()'d
 */
void
purple_keyring_export_password(PurpleAccount * account,
			       GError ** error,
			       PurpleKeyringImportCallback cb,
			       gpointer data)
{
	PurpleKeyringExportPassword export;

	if (purple_keyring_inuse == NULL) {

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOKEYRING, 
			"No Keyring configured.");
		cb(error, data);

	} else {
		export = purple_keyring_get_export_password(purple_keyring_inuse);
		export(account, error, cb, data);
	}
	return;
}


/** 
 * functions called from the code to access passwords (account.h):
 *	purple_account_get_password()	<- TODO : rewrite these functions as asynch, as well as functions calling them. Will most likely break API compatibility.
 *	purple_account_set_password()
 * so these functions will call : 
 */
void 
purple_keyring_get_password(const PurpleAccount *account,
			    GError ** error,
			    PurpleKeyringReadCallback cb,
			    gpointer data)
{
	PurpleKeyringRead read;

	if (purple_keyring_inuse == NULL) {

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOKEYRING, 
			"No Keyring configured.");
		cb(account, NULL, error, data);

	} else {

		read = purple_keyring_get_read_password(purple_keyring_inuse);
		read(account, error, cb, data);

	}
	return;
}

void 
purple_keyring_set_password(const PurpleAccount * account, 
			    gchar * password, 
			    GError ** error, 
			    PurpleKeyringSaveCallback cb,
			    gpointer data)
{
	PurpleKeyringSave save;

	if (purple_keyring_inuse == NULL) {

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOKEYRING, 
				"No Keyring configured.");
		cb(account, error, data);

	} else {
		save = purple_keyring_get_save_password(purple_keyring_inuse);
		save(account, password, error, cb, data);
	}
	return;
}

/* accessors for data structure fields */
	/* PurpleKeyring */
const char *
purple_keyring_get_name(const PurpleKeyring * info)
{
	return info->name;
}

PurpleKeyringRead 
purple_keyring_get_read_password(const PurpleKeyring * info)
{
	return info->read_password;
}

PurpleKeyringSave
purple_keyring_get_save_password(const PurpleKeyring * info)
{
	return info->save_password;
}

PurpleKeyringClose 
purple_keyring_get_close_keyring(const PurpleKeyring * info)
{
	return info->close_keyring;
}

PurpleKeyringFree 
purple_keyring_get_free_password(const PurpleKeyring * info)
{
	return info->free_password;
}

PurpleKeyringChangeMaster 
purple_keyring_get_change_master(const PurpleKeyring * info)
{
	return info->change_master;
}

PurpleKeyringImportPassword
purple_keyring_get_import_password(const PurpleKeyring * info)
{
	return info->import_password;
}

PurpleKeyringExportPassword 
purple_keyring_get_export_password(const PurpleKeyring * info)
{
	return info->export_password;
}


void 
purple_keyring_set_name(PurpleKeyring * info,
			char * name)
{
	info->name = name;
}

void 
purple_keyring_set_read_password(PurpleKeyring * info,
				 PurpleKeyringRead read)
{
	info->read_password = read; /*  returned data must be g_free()'d */
}

void 
purple_keyring_set_save_password(PurpleKeyring * info,
				 PurpleKeyringSave save)
{
	info->save_password = save;
}

void 
purple_keyring_set_close_keyring(PurpleKeyring * info,
				 PurpleKeyringClose close)
{
	info->close_keyring = close;
}

void 
purple_keyring_set_free_password(PurpleKeyring * info,
				 PurpleKeyringFree free)
{
	info->free_password = free;
}

void 
purple_keyring_set_change_master(PurpleKeyring * info,
				 PurpleKeyringChangeMaster change_master)
{
	info->change_master = change_master;
}

void 
purple_keyring_set_import_password(PurpleKeyring * info,
				   PurpleKeyringImportPassword import_password)
{
	info->import_password = import_password;
}
void 
purple_keyring_set_export_password(PurpleKeyring * info,
				   PurpleKeyringExportPassword export_password)
{
	info->export_password = export_password;
}


	/* PurpleKeyringPasswordNode */

PurpleKeyringPasswordNode * 
purple_keyring_password_node_new(void)
{
	PurpleKeyringPasswordNode * ret;

	ret = g_malloc(sizeof(PurpleKeyringPasswordNode));
	return ret;
}

void 
purple_keyring_password_node_free(PurpleKeyringPasswordNode * node)
{
	g_free(node);
	return;
}

const PurpleAccount * 
purple_keyring_password_node_get_account(const PurpleKeyringPasswordNode * info)
{
	return info->account;
}

const char * 
purple_keyring_password_node_get_encryption(const PurpleKeyringPasswordNode * info)
{
	return info->encryption;
}

const char * 
purple_keyring_password_node_get_mode(const PurpleKeyringPasswordNode * info)
{
	return info->mode;
}

const char * 
purple_keyring_password_node_get_data(const PurpleKeyringPasswordNode * info)
{
	return info->data;
}


void 
purple_keyring_password_node_set_account(PurpleKeyringPasswordNode * info, 
					 PurpleAccount * account)
{
	info->account = account;
	return;	
}

void 
purple_keyring_password_node_set_encryption(PurpleKeyringPasswordNode * info,
					    const char * encryption)
{
	info->encryption = encryption;
	return;
}

void 
purple_keyring_password_node_set_mode(PurpleKeyringPasswordNode * info,
				      const char * mode)
{
	info->mode = mode;
	return;
}

void 
purple_keyring_password_node_set_data(PurpleKeyringPasswordNode * info,
				      const char * data)
{
	info->data = data;
	return;
}

GQuark
purple_keyring_error_domain(void)
{
	return g_quark_from_static_string("Libpurple keyring");
}

/**
 * prepare stuff (called at startup)
 * TODO : see if we need to cleanup
 */
void purple_keyring_init()
{//FIXME
	/**
	 * init error GQuark (FIXME change it in headers as well)
	 * read safe to use in confing
	 * make sure said safe is loaded
	 * else fallback
	 */
}
