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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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

/** Possible errors that can cause a connection to be closed. */
typedef enum
{
	/** There was an error sending or receiving on the network socket. */
	PURPLE_REASON_NETWORK_ERROR = 0,
	/** The username or password was invalid. */
	PURPLE_REASON_AUTHENTICATION_FAILED,
	/** There was an error negotiating SSL on this connection, or encryption
	 *  was unavailable and an account option was set to require it.
	 */
	PURPLE_REASON_ENCRYPTION_ERROR,
	/** Someone is already connected to the server using the name you are
	 *  trying to connect with.
	 */
	PURPLE_REASON_NAME_IN_USE,

	/** The server did not provide a SSL certificate. */
	PURPLE_REASON_CERT_NOT_PROVIDED,
	/** The server's SSL certificate could not be trusted. */
	PURPLE_REASON_CERT_UNTRUSTED,
	/** The server's SSL certificate has expired. */
	PURPLE_REASON_CERT_EXPIRED,
	/** The server's SSL certificate is not yet valid. */
	PURPLE_REASON_CERT_NOT_ACTIVATED,
	/** The server's SSL certificate did not match its hostname. */
	PURPLE_REASON_CERT_HOSTNAME_MISMATCH,
	/** The server's SSL certificate does not have the expected
	 *  fingerprint.
	 */
	PURPLE_REASON_CERT_FINGERPRINT_MISMATCH,
	/** The server's SSL certificate is self-signed.  */
	PURPLE_REASON_CERT_SELF_SIGNED,
	/** There was some other error validating the server's SSL certificate.
	 */
	PURPLE_REASON_CERT_OTHER_ERROR,

	/** Some other error occured which fits into none of the other
	 *  categories.
	 */
	PURPLE_REASON_OTHER_ERROR,

	/** The number of PurpleDisconnectReason elements; not a valid reason.
	 */
	PURPLE_NUM_REASONS
} PurpleDisconnectReason;

#include <time.h>

#include "account.h"
#include "plugin.h"
#include "status.h"

/** Connection UI operations.  Used to notify the user of changes to
 *  connections, such as being disconnected, and to respond to the
 *  underlying network connection appearing and disappearing.  UIs should
 *  call #purple_connections_set_ui_ops() with an instance of this struct.
 *
 *  @see @ref ui-ops
 */
typedef struct
{
	/** When an account is connecting, this operation is called to notify
	 *  the UI of what is happening, as well as which @a step out of @a
	 *  step_count has been reached (which might be displayed as a progress
	 *  bar).
	 *  @see #purple_connection_update_progress
	 */
	void (*connect_progress)(PurpleConnection *gc,
	                         const char *text,
	                         size_t step,
	                         size_t step_count);

	/** Called when a connection is established (just before the
	 *  @ref signed-on signal).
	 */
	void (*connected)(PurpleConnection *gc);
	/** Called when a connection is ended (between the @ref signing-off
	 *  and @ref signed-off signals).
	 */
	void (*disconnected)(PurpleConnection *gc);

	/** Used to display connection-specific notices.  (Pidgin's Gtk user
	 *  interface implements this as a no-op; #purple_connection_notice(),
	 *  which uses this operation, is not used by any of the protocols
	 *  shipped with libpurple.)
	 */
	void (*notice)(PurpleConnection *gc, const char *text);

	/** Called when an error causes a connection to be disconnected.
	 *  Called before #disconnected.
	 *  @param text  a localized error message.
	 *  @see #purple_connection_error
	 *  @deprecated in favour of
	 *              #PurpleConnectionUiOps.report_disconnect_reason.
	 */
	void (*report_disconnect)(PurpleConnection *gc, const char *text);

	/** Called when libpurple discovers that the computer's network
	 *  connection is active.  On Linux, this uses Network Manager if
	 *  available; on Windows, it uses Win32's network change notification
	 *  infrastructure.
	 */
	void (*network_connected)();
	/** Called when libpurple discovers that the computer's network
	 *  connection has gone away.
	 */
	void (*network_disconnected)();

	/** Called when a connection is disconnected, whether due to an
	 *  error or to user request.  Called before #disconnected.
	 *  @param reason  why the connection ended, if known, or
	 *                 PURPLE_REASON_OTHER_ERROR, if not.
	 *  @param text  a localized message describing the disconnection
	 *               in more detail to the user.
	 *  @see #purple_connection_error_reason
	 */
	void (*report_disconnect_reason)(PurpleConnection *gc,
	                                 PurpleDisconnectReason reason,
	                                 const char *text);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
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
 * This function should only be called by purple_account_unregister()
 * in account.c.
 *
 * Tries to unregister the account on the server. If the account is not
 * connected, also creates a new connection.
 *
 * @param account  The account to unregister
 * @param password The password to use.
 */
void purple_connection_new_unregister(PurpleAccount *account, const char *password, PurpleAccountUnregistrationCb cb, void *user_data);

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
 * @deprecated in favour of #purple_connection_error_reason.  Calling
 *  @c purple_connection_error(gc, text) is equivalent to calling
 *  @c purple_connection_error_reason(gc, PURPLE_REASON_OTHER_ERROR, text).
 */
void purple_connection_error(PurpleConnection *gc, const char *reason);

/**
 * Closes a connection with an error and an optional description of the
 * error.
 *
 * @param reason      why the connection is closing.
 * @param description a localized description of the error.
 */
void
purple_connection_error_reason (PurpleConnection *gc,
                                PurpleDisconnectReason reason,
                                const char *description);

/**
 * Reports whether a disconnection reason is fatal (in which case the account
 * should probably not be automatically reconnected) or transient (so
 * auto-reconnection is a good idea.
 * For instance, #PURPLE_REASON_NETWORK_ERROR is a temporary
 * error, which might be caused by losing the network connection, so
 * @a purple_connection_reason_is_fatal(PURPLE_REASON_NETWORK_ERROR) is
 * @a FALSE.  On the other hand, #PURPLE_REASON_AUTHENTICATION_FAILED probably
 * indicates a misconfiguration of the account which needs the user to go fix
 * it up, so @a
 * purple_connection_reason_is_fatal(PURPLE_REASON_AUTHENTICATION_FAILED)
 * is @a TRUE.
 *
 * (This function is meant to replace checking PurpleConnection.wants_to_die.)
 *
 * @return @a TRUE iff automatic reconnection is a bad idea.
 */
gboolean
purple_connection_reason_is_fatal (PurpleDisconnectReason reason);

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
 * @constreturn A list of all active connections.
 */
GList *purple_connections_get_all(void);

/**
 * Returns a list of all connections in the process of connecting.
 *
 * @constreturn A list of connecting connections.
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
#define PURPLE_CONNECTION_IS_VALID(gc) (g_list_find(purple_connections_get_all(), (gc)) != NULL)

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
