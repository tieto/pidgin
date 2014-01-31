/**
 * @file xfer.h File Transfer API
 * @ingroup core
 * @see @ref xfer-signals
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
#ifndef _PURPLE_XFER_H_
#define _PURPLE_XFER_H_

#define PURPLE_TYPE_XFER             (purple_xfer_get_type())
#define PURPLE_XFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_XFER, PurpleXfer))
#define PURPLE_XFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_XFER, PurpleXferClass))
#define PURPLE_IS_XFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_XFER))
#define PURPLE_IS_XFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_XFER))
#define PURPLE_XFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_XFER, PurpleXferClass))

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/
typedef struct _PurpleXfer PurpleXfer;
typedef struct _PurpleXferClass PurpleXferClass;

#include <glib.h>
#include <stdio.h>

#include "account.h"

/**
 * PurpleXferType:
 * @PURPLE_XFER_TYPE_UNKNOWN: Unknown file transfer type.
 * @PURPLE_XFER_TYPE_SEND:    File sending.
 * @PURPLE_XFER_TYPE_RECEIVE: File receiving.
 *
 * Types of file transfers.
 */
typedef enum
{
	PURPLE_XFER_TYPE_UNKNOWN = 0,
	PURPLE_XFER_TYPE_SEND,
	PURPLE_XFER_TYPE_RECEIVE

} PurpleXferType;

/**
 * PurpleXferStatus:
 * @PURPLE_XFER_STATUS_UNKNOWN:       Unknown, the xfer may be null.
 * @PURPLE_XFER_STATUS_NOT_STARTED:   It hasn't started yet.
 * @PURPLE_XFER_STATUS_ACCEPTED:      Receive accepted, but destination file
 *                                    not selected yet
 * @PURPLE_XFER_STATUS_STARTED:       purple_xfer_start has been called.
 * @PURPLE_XFER_STATUS_DONE:          The xfer completed successfully.
 * @PURPLE_XFER_STATUS_CANCEL_LOCAL:  The xfer was cancelled by us.
 * @PURPLE_XFER_STATUS_CANCEL_REMOTE: The xfer was cancelled by the other end,
 *                                    or we couldn't connect.
 *
 * The different states of the xfer.
 */
typedef enum
{
	PURPLE_XFER_STATUS_UNKNOWN = 0,
	PURPLE_XFER_STATUS_NOT_STARTED,
	PURPLE_XFER_STATUS_ACCEPTED,
	PURPLE_XFER_STATUS_STARTED,
	PURPLE_XFER_STATUS_DONE,
	PURPLE_XFER_STATUS_CANCEL_LOCAL,
	PURPLE_XFER_STATUS_CANCEL_REMOTE
} PurpleXferStatus;

/**
 * PurpleXferUiOps:
 *
 * File transfer UI operations.
 *
 * Any UI representing a file transfer must assign a filled-out
 * PurpleXferUiOps structure to the purple_xfer.
 */
typedef struct
{
	void (*new_xfer)(PurpleXfer *xfer);
	void (*destroy)(PurpleXfer *xfer);
	void (*add_xfer)(PurpleXfer *xfer);
	void (*update_progress)(PurpleXfer *xfer, double percent);
	void (*cancel_local)(PurpleXfer *xfer);
	void (*cancel_remote)(PurpleXfer *xfer);

	/**
	 * UI op to write data received from the protocol. The UI must deal with the
	 * entire buffer and return size, or it is treated as an error.
	 *
	 * @xfer:    The file transfer structure
	 * @buffer:  The buffer to write
	 * @size:    The size of the buffer
	 *
	 * Returns: size if the write was successful, or a value between 0 and
	 *         size on error.
	 */
	gssize (*ui_write)(PurpleXfer *xfer, const guchar *buffer, gssize size);

	/**
	 * UI op to read data to send to the protocol for a file transfer.
	 *
	 * @xfer:    The file transfer structure
	 * @buffer:  A pointer to a buffer. The UI must allocate this buffer.
	 *                libpurple will free the data.
	 * @size:    The maximum amount of data to put in the buffer.
	 *
	 * Returns: The amount of data in the buffer, 0 if nothing is available,
	 *          and a negative value if an error occurred and the transfer
	 *          should be cancelled (libpurple will cancel).
	 */
	gssize (*ui_read)(PurpleXfer *xfer, guchar **buffer, gssize size);

	/**
	 * Op to notify the UI that not all the data read in was written. The UI
	 * should re-enqueue this data and return it the next time read is called.
	 *
	 * This MUST be implemented if read and write are implemented.
	 *
	 * @xfer:    The file transfer structure
	 * @buffer:  A pointer to the beginning of the unwritten data.
	 * @size:    The amount of unwritten data.
	 */
	void (*data_not_sent)(PurpleXfer *xfer, const guchar *buffer, gsize size);

	/**
	 * Op to create a thumbnail image for a file transfer
	 *
	 * @xfer:   The file transfer structure
	 */
	void (*add_thumbnail)(PurpleXfer *xfer, const gchar *formats);
} PurpleXferUiOps;

