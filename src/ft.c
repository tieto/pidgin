/*
 * gaim - file transfer functions
 *
 * Copyright (C) 2002, Wil Mahan <wtm2@duke.edu>
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

#define FT_BUFFER_SIZE (4096)

#include <gtk/gtk.h>
#include "gaim.h"
#include "proxy.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

/* Completely describes a file transfer.  Opaque to callers. */
struct file_transfer {
	enum { FILE_TRANSFER_TYPE_SEND, FILE_TRANSFER_TYPE_RECEIVE } type;

	enum {
		FILE_TRANSFER_STATE_ASK, /* waiting for confirmation */
		FILE_TRANSFER_STATE_CHOOSEFILE, /* waiting for file dialog */
		FILE_TRANSFER_STATE_TRANSFERRING, /* in actual transfer */
		FILE_TRANSFER_STATE_INTERRUPTED, /* other user canceled */
		FILE_TRANSFER_STATE_CANCELED, /* we canceled */
		FILE_TRANSFER_STATE_DONE, /* transfer complete */
		FILE_TRANSFER_STATE_CLEANUP /* freeing memory */
	} state;

	char *who;

	/* file selection dialog */
	GtkWidget *w;
	char **names;
	int *sizes;
	char *dir;
	char *initname;
	FILE *file;

	/* connection info */
	struct gaim_connection *gc;
	int fd;
	int watcher;

	/* state */
	int totfiles;
	int filesdone;
	int totsize;
	int bytessent;
	int bytesleft;
};



static int ft_choose_file(struct file_transfer *xfer);
static void ft_cancel(struct file_transfer *xfer);
static void ft_delete(struct file_transfer *xfer);
static void ft_callback(gpointer data, gint source, GaimInputCondition condition);
static void ft_nextfile(struct file_transfer *xfer);
static int ft_mkdir(const char *name);
static int ft_mkdir_help(char *dir);

static struct file_transfer *ft_new(int type, struct gaim_connection *gc,
		const char *who)
{
	struct file_transfer *xfer = g_new0(struct file_transfer, 1);
	xfer->type = type;
	xfer->state = FILE_TRANSFER_STATE_ASK;
	xfer->gc = gc;
	xfer->who = g_strdup(who);
	xfer->filesdone = 0;

	return xfer;
}

struct file_transfer *transfer_in_add(struct gaim_connection *gc,
		const char *who, const char *initname, int totsize,
		int totfiles, const char *msg)
{
	struct file_transfer *xfer = ft_new(FILE_TRANSFER_TYPE_RECEIVE, gc,
			who);
	char *buf;
	char *sizebuf;
	static const char *sizestr[4] = { "bytes", "KB", "MB", "GB" };
	float sizemag = (float)totsize;
	int szindex = 0;

	xfer->initname = g_strdup(initname);
	xfer->totsize = totsize;
	xfer->totfiles = totfiles;
	xfer->filesdone = 0;

	/* Ask the user whether he or she accepts the transfer. */
	while ((szindex < 4) && (sizemag > 1024)) {
		sizemag /= 1024;
		szindex++;
	}

	if (totsize == -1)
		sizebuf = g_strdup_printf(_("Unkown"));
	else
		sizebuf = g_strdup_printf("%.3g %s", sizemag, sizestr[szindex]);

	if (xfer->totfiles == 1) {
		buf = g_strdup_printf(_("%s requests that %s accept a file: %s (%s)"),
		who, xfer->gc->username, initname, sizebuf);
	} else {
		buf = g_strdup_printf(_("%s requests that %s accept %d files: %s (%s)"),
		who, xfer->gc->username, xfer->totfiles,
		initname, sizebuf);
	}

	g_free(sizebuf);

	if (msg) {
		char *newmsg = g_strconcat(buf, ": ", msg, NULL);
		g_free(buf);
		buf = newmsg;
	}

	do_ask_dialog(buf, NULL, xfer, _("Accept"), ft_choose_file, _("Cancel"), ft_cancel);
	g_free(buf);

	return xfer;
}

