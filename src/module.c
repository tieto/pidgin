/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 * ----------------
 * The Plug-in plug
 *
 * Plugin support is currently being maintained by Mike Saraf
 * msaraf@dwc.edu
 *
 * Well, I didn't see any work done on it for a while, so I'm going to try
 * my hand at it. - Eric warmenhoven@yahoo.com
 *
 * Mike is my roomate.  I can assure you that he's lazy :-P  -- Rob rob@marko.net
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gaim.h"

#ifdef GAIM_PLUGINS

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------ Global Variables ----------------------- */

GList *plugins = NULL;
GList *callbacks = NULL;

char *last_dir = NULL;

/* --------------- Function Declarations --------------------- */

struct gaim_plugin *  load_plugin(char *);
              void  unload_plugin(struct gaim_plugin *p);
struct gaim_plugin *reload_plugin(struct gaim_plugin *p);

void gaim_signal_connect(GModule *, enum gaim_event, void *, void *);
void gaim_signal_disconnect(GModule *, enum gaim_event, void *);
void gaim_plugin_unload(GModule *);

/* --------------- Static Function Declarations ------------- */

static void plugin_remove_callbacks(GModule *);

/* ------------------ Code Below ---------------------------- */

struct gaim_plugin *load_plugin(char *filename)
{
	struct gaim_plugin *plug;
	GList *c = plugins;
	char *(*gaim_plugin_init)(GModule *);
	char *(*cfunc)();
	char *error;
	char *retval;

	if (!g_module_supported())
		return NULL;
	if (filename && !strlen(filename))
		return NULL;

	while (filename && c) {
		plug = (struct gaim_plugin *)c->data;
		if (!strcmp(filename, g_module_name(plug->handle))) {
			/* just need to reload plugin */
			return reload_plugin(plug);
		} else
			c = g_list_next(c);
	}
	plug = g_malloc(sizeof *plug);

	if (last_dir)
		g_free(last_dir);
	last_dir = g_dirname(filename);

	debug_printf("Loading %s\n", filename);
	plug->handle = g_module_open(filename, 0);
	if (!plug->handle) {
		error = (char *)g_module_error();
		do_error_dialog(_("Gaim was unable to load your plugin."), error, GAIM_ERROR);
		g_free(plug);
		return NULL;
	}

	if (!g_module_symbol(plug->handle, "gaim_plugin_init", (gpointer *)&gaim_plugin_init)) {
		do_error_dialog(_("Gaim was unable to load your plugin."), g_module_error(), GAIM_ERROR);
		g_module_close(plug->handle);
		g_free(plug);
		return NULL;
	}

	retval = gaim_plugin_init(plug->handle);
	debug_printf("loaded plugin returned %s\n", retval ? retval : "NULL");
	if (retval) {
		plugin_remove_callbacks(plug->handle);
		do_error_dialog("Gaim was unable to load your plugin.", retval, GAIM_ERROR);
		g_module_close(plug->handle);
		g_free(plug);
		return NULL;
	}

	plugins = g_list_append(plugins, plug);

	if (g_module_symbol(plug->handle, "name", (gpointer *)&cfunc)) {
		plug->name = cfunc();
	} else {
		plug->name = NULL;
	}

	if (g_module_symbol(plug->handle, "description", (gpointer *)&cfunc))
		plug->description = cfunc();
	else
		plug->description = NULL;

	save_prefs();
	return plug;
}

static void unload_gaim_plugin(struct gaim_plugin *p)
{
	void (*gaim_plugin_remove)();

	debug_printf("Unloading %s\n", g_module_name(p->handle));

	/* Attempt to call the plugin's remove function (if there) */
	if (g_module_symbol(p->handle, "gaim_plugin_remove", (gpointer *)&gaim_plugin_remove))
		gaim_plugin_remove();

	plugin_remove_callbacks(p->handle);

	plugins = g_list_remove(plugins, p);
	g_free(p);
	save_prefs();
}

void unload_plugin(struct gaim_plugin *p)
{
	GModule *handle = p->handle;
	unload_gaim_plugin(p);
	g_module_close(handle);
}

static gboolean unload_timeout(gpointer handle)
{
	g_module_close(handle);
	return FALSE;
}

void gaim_plugin_unload(GModule *handle)
{
	GList *pl = plugins;
	struct gaim_plugin *p = NULL;
	void (*gaim_plugin_remove)();

	while (pl) {
		p = pl->data;
		if (p->handle == handle)
			break;
		pl = pl->next;
	}
	if (!pl)
		return;

	debug_printf("Unloading %s\n", g_module_name(p->handle));

	if (g_module_symbol(p->handle, "gaim_plugin_remove", (gpointer *)&gaim_plugin_remove))
		gaim_plugin_remove();
	plugin_remove_callbacks(p->handle);
	plugins = g_list_remove(plugins, p);
	g_free(p);
	/* XXX CUI need to tell UI what happened, but not like this */
	update_show_plugins();

	g_timeout_add(5000, unload_timeout, handle);
}