/**
 * PurpleXfer:
 * @ui_data: The UI data associated with this file transfer. This is a
 *           convenience field provided to the UIs -- it is not used by the
 *           libpurple core.
 *
 * A core representation of a file transfer.
 */
struct _PurpleXfer
{
	GObject gparent;

	gpointer ui_data;
};

/**
 * PurpleXferClass:
 *
 * Base class for all #PurpleXfer's
 */
struct _PurpleXferClass
{
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name File Transfer API                                               */
/**************************************************************************/
/*@{*/

/**
 * purple_xfer_get_type:
 *
 * Returns the GType for the PurpleXfer object.
 */
GType purple_xfer_get_type(void);

/**
 * purple_xfer_new:
 * @account: The account sending or receiving the file.
 * @type:    The type of file transfer.
 * @who:     The name of the remote user.
 *
 * Creates a new file transfer handle.
 * This is called by protocols.
 *
 * Returns: A file transfer handle.
 */
PurpleXfer *purple_xfer_new(PurpleAccount *account,
								PurpleXferType type, const char *who);

/**
 * purple_xfer_request:
 * @xfer: The file transfer to request confirmation on.
 *
 * Requests confirmation for a file transfer from the user. If receiving
 * a file which is known at this point, this requests user to accept and
 * save the file. If the filename is unknown (not set) this only requests user
 * to accept the file transfer. In this case protocol must call this function
 * again once the filename is available.
 */
void purple_xfer_request(PurpleXfer *xfer);

/**
 * purple_xfer_request_accepted:
 * @xfer:     The file transfer.
 * @filename: The filename.
 *
 * Called if the user accepts the file transfer request.
 */
void purple_xfer_request_accepted(PurpleXfer *xfer, const char *filename);

/**
 * purple_xfer_request_denied:
 * @xfer: The file transfer.
 *
 * Called if the user rejects the file transfer request.
 */
void purple_xfer_request_denied(PurpleXfer *xfer);

/**
 * purple_xfer_get_fd:
 * @xfer: The file transfer.
 *
 * Returns the socket file descriptor.
 *
 * Returns: The socket file descriptor.
 */
int purple_xfer_get_fd(PurpleXfer *xfer);

/**
 * purple_xfer_get_watcher:
 * @xfer: The file transfer.
 *
 * Returns the Watcher for the transfer.
 *
 * Returns: The watcher.
 */
int purple_xfer_get_watcher(PurpleXfer *xfer);

/**
 * purple_xfer_get_xfer_type:
 * @xfer: The file transfer.
 *
 * Returns the type of file transfer.
 *
 * Returns: The type of the file transfer.
 */
PurpleXferType purple_xfer_get_xfer_type(const PurpleXfer *xfer);

/**
 * purple_xfer_get_account:
 * @xfer: The file transfer.
 *
 * Returns the account the file transfer is using.
 *
 * Returns: The account.
 */
PurpleAccount *purple_xfer_get_account(const PurpleXfer *xfer);

/**
 * purple_xfer_set_remote_user:
 * @xfer: The file transfer.
 * @who:  The name of the remote user.
 *
 * Sets the name of the remote user.
 */
void purple_xfer_set_remote_user(PurpleXfer *xfer, const char *who);

/**
 * purple_xfer_get_remote_user:
 * @xfer: The file transfer.
 *
 * Returns the name of the remote user.
 *
 * Returns: The name of the remote user.
 */
const char *purple_xfer_get_remote_user(const PurpleXfer *xfer);

/**
 * purple_xfer_get_status:
 * @xfer: The file transfer.
 *
 * Returns the status of the xfer.
 *
 * Returns: The status.
 */
PurpleXferStatus purple_xfer_get_status(const PurpleXfer *xfer);

/**
 * purple_xfer_is_cancelled:
 * @xfer: The file transfer.
 *
 * Returns true if the file transfer was cancelled.
 *
 * Returns: Whether or not the transfer was cancelled.
 */
gboolean purple_xfer_is_cancelled(const PurpleXfer *xfer);

/**
 * purple_xfer_is_completed:
 * @xfer: The file transfer.
 *
 * Returns the completed state for a file transfer.
 *
 * Returns: The completed state.
 */
gboolean purple_xfer_is_completed(const PurpleXfer *xfer);

/**
 * purple_xfer_get_filename:
 * @xfer: The file transfer.
 *
 * Returns the name of the file being sent or received.
 *
 * Returns: The filename.
 */
const char *purple_xfer_get_filename(const PurpleXfer *xfer);

/**
 * purple_xfer_get_local_filename:
 * @xfer: The file transfer.
 *
 * Returns the file's destination filename,
 *
 * Returns: The destination filename.
 */
const char *purple_xfer_get_local_filename(const PurpleXfer *xfer);

/**
 * purple_xfer_get_bytes_sent:
 * @xfer: The file transfer.
 *
 * Returns the number of bytes sent (or received) so far.
 *
 * Returns: The number of bytes sent.
 */
goffset purple_xfer_get_bytes_sent(const PurpleXfer *xfer);

/**
 * purple_xfer_get_bytes_remaining:
 * @xfer: The file transfer.
 *
 * Returns the number of bytes remaining to send or receive.
 *
 * Returns: The number of bytes remaining.
 */
goffset purple_xfer_get_bytes_remaining(const PurpleXfer *xfer);

/**
 * purple_xfer_get_size:
 * @xfer: The file transfer.
 *
 * Returns the size of the file being sent or received.
 *
 * Returns: The total size of the file.
 */
goffset purple_xfer_get_size(const PurpleXfer *xfer);

/**
 * purple_xfer_get_progress:
 * @xfer: The file transfer.
 *
 * Returns the current percentage of progress of the transfer.
 *
 * This is a number between 0 (0%) and 1 (100%).
 *
 * Returns: The percentage complete.
 */
double purple_xfer_get_progress(const PurpleXfer *xfer);

/**
 * purple_xfer_get_local_port:
 * @xfer: The file transfer.
 *
 * Returns the local port number in the file transfer.
 *
 * Returns: The port number on this end.
 */
guint16 purple_xfer_get_local_port(const PurpleXfer *xfer);

/**
 * purple_xfer_get_remote_ip:
 * @xfer: The file transfer.
 *
 * Returns the remote IP address in the file transfer.
 *
 * Returns: The IP address on the other end.
 */
const char *purple_xfer_get_remote_ip(const PurpleXfer *xfer);

/**
 * purple_xfer_get_remote_port:
 * @xfer: The file transfer.
 *
 * Returns the remote port number in the file transfer.
 *
 * Returns: The port number on the other end.
 */
guint16 purple_xfer_get_remote_port(const PurpleXfer *xfer);

/**
 * purple_xfer_get_start_time:
 * @xfer:  The file transfer.
 *
 * Returns the time the transfer of a file started.
 *
 * Returns: The time when the transfer started.
 */
time_t purple_xfer_get_start_time(const PurpleXfer *xfer);

/**
 * purple_xfer_get_end_time:
 * @xfer:  The file transfer.
 *
 * Returns the time the transfer of a file ended.
 *
 * Returns: The time when the transfer ended.
 */
time_t purple_xfer_get_end_time(const PurpleXfer *xfer);

/**
 * purple_xfer_set_fd:
 * @xfer:      The file transfer.
 * @fd:        The file descriptor.
 *
 * Sets the socket file descriptor.
 */
void purple_xfer_set_fd(PurpleXfer *xfer, int fd);

/**
 * purple_xfer_set_watcher:
 * @xfer:      The file transfer.
 * @watcher:   The watcher.
 *
 * Sets the watcher for the file transfer.
 */
void purple_xfer_set_watcher(PurpleXfer *xfer, int watcher);

/**
 * purple_xfer_set_completed:
 * @xfer:      The file transfer.
 * @completed: The completed state.
 *
 * Sets the completed state for the file transfer.
 */
void purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed);

