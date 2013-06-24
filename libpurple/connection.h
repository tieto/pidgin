/**
 * @file connection.h Connection API
 * @ingroup core
 * @see @ref connection-signals
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
#ifndef _PURPLE_CONNECTION_H_
#define _PURPLE_CONNECTION_H_

/** @copydoc _PurpleConnection */
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
	PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY = 0x0100, /**< Connection supports sending and receiving custom smileys */
	PURPLE_CONNECTION_SUPPORT_MOODS = 0x0200, /**< Connection supports setting moods */
	PURPLE_CONNECTION_SUPPORT_MOOD_MESSAGES = 0x0400 /**< Connection supports setting a message on moods */
} PurpleConnectionFlags;

typedef enum
{
	PURPLE_DISCONNECTED = 0, /**< Disconnected. */
	PURPLE_CONNECTED,        /**< Connected.    */
	PURPLE_CONNECTING        /**< Connecting.   */

} PurpleConnectionState;

/**
 * Possible errors that can cause a connection to be closed.
 */
typedef enum
{
	/** There was an error sending or receiving on the network socket, or
	 *  there was some protocol error (such as the server sending malformed
	 *  data).
	 */
	PURPLE_CONNECTION_ERROR_NETWORK_ERROR = 0,
	/** The username supplied was not valid. */
	PURPLE_CONNECTION_ERROR_INVALID_USERNAME = 1,
	/** The username, password or some other credential was incorrect.  Use
	 *  #PURPLE_CONNECTION_ERROR_INVALID_USERNAME instead if the username
	 *  is known to be invalid.
	 */
	PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED = 2,
	/** libpurple doesn't speak any of the authentication methods the
	 *  server offered.
	 */
	PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE = 3,
	/** libpurple was built without SSL support, and the connection needs
	 *  SSL.
	 */
	PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT = 4,
	/** There was an error negotiating SSL on this connection, or the
	 *  server does not support encryption but an account option was set to
	 *  require it.
	 */
	PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR = 5,
	/** Someone is already connected to the server using the name you are
	 *  trying to connect with.
	 */
	PURPLE_CONNECTION_ERROR_NAME_IN_USE = 6,

	/** The username/server/other preference for the account isn't valid.
	 *  For instance, on IRC the username cannot contain white space.
	 *  This reason should not be used for incorrect passwords etc: use
	 *  #PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED for that.
	 *
	 *  @todo This reason really shouldn't be necessary.  Usernames and
	 *        other account preferences should be validated when the
	 *        account is created.
	 */
	PURPLE_CONNECTION_ERROR_INVALID_SETTINGS = 7,

	/** The server did not provide a SSL certificate. */
	PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED = 8,
	/** The server's SSL certificate could not be trusted. */
	PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED = 9,
	/** The server's SSL certificate has expired. */
	PURPLE_CONNECTION_ERROR_CERT_EXPIRED = 10,
	/** The server's SSL certificate is not yet valid. */
	PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED = 11,
	/** The server's SSL certificate did not match its hostname. */
	PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH = 12,
	/** The server's SSL certificate does not have the expected
	 *  fingerprint.
	 */
	PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH = 13,
	/** The server's SSL certificate is self-signed.  */
	PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED = 14,
	/** There was some other error validating the server's SSL certificate.
	 */
	PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR = 15,

	/** Some other error occurred which fits into none of the other
	 *  categories.
	 */
	/* purple_connection_error() in connection.c uses the fact that
	 * this is the last member of the enum when sanity-checking; if other
	 * reasons are added after it, the check must be updated.
	 */
	PURPLE_CONNECTION_ERROR_OTHER_ERROR = 16
} PurpleConnectionError;

/** Holds the type of an error along with its description. */
typedef struct
{
	/** The type of error. */
	PurpleConnectionError type;
	/** A localised, human-readable description of the error. */
	char *description;
} PurpleConnectionErrorInfo;

#include <time.h>

