/**
 * @file xfer.c File Transfer API
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
 *
 */
#include "internal.h"
#include "dbus-maybe.h"
#include "enums.h"
#include "xfer.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "util.h"
#include "debug.h"

#define FT_INITIAL_BUFFER_SIZE 4096
#define FT_MAX_BUFFER_SIZE     65535

#define PURPLE_XFER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_XFER, PurpleXferPrivate))

/** @copydoc _PurpleXferPrivate */
typedef struct _PurpleXferPrivate  PurpleXferPrivate;

static PurpleXferUiOps *xfer_ui_ops = NULL;
static GList *xfers;

/** Private data for a file transfer */
struct _PurpleXferPrivate {
	PurpleXferType type;         /**< The type of transfer.               */

	PurpleAccount *account;      /**< The account.                        */

	char *who;                   /**< The person on the other end of the
	                                  transfer.                           */

	char *message;               /**< A message sent with the request     */
	char *filename;              /**< The name sent over the network.     */
	char *local_filename;        /**< The name on the local hard drive.   */
	goffset size;                /**< The size of the file.               */

	FILE *dest_fp;               /**< The destination file pointer.       */

	char *remote_ip;             /**< The remote IP address.              */
	int local_port;              /**< The local port.                     */
	int remote_port;             /**< The remote port.                    */

	int fd;                      /**< The socket file descriptor.         */
	int watcher;                 /**< Watcher.                            */

	goffset bytes_sent;          /**< The number of bytes sent.           */
	goffset bytes_remaining;     /**< The number of bytes remaining.      */
	time_t start_time;           /**< When the transfer of data began.    */
	time_t end_time;             /**< When the transfer of data ended.    */

	size_t current_buffer_size;  /**< This gradually increases for fast
	                                   network connections.               */

	PurpleXferStatus status;     /**< File Transfer's status.             */

	/** I/O operations, which should be set by the protocol using
	 *  purple_xfer_set_init_fnc() and friends.  Setting #init is
	 *  mandatory; all others are optional.
	 */
	struct
	{
		void (*init)(PurpleXfer *xfer);
		void (*request_denied)(PurpleXfer *xfer);
		void (*start)(PurpleXfer *xfer);
		void (*end)(PurpleXfer *xfer);
		void (*cancel_send)(PurpleXfer *xfer);
		void (*cancel_recv)(PurpleXfer *xfer);
		gssize (*read)(guchar **buffer, PurpleXfer *xfer);
		gssize (*write)(const guchar *buffer, size_t size, PurpleXfer *xfer);
		void (*ack)(PurpleXfer *xfer, const guchar *buffer, size_t size);
	} ops;

	PurpleXferUiOps *ui_ops;     /**< UI-specific operations.             */

	void *proto_data;            /**< Protocol-specific data.
	                                  TODO Remove this, and use
	                                       protocol-specific subclasses   */

	/*
	 * Used to moderate the file transfer when either the read/write ui_ops are
	 * set or fd is not set. In those cases, the UI/protocol call the respective
	 * function, which is somewhat akin to a fd watch being triggered.
	 */
	enum {
		PURPLE_XFER_READY_NONE = 0x0,
		PURPLE_XFER_READY_UI   = 0x1,
		PURPLE_XFER_READY_PROTOCOL = 0x2,
	} ready;

	/* TODO: Should really use a PurpleCircBuffer for this. */
	GByteArray *buffer;

	gpointer thumbnail_data;     /**< thumbnail image */
	gsize thumbnail_size;
	gchar *thumbnail_mimetype;
};

/* GObject property enums */
enum
{
	PROP_0,
	PROP_TYPE,
	PROP_ACCOUNT,
	PROP_REMOTE_USER,
	PROP_MESSAGE,
	PROP_FILENAME,
	PROP_LOCAL_FILENAME,
	PROP_FILE_SIZE,
	PROP_REMOTE_IP,
	PROP_LOCAL_PORT,
	PROP_REMOTE_PORT,
	PROP_FD,
	PROP_WATCHER,
	PROP_BYTES_SENT,
	PROP_START_TIME,
	PROP_END_TIME,
	PROP_STATUS,
	PROP_LAST
};

static GObjectClass *parent_class;

static int purple_xfer_choose_file(PurpleXfer *xfer);

static const gchar *
purple_xfer_status_type_to_string(PurpleXferStatus type)
{
	static const struct {
		PurpleXferStatus type;
		const char *name;
	} type_names[] = {
		{ PURPLE_XFER_STATUS_UNKNOWN, "unknown" },
		{ PURPLE_XFER_STATUS_NOT_STARTED, "not started" },
		{ PURPLE_XFER_STATUS_ACCEPTED, "accepted" },
		{ PURPLE_XFER_STATUS_STARTED, "started" },
		{ PURPLE_XFER_STATUS_DONE, "done" },
		{ PURPLE_XFER_STATUS_CANCEL_LOCAL, "cancelled locally" },
		{ PURPLE_XFER_STATUS_CANCEL_REMOTE, "cancelled remotely" }
	};
	gsize i;

	for (i = 0; i < G_N_ELEMENTS(type_names); ++i)
		if (type_names[i].type == type)
			return type_names[i].name;

	return "invalid state";
}

void
purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatus status)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	if (purple_debug_is_verbose())
		purple_debug_info("xfer", "Changing status of xfer %p from %s to %s\n",
				xfer, purple_xfer_status_type_to_string(priv->status),
				purple_xfer_status_type_to_string(status));

	if (priv->status == status)
		return;

	priv->status = status;

	if(priv->type == PURPLE_XFER_TYPE_SEND) {
		switch(status) {
			case PURPLE_XFER_STATUS_ACCEPTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-accept", xfer);
				break;
			case PURPLE_XFER_STATUS_STARTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-start", xfer);
				break;
			case PURPLE_XFER_STATUS_DONE:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-complete", xfer);
				break;
			case PURPLE_XFER_STATUS_CANCEL_LOCAL:
			case PURPLE_XFER_STATUS_CANCEL_REMOTE:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-cancel", xfer);
				break;
			default:
				break;
		}
	} else if(priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		switch(status) {
			case PURPLE_XFER_STATUS_ACCEPTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-accept", xfer);
				break;
			case PURPLE_XFER_STATUS_STARTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-start", xfer);
				break;
			case PURPLE_XFER_STATUS_DONE:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-complete", xfer);
				break;
			case PURPLE_XFER_STATUS_CANCEL_LOCAL:
			case PURPLE_XFER_STATUS_CANCEL_REMOTE:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-cancel", xfer);
				break;
			default:
				break;
		}
	}
}

