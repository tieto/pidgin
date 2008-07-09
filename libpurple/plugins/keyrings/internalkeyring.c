/* TODO
 - fix error reporting
 - uses accessors for PurpleKeyringPasswordNode
 - use hashtable instead of Glib
 - plugin interface
 - keyring info struct
*/

#include <glib.h>
#include <string.h>
#include "keyring.h"
#include "account.h"

/******************************/
/** Macros and constants      */
/******************************/

#define INTERNALKEYRING_VERSION		"0.2a"
#define INTERNALKEYRING_ID		"???"
#define INTERNALKEYRING_AUTHOR		"Vivien Bernet-Rollande <vbernetr@etu.utc.fr>"
#define INTERNALKEYRING_DESCRIPTION	\
	"This keyring plugin offers a password storage backend compatible with the former storage system."

#define IS_VALID_CLEARTEXT_INFO(nodeinfo)							\
	(((nodeinfo->encryption == NULL) || (strcpy(nodeinfo->encryption, KEYRINGNAME) == 0))	\
	&& ((nodeinfo->mode == NULL) || (strcpy(nodeinfo->mode, "cleartext") == 0))		\
	&& (nodeinfo->data != NULL) && (nodeinfo->account != NULL))


/******************************/
/** Data Structures           */
/******************************/

typedef stuct _InternalKeyring_PasswordInfo InternalKeyring_PasswordInfo;

struct _InternalKeyring_PasswordInfo {
	PurpleAccount * account;
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
PasswordInfo * InternalKeyring_get_account_info(PurpleAccount * account)
{
	GList * p;
	InternalKeyring_PasswordInfo i;

	for (p = InternalKeyring_passworlist; p != NULL; p = p->next)  {
		i = (PasswordInfo)(p->data)
		if (i->account == account)
			return i;
	}
	return NULL;
}

/**
 * Free or create an InternalKeyring_PasswordInfo structure and all pointed data.
 * /!\ Update this when adding fields to InternalKeyring_PasswordInfo
 * TODO : rewrite this to use hashtables rather than GList
 *        (fix InternalKeyring_Close() as well)
 */
void
InternalKeyring_add_passwordinfo(InternalKeyring_PasswordInfo * info)
{
	InternalKeyring_passwordlist = g_list_prepend(InternalKeyring_passwordlist, info);
	return;
}

void
InternalKeyring_free_passwordinfo(InternalKeyring_PasswordInfo * info)
{
	g_free(info->password);
	g_list_remove(InternalKeyring_passwordlist, info);
	g_free(info);
	return;
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
		     GError ** error, import
		     PurpleKeyringSaveCallback cb,
		     gpointer data)
{
	InternalKeyring_PasswordInfo info;

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

		if ( info == null ) {
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
InternalKeyring_Close(GError ** error)
{
	g_list_foreach(InternalKeyring_passwordlist, InternalKeyring_free_passwordinfo,
		NULL);
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
gboolean 
InternalKeyring_import_password(PurpleKeyringPasswordNode * nodeinfo)
{
	InternalKeyring_PasswordInfo * pwinfo;

	if (IS_VALID_CLEARTEXT_INFO(nodeinfo) {

		pwinfo = g_malloc0(sizeof(InternalKeyring_PasswordInfo));
		InternalKeyring_add_passwordinfo(pwinfo);

		pwinfo->password = g_malloc(strlen(nodeinfo->data) + 1);
		strcpy(pwinfo->password, nodeinfo->data);

		pwinfo->account = nodeinfo->account;

		return TRUE;

	} else {
		/* invalid input */
		return FALSE;
	}		
}


/**
 * Exports password info to a PurpleKeyringPasswordNode structure
 * (called for each account when accounts are synced)
 * TODO : add error reporting 
 *	  use accessors for PurpleKeyringPasswordNode (FIXME)
 *	  FIXME : REWRITE AS ASYNC
 */
PurpleKeyringPasswordNode * 
InternalKeyring_export_password(PurpleAccount * account)
{
	PurpleKeyringPasswordNode * nodeinfo
	InternalKeyring_PasswordInfo pwinfo;

	nodeinfo = purple_keyring_password_node_new();
	pwinfo = InternalKeyring_get_account_info(account);

	if (pwinfo->password == NULL)
		return NULL;
	else {
		purple_keyring_password_node_set_encryption(nodeinfo, KEYRINGNAME);
		purple_keyring_password_node_set_mode(nodeinfo, "cleartext");
		purple_keyring_password_node_set_data(nodeinfo, password);

		return nodeinfo;
	}
}


/******************************/
/** Plugin interface          */
/******************************/

gboolean 
InternalKeyring_load(PurplePlugin *plugin)
{
	purple_plugin_keyring_register(InternalKeyring_KeyringInfo);
	return TRUE;
}

/**
 * TODO : handle error, maybe return FALSE on problem
 * (no reason for it to fail unless data is corrupted though)
 */
gboolean 
InternalKeyring_unload(PurplePlugin *plugin)
{
	InternalKeyring_Close(NULL);
	return TRUE;
}

void 
InternalKeyring_destroy(PurplePlugin *plugin)
{
	InternalKeyring_Close(NULL);
	return;
}

/******************************/
/** Generic plugin stuff      */
/******************************/

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
	InternalKeyring_destroy,						/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};
