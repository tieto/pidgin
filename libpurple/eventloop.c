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
#include "internal.h"
#include "eventloop.h"

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleIOClosure;

static gboolean
purple_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

#ifdef _WIN32
	if(!purple_cond) {
		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

guint
purple_input_add(int source, PurpleInputCondition condition, PurpleInputFunction func, gpointer user_data)
{
	PurpleIOClosure *closure = g_new0(PurpleIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = func;
	closure->data = user_data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

#ifdef _WIN32
	channel = g_io_channel_win32_new_socket(source);
#else
	channel = g_io_channel_unix_new(source);
#endif

	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT,
			cond, purple_io_invoke, closure, g_free);

	g_io_channel_unref(channel);
	return closure->result;
}

gboolean
purple_input_remove(guint tag)
{
	return g_source_remove(tag);
}

int
purple_input_pipe(int pipefd[2])
{
#ifdef _WIN32
	return wpurple_input_pipe(pipefd);
#else
	return pipe(pipefd);
#endif
}