static void
purple_xfer_conversation_write_internal(PurpleXfer *xfer,
	const char *message, gboolean is_error, gboolean print_thumbnail)
{
	PurpleIMConversation *im = NULL;
	PurpleMessageFlags flags = PURPLE_MESSAGE_SYSTEM;
	char *escaped;
	gconstpointer thumbnail_data;
	gsize size;
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(message != NULL);

	thumbnail_data = purple_xfer_get_thumbnail(xfer, &size);

	im = purple_conversations_find_im_with_account(priv->who,
											   purple_xfer_get_account(xfer));

	if (im == NULL)
		return;

	escaped = g_markup_escape_text(message, -1);

	if (is_error)
		flags |= PURPLE_MESSAGE_ERROR;

	if (print_thumbnail && thumbnail_data) {
		gchar *message_with_img;
		gpointer data = g_memdup(thumbnail_data, size);
		int id = purple_imgstore_new_with_id(data, size, NULL);

		message_with_img =
			g_strdup_printf("<img src='" PURPLE_STORED_IMAGE_PROTOCOL "%d'> %s",
			                id, escaped);
		purple_conversation_write(PURPLE_CONVERSATION(im), NULL,
			message_with_img, flags, time(NULL));
		purple_imgstore_unref_by_id(id);
		g_free(message_with_img);
	} else {
		purple_conversation_write(PURPLE_CONVERSATION(im), NULL, escaped, flags,
			time(NULL));
	}
	g_free(escaped);
}

void
purple_xfer_conversation_write(PurpleXfer *xfer, const gchar *message,
	gboolean is_error)
{
	purple_xfer_conversation_write_internal(xfer, message, is_error, FALSE);
}

/* maybe this one should be exported publically? */
static void
purple_xfer_conversation_write_with_thumbnail(PurpleXfer *xfer,
	const gchar *message)
{
	purple_xfer_conversation_write_internal(xfer, message, FALSE, TRUE);
}


static void purple_xfer_show_file_error(PurpleXfer *xfer, const char *filename)
{
	int err = errno;
	gchar *msg = NULL, *utf8;
	PurpleXferType xfer_type = purple_xfer_get_xfer_type(xfer);
	PurpleAccount *account = purple_xfer_get_account(xfer);
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	switch(xfer_type) {
		case PURPLE_XFER_TYPE_SEND:
			msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
		case PURPLE_XFER_TYPE_RECEIVE:
			msg = g_strdup_printf(_("Error writing %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
		default:
			msg = g_strdup_printf(_("Error accessing %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
	}
	g_free(utf8);

	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(xfer_type, account, priv->who, msg);
	g_free(msg);
}

static void
purple_xfer_choose_file_ok_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer;
	PurpleXferType type;
	GStatBuf st;
	gchar *dir;

	xfer = (PurpleXfer *)user_data;
	type = purple_xfer_get_xfer_type(xfer);

	if (g_stat(filename, &st) != 0) {
		/* File not found. */
		if (type == PURPLE_XFER_TYPE_RECEIVE) {
#ifndef _WIN32
			int mode = W_OK;
#else
			int mode = F_OK;
#endif
			dir = g_path_get_dirname(filename);

			if (g_access(dir, mode) == 0) {
				purple_xfer_request_accepted(xfer, filename);
			} else {
				g_object_ref(xfer);
				purple_notify_message(
					NULL, PURPLE_NOTIFY_MSG_ERROR, NULL,
					_("Directory is not writable."), NULL,
					purple_request_cpar_from_account(
						purple_xfer_get_account(xfer)),
					(PurpleNotifyCloseCallback)purple_xfer_choose_file, xfer);
			}

			g_free(dir);
		}
		else {
			purple_xfer_show_file_error(xfer, filename);
			purple_xfer_cancel_local(xfer);
		}
	}
	else if ((type == PURPLE_XFER_TYPE_SEND) && (st.st_size == 0)) {

		purple_notify_error(NULL, NULL,
			_("Cannot send a file of 0 bytes."), NULL,
			purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));

		purple_xfer_cancel_local(xfer);
	}
	else if ((type == PURPLE_XFER_TYPE_SEND) && S_ISDIR(st.st_mode)) {
		/*
		 * XXX - Sending a directory should be valid for some protocols.
		 */
		purple_notify_error(NULL, NULL, _("Cannot send a directory."),
			NULL, purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));

		purple_xfer_cancel_local(xfer);
	}
	else if ((type == PURPLE_XFER_TYPE_RECEIVE) && S_ISDIR(st.st_mode)) {
		char *msg, *utf8;
		utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
		msg = g_strdup_printf(
					_("%s is not a regular file. Cowardly refusing to overwrite it.\n"), utf8);
		g_free(utf8);
		purple_notify_error(NULL, NULL, msg, NULL,
			purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));
		g_free(msg);
		purple_xfer_request_denied(xfer);
	}
	else if (type == PURPLE_XFER_TYPE_SEND) {
#ifndef _WIN32
		int mode = R_OK;
#else
		int mode = F_OK;
#endif

		if (g_access(filename, mode) == 0) {
			purple_xfer_request_accepted(xfer, filename);
		} else {
			g_object_ref(xfer);
			purple_notify_message(
				NULL, PURPLE_NOTIFY_MSG_ERROR, NULL,
				_("File is not readable."), NULL,
				purple_request_cpar_from_account(
					purple_xfer_get_account(xfer)),
				(PurpleNotifyCloseCallback)purple_xfer_choose_file, xfer);
		}
	}
	else {
		purple_xfer_request_accepted(xfer, filename);
	}

	g_object_unref(xfer);
}

static void
purple_xfer_choose_file_cancel_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer = (PurpleXfer *)user_data;

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
		purple_xfer_cancel_local(xfer);
	else
		purple_xfer_request_denied(xfer);
	g_object_unref(xfer);
}

static int
purple_xfer_choose_file(PurpleXfer *xfer)
{
	purple_request_file(xfer, NULL, purple_xfer_get_filename(xfer),
		(purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE),
		G_CALLBACK(purple_xfer_choose_file_ok_cb),
		G_CALLBACK(purple_xfer_choose_file_cancel_cb),
		purple_request_cpar_from_account(purple_xfer_get_account(xfer)),
		xfer);

	return 0;
}

static int
cancel_recv_cb(PurpleXfer *xfer)
{
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	purple_xfer_request_denied(xfer);
	g_object_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_recv(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	char *buf, *size_buf;
	goffset size;
	gconstpointer thumb;
	gsize thumb_size;

	/* If we have already accepted the request, ask the destination file
	   name directly */
	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_ACCEPTED) {
		PurpleRequestCommonParameters *cpar;
		PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

		if (purple_xfer_get_filename(xfer) != NULL)
		{
			size = purple_xfer_get_size(xfer);
			size_buf = purple_str_size_to_units(size);
			buf = g_strdup_printf(_("%s wants to send you %s (%s)"),
						  buddy ? purple_buddy_get_alias(buddy) : priv->who,
						  purple_xfer_get_filename(xfer), size_buf);
			g_free(size_buf);
		}
		else
		{
			buf = g_strdup_printf(_("%s wants to send you a file"),
						buddy ? purple_buddy_get_alias(buddy) : priv->who);
		}

		if (priv->message != NULL)
			serv_got_im(purple_account_get_connection(priv->account),
								 priv->who, priv->message, 0, time(NULL));

		cpar = purple_request_cpar_from_account(priv->account);
		if ((thumb = purple_xfer_get_thumbnail(xfer, &thumb_size))) {
			purple_request_cpar_set_custom_icon(cpar, thumb,
				thumb_size);
		}

		purple_request_accept_cancel(xfer, NULL, buf, NULL,
			PURPLE_DEFAULT_ACTION_NONE, cpar, xfer,
			G_CALLBACK(purple_xfer_choose_file),
			G_CALLBACK(cancel_recv_cb));

		g_free(buf);
	} else
		purple_xfer_choose_file(xfer);
}

