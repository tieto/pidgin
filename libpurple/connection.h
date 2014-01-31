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

#define PURPLE_TYPE_CONNECTION             (purple_connection_get_type())
#define PURPLE_CONNECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CONNECTION, PurpleConnection))
#define PURPLE_CONNECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CONNECTION, PurpleConnectionClass))
#define PURPLE_IS_CONNECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CONNECTION))
#define PURPLE_IS_CONNECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CONNECTION))
#define PURPLE_CONNECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CONNECTION, PurpleConnectionClass))

#define PURPLE_TYPE_CONNECTION_ERROR_INFO  (purple_connection_error_info_get_type())

typedef struct _PurpleConnection PurpleConnection;
typedef struct _PurpleConnectionClass PurpleConnectionClass;

/**
 * PurpleConnectionFlags:
 * @PURPLE_CONNECTION_FLAG_HTML: Connection sends/receives in 'HTML'
 * @PURPLE_CONNECTION_FLAG_NO_BGCOLOR: Connection does not send/receive
 *                                     background colors
 * @PURPLE_CONNECTION_FLAG_AUTO_RESP: Send auto responses when away
 * @PURPLE_CONNECTION_FLAG_FORMATTING_WBFO: The text buffer must be formatted
 *                                          as a whole
 * @PURPLE_CONNECTION_FLAG_NO_NEWLINES: No new lines are allowed in outgoing
 *                                      messages
 * @PURPLE_CONNECTION_FLAG_NO_FONTSIZE: Connection does not send/receive font
 *                                      sizes
 * @PURPLE_CONNECTION_FLAG_NO_URLDESC: Connection does not support descriptions
 *                                     with links
 * @PURPLE_CONNECTION_FLAG_NO_IMAGES: Connection does not support sending of
 *                                    images
 * @PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY: Connection supports sending
 *                                              and receiving custom smileys
 * @PURPLE_CONNECTION_FLAG_SUPPORT_MOODS: Connection supports setting moods
 * @PURPLE_CONNECTION_FLAG_SUPPORT_MOOD_MESSAGES: Connection supports setting
 *                                                a message on moods
 *
 * Flags to change behavior of the client for a given connection.
 */
typedef enum /*< flags >*/
{
	PURPLE_CONNECTION_FLAG_HTML       = 0x0001,
	PURPLE_CONNECTION_FLAG_NO_BGCOLOR = 0x0002,
	PURPLE_CONNECTION_FLAG_AUTO_RESP  = 0x0004,
	PURPLE_CONNECTION_FLAG_FORMATTING_WBFO = 0x0008,
	PURPLE_CONNECTION_FLAG_NO_NEWLINES = 0x0010,
	PURPLE_CONNECTION_FLAG_NO_FONTSIZE = 0x0020,
	PURPLE_CONNECTION_FLAG_NO_URLDESC = 0x0040,
	PURPLE_CONNECTION_FLAG_NO_IMAGES = 0x0080,
	PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY = 0x0100,
	PURPLE_CONNECTION_FLAG_SUPPORT_MOODS = 0x0200,
	PURPLE_CONNECTION_FLAG_SUPPORT_MOOD_MESSAGES = 0x0400
} PurpleConnectionFlags;

/**
 * PurpleConnectionState:
 * @PURPLE_CONNECTION_DISCONNECTED: Disconnected.
 * @PURPLE_CONNECTION_CONNECTED:    Connected.
 * @PURPLE_CONNECTION_CONNECTING:   Connecting.
 */
typedef enum
{
	PURPLE_CONNECTION_DISCONNECTED = 0,
	PURPLE_CONNECTION_CONNECTED,
	PURPLE_CONNECTION_CONNECTING
} PurpleConnectionState;

