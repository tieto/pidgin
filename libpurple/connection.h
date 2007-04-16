/**
 * @file connection.h Connection API
 * @ingroup core
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @see @ref connection-signals
 */
#ifndef _PURPLE_CONNECTION_H_
#define _PURPLE_CONNECTION_H_

typedef struct _PurpleConnection PurpleConnection;

/**
 * Flags to change behavior of the client for a given connection.
 */
typedef enum
{
	PURPLE_CONNECTION_HTML       = 0x0001, /**< Connection sends/receives in 'HTML'. */
	PURPLE_CONNECTION_NO_BGCOLOR = 0x0002, /**< Connection does not send/receive
					           background colors.                  */
	PURPLE_CONNECTION_AUTO_RESP  = 0x0004,  /**< Send auto responses when away.       */
	PURPLE_CONNECTION_FORMATTING_WBFO = 0x0008, /**< The text buffer must be formatted as a whole */
	PURPLE_CONNECTION_NO_NEWLINES = 0x0010, /**< No new lines are allowed in outgoing messages */
	PURPLE_CONNECTION_NO_FONTSIZE = 0x0020, /**< Connection does not send/receive font sizes */
	PURPLE_CONNECTION_NO_URLDESC = 0x0040,  /**< Connection does not support descriptions with links */ 
	PURPLE_CONNECTION_NO_IMAGES = 0x0080,  /**< Connection does not support sending of images */

} PurpleConnectionFlags;

typedef enum
{
	PURPLE_DISCONNECTED = 0, /**< Disconnected. */
	PURPLE_CONNECTED,        /**< Connected.    */
	PURPLE_CONNECTING        /**< Connecting.   */

} PurpleConnectionState;

#include <time.h>

#include "account.h"
#include "plugin.h"
#include "status.h"

typedef struct
{
	void (*connect_progress)(PurpleConnection *gc, const char *text,
							 size_t step, size_t step_count);
	void (*connected)(PurpleConnection *gc);
	void (*disconnected)(PurpleConnection *gc);
	void (*notice)(PurpleConnection *gc, const char *text);
	void (*report_disconnect)(PurpleConnection *gc, const char *text);
	void (*network_connected)();
	void (*network_disconnected)();

} PurpleConnectionUiOps;

struct _PurpleConnection
{
	PurplePlugin *prpl;            /**< The protocol plugin.               */
	PurpleConnectionFlags flags;   /**< Connection flags.                  */

	PurpleConnectionState state;   /**< The connection state.              */

	PurpleAccount *account;        /**< The account being connected to.    */
	char *password;              /**< The password used.                 */
	int inpa;                    /**< The input watcher.                 */

	GSList *buddy_chats;         /**< A list of active chats.            */
	void *proto_data;            /**< Protocol-specific data.            */

	char *display_name;          /**< How you appear to other people.    */
	guint keepalive;             /**< Keep-alive.                        */


	gboolean wants_to_die;	     /**< Wants to Die state.  This is set
	                                  when the user chooses to log out,
	                                  or when the protocol is
	                                  disconnected and should not be
	                                  automatically reconnected
	                                  (incorrect password, etc.)         */
	guint disconnect_timeout;    /**< Timer used for nasty stack tricks  */
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Connection API                                                  */
/**************************************************************************/
/*@{*/

/**
 * This function should only be called by purple_account_connect()
 * in account.c.  If you're trying to sign on an account, use that
 * function instead.
 *
 * Creates a connection to the specified account and either connects
 * or attempts to register a new account.  If you are logging in,
 * the connection uses the current active status for this account.
 * So if you want to sign on as "away," for example, you need to
 * have called purple_account_set_status(account, "away").
 * (And this will call purple_account_connect() automatically).
 *
 * @param account  The account the connection should be connecting to.
 * @param regist   Whether we are registering a new account or just
 *                 trying to do a normal signon.
 * @param password The password to use.
 */
void purple_connection_new(PurpleAccount *account, gboolean regist,
									const char *password);

/**
 * Disconnects and destroys a PurpleConnection.
 *
 * This function should only be called by purple_account_disconnect()
 * in account.c.  If you're trying to sign off an account, use that
 * function instead.
 *
 * @param gc The purple connection to destroy.
 */
void purple_connection_destroy(PurpleConnection *gc);

/**
 * Sets the connection state.  PRPLs should call this and pass in
 * the state "PURPLE_CONNECTED" when the account is completely
 * signed on.  What does it mean to be completely signed on?  If
 * the core can call prpl->set_status, and it successfully changes
 * your status, then the account is online.
 *
 * @param gc    The connection.
 * @param state The connection state.
 */
void purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state);