static int
ask_accept_ok(PurpleXfer *xfer)
{
	purple_xfer_request_accepted(xfer, NULL);

	return 0;
}

static int
ask_accept_cancel(PurpleXfer *xfer)
{
	purple_xfer_request_denied(xfer);
	g_object_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_accept(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	char *buf, *buf2 = NULL;
	PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

	buf = g_strdup_printf(_("Accept file transfer request from %s?"),
				  buddy ? purple_buddy_get_alias(buddy) : priv->who);
	if (purple_xfer_get_remote_ip(xfer) &&
		purple_xfer_get_remote_port(xfer))
		buf2 = g_strdup_printf(_("A file is available for download from:\n"
					 "Remote host: %s\nRemote port: %d"),
					   purple_xfer_get_remote_ip(xfer),
					   purple_xfer_get_remote_port(xfer));
	purple_request_accept_cancel(xfer, NULL, buf, buf2,
		PURPLE_DEFAULT_ACTION_NONE,
		purple_request_cpar_from_account(priv->account), xfer,
		G_CALLBACK(ask_accept_ok), G_CALLBACK(ask_accept_cancel));
	g_free(buf);
	g_free(buf2);
}

void
purple_xfer_request(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->ops.init != NULL);

	g_object_ref(xfer);

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE)
	{
		purple_signal_emit(purple_xfers_get_handle(), "file-recv-request", xfer);
		if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
		{
			/* The file-transfer was cancelled by a plugin */
			purple_xfer_cancel_local(xfer);
		}
		else if (purple_xfer_get_filename(xfer) ||
		           purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_ACCEPTED)
		{
			gchar* message = NULL;
			PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

			message = g_strdup_printf(_("%s is offering to send file %s"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who, purple_xfer_get_filename(xfer));
			purple_xfer_conversation_write_with_thumbnail(xfer, message);
			g_free(message);

			/* Ask for a filename to save to if it's not already given by a plugin */
			if (priv->local_filename == NULL)
				purple_xfer_ask_recv(xfer);
		}
		else
		{
			purple_xfer_ask_accept(xfer);
		}
	}
	else
	{
		purple_xfer_choose_file(xfer);
	}
}

void
purple_xfer_request_accepted(PurpleXfer *xfer, const char *filename)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferType type;
	GStatBuf st;
	char *msg, *utf8, *base;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	if (priv == NULL)
		return;

	type = purple_xfer_get_xfer_type(xfer);
	account = purple_xfer_get_account(xfer);

	purple_debug_misc("xfer", "request accepted for %p\n", xfer);

	if (!filename && type == PURPLE_XFER_TYPE_RECEIVE) {
		priv->status = PURPLE_XFER_STATUS_ACCEPTED;
		priv->ops.init(xfer);
		return;
	}

	buddy = purple_blist_find_buddy(account, priv->who);

	if (type == PURPLE_XFER_TYPE_SEND) {
		/* Sending a file */
		/* Check the filename. */
		PurpleXferUiOps *ui_ops;
		ui_ops = purple_xfer_get_ui_ops(xfer);

#ifdef _WIN32
		if (g_strrstr(filename, "../") || g_strrstr(filename, "..\\"))
#else
		if (g_strrstr(filename, "../"))
#endif
		{
			utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			msg = g_strdup_printf(_("%s is not a valid filename.\n"), utf8);
			purple_xfer_error(type, account, priv->who, msg);
			g_free(utf8);
			g_free(msg);

			g_object_unref(xfer);
			return;
		}

		if (ui_ops == NULL || (ui_ops->ui_read == NULL && ui_ops->ui_write == NULL)) {
			if (g_stat(filename, &st) == -1) {
				purple_xfer_show_file_error(xfer, filename);
				g_object_unref(xfer);
				return;
			}

			purple_xfer_set_local_filename(xfer, filename);
			purple_xfer_set_size(xfer, st.st_size);
		} else {
			purple_xfer_set_local_filename(xfer, filename);
		}

		base = g_path_get_basename(filename);
		utf8 = g_filename_to_utf8(base, -1, NULL, NULL, NULL);
		g_free(base);
		purple_xfer_set_filename(xfer, utf8);

		msg = g_strdup_printf(_("Offering to send %s to %s"),
				utf8, buddy ? purple_buddy_get_alias(buddy) : priv->who);
		g_free(utf8);
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}
	else {
		/* Receiving a file */
		priv->status = PURPLE_XFER_STATUS_ACCEPTED;
		purple_xfer_set_local_filename(xfer, filename);

		msg = g_strdup_printf(_("Starting transfer of %s from %s"),
				priv->filename, buddy ? purple_buddy_get_alias(buddy) : priv->who);
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	purple_xfer_add(xfer);
	priv->ops.init(xfer);

}

void
purple_xfer_request_denied(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	purple_debug_misc("xfer", "xfer %p denied\n", xfer);

	if (priv->ops.request_denied != NULL)
		priv->ops.request_denied(xfer);

	g_object_unref(xfer);
}

int purple_xfer_get_fd(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->fd;
}

int purple_xfer_get_watcher(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->watcher;
}

PurpleXferType
purple_xfer_get_xfer_type(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, PURPLE_XFER_TYPE_UNKNOWN);

	return priv->type;
}

PurpleAccount *
purple_xfer_get_account(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

void
purple_xfer_set_remote_user(PurpleXfer *xfer, const char *who)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	g_free(priv->who);
	priv->who = g_strdup(who);
}

const char *
purple_xfer_get_remote_user(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;
}

PurpleXferStatus
purple_xfer_get_status(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, PURPLE_XFER_STATUS_UNKNOWN);

	return priv->status;
}

gboolean
purple_xfer_is_cancelled(const PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), TRUE);

	if ((purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) ||
	    (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_REMOTE))
		return TRUE;
	else
		return FALSE;
}

gboolean
purple_xfer_is_completed(const PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), TRUE);

	return (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_DONE);
}

const char *
purple_xfer_get_filename(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->filename;
}

const char *
purple_xfer_get_local_filename(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->local_filename;
}

goffset
purple_xfer_get_bytes_sent(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->bytes_sent;
}

