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

#ifndef _PURPLE_EVENTLOOP_H_
#define _PURPLE_EVENTLOOP_H_
/**
 * SECTION:eventloop
 * @section_id: libpurple-eventloop
 * @short_description: <filename>eventloop.h</filename>
 * @title: Event Loop API
 */

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_EVENTLOOP_UI_OPS (purple_eventloop_ui_ops_get_type())

/**
 * PurpleInputCondition:
 * @PURPLE_INPUT_READ:  A read condition.
 * @PURPLE_INPUT_WRITE: A write condition.
 *
 * An input condition.
 */
typedef enum
{
	PURPLE_INPUT_READ  = 1 << 0,
	PURPLE_INPUT_WRITE = 1 << 1

} PurpleInputCondition;

/**
 * PurpleInputFunction:
 *
 * The type of callbacks to handle events on file descriptors, as passed to
 * purple_input_add().  The callback will receive the @user_data passed to
 * purple_input_add(), the file descriptor on which the event occurred, and the
 * condition that was satisfied to cause the callback to be invoked.
 */
typedef void (*PurpleInputFunction)(gpointer, gint, PurpleInputCondition);

typedef struct _PurpleEventLoopUiOps PurpleEventLoopUiOps;

/**
 * PurpleEventLoopUiOps:
 * @timeout_add: Should create a callback timer with an interval measured in
 *               milliseconds. The supplied @function should be called every
 *               @interval seconds until it returns %FALSE, after which it
 *               should not be called again.
 *               <sbr/>Analogous to g_timeout_add in glib.
 *               <sbr/>Note: On Win32, this function may be called from a thread
 *               other than the libpurple thread. You should make sure to detect
 *               this situation and to only call "function" from the libpurple
 *               thread.
 *               <sbr/>See purple_timeout_add().
 *               <sbr/>@interval: the interval in
 *                                <emphasis>milliseconds</emphasis> between
 *                                calls to @function.
 *               <sbr/>@data: arbitrary data to be passed to @function at each
 *                            call.
 *               <sbr/>Returns: a handle for the timeout, which can be passed to
 *                              @timeout_remove.
 * @timeout_remove: Should remove a callback timer. Analogous to
 *                  g_source_remove() in glib.
 *                  <sbr/>See purple_timeout_remove().
 *                  <sbr/>@handle: an identifier for a timeout, as returned by
 *                                 @timeout_add.
 *                  <sbr/>Returns: %TRUE if the timeout identified by @handle
 *                                 was found and removed.
 * @input_add: Should add an input handler. Analogous to g_io_add_watch_full()
 *             in glib.
 *                <sbr/>See purple_input_add().
 *             <sbr/>@fd:        a file descriptor to watch for events
 *             <sbr/>@cond:      a bitwise OR of events on @fd for which @func
 *                               should be called.
 *             <sbr/>@func:      a callback to fire whenever a relevant event on
 *                               @fd occurs.
 *             <sbr/>@user_data: arbitrary data to pass to @fd.
 *             <sbr/>Returns:    an identifier for this input handler, which can
 *                               be passed to @input_remove.
 * @input_remove: Should remove an input handler. Analogous to g_source_remove()
 *                in glib.
 *                <sbr/>See purple_input_remove().
 *                <sbr/>@handle: an identifier, as returned by #input_add.
 *                <sbr/>Returns: %TRUE if the input handler was found and
 *                               removed.
 * @input_get_error: If implemented, should get the current error status for an
 *                   input.
 *                   <sbr/>Implementation of this UI op is optional. Implement
 *                   it if the UI's sockets or event loop needs to customize
 *                   determination of socket error status. If unimplemented,
 *                   <literal>getsockopt(2)</literal> will be used instead.
 *                   <sbr/>See purple_input_get_error().
 * @timeout_add_seconds: If implemented, should create a callback timer with an
 *                       interval measured in seconds. Analogous to
 *                       g_timeout_add_seconds() in glib.
 *                       <sbr/>This allows UIs to group timers for better power
 *                       efficiency. For this reason, @interval may be rounded
 *                       by up to a second.
 *                       <sbr/>Implementation of this UI op is optional. If it's
 *                       not implemented, calls to purple_timeout_add_seconds()
 *                       will be serviced by @timeout_add.
 *                       <sbr/>See purple_timeout_add_seconds().
 *
 * An abstraction of an application's mainloop; libpurple will use this to
 * watch file descriptors and schedule timed callbacks.  If your application
 * uses the glib mainloop, there is an implementation of this struct in
 * <filename>libpurple/example/nullclient.c</filename> which you can use
 * verbatim.
 */
struct _PurpleEventLoopUiOps
{
	/* TODO Who is responsible for freeing @data? */
	guint (*timeout_add)(guint interval, GSourceFunc function, gpointer data);