/**
 * PurpleConnectionError:
 * @PURPLE_CONNECTION_ERROR_NETWORK_ERROR: There was an error sending or
 *         receiving on the network socket, or there was some protocol error
 *         (such as the server sending malformed data).
 * @PURPLE_CONNECTION_ERROR_INVALID_USERNAME: The username supplied was not
 *         valid.
 * @PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED: The username, password or
 *         some other credential was incorrect.  Use
 *         #PURPLE_CONNECTION_ERROR_INVALID_USERNAME instead if the username
 *         is known to be invalid.
 * @PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE: libpurple doesn't speak
 *         any of the authentication methods the server offered.
 * @PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT: libpurple was built without SSL
 *         support, and the connection needs SSL.
 * @PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR: There was an error negotiating
 *         SSL on this connection, or the server does not support encryption
 *         but an account option was set to require it.
 * @PURPLE_CONNECTION_ERROR_NAME_IN_USE: Someone is already connected to the
 *         server using the name you are trying to connect with.
 * @PURPLE_CONNECTION_ERROR_INVALID_SETTINGS: The username/server/other
 *         preference for the account isn't valid.  For instance, on IRC the
 *         username cannot contain white space.  This reason should not be used
 *         for incorrect passwords etc: use
 *         #PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED for that.
 *         @todo This reason really shouldn't be necessary.  Usernames and
 *               other account preferences should be validated when the
 *               account is created.
 * @PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED: The server did not provide a
 *         SSL certificate.
 * @PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED: The server's SSL certificate could
 *         not be trusted.
 * @PURPLE_CONNECTION_ERROR_CERT_EXPIRED: The server's SSL certificate has
 *         expired.
 * @PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED: The server's SSL certificate is
 *         not yet valid.
 * @PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH: The server's SSL
 *         certificate did not match its hostname.
 * @PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH: The server's SSL
 *         certificate does not have the expected fingerprint.
 * @PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED: The server's SSL certificate is
 *         self-signed.
 * @PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR: There was some other error
 *         validating the server's SSL certificate.
 * @PURPLE_CONNECTION_ERROR_OTHER_ERROR: Some other error occurred which fits
 *         into none of the other categories.
 *
 * Possible errors that can cause a connection to be closed.
 */
typedef enum
{
	PURPLE_CONNECTION_ERROR_NETWORK_ERROR = 0,
	PURPLE_CONNECTION_ERROR_INVALID_USERNAME = 1,
	PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED = 2,
	PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE = 3,
	PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT = 4,
	PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR = 5,
	PURPLE_CONNECTION_ERROR_NAME_IN_USE = 6,
	PURPLE_CONNECTION_ERROR_INVALID_SETTINGS = 7,
	PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED = 8,
	PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED = 9,
	PURPLE_CONNECTION_ERROR_CERT_EXPIRED = 10,
	PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED = 11,
	PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH = 12,
	PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH = 13,
	PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED = 14,
	PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR = 15,

	/* purple_connection_error() in connection.c uses the fact that
	 * this is the last member of the enum when sanity-checking; if other
	 * reasons are added after it, the check must be updated.
	 */
	PURPLE_CONNECTION_ERROR_OTHER_ERROR = 16
} PurpleConnectionError;

/**
 * PurpleConnectionErrorInfo:
 * @type: The type of error.
 * @description: A localised, human-readable description of the error.
 *
 * Holds the type of an error along with its description.
 */
typedef struct
{
	PurpleConnectionError type;
	char *description;
} PurpleConnectionErrorInfo;

#include <time.h>

#include "account.h"
#include "plugin.h"
#include "status.h"
#include "sslconn.h"

/**
 * PurpleConnectionUiOps:
 *
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
	 * @reason:  why the connection ended, if known, or
	 *                #PURPLE_CONNECTION_ERROR_OTHER_ERROR, if not.
	 * @text:  a localized message describing the disconnection
	 *              in more detail to the user.
	 * @see #purple_connection_error
	 */
	void (*report_disconnect)(PurpleConnection *gc,
	                          PurpleConnectionError reason,
	                          const char *text);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleConnectionUiOps;

/**
 * PurpleConnection:
 *
 * Represents an active connection on an account.
 */
struct _PurpleConnection
{
	GObject gparent;
};

/**
 * PurpleConnectionClass:
 *
 * Base class for all #PurpleConnection's
 */
struct _PurpleConnectionClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Connection API                                                  */
/**************************************************************************/
/*@{*/

/**
 * purple_connection_get_type:
 *
 * Returns the GType for the Connection object.
 */
GType purple_connection_get_type(void);

/**
 * purple_connection_error_info_get_type:
 *
 * Returns the GType for the PurpleConnectionErrorInfo boxed structure.
 */
GType purple_connection_error_info_get_type(void);