goffset
purple_xfer_get_bytes_remaining(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->bytes_remaining;
}

goffset
purple_xfer_get_size(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->size;
}

double
purple_xfer_get_progress(const PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0.0);

	if (purple_xfer_get_size(xfer) == 0)
		return 0.0;

	return ((double)purple_xfer_get_bytes_sent(xfer) /
			(double)purple_xfer_get_size(xfer));
}

unsigned int
purple_xfer_get_local_port(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->local_port;
}

const char *
purple_xfer_get_remote_ip(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->remote_ip;
}

unsigned int
purple_xfer_get_remote_port(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->remote_port;
}

time_t
purple_xfer_get_start_time(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->start_time;
}

time_t
purple_xfer_get_end_time(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->end_time;
}

void purple_xfer_set_fd(PurpleXfer *xfer, int fd)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->fd = fd;
}

void purple_xfer_set_watcher(PurpleXfer *xfer, int watcher)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->watcher = watcher;
}

void
purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(priv != NULL);

	if (completed == TRUE) {
		char *msg = NULL;
		PurpleIMConversation *im;

		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_DONE);

		if (purple_xfer_get_filename(xfer) != NULL)
		{
			char *filename = g_markup_escape_text(purple_xfer_get_filename(xfer), -1);
			if (purple_xfer_get_local_filename(xfer)
			 && purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE)
			{
				char *local = g_markup_escape_text(purple_xfer_get_local_filename(xfer), -1);
				msg = g_strdup_printf(_("Transfer of file <A HREF=\"file://%s\">%s</A> complete"),
				                      local, filename);
				g_free(local);
			}
			else
				msg = g_strdup_printf(_("Transfer of file %s complete"),
				                      filename);
			g_free(filename);
		}
		else
			msg = g_strdup(_("File transfer complete"));

		im = purple_conversations_find_im_with_account(priv->who,
		                                             purple_xfer_get_account(xfer));

		if (im != NULL)
			purple_conversation_write(PURPLE_CONVERSATION(im), NULL, msg,
					PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, purple_xfer_get_progress(xfer));
}

void
purple_xfer_set_message(PurpleXfer *xfer, const char *message)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	g_free(priv->message);
	priv->message = g_strdup(message);
}

const char *
purple_xfer_get_message(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->message;
}

void
purple_xfer_set_filename(PurpleXfer *xfer, const char *filename)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	g_free(priv->filename);
	priv->filename = g_strdup(filename);
}

void
purple_xfer_set_local_filename(PurpleXfer *xfer, const char *filename)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	g_free(priv->local_filename);
	priv->local_filename = g_strdup(filename);
}

void
purple_xfer_set_size(PurpleXfer *xfer, goffset size)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->size = size;
	priv->bytes_remaining = priv->size - purple_xfer_get_bytes_sent(xfer);
}

void
purple_xfer_set_local_port(PurpleXfer *xfer, unsigned int local_port)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->local_port = local_port;
}

void
purple_xfer_set_bytes_sent(PurpleXfer *xfer, goffset bytes_sent)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->bytes_sent = bytes_sent;
	priv->bytes_remaining = purple_xfer_get_size(xfer) - bytes_sent;
}

PurpleXferUiOps *
purple_xfer_get_ui_ops(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ui_ops;
}

void
purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.init = fnc;
}

void purple_xfer_set_request_denied_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.request_denied = fnc;
}

void
purple_xfer_set_read_fnc(PurpleXfer *xfer, gssize (*fnc)(guchar **, PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.read = fnc;
}

void
purple_xfer_set_write_fnc(PurpleXfer *xfer,
						gssize (*fnc)(const guchar *, size_t, PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.write = fnc;
}

void
purple_xfer_set_ack_fnc(PurpleXfer *xfer,
			  void (*fnc)(PurpleXfer *, const guchar *, size_t))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.ack = fnc;
}

void
purple_xfer_set_start_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.start = fnc;
}

void
purple_xfer_set_end_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.end = fnc;
}

void
purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.cancel_send = fnc;
}

void
purple_xfer_set_cancel_recv_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ops.cancel_recv = fnc;
}

static void
purple_xfer_increase_buffer_size(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	priv->current_buffer_size = MIN(priv->current_buffer_size * 1.5,
			FT_MAX_BUFFER_SIZE);
}