	gboolean (*timeout_remove)(guint handle);

	guint (*input_add)(int fd, PurpleInputCondition cond,
	                   PurpleInputFunction func, gpointer user_data);

	gboolean (*input_remove)(guint handle);

	int (*input_get_error)(int fd, int *error);

	guint (*timeout_add_seconds)(guint interval, GSourceFunc function,
	                             gpointer data);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* Event Loop API                                                         */
/**************************************************************************/

/**
 * purple_timeout_add:
 * @interval: The time between calls of the function, in milliseconds.
 * @function: (scope call): The function to call.
 * @data:     data to pass to @function.
 *
 * Creates a callback timer.
 *
 * The timer will repeat until the function returns %FALSE. The
 * first call will be at the end of the first interval.
 *
 * If the timer is in a multiple of seconds, use purple_timeout_add_seconds()
 * instead as it allows UIs to group timers for power efficiency.
 *
 * Returns: A handle to the timer which can be passed to
 *         purple_timeout_remove() to remove the timer.
 */
guint purple_timeout_add(guint interval, GSourceFunc function, gpointer data);

/**
 * purple_timeout_add_seconds:
 * @interval: The time between calls of the function, in seconds.
 * @function: (scope call): The function to call.
 * @data:     data to pass to @function.
 *
 * Creates a callback timer.
 *
 * The timer will repeat until the function returns %FALSE. The
 * first call will be at the end of the first interval.
 *
 * This function allows UIs to group timers for better power efficiency.  For
 * this reason, @interval may be rounded by up to a second.
 *
 * Returns: A handle to the timer which can be passed to
 *         purple_timeout_remove() to remove the timer.
 */
guint purple_timeout_add_seconds(guint interval, GSourceFunc function, gpointer data);

/**
 * purple_timeout_remove:
 * @handle: The handle, as returned by purple_timeout_add().
 *
 * Removes a timeout handler.
 *
 * Returns: %TRUE if the handler was successfully removed.
 */
gboolean purple_timeout_remove(guint handle);

/**
 * purple_input_add:
 * @fd:        The input file descriptor.
 * @cond:      The condition type.
 * @func:      (scope call): The callback function for data.
 * @user_data: User-specified data.
 *
 * Adds an input handler.
 *
 * See g_io_add_watch_full().
 *
 * Returns: The resulting handle (will be greater than 0).
 */
guint purple_input_add(int fd, PurpleInputCondition cond,
                       PurpleInputFunction func, gpointer user_data);

/**
 * purple_input_remove:
 * @handle: The handle of the input handler. Note that this is the return
 *          value from purple_input_add(), <emphasis>not</emphasis> the
 *          file descriptor.
 *
 * Removes an input handler.
 */
gboolean purple_input_remove(guint handle);

/**
 * purple_input_get_error:
 * @fd:        The input file descriptor.
 * @error:     A pointer to an #int which on return will have the error, or
 *             0 if no error.
 *
 * Get the current error status for an input.
 *
 * The return value and error follow getsockopt() with a level of SOL_SOCKET and an
 * option name of SO_ERROR, and this is how the error is determined if the UI does not
 * implement the input_get_error UI op.
 *
 * Returns: 0 if there is no error; -1 if there is an error, in which case
 *          #errno will be set.
 */
int
purple_input_get_error(int fd, int *error);

/**
 * purple_input_pipe:
 * @pipefd: Array used to return file descriptors for both ends of pipe.
 *
 * Creates a pipe - an unidirectional data channel that can be used for
 * interprocess communication.
 *
 * File descriptors for both ends of pipe will be written into provided array.
 * The first one (pipefd[0]) can be used for reading, the second one (pipefd[1])
 * for writing.
 *
 * On Windows it's simulated by creating a pair of connected sockets, on other
 * systems pipe() is used.
 *
 * Returns: 0 on success, -1 on error.
 */
int
purple_input_pipe(int pipefd[2]);



/**************************************************************************/
/* UI Registration Functions                                              */
/**************************************************************************/

/**
 * purple_eventloop_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleEventLoopUiOps boxed structure.
 */
GType purple_eventloop_ui_ops_get_type(void);

/**
 * purple_eventloop_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used for accounts.
 */
void purple_eventloop_set_ui_ops(PurpleEventLoopUiOps *ops);

/**
 * purple_eventloop_get_ui_ops:
 *
 * Returns the UI operations structure used for accounts.
 *
 * Returns: The UI operations structure in use.
 */
PurpleEventLoopUiOps *purple_eventloop_get_ui_ops(void);

G_END_DECLS

#endif /* _PURPLE_EVENTLOOP_H_ */
