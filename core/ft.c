/**
 * @file ft.c File Transfer API
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
 */
#include "internal.h"
#include "ft.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "util.h"

static GaimXferUiOps *xfer_ui_ops = NULL;

GaimXfer *
gaim_xfer_new(GaimAccount *account, GaimXferType type, const char *who)
{
	GaimXfer *xfer;
	GaimXferUiOps *ui_ops;

	g_return_val_if_fail(type    != GAIM_XFER_UNKNOWN, NULL);
	g_return_val_if_fail(account != NULL,              NULL);
	g_return_val_if_fail(who     != NULL,              NULL);

	xfer = g_new0(GaimXfer, 1);

	xfer->ref = 1;
	xfer->type    = type;
	xfer->account = account;
	xfer->who     = g_strdup(who);
	xfer->ui_ops  = gaim_xfers_get_ui_ops();
	xfer->message = NULL;

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->new_xfer != NULL)
		ui_ops->new_xfer(xfer);

	return xfer;
}

static void
gaim_xfer_destroy(GaimXfer *xfer)
{
	GaimXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	/* Close the file browser, if it's open */
	gaim_request_close_with_handle(xfer);

	if (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_STARTED)
		gaim_xfer_cancel_local(xfer);

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->destroy != NULL)
		ui_ops->destroy(xfer);

	g_free(xfer->who);
	g_free(xfer->filename);
	g_free(xfer->remote_ip);
	g_free(xfer->local_filename);

	g_free(xfer);
}

void
gaim_xfer_ref(GaimXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	xfer->ref++;
}

void
gaim_xfer_unref(GaimXfer *xfer)
{
	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->ref > 0);

	xfer->ref--;

	if (xfer->ref == 0)
		gaim_xfer_destroy(xfer);
}

static void
gaim_xfer_set_status(GaimXfer *xfer, GaimXferStatusType status)
{
	g_return_if_fail(xfer != NULL);

	if(xfer->type == GAIM_XFER_SEND) {
		switch(status) {
			case GAIM_XFER_STATUS_ACCEPTED:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-send-accept", xfer);
				break;
			case GAIM_XFER_STATUS_STARTED:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-send-start", xfer);
				break;
			case GAIM_XFER_STATUS_DONE:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-send-complete", xfer);
				break;
			case GAIM_XFER_STATUS_CANCEL_LOCAL:
			case GAIM_XFER_STATUS_CANCEL_REMOTE:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-send-cancel", xfer);
				break;
			default:
				break;
		}
	} else if(xfer->type == GAIM_XFER_RECEIVE) {
		switch(status) {
			case GAIM_XFER_STATUS_ACCEPTED:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-recv-accept", xfer);
				break;
			case GAIM_XFER_STATUS_STARTED:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-recv-start", xfer);
				break;
			case GAIM_XFER_STATUS_DONE:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-recv-complete", xfer);
				break;
			case GAIM_XFER_STATUS_CANCEL_LOCAL:
			case GAIM_XFER_STATUS_CANCEL_REMOTE:
				gaim_signal_emit(gaim_xfers_get_handle(), "file-recv-cancel", xfer);
				break;
			default:
				break;
		}
	}

	xfer->status = status;
}

void gaim_xfer_conversation_write(GaimXfer *xfer, char *message, gboolean is_error)
{
	GaimConversation *conv = NULL;
	GaimMessageFlags flags = GAIM_MESSAGE_SYSTEM;
	char *escaped;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(message != NULL);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, xfer->who,
											   gaim_xfer_get_account(xfer));

	if (conv == NULL)
		return;

	escaped = g_markup_escape_text(message, -1);

	if (is_error)
		flags = GAIM_MESSAGE_ERROR;

	gaim_conversation_write(conv, NULL, escaped, flags, time(NULL));
	g_free(escaped);
}

