/**
 * @file ft.h The file transfer interface
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
 */
#ifndef _GAIM_FT_H_
#define _GAIM_FT_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/
struct gaim_xfer *xfer;

/**
 * Types of file transfers.
 */
typedef enum
{
	GAIM_XFER_UNKNOWN = 0,  /**< Unknown file transfer type. */
	GAIM_XFER_SEND,         /**< File sending.               */
	GAIM_XFER_RECEIVE       /**< File receiving.             */

} GaimXferType;

/**
 * File transfer UI operations.
 *
 * Any UI representing a file transfer must assign a filled-out
 * gaim_xfer_ui_ops structure to the gaim_xfer.
 */
struct gaim_xfer_ui_ops
{
	void (*destroy)(struct gaim_xfer *xfer);
	void (*request_file)(struct gaim_xfer *xfer);
	void (*ask_cancel)(struct gaim_xfer *xfer);
	void (*add_xfer)(struct gaim_xfer *xfer);
	void (*update_progress)(struct gaim_xfer *xfer, double percent);
	void (*cancel)(struct gaim_xfer *xfer);
};

/**
 * A core representation of a file transfer.
 */
struct gaim_xfer
{
	GaimXferType type;            /**< The type of transfer.               */

	struct gaim_account *account; /**< The account.                        */

	char *who;                    /**< The person on the other end of the
	                                   transfer.                           */

	char *filename;               /**< The name of the file.               */
	char *dest_filename;          /**< The destination filename.           */
	size_t size;                  /**< The size of the file.               */

	FILE *dest_fp;                /**< The destination file pointer.       */

	char *local_ip;               /**< The local IP address.               */
	char *remote_ip;              /**< The remote IP address.              */
	int local_port;               /**< The local port.                     */
	int remote_port;              /**< The remote port.                    */

	int fd;                       /**< The socket file descriptor.         */
	int watcher;                  /**< Watcher.                            */

	size_t bytes_sent;            /**< The number of bytes sent.           */
	size_t bytes_remaining;       /**< The number of bytes remaining.      */

	gboolean completed;           /**< File Transfer is completed.         */

	/* I/O operations. */
	struct
	{
		void (*init)(struct gaim_xfer *xfer);
		void (*start)(struct gaim_xfer *xfer);
		void (*end)(struct gaim_xfer *xfer);
		void (*cancel)(struct gaim_xfer *xfer);
		size_t (*read)(char **buffer, struct gaim_xfer *xfer);
		size_t (*write)(const char *buffer, size_t size,
						struct gaim_xfer *xfer);
		void (*ack)(struct gaim_xfer *xfer, const char *buffer, 
						size_t size);

	} ops;

	struct gaim_xfer_ui_ops *ui_ops;  /**< UI-specific operations. */
	void *ui_data;                    /**< UI-specific data.       */

	void *data;                       /**< prpl-specific data.     */
};

/**************************************************************************/
/** @name File Transfer API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates a new file transfer handle.
 *
 * @param account The account sending or receiving the file.
 * @param type    The type of file transfer.
 * @param who     The name of the remote user.
 *
 * @return A file transfer handle.
 */
struct gaim_xfer *gaim_xfer_new(struct gaim_account *account,
								GaimXferType type, const char *who);

/**
 * Destroys a file transfer handle.
 *
 * @param xfer The file transfer to destroy.
 */
void gaim_xfer_destroy(struct gaim_xfer *xfer);

/**
 * Requests confirmation for a file transfer from the user.
 *
 * @param xfer The file transfer to request confirmation on.
 */
void gaim_xfer_request(struct gaim_xfer *xfer);

/**
 * Called if the user accepts the file transfer request.
 *
 * @param xfer     The file transfer.
 * @param filename The filename.
 */
void gaim_xfer_request_accepted(struct gaim_xfer *xfer, char *filename);

/**
 * Called if the user rejects the file transfer request.
 *
 * @param xfer The file transfer.
 */
void gaim_xfer_request_denied(struct gaim_xfer *xfer);

/**
 * Returns the type of file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The type of the file transfer.
 */
GaimXferType gaim_xfer_get_type(const struct gaim_xfer *xfer);

/**
 * Returns the account the file transfer is using.
 *
 * @param xfer The file transfer.
 *
 * @return The account.
 */
struct gaim_account *gaim_xfer_get_account(const struct gaim_xfer *xfer);

/**
 * Returns the completed state for a file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The completed state.
 */
gboolean gaim_xfer_is_completed(const struct gaim_xfer *xfer);

/**
 * Returns the name of the file being sent or received.
 *
 * @param xfer The file transfer.
 *
 * @return The filename.
 */
const char *gaim_xfer_get_filename(const struct gaim_xfer *xfer);

/**
 * Returns the file's destination filename,
 *
 * @param xfer The file transfer.
 *
 * @return The destination filename.
 */
const char *gaim_xfer_get_dest_filename(const struct gaim_xfer *xfer);

/**
 * Returns the number of bytes sent so far.
 *
 * @param xfer The file transfer.
 *
 * @return The number of bytes sent.
 */
size_t gaim_xfer_get_bytes_sent(const struct gaim_xfer *xfer);

/**
 * Returns the number of bytes received so far.
 *
 * @param xfer The file transfer.
 *
 * @return The number of bytes received.
 */
size_t gaim_xfer_get_bytes_remaining(const struct gaim_xfer *xfer);

/**
 * Returns the size of the file being sent or received.
 *
 * @param xfer The file transfer.
 * 
 * @return The total size of the file.
 */
size_t gaim_xfer_get_size(const struct gaim_xfer *xfer);

/**
 * Returns the current percentage of progress of the transfer.
 *
 * This is a number between 0 (0%) and 1 (100%).
 *
 * @param xfer The file transfer.
 *
 * @return The percentage complete.
 */
