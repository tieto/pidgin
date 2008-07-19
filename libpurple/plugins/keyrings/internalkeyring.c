/* TODO
 - fix error reporting
 - use hashtable instead of Glib
 - plugin interface
 - move keyring info struct upwards
*/

#include <glib.h>
#include <string.h>
#include "version.h"
#include "keyring.h"
#include "account.h"

/******************************/
/** Macros and constants      */
/******************************/

#define KEYRINGNAME			"internalkeyring"
#define INTERNALKEYRING_VERSION		"0.2a"
#define INTERNALKEYRING_ID		"core-scrouaf-internalkeyring"
#define INTERNALKEYRING_AUTHOR		"Vivien Bernet-Rollande <vbernetr@etu.utc.fr>"
#define INTERNALKEYRING_DESCRIPTION	\
	"This keyring plugin offers a password storage backend compatible with the former storage system."


/******************************/
/** Data Structures           */
/******************************/

typedef struct _InternalKeyring_PasswordInfo InternalKeyring_PasswordInfo;

struct _InternalKeyring_PasswordInfo {
	const PurpleAccount * account;
	gchar * password;
};

/******************************/
/** Globals                   */
/******************************/

GList * InternalKeyring_passwordlist = NULL;		/* use hashtable ? */


/******************************/
/** Internal functions        */
/******************************/

/**
 * retrieve the InternalKeyring_PasswordInfo structure for an account
 * TODO : rewrite this to use hashtables rather than GList
 */
static InternalKeyring_PasswordInfo * 
InternalKeyring_get_account_info(const PurpleAccount * account)
{
	GList * p;
	InternalKeyring_PasswordInfo * i;

	for (p = InternalKeyring_passwordlist; p != NULL; p = p->next)  {
		i = (InternalKeyring_PasswordInfo*)(p->data);
		if (i->account == account)
			return i;
	}
	return NULL;
}

/**
 * Free or create an InternalKeyring_PasswordInfo structure and all pointed data.
 * XXX /!\ Update this when adding fields to InternalKeyring_PasswordInfo
 * XXX : rewrite this to use hashtables rather than GList
 *        (fix InternalKeyring_Close() as well)
 */
static void
InternalKeyring_add_passwordinfo(InternalKeyring_PasswordInfo * info)
{
	InternalKeyring_passwordlist = g_list_prepend(InternalKeyring_passwordlist, info);
	return;
}

static void
InternalKeyring_free_passwordinfo(InternalKeyring_PasswordInfo * info)
{
	g_free(info->password);
	InternalKeyring_passwordlist = g_list_remove(InternalKeyring_passwordlist, info);
	g_free(info);
	return;
}

/**
 * wrapper so we can use it in close
 * TODO : find a more elegant way
 */
static void
InternalKeyring_free_passwordinfo_from_g_list(gpointer info, gpointer data)
{
	InternalKeyring_free_passwordinfo((InternalKeyring_PasswordInfo*)info);
	return;
}


gboolean
InternalKeyring_is_valid_cleartext(const PurpleKeyringPasswordNode * node)
{
	const char * enc;
	const char * mode;
	const char * data;
	const PurpleAccount * account;	

	enc = purple_keyring_password_node_get_encryption(node);
	mode = purple_keyring_password_node_get_mode(node);
	data = purple_keyring_password_node_get_data(node);
	account = purple_keyring_password_node_get_account(node);

	if ((enc == NULL || strcmp(enc, KEYRINGNAME) == 0)&&
	    (mode == NULL || strcmp(mode, "cleartext") == 0)&&
	    data != NULL &&
	    account != NULL) {

		return TRUE;

	} else {

		return FALSE;

	}
}

/******************************/
/** Keyring interface         */
/******************************/

/**
 * returns the password if the password is known.
 */
void 
InternalKeyring_read(const PurpleAccount * account,
		     GError ** error, 
		     PurpleKeyringReadCallback cb,
		     gpointer data)
{
	InternalKeyring_PasswordInfo * info;
	char * ret;
	
	info = InternalKeyring_get_account_info(account);

	if ( info == NULL ) {				/* no info on account */

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOACCOUNT, 
			"No info for account.");
		cb(account, NULL, error, data);
		return;

	} else if (info->password == NULL) {		/* unknown password */

		g_set_error(error, ERR_PIDGINKEYRING, ERR_NOPASSWD, 
			"No Password for this account.");
		cb(account, NULL, error, data);
		return;

	} else {

		ret = info->password;
		cb(account, ret, error, data);
	}
}

/*
 * save a new password
 */
void
InternalKeyring_save(const PurpleAccount * account, 
		     gchar * password,
		     GError ** error,
		     PurpleKeyringSaveCallback cb,
		     gpointer data)
{
	InternalKeyring_PasswordInfo * info;

	info = InternalKeyring_get_account_info(account);

	if (password == NULL) {
		/* forget password */
		if (info == NULL) {
			g_set_error(error, ERR_PIDGINKEYRING, ERR_NOPASSWD, 
				"No Password for this account.");
			cb(account, error, data);
			return;
		}

		InternalKeyring_free_passwordinfo(info);

		if (cb != NULL)
			cb(account, error, data);
		return;

	} else {	/* password != NULL */

		if ( info == NULL ) {
			info = g_malloc0(sizeof (InternalKeyring_PasswordInfo));
			InternalKeyring_add_passwordinfo(info);
		}

		/* if we already had a password, forget about it */
		if ( info->password != NULL )	
			g_free(info->password);

		info->password = g_malloc(strlen( password + 1 ));
		strcpy(info->password, password);

		if (cb != NULL)
			cb(account, error, data);
		return;
	}
}