static void gaim_xfer_show_file_error(GaimXfer *xfer, const char *filename)
{
	int err = errno;
	gchar *msg = NULL, *utf8;
	GaimXferType xfer_type = gaim_xfer_get_type(xfer);
	GaimAccount *account = gaim_xfer_get_account(xfer);

	utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	switch(xfer_type) {
		case GAIM_XFER_SEND:
			msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
		case GAIM_XFER_RECEIVE:
			msg = g_strdup_printf(_("Error writing %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
		default:
			msg = g_strdup_printf(_("Error accessing %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
	}
	g_free(utf8);

	gaim_xfer_conversation_write(xfer, msg, TRUE);
	gaim_xfer_error(xfer_type, account, xfer->who, msg);
	g_free(msg);
}

static void
gaim_xfer_choose_file_ok_cb(void *user_data, const char *filename)
{
	GaimXfer *xfer;
	struct stat st;

	xfer = (GaimXfer *)user_data;

	if (g_stat(filename, &st) != 0) {
		/* File not found. */
		if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
			gaim_xfer_request_accepted(xfer, filename);
		}
		else {
			gaim_xfer_show_file_error(xfer, filename);
			gaim_xfer_request_denied(xfer);
		}
	}
	else if ((gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) &&
			 (st.st_size == 0)) {

		gaim_notify_error(NULL, NULL,
						  _("Cannot send a file of 0 bytes."), NULL);

		gaim_xfer_request_denied(xfer);
	}
	else if ((gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) &&
			 S_ISDIR(st.st_mode)) {
		/*
		 * XXX - Sending a directory should be valid for some protocols.
		 */
		gaim_notify_error(NULL, NULL,
						  _("Cannot send a directory."), NULL);

		gaim_xfer_request_denied(xfer);
	}
	else if ((gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) &&
			 S_ISDIR(st.st_mode)) {
		char *msg, *utf8;
		utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
		msg = g_strdup_printf(
					_("%s is not a regular file. Cowardly refusing to overwrite it.\n"), utf8);
		g_free(utf8);
		gaim_notify_error(NULL, NULL, msg, NULL);
		g_free(msg);
		gaim_xfer_request_denied(xfer);
	}
	else {
		gaim_xfer_request_accepted(xfer, filename);
	}

	gaim_xfer_unref(xfer);
}

static void
gaim_xfer_choose_file_cancel_cb(void *user_data, const char *filename)
{
	GaimXfer *xfer = (GaimXfer *)user_data;

	gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_CANCEL_LOCAL);
	gaim_xfer_request_denied(xfer);
}

static int
gaim_xfer_choose_file(GaimXfer *xfer)
{
	gaim_request_file(xfer, NULL, gaim_xfer_get_filename(xfer),
					  (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE),
					  G_CALLBACK(gaim_xfer_choose_file_ok_cb),
					  G_CALLBACK(gaim_xfer_choose_file_cancel_cb), xfer);

	return 0;
}

static int
cancel_recv_cb(GaimXfer *xfer)
{
	gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_CANCEL_LOCAL);
	gaim_xfer_request_denied(xfer);
	gaim_xfer_unref(xfer);

	return 0;
}

static void
gaim_xfer_ask_recv(GaimXfer *xfer)
{
	char *buf, *size_buf;
	size_t size;

	/* If we have already accepted the request, ask the destination file
	   name directly */
	if (gaim_xfer_get_status(xfer) != GAIM_XFER_STATUS_ACCEPTED) {
		GaimBuddy *buddy = gaim_find_buddy(xfer->account, xfer->who);

		if (gaim_xfer_get_filename(xfer) != NULL)
		{
			size = gaim_xfer_get_size(xfer);
			size_buf = gaim_str_size_to_units(size);
			buf = g_strdup_printf(_("%s wants to send you %s (%s)"),
						  buddy ? gaim_buddy_get_alias(buddy) : xfer->who,
						  gaim_xfer_get_filename(xfer), size_buf);
			g_free(size_buf);
		}
		else
		{
			buf = g_strdup_printf(_("%s wants to send you a file"),
						buddy ? gaim_buddy_get_alias(buddy) : xfer->who);
		}

		if (xfer->message != NULL)
			serv_got_im(gaim_account_get_connection(xfer->account),
								 xfer->who, xfer->message, 0, time(NULL));

		gaim_request_accept_cancel(xfer, NULL, buf, NULL,
								  GAIM_DEFAULT_ACTION_NONE, xfer,
								  G_CALLBACK(gaim_xfer_choose_file),
								  G_CALLBACK(cancel_recv_cb));

		g_free(buf);
	} else
		gaim_xfer_choose_file(xfer);
}

static int
ask_accept_ok(GaimXfer *xfer)
{
	gaim_xfer_request_accepted(xfer, NULL);

	return 0;
}

static int
ask_accept_cancel(GaimXfer *xfer)
{
	gaim_xfer_request_denied(xfer);
	gaim_xfer_unref(xfer);

	return 0;
}

static void
gaim_xfer_ask_accept(GaimXfer *xfer)
{
	char *buf, *buf2 = NULL;
	GaimBuddy *buddy = gaim_find_buddy(xfer->account, xfer->who);

	buf = g_strdup_printf(_("Accept file transfer request from %s?"),
				  buddy ? gaim_buddy_get_alias(buddy) : xfer->who);
	if (gaim_xfer_get_remote_ip(xfer) &&
		gaim_xfer_get_remote_port(xfer))
		buf2 = g_strdup_printf(_("A file is available for download from:\n"
					 "Remote host: %s\nRemote port: %d"),
					   gaim_xfer_get_remote_ip(xfer),
					   gaim_xfer_get_remote_port(xfer));
	gaim_request_accept_cancel(xfer, NULL, buf, buf2,
							   GAIM_DEFAULT_ACTION_NONE, xfer,
							   G_CALLBACK(ask_accept_ok),
							   G_CALLBACK(ask_accept_cancel));
	g_free(buf);
	g_free(buf2);
}

void
gaim_xfer_request(GaimXfer *xfer)
{
	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->ops.init != NULL);

	gaim_xfer_ref(xfer);

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE)
	{
		gaim_signal_emit(gaim_xfers_get_handle(), "file-recv-request", xfer);
		if (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL)
		{
			/* The file-transfer was cancelled by a plugin */
			gaim_xfer_cancel_local(xfer);
		}
		else if (gaim_xfer_get_filename(xfer) ||
		           gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_ACCEPTED)
		{
			gchar* message = NULL;
			GaimBuddy *buddy = gaim_find_buddy(xfer->account, xfer->who);
			message = g_strdup_printf(_("%s is offering to send file %s"),
				buddy ? gaim_buddy_get_alias(buddy) : xfer->who, gaim_xfer_get_filename(xfer));
			gaim_xfer_conversation_write(xfer, message, FALSE);
			g_free(message);
			/* Ask for a filename to save to if it's not already given by a plugin */
			if (xfer->local_filename == NULL)
				gaim_xfer_ask_recv(xfer);
		}
		else
		{
			gaim_xfer_ask_accept(xfer);
		}
	}
	else
	{
		gaim_xfer_choose_file(xfer);
	}
}

