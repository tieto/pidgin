/**
 * @file keyring.h Keyring plugin API
 * @ingroup core
 *
 * @todo
 *  - Offer a way to prompt the user for a password or for a password change.
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

/**
 * Default keyring
 */
#define PURPLE_DEFAULT_KEYRING "core-scrouaf-internalkeyring"

/*******************************************************/
/** @name data structures and types                    */
/*******************************************************/
/*@{*/
typedef struct _PurpleKeyring PurpleKeyring;

/*@}*/

/**
 * XXX maybe strip a couple GError* if they're not used,
 * since they should only be interresting for the callback
 *  --> ability to forward errors ?
 *
 */

/********************************************************/
/** @name Callbacks for basic keyring operation         */
/********************************************************/
/*@{*/

/**
 * Callback for once a password is read. If there was a problem, the password
 * should be NULL, and the error set.
 *
 * @param account  The account of which the password was asked.
 * @param password The password that was read. This should be freed when the
 *                 callback returns.
 * @param error    Error that could have occured. Must be freed by the calling
 *                 function.
 * @param data     Data passed to the callback.
 */
typedef void (*PurpleKeyringReadCallback)(PurpleAccount *account,
                                          gchar *password,
                                          GError *error,
                                          gpointer data);

/**
 * Callback for once a password has been stored. If there was a problem, the
 * error will be set.
 *
 * @param error   Error that could have occured. Must be freed by the calling
 *                function.
 * @param account The account for which the password was saved.
 * @param data    Data passed to the callback.
 */
typedef void (*PurpleKeyringSaveCallback)(PurpleAccount *account,
                                          GError *error,
                                          gpointer data);

/**
 * Callback for once the master password for a keyring has been changed.
 *
 * @param result Will be TRUE if the password has been changed, false otherwise.
 * @param error  Error that has occured. Must be freed if non NULL.
 * @param data   Data passed to the callback.
 */
typedef void (*PurpleKeyringChangeMasterCallback)(gboolean result,
                                                  GError *error,
                                                  gpointer data);

/**
 * Callback for when we change the keyring.
 *
 * @param keyring The keyring that is in use.
 * @param result  TRUE if the keyring was changed, FALSE otherwise.
 * @param error   An error that might have occured.
 * @param data    A pointer to user supplied data.
 */
typedef void (*PurpleKeyringSetInUseCallback)(const PurpleKeyring *keyring,
                                              gboolean result,
                                              GError *error,
                                              gpointer data);

/*@}*/

/********************************************************/
/** @name pointers to the keyring's functions           */
/********************************************************/
/*@{*/

/**
 * Read the password for an account
 *
 * @param account The account which's password we want.
 * @param cb      A callback to be used once the password is found.
 * @param data    Data to be passed to the callback.
 */
typedef void (*PurpleKeyringRead)(PurpleAccount *account,
				  PurpleKeyringReadCallback cb,
				  gpointer data);

/**
 * Store a password in the keyring.
 *
 * @param account  The account for which the password is to be stored.
 * @param password The password to be stored. This password will be freed once
 *                 the function returns, so one must take care to make a copy if
 *                 they call other async functions later. If the password is
 *                 NULL, this means that the keyring should forget about that
 *                 password.
 * @param cb       A callback to be called once the password is saved.
 * @param data     A pointer to be passed to the callback
 */
typedef void (*PurpleKeyringSave)(PurpleAccount *account,
                                  const gchar *password,
                                  PurpleKeyringSaveCallback cb,
                                  gpointer data);

/**
 * Close the keyring.
 * This will be called so the keyring can do any cleanup it wants.
 *
 * @param error An error that may occur.
 */
typedef void (*PurpleKeyringClose)(GError **error);

/**
 * Change the master password for the keyring. If NULL, it means the Keyring
 * has no master password capabilities.
 *
 * @param error An error that may occur.
 * @param cb    A callback to call once the master password has been changed.
 * @param data  A pointer to pass to the callback.
 */
typedef void (*PurpleKeyringChangeMaster)(PurpleKeyringChangeMasterCallback cb,
                                          gpointer data);

/**
 * Import info on an XML node.
 * This is not async because it is not meant to prompt for a master password and
 * decrypt passwords.
 *
 * @param account The account.
 * @param mode    A keyring specific option that was stored. Can be NULL, will
 *                be freed when function returns.
 * @param data    Data that was stored, Can be NULL, will be freed when function
 *                returns (so copy it if you need it).
 *
 * @return TRUE on success, FALSE on failure.
 */