/*
 * clears and frees all PasswordInfo structures.
 * TODO : rewrite using Hashtable
 */
void 
InternalKeyring_close(GError ** error)
{
	g_list_foreach(InternalKeyring_passwordlist,
		InternalKeyring_free_passwordinfo_from_g_list, NULL);
	return;
}

/*
 * does nothing since we don't want to free the stored info
 */
void 
InternalKeyring_free(gchar * password,
		     GError ** error)
{
	return;		/* nothing to free or cleanup until we forget the password */
}

/**
 * Imports password info from a PurpleKeyringPasswordNode structure
 * (called for each account when accounts.xml is parsed)
 * returns TRUE if sucessful, FALSE otherwise.
 * TODO : add error reporting 
 *	  use accessors for PurpleKeyringPasswordNode (FIXME)
 *	  FIXME : REWRITE AS ASYNC
 */
void
InternalKeyring_import_password(const PurpleKeyringPasswordNode * nodeinfo,
				GError ** error,
				PurpleKeyringImportCallback cb,
				gpointer cbdata)
{
	InternalKeyring_PasswordInfo * pwinfo;
	const char * data;

	if (InternalKeyring_is_valid_cleartext(nodeinfo)) {

		pwinfo = g_malloc0(sizeof(InternalKeyring_PasswordInfo));
		InternalKeyring_add_passwordinfo(pwinfo);

		data = purple_keyring_password_node_get_data(nodeinfo);		
		pwinfo->password = g_malloc(strlen(data) + 1);
		strcpy(pwinfo->password, data);

		pwinfo->account = purple_keyring_password_node_get_account(nodeinfo);

		//return TRUE;

	} else {
		/* invalid input */
		//return FALSE;
	}		
}


/**
 * Exports password info to a PurpleKeyringPasswordNode structure
 * (called for each account when accounts are synced)
 * TODO : add proper error reporting 
 */
void
InternalKeyring_export_password(const PurpleAccount * account,
				GError ** error,
				PurpleKeyringExportCallback cb,
				gpointer data)
{
	PurpleKeyringPasswordNode * nodeinfo;
	InternalKeyring_PasswordInfo * pwinfo;

	nodeinfo = purple_keyring_password_node_new();
	pwinfo = InternalKeyring_get_account_info(account);

	if (pwinfo->password == NULL) {

		// FIXME : error
		cb(NULL, error, data);
		return;

	} else {

		purple_keyring_password_node_set_encryption(nodeinfo, KEYRINGNAME);
		purple_keyring_password_node_set_mode(nodeinfo, "cleartext");
		purple_keyring_password_node_set_data(nodeinfo, pwinfo->password);

		cb(nodeinfo, error, data);
		return;
	}
}



/******************************/
/** Keyring plugin stuff      */
/******************************/

PurpleKeyring InternalKeyring_KeyringInfo =
{
	"internalkeyring",
	InternalKeyring_read,
	InternalKeyring_save,
	InternalKeyring_close,
	InternalKeyring_free,
	NULL,				/* change_master */
	InternalKeyring_import_password,
	InternalKeyring_export_password,
	NULL,				/* RESERVED */
	NULL,				/* RESERVED */
	NULL				/* RESERVED */
};


/******************************/
/** Plugin interface          */
/******************************/

gboolean 
InternalKeyring_load(PurplePlugin *plugin)
{
	purple_plugin_keyring_register(&InternalKeyring_KeyringInfo);	/* FIXME : structure should be hidden */
	return TRUE;
}

/**
 * TODO : handle error, maybe return FALSE on problem
 * (no reason for it to fail unless data is corrupted though)
 */
gboolean 
InternalKeyring_unload(PurplePlugin *plugin)
{
	InternalKeyring_close(NULL);
	return TRUE;
}

void 
InternalKeyring_destroy(PurplePlugin *plugin)
{
	InternalKeyring_close(NULL);
	return;
}


PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,						/* magic */
	PURPLE_MAJOR_VERSION,						/* major_version */
	PURPLE_MINOR_VERSION,						/* minor_version */
	PURPLE_PLUGIN_STANDARD,						/* type */
	NULL,								/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE|PURPLE_PLUGIN_FLAG_AUTOLOAD,	/* flags */
	NULL,								/* dependencies */
	PURPLE_PRIORITY_DEFAULT,					/* priority */
	INTERNALKEYRING_ID,						/* id */
	"internal-keyring-plugin",					/* name */
	INTERNALKEYRING_VERSION,					/* version */
	"Internal Keyring Plugin",					/* summary */
	INTERNALKEYRING_DESCRIPTION,					/* description */
	INTERNALKEYRING_AUTHOR,						/* author */
	"N/A",								/* homepage */
	InternalKeyring_load,						/* load */
	InternalKeyring_unload,						/* unload */
	InternalKeyring_destroy,					/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};