void
gaim_xfer_request_accepted(GaimXfer *xfer, const char *filename)
{
	GaimXferType type;
	struct stat st;
	char *msg, *utf8;
	GaimAccount *account;
	GaimBuddy *buddy;

	if (xfer == NULL)
		return;

	type = gaim_xfer_get_type(xfer);
	account = gaim_xfer_get_account(xfer);

	if (!filename && type == GAIM_XFER_RECEIVE) {
		xfer->status = GAIM_XFER_STATUS_ACCEPTED;
		xfer->ops.init(xfer);
		return;
	}

	buddy = gaim_find_buddy(account, xfer->who);

	if (type == GAIM_XFER_SEND) {
		/* Sending a file */
		/* Check the filename. */
#ifdef _WIN32
		if (g_strrstr(filename, "../") || g_strrstr(filename, "..\\")) {
#else
		if (g_strrstr(filename, "../")) {
#endif
			char *utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			msg = g_strdup_printf(_("%s is not a valid filename.\n"), utf8);
			gaim_xfer_error(type, account, xfer->who, msg);
			g_free(utf8);
			g_free(msg);

			gaim_xfer_unref(xfer);
			return;
		}

		if (g_stat(filename, &st) == -1) {
			gaim_xfer_show_file_error(xfer, filename);
			gaim_xfer_unref(xfer);
			return;
		}

		gaim_xfer_set_local_filename(xfer, filename);
		gaim_xfer_set_size(xfer, st.st_size);

		utf8 = g_filename_to_utf8(g_basename(filename), -1, NULL, NULL, NULL);
		gaim_xfer_set_filename(xfer, utf8);

		msg = g_strdup_printf(_("Offering to send %s to %s"),
				utf8, buddy ? gaim_buddy_get_alias(buddy) : xfer->who);
		g_free(utf8);

		gaim_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}
	else {
		/* Receiving a file */
		xfer->status = GAIM_XFER_STATUS_ACCEPTED;
		gaim_xfer_set_local_filename(xfer, filename);

		msg = g_strdup_printf(_("Starting transfer of %s from %s"),
				xfer->filename, buddy ? gaim_buddy_get_alias(buddy) : xfer->who);
		gaim_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	gaim_xfer_add(xfer);
	xfer->ops.init(xfer);

}

void
gaim_xfer_request_denied(GaimXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (xfer->ops.request_denied != NULL)
		xfer->ops.request_denied(xfer);

	gaim_xfer_unref(xfer);
}

GaimXferType
gaim_xfer_get_type(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, GAIM_XFER_UNKNOWN);

	return xfer->type;
}

GaimAccount *
gaim_xfer_get_account(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->account;
}

GaimXferStatusType
gaim_xfer_get_status(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, GAIM_XFER_STATUS_UNKNOWN);

	return xfer->status;
}

