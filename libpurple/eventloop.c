/**
 * @file eventloop.c Gaim Event Loop API
 * @ingroup core
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
#include "eventloop.h"
#include "internal.h"

static GaimEventLoopUiOps *eventloop_ui_ops = NULL;

guint
gaim_timeout_add(guint interval, GSourceFunc function, gpointer data)
{
	GaimEventLoopUiOps *ops = gaim_eventloop_get_ui_ops();

	return ops->timeout_add(interval, function, data);
}

gboolean
gaim_timeout_remove(guint tag)
{
	GaimEventLoopUiOps *ops = gaim_eventloop_get_ui_ops();

	return ops->timeout_remove(tag);
}

guint
gaim_input_add(int source, GaimInputCondition condition, GaimInputFunction func, gpointer user_data)
{
	GaimEventLoopUiOps *ops = gaim_eventloop_get_ui_ops();

	return ops->input_add(source, condition, func, user_data);
}

gboolean
gaim_input_remove(guint tag)
{
	GaimEventLoopUiOps *ops = gaim_eventloop_get_ui_ops();

	return ops->input_remove(tag);
}

int
gaim_input_get_error(int fd, int *error)
{
	GaimEventLoopUiOps *ops = gaim_eventloop_get_ui_ops();

	if (ops->input_get_error)
	{
		int ret = ops->input_get_error(fd, error);
		errno = *error;
		return ret;
	}
	else
	{
		socklen_t len;
		len = sizeof(*error);

		return getsockopt(fd, SOL_SOCKET, SO_ERROR, error, &len);
	}
}

void
gaim_eventloop_set_ui_ops(GaimEventLoopUiOps *ops)
{
	eventloop_ui_ops = ops;
}

GaimEventLoopUiOps *
gaim_eventloop_get_ui_ops(void)
{
	g_return_val_if_fail(eventloop_ui_ops != NULL, NULL);

	return eventloop_ui_ops;
}
