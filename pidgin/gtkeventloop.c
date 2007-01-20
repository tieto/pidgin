/**
 * @file gtk_eventloop.c Gaim Event Loop API (gtk implementation)
 * @ingroup gtkui
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
 */

#include <glib.h>
#include "gtkeventloop.h"
#include "eventloop.h"
#ifdef _WIN32
#include "win32dep.h"
#endif

#define GAIM_GTK_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_GTK_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _GaimGtkIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;

} GaimGtkIOClosure;

static void gaim_gtk_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_gtk_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimGtkIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_GTK_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_GTK_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

#if 0
	gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: callback for %d, fd is %d\n",
			   closure->result, g_io_channel_unix_get_fd(source));
#endif

#ifdef _WIN32
	if(! gaim_cond) {
#if DEBUG
		gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE received GIOCondition of 0x%x, which does not"
			   " match 0x%x (READ) or 0x%x (WRITE)\n",
			   condition, GAIM_GTK_READ_COND, GAIM_GTK_WRITE_COND);
#endif /* DEBUG */

		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  gaim_cond);

	return TRUE;
}

static guint gaim_gtk_input_add(gint fd, GaimInputCondition condition, GaimInputFunction function,
							   gpointer data)
{
	GaimGtkIOClosure *closure = g_new0(GaimGtkIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_GTK_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_GTK_WRITE_COND;

#ifdef _WIN32
	channel = wgaim_g_io_channel_win32_new_socket(fd);
#else
	channel = g_io_channel_unix_new(fd);
#endif
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_gtk_io_invoke, closure, gaim_gtk_io_destroy);

#if 0
	gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: adding input watcher %d for fd %d\n",
			   closure->result, fd);
#endif

	g_io_channel_unref(channel);
	return closure->result;
}

static GaimEventLoopUiOps eventloop_ops =
{
	g_timeout_add,
	(guint (*)(guint))g_source_remove,
	gaim_gtk_input_add,
	(guint (*)(guint))g_source_remove
};

GaimEventLoopUiOps *
gaim_gtk_eventloop_get_ui_ops(void)
{
	return &eventloop_ops;
}