typedef gboolean (*PurpleKeyringImportPassword)(PurpleAccount *account,
                                                const char *mode,
                                                const char *data,
                                                GError **error);

/**
 * Export information that will be stored in an XML node.
 * This is not async because it is not meant to prompt for a master password or
 * encrypt passwords.
 *
 * @param account The account for which we want the info.
 * @param mode    An option field that can be used by the plugin. This is
 *                expected to be a static string.
 * @param data    The data to be stored in the XML node. This string will be
 *                freed using destroy() once not needed anymore.
 * @param error   Will be set if a problem occured.
 * @param destroy A function to be called, if non NULL, to free data.
 *
 * @return TRUE on success, FALSE on failure.
 */
typedef gboolean (*PurpleKeyringExportPassword)(PurpleAccount *account,
                                                const char **mode,
                                                char **data,
                                                GError **error,
                                                GDestroyNotify *destroy);


/*@}*/

#ifdef __cplusplus
extern "C" {
#endif

/***************************************/
/** @name Keyring API                  */
/***************************************/
/*@{*/

/**
 * Find a keyring from it's keyring id.
 *
 * @param id The id for the keyring.
 *
 * @return The keyring, or NULL if not found.
 */
PurpleKeyring *purple_keyring_get_keyring_by_id(const char *id);

/**
 * Get a list of id/name pairs (for preferences)
 *
 * @return The list.
 */
GList *purple_keyring_get_options(void);

/**
 * Prepare stuff at startup.
 */
void purple_keyring_init(void);

/**
 * Do some cleanup.
 */
void purple_keyring_uninit(void);

/**
 * Get the keyring list. Used by the UI.
 */
const GList *
purple_keyring_get_keyrings(void);

/**
 * Get the keyring being used.
 */
const PurpleKeyring *
purple_keyring_get_inuse(void);

/**
 * Set the keyring to use. This function will move all passwords from
 * the old keyring to the new one. If it fails, it will cancel all changes,
 * close the new keyring, and notify the callback. If it succeeds, it will
 * remove all passwords from the old safe and close that safe.
 *
 * @param newkeyring The new keyring to use.
 * @param error      Error that may occur.
 * @param cb         The callback to call once the change is done.
 * @param data       A pointer that will be passed to the callback.
 * @param force      FALSE if the change can be cancelled. If this is TRUE and
 *                   an error occurs, data might be lost.
 */
void
purple_keyring_set_inuse(const PurpleKeyring *newkeyring,
                         gboolean force,
                         PurpleKeyringSetInUseCallback cb,
                         gpointer data);

/**
 * Register a keyring plugin.
 *
 * @param keyring The keyring to register.
 */
void
purple_keyring_register(PurpleKeyring *keyring);

/**
 * Unregister a keyring plugin. In case the keyring is in use,
 * passwords will be moved to a fallback safe, and the keyring
 * to unregister will be properly closed.
 *
 * @param keyring The keyring to unregister.
 */
void
purple_keyring_unregister(PurpleKeyring *keyring);

/*@}*/


/***************************************/
/** @name Keyring plugin wrappers      */
/***************************************/
/*@{*/

/**
 * used by account.c while reading a password from xml.
 *
 * @param account   The account.
 * @param keyringid The plugin ID that was stored in the xml file. Can be NULL.
 * @param mode      A keyring specific option that was stored. Can be NULL.
 * @param data      Data that was stored, can be NULL.
 *
 * @return TRUE if the input was accepted, FALSE otherwise.
 */
gboolean purple_keyring_import_password(PurpleAccount *account,
                                        const char *keyringid,
                                        const char *mode,
                                        const char *data,
                                        GError **error);

/**
 * used by account.c while syncing accounts
 *
 * @param account   The account for which we want the info.
 * @param keyringid The plugin id to be stored in the XML node. This will be
 *                  NULL or a string that can be considered static.
 * @param mode      An option field that can be used by the plugin. This will be
 *                  NULL or a string that can be considered static.
 * @param data      The data to be stored in the XML node. This string must be
 *                  freed using destroy() once not needed anymore if it is not
 *                  NULL.
 * @param error     Will be set if a problem occured.
 * @param destroy   A function to be called, if non NULL, to free data.
 *
 * @return TRUE if the info was exported successfully, FALSE otherwise.
 */
gboolean
purple_keyring_export_password(PurpleAccount *account,
                               const char **keyringid,
                               const char **mode,
                               char **data,
                               GError **error,
                               GDestroyNotify *destroy);

/**
 * Read a password from the active safe.
 *
 * @param account The account for which we want the password.
 * @param cb      The callback to be called.
 * @param data    A pointer passed to the callback.
 */
void
purple_keyring_get_password(PurpleAccount *account,
                            PurpleKeyringReadCallback cb,
                            gpointer data);

/**
 * Set a password to be remembered.
 *
 * @param account  The account for which the password is to be saved.
 * @param password The password to save.
 * @param cb       A callback for once the password is saved.
 * @param data     A pointer to be passed to the callback.
 */
void
purple_keyring_set_password(PurpleAccount *account,
                            const gchar *password,
                            PurpleKeyringSaveCallback cb,
                            gpointer data);

/**
 * Close a safe.
 *
 * @param keyring The safe to close.
 * @param error   Error that might occur.
 */
void
purple_keyring_close(PurpleKeyring *keyring, GError **error);

/**
 * Change the master password for a safe (if the safe supports it).
 *
 * @param cb   A callback to notify once we're done.
 * @param data A pointer to be passed to the callback.
 */
void
purple_keyring_change_master(PurpleKeyringChangeMasterCallback cb,
                             gpointer data);

/*@}*/

/***************************************/
/** @name PurpleKeyring Accessors      */
/***************************************/
/*@{*/

PurpleKeyring *purple_keyring_new(void);
void purple_keyring_free(PurpleKeyring *keyring);

const char *purple_keyring_get_name(const PurpleKeyring *info);
const char *purple_keyring_get_id(const PurpleKeyring *info);
PurpleKeyringRead purple_keyring_get_read_password(const PurpleKeyring *info);
PurpleKeyringSave purple_keyring_get_save_password(const PurpleKeyring *info);
PurpleKeyringClose purple_keyring_get_close_keyring(const PurpleKeyring *info);
PurpleKeyringChangeMaster purple_keyring_get_change_master(const PurpleKeyring *info);
PurpleKeyringImportPassword purple_keyring_get_import_password(const PurpleKeyring *info);
PurpleKeyringExportPassword purple_keyring_get_export_password(const PurpleKeyring *info);

void purple_keyring_set_name(PurpleKeyring *info, char *name);
void purple_keyring_set_id(PurpleKeyring *info, char *id);
void purple_keyring_set_read_password(PurpleKeyring *info, PurpleKeyringRead read);
void purple_keyring_set_save_password(PurpleKeyring *info, PurpleKeyringSave save);
void purple_keyring_set_close_keyring(PurpleKeyring *info, PurpleKeyringClose close);
void purple_keyring_set_change_master(PurpleKeyring *info, PurpleKeyringChangeMaster change_master);
void purple_keyring_set_import_password(PurpleKeyring *info, PurpleKeyringImportPassword import_password);
void purple_keyring_set_export_password(PurpleKeyring *info, PurpleKeyringExportPassword export_password);

/*@}*/

/***************************************/
/** @name Error Codes                  */
/***************************************/
/*@{*/

/**
 * Error domain GQuark.
 * See @ref purple_keyring_error_domain .
 */
#define ERR_PIDGINKEYRING 	purple_keyring_error_domain()
/** stuff here too */
GQuark purple_keyring_error_domain(void);

/** error codes for keyrings. */
enum PurpleKeyringError
{
	ERR_OK = 0,		/**< no error. */
	ERR_NOPASSWD = 1,	/**< no stored password. */
	ERR_NOACCOUNT,		/**< account not found. */
	ERR_WRONGPASS,		/**< user submitted wrong password when prompted. */
	ERR_WRONGFORMAT,	/**< data passed is not in suitable format. */
	ERR_NOKEYRING,		/**< no keyring configured. */
	ERR_NOCHANNEL,		/**< failed to communicate with the backend */
	ERR_INVALID,		/**< invalid input */
	ERR_NOCAP,		/**< keyring doesn't support this */
	ERR_UNKNOWN		/**< unknown error */
};

#ifdef __cplusplus
}
#endif
/*}@*/
#endif /* _PURPLE_KEYRING_H_ */