/**
 * purple_xfer_set_status:
 * @xfer:      The file transfer.
 * @status:    The current status.
 *
 * Sets the current status for the file transfer.
 */
void purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatus status);

/**
 * purple_xfer_set_message:
 * @xfer:     The file transfer.
 * @message: The message.
 *
 * Sets the message for the file transfer.
 */
void purple_xfer_set_message(PurpleXfer *xfer, const char *message);

/**
 * purple_xfer_get_message:
 * @xfer:     The file transfer.
 *
 * Returns the message for the file transfer.
 *
 * Returns: The message.
 */
const char *purple_xfer_get_message(const PurpleXfer *xfer);

/**
 * purple_xfer_set_filename:
 * @xfer:     The file transfer.
 * @filename: The filename.
 *
 * Sets the filename for the file transfer.
 */
void purple_xfer_set_filename(PurpleXfer *xfer, const char *filename);

/**
 * purple_xfer_set_local_filename:
 * @xfer:     The file transfer.
 * @filename: The filename
 *
 * Sets the local filename for the file transfer.
 */
void purple_xfer_set_local_filename(PurpleXfer *xfer, const char *filename);

/**
 * purple_xfer_set_size:
 * @xfer: The file transfer.
 * @size: The size of the file.
 *
 * Sets the size of the file in a file transfer.
 */
