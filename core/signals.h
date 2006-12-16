/**
 * @file signals.h Signal API
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
#ifndef _GAIM_SIGNALS_H_
#define _GAIM_SIGNALS_H_

#include <glib.h>
#include "value.h"

#define GAIM_CALLBACK(func) ((GaimCallback)func)

typedef void (*GaimCallback)(void);
typedef void (*GaimSignalMarshalFunc)(GaimCallback cb, va_list args,
									  void *data, void **return_val);

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Signal API                                                      */
/**************************************************************************/
/*@{*/

/**
 * Signal Connect Priorities
 */
#define GAIM_SIGNAL_PRIORITY_DEFAULT     0
#define GAIM_SIGNAL_PRIORITY_HIGHEST  9999
#define GAIM_SIGNAL_PRIORITY_LOWEST  -9999

/**
 * Registers a signal in an instance.
 *
 * @param instance   The instance to register the signal for.
 * @param signal     The signal name.
 * @param marshal    The marshal function.
 * @param ret_value  The return value type, or NULL for no return value.
 * @param num_values The number of values to be passed to the callbacks.
 * @param ...        The values to pass to the callbacks.
 *
 * @return The signal ID local to that instance, or 0 if the signal
 *         couldn't be registered.
 *
 * @see GaimValue
 */
gulong gaim_signal_register(void *instance, const char *signal,
							GaimSignalMarshalFunc marshal,
							GaimValue *ret_value, int num_values, ...);

/**
 * Unregisters a signal in an instance.
 *
 * @param instance The instance to unregister the signal for.
 * @param signal   The signal name.
 */
void gaim_signal_unregister(void *instance, const char *signal);

/**
 * Unregisters all signals in an instance.
 *
 * @param instance The instance to unregister the signal for.
 */
void gaim_signals_unregister_by_instance(void *instance);

/**
 * Returns a list of value types used for a signal.
 *
 * @param instance   The instance the signal is registered to.
 * @param signal     The signal.
 * @param ret_value  The return value from the last signal handler.
 * @param num_values The returned number of values.
 * @param values     The returned list of values.
 */
void gaim_signal_get_values(void *instance, const char *signal,
							GaimValue **ret_value,
							int *num_values, GaimValue ***values);

/**
 * Connects a signal handler to a signal for a particular object.
 *
 * Take care not to register a handler function twice. Gaim will
 * not correct any mistakes for you in this area.
 *
 * @param instance The instance to connect to.
 * @param signal   The name of the signal to connect.
 * @param handle   The handle of the receiver.
 * @param func     The callback function.
 * @param data     The data to pass to the callback function.
 * @param priority The priority with which the handler should be called. Signal handlers are called
 *                 in order from GAIM_SIGNAL_PRIORITY_LOWEST to GAIM_SIGNAL_PRIORITY_HIGHEST.
 *
 * @return The signal handler ID.
 *
 * @see gaim_signal_disconnect()
 */
gulong gaim_signal_connect_priority(void *instance, const char *signal,
				   void *handle, GaimCallback func, void *data, int priority);

/**
 * Connects a signal handler to a signal for a particular object.
 * (priority defaults to 0)
 * 
 * Take care not to register a handler function twice. Gaim will
 * not correct any mistakes for you in this area.
 *
 * @param instance The instance to connect to.
 * @param signal   The name of the signal to connect.
 * @param handle   The handle of the receiver.
 * @param func     The callback function.
 * @param data     The data to pass to the callback function.
 *
 * @return The signal handler ID.
 *
 * @see gaim_signal_disconnect()
 */
gulong gaim_signal_connect(void *instance, const char *signal,
						   void *handle, GaimCallback func, void *data);

/**
 * Connects a signal handler to a signal for a particular object.
 *
 * The signal handler will take a va_args of arguments, instead of
 * individual arguments.
 *
 * Take care not to register a handler function twice. Gaim will
 * not correct any mistakes for you in this area.
 *
 * @param instance The instance to connect to.
 * @param signal   The name of the signal to connect.
 * @param handle   The handle of the receiver.
 * @param func     The callback function.
 * @param data     The data to pass to the callback function.
 * @param priority The order in which the signal should be added to the list
 *
 * @return The signal handler ID.
 *
 * @see gaim_signal_disconnect()
 */