struct file_transfer *transfer_out_add(struct gaim_connection *gc,
		const char *who)
{
	struct file_transfer *xfer = ft_new(FILE_TRANSFER_TYPE_SEND, gc,
			who);

	ft_choose_file(xfer);

	return xfer;
}

/* We canceled the transfer, either by declining the initial
 * confirmation dialog or canceling the file dialog.
 */
static void ft_cancel(struct file_transfer *xfer)
{
	/* Make sure we weren't aborted while waiting for
	 * confirmation from the user.
	 */
	if (xfer->state == FILE_TRANSFER_STATE_INTERRUPTED) {
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, NULL);
		return;
	}

	xfer->state = FILE_TRANSFER_STATE_CANCELED;
	if (xfer->w) {
		gtk_widget_destroy(xfer->w);
		xfer->w = NULL;
	}

	if (xfer->gc->prpl->file_transfer_cancel)
		xfer->gc->prpl->file_transfer_cancel(xfer->gc, xfer);

	ft_delete(xfer);
}

/* This is called when the other user aborts the transfer,
 * possibly in the middle of a transfer.
 */
int transfer_abort(struct file_transfer *xfer, const char *why)
{
	if (xfer->state == FILE_TRANSFER_STATE_INTERRUPTED) {
		/* If for some reason we have already been
		 * here and are waiting on some event before
		 * cleaning up, but we get another abort request,
		 * we don't need to do anything else.
		 */
		return 1;
	}
	else if (xfer->state == FILE_TRANSFER_STATE_ASK) {
		/* Kludge:  since there is no way to kill a
		 * do_ask_dialog() window, we just note the
		 * status here and clean up after the user
		 * makes a selection.
		 */
		xfer->state = FILE_TRANSFER_STATE_INTERRUPTED;
		return 1;
	}
	else if (xfer->state == FILE_TRANSFER_STATE_TRANSFERRING) {
		if (xfer->watcher) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = 0;
		}
		if (xfer->file) {
			fclose(xfer->file);
			xfer->file = NULL;
		}
		/* XXX theoretically, there is a race condition here,
		 * because we could be inside ft_callback() when we
		 * free xfer below, with undefined results.  Since
		 * we use non-blocking IO, this doesn't seem to be
		 * a problem, but it still makes me nervous--I don't
		 * know how to fix it other than using locks, though.
		 * -- wtm
		 */
	}
	else if (xfer->state == FILE_TRANSFER_STATE_CHOOSEFILE) {
		/* It's safe to clean up now.  Just make sure we
		 * destroy the dialog window first.
		 */
		if (xfer->w) {
			gtk_signal_disconnect_by_func(GTK_OBJECT(xfer->w),
					GTK_SIGNAL_FUNC(ft_cancel), xfer);
			gtk_widget_destroy(xfer->w);
			xfer->w = NULL;
		}
	}

	/* Let the user know that we were aborted, unless we already
	 * finished or the user aborted first.
	 */
	/* if ((xfer->state != FILE_TRANSFER_STATE_DONE) &&
			(xfer->state != FILE_TRANSFER_STATE_CANCELED)) { */
	if (why) {
		char *msg;

		if (xfer->type == FILE_TRANSFER_TYPE_SEND)
			msg = g_strdup_printf(_("File transfer to %s aborted."), xfer->who);
		else
			msg = g_strdup_printf(_("File transfer from %s aborted."), xfer->who);
		do_error_dialog(msg, why, GAIM_ERROR);
		g_free(msg);
	}

	ft_delete(xfer);

	return 0;
}


static void ft_delete(struct file_transfer *xfer)
{
	if (xfer->names)
		g_strfreev(xfer->names);
	if (xfer->initname)
		g_free(xfer->initname);
	if (xfer->who)
		g_free(xfer->who);
	if (xfer->sizes)
		g_free(xfer->sizes);
	g_free(xfer);
}