/* Do unload/load cycle of plugin. */
struct gaim_plugin *reload_plugin(struct gaim_plugin *p)
{
	char file[1024];
	GModule *handle = p->handle;

	strncpy(file, g_module_name(handle), sizeof(file));
	file[sizeof(file) - 1] = '\0';

	debug_printf("Reloading %s\n", file);

	/* Unload */
	unload_plugin(p);

	/* Load */
	return load_plugin(file);
}

/* Remove all callbacks associated with plugin handle */
static void plugin_remove_callbacks(GModule *handle)
{
	GList *c = callbacks;
	struct gaim_callback *g;

	debug_printf("%d callbacks to search\n", g_list_length(callbacks));

	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->handle == handle) {
			c = g_list_next(c);
			callbacks = g_list_remove(callbacks, (gpointer)g);
			debug_printf("Removing callback, %d remain\n", g_list_length(callbacks));
		} else
			c = g_list_next(c);
	}
}

void gaim_signal_connect(GModule *handle, enum gaim_event which, void *func, void *data)
{
	struct gaim_callback *call = g_new0(struct gaim_callback, 1);
	call->handle = handle;
	call->event = which;
	call->function = func;
	call->data = data;

	callbacks = g_list_append(callbacks, call);
	debug_printf("Adding callback %d\n", g_list_length(callbacks));
}

void gaim_signal_disconnect(GModule *handle, enum gaim_event which, void *func)
{
	GList *c = callbacks;
	struct gaim_callback *g = NULL;

	while (c) {
		g = (struct gaim_callback *)c->data;
		if (handle == g->handle && func == g->function) {
			callbacks = g_list_remove(callbacks, c->data);
			g_free(g);
			c = callbacks;
			if (c == NULL)
				break;
		}
		c = g_list_next(c);
	}
}

#endif /* GAIM_PLUGINS */

char *event_name(enum gaim_event event)
{
	static char buf[128];
	switch (event) {
	case event_signon:
		sprintf(buf, "event_signon");
		break;
	case event_signoff:
		sprintf(buf, "event_signoff");
		break;
	case event_away:
		sprintf(buf, "event_away");
		break;
	case event_back:
		sprintf(buf, "event_back");
		break;
	case event_im_recv:
		sprintf(buf, "event_im_recv");
		break;
	case event_im_send:
		sprintf(buf, "event_im_send");
		break;
	case event_buddy_signon:
		sprintf(buf, "event_buddy_signon");
		break;
	case event_buddy_signoff:
		sprintf(buf, "event_buddy_signoff");
		break;
	case event_buddy_away:
		sprintf(buf, "event_buddy_away");
		break;
	case event_buddy_back:
		sprintf(buf, "event_buddy_back");
		break;
	case event_buddy_idle:
		sprintf(buf, "event_buddy_idle");
		break;
	case event_buddy_unidle:
		sprintf(buf, "event_buddy_unidle");
		break;
	case event_blist_update:
		sprintf(buf, "event_blist_update");
		break;
	case event_chat_invited:
		sprintf(buf, "event_chat_invited");
		break;
	case event_chat_join:
		sprintf(buf, "event_chat_join");
		break;
	case event_chat_leave:
		sprintf(buf, "event_chat_leave");
		break;
	case event_chat_buddy_join:
		sprintf(buf, "event_chat_buddy_join");
		break;
	case event_chat_buddy_leave:
		sprintf(buf, "event_chat_buddy_leave");
		break;
	case event_chat_recv:
		sprintf(buf, "event_chat_recv");
		break;
	case event_chat_send:
		sprintf(buf, "event_chat_send");
		break;
	case event_warned:
		sprintf(buf, "event_warned");
		break;
	case event_error:
		sprintf(buf, "event_error");
		break;
	case event_quit:
		sprintf(buf, "event_quit");
		break;
	case event_new_conversation:
		sprintf(buf, "event_new_conversation");
		break;
	case event_set_info:
		sprintf(buf, "event_set_info");
		break;
	case event_draw_menu:
		sprintf(buf, "event_draw_menu");
		break;
	case event_im_displayed_sent:
		sprintf(buf, "event_im_displayed_sent");
		break;
	case event_im_displayed_rcvd:
		sprintf(buf, "event_im_displayed_rcvd");
		break;
	case event_chat_send_invite:
		sprintf(buf, "event_chat_send_invite");
		break;
	case event_got_typing:
		sprintf(buf, "event_got_typing");
		break;
	default:
		sprintf(buf, "event_unknown");
		break;
	}
	return buf;
}

