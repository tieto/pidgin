/**
 * @file event.h Event API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_EVENT_H_
#define _GAIM_EVENT_H_

#include <glib.h>

/**
 * Event types
 */
typedef enum gaim_event
{
	event_signon = 0,
	event_signoff,
	event_away,
	event_back,
	event_im_recv,
	event_im_send,
	event_buddy_signon,
	event_buddy_signoff,
	event_buddy_away,
	event_buddy_back,
	event_buddy_idle,
	event_buddy_unidle,
	event_blist_update,
	event_chat_invited,
	event_chat_join,
	event_chat_leave,
	event_chat_buddy_join,
	event_chat_buddy_leave,
	event_chat_recv,
	event_chat_send,
	event_warned,
	event_error,
	event_quit,
	event_new_conversation,
	event_set_info,
	event_draw_menu,
	event_im_displayed_sent,
	event_im_displayed_rcvd,
	event_chat_send_invite,
	event_got_typing,
	event_del_conversation,
	event_connecting,
	/* any others? it's easy to add... */

} GaimEvent;

typedef int (*GaimSignalBroadcastFunc)(GaimEvent event, void *data,
									   va_list args);

/**
 * Connects a signal handler to a gaim event.
 *
 * @param module The optional module handle.
 * @param event  The event to connect to.
 * @param func   The callback function.
 * @param data   The data to pass to the callback function.
 *
 * @see gaim_signal_disconnect()
 */
void gaim_signal_connect(void *module, GaimEvent event,
						 void *func, void *data);

/**
 * Disconnects a signal handler from a gaim event.
 *
 * @param module The optional module handle.
 * @param event  The event to disconnect from.
 * @param func   The registered function to disconnect.
 *
 * @see gaim_signal_connect()
 */
void gaim_signal_disconnect(void *module, GaimEvent event,
							void *func);

/**
 * Removes all callbacks associated with a handle.
 *
 * @param handle The handle.
 */
void gaim_signals_disconnect_by_handle(void *handle);

/**
 * Registers a function that re-broadcasts events.
 *
 * @param func The function.
 * @param data Data to be passed to the callback.
 */
void gaim_signals_register_broadcast_func(GaimSignalBroadcastFunc func,
										  void *data);

/**
 * Unregisters a function that re-broadcasts events.
 *
 * @param func The function.
 */
void gaim_signals_unregister_broadcast_func(GaimSignalBroadcastFunc func);

/**
 * Returns a list of all signal callbacks.
 *
 * @return A list of all signal callbacks.
 */
GList *gaim_signals_get_callbacks(void);

/**
 * Broadcasts an event to all registered signal handlers.
 *
 * @param event The event to broadcast
 *
 * @see gaim_signal_connect()
 * @see gaim_signal_disconnect()
 */
int gaim_event_broadcast(GaimEvent event, ...);

/**
 * Returns a human-readable representation of an event name.
 *
 * @param event The event.
 *
 * @return A human-readable string of the name.
 */
const char *gaim_event_get_name(GaimEvent event);

#endif /* _GAIM_EVENT_H_ */