void purple_xfer_set_size(PurpleXfer *xfer, goffset size);

/**
 * purple_xfer_set_local_port:
 * @xfer:          The file transfer.
 * @local_port:    The local port.
 *
 * Sets the local port of the file transfer.
 */
void purple_xfer_set_local_port(PurpleXfer *xfer, guint16 local_port);

/**
 * purple_xfer_set_bytes_sent:
 * @xfer:       The file transfer.
 * @bytes_sent: The new current position in the file.  If we're
 *                   sending a file then this is the byte that we will
 *                   send.  If we're receiving a file, this is the
 *                   next byte that we expect to receive.
 *
 * Sets the current working position in the active file transfer.  This
 * can be used to jump backward in the file if the protocol detects
 * that some bit of data needs to be resent or has been sent twice.
 *
 * It's used for pausing and resuming an oscar file transfer.
 */
void purple_xfer_set_bytes_sent(PurpleXfer *xfer, goffset bytes_sent);

/**
 * purple_xfer_get_ui_ops:
 * @xfer: The file transfer.
 *
 * Returns the UI operations structure for a file transfer.
 *
 * Returns: The UI operations structure.
 */
PurpleXferUiOps *purple_xfer_get_ui_ops(const PurpleXfer *xfer);

/**
 * purple_xfer_set_read_fnc:
 * @xfer: The file transfer.
 * @fnc:  The read function.
 *
 * Sets the read function for the file transfer.
 */
void purple_xfer_set_read_fnc(PurpleXfer *xfer,
		gssize (*fnc)(guchar **, size_t, PurpleXfer *));

/**
 * purple_xfer_set_write_fnc:
 * @xfer: The file transfer.
 * @fnc:  The write function.
 *
 * Sets the write function for the file transfer.
 */
void purple_xfer_set_write_fnc(PurpleXfer *xfer,
		gssize (*fnc)(const guchar *, size_t, PurpleXfer *));

/**
 * purple_xfer_set_ack_fnc:
 * @xfer: The file transfer.
 * @fnc:  The acknowledge function.
 *
 * Sets the acknowledge function for the file transfer.
 */
void purple_xfer_set_ack_fnc(PurpleXfer *xfer,
		void (*fnc)(PurpleXfer *, const guchar *, size_t));

/**
 * purple_xfer_set_request_denied_fnc:
 * @xfer: The file transfer.
 * @fnc: The request denied protocol callback.
 *
 * Sets the function to be called if the request is denied.
 */
void purple_xfer_set_request_denied_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_set_init_fnc:
 * @xfer: The file transfer.
 * @fnc:  The transfer initialization function.
 *
 * Sets the transfer initialization function for the file transfer.
 *
 * This function is required, and must call purple_xfer_start() with
 * the necessary parameters. This will be called if the file transfer
 * is accepted by the user.
 */
void purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_set_start_fnc:
 * @xfer: The file transfer.
 * @fnc:  The start transfer function.
 *
 * Sets the start transfer function for the file transfer.
 */
void purple_xfer_set_start_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_set_end_fnc:
 * @xfer: The file transfer.
 * @fnc:  The end transfer function.
 *
 * Sets the end transfer function for the file transfer.
 */
void purple_xfer_set_end_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_set_cancel_send_fnc:
 * @xfer: The file transfer.
 * @fnc:  The cancel send function.
 *
 * Sets the cancel send function for the file transfer.
 */
void purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_set_cancel_recv_fnc:
 * @xfer: The file transfer.
 * @fnc:  The cancel receive function.
 *
 * Sets the cancel receive function for the file transfer.
 */