gssize
purple_xfer_read(PurpleXfer *xfer, guchar **buffer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	gssize s, r;

	g_return_val_if_fail(priv   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);

	if (purple_xfer_get_size(xfer) == 0)
		s = priv->current_buffer_size;
	else
		s = MIN(purple_xfer_get_bytes_remaining(xfer), priv->current_buffer_size);

	if (priv->ops.read != NULL)	{
		r = (priv->ops.read)(buffer, xfer);
	}
	else {
		*buffer = g_malloc0(s);

		r = read(priv->fd, *buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
		else if (r < 0)
			r = -1;
		else if (r == 0)
			r = -1;
	}

	if (r >= 0 && (gsize)r == priv->current_buffer_size)
		/*
		 * We managed to read the entire buffer.  This means our this
		 * network is fast and our buffer is too small, so make it
		 * bigger.
		 */
		purple_xfer_increase_buffer_size(xfer);

	return r;
}

gssize
purple_xfer_write(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	gssize r, s;

	g_return_val_if_fail(priv   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);
	g_return_val_if_fail(size   != 0,    0);

	s = MIN(purple_xfer_get_bytes_remaining(xfer), size);

	if (priv->ops.write != NULL) {
		r = (priv->ops.write)(buffer, s, xfer);
	} else {
		r = write(priv->fd, buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
	}
	if (r >= 0 && (purple_xfer_get_bytes_sent(xfer)+r) >= purple_xfer_get_size(xfer) &&
		!purple_xfer_is_completed(xfer))
		purple_xfer_set_completed(xfer, TRUE);
	

	return r;
}

gboolean
purple_xfer_write_file(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;
	gsize wc;
	gboolean fs_known;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(buffer != NULL, FALSE);

	ui_ops = purple_xfer_get_ui_ops(xfer);
	fs_known = (purple_xfer_get_size(xfer) > 0);

	if (fs_known && size > purple_xfer_get_bytes_remaining(xfer)) {
		purple_debug_warning("filetransfer",
			"Got too much data (truncating at %" G_GOFFSET_FORMAT
			").\n", purple_xfer_get_size(xfer));
		size = purple_xfer_get_bytes_remaining(xfer);
	}

	if (ui_ops && ui_ops->ui_write)
		wc = ui_ops->ui_write(xfer, buffer, size);
	else {
		if (priv->dest_fp == NULL) {
			purple_debug_error("filetransfer",
				"File is not opened for writing\n");
			purple_xfer_cancel_local(xfer);
			return FALSE;
		}
		wc = fwrite(buffer, size, 1, priv->dest_fp);
	}

	if (wc != size) {
		purple_debug_error("filetransfer",
			"Unable to write whole buffer.\n");
		purple_xfer_cancel_local(xfer);
		return FALSE;
	}

	purple_xfer_set_bytes_sent(xfer, purple_xfer_get_bytes_sent(xfer) +
		size);

	return TRUE;
}

gssize
purple_xfer_read_file(PurpleXfer *xfer, guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;
	gssize got_len;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(buffer != NULL, FALSE);

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops && ui_ops->ui_read) {
		guchar *buffer_got = NULL;

		got_len = ui_ops->ui_read(xfer, &buffer_got, size);

		if (got_len >= 0 && (gsize)got_len > size) {
			g_free(buffer_got);
			purple_debug_error("filetransfer",
				"Got too much data from UI.\n");
			purple_xfer_cancel_local(xfer);
			return -1;
		}

		if (got_len > 0)
			memcpy(buffer, buffer_got, got_len);
		g_free(buffer_got);
	} else {
		if (priv->dest_fp == NULL) {
			purple_debug_error("filetransfer",
				"File is not opened for reading\n");
			purple_xfer_cancel_local(xfer);
			return -1;
		}
		got_len = fread(buffer, size, 1, priv->dest_fp);
		if ((got_len < 0 || (gsize)got_len != size) &&
			ferror(priv->dest_fp))
		{
			purple_debug_error("filetransfer",
				"Unable to read file.\n");
			purple_xfer_cancel_local(xfer);
			return -1;
		}
	}

	if (got_len > 0) {
		purple_xfer_set_bytes_sent(xfer,
			purple_xfer_get_bytes_sent(xfer) + got_len);
	}

	return got_len;
}

static void
do_transfer(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;
	guchar *buffer = NULL;
	gssize r = 0;

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		r = purple_xfer_read(xfer, &buffer);
		if (r > 0) {
			if (!purple_xfer_write_file(xfer, buffer, r)) {
				g_free(buffer);
				return;
			}

			if ((purple_xfer_get_size(xfer) > 0) &&
				((purple_xfer_get_bytes_sent(xfer)+r) >= purple_xfer_get_size(xfer)))
				purple_xfer_set_completed(xfer, TRUE);
		} else if(r < 0) {
			purple_xfer_cancel_remote(xfer);
			g_free(buffer);
			return;
		}
	} else if (priv->type == PURPLE_XFER_TYPE_SEND) {
		gssize result = 0;
		size_t s = MIN(purple_xfer_get_bytes_remaining(xfer), priv->current_buffer_size);
		gboolean read = TRUE;

		/* this is so the protocol can keep the connection open
		   if it needs to for some odd reason. */
		if (s == 0) {
			if (priv->watcher) {
				purple_input_remove(priv->watcher);
				priv->watcher = 0;
			}
			return;
		}

		if (priv->buffer) {
			if (priv->buffer->len < s) {
				s -= priv->buffer->len;
				read = TRUE;
			} else {
				read = FALSE;
			}
		}

		if (read) {
			buffer = g_new(guchar, s);
			result = purple_xfer_read_file(xfer, buffer, s);
			if (result == 0) {
				/*
				 * The UI claimed it was ready, but didn't have any data for
				 * us...  It will call purple_xfer_ui_ready when ready, which
				 * sets back up this watcher.
				 */
				if (priv->watcher != 0) {
					purple_input_remove(priv->watcher);
					priv->watcher = 0;
				}

				/* Need to indicate the protocol is still ready... */
				priv->ready |= PURPLE_XFER_READY_PROTOCOL;

				g_return_if_reached();
			}
			if (result < 0)
				return;
		}

		if (priv->buffer) {
			g_byte_array_append(priv->buffer, buffer, result);
			g_free(buffer);
			buffer = priv->buffer->data;
			result = priv->buffer->len;
		}

		r = purple_xfer_write(xfer, buffer, result);

		if (r == -1) {
			purple_xfer_cancel_remote(xfer);
			if (!priv->buffer)
				/* We don't free buffer if priv->buffer is set, because in
				   that case buffer doesn't belong to us. */
				g_free(buffer);
			return;
		} else if (r == result) {
			/*
			 * We managed to write the entire buffer.  This means our
			 * network is fast and our buffer is too small, so make it
			 * bigger.
			 */
			purple_xfer_increase_buffer_size(xfer);
		} else {
			if (ui_ops && ui_ops->data_not_sent)
				ui_ops->data_not_sent(xfer, buffer + r, result - r);
		}

		if (priv->buffer) {
			/*
			 * Remove what we wrote
			 * If we wrote the whole buffer the byte array will be empty
			 * Otherwise we'll keep what wasn't sent for next time.
			 */
			buffer = NULL;
			g_byte_array_remove_range(priv->buffer, 0, r);
		}
	}

	if (r > 0) {
		if (purple_xfer_get_size(xfer) > 0)
			priv->bytes_remaining -= r;

		priv->bytes_sent += r;

		if (priv->ops.ack != NULL)
			priv->ops.ack(xfer, buffer, r);

		g_free(buffer);

		if (ui_ops != NULL && ui_ops->update_progress != NULL)
			ui_ops->update_progress(xfer,
				purple_xfer_get_progress(xfer));
	}

	if (purple_xfer_is_completed(xfer))
		purple_xfer_end(xfer);
}

static void
transfer_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer = data;
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	if (priv->dest_fp == NULL) {
		/* The UI is moderating its side manually */
		if (0 == (priv->ready & PURPLE_XFER_READY_UI)) {
			priv->ready |= PURPLE_XFER_READY_PROTOCOL;

			purple_input_remove(priv->watcher);
			priv->watcher = 0;

			purple_debug_misc("xfer", "Protocol is ready on ft %p, waiting for UI\n", xfer);
			return;
		}

		priv->ready = PURPLE_XFER_READY_NONE;
	}

	do_transfer(xfer);
}

static void
begin_transfer(PurpleXfer *xfer, PurpleInputCondition cond)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferType type = purple_xfer_get_xfer_type(xfer);
	PurpleXferUiOps *ui_ops = purple_xfer_get_ui_ops(xfer);

	if (priv->start_time != 0) {
		purple_debug_error("xfer", "Transfer is being started multiple times\n");
		g_return_if_reached();
	}

	if (ui_ops == NULL || (ui_ops->ui_read == NULL && ui_ops->ui_write == NULL)) {
		priv->dest_fp = g_fopen(purple_xfer_get_local_filename(xfer),
		                        type == PURPLE_XFER_TYPE_RECEIVE ? "wb" : "rb");

		if (priv->dest_fp == NULL) {
			purple_xfer_show_file_error(xfer, purple_xfer_get_local_filename(xfer));
			purple_xfer_cancel_local(xfer);
			return;
		}

		fseek(priv->dest_fp, priv->bytes_sent, SEEK_SET);
	}

	if (priv->fd != -1)
		priv->watcher = purple_input_add(priv->fd, cond, transfer_cb, xfer);

	priv->start_time = time(NULL);

	if (priv->ops.start != NULL)
		priv->ops.start(xfer);
}