gboolean
gaim_xfer_is_canceled(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, TRUE);

	if ((gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL) ||
	    (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_REMOTE))
		return TRUE;
	else
		return FALSE;
}

gboolean
gaim_xfer_is_completed(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, TRUE);

	return (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_DONE);
}

const char *
gaim_xfer_get_filename(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->filename;
}

const char *
gaim_xfer_get_local_filename(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->local_filename;
}

size_t
gaim_xfer_get_bytes_sent(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->bytes_sent;
}

size_t
gaim_xfer_get_bytes_remaining(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->bytes_remaining;
}

size_t
gaim_xfer_get_size(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->size;
}

double
gaim_xfer_get_progress(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0.0);

	if (gaim_xfer_get_size(xfer) == 0)
		return 0.0;

	return ((double)gaim_xfer_get_bytes_sent(xfer) /
			(double)gaim_xfer_get_size(xfer));
}

unsigned int
gaim_xfer_get_local_port(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, -1);

	return xfer->local_port;
}

const char *
gaim_xfer_get_remote_ip(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->remote_ip;
}

unsigned int
gaim_xfer_get_remote_port(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, -1);

	return xfer->remote_port;
}

void
gaim_xfer_set_completed(GaimXfer *xfer, gboolean completed)
{
	GaimXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	if (completed == TRUE) {
		char *msg = NULL;
		gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_DONE);

		if (gaim_xfer_get_filename(xfer) != NULL)
			msg = g_strdup_printf(_("Transfer of file %s complete"),
								gaim_xfer_get_filename(xfer));
		else
			msg = g_strdup_printf(_("File transfer complete"));
		gaim_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, gaim_xfer_get_progress(xfer));
}

void
gaim_xfer_set_message(GaimXfer *xfer, const char *message)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->message);
	xfer->message = g_strdup(message);
}

void
gaim_xfer_set_filename(GaimXfer *xfer, const char *filename)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->filename);
	xfer->filename = g_strdup(filename);
}

void
gaim_xfer_set_local_filename(GaimXfer *xfer, const char *filename)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->local_filename);
	xfer->local_filename = g_strdup(filename);
}