void purple_xfer_set_cancel_recv_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * purple_xfer_read:
 * @xfer:   The file transfer.
 * @buffer: The buffer that will be created to contain the data.
 *
 * Reads in data from a file transfer stream.
 *
 * Returns: The number of bytes read, or -1.
 */
gssize purple_xfer_read(PurpleXfer *xfer, guchar **buffer);

/**
 * purple_xfer_write:
 * @xfer:   The file transfer.
 * @buffer: The buffer to read the data from.
 * @size:   The number of bytes to write.
 *
 * Writes data to a file transfer stream.
 *
 * Returns: The number of bytes written, or -1.
 */
gssize purple_xfer_write(PurpleXfer *xfer, const guchar *buffer, gsize size);

/**
 * purple_xfer_write_file:
 * @xfer:   The file transfer.
 * @buffer: The buffer to read the data from.
 * @size:   The number of bytes to write.
 *
 * Writes chunk of received file.
 *
 * Returns: TRUE on success, FALSE otherwise.
 */
gboolean
purple_xfer_write_file(PurpleXfer *xfer, const guchar *buffer, gsize size);

/**
 * purple_xfer_read_file:
 * @xfer:   The file transfer.
 * @buffer: The buffer to write the data to.
 * @size:   The size of buffer.
 *
 * Writes chunk of file being sent.
 *
 * Returns: Number of bytes written (0 means, the device is busy), or -1 on
 *         failure.
 */
gssize
purple_xfer_read_file(PurpleXfer *xfer, guchar *buffer, gsize size);

/**
 * purple_xfer_start:
 * @xfer: The file transfer.
 * @fd:   The file descriptor for the socket.
 * @ip:   The IP address to connect to.
 * @port: The port to connect to.
 *
 * Starts a file transfer.
 *
 * Either @fd must be specified <i>or</i> @ip and @port on a
 * file receive transfer. On send, @fd must be specified, and
 * @ip and @port are ignored.
 *
 * Passing @fd as '-1' is a special-case and indicates to the
 * protocol plugin to facilitate the file transfer itself.
 */
void purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip, guint16 port);

/**
 * purple_xfer_end:
 * @xfer: The file transfer.
 *
 * Ends a file transfer.
 */
void purple_xfer_end(PurpleXfer *xfer);

/**
 * purple_xfer_add:
 * @xfer: The file transfer.
 *
 * Adds a new file transfer to the list of file transfers. Call this only
 * if you are not using purple_xfer_start.
 */
void purple_xfer_add(PurpleXfer *xfer);

/**
 * purple_xfer_cancel_local:
 * @xfer: The file transfer.
 *
 * Cancels a file transfer on the local end.
 */
void purple_xfer_cancel_local(PurpleXfer *xfer);

/**
 * purple_xfer_cancel_remote:
 * @xfer: The file transfer.
 *
 * Cancels a file transfer from the remote end.
 */
void purple_xfer_cancel_remote(PurpleXfer *xfer);

/**
 * purple_xfer_error:
 * @type:    The type of file transfer.
 * @account: The account sending or receiving the file.
 * @who:     The user on the other end of the transfer.
 * @msg:     The message to display.
 *
 * Displays a file transfer-related error message.
 *
 * This is a wrapper around purple_notify_error(), which automatically
 * specifies a title ("File transfer to <i>user</i> failed" or
 * "File Transfer from <i>user</i> failed").
 */
void purple_xfer_error(PurpleXferType type, PurpleAccount *account, const char *who, const char *msg);

/**
 * purple_xfer_update_progress:
 * @xfer:      The file transfer.
 *
 * Updates file transfer progress.
 */
void purple_xfer_update_progress(PurpleXfer *xfer);

/**
 * purple_xfer_conversation_write:
 * @xfer: The file transfer to which this message relates.
 * @message: The message to display.
 * @is_error: Is this an error message?.
 *
 * Displays a file transfer-related message in the conversation window
 *
 * This is a wrapper around purple_conversation_write
 */
void purple_xfer_conversation_write(PurpleXfer *xfer, const gchar *message, gboolean is_error);

/**
 * purple_xfer_ui_ready:
 * @xfer: The file transfer which is ready.
 *
 * Allows the UI to signal it's ready to send/receive data (depending on
 * the direction of the file transfer. Used when the UI is providing
 * read/write/data_not_sent UI ops.
 */
void purple_xfer_ui_ready(PurpleXfer *xfer);

