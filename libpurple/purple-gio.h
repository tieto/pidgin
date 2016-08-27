/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_GIO_H
#define _PURPLE_GIO_H
/**
 * SECTION:purple-gio
 * @section_id: libpurple-purple-gio
 * @short_description: Gio helper functions
 * @title: Purple Gio API
 *
 * The Purple Gio API provides helper functions for Gio operations which
 * are commonly used within libpurple and its consumers. These contain
 * such functions as setting up connections and shutting them down
 * gracefully.
 */

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * purple_gio_graceful_close:
 * @stream: A #GIOStream to close
 * @input: (optional): A #GInputStream which wraps @stream's input stream
 * @output: (optional): A #GOutputStream which wraps @stream's output stream
 *
 * Closes @input, @output, @stream. If there are pending operations, it
 * asynchronously waits for the operations to finish before closing the
 * arguments. Ensure the Gio callbacks can safely handle this being done
 * asynchronously.
 */
void
purple_gio_graceful_close(GIOStream *stream,
		GInputStream *input, GOutputStream *output);

G_END_DECLS

#endif /* _PURPLE_GIO_H */