double gaim_xfer_get_progress(const struct gaim_xfer *xfer);

/**
 * Returns the local IP address in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The IP address on this end.
 */
const char *gaim_xfer_get_local_ip(const struct gaim_xfer *xfer);

/**
 * Returns the local port number in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The port number on this end.
 */
unsigned int gaim_xfer_get_local_port(const struct gaim_xfer *xfer);

/**
 * Returns the remote IP address in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The IP address on the other end.
 */
const char *gaim_xfer_get_remote_ip(const struct gaim_xfer *xfer);

/**
 * Returns the remote port number in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The port number on the other end.
 */
unsigned int gaim_xfer_get_remote_port(const struct gaim_xfer *xfer);

/**
 * Sets the completed state for the file transfer.
 *
 * @param xfer      The file transfer.
 * @param completed The completed state.
 */
void gaim_xfer_set_completed(struct gaim_xfer *xfer, gboolean completed);

/**
 * Sets the filename for the file transfer.
 *
 * @param xfer     The file transfer.
 * @param filename The filename.
 */
void gaim_xfer_set_filename(struct gaim_xfer *xfer, const char *filename);

/**
 * Sets the destination filename for the file transfer.
 *
 * @param xfer     The file transfer.
 * @param filename The filename
 */
void gaim_xfer_set_dest_filename(struct gaim_xfer *xfer, const char *filename);

/**
 * Sets the size of the file in a file transfer.
 *
 * @param xfer The file transfer.
 * @param size The size of the file.
 */
void gaim_xfer_set_size(struct gaim_xfer *xfer, size_t size);

/**
 * Returns the UI operations structure for a file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The UI operations structure.
 */
struct gaim_xfer_ui_ops *gaim_xfer_get_ui_ops(const struct gaim_xfer *xfer);

/**
 * Sets the read function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The read function.
 */
void gaim_xfer_set_read_fnc(struct gaim_xfer *xfer,
	size_t (*fnc)(char **, struct gaim_xfer *));

/**
 * Sets the write function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The write function.
 */
void gaim_xfer_set_write_fnc(struct gaim_xfer *xfer,
		size_t (*fnc)(const char *, size_t, struct gaim_xfer *));

/**
 * Sets the acknowledge function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The acknowledge function.
 */
void gaim_xfer_set_ack_fnc(struct gaim_xfer *xfer,
						   void (*fnc)(struct gaim_xfer *));

/**
 * Sets the transfer initialization function for the file transfer.
 *
 * This function is required, and must call gaim_xfer_start() with
 * the necessary parameters. This will be called if the file transfer
 * is accepted by the user.
 *
 * @param xfer The file transfer.
 * @param fnc  The transfer initialization function.
 */
void gaim_xfer_set_init_fnc(struct gaim_xfer *xfer,
							void (*fnc)(struct gaim_xfer *));

/**
 * Sets the start transfer function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The start transfer function.
 */
void gaim_xfer_set_start_fnc(struct gaim_xfer *xfer,
							 void (*fnc)(struct gaim_xfer *));

/**
 * Sets the end transfer function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The end transfer function.
 */
void gaim_xfer_set_end_fnc(struct gaim_xfer *xfer,
						   void (*fnc)(struct gaim_xfer *));

/**
 * Sets the cancel transfer function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The cancel transfer function.
 */
void gaim_xfer_set_cancel_fnc(struct gaim_xfer *xfer,
							  void (*fnc)(struct gaim_xfer *));

/**
 * Reads in data from a file transfer stream.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer that will be created to contain the data.
 *
 * @return The number of bytes read.
 */
size_t gaim_xfer_read(struct gaim_xfer *xfer, char **buffer);

/**
 * Writes data to a file transfer stream.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer to read the data from.
 * @param size   The number of bytes to write.
 *
 * @return The number of bytes written.
 */
size_t gaim_xfer_write(struct gaim_xfer *xfer, const char *buffer,
					   size_t size);

/**
 * Starts a file transfer.
 *
 * Either @a fd must be specified <i>or</i> @a ip and @a port on a
 * file receive transfer. On send, @a fd must be specified, and
 * @a ip and @a port are ignored.
 *
 * @param xfer The file transfer.
 * @param fd   The file descriptor for the socket.
 * @param ip   The IP address to connect to.
 * @param port The port to connect to.
 */
void gaim_xfer_start(struct gaim_xfer *xfer, int fd, const char *ip,
					 unsigned int port);

/**
 * Ends a file transfer.
 *
 * @param xfer The file transfer.
 */
void gaim_xfer_end(struct gaim_xfer *xfer);

/**
 * Cancels a file transfer.
 *
 * @param xfer The file transfer.
 */
void gaim_xfer_cancel(struct gaim_xfer *xfer);

/**
 * Displays a file transfer-related error message.
 *
 * This is a wrapper around do_error_dialog(), which automatically
 * specifies a title ("File transfer to <i>user</i> aborted" or
 * "File Transfer from <i>user</i> aborted").
 *
 * @param type The type of file transfer.
 * @param who  The user on the other end of the transfer.
 * @param msg  The message to display.
 */
void gaim_xfer_error(GaimXferType type, const char *who, const char *msg);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used in all gaim file transfers.
 *
 * @param fnc The function.
 */
void gaim_set_xfer_ui_ops(struct gaim_xfer_ui_ops *ops);

/**
 * Returns the UI operations structure to be used in all gaim file transfers.
 *
 * @return The UI operations structure.
 */
struct gaim_xfer_ui_ops *gaim_get_xfer_ui_ops(void);

/*@}*/

#endif /* _GAIM_FT_H_ */