/**
 * purple_xfer_protocol_ready:
 * @xfer: The file transfer which is ready.
 *
 * Allows the protocol to signal it's ready to send/receive data (depending on
 * the direction of the file transfer. Used when the protocol provides read/write
 * ops and cannot/does not provide a raw fd to the core.
 */
void purple_xfer_prpl_ready(PurpleXfer *xfer);

/**
 * purple_xfer_get_thumbnail:
 * @xfer: The file transfer to get the thumbnail for
 * @len:  If not %NULL, the length of the thumbnail data returned
 *             will be set in the location pointed to by this.
 *
 * Gets the thumbnail data for a transfer
 *
 * Returns: The thumbnail data, or NULL if there is no thumbnail
 */
gconstpointer purple_xfer_get_thumbnail(const PurpleXfer *xfer, gsize *len);

/**
 * purple_xfer_get_thumbnail_mimetype:
 * @xfer: The file transfer to get the mimetype for
 *
 * Gets the mimetype of the thumbnail preview for a transfer
 *
 * Returns: The mimetype of the thumbnail, or %NULL if not thumbnail is set
 */
const gchar *purple_xfer_get_thumbnail_mimetype(const PurpleXfer *xfer);


/**
 * purple_xfer_set_thumbnail:
 * @xfer: The file transfer to set the data for
 * @thumbnail: A pointer to the thumbnail data, this will be copied
 * @size: The size in bytes of the passed in thumbnail data
 * @mimetype: The mimetype of the generated thumbnail
 *
 * Sets the thumbnail data for a transfer
 */
void purple_xfer_set_thumbnail(PurpleXfer *xfer, gconstpointer thumbnail,
	gsize size, const gchar *mimetype);

/**
 * purple_xfer_prepare_thumbnail:
 * @xfer: The file transfer to create a thumbnail for
 * @formats: A comma-separated list of mimetypes for image formats
 *	 	  the protocols can use for thumbnails.
 *
 * Prepare a thumbnail for a transfer (if the UI supports it)
 * will be no-op in case the UI doesn't implement thumbnail creation
 */
void purple_xfer_prepare_thumbnail(PurpleXfer *xfer, const gchar *formats);

/**
 * purple_xfer_set_protocol_data:
 * @xfer:			The file transfer.
 * @proto_data:	The protocol data to set for the file transfer.
 *
 * Sets the protocol data for a file transfer.
 */
void purple_xfer_set_protocol_data(PurpleXfer *xfer, gpointer proto_data);
 
/**
 * purple_xfer_get_protocol_data:
 * @xfer:			The file transfer.
 *
 * Gets the protocol data for a file transfer.
 *
 * Returns: The protocol data for the file transfer.
 */
gpointer purple_xfer_get_protocol_data(const PurpleXfer *xfer);

/**
 * purple_xfer_set_ui_data:
 * @xfer:			The file transfer.
 * @ui_data:		A pointer to associate with this file transfer.
 *
 * Set the UI data associated with this file transfer.
 */
void purple_xfer_set_ui_data(PurpleXfer *xfer, gpointer ui_data);

/**
 * purple_xfer_get_ui_data:
 * @xfer:			The file transfer.
 *
 * Get the UI data associated with this file transfer.
 *
 * Returns: The UI data associated with this file transfer.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_xfer_get_ui_data(const PurpleXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name File Transfer Subsystem API                                     */
/**************************************************************************/
/*@{*/

/**
 * purple_xfers_get_all:
 *
 * Returns all xfers
 *
 * Returns: all current xfers with refs
 */
GList *purple_xfers_get_all(void);

/**
 * purple_xfers_get_handle:
 *
 * Returns the handle to the file transfer subsystem
 *
 * Returns: The handle
 */
void *purple_xfers_get_handle(void);

/**
 * purple_xfers_init:
 *
 * Initializes the file transfer subsystem
 */
void purple_xfers_init(void);

/**
 * purple_xfers_uninit:
 *
 * Uninitializes the file transfer subsystem
 */
void purple_xfers_uninit(void);

/**
 * purple_xfers_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used in all purple file transfers.
 */
void purple_xfers_set_ui_ops(PurpleXferUiOps *ops);

/**
 * purple_xfers_get_ui_ops:
 *
 * Returns the UI operations structure to be used in all purple file transfers.
 *
 * Returns: The UI operations structure.
 */
PurpleXferUiOps *purple_xfers_get_ui_ops(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_XFER_H_ */

