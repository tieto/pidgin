/**
 * @file event.c Event API
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
#include "event.h"
#include "debug.h"

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

/**
 * A signal callback.
 */
typedef struct
{
	void *handle;                /**< The plugin module handle.         */
	GaimEvent event;             /**< The event type.                   */
	void *function;              /**< The function to call.             */
	void *data;                  /**< The data to pass to the function. */

} GaimSignalCallback;

/**
 * A broadcast function.
 */
typedef struct
{
	GaimSignalBroadcastFunc func;
	void *data;

} GaimSignalBroadcaster;

static GList *callbacks = NULL;
static GList *broadcasters = NULL;

void
gaim_signal_connect(void *handle, GaimEvent event,
					void *func, void *data)
{
	GaimSignalCallback *call;

	g_return_if_fail(func != NULL);

	call = g_new0(GaimSignalCallback, 1);
	call->handle   = handle;
	call->event    = event;
	call->function = func;
	call->data     = data;

	callbacks = g_list_append(callbacks, call);

	gaim_debug(GAIM_DEBUG_INFO, "signals",
			   "Adding callback %d\n", g_list_length(callbacks));
}

void
gaim_signal_disconnect(void *handle, GaimEvent event, void *func)
{
	GList *c, *next_c;
	GaimSignalCallback *g = NULL;

	g_return_if_fail(func != NULL);

	for (c = callbacks; c != NULL; c = next_c) {
		next_c = c->next;
		g = (GaimSignalCallback *)c->data;

		if (handle == g->handle && func == g->function) {
			callbacks = g_list_remove(callbacks, c->data);
			g_free(g);
		}
	}
}

void
gaim_signals_disconnect_by_handle(void *handle)
{
	GList *c, *c_next;
	GaimSignalCallback *g;

	g_return_if_fail(handle != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "signals",
			   "Disconnecting callbacks. %d to search.\n",
			   g_list_length(callbacks));

	for (c = callbacks; c != NULL; c = c_next) {
		c_next = c->next;
		g = (GaimSignalCallback *)c->data;

		if (g->handle == handle) {
			callbacks = g_list_remove(callbacks, c->data);
			g_free(g);

			gaim_debug(GAIM_DEBUG_INFO, "signals",
					   "Removing callback. %d remain.\n",
					   g_list_length(callbacks));
		}
	}
}

void
gaim_signals_register_broadcast_func(GaimSignalBroadcastFunc func,
									 void *data)
{
	GaimSignalBroadcaster *broadcaster;

	g_return_if_fail(func != NULL);

	broadcaster = g_new0(GaimSignalBroadcaster, 1);

	broadcaster->func = func;
	broadcaster->data = data;

	broadcasters = g_list_append(broadcasters, broadcaster);
}

void
gaim_signals_unregister_broadcast_func(GaimSignalBroadcastFunc func)
{
	GList *l;
	GaimSignalBroadcaster *broadcaster;

	g_return_if_fail(func != NULL);

	for (l = broadcasters; l != NULL; l = l->next) {
		broadcaster = l->data;

		if (broadcaster->func == func) {
			broadcasters = g_list_remove(broadcasters, broadcaster);

			g_free(broadcaster);

			break;
		}
	}
}

GList *
gaim_signals_get_callbacks(void)
{
	return callbacks;
}

int
gaim_event_broadcast(GaimEvent event, ...)
{
	GList *c;
	GList *l;
	GaimSignalCallback *g;
	GaimSignalBroadcaster *broadcaster;
	int retval = 0;
	va_list arrg;
	void    *arg1 = NULL, *arg2 = NULL, *arg3 = NULL, *arg4 = NULL;

	for (c = callbacks; c != NULL; c = c->next) {
		void (*cbfunc)(void *, ...);

		g = (GaimSignalCallback *)c->data;

		if (g->event == event && g->function != NULL) {
			time_t time;
			int id;
			cbfunc = g->function;

			va_start(arrg, event);

			switch (event) {
				/* no args */
				case event_blist_update:
				case event_quit:
					cbfunc(g->data);
					break;

				/* one arg */
				case event_signon:
				case event_signoff:
				case event_new_conversation:
				case event_del_conversation:
			        case event_conversation_switch:
				case event_error:
				case event_connecting:
					arg1 = va_arg(arrg, void *);
					cbfunc(arg1, g->data);
					break;

				/* two args */
				case event_buddy_signon:
				case event_buddy_signoff:
				case event_buddy_away:
				case event_buddy_back:
				case event_buddy_idle:
				case event_buddy_unidle:
				case event_set_info:
				case event_draw_menu:
				case event_got_typing:
					arg1 = va_arg(arrg, void *);
					arg2 = va_arg(arrg, void *);
					cbfunc(arg1, arg2, g->data);
					break;

				case event_chat_leave:
					arg1 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					cbfunc(arg1, id, g->data);
					break;

				/* three args */
				case event_im_send:
				case event_im_displayed_sent:
				case event_away:
					arg1 = va_arg(arrg, void *);
					arg2 = va_arg(arrg, void *);
					arg3 = va_arg(arrg, void *);
					cbfunc(arg1, arg2, arg3, g->data);
					break;

				case event_chat_buddy_join:
				case event_chat_buddy_leave:
				case event_chat_send:
				case event_chat_join:
					arg1 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					arg3 = va_arg(arrg, void*);
					cbfunc(arg1, id, arg3, g->data);
					break;

				case event_warned:
					arg1 = va_arg(arrg, void*);
					arg2 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					cbfunc(arg1, arg2, id, g->data);
					break;

				/* four args */
				case event_im_recv:
				case event_chat_invited:
					arg1 = va_arg(arrg, void *);
					arg2 = va_arg(arrg, void *);
					arg3 = va_arg(arrg, void *);
					arg4 = va_arg(arrg, void *);
					cbfunc(arg1, arg2, arg3, arg4, g->data);
					break;

				case event_chat_recv:
				case event_chat_send_invite:
					arg1 = va_arg(arrg, void *);
					id = va_arg(arrg, int);

					arg3 = va_arg(arrg, void *);
					arg4 = va_arg(arrg, void *);
					cbfunc(arg1, id, arg3, arg4, g->data);
					break;

				/* five args */
				case event_im_displayed_rcvd:
					arg1 = va_arg(arrg, void *);
					arg2 = va_arg(arrg, void *);
					arg3 = va_arg(arrg, void *);
					arg4 = va_arg(arrg, void *);
					time = va_arg(arrg, time_t);
					cbfunc(arg1, arg2, arg3, arg4, time, g->data);
					break;

				default:
					gaim_debug(GAIM_DEBUG_WARNING, "events",
							   "Unknown event %d\n", event);
					break;
			}

			va_end(arrg);
		}
	}

	for (l = broadcasters; l != NULL && retval == 0; l = l->next) {
		broadcaster = l->data;

		va_start(arrg, event);
		retval = broadcaster->func(event, broadcaster->data, arrg);
	}

	return retval;
}