static void debug_event(enum gaim_event event, void *arg1, void *arg2, void *arg3, void *arg4)
{
	if (!opt_debug && !(misc_options & OPT_MISC_DEBUG))
		return;

	switch (event) {
	        case event_quit:
			debug_printf("%s\n", event_name(event));
			break;
		case event_signon:
		case event_signoff:
			debug_printf("%s: %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username);
			break;
		case event_new_conversation:
			debug_printf("event_new_conversation: %s\n", (char *)arg1);
			break;
		case event_error:
			debug_printf("event_error: %d\n", (int)arg1);
			break;
		case event_buddy_signon:
		case event_buddy_signoff:
		case event_buddy_away:
		case event_buddy_back:
		case event_buddy_idle:
	        case event_buddy_unidle:
	        case event_set_info:
	        case event_got_typing:
		debug_printf("%s: %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username, (char *)arg2);
			break;
		case event_chat_leave:
			debug_printf("event_chat_leave: %s %d\n",
					((struct gaim_connection *)arg1)->username, (int)arg2);
			break;
		case event_im_send:
		case event_im_displayed_sent:
			debug_printf("%s: %s %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(char *)arg2, *(char **)arg3 ? *(char **)arg3 : "");
			break;
		case event_chat_join:
		case event_chat_buddy_join:
		case event_chat_buddy_leave:
			debug_printf("%s: %s %d %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(int)arg2, (char *)arg3);
			break;
		case event_chat_send:
			debug_printf("%s: %s %d %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(int)arg2, *(char **)arg3 ? *(char **)arg3 : "");
			break;
		case event_away:
			debug_printf("%s: %s %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(char *)arg2, (char *)arg3 ? (char *)arg3 : "");
			break;
		case event_warned:
			debug_printf("%s: %s %s %d\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(char *)arg2 ? (char *)arg2 : "", (int)arg3);
			break;
		case event_im_recv:
			debug_printf("%s: %s %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					*(char **)arg2 ? *(char **)arg2 : "",
					*(char **)arg3 ? *(char **)arg3 : "");
			break;
		case event_im_displayed_rcvd:
			debug_printf("%s: %s %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(char *)arg2 ? (char *)arg2 : "",
					(char *)arg3 ? (char *)arg3 : "");
			break;
		case event_chat_recv:
			debug_printf("%s: %s %d %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(int)arg2,
					*(char **)arg3 ? *(char **)arg3 : "",
					*(char **)arg4 ? *(char **)arg4 : "");
			break;
		case event_chat_send_invite:
			debug_printf("%s: %s %d %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(int)arg2, (char *)arg3,
					*(char **)arg4 ? *(char **)arg4 : "");
			break;
		case event_chat_invited:
			debug_printf("%s: %s %s %s %s\n", event_name(event),
					((struct gaim_connection *)arg1)->username,
					(char *)arg2, (char *)arg3,
					(char *)arg4 ? (char *)arg4 : "");
			break;
		default:
			break;
	}
}

int plugin_event(enum gaim_event event, void *arg1, void *arg2, void *arg3, void *arg4)
{
#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
#endif

	debug_event(event, arg1, arg2, arg3, arg4);

#ifdef GAIM_PLUGINS
	while (c) {
		void (*zero)(void *);
		void (*one)(void *, void *);
		void (*two)(void *, void *, void *);
		void (*three)(void *, void *, void *, void *);
		void (*four)(void *, void *, void *, void *, void *);

		g = (struct gaim_callback *)c->data;
		if (g->event == event && g->function != NULL) {
			switch (event) {

				/* no args */
			case event_blist_update:
			case event_quit:
				zero = g->function;
				zero(g->data);
				break;

				/* one arg */
			case event_signon:
			case event_signoff:
			case event_new_conversation:
			case event_del_conversation:
			case event_error:
				one = g->function;
				one(arg1, g->data);
				break;

				/* two args */
			case event_buddy_signon:
			case event_buddy_signoff:
			case event_buddy_away:
			case event_buddy_back:
			case event_buddy_idle:
			case event_buddy_unidle:
			case event_chat_leave:
			case event_set_info:
			case event_draw_menu:
			case event_got_typing:
				two = g->function;
				two(arg1, arg2, g->data);
				break;

				/* three args */
			case event_im_send:
			case event_im_displayed_sent:
			case event_chat_join:
			case event_chat_buddy_join:
			case event_chat_buddy_leave:
			case event_chat_send:
			case event_away:
			case event_back:
			case event_warned:
				three = g->function;
				three(arg1, arg2, arg3, g->data);
				break;

				/* four args */
			case event_im_recv:
			case event_chat_recv:
			case event_im_displayed_rcvd:
			case event_chat_send_invite:
			case event_chat_invited:
				four = g->function;
				four(arg1, arg2, arg3, arg4, g->data);
				break;

			default:
				debug_printf("unknown event %d\n", event);
				break;
			}
		}
		c = c->next;
	}
#endif /* GAIM_PLUGINS */
#ifdef USE_PERL
	return perl_event(event, arg1, arg2, arg3, arg4);
#else
	return 0;
#endif
}

/* Calls the gaim_plugin_remove function in any loaded plugin that has one */
#ifdef GAIM_PLUGINS
void remove_all_plugins()
{
	GList *c = plugins;
	struct gaim_plugin *p;
	void (*gaim_plugin_remove)();

	while (c) {
		p = (struct gaim_plugin *)c->data;
		if (g_module_symbol(p->handle, "gaim_plugin_remove", (gpointer *)&gaim_plugin_remove))
			gaim_plugin_remove();
		g_free(p);
		c = c->next;
	}
}
#endif