static void
connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = (PurpleXfer *)data;
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	if (source < 0) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	priv->fd = source;

	begin_transfer(xfer, PURPLE_INPUT_READ);
}

void
purple_xfer_ui_ready(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleInputCondition cond;
	PurpleXferType type;

	g_return_if_fail(priv != NULL);

	priv->ready |= PURPLE_XFER_READY_UI;

	if (0 == (priv->ready & PURPLE_XFER_READY_PROTOCOL)) {
		purple_debug_misc("xfer", "UI is ready on ft %p, waiting for protocol\n", xfer);
		return;
	}

	purple_debug_misc("xfer", "UI (and protocol) ready on ft %p, so proceeding\n", xfer);

	type = purple_xfer_get_xfer_type(xfer);
	if (type == PURPLE_XFER_TYPE_SEND)
		cond = PURPLE_INPUT_WRITE;
	else /* if (type == PURPLE_XFER_TYPE_RECEIVE) */
		cond = PURPLE_INPUT_READ;

	if (priv->watcher == 0 && priv->fd != -1)
		priv->watcher = purple_input_add(priv->fd, cond, transfer_cb, xfer);

	priv->ready = PURPLE_XFER_READY_NONE;

	do_transfer(xfer);
}

void
purple_xfer_protocol_ready(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->ready |= PURPLE_XFER_READY_PROTOCOL;

	/* I don't think fwrite/fread are ever *not* ready */
	if (priv->dest_fp == NULL && 0 == (priv->ready & PURPLE_XFER_READY_UI)) {
		purple_debug_misc("xfer", "Protocol is ready on ft %p, waiting for UI\n", xfer);
		return;
	}

	purple_debug_misc("xfer", "Protocol (and UI) ready on ft %p, so proceeding\n", xfer);

	priv->ready = PURPLE_XFER_READY_NONE;

	do_transfer(xfer);
}

void
purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip,
				unsigned int port)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleInputCondition cond;
	PurpleXferType type;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(purple_xfer_get_xfer_type(xfer) != PURPLE_XFER_TYPE_UNKNOWN);

	type = purple_xfer_get_xfer_type(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_STARTED);

	if (type == PURPLE_XFER_TYPE_RECEIVE) {
		cond = PURPLE_INPUT_READ;

		if (ip != NULL) {
			priv->remote_ip   = g_strdup(ip);
			priv->remote_port = port;

			/* Establish a file descriptor. */
			purple_proxy_connect(NULL, priv->account, priv->remote_ip,
							   priv->remote_port, connect_cb, xfer);

			return;
		}
		else {
			priv->fd = fd;
		}
	}
	else {
		cond = PURPLE_INPUT_WRITE;

		priv->fd = fd;
	}

	begin_transfer(xfer, cond);
}

void
purple_xfer_end(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	/* See if we are actually trying to cancel this. */
	if (!purple_xfer_is_completed(xfer)) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	priv->end_time = time(NULL);
	if (priv->ops.end != NULL)
		priv->ops.end(xfer);

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		priv->watcher = 0;
	}

	if (priv->fd != -1)
		close(priv->fd);

	if (priv->dest_fp != NULL) {
		fclose(priv->dest_fp);
		priv->dest_fp = NULL;
	}

	g_object_unref(xfer);
}

void
purple_xfer_add(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->add_xfer != NULL)
		ui_ops->add_xfer(xfer);
}

void
purple_xfer_cancel_local(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;
	char *msg = NULL;

	g_return_if_fail(priv != NULL);

	/* TODO: We definitely want to close any open request dialogs associated
	   with this transfer.  However, in some cases the request dialog might
	   own a reference on the xfer.  This happens at least with the "%s wants
	   to send you %s" dialog from purple_xfer_ask_recv().  In these cases
	   the ref count will not be decremented when the request dialog is
	   closed, so the ref count will never reach 0 and the xfer will never
	   be freed.  This is a memleak and should be fixed.  It's not clear what
	   the correct fix is.  Probably requests should have a destroy function
	   that is called when the request is destroyed.  But also, ref counting
	   xfer objects makes this code REALLY complicated.  An alternate fix is
	   to not ref count and instead just make sure the object still exists
	   when we try to use it. */
	purple_request_close_with_handle(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	priv->end_time = time(NULL);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("You cancelled the transfer of %s"),
							  purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup(_("File transfer cancelled"));
	}
	purple_xfer_conversation_write(xfer, msg, FALSE);
	g_free(msg);

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
	{
		if (priv->ops.cancel_send != NULL)
			priv->ops.cancel_send(xfer);
	}
	else
	{
		if (priv->ops.cancel_recv != NULL)
			priv->ops.cancel_recv(xfer);
	}

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		priv->watcher = 0;
	}

	if (priv->fd != -1)
		close(priv->fd);

	if (priv->dest_fp != NULL) {
		fclose(priv->dest_fp);
		priv->dest_fp = NULL;
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_local != NULL)
		ui_ops->cancel_local(xfer);

	priv->bytes_remaining = 0;

	g_object_unref(xfer);
}

void
purple_xfer_cancel_remote(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;
	gchar *msg;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	g_return_if_fail(priv != NULL);

	purple_request_close_with_handle(xfer);
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
	priv->end_time = time(NULL);

	account = purple_xfer_get_account(xfer);
	buddy = purple_blist_find_buddy(account, priv->who);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("%s cancelled the transfer of %s"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who, purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("%s cancelled the file transfer"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who);
	}
	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(purple_xfer_get_xfer_type(xfer), account, priv->who, msg);
	g_free(msg);

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
	{
		if (priv->ops.cancel_send != NULL)
			priv->ops.cancel_send(xfer);
	}
	else
	{
		if (priv->ops.cancel_recv != NULL)
			priv->ops.cancel_recv(xfer);
	}

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		priv->watcher = 0;
	}

	if (priv->fd != -1)
		close(priv->fd);

	if (priv->dest_fp != NULL) {
		fclose(priv->dest_fp);
		priv->dest_fp = NULL;
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_remote != NULL)
		ui_ops->cancel_remote(xfer);

	priv->bytes_remaining = 0;

	g_object_unref(xfer);
}

void
purple_xfer_error(PurpleXferType type, PurpleAccount *account, const char *who, const char *msg)
{
	char *title;

	g_return_if_fail(msg  != NULL);
	g_return_if_fail(type != PURPLE_XFER_TYPE_UNKNOWN);

	if (account) {
		PurpleBuddy *buddy;
		buddy = purple_blist_find_buddy(account, who);
		if (buddy)
			who = purple_buddy_get_alias(buddy);
	}

	if (type == PURPLE_XFER_TYPE_SEND)
		title = g_strdup_printf(_("File transfer to %s failed."), who);
	else
		title = g_strdup_printf(_("File transfer from %s failed."), who);

	purple_notify_error(NULL, NULL, title, msg,
		purple_request_cpar_from_account(account));

	g_free(title);
}

