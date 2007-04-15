/**
 * @file ft.c File Transfer API
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
 */
#include "internal.h"
#include "ft.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "util.h"

#define FT_INITIAL_BUFFER_SIZE 4096
#define FT_MAX_BUFFER_SIZE     65535

static PurpleXferUiOps *xfer_ui_ops = NULL;
static GList *xfers;

static int purple_xfer_choose_file(PurpleXfer *xfer);

GList *
purple_xfers_get_all()
{
	return xfers;
}

PurpleXfer *
purple_xfer_new(PurpleAccount *account, PurpleXferType type, const char *who)
{
	PurpleXfer *xfer;
	PurpleXferUiOps *ui_ops;

	g_return_val_if_fail(type    != PURPLE_XFER_UNKNOWN, NULL);
	g_return_val_if_fail(account != NULL,              NULL);
	g_return_val_if_fail(who     != NULL,              NULL);

	xfer = g_new0(PurpleXfer, 1);

	xfer->ref = 1;
	xfer->type    = type;
	xfer->account = account;
	xfer->who     = g_strdup(who);
	xfer->ui_ops  = purple_xfers_get_ui_ops();
	xfer->message = NULL;
	xfer->current_buffer_size = FT_INITIAL_BUFFER_SIZE;

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->new_xfer != NULL)
		ui_ops->new_xfer(xfer);

	xfers = g_list_prepend(xfers, xfer);
	return xfer;
}

static void
purple_xfer_destroy(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	/* Close the file browser, if it's open */
	purple_request_close_with_handle(xfer);

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_STARTED)
		purple_xfer_cancel_local(xfer);

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->destroy != NULL)
		ui_ops->destroy(xfer);

	g_free(xfer->who);
	g_free(xfer->filename);
	g_free(xfer->remote_ip);
	g_free(xfer->local_filename);

	g_free(xfer);
	xfers = g_list_remove(xfers, xfer);
}

void
purple_xfer_ref(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	xfer->ref++;
}

void
purple_xfer_unref(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->ref > 0);

	xfer->ref--;

	if (xfer->ref == 0)
		purple_xfer_destroy(xfer);
}

static void
purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatusType status)
{
	g_return_if_fail(xfer != NULL);

	if(xfer->type == PURPLE_XFER_SEND) {
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
	} else if(xfer->type == PURPLE_XFER_RECEIVE) {
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

	xfer->status = status;
}

void purple_xfer_conversation_write(PurpleXfer *xfer, char *message, gboolean is_error)
{
	PurpleConversation *conv = NULL;
	PurpleMessageFlags flags = PURPLE_MESSAGE_SYSTEM;
	char *escaped;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(message != NULL);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, xfer->who,
											   purple_xfer_get_account(xfer));

	if (conv == NULL)
		return;

	escaped = g_markup_escape_text(message, -1);

	if (is_error)
		flags = PURPLE_MESSAGE_ERROR;

	purple_conversation_write(conv, NULL, escaped, flags, time(NULL));
	g_free(escaped);
}