/**
 * purple_connection_set_state:
 * @gc:    The connection.
 * @state: The connection state.
 *
 * Sets the connection state.  Protocols should call this and pass in
 * the state #PURPLE_CONNECTION_CONNECTED when the account is completely
 * signed on.  What does it mean to be completely signed on?  If
 * the core can call protocol's set_status, and it successfully changes
 * your status, then the account is online.
 */
void purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state);

/**
 * purple_connection_set_flags:
 * @gc:    The connection.
 * @flags: The flags.
 *
 * Sets the connection flags.
 */
void purple_connection_set_flags(PurpleConnection *gc, PurpleConnectionFlags flags);

/**
 * purple_connection_set_display_name:
 * @gc:   The connection.
 * @name: The displayed name.
 *
 * Sets the connection's displayed name.
 */
void purple_connection_set_display_name(PurpleConnection *gc, const char *name);

/**
 * purple_connection_set_protocol_data:
 * @connection: The PurpleConnection.
 * @proto_data: The protocol data to set for the connection.
 *
 * Sets the protocol data for a connection.
 */
void purple_connection_set_protocol_data(PurpleConnection *connection, void *proto_data);

/**
 * purple_connection_get_state:
 * @gc: The connection.
 *
 * Returns the connection state.
 *
 * Returns: The connection state.
 */
PurpleConnectionState purple_connection_get_state(const PurpleConnection *gc);

/**
 * purple_connection_get_flags:
 * @gc: The connection.
 *
 * Returns the connection flags.
 *
 * Returns: The connection flags.
 */
PurpleConnectionFlags purple_connection_get_flags(const PurpleConnection *gc);

/**
 * PURPLE_CONNECTION_IS_CONNECTED:
 *
 * Returns TRUE if the account is connected, otherwise returns FALSE.
 *
 * Returns: TRUE if the account is connected, otherwise returns FALSE.
 */
#define PURPLE_CONNECTION_IS_CONNECTED(gc) \
	(purple_connection_get_state(gc) == PURPLE_CONNECTION_CONNECTED)

/**
 * purple_connection_get_account:
 * @gc: The connection.
 *
 * Returns the connection's account.
 *
 * Returns: The connection's account.
 */
PurpleAccount *purple_connection_get_account(const PurpleConnection *gc);

/**
 * purple_connection_get_prpl:
 * @gc: The connection.
 *
 * Returns the protocol plugin managing a connection.
 *
 * Returns: The protocol plugin.
 */
PurplePlugin *purple_connection_get_prpl(const PurpleConnection *gc);

/**
 * purple_connection_get_password:
 * @gc: The connection.
 *
 * Returns the connection's password.
 *
 * Returns: The connection's password.
 */
const char *purple_connection_get_password(const PurpleConnection *gc);

/**
 * purple_connection_get_active_chats:
 * @gc: The connection.
 *
 * Returns a list of active chat conversations on a connection.
 *
 * Returns: The active chats on the connection.
 */
GSList *purple_connection_get_active_chats(const PurpleConnection *gc);

/**
 * purple_connection_get_display_name:
 * @gc: The connection.
 *
 * Returns the connection's displayed name.
 *
 * Returns: The connection's displayed name.
 */
const char *purple_connection_get_display_name(const PurpleConnection *gc);

/**
 * purple_connection_get_protocol_data:
 * @gc: The PurpleConnection.
 *
 * Gets the protocol data from a connection.
 *
 * Returns: The protocol data for the connection.
 */
void *purple_connection_get_protocol_data(const PurpleConnection *gc);

/**
 * purple_connection_update_progress:
 * @gc:    The connection.
 * @text:  Information on the current step.
 * @step:  The current step.
 * @count: The total number of steps.
 *
 * Updates the connection progress.
 */
void purple_connection_update_progress(PurpleConnection *gc, const char *text,
									 size_t step, size_t count);

/**
 * purple_connection_notice:
 * @gc:   The connection.
 * @text: The notice text.
 *
 * Displays a connection-specific notice.
 */
void purple_connection_notice(PurpleConnection *gc, const char *text);

/**
 * purple_connection_error:
 * @gc:          the connection which is closing.
 * @reason:      why the connection is closing.
 * @description: a localized description of the error (not %NULL ).
 *
 * Closes a connection with an error and a human-readable description of the
 * error.
 */