void
gaim_xfer_set_size(GaimXfer *xfer, size_t size)
{
	g_return_if_fail(xfer != NULL);

	if (xfer->size == 0)
		xfer->bytes_remaining = size - xfer->bytes_sent;

	xfer->size = size;
}

GaimXferUiOps *
gaim_xfer_get_ui_ops(const GaimXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->ui_ops;
}

void
gaim_xfer_set_init_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.init = fnc;
}

void gaim_xfer_set_request_denied_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.request_denied = fnc;
}

void
gaim_xfer_set_read_fnc(GaimXfer *xfer, gssize (*fnc)(guchar **, GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.read = fnc;
}

void
gaim_xfer_set_write_fnc(GaimXfer *xfer,
						gssize (*fnc)(const guchar *, size_t, GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.write = fnc;
}

void
gaim_xfer_set_ack_fnc(GaimXfer *xfer,
			  void (*fnc)(GaimXfer *, const guchar *, size_t))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.ack = fnc;
}

void
gaim_xfer_set_start_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.start = fnc;
}

void
gaim_xfer_set_end_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.end = fnc;
}

void
gaim_xfer_set_cancel_send_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.cancel_send = fnc;
}

void
gaim_xfer_set_cancel_recv_fnc(GaimXfer *xfer, void (*fnc)(GaimXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.cancel_recv = fnc;
}

gssize
gaim_xfer_read(GaimXfer *xfer, guchar **buffer)
{
	gssize s, r;

	g_return_val_if_fail(xfer   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);

	if (gaim_xfer_get_size(xfer) == 0)
		s = 4096;
	else
		s = MIN(gaim_xfer_get_bytes_remaining(xfer), 4096);

	if (xfer->ops.read != NULL)
		r = (xfer->ops.read)(buffer, xfer);
	else {
		*buffer = g_malloc0(s);

		r = read(xfer->fd, *buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
		else if (r < 0)
			r = -1;
		else if ((gaim_xfer_get_size(xfer) > 0) &&
			((gaim_xfer_get_bytes_sent(xfer)+r) >= gaim_xfer_get_size(xfer)))
			gaim_xfer_set_completed(xfer, TRUE);
		else if (r == 0)
			r = -1;
	}

	return r;
}

gssize
gaim_xfer_write(GaimXfer *xfer, const guchar *buffer, gsize size)
{
	gssize r, s;

	g_return_val_if_fail(xfer   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);
	g_return_val_if_fail(size   != 0,    0);

	s = MIN(gaim_xfer_get_bytes_remaining(xfer), size);

	if (xfer->ops.write != NULL) {
		r = (xfer->ops.write)(buffer, s, xfer);
	} else {
		r = write(xfer->fd, buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
		if ((gaim_xfer_get_bytes_sent(xfer)+r) >= gaim_xfer_get_size(xfer))
			gaim_xfer_set_completed(xfer, TRUE);
	}

	return r;
}

static void
transfer_cb(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXferUiOps *ui_ops;
	GaimXfer *xfer = (GaimXfer *)data;
	guchar *buffer = NULL;
	gssize r = 0;

	if (condition & GAIM_INPUT_READ) {
		r = gaim_xfer_read(xfer, &buffer);
		if (r > 0) {
			fwrite(buffer, 1, r, xfer->dest_fp);
		} else if(r <= 0) {
			gaim_xfer_cancel_remote(xfer);
			return;
		}
	}

	if (condition & GAIM_INPUT_WRITE) {
		size_t s = MIN(gaim_xfer_get_bytes_remaining(xfer), 4096);

		/* this is so the prpl can keep the connection open
		   if it needs to for some odd reason. */
		if (s == 0) {
			if (xfer->watcher) {
				gaim_input_remove(xfer->watcher);
				xfer->watcher = 0;
			}
			return;
		}

		buffer = g_malloc0(s);

		fread(buffer, 1, s, xfer->dest_fp);

		/* Write as much as we're allowed to. */
		r = gaim_xfer_write(xfer, buffer, s);

		if (r == -1) {
			gaim_xfer_cancel_remote(xfer);
			g_free(buffer);
			return;
		} else if (r < s) {
			/* We have to seek back in the file now. */
			fseek(xfer->dest_fp, r - s, SEEK_CUR);
		}
	}

	if (r > 0) {
		if (gaim_xfer_get_size(xfer) > 0)
			xfer->bytes_remaining -= r;

		xfer->bytes_sent += r;

		if (xfer->ops.ack != NULL)
			xfer->ops.ack(xfer, buffer, r);

		g_free(buffer);

		ui_ops = gaim_xfer_get_ui_ops(xfer);

		if (ui_ops != NULL && ui_ops->update_progress != NULL)
			ui_ops->update_progress(xfer,
				gaim_xfer_get_progress(xfer));
	}

	if (gaim_xfer_is_completed(xfer))
		gaim_xfer_end(xfer);
}

static void
begin_transfer(GaimXfer *xfer, GaimInputCondition cond)
{
	GaimXferType type = gaim_xfer_get_type(xfer);

	xfer->dest_fp = g_fopen(gaim_xfer_get_local_filename(xfer),
						  type == GAIM_XFER_RECEIVE ? "wb" : "rb");

	if (xfer->dest_fp == NULL) {
		gaim_xfer_show_file_error(xfer, gaim_xfer_get_local_filename(xfer));
		gaim_xfer_cancel_local(xfer);
		return;
	}

	xfer->watcher = gaim_input_add(xfer->fd, cond, transfer_cb, xfer);

	xfer->start_time = time(NULL);

	if (xfer->ops.start != NULL)
		xfer->ops.start(xfer);
}

static void
connect_cb(gpointer data, gint source, const gchar *error_message)
{
	GaimXfer *xfer = (GaimXfer *)data;

	xfer->fd = source;

	begin_transfer(xfer, GAIM_INPUT_READ);
}

void
gaim_xfer_start(GaimXfer *xfer, int fd, const char *ip,
				unsigned int port)
{
	GaimInputCondition cond;
	GaimXferType type;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(gaim_xfer_get_type(xfer) != GAIM_XFER_UNKNOWN);

	type = gaim_xfer_get_type(xfer);

	xfer->bytes_remaining = gaim_xfer_get_size(xfer);
	xfer->bytes_sent      = 0;

	gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_STARTED);

	if (type == GAIM_XFER_RECEIVE) {
		cond = GAIM_INPUT_READ;

		if (ip != NULL) {
			xfer->remote_ip   = g_strdup(ip);
			xfer->remote_port = port;

			/* Establish a file descriptor. */
			gaim_proxy_connect(xfer->account, xfer->remote_ip,
							   xfer->remote_port, connect_cb, xfer);

			return;
		}
		else {
			xfer->fd = fd;
		}
	}
	else {
		cond = GAIM_INPUT_WRITE;

		xfer->fd = fd;
	}

	begin_transfer(xfer, cond);
}

void
gaim_xfer_end(GaimXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	/* See if we are actually trying to cancel this. */
	if (!gaim_xfer_is_completed(xfer)) {
		gaim_xfer_cancel_local(xfer);
		return;
	}

	xfer->end_time = time(NULL);
	if (xfer->ops.end != NULL)
		xfer->ops.end(xfer);

	if (xfer->watcher != 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	gaim_xfer_unref(xfer);
}

void
gaim_xfer_add(GaimXfer *xfer)
{
	GaimXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->add_xfer != NULL)
		ui_ops->add_xfer(xfer);
}

void
gaim_xfer_cancel_local(GaimXfer *xfer)
{
	GaimXferUiOps *ui_ops;
	char *msg = NULL;

	g_return_if_fail(xfer != NULL);

	gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_CANCEL_LOCAL);
	xfer->end_time = time(NULL);

	if (gaim_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("You canceled the transfer of %s"),
							  gaim_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("File transfer cancelled"));
	}
	gaim_xfer_conversation_write(xfer, msg, FALSE);
	g_free(msg);

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND)
	{
		if (xfer->ops.cancel_send != NULL)
			xfer->ops.cancel_send(xfer);
	}
	else
	{
		if (xfer->ops.cancel_recv != NULL)
			xfer->ops.cancel_recv(xfer);
	}

	if (xfer->watcher != 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_local != NULL)
		ui_ops->cancel_local(xfer);

	xfer->bytes_remaining = 0;

	gaim_xfer_unref(xfer);
}