#include "account.h"
#include "plugin.h"
#include "status.h"
#include "sslconn.h"

/**
 * Connection UI operations.  Used to notify the user of changes to
 * connections, such as being disconnected, and to respond to the
 * underlying network connection appearing and disappearing.  UIs should
 * call #purple_connections_set_ui_ops() with an instance of this struct.
 *
 * @see @ref ui-ops
 */
typedef struct
{
	/**
	 * When an account is connecting, this operation is called to notify
	 * the UI of what is happening, as well as which @a step out of @a
	 * step_count has been reached (which might be displayed as a progress
	 * bar).
	 * @see #purple_connection_update_progress
	 */
	void (*connect_progress)(PurpleConnection *gc,
	                         const char *text,
	                         size_t step,
	                         size_t step_count);

	/**
	 * Called when a connection is established (just before the
	 * @ref signed-on signal).
	 */
	void (*connected)(PurpleConnection *gc);

	/**
	 * Called when a connection is ended (between the @ref signing-off
	 * and @ref signed-off signals).
	 */
	void (*disconnected)(PurpleConnection *gc);

	/**
	 * Used to display connection-specific notices.  (Pidgin's Gtk user
	 * interface implements this as a no-op; #purple_connection_notice(),
	 * which uses this operation, is not used by any of the protocols
	 * shipped with libpurple.)
	 */
	void (*notice)(PurpleConnection *gc, const char *text);

	/**
	 * Called when libpurple discovers that the computer's network
	 * connection is active.  On Linux, this uses Network Manager if
	 * available; on Windows, it uses Win32's network change notification
	 * infrastructure.
	 */
	void (*network_connected)(void);

	/**
	 * Called when libpurple discovers that the computer's network
	 * connection has gone away.
	 */
	void (*network_disconnected)(void);

	/**
	 * Called when an error causes a connection to be disconnected.
	 * Called before #disconnected.
	 *
	 * @param reason  why the connection ended, if known, or
	 *                #PURPLE_CONNECTION_ERROR_OTHER_ERROR, if not.
	 * @param text  a localized message describing the disconnection
	 *              in more detail to the user.
	 * @see #purple_connection_error
	 */
	void (*report_disconnect)(PurpleConnection *gc,
	                          PurpleConnectionError reason,
	                          const char *text);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
} PurpleConnectionUiOps;


/* Represents an active connection on an account. */
struct _PurpleConnection
{
	PurplePlugin *prpl;            /**< The protocol plugin.               */
	PurpleConnectionFlags flags;   /**< Connection flags.                  */

	PurpleConnectionState state;   /**< The connection state.              */

	PurpleAccount *account;        /**< The account being connected to.    */
	char *password;              /**< The password used.                 */

	GSList *buddy_chats;         /**< A list of active chats
	                                  (#PurpleChatConversation structs). */
	void *proto_data;            /**< Protocol-specific data.            */

	char *display_name;          /**< How you appear to other people.    */
	guint keepalive;             /**< Keep-alive.                        */

	/** Wants to Die state.  This is set when the user chooses to log out, or
	 * when the protocol is disconnected and should not be automatically
	 * reconnected (incorrect password, etc.).  prpls should rely on
	 * purple_connection_error() to set this for them rather than
	 * setting it themselves.
	 * @see purple_connection_error_is_fatal
	 */
	gboolean wants_to_die;

	guint disconnect_timeout;    /**< Timer used for nasty stack tricks  */
	time_t last_received;        /**< When we last received a packet. Set by the
					  prpl to avoid sending unneeded keepalives */
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Connection API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Sets the connection state.  PRPLs should call this and pass in
 * the state #PURPLE_CONNECTED when the account is completely
 * signed on.  What does it mean to be completely signed on?  If
 * the core can call prpl->set_status, and it successfully changes
 * your status, then the account is online.
 *
 * @param gc    The connection.
 * @param state The connection state.
 */
void purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state);

/**
 * Sets the connection flags.
 *
 * @param gc    The connection.
 * @param flags The flags.
 */