static void purple_xfer_show_file_error(PurpleXfer *xfer, const char *filename)
{
	int err = errno;
	gchar *msg = NULL, *utf8;
	PurpleXferType xfer_type = purple_xfer_get_type(xfer);
	PurpleAccount *account = purple_xfer_get_account(xfer);

	utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	switch(xfer_type) {
		case PURPLE_XFER_SEND:
			msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
		case PURPLE_XFER_RECEIVE:
			msg = g_strdup_printf(_("Error writing %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
		default:
			msg = g_strdup_printf(_("Error accessing %s: \n%s.\n"),
								  utf8, strerror(err));
			break;
	}
	g_free(utf8);

	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(xfer_type, account, xfer->who, msg);
	g_free(msg);
}

static void
purple_xfer_choose_file_ok_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer;
	struct stat st;
	gchar *dir;

	xfer = (PurpleXfer *)user_data;

	if (g_stat(filename, &st) != 0) {
		/* File not found. */
		if (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) {
#ifndef _WIN32
			int mode = W_OK;
#else
			int mode = F_OK;
#endif
			dir = g_path_get_dirname(filename);

			if (g_access(dir, mode) == 0) {
				purple_xfer_request_accepted(xfer, filename);
			} else {
				purple_xfer_ref(xfer);
				purple_notify_message(
					NULL, PURPLE_NOTIFY_MSG_ERROR, NULL,
					_("Directory is not writable."), NULL,
					(PurpleNotifyCloseCallback)purple_xfer_choose_file, xfer);
			}

			g_free(dir);
		}
		else {
			purple_xfer_show_file_error(xfer, filename);
			purple_xfer_request_denied(xfer);
		}
	}
	else if ((purple_xfer_get_type(xfer) == PURPLE_XFER_SEND) &&
			 (st.st_size == 0)) {

		purple_notify_error(NULL, NULL,
						  _("Cannot send a file of 0 bytes."), NULL);

		purple_xfer_request_denied(xfer);
	}
	else if ((purple_xfer_get_type(xfer) == PURPLE_XFER_SEND) &&
			 S_ISDIR(st.st_mode)) {
		/*
		 * XXX - Sending a directory should be valid for some protocols.
		 */
		purple_notify_error(NULL, NULL,
						  _("Cannot send a directory."), NULL);

		purple_xfer_request_denied(xfer);
	}
	else if ((purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) &&
			 S_ISDIR(st.st_mode)) {
		char *msg, *utf8;
		utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
		msg = g_strdup_printf(
					_("%s is not a regular file. Cowardly refusing to overwrite it.\n"), utf8);
		g_free(utf8);
		purple_notify_error(NULL, NULL, msg, NULL);
		g_free(msg);
		purple_xfer_request_denied(xfer);
	}
	else {
		purple_xfer_request_accepted(xfer, filename);
	}

	purple_xfer_unref(xfer);
}

static void
purple_xfer_choose_file_cancel_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer = (PurpleXfer *)user_data;

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	purple_xfer_request_denied(xfer);
}

static int
purple_xfer_choose_file(PurpleXfer *xfer)
{
	purple_request_file(xfer, NULL, purple_xfer_get_filename(xfer),
					  (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE),
					  G_CALLBACK(purple_xfer_choose_file_ok_cb),
					  G_CALLBACK(purple_xfer_choose_file_cancel_cb), xfer);

	return 0;
}