void
gaim_xfer_cancel_remote(GaimXfer *xfer)
{
	GaimXferUiOps *ui_ops;
	gchar *msg;
	GaimAccount *account;
	GaimBuddy *buddy;

	g_return_if_fail(xfer != NULL);

	gaim_request_close_with_handle(xfer);
	gaim_xfer_set_status(xfer, GAIM_XFER_STATUS_CANCEL_REMOTE);
	xfer->end_time = time(NULL);

	account = gaim_xfer_get_account(xfer);
	buddy = gaim_find_buddy(account, xfer->who);

	if (gaim_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("%s canceled the transfer of %s"),
				buddy ? gaim_buddy_get_alias(buddy) : xfer->who, gaim_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("%s canceled the file transfer"),
				buddy ? gaim_buddy_get_alias(buddy) : xfer->who);
	}
	gaim_xfer_conversation_write(xfer, msg, TRUE);
	gaim_xfer_error(gaim_xfer_get_type(xfer), account, xfer->who, msg);
	g_free(msg);

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND)
	{
		if (xfer->ops.cancel_send != NULL)
			xfer->ops.cancel_send(xfer);
	}
	else
	{
		if (xfer->ops.cancel_recv != NULL)
			xfer->ops.cancel_recv(xfer);
	}

	if (xfer->watcher != 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_remote != NULL)
		ui_ops->cancel_remote(xfer);

	xfer->bytes_remaining = 0;

	gaim_xfer_unref(xfer);
}

