/**
 * @file connection.h Connection API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
 */
#ifndef _GAIM_CONNECTION_H_
#define _GAIM_CONNECTION_H_

#include <stdlib.h>
#include <time.h>

typedef struct _GaimConnection GaimConnection;

#define OPT_CONN_HTML		0x00000001
/* set this flag on a gc if you want serv_got_im to autoreply when away */
#define OPT_CONN_AUTO_RESP	0x00000002

typedef enum
{
	GAIM_DISCONNECTED = 0, /**< Disconnected. */
	GAIM_CONNECTED,        /**< Connected.    */
	GAIM_CONNECTING        /**< Connecting.   */

} GaimConnectionState;

#include "account.h"
#include "plugin.h"

typedef struct
{
	void (*connect_progress)(GaimConnection *gc, const char *text,
							 size_t step, size_t step_count);
	void (*connected)(GaimConnection *gc);
	void (*request_pass)(GaimConnection *gc);
	void (*disconnected)(GaimConnection *gc, const char *reason);
	void (*notice)(GaimConnection *gc, const char *text);

} GaimConnectionUiOps;

struct _GaimConnection
{
	GaimPlugin *prpl;            /**< The protocol plugin.               */
	guint32 flags;               /**< Connection flags.                  */

	GaimConnectionState state;   /**< The connection state.              */

	GaimAccount *account;        /**< The account being connected to.    */
	int inpa;                    /**< The input watcher.                 */

	GSList *buddy_chats;         /**< A list of active chats.            */
	void *proto_data;            /**< Protocol-specific data.            */

	char *display_name;          /**< The name displayed.                */
	guint keep_alive;            /**< Keep-alive.                        */

	guint idle_timer;            /**< The idle timer.                    */
	time_t login_time;           /**< Time of login.                     */
	time_t login_time_official;  /**< Official time of login.            */
	time_t last_sent_time;       /**< The time something was last sent.  */
	int is_idle;                 /**< Idle state of the connection.      */

	char *away;                  /**< The current away message, or NULL  */
	char *away_state;            /**< The last away type.                */
	gboolean is_auto_away;       /**< Whether or not it's auto-away.     */

	int evil;                    /**< Warning level for AIM (why is
	                                  this here?)                        */

	gboolean wants_to_die;	     /**< Wants to Die state.                */
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Connection API                                                  */
/**************************************************************************/

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

/*@}*/

/**************************************************************************/
/** @name UI Operations API                                               */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for connections.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_connection_ui_ops(GaimConnectionUiOps *ops);

/**
 * Returns the UI operations structure used for connections.
 *
 * @return The UI operations structure in use.
 */
GaimConnectionUiOps *gaim_get_connection_ui_ops(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_CONNECTION_H_ */