static void ft_choose_ok(gpointer a, struct file_transfer *xfer) {
	gboolean exists, is_dir;
	struct stat st;
	const char *err = NULL;
	
	xfer->names = gtk_file_selection_get_selections(GTK_FILE_SELECTION(xfer->w));
	exists = !stat(*xfer->names, &st);
	is_dir = (exists) ? S_ISDIR(st.st_mode) : 0;

	if (exists) {
		if (xfer->type == FILE_TRANSFER_TYPE_RECEIVE)
			/* XXX overwrite/append/cancel prompt */
			err = _("That file already exists; please choose another name.");
		else { /* (xfer->type == FILE_TRANSFER_TYPE_SEND) */
			char **cur;
			/* First find the total number of files,
			 * so we know how much space to allocate.
			 */
			xfer->totfiles = 0;
			for (cur = xfer->names; *cur; cur++) {
				xfer->totfiles++;
			}

			/* Now get sizes for each file. */
			xfer->totsize = st.st_size;
			xfer->sizes = g_malloc(xfer->totfiles
					* sizeof(*xfer->sizes));
			xfer->sizes[0] = st.st_size;
			for (cur = xfer->names + 1; *cur; cur++) {
				exists = !stat(*cur, &st);
				if (!exists) {
					err = _("File not found.");
					break;
				}
				xfer->sizes[cur - xfer->names] =
					st.st_size;
				xfer->totsize += st.st_size;
			}
		}
	}
	else { /* doesn't exist */
		if (xfer->type == FILE_TRANSFER_TYPE_SEND)
			err = _("File not found.");
		else if (xfer->totfiles > 1) {
			if (!xfer->names[0] || xfer->names[1]) {
				err = _("You may only choose one new directory.");
			}
			else {
				if (ft_mkdir(*xfer->names))
					err = _("Unable to create directory.");
				else
					xfer->dir = g_strconcat(xfer->names[0],
						"/", NULL);
			}
		}
	}

	if (err)
		do_error_dialog(err, NULL, GAIM_ERROR);
	else {
		/* File name looks valid */
		gtk_widget_destroy(xfer->w);
		xfer->w = NULL;

		if (xfer->type == FILE_TRANSFER_TYPE_SEND) {
			char *desc;
			if (xfer->totfiles == 1)
				desc = *xfer->names;
			else
				/* XXX what else? */
				desc = "*";
				/* desc = g_path_get_basename(g_path_get_dirname(*xfer->names)); */
			xfer->gc->prpl->file_transfer_out(xfer->gc, xfer,
					desc, xfer->totfiles,
					xfer->totsize);
		}
		else
			xfer->gc->prpl->file_transfer_in(xfer->gc, xfer,
					0); /* XXX */
	}
}

/* Called on outgoing transfers to get information about the
 * current file.
 */
int transfer_get_file_info(struct file_transfer *xfer, int *size,
		char **name)
{
	*size = xfer->sizes[xfer->filesdone];
	*name = xfer->names[xfer->filesdone];
	return 0;
}

static int ft_choose_file(struct file_transfer *xfer)
{
	char *curdir = g_get_current_dir(); /* should be freed */
	char *initstr;

	/* If the connection is interrupted while we are waiting
	 * for do_ask_dialog(), then we can't clean up until we
	 * get here, after the user makes a selection.
	 */
	if (xfer->state == FILE_TRANSFER_STATE_INTERRUPTED) {
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, NULL);
		return 1;
	}

	xfer->state = FILE_TRANSFER_STATE_CHOOSEFILE;
	if (xfer->type == FILE_TRANSFER_TYPE_RECEIVE)
		xfer->w = gtk_file_selection_new(_("Gaim - Save As..."));
	else /* (xfer->type == FILE_TRANSFER_TYPE_SEND) */ {
		xfer->w = gtk_file_selection_new(_("Gaim - Open..."));
		gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(xfer->w),
				1);
	}

	if (xfer->initname) {
		initstr = g_strdup_printf("%s/%s", curdir, xfer->initname);
	} else 
		initstr = g_strconcat(curdir, "/", NULL);
	g_free(curdir);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(xfer->w),
			initstr);
	g_free(initstr);

	gtk_signal_connect(GTK_OBJECT(xfer->w), "delete_event",
			GTK_SIGNAL_FUNC(ft_cancel), xfer);
        gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(xfer->w)->cancel_button),
			"clicked", GTK_SIGNAL_FUNC(ft_cancel), xfer);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(xfer->w)->ok_button),
			"clicked", GTK_SIGNAL_FUNC(ft_choose_ok), xfer);
	gtk_widget_show(xfer->w);

	return 0;
}