static int
cancel_recv_cb(PurpleXfer *xfer)
{
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	purple_xfer_request_denied(xfer);
	purple_xfer_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_recv(PurpleXfer *xfer)
{
	char *buf, *size_buf;
	size_t size;

	/* If we have already accepted the request, ask the destination file
	   name directly */
	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_ACCEPTED) {
		PurpleBuddy *buddy = purple_find_buddy(xfer->account, xfer->who);

		if (purple_xfer_get_filename(xfer) != NULL)
		{
			size = purple_xfer_get_size(xfer);
			size_buf = purple_str_size_to_units(size);
			buf = g_strdup_printf(_("%s wants to send you %s (%s)"),
						  buddy ? purple_buddy_get_alias(buddy) : xfer->who,
						  purple_xfer_get_filename(xfer), size_buf);
			g_free(size_buf);
		}
		else
		{
			buf = g_strdup_printf(_("%s wants to send you a file"),
						buddy ? purple_buddy_get_alias(buddy) : xfer->who);
		}

		if (xfer->message != NULL)
			serv_got_im(purple_account_get_connection(xfer->account),
								 xfer->who, xfer->message, 0, time(NULL));

		purple_request_accept_cancel(xfer, NULL, buf, NULL,
								  PURPLE_DEFAULT_ACTION_NONE, xfer,
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
	purple_xfer_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_accept(PurpleXfer *xfer)
{
	char *buf, *buf2 = NULL;
	PurpleBuddy *buddy = purple_find_buddy(xfer->account, xfer->who);

	buf = g_strdup_printf(_("Accept file transfer request from %s?"),
				  buddy ? purple_buddy_get_alias(buddy) : xfer->who);
	if (purple_xfer_get_remote_ip(xfer) &&
		purple_xfer_get_remote_port(xfer))
		buf2 = g_strdup_printf(_("A file is available for download from:\n"
					 "Remote host: %s\nRemote port: %d"),
					   purple_xfer_get_remote_ip(xfer),
					   purple_xfer_get_remote_port(xfer));
	purple_request_accept_cancel(xfer, NULL, buf, buf2,
							   PURPLE_DEFAULT_ACTION_NONE, xfer,
							   G_CALLBACK(ask_accept_ok),
							   G_CALLBACK(ask_accept_cancel));
	g_free(buf);
	g_free(buf2);
}

void
purple_xfer_request(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->ops.init != NULL);

	purple_xfer_ref(xfer);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE)
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
			PurpleBuddy *buddy = purple_find_buddy(xfer->account, xfer->who);
			message = g_strdup_printf(_("%s is offering to send file %s"),
				buddy ? purple_buddy_get_alias(buddy) : xfer->who, purple_xfer_get_filename(xfer));
			purple_xfer_conversation_write(xfer, message, FALSE);
			g_free(message);
			/* Ask for a filename to save to if it's not already given by a plugin */
			if (xfer->local_filename == NULL)
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
	PurpleXferType type;
	struct stat st;
	char *msg, *utf8;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	if (xfer == NULL)
		return;

	type = purple_xfer_get_type(xfer);
	account = purple_xfer_get_account(xfer);

	if (!filename && type == PURPLE_XFER_RECEIVE) {
		xfer->status = PURPLE_XFER_STATUS_ACCEPTED;
		xfer->ops.init(xfer);
		return;
	}

	buddy = purple_find_buddy(account, xfer->who);

	if (type == PURPLE_XFER_SEND) {
		/* Sending a file */
		/* Check the filename. */
#ifdef _WIN32
		if (g_strrstr(filename, "../") || g_strrstr(filename, "..\\")) {
#else
		if (g_strrstr(filename, "../")) {
#endif
			char *utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			msg = g_strdup_printf(_("%s is not a valid filename.\n"), utf8);
			purple_xfer_error(type, account, xfer->who, msg);
			g_free(utf8);
			g_free(msg);

			purple_xfer_unref(xfer);
			return;
		}

		if (g_stat(filename, &st) == -1) {
			purple_xfer_show_file_error(xfer, filename);
			purple_xfer_unref(xfer);
			return;
		}

		purple_xfer_set_local_filename(xfer, filename);
		purple_xfer_set_size(xfer, st.st_size);

		utf8 = g_filename_to_utf8(g_basename(filename), -1, NULL, NULL, NULL);
		purple_xfer_set_filename(xfer, utf8);

		msg = g_strdup_printf(_("Offering to send %s to %s"),
				utf8, buddy ? purple_buddy_get_alias(buddy) : xfer->who);
		g_free(utf8);

		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}
	else {
		/* Receiving a file */
		xfer->status = PURPLE_XFER_STATUS_ACCEPTED;
		purple_xfer_set_local_filename(xfer, filename);

		msg = g_strdup_printf(_("Starting transfer of %s from %s"),
				xfer->filename, buddy ? purple_buddy_get_alias(buddy) : xfer->who);
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	purple_xfer_add(xfer);
	xfer->ops.init(xfer);

}

void
purple_xfer_request_denied(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (xfer->ops.request_denied != NULL)
		xfer->ops.request_denied(xfer);

	purple_xfer_unref(xfer);
}

PurpleXferType
purple_xfer_get_type(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, PURPLE_XFER_UNKNOWN);

	return xfer->type;
}

PurpleAccount *
purple_xfer_get_account(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->account;
}

PurpleXferStatusType
purple_xfer_get_status(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, PURPLE_XFER_STATUS_UNKNOWN);

	return xfer->status;
}

gboolean
purple_xfer_is_canceled(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, TRUE);

	if ((purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) ||
	    (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_REMOTE))
		return TRUE;
	else
		return FALSE;
}

gboolean
purple_xfer_is_completed(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, TRUE);

	return (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_DONE);
}

const char *
purple_xfer_get_filename(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->filename;
}

const char *
purple_xfer_get_local_filename(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->local_filename;
}

size_t
purple_xfer_get_bytes_sent(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->bytes_sent;
}

size_t
purple_xfer_get_bytes_remaining(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->bytes_remaining;
}

size_t
purple_xfer_get_size(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0);

	return xfer->size;
}