const char *
gaim_event_get_name(GaimEvent event)
{
	static char buf[128];

	switch (event) {
		case event_signon:
			snprintf(buf, sizeof(buf), "event_signon");
			break;
		case event_signoff:
			snprintf(buf, sizeof(buf), "event_signoff");
			break;
		case event_away:
			snprintf(buf, sizeof(buf), "event_away");
			break;
		case event_back:
			snprintf(buf, sizeof(buf), "event_back");
			break;
		case event_im_recv:
			snprintf(buf, sizeof(buf), "event_im_recv");
			break;
		case event_im_send:
			snprintf(buf, sizeof(buf), "event_im_send");
			break;
		case event_buddy_signon:
			snprintf(buf, sizeof(buf), "event_buddy_signon");
			break;
		case event_buddy_signoff:
			snprintf(buf, sizeof(buf), "event_buddy_signoff");
			break;
		case event_buddy_away:
			snprintf(buf, sizeof(buf), "event_buddy_away");
			break;
		case event_buddy_back:
			snprintf(buf, sizeof(buf), "event_buddy_back");
			break;
		case event_buddy_idle:
			snprintf(buf, sizeof(buf), "event_buddy_idle");
			break;
		case event_buddy_unidle:
			snprintf(buf, sizeof(buf), "event_buddy_unidle");
			break;
		case event_blist_update:
			snprintf(buf, sizeof(buf), "event_blist_update");
			break;
		case event_chat_invited:
			snprintf(buf, sizeof(buf), "event_chat_invited");
			break;
		case event_chat_join:
			snprintf(buf, sizeof(buf), "event_chat_join");
			break;
		case event_chat_leave:
			snprintf(buf, sizeof(buf), "event_chat_leave");
			break;
		case event_chat_buddy_join:
			snprintf(buf, sizeof(buf), "event_chat_buddy_join");
			break;
		case event_chat_buddy_leave:
			snprintf(buf, sizeof(buf), "event_chat_buddy_leave");
			break;
		case event_chat_recv:
			snprintf(buf, sizeof(buf), "event_chat_recv");
			break;
		case event_chat_send:
			snprintf(buf, sizeof(buf), "event_chat_send");
			break;
		case event_warned:
			snprintf(buf, sizeof(buf), "event_warned");
			break;
		case event_error:
			snprintf(buf, sizeof(buf), "event_error");
			break;
		case event_quit:
			snprintf(buf, sizeof(buf), "event_quit");
			break;
		case event_new_conversation:
			snprintf(buf, sizeof(buf), "event_new_conversation");
			break;
		case event_set_info:
			snprintf(buf, sizeof(buf), "event_set_info");
			break;
		case event_draw_menu:
			snprintf(buf, sizeof(buf), "event_draw_menu");
			break;
		case event_im_displayed_sent:
			snprintf(buf, sizeof(buf), "event_im_displayed_sent");
			break;
		case event_im_displayed_rcvd:
			snprintf(buf, sizeof(buf), "event_im_displayed_rcvd");
			break;
		case event_chat_send_invite:
			snprintf(buf, sizeof(buf), "event_chat_send_invite");
			break;
		case event_got_typing:
			snprintf(buf, sizeof(buf), "event_got_typing");
			break;
		case event_del_conversation:
			snprintf(buf, sizeof(buf), "event_del_conversation");
			break;
		case event_connecting:
			snprintf(buf, sizeof(buf), "event_connecting");
			break;
		case event_conversation_switch:
			snprintf(buf, sizeof(buf), "event_conversation_switch");
			break;				
		default:
			snprintf(buf, sizeof(buf), "event_unknown");
			break;
	}

	return buf;
}