void purple_connection_set_flags(PurpleConnection *gc, PurpleConnectionFlags flags);

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
 * Sets the protocol data for a connection.
 *
 * @param connection The PurpleConnection.
 * @param proto_data The protocol data to set for the connection.
 */
void purple_connection_set_protocol_data(PurpleConnection *connection, void *proto_data);

/**
 * Returns the connection state.
 *
 * @param gc The connection.
 *
 * @return The connection state.
 */
PurpleConnectionState purple_connection_get_state(const PurpleConnection *gc);

/**
 * Returns the connection flags.
 *
 * @param gc The connection.
 *
 * @return The connection flags.
 */
PurpleConnectionFlags purple_connection_get_flags(const PurpleConnection *gc);

/**
 * Returns TRUE if the account is connected, otherwise returns FALSE.
 *
 * @return TRUE if the account is connected, otherwise returns FALSE.
 */
#define PURPLE_CONNECTION_IS_CONNECTED(gc) \
	(purple_connection_get_state(gc) == PURPLE_CONNECTED)

/**
 * Returns the connection's account.
 *
 * @param gc The connection.
 *
 * @return The connection's account.
 */
PurpleAccount *purple_connection_get_account(const PurpleConnection *gc);

/**
 * Returns the protocol plugin managing a connection.
 *
 * @param gc The connection.
 *
 * @return The protocol plugin.
 */
PurplePlugin * purple_connection_get_prpl(const PurpleConnection *gc);

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
 * Gets the protocol data from a connection.
 *
 * @param connection The PurpleConnection.
 *
 * @return The protocol data for the connection.
 */
void *purple_connection_get_protocol_data(const PurpleConnection *connection);

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
 * Closes a connection with an error and a human-readable description of the
 * error.  It also sets @c gc->wants_to_die to the value of
 * #purple_connection_error_is_fatal(@a reason), mainly for
 * backwards-compatibility.
 *
 * @param gc          the connection which is closing.
 * @param reason      why the connection is closing.
 * @param description a non-@c NULL localized description of the error.
 */
void
purple_connection_error(PurpleConnection *gc,
                        PurpleConnectionError reason,
                        const char *description);

/**
 * Closes a connection due to an SSL error; this is basically a shortcut to
 * turning the #PurpleSslErrorType into a #PurpleConnectionError and a
 * human-readable string and then calling purple_connection_error().
 */
void
purple_connection_ssl_error (PurpleConnection *gc,
                             PurpleSslErrorType ssl_error);

/**
 * Reports whether a disconnection reason is fatal (in which case the account
 * should probably not be automatically reconnected) or transient (so
 * auto-reconnection is a good idea).
 * For instance, #PURPLE_CONNECTION_ERROR_NETWORK_ERROR is a temporary error,
 * which might be caused by losing the network connection, so <tt>
 * purple_connection_error_is_fatal (PURPLE_CONNECTION_ERROR_NETWORK_ERROR)</tt>
 * is @c FALSE.  On the other hand,
 * #PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED probably indicates a
 * misconfiguration of the account which needs the user to go fix it up, so
 * <tt> purple_connection_error_is_fatal
 * (PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED)</tt> is @c TRUE.
 *
 * (This function is meant to replace checking PurpleConnection.wants_to_die.)
 *
 * @return @c TRUE if the account should not be automatically reconnected, and
 *         @c FALSE otherwise.
 */
gboolean
purple_connection_error_is_fatal (PurpleConnectionError reason);

/**
 * Indicate that a packet was received on the connection.
 * Set by the prpl to avoid sending unneeded keepalives.
 *
 * @param gc   The connection.
 */
void purple_connection_update_last_received(PurpleConnection *gc);

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
 *
 * @deprecated Do not use this.  Instead, cancel your asynchronous request
 *             when the PurpleConnection is destroyed.
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


G_END_DECLS

#endif /* _PURPLE_CONNECTION_H_ */