double
purple_xfer_get_progress(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, 0.0);

	if (purple_xfer_get_size(xfer) == 0)
		return 0.0;

	return ((double)purple_xfer_get_bytes_sent(xfer) /
			(double)purple_xfer_get_size(xfer));
}

unsigned int
purple_xfer_get_local_port(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, -1);

	return xfer->local_port;
}

const char *
purple_xfer_get_remote_ip(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->remote_ip;
}

unsigned int
purple_xfer_get_remote_port(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, -1);

	return xfer->remote_port;
}

void
purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	if (completed == TRUE) {
		char *msg = NULL;
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_DONE);

		if (purple_xfer_get_filename(xfer) != NULL)
			msg = g_strdup_printf(_("Transfer of file %s complete"),
								purple_xfer_get_filename(xfer));
		else
			msg = g_strdup_printf(_("File transfer complete"));
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, purple_xfer_get_progress(xfer));
}

void
purple_xfer_set_message(PurpleXfer *xfer, const char *message)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->message);
	xfer->message = g_strdup(message);
}

void
purple_xfer_set_filename(PurpleXfer *xfer, const char *filename)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->filename);
	xfer->filename = g_strdup(filename);
}

void
purple_xfer_set_local_filename(PurpleXfer *xfer, const char *filename)
{
	g_return_if_fail(xfer != NULL);

	g_free(xfer->local_filename);
	xfer->local_filename = g_strdup(filename);
}

void
purple_xfer_set_size(PurpleXfer *xfer, size_t size)
{
	g_return_if_fail(xfer != NULL);

	xfer->size = size;
	xfer->bytes_remaining = xfer->size - purple_xfer_get_bytes_sent(xfer);
}

void
purple_xfer_set_bytes_sent(PurpleXfer *xfer, size_t bytes_sent)
{
	g_return_if_fail(xfer != NULL);

	xfer->bytes_sent = bytes_sent;
	xfer->bytes_remaining = purple_xfer_get_size(xfer) - bytes_sent;
}

PurpleXferUiOps *
purple_xfer_get_ui_ops(const PurpleXfer *xfer)
{
	g_return_val_if_fail(xfer != NULL, NULL);

	return xfer->ui_ops;
}

void
purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.init = fnc;
}

void purple_xfer_set_request_denied_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.request_denied = fnc;
}

void
purple_xfer_set_read_fnc(PurpleXfer *xfer, gssize (*fnc)(guchar **, PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.read = fnc;
}

void
purple_xfer_set_write_fnc(PurpleXfer *xfer,
						gssize (*fnc)(const guchar *, size_t, PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.write = fnc;
}

void
purple_xfer_set_ack_fnc(PurpleXfer *xfer,
			  void (*fnc)(PurpleXfer *, const guchar *, size_t))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.ack = fnc;
}

void
purple_xfer_set_start_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.start = fnc;
}

void
purple_xfer_set_end_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.end = fnc;
}

void
purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.cancel_send = fnc;
}

void
purple_xfer_set_cancel_recv_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
	g_return_if_fail(xfer != NULL);

	xfer->ops.cancel_recv = fnc;
}

static void
purple_xfer_increase_buffer_size(PurpleXfer *xfer)
{
	xfer->current_buffer_size = MIN(xfer->current_buffer_size * 1.5,
			FT_MAX_BUFFER_SIZE);
}

