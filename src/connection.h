/**
 * @file connection.h Connection API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#ifndef _GAIM_CONNECTION_H_
#define _GAIM_CONNECTION_H_

typedef struct _GaimConnection GaimConnection;

/**
 * Flags to change behavior of the client for a given connection.
 */
typedef enum
{
	GAIM_CONNECTION_HTML       = 0x0001, /**< Connection sends/receives in 'HTML'. */
	GAIM_CONNECTION_NO_BGCOLOR = 0x0002, /**< Connection does not send/receive
					           background colors.                  */
	GAIM_CONNECTION_AUTO_RESP  = 0x0004,  /**< Send auto responses when away.       */
	GAIM_CONNECTION_FORMATTING_WBFO = 0x0008, /**< The text buffer must be formatted as a whole */
	GAIM_CONNECTION_NO_NEWLINES = 0x0010, /**< No new lines are allowed in outgoing messages */
	GAIM_CONNECTION_NO_FONTSIZE = 0x0020, /**< Connection does not send/receive font sizes */
	GAIM_CONNECTION_NO_URLDESC = 0x0040  /**< Connection does not support descriptions with links */

} GaimConnectionFlags;

typedef enum
{
	GAIM_DISCONNECTED = 0, /**< Disconnected. */
	GAIM_CONNECTED,        /**< Connected.    */
	GAIM_CONNECTING        /**< Connecting.   */

} GaimConnectionState;

#include "account.h"
#include "internal.h"
#include "plugin.h"

typedef struct
{
	void (*connect_progress)(GaimConnection *gc, const char *text,
							 size_t step, size_t step_count);
	void (*connected)(GaimConnection *gc);
	void (*disconnected)(GaimConnection *gc);
	void (*notice)(GaimConnection *gc, const char *text);
	void (*report_disconnect)(GaimConnection *gc, const char *text);

} GaimConnectionUiOps;

struct _GaimConnection
{
	GaimPlugin *prpl;            /**< The protocol plugin.               */
	GaimConnectionFlags flags;   /**< Connection flags.                  */

	GaimConnectionState state;   /**< The connection state.              */

	GaimAccount *account;        /**< The account being connected to.    */
	int inpa;                    /**< The input watcher.                 */

	GSList *buddy_chats;         /**< A list of active chats.            */
	void *proto_data;            /**< Protocol-specific data.            */

	char *display_name;          /**< The name displayed.                */
	guint keep_alive;            /**< Keep-alive.                        */

	guint idle_timer;            /**< The idle timer.                    */
	time_t login_time;           /**< Time of login.                     */
	time_t last_sent_time;       /**< The time something was last sent.  */
	int is_idle;                 /**< Idle state of the connection.      */

	gboolean is_auto_away;       /**< Whether or not it's auto-away.     */

	gboolean wants_to_die;	     /**< Wants to Die state.  This is set
	                                  when the user chooses to sign off,
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
 * Creates a connection to the specified account.
 *
 * @param account The account the connection should be connecting to.
 *
 * @return The gaim connection.
 */
GaimConnection *gaim_connection_new(GaimAccount *account);

/**
 * Destroys and closes a gaim connection.
 *
 * @param gc The gaim connection to destroy.
 */
void gaim_connection_destroy(GaimConnection *gc);

/**
 * Signs a connection on.
 *
 * @param gc The connection to sign on.
 *
 * @see gaim_connection_disconnect()
 */
void gaim_connection_connect(GaimConnection *gc);

/**
 * Registers a connection.
 *
 * @param gc The connection to register.
 */
void gaim_connection_register(GaimConnection *gc);

/**
 * Signs a connection off.
 *
 * @param gc The connection to sign off.
 *
 * @see gaim_connection_connect()
 */
void gaim_connection_disconnect(GaimConnection *gc);

/**
 * Sets the connection state.
 *
 * @param gc    The connection.
 * @param state The connection state.
 */
void gaim_connection_set_state(GaimConnection *gc, GaimConnectionState state);

/**
 * Sets the connection's account.
 *
 * @param gc      The connection.
 * @param account The account.
 */
void gaim_connection_set_account(GaimConnection *gc, GaimAccount *account);

/**
 * Sets the connection's displayed name.
 *
 * @param gc   The connection.
 * @param name The displayed name.
 */
void gaim_connection_set_display_name(GaimConnection *gc, const char *name);

/**
 * Returns the connection state.
 *
 * @param gc The connection.
 *
 * @return The connection state.
 */
GaimConnectionState gaim_connection_get_state(const GaimConnection *gc);

/**
 * Returns TRUE if the account is connected, otherwise returns FALSE.
 *
 * @return TRUE if the account is connected, otherwise returns FALSE.
 */
#define GAIM_CONNECTION_IS_CONNECTED(gc) \
	(gc->state == GAIM_CONNECTED)

/**
 * Returns the connection's account.
 *
 * @param gc The connection.
 *
 * @return The connection's account.
 */
GaimAccount *gaim_connection_get_account(const GaimConnection *gc);

/**
 * Returns the connection's displayed name.
 *
 * @param gc The connection.
 *
 * @return The connection's displayed name.
 */
const char *gaim_connection_get_display_name(const GaimConnection *gc);

/**
 * Updates the connection progress.
 *
 * @param gc    The connection.
 * @param text  Information on the current step.
 * @param step  The current step.
 * @param count The total number of steps.
 */
void gaim_connection_update_progress(GaimConnection *gc, const char *text,
									 size_t step, size_t count);

/**
 * Displays a connection-specific notice.
 *
 * @param gc   The connection.
 * @param text The notice text.
 */
void gaim_connection_notice(GaimConnection *gc, const char *text);

/**
 * Closes a connection with an error.
 *
 * @param gc     The connection.
 * @param reason The error text.
 */
void gaim_connection_error(GaimConnection *gc, const char *reason);

/*@}*/

/**************************************************************************/
/** @name Connections API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Disconnects from all connections.
 */
void gaim_connections_disconnect_all(void);

/**
 * Returns a list of all active connections.
 *
 * @return A list of all active connections.
 */
GList *gaim_connections_get_all(void);

/**
 * Returns a list of all connections in the process of connecting.
 *
 * @return A list of connecting connections.
 */
GList *gaim_connections_get_connecting(void);

/**
 * Checks if gc is still a valid pointer to a gc.
 *
 * @return @c TRUE if gc is valid.
 */
#define GAIM_CONNECTION_IS_VALID(gc) (g_list_find(gaim_connections_get_all(), (gc)) || g_list_find(gaim_connections_get_connecting(), (gc)))

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
void gaim_connections_set_ui_ops(GaimConnectionUiOps *ops);

/**
 * Returns the UI operations structure used for connections.
 *
 * @return The UI operations structure in use.
 */
GaimConnectionUiOps *gaim_connections_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Connections Subsystem                                           */
/**************************************************************************/
/*@{*/

/**
 * Initializes the connections subsystem.
 */
void gaim_connections_init(void);

/**
 * Uninitializes the connections subsystem.
 */
void gaim_connections_uninit(void);

/**
 * Returns the handle to the connections subsystem.
 *
 * @return The connections subsystem handle.
 */
void *gaim_connections_get_handle(void);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_CONNECTION_H_ */