void
gaim_xfer_error(GaimXferType type, GaimAccount *account, const char *who, const char *msg)
{
	char *title;

	g_return_if_fail(msg  != NULL);
	g_return_if_fail(type != GAIM_XFER_UNKNOWN);

	if (account) {
		GaimBuddy *buddy;
		buddy = gaim_find_buddy(account, who);
		if (buddy)
			who = gaim_buddy_get_alias(buddy);
	}

	if (type == GAIM_XFER_SEND)
		title = g_strdup_printf(_("File transfer to %s failed."), who);
	else
		title = g_strdup_printf(_("File transfer from %s failed."), who);

	gaim_notify_error(NULL, NULL, title, msg);

	g_free(title);
}

void
gaim_xfer_update_progress(GaimXfer *xfer)
{
	GaimXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	ui_ops = gaim_xfer_get_ui_ops(xfer);
	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, gaim_xfer_get_progress(xfer));
}


/**************************************************************************
 * File Transfer Subsystem API
 **************************************************************************/
void *
gaim_xfers_get_handle(void) {
	static int handle = 0;

	return &handle;
}

void
gaim_xfers_init(void) {
	void *handle = gaim_xfers_get_handle();

	/* register signals */
	gaim_signal_register(handle, "file-recv-accept",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-send-accept",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-recv-start",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-send-start",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-send-cancel",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-recv-cancel",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-send-complete",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-recv-complete",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
	gaim_signal_register(handle, "file-recv-request",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_POINTER, GAIM_SUBTYPE_XFER));
}

void
gaim_xfers_uninit(void) {
	gaim_signals_disconnect_by_handle(gaim_xfers_get_handle());
}

void
gaim_xfers_set_ui_ops(GaimXferUiOps *ops) {
	xfer_ui_ops = ops;
}

GaimXferUiOps *
gaim_xfers_get_ui_ops(void) {
	return xfer_ui_ops;
}