void
purple_connection_error(PurpleConnection *gc,
                        PurpleConnectionError reason,
                        const char *description);

/**
 * purple_connection_get_error_info:
 * @gc: The connection.
 *
 * Returns the #PurpleConnectionErrorInfo instance of a connection if an
 * error exists.
 *
 * Returns: The #PurpleConnectionErrorInfo instance of the connection if an
 *         error exists, %NULL otherwise.
 */
PurpleConnectionErrorInfo *
purple_connection_get_error_info(const PurpleConnection *gc);

/**
 * purple_connection_ssl_error:
 *
 * Closes a connection due to an SSL error; this is basically a shortcut to
 * turning the #PurpleSslErrorType into a #PurpleConnectionError and a
 * human-readable string and then calling purple_connection_error().
 */
void
purple_connection_ssl_error (PurpleConnection *gc,
                             PurpleSslErrorType ssl_error);

/**
 * purple_connection_error_is_fatal:
 *
 * Reports whether a disconnection reason is fatal (in which case the account
 * should probably not be automatically reconnected) or transient (so
 * auto-reconnection is a good idea).
 * For instance, #PURPLE_CONNECTION_ERROR_NETWORK_ERROR is a temporary error,
 * which might be caused by losing the network connection, so <tt>
 * purple_connection_error_is_fatal (PURPLE_CONNECTION_ERROR_NETWORK_ERROR)</tt>
 * is %FALSE.  On the other hand,
 * #PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED probably indicates a
 * misconfiguration of the account which needs the user to go fix it up, so
 * <tt> purple_connection_error_is_fatal
 * (PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED)</tt> is %TRUE.
 *
 * Returns: %TRUE if the account should not be automatically reconnected, and
 *         %FALSE otherwise.
 */
gboolean
purple_connection_error_is_fatal (PurpleConnectionError reason);

/**
 * purple_connection_update_last_received:
 * @gc:   The connection.
 *
 * Indicate that a packet was received on the connection.
 * Set by the protocol to avoid sending unneeded keepalives.
 */
void purple_connection_update_last_received(PurpleConnection *gc);

/*@}*/

/**************************************************************************/
/** @name Connections API                                                 */
/**************************************************************************/
/*@{*/

/**
 * purple_connections_disconnect_all:
 *
 * Disconnects from all connections.
 */
void purple_connections_disconnect_all(void);

/**
 * purple_connections_get_all:
 *
 * Returns a list of all active connections.  This does not
 * include connections that are in the process of connecting.
 *
 * Returns: (transfer none): A list of all active connections.
 */
GList *purple_connections_get_all(void);

/**
 * purple_connections_get_connecting:
 *
 * Returns a list of all connections in the process of connecting.
 *
 * Returns: (transfer none): A list of connecting connections.
 */
GList *purple_connections_get_connecting(void);

/**
 * PURPLE_CONNECTION_IS_VALID:
 *
 * Checks if gc is still a valid pointer to a gc.
 *
 * Returns: %TRUE if gc is valid.
 *
 * Deprecated: Do not use this.  Instead, cancel your asynchronous request
 *             when the PurpleConnection is destroyed.
 */
/*
 * TODO: Eventually this bad boy will be removed, because it is
 *       a gross fix for a crashy problem.
 */
#define PURPLE_CONNECTION_IS_VALID(gc) \
	(g_list_find(purple_connections_get_all(), (gc)) != NULL)

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * purple_connections_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used for connections.
 */
void purple_connections_set_ui_ops(PurpleConnectionUiOps *ops);

/**
 * purple_connections_get_ui_ops:
 *
 * Returns the UI operations structure used for connections.
 *
 * Returns: The UI operations structure in use.
 */
PurpleConnectionUiOps *purple_connections_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Connections Subsystem                                           */
/**************************************************************************/
/*@{*/

/**
 * purple_connections_init:
 *
 * Initializes the connections subsystem.
 */
void purple_connections_init(void);

/**
 * purple_connections_uninit:
 *
 * Uninitializes the connections subsystem.
 */
void purple_connections_uninit(void);

/**
 * purple_connections_get_handle:
 *
 * Returns the handle to the connections subsystem.
 *
 * Returns: The connections subsystem handle.
 */
void *purple_connections_get_handle(void);

/*@}*/


G_END_DECLS

#endif /* _PURPLE_CONNECTION_H_ */