gulong gaim_signal_connect_priority_vargs(void *instance, const char *signal,
					void *handle, GaimCallback func, void *data, int priority);

/**
 * Connects a signal handler to a signal for a particular object.
 * (priority defaults to 0)
 * The signal handler will take a va_args of arguments, instead of
 * individual arguments.
 *
 * Take care not to register a handler function twice. Gaim will
 * not correct any mistakes for you in this area.
 *
 * @param instance The instance to connect to.
 * @param signal   The name of the signal to connect.
 * @param handle   The handle of the receiver.
 * @param func     The callback function.
 * @param data     The data to pass to the callback function.
 *
 * @return The signal handler ID.
 *
 * @see gaim_signal_disconnect()
 */
gulong gaim_signal_connect_vargs(void *instance, const char *signal,
								 void *handle, GaimCallback func, void *data);

/**
 * Disconnects a signal handler from a signal on an object.
 *
 * @param instance The instance to disconnect from.
 * @param signal   The name of the signal to disconnect.
 * @param handle   The handle of the receiver.
 * @param func     The registered function to disconnect.
 *
 * @see gaim_signal_connect()
 */
void gaim_signal_disconnect(void *instance, const char *signal,
							void *handle, GaimCallback func);

/**
 * Removes all callbacks associated with a receiver handle.
 *
 * @param handle The receiver handle.
 */
void gaim_signals_disconnect_by_handle(void *handle);

/**
 * Emits a signal.
 *
 * @param instance The instance emitting the signal.
 * @param signal   The signal being emitted.
 *
 * @see gaim_signal_connect()
 * @see gaim_signal_disconnect()
 */
void gaim_signal_emit(void *instance, const char *signal, ...);

/**
 * Emits a signal, using a va_list of arguments.
 *
 * @param instance The instance emitting the signal.
 * @param signal   The signal being emitted.
 * @param args     The arguments list.
 *
 * @see gaim_signal_connect()
 * @see gaim_signal_disconnect()
 */
void gaim_signal_emit_vargs(void *instance, const char *signal, va_list args);

/**
 * Emits a signal and returns the first non-NULL return value.
 *
 * Further signal handlers are NOT called after a handler returns
 * something other than NULL.
 *
 * @param instance The instance emitting the signal.
 * @param signal   The signal being emitted.
 *
 * @return The first non-NULL return value
 */
void *gaim_signal_emit_return_1(void *instance, const char *signal, ...);

/**
 * Emits a signal and returns the first non-NULL return value.
 *
 * Further signal handlers are NOT called after a handler returns
 * something other than NULL.
 *
 * @param instance The instance emitting the signal.
 * @param signal   The signal being emitted.
 * @param args     The arguments list.
 *
 * @return The first non-NULL return value
 */
void *gaim_signal_emit_vargs_return_1(void *instance, const char *signal,
									  va_list args);

/**
 * Initializes the signals subsystem.
 */
void gaim_signals_init(void);

/**
 * Uninitializes the signals subsystem.
 */
void gaim_signals_uninit(void);

/*@}*/

/**************************************************************************/
/** @name Marshal Functions                                               */
/**************************************************************************/
/*@{*/

void gaim_marshal_VOID(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__INT_INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_INT_INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_UINT_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_VOID__POINTER_POINTER_POINTER_UINT_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);

void gaim_marshal_INT__INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_INT__INT_INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_INT__POINTER_POINTER_POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);

void gaim_marshal_BOOLEAN__POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_UINT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);

void gaim_marshal_BOOLEAN__INT_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);

void gaim_marshal_POINTER__POINTER_INT(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_POINTER__POINTER_INT64(
		GaimCallback cb, va_list args, void *data, void **return_val);
void gaim_marshal_POINTER__POINTER_POINTER(
		GaimCallback cb, va_list args, void *data, void **return_val);
/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_SIGNALS_H_ */