void
purple_xfer_update_progress(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	ui_ops = purple_xfer_get_ui_ops(xfer);
	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, purple_xfer_get_progress(xfer));
}

gconstpointer
purple_xfer_get_thumbnail(const PurpleXfer *xfer, gsize *len)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	if (len)
		*len = priv->thumbnail_size;

	return priv->thumbnail_data;
}

const gchar *
purple_xfer_get_thumbnail_mimetype(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->thumbnail_mimetype;
}

void
purple_xfer_set_thumbnail(PurpleXfer *xfer, gconstpointer thumbnail,
	gsize size, const gchar *mimetype)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	g_free(priv->thumbnail_data);
	g_free(priv->thumbnail_mimetype);

	if (thumbnail && size > 0) {
		priv->thumbnail_data = g_memdup(thumbnail, size);
		priv->thumbnail_size = size;
		priv->thumbnail_mimetype = g_strdup(mimetype);
	} else {
		priv->thumbnail_data = NULL;
		priv->thumbnail_size = 0;
		priv->thumbnail_mimetype = NULL;
	}
}

void
purple_xfer_prepare_thumbnail(PurpleXfer *xfer, const gchar *formats)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	if (priv->ui_ops->add_thumbnail) {
		priv->ui_ops->add_thumbnail(xfer, formats);
	}
}

void
purple_xfer_set_protocol_data(PurpleXfer *xfer, gpointer proto_data)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_if_fail(priv != NULL);

	priv->proto_data = proto_data;
}

gpointer
purple_xfer_get_protocol_data(const PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void purple_xfer_set_ui_data(PurpleXfer *xfer, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_XFER(xfer));

	xfer->ui_data = ui_data;
}

gpointer purple_xfer_get_ui_data(const PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	return xfer->ui_data;
}

/**************************************************************************
 * GObject code
 **************************************************************************/
/* GObject Property names */
#define PROP_TYPE_S            "type"
#define PROP_ACCOUNT_S         "account"
#define PROP_REMOTE_USER_S     "remote-user"
#define PROP_MESSAGE_S         "message"
#define PROP_FILENAME_S        "filename"
#define PROP_LOCAL_FILENAME_S  "local-filename"
#define PROP_FILE_SIZE_S       "file-size"
#define PROP_REMOTE_IP_S       "remote-ip"
#define PROP_LOCAL_PORT_S      "local-port"
#define PROP_REMOTE_PORT_S     "remote-port"
#define PROP_FD_S              "fd"
#define PROP_WATCHER_S         "watcher"
#define PROP_BYTES_SENT_S      "bytes-sent"
#define PROP_START_TIME_S      "start-time"
#define PROP_END_TIME_S        "end-time"
#define PROP_STATUS_S          "status"

/* Set method for GObject properties */
static void
purple_xfer_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleXfer *xfer = PURPLE_XFER(obj);
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	switch (param_id) {
		case PROP_TYPE:
			priv->type = g_value_get_enum(value);
			break;
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_REMOTE_USER:
			purple_xfer_set_remote_user(xfer, g_value_get_string(value));
			break;
		case PROP_MESSAGE:
			purple_xfer_set_message(xfer, g_value_get_string(value));
			break;
		case PROP_FILENAME:
			purple_xfer_set_filename(xfer, g_value_get_string(value));
			break;
		case PROP_LOCAL_FILENAME:
			purple_xfer_set_local_filename(xfer, g_value_get_string(value));
			break;
		case PROP_FILE_SIZE:
			purple_xfer_set_size(xfer, g_value_get_int64(value));
			break;
		case PROP_LOCAL_PORT:
			purple_xfer_set_local_port(xfer, g_value_get_int(value));
			break;
		case PROP_FD:
			purple_xfer_set_fd(xfer, g_value_get_int(value));
			break;
		case PROP_WATCHER:
			purple_xfer_set_watcher(xfer, g_value_get_int(value));
			break;
		case PROP_BYTES_SENT:
			purple_xfer_set_bytes_sent(xfer, g_value_get_int64(value));
			break;
		case PROP_STATUS:
			purple_xfer_set_status(xfer, g_value_get_enum(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_xfer_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleXfer *xfer = PURPLE_XFER(obj);

	switch (param_id) {
		case PROP_TYPE:
			g_value_set_enum(value, purple_xfer_get_xfer_type(xfer));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_xfer_get_account(xfer));
			break;
		case PROP_REMOTE_USER:
			g_value_set_string(value, purple_xfer_get_remote_user(xfer));
			break;
		case PROP_MESSAGE:
			g_value_set_string(value, purple_xfer_get_message(xfer));
			break;
		case PROP_FILENAME:
			g_value_set_string(value, purple_xfer_get_filename(xfer));
			break;
		case PROP_LOCAL_FILENAME:
			g_value_set_string(value, purple_xfer_get_local_filename(xfer));
			break;
		case PROP_FILE_SIZE:
			g_value_set_int64(value, purple_xfer_get_size(xfer));
			break;
		case PROP_REMOTE_IP:
			g_value_set_string(value, purple_xfer_get_remote_ip(xfer));
			break;
		case PROP_LOCAL_PORT:
			g_value_set_int(value, purple_xfer_get_local_port(xfer));
			break;
		case PROP_REMOTE_PORT:
			g_value_set_int(value, purple_xfer_get_remote_port(xfer));
			break;
		case PROP_FD:
			g_value_set_int(value, purple_xfer_get_fd(xfer));
			break;
		case PROP_WATCHER:
			g_value_set_int(value, purple_xfer_get_watcher(xfer));
			break;
		case PROP_BYTES_SENT:
			g_value_set_int64(value, purple_xfer_get_bytes_sent(xfer));
			break;
		case PROP_START_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_xfer_get_start_time(xfer));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_xfer_get_start_time(xfer));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_END_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_xfer_get_end_time(xfer));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_xfer_get_end_time(xfer));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_STATUS:
			g_value_set_enum(value, purple_xfer_get_status(xfer));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_xfer_init(GTypeInstance *instance, gpointer klass)
{
	PurpleXfer *xfer = PURPLE_XFER(instance);
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);

	PURPLE_DBUS_REGISTER_POINTER(xfer, PurpleXfer);

	priv->ui_ops = purple_xfers_get_ui_ops();
	priv->message = NULL;
	priv->current_buffer_size = FT_INITIAL_BUFFER_SIZE;
	priv->fd = -1;
	priv->ready = PURPLE_XFER_READY_NONE;
}