static int ft_open_file(struct file_transfer *xfer, const char *filename,
		int offset)
{
	char *err = NULL;

	if (xfer->type == FILE_TRANSFER_TYPE_RECEIVE) {
		xfer->file = fopen(filename,
				(offset > 0) ? "a" : "w");

		if (!xfer->file)
			err = g_strdup_printf(_("Could not open %s for writing: %s"),
					filename, g_strerror(errno));

	}
	else /* (xfer->type == FILE_TRANSFER_TYPE_SEND) */ {
		xfer->file = fopen(filename, "r");
		if (!xfer->file)
			err = g_strdup_printf(_("Could not open %s for reading: %s"),
					filename, g_strerror(errno));
	}

	if (err) {
		do_error_dialog(err, NULL, GAIM_ERROR);
		g_free(err);
		return -1;
	}

	fseek(xfer->file, offset, SEEK_SET);

	return 0;
}

/* Takes a full file name, and creates any directories above it
 * that don't exist already.
 */
static int ft_mkdir(const char *name) {
	int ret = 0;
	struct stat st;
	mode_t m = umask(0077);
	char *dir;

	dir = g_path_get_dirname(name);
	if (stat(dir, &st))
		ret = ft_mkdir_help(dir);

	g_free(dir);
	umask(m);
	return ret;
}

/* Two functions, one recursive, just to make a directory.  Yuck. */
static int ft_mkdir_help(char *dir) {
	int ret;

	ret = mkdir(dir, 0775);

	if (ret) {
		char *index = strrchr(dir, G_DIR_SEPARATOR);
		if (!index)
			return -1;
		*index = '\0';
		ret = ft_mkdir_help(dir);
		*index = G_DIR_SEPARATOR;
		if (!ret)
			ret = mkdir(dir, 0775);
	}

	return ret;
}
 
int transfer_in_do(struct file_transfer *xfer, int fd,
		const char *filename, int size)
{
	char *fullname;

	xfer->state = FILE_TRANSFER_STATE_TRANSFERRING;
	xfer->fd = fd;
	xfer->bytesleft = size;

	/* XXX implement resuming incoming transfers */
#if 0
	if (xfer->sizes)
		xfer->bytesleft -= xfer->sizes[0];
#endif

	/* Security check */
	if (g_strrstr(filename, "..")) {
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, _("Invalid incoming filename component"));
		return -1;
	}

	if (xfer->totfiles > 1)
		fullname = g_strconcat(xfer->dir, filename, NULL);
	else
		/* Careful:  filename is the name on the *other*
		 * end; don't use it here. */
		fullname = g_strdup(xfer->names[xfer->filesdone]);

	
	if (ft_mkdir(fullname)) {
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, _("Invalid incoming filename"));
		return -1;
	}

	if (!ft_open_file(xfer, fullname, 0)) {
		/* Special case:  if we are receiving an empty file,
		 * we would never enter the callback.  Just avoid the
		 * callback altogether.
		 */
		if (xfer->bytesleft == 0)
			ft_nextfile(xfer);
		else
			xfer->watcher = gaim_input_add(fd,
					GAIM_INPUT_READ,
					ft_callback, xfer);
	} else {
		/* Error opening file */
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, NULL);
		g_free(fullname);
		return -1;
	}

	g_free(fullname);
	return 0;
}

