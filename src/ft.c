/**
 * @file ft.c The file transfer interface.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "proxy.h"

static struct gaim_xfer_ui_ops *xfer_ui_ops = NULL;

struct gaim_xfer *
gaim_xfer_new(struct gaim_account *account, GaimXferType type,
			  const char *who)
{
	struct gaim_xfer *xfer;

	if (account == NULL || type == GAIM_XFER_UNKNOWN || who == NULL)
		return NULL;

	xfer = g_malloc0(sizeof(struct gaim_xfer));

	xfer->type    = type;
	xfer->account = account;
	xfer->who     = g_strdup(who);
	xfer->ui_ops  = gaim_get_xfer_ui_ops();

	return xfer;
}

void
gaim_xfer_destroy(struct gaim_xfer *xfer)
{
	struct gaim_xfer_ui_ops *ui_ops;

	if (xfer == NULL)
		return;

	if (xfer->bytes_remaining > 0) {
		gaim_xfer_cancel(xfer);
		return;
	}

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->destroy != NULL)
		ui_ops->destroy(xfer);

	g_free(xfer->who);
	g_free(xfer->filename);

	if (xfer->remote_ip != NULL) g_free(xfer->remote_ip);
	if (xfer->local_ip  != NULL) g_free(xfer->local_ip);

	if (xfer->dest_filename != NULL)
		g_free(xfer->dest_filename);

	g_free(xfer);
}

void
gaim_xfer_request(struct gaim_xfer *xfer)
{
	struct gaim_xfer_ui_ops *ui_ops;

	if (xfer == NULL || xfer->ops.init == NULL)
		return;

	ui_ops = gaim_get_xfer_ui_ops();

	if (ui_ops == NULL || ui_ops->request_file == NULL)
		return;

	ui_ops->request_file(xfer);
}

void
gaim_xfer_request_accepted(struct gaim_xfer *xfer, char *filename)
{
	GaimXferType type;

	if (xfer == NULL || filename == NULL) {

		if (filename != NULL)
			g_free(filename);

		return;
	}

	type = gaim_xfer_get_type(xfer);

	if (type == GAIM_XFER_SEND) {
		struct stat sb;

		/* Check the filename. */
		if (g_strrstr(filename, "..")) {
			char *msg;

			msg = g_strdup_printf(_("%s is not a valid filename.\n"),
								  filename);

			gaim_xfer_error(type, xfer->who, msg);

			g_free(msg);
			g_free(filename);

			return;
		}

		if (stat(filename, &sb) == -1) {
			char *msg;

			msg = g_strdup_printf(_("%s was not found.\n"), filename);

			gaim_xfer_error(type, xfer->who, msg);

			g_free(msg);
			g_free(filename);

			return;
		}

		gaim_xfer_set_size(xfer, sb.st_size);
	}
	else {
		/* TODO: Sanity checks and file overwrite checks. */

		gaim_xfer_set_dest_filename(xfer, filename);
	}

	g_free(filename);

	xfer->ops.init(xfer);
}

void
gaim_xfer_request_denied(struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return;

	/* TODO */
}

GaimXferType
gaim_xfer_get_type(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return GAIM_XFER_UNKNOWN;

	return xfer->type;
}

struct gaim_account *
gaim_xfer_get_account(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->account;
}

const char *
gaim_xfer_get_filename(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->filename;
}

const char *
gaim_xfer_get_dest_filename(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->dest_filename;
}

size_t
gaim_xfer_get_bytes_sent(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return 0;

	return xfer->bytes_sent;
}

size_t
gaim_xfer_get_bytes_remaining(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return 0;

	return xfer->bytes_remaining;
}

size_t
gaim_xfer_get_size(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return 0;

	return xfer->size;
}

double
gaim_xfer_get_progress(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return 0.0;

	if (gaim_xfer_get_size(xfer) == 0)
		return 0.0;

	return ((double)gaim_xfer_get_bytes_sent(xfer) /
			(double)gaim_xfer_get_size(xfer));
}

const char *
gaim_xfer_get_local_ip(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->local_ip;
}

unsigned int
gaim_xfer_get_local_port(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return -1;

	return xfer->local_port;
}