/**
 * Sets the connection's account.
 *
 * @param gc      The connection.
 * @param account The account.
 */
void purple_connection_set_account(PurpleConnection *gc, PurpleAccount *account);

/**
 * Sets the connection's displayed name.
 *
 * @param gc   The connection.
 * @param name The displayed name.
 */
void purple_connection_set_display_name(PurpleConnection *gc, const char *name);

/**
 * Returns the connection state.
 *
 * @param gc The connection.
 *
 * @return The connection state.
 */
PurpleConnectionState purple_connection_get_state(const PurpleConnection *gc);

/**
 * Returns TRUE if the account is connected, otherwise returns FALSE.
 *
 * @return TRUE if the account is connected, otherwise returns FALSE.
 */
#define PURPLE_CONNECTION_IS_CONNECTED(gc) \
	(gc->state == PURPLE_CONNECTED)

/**
 * Returns the connection's account.
 *
 * @param gc The connection.
 *
 * @return The connection's account.
 */
PurpleAccount *purple_connection_get_account(const PurpleConnection *gc);

/**
 * Returns the connection's password.
 *
 * @param gc The connection.
 *
 * @return The connection's password.
 */
const char *purple_connection_get_password(const PurpleConnection *gc);

/**
 * Returns the connection's displayed name.
 *
 * @param gc The connection.
 *
 * @return The connection's displayed name.
 */
const char *purple_connection_get_display_name(const PurpleConnection *gc);

/**
 * Updates the connection progress.
 *
 * @param gc    The connection.
 * @param text  Information on the current step.
 * @param step  The current step.
 * @param count The total number of steps.
 */
void purple_connection_update_progress(PurpleConnection *gc, const char *text,
									 size_t step, size_t count);

/**
 * Displays a connection-specific notice.
 *
 * @param gc   The connection.
 * @param text The notice text.
 */
void purple_connection_notice(PurpleConnection *gc, const char *text);

/**
 * Closes a connection with an error.
 *
 * @param gc     The connection.
 * @param reason The error text.
 */
void purple_connection_error(PurpleConnection *gc, const char *reason);

/*@}*/

/**************************************************************************/
/** @name Connections API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Disconnects from all connections.
 */
void purple_connections_disconnect_all(void);

/**
 * Returns a list of all active connections.  This does not
 * include connections that are in the process of connecting.
 *
 * @return A list of all active connections.
 */
GList *purple_connections_get_all(void);

/**
 * Returns a list of all connections in the process of connecting.
 *
 * @return A list of connecting connections.
 */
GList *purple_connections_get_connecting(void);

/**
 * Checks if gc is still a valid pointer to a gc.
 *
 * @return @c TRUE if gc is valid.
 */
/*
 * TODO: Eventually this bad boy will be removed, because it is
 *       a gross fix for a crashy problem.
 */
#define PURPLE_CONNECTION_IS_VALID(gc) (g_list_find(purple_connections_get_all(), (gc)))

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for connections.
 *
 * @param ops The UI operations structure.
 */
void purple_connections_set_ui_ops(PurpleConnectionUiOps *ops);

/**
 * Returns the UI operations structure used for connections.
 *
 * @return The UI operations structure in use.
 */
PurpleConnectionUiOps *purple_connections_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Connections Subsystem                                           */
/**************************************************************************/
/*@{*/

/**
 * Initializes the connections subsystem.
 */
void purple_connections_init(void);

/**
 * Uninitializes the connections subsystem.
 */
void purple_connections_uninit(void);

/**
 * Returns the handle to the connections subsystem.
 *
 * @return The connections subsystem handle.
 */
void *purple_connections_get_handle(void);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_CONNECTION_H_ */