/* Called when done constructing */
static void
purple_xfer_constructed(GObject *object)
{
	PurpleXfer *xfer = PURPLE_XFER(object);
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(xfer);
	PurpleXferUiOps *ui_ops;

	parent_class->constructed(object);

	ui_ops = purple_xfers_get_ui_ops();

	if (ui_ops && ui_ops->data_not_sent) {
		/* If the ui will handle unsent data no need for buffer */
		priv->buffer = NULL;
	} else {
		priv->buffer = g_byte_array_sized_new(FT_INITIAL_BUFFER_SIZE);
	}

	if (ui_ops != NULL && ui_ops->new_xfer != NULL)
		ui_ops->new_xfer(xfer);

	xfers = g_list_prepend(xfers, xfer);
}

/* GObject dispose function */
static void
purple_xfer_dispose(GObject *object)
{
	PurpleXfer *xfer = PURPLE_XFER(object);
	PurpleXferUiOps *ui_ops;

	/* Close the file browser, if it's open */
	purple_request_close_with_handle(xfer);

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_STARTED)
		purple_xfer_cancel_local(xfer);

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->destroy != NULL)
		ui_ops->destroy(xfer);

	xfers = g_list_remove(xfers, xfer);

	PURPLE_DBUS_UNREGISTER_POINTER(xfer);

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_xfer_finalize(GObject *object)
{
	PurpleXferPrivate *priv = PURPLE_XFER_GET_PRIVATE(object);

	g_free(priv->who);
	g_free(priv->filename);
	g_free(priv->remote_ip);
	g_free(priv->local_filename);

	if (priv->buffer)
		g_byte_array_free(priv->buffer, TRUE);

	g_free(priv->thumbnail_data);
	g_free(priv->thumbnail_mimetype);

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_xfer_class_init(PurpleXferClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_xfer_dispose;
	obj_class->finalize = purple_xfer_finalize;
	obj_class->constructed = purple_xfer_constructed;

	/* Setup properties */
	obj_class->get_property = purple_xfer_get_property;
	obj_class->set_property = purple_xfer_set_property;

	g_object_class_install_property(obj_class, PROP_TYPE,
			g_param_spec_enum(PROP_TYPE_S, _("Transfer type"),
				_("The type of file transfer."), PURPLE_TYPE_XFER_TYPE,
				PURPLE_XFER_TYPE_UNKNOWN,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_object_class_install_property(obj_class, PROP_ACCOUNT,
			g_param_spec_object(PROP_ACCOUNT_S, _("Account"),
				_("The account sending or receiving the file."),
				PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_object_class_install_property(obj_class, PROP_REMOTE_USER,
			g_param_spec_string(PROP_REMOTE_USER_S, _("Remote user"),
				_("The name of the remote user."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, PROP_MESSAGE,
			g_param_spec_string(PROP_MESSAGE_S, _("Message"),
				_("The message for the file transfer."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_FILENAME,
			g_param_spec_string(PROP_FILENAME_S, _("Filename"),
				_("The filename for the file transfer."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_LOCAL_FILENAME,
			g_param_spec_string(PROP_LOCAL_FILENAME_S, _("Local filename"),
				_("The local filename for the file transfer."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_FILE_SIZE,
			g_param_spec_int64(PROP_FILE_SIZE_S, _("File size"),
				_("Size of the file in a file transfer."),
				G_MININT64, G_MAXINT64, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_REMOTE_IP,
			g_param_spec_string(PROP_REMOTE_IP_S, _("Remote IP"),
				_("The remote IP address in the file transfer."), NULL,
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, PROP_LOCAL_PORT,
			g_param_spec_int(PROP_LOCAL_PORT_S, _("Local port"),
				_("The local port number in the file transfer."),
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_REMOTE_PORT,
			g_param_spec_int(PROP_REMOTE_PORT_S, _("Remote port"),
				_("The remote port number in the file transfer."),
				G_MININT, G_MAXINT, 0,
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, PROP_FD,
			g_param_spec_int(PROP_FD_S, _("Socket FD"),
				_("The socket file descriptor."),
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_WATCHER,
			g_param_spec_int(PROP_WATCHER_S, _("Watcher"),
				_("The watcher for the file transfer."),
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_BYTES_SENT,
			g_param_spec_int64(PROP_BYTES_SENT_S, _("Bytes sent"),
				_("The number of bytes sent (or received) so far."),
				G_MININT64, G_MAXINT64, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_START_TIME,
#if SIZEOF_TIME_T == 4
			g_param_spec_int
#elif SIZEOF_TIME_T == 8
			g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				(PROP_START_TIME_S, _("Start time"),
				_("The time the transfer of a file started."),
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, PROP_END_TIME,
#if SIZEOF_TIME_T == 4
			g_param_spec_int
#elif SIZEOF_TIME_T == 8
			g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				(PROP_END_TIME_S, _("End time"),
				_("The time the transfer of a file ended."),
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, PROP_STATUS,
			g_param_spec_enum(PROP_STATUS_S, _("Status"),
				_("The current status for the file transfer."),
				PURPLE_TYPE_XFER_STATUS, PURPLE_XFER_STATUS_UNKNOWN,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleXferPrivate));
}

GType
purple_xfer_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleXferClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_xfer_class_init,
			NULL,
			NULL,
			sizeof(PurpleXfer),
			0,
			(GInstanceInitFunc)purple_xfer_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleXfer",
				&info, 0);
	}

	return type;
}

PurpleXfer *
purple_xfer_new(PurpleAccount *account, PurpleXferType type, const char *who)
{
	PurpleXfer *xfer;
	PurpleProtocol *protocol;

	g_return_val_if_fail(type != PURPLE_XFER_TYPE_UNKNOWN, NULL);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(who != NULL, NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));

	g_return_val_if_fail(protocol != NULL, NULL);

	if (PURPLE_PROTOCOL_IMPLEMENTS(protocol, FACTORY_IFACE, xfer_new))
		xfer = purple_protocol_factory_iface_xfer_new(protocol, account, type,
				who);
	else
		xfer = g_object_new(PURPLE_TYPE_XFER,
			PROP_ACCOUNT_S,     account,
			PROP_TYPE_S,        type,
			PROP_REMOTE_USER_S, who,
			NULL
		);

	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer;
}

/**************************************************************************
 * File Transfer Subsystem API
 **************************************************************************/
GList *
purple_xfers_get_all()
{
	return xfers;
}

void *
purple_xfers_get_handle(void) {
	static int handle = 0;

	return &handle;
}

void
purple_xfers_init(void) {
	void *handle = purple_xfers_get_handle();

	/* register signals */
	purple_signal_register(handle, "file-recv-accept",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-send-accept",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-recv-start",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-send-start",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-send-cancel",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-recv-cancel",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-send-complete",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-recv-complete",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
	purple_signal_register(handle, "file-recv-request",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
}

void
purple_xfers_uninit(void)
{
	void *handle = purple_xfers_get_handle();

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);
}

void
purple_xfers_set_ui_ops(PurpleXferUiOps *ops) {
	xfer_ui_ops = ops;
}

PurpleXferUiOps *
purple_xfers_get_ui_ops(void) {
	return xfer_ui_ops;
}