const char *
gaim_xfer_get_remote_ip(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->remote_ip;
}

unsigned int
gaim_xfer_get_remote_port(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return -1;

	return xfer->remote_port;
}

void
gaim_xfer_set_filename(struct gaim_xfer *xfer, const char *filename)
{
	if (xfer == NULL)
		return;

	if (xfer->filename != NULL)
		g_free(xfer->filename);

	xfer->filename = (filename == NULL ? NULL : g_strdup(filename));
}

void
gaim_xfer_set_dest_filename(struct gaim_xfer *xfer, const char *filename)
{
	if (xfer == NULL)
		return;

	if (xfer->dest_filename != NULL)
		g_free(xfer->dest_filename);

	xfer->dest_filename = (filename == NULL ? NULL : g_strdup(filename));
}

void
gaim_xfer_set_size(struct gaim_xfer *xfer, size_t size)
{
	if (xfer == NULL)
		return;

	xfer->size = size;
}

struct gaim_xfer_ui_ops *
gaim_xfer_get_ui_ops(const struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return NULL;

	return xfer->ui_ops;
}

void
gaim_xfer_set_init_fnc(struct gaim_xfer *xfer,
					   void (*fnc)(struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.init = fnc;
}


void
gaim_xfer_set_read_fnc(struct gaim_xfer *xfer,
					   size_t (*fnc)(char **, struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.read = fnc;
}

void
gaim_xfer_set_write_fnc(struct gaim_xfer *xfer,
						size_t (*fnc)(const char *, size_t,
									  struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.write = fnc;
}

void
gaim_xfer_set_ack_fnc(struct gaim_xfer *xfer,
					  void (*fnc)(struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.ack = fnc;
}

void
gaim_xfer_set_start_fnc(struct gaim_xfer *xfer,
						void (*fnc)(struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.start = fnc;
}

void
gaim_xfer_set_end_fnc(struct gaim_xfer *xfer,
					  void (*fnc)(struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.end = fnc;
}

void
gaim_xfer_set_cancel_fnc(struct gaim_xfer *xfer,
						 void (*fnc)(struct gaim_xfer *))
{
	if (xfer == NULL)
		return;

	xfer->ops.cancel = fnc;
}

size_t
gaim_xfer_read(struct gaim_xfer *xfer, char **buffer)
{
	size_t s, r;

	if (xfer == NULL || buffer == NULL)
		return 0;

	if (gaim_xfer_get_size(xfer) == 0)
		s = 4096;
	else
		s = MIN(gaim_xfer_get_bytes_remaining(xfer), 4096);

	if (xfer->ops.read != NULL)
		r = xfer->ops.read(buffer, xfer);
	else {
		*buffer = g_malloc0(s);

		r = read(xfer->fd, *buffer, s);
	}

	return r;
}

size_t
gaim_xfer_write(struct gaim_xfer *xfer, const char *buffer, size_t size)
{
	size_t r, s;

	if (xfer == NULL || buffer == NULL || size == 0)
		return 0;

	s = MIN(gaim_xfer_get_bytes_remaining(xfer), size);

	if (xfer->ops.write != NULL)
		r = xfer->ops.write(buffer, s, xfer);
	else
		r = write(xfer->fd, buffer, s);

	return r;
}

static void
transfer_cb(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_xfer_ui_ops *ui_ops;
	struct gaim_xfer *xfer = (struct gaim_xfer *)data;
	char *buffer = NULL;
	size_t r;

	if (condition & GAIM_INPUT_READ) {
		r = gaim_xfer_read(xfer, &buffer);

		if (r > 0)
			fwrite(buffer, 1, r, xfer->dest_fp);
	}
	else {
		size_t s = MIN(gaim_xfer_get_bytes_remaining(xfer), 4096);

		buffer = g_malloc0(s);

		fread(buffer, 1, s, xfer->dest_fp);

		/* Write as much as we're allowed to. */
		r = gaim_xfer_write(xfer, buffer, s);

		if (r < s) {
			/* We have to seek back in the file now. */
			fseek(xfer->dest_fp, r - s, SEEK_CUR);
		}
	}

	g_free(buffer);

	if (r < 0)
		return;

	xfer->bytes_remaining -= r;
	xfer->bytes_sent += r;

	if (xfer->ops.ack != NULL)
		xfer->ops.ack(xfer);

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	if (ui_ops != NULL && ui_ops->update_progress != NULL)
		ui_ops->update_progress(xfer, gaim_xfer_get_progress(xfer));

	if (r == 0)
		gaim_xfer_end(xfer);
}

static void
begin_transfer(struct gaim_xfer *xfer, GaimInputCondition cond)
{
	struct gaim_xfer_ui_ops *ui_ops;
	GaimXferType type = gaim_xfer_get_type(xfer);

	ui_ops = gaim_xfer_get_ui_ops(xfer);

	/*
	 * We'll have already tried to open this earlier to make sure we can
	 * read/write here. Should be safe.
	 */
	xfer->dest_fp = fopen(gaim_xfer_get_dest_filename(xfer),
						  type == GAIM_XFER_RECEIVE ? "wb" : "rb");

	/* Just in case, though. */
	if (xfer->dest_fp == NULL) {
		gaim_xfer_cancel(xfer); /* ? */
		return;
	}

	xfer->watcher = gaim_input_add(xfer->fd, cond, transfer_cb, xfer);

	if (ui_ops != NULL && ui_ops->add_xfer != NULL)
		ui_ops->add_xfer(xfer);

	if (xfer->ops.start != NULL)
		xfer->ops.start(xfer);
}

static void
connect_cb(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_xfer *xfer = (struct gaim_xfer *)data;

	xfer->fd = source;

	begin_transfer(xfer, condition);
}

void
gaim_xfer_start(struct gaim_xfer *xfer, int fd, const char *ip,
				unsigned int port)
{
	GaimInputCondition cond;
	GaimXferType type;

	if (xfer == NULL || gaim_xfer_get_type(xfer) == GAIM_XFER_UNKNOWN)
		return;

	type = gaim_xfer_get_type(xfer);

	xfer->bytes_remaining = gaim_xfer_get_size(xfer);
	xfer->bytes_sent      = 0;

	if (type == GAIM_XFER_RECEIVE) {
		cond = GAIM_INPUT_READ;

		if (ip != NULL) {
			xfer->remote_ip   = g_strdup(ip);
			xfer->remote_port = port;

			/* Establish a file descriptor. */
			proxy_connect(xfer->remote_ip, xfer->remote_port,
						  connect_cb, xfer);

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
gaim_xfer_end(struct gaim_xfer *xfer)
{
	if (xfer == NULL)
		return;

	/* See if we are actually trying to cancel this. */
	if (gaim_xfer_get_bytes_remaining(xfer) > 0) {
		gaim_xfer_cancel(xfer);
		return;
	}

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

	/* Delete the transfer. */
	gaim_xfer_destroy(xfer);
}

void
gaim_xfer_cancel(struct gaim_xfer *xfer)
{
	struct gaim_xfer_ui_ops *ui_ops;

	if (xfer == NULL)
		return;

	if (xfer->ops.cancel != NULL)
		xfer->ops.cancel(xfer);

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

	if (ui_ops != NULL && ui_ops->cancel != NULL)
		ui_ops->cancel(xfer);

	/* Delete the transfer. */
	gaim_xfer_destroy(xfer);
}

void
gaim_xfer_error(GaimXferType type, const char *who, const char *msg)
{
	char *title;

	if (xfer == NULL || msg == NULL || type == GAIM_XFER_UNKNOWN)
		return;

	if (type == GAIM_XFER_SEND)
		title = g_strdup_printf(_("File transfer to %s aborted.\n"), who);
	else
		title = g_strdup_printf(_("File transfer from %s aborted.\n"), who);

	do_error_dialog(title, msg, GAIM_ERROR);

	g_free(title);
}

void
gaim_set_xfer_ui_ops(struct gaim_xfer_ui_ops *ops)
{
	if (ops == NULL)
		return;

	xfer_ui_ops = ops;
}

struct gaim_xfer_ui_ops *
gaim_get_xfer_ui_ops(void)
{
	return xfer_ui_ops;
}

