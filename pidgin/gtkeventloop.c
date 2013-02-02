/**
 * @file gtk_eventloop.c Purple Event Loop API (gtk implementation)
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#include <glib.h>
#include "gtkeventloop.h"
#include "eventloop.h"
#ifdef _WIN32
#include "win32dep.h"
#endif

#define PIDGIN_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PIDGIN_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PidginIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;

} PidginIOClosure;

static gboolean pidgin_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PidginIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PIDGIN_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PIDGIN_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

#if 0
	purple_debug_misc("gtk_eventloop",
			   "CLOSURE: callback for %d, fd is %d\n",
			   closure->result, g_io_channel_unix_get_fd(source));
#endif

#ifdef _WIN32
	if(! purple_cond) {
#ifdef DEBUG
		purple_debug_misc("gtk_eventloop",
			   "CLOSURE received GIOCondition of 0x%x, which does not"
			   " match 0x%x (READ) or 0x%x (WRITE)\n",
			   condition, PIDGIN_READ_COND, PIDGIN_WRITE_COND);
#endif /* DEBUG */

		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

static guint pidgin_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							   gpointer data)
{
	PidginIOClosure *closure = g_new0(PidginIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;
#ifdef _WIN32
	static int use_glib_io_channel = -1;

	if (use_glib_io_channel == -1)
		use_glib_io_channel = (g_getenv("PIDGIN_GLIB_IO_CHANNEL") != NULL) ? 1 : 0;
#endif

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PIDGIN_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PIDGIN_WRITE_COND;

#ifdef _WIN32
	if (use_glib_io_channel == 0)
		channel = wpurple_g_io_channel_win32_new_socket(fd);
	else
#endif
	channel = g_io_channel_unix_new(fd);

	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      pidgin_io_invoke, closure, g_free);

#if 0
	purple_debug_misc("gtk_eventloop",
			   "CLOSURE: adding input watcher %d for fd %d\n",
			   closure->result, fd);
#endif

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps eventloop_ops =
{
	g_timeout_add,
	g_source_remove,
	pidgin_input_add,
	g_source_remove,
	NULL, /* input_get_error */
	g_timeout_add_seconds,
	NULL,
	NULL,
	NULL
};

PurpleEventLoopUiOps *
pidgin_eventloop_get_ui_ops(void)
{
	return &eventloop_ops;
}