gssize
purple_xfer_read(PurpleXfer *xfer, guchar **buffer)
{
	gssize s, r;

	g_return_val_if_fail(xfer   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);

	if (purple_xfer_get_size(xfer) == 0)
		s = xfer->current_buffer_size;
	else
		s = MIN(purple_xfer_get_bytes_remaining(xfer), xfer->current_buffer_size);

	if (xfer->ops.read != NULL)
		r = (xfer->ops.read)(buffer, xfer);
	else {
		*buffer = g_malloc0(s);

		r = read(xfer->fd, *buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
		else if (r < 0)
			r = -1;
		else if ((purple_xfer_get_size(xfer) > 0) &&
			((purple_xfer_get_bytes_sent(xfer)+r) >= purple_xfer_get_size(xfer)))
			purple_xfer_set_completed(xfer, TRUE);
		else if (r == 0)
			r = -1;
	}

	if (r == xfer->current_buffer_size)
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
	gssize r, s;

	g_return_val_if_fail(xfer   != NULL, 0);
	g_return_val_if_fail(buffer != NULL, 0);
	g_return_val_if_fail(size   != 0,    0);

	s = MIN(purple_xfer_get_bytes_remaining(xfer), size);

	if (xfer->ops.write != NULL) {
		r = (xfer->ops.write)(buffer, s, xfer);
	} else {
		r = write(xfer->fd, buffer, s);
		if (r < 0 && errno == EAGAIN)
			r = 0;
		if ((purple_xfer_get_bytes_sent(xfer)+r) >= purple_xfer_get_size(xfer))
			purple_xfer_set_completed(xfer, TRUE);
	}

	return r;
}

static void
transfer_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXferUiOps *ui_ops;
	PurpleXfer *xfer = (PurpleXfer *)data;
	guchar *buffer = NULL;
	gssize r = 0;

	if (condition & PURPLE_INPUT_READ) {
		r = purple_xfer_read(xfer, &buffer);
		if (r > 0) {
			fwrite(buffer, 1, r, xfer->dest_fp);
		} else if(r <= 0) {
			purple_xfer_cancel_remote(xfer);
			return;
		}
	}

	if (condition & PURPLE_INPUT_WRITE) {
		size_t s = MIN(purple_xfer_get_bytes_remaining(xfer), xfer->current_buffer_size);

		/* this is so the prpl can keep the connection open
		   if it needs to for some odd reason. */
		if (s == 0) {
			if (xfer->watcher) {
				purple_input_remove(xfer->watcher);
				xfer->watcher = 0;
			}
			return;
		}

		buffer = g_malloc0(s);

		fread(buffer, 1, s, xfer->dest_fp);

		/* Write as much as we're allowed to. */
		r = purple_xfer_write(xfer, buffer, s);

		if (r == -1) {
			purple_xfer_cancel_remote(xfer);
			g_free(buffer);
			return;
		} else if (r < s) {
			/* We have to seek back in the file now. */
			fseek(xfer->dest_fp, r - s, SEEK_CUR);
		} else {
			/*
			 * We managed to write the entire buffer.  This means our
			 * network is fast and our buffer is too small, so make it
			 * bigger.
			 */
			purple_xfer_increase_buffer_size(xfer);
		}
	}

	if (r > 0) {
		if (purple_xfer_get_size(xfer) > 0)
			xfer->bytes_remaining -= r;

		xfer->bytes_sent += r;

		if (xfer->ops.ack != NULL)
			xfer->ops.ack(xfer, buffer, r);

		g_free(buffer);

		ui_ops = purple_xfer_get_ui_ops(xfer);

		if (ui_ops != NULL && ui_ops->update_progress != NULL)
			ui_ops->update_progress(xfer,
				purple_xfer_get_progress(xfer));
	}

	if (purple_xfer_is_completed(xfer))
		purple_xfer_end(xfer);
}

static void
begin_transfer(PurpleXfer *xfer, PurpleInputCondition cond)
{
	PurpleXferType type = purple_xfer_get_type(xfer);

	xfer->dest_fp = g_fopen(purple_xfer_get_local_filename(xfer),
						  type == PURPLE_XFER_RECEIVE ? "wb" : "rb");

	if (xfer->dest_fp == NULL) {
		purple_xfer_show_file_error(xfer, purple_xfer_get_local_filename(xfer));
		purple_xfer_cancel_local(xfer);
		return;
	}

	fseek(xfer->dest_fp, xfer->bytes_sent, SEEK_SET);

	xfer->watcher = purple_input_add(xfer->fd, cond, transfer_cb, xfer);

	xfer->start_time = time(NULL);

	if (xfer->ops.start != NULL)
		xfer->ops.start(xfer);
}

static void
connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = (PurpleXfer *)data;

	xfer->fd = source;

	begin_transfer(xfer, PURPLE_INPUT_READ);
}