int transfer_out_do(struct file_transfer *xfer, int fd, int offset) {
	xfer->state = FILE_TRANSFER_STATE_TRANSFERRING;
	xfer->fd = fd;
	xfer->bytesleft = xfer->sizes[xfer->filesdone] - offset;

	if (!ft_open_file(xfer, xfer->names[xfer->filesdone], offset)) {
		/* Special case:  see transfer_in_do().
		 */
		if (xfer->bytesleft == 0)
			ft_nextfile(xfer);
		else
			xfer->watcher = gaim_input_add(fd,
					GAIM_INPUT_WRITE, ft_callback,
					xfer);
	}
	else {
		/* Error opening file */
		xfer->state = FILE_TRANSFER_STATE_CLEANUP;
		transfer_abort(xfer, NULL);
		return -1;
	}

	return 0;
}

static void ft_callback(gpointer data, gint source,
		GaimInputCondition condition)
{
	struct file_transfer *xfer = (struct file_transfer *)data;
	int rt, i;
	char *buf = NULL;

	if (condition & GAIM_INPUT_READ) {
		if (xfer->gc->prpl->file_transfer_read)
			rt = xfer->gc->prpl->file_transfer_read(xfer->gc, xfer,
													xfer->fd, &buf);
		else {
			buf = g_new0(char, MIN(xfer->bytesleft, FT_BUFFER_SIZE));
			rt = read(xfer->fd, buf, MIN(xfer->bytesleft, FT_BUFFER_SIZE));
		}

		/* XXX What if the transfer is interrupted while we
		 * are inside read()?  How can this be handled safely?
		 * -- wtm
		 */
		if (rt > 0) {
			xfer->bytesleft -= rt;
			fwrite(buf, 1, rt, xfer->file);
		}

	}
	else /* (condition & GAIM_INPUT_WRITE) */ {
		int remain = MIN(xfer->bytesleft, FT_BUFFER_SIZE);

		buf = g_new0(char, remain);

		fread(buf, 1, remain, xfer->file);

		if (xfer->gc->prpl->file_transfer_write)
			rt = xfer->gc->prpl->file_transfer_write(xfer->gc, xfer, xfer->fd,
													 buf, remain);
		else
			rt = write(xfer->fd, buf, remain);

		if (rt > 0)
			xfer->bytesleft -= rt;
	}

	if (rt < 0) {
		if (buf != NULL)
			g_free(buf);

		return;
	}

	xfer->bytessent += rt;

	if (xfer->gc->prpl->file_transfer_data_chunk)
		xfer->gc->prpl->file_transfer_data_chunk(xfer->gc, xfer, buf, rt);

	if (rt > 0 && xfer->bytesleft == 0) {
		/* We are done with this file! */
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
		fclose(xfer->file);
		xfer->file = 0;
		ft_nextfile(xfer);
	}

	if (buf != NULL)
		g_free(buf);
}

static void ft_nextfile(struct file_transfer *xfer)
{
	debug_printf("file transfer %d of %d done\n",
			xfer->filesdone + 1, xfer->totfiles);

	if (++xfer->filesdone == xfer->totfiles) {
		char *msg;
		char *msg2;

		xfer->gc->prpl->file_transfer_done(xfer->gc, xfer);

		if (xfer->type == FILE_TRANSFER_TYPE_RECEIVE)
			msg = g_strdup_printf(_("File transfer from %s to %s completed successfully."),
					xfer->who, xfer->gc->username);
		else 
			msg = g_strdup_printf(_("File transfer from %s to %s completed successfully."),
					xfer->gc->username, xfer->who);
		xfer->state = FILE_TRANSFER_STATE_DONE;

		if (xfer->totfiles > 1)
			msg2 = g_strdup_printf(_("%d files transferred."), 
						xfer->totfiles);
		else
			msg2 = NULL;

		do_error_dialog(msg, msg2, GAIM_INFO);
		g_free(msg);
		if (msg2)
			g_free(msg2);

		ft_delete(xfer);
	}
	else {
		xfer->gc->prpl->file_transfer_nextfile(xfer->gc, xfer);
	}
}