void
purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip,
				unsigned int port)
{
	PurpleInputCondition cond;
	PurpleXferType type;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(purple_xfer_get_type(xfer) != PURPLE_XFER_UNKNOWN);

	type = purple_xfer_get_type(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_STARTED);

	if (type == PURPLE_XFER_RECEIVE) {
		cond = PURPLE_INPUT_READ;

		if (ip != NULL) {
			xfer->remote_ip   = g_strdup(ip);
			xfer->remote_port = port;

			/* Establish a file descriptor. */
			purple_proxy_connect(NULL, xfer->account, xfer->remote_ip,
							   xfer->remote_port, connect_cb, xfer);

			return;
		}
		else {
			xfer->fd = fd;
		}
	}
	else {
		cond = PURPLE_INPUT_WRITE;

		xfer->fd = fd;
	}

	begin_transfer(xfer, cond);
}

void
purple_xfer_end(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	/* See if we are actually trying to cancel this. */
	if (!purple_xfer_is_completed(xfer)) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	xfer->end_time = time(NULL);
	if (xfer->ops.end != NULL)
		xfer->ops.end(xfer);

	if (xfer->watcher != 0) {
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	purple_xfer_unref(xfer);
}

void
purple_xfer_add(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->add_xfer != NULL)
		ui_ops->add_xfer(xfer);
}

void
purple_xfer_cancel_local(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;
	char *msg = NULL;

	g_return_if_fail(xfer != NULL);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	xfer->end_time = time(NULL);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("You canceled the transfer of %s"),
							  purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("File transfer cancelled"));
	}
	purple_xfer_conversation_write(xfer, msg, FALSE);
	g_free(msg);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)
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
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_local != NULL)
		ui_ops->cancel_local(xfer);

	xfer->bytes_remaining = 0;

	purple_xfer_unref(xfer);
}

void
purple_xfer_cancel_remote(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;
	gchar *msg;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	g_return_if_fail(xfer != NULL);

	purple_request_close_with_handle(xfer);
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
	xfer->end_time = time(NULL);

	account = purple_xfer_get_account(xfer);
	buddy = purple_find_buddy(account, xfer->who);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("%s canceled the transfer of %s"),
				buddy ? purple_buddy_get_alias(buddy) : xfer->who, purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("%s canceled the file transfer"),
				buddy ? purple_buddy_get_alias(buddy) : xfer->who);
	}
	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(purple_xfer_get_type(xfer), account, xfer->who, msg);
	g_free(msg);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)
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
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
	}

	if (xfer->fd != 0)
		close(xfer->fd);

	if (xfer->dest_fp != NULL) {
		fclose(xfer->dest_fp);
		xfer->dest_fp = NULL;
	}

	ui_ops = purple_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->cancel_remote != NULL)
		ui_ops->cancel_remote(xfer);

	xfer->bytes_remaining = 0;

	purple_xfer_unref(xfer);
}

void
purple_xfer_error(PurpleXferType type, PurpleAccount *account, const char *who, const char *msg)
{
	char *title;

	g_return_if_fail(msg  != NULL);
	g_return_if_fail(type != PURPLE_XFER_UNKNOWN);

	if (account) {
		PurpleBuddy *buddy;
		buddy = purple_find_buddy(account, who);
		if (buddy)
			who = purple_buddy_get_alias(buddy);
	}

	if (type == PURPLE_XFER_SEND)
		title = g_strdup_printf(_("File transfer to %s failed."), who);
	else
		title = g_strdup_printf(_("File transfer from %s failed."), who);

	purple_notify_error(NULL, NULL, title, msg);

	g_free(title);
}

void
purple_xfer_update_progress(PurpleXfer *xfer)
{
	PurpleXferUiOps *ui_ops;

	g_return_if_fail(xfer != NULL);

	ui_ops = purple_xfer_get_ui_ops(xfer);
	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, purple_xfer_get_progress(xfer));
}


/**************************************************************************
 * File Transfer Subsystem API
 **************************************************************************/
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
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-send-accept",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-recv-start",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-send-start",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-send-cancel",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-recv-cancel",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-send-complete",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-recv-complete",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
	purple_signal_register(handle, "file-recv-request",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_XFER));
}

void
purple_xfers_uninit(void) {
	purple_signals_disconnect_by_handle(purple_xfers_get_handle());
}

void
purple_xfers_set_ui_ops(PurpleXferUiOps *ops) {
	xfer_ui_ops = ops;
}

PurpleXferUiOps *
purple_xfers_get_ui_ops(void) {
	return xfer_ui_ops;
}
