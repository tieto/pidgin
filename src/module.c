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
#include "prpl.h"

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

/* ------------------ Global Variables ----------------------- */

GList *plugins = NULL;
GList *probed_plugins = NULL;
GList *callbacks = NULL;

char *last_dir = NULL;

/* --------------- Function Declarations --------------------- */

struct gaim_plugin *  load_plugin(const char *);
#ifdef GAIM_PLUGINS
              void  unload_plugin(struct gaim_plugin *p);
void gaim_signal_connect(GModule *, enum gaim_event, void *, void *);
void gaim_signal_disconnect(GModule *, enum gaim_event, void *);
void gaim_plugin_unload(GModule *);

/* --------------- Static Function Declarations ------------- */

static void plugin_remove_callbacks(GModule *);
#endif
/* ------------------ Code Below ---------------------------- */

static int is_so_file(char *filename, char *ext)
{
	int len;
	if (!filename) return 0;
	if (!filename[0]) return 0;
	len = strlen(filename);
	len -= strlen(ext);
	if (len < 0) return 0;
	return (!strncmp(filename + len, ext, strlen(ext)));
}

void gaim_probe_plugins() {
	GDir *dir;
	const gchar *file;
	gchar *path;
	/*struct gaim_plugin_description *plugdes;*/
	struct gaim_plugin *plug;
	char *probedirs[3];
	int l;
#if GAIM_PLUGINS     
	char *(*gaim_plugin_init)(GModule *);
	char *(*gaim_prpl_init)(struct prpl *);
	char *(*cfunc)();
	struct prpl * new_prpl;
	struct gaim_plugin_description *(*desc)();
	GModule *handle;
#endif

	probedirs[0] = LIBDIR;
	probedirs[1] = gaim_user_dir();
	probedirs[2] = 0;

	for (l=0; probedirs[l]; l++) {
		dir = g_dir_open(probedirs[l], 0, NULL);
		if (dir) {
			while ((file = g_dir_read_name(dir))) {
#ifdef GAIM_PLUGINS
				if (is_so_file((char*)file, 
#ifndef _WIN32
					       ".so"
#else
					       ".dll"
#endif
					       ) && g_module_supported()) {
					path = g_build_filename(probedirs[l], file, NULL);
					handle = g_module_open(path, 0);
					if (!handle) {
						debug_printf("%s is unloadable: %s\n", file, g_module_error());
						continue;
					}
					if (g_module_symbol(handle, "gaim_prpl_init", (gpointer *)&gaim_prpl_init)) {
						plug = g_new0(struct gaim_plugin, 1);
						g_snprintf(plug->path, sizeof(plug->path), path);
						plug->type = plugin;

						new_prpl = g_new0(struct prpl, 1);
						new_prpl->plug = plug;
						gaim_prpl_init(new_prpl);
						if (new_prpl->protocol == PROTO_ICQ ||
						    find_prpl(new_prpl->protocol)) {
							/* Nothing to see here--move along, move along */
							unload_protocol(new_prpl);
							continue;
						}
						protocols = g_slist_insert_sorted(protocols, new_prpl, (GCompareFunc)proto_compare);
						g_module_close(handle);
						continue;
					}
					
					if (!g_module_symbol(handle, "gaim_plugin_init", (gpointer *)&gaim_plugin_init)) {
						debug_printf("%s is unloadable %s\n", file, g_module_error());
						g_module_close(handle);
						continue;
					}
					plug = g_new0(struct gaim_plugin, 1);
					g_snprintf(plug->path, sizeof(plug->path), path);
					plug->type = plugin;
					g_free(path);
					if (g_module_symbol(handle, "gaim_plugin_desc", (gpointer *)&desc)) {
						memcpy(&(plug->desc), desc(), sizeof(plug->desc));
					} else {
						if (g_module_symbol(handle, "name", (gpointer *)&cfunc)) {
							plug->desc.name = g_strdup(cfunc());
						}
						if (g_module_symbol(handle, "description", (gpointer *)&cfunc)) {
							plug->desc.description = g_strdup(cfunc());
						}
					}
					probed_plugins = g_list_append(probed_plugins, plug);
					g_module_close(handle);
				}
#endif
#ifdef USE_PERL
				if (is_so_file((char*)file, ".pl")) {
					path = g_build_filename(probedirs[l], file, NULL);
					plug = probe_perl(path);
					if (plug) 
						probed_plugins = g_list_append(probed_plugins, plug);
					g_free(path);
				}
#endif
			}
			g_dir_close(dir);
		}
	}
}

#ifdef GAIM_PLUGINS
struct gaim_plugin *load_plugin(const char *filename)
{
	struct gaim_plugin *plug=NULL;
	struct gaim_plugin_description *desc;
	struct gaim_plugin_description *(*gaim_plugin_desc)();
	char *(*cfunc)();
	/*GList *c = plugins;*/
	GList *p = probed_plugins;
	char *(*gaim_plugin_init)(GModule *);
	char *error=NULL;
	char *retval;
	gboolean newplug = FALSE;

	if (!g_module_supported())
		return NULL;
	if (!filename || !strlen(filename))
		return NULL;

#ifdef USE_PERL
	if (is_so_file((char*)filename, ".pl")) {
		/* perl_load_file is returning an int.. this should be fixed */
		return (struct gaim_plugin *)perl_load_file((char*)filename);
	}
#endif

	while (filename && p) {
		plug = (struct gaim_plugin *)p->data;
		if (!strcmp(filename, plug->path)) 
			break;
		p = p->next;
	}
	
	if (plug && plug->handle) {
		return plug;
	}
	    
	if (!plug) {
		plug = g_new0(struct gaim_plugin, 1);
		g_snprintf(plug->path, sizeof(plug->path), filename);
		newplug = TRUE;
	}
			
	debug_printf("Loading %s\n", filename);
	plug->handle = g_module_open(filename, 0);
	if (!plug->handle) {
		error = (char *)g_module_error();
		plug->handle = NULL;
		g_snprintf(plug->error, sizeof(plug->error), error);
		return NULL;
	}
	
	if (!g_module_symbol(plug->handle, "gaim_plugin_init", (gpointer *)&gaim_plugin_init)) {
		g_module_close(plug->handle);
		plug->handle = NULL;
		g_snprintf(plug->error, sizeof(plug->error), error);
		return NULL;
	}

	plug->error[0] = '\0';
	retval = gaim_plugin_init(plug->handle);
	debug_printf("loaded plugin returned %s\n", retval ? retval : "NULL");
	if (retval) {
		plugin_remove_callbacks(plug->handle);
		do_error_dialog("Gaim was unable to load your plugin.", retval, GAIM_ERROR);
		g_module_close(plug->handle);
		plug->handle = NULL;
		return NULL;
	}

	plugins = g_list_append(plugins, plug);

	if (newplug) {
		g_snprintf(plug->path, sizeof(plug->path), filename);
		if (g_module_symbol(plug->handle, "gaim_plugin_desc", (gpointer *)&gaim_plugin_desc)) {
			desc = gaim_plugin_desc();
			plug->desc.name = desc->name;
		} else {
			if (g_module_symbol(plug->handle, "name", (gpointer *)&cfunc)) {
				plug->desc.name = g_strdup(cfunc());
			}
			if (g_module_symbol(plug->handle, "description", (gpointer *)&cfunc)) {
				plug->desc.description = g_strdup(cfunc());
			}
		}
		probed_plugins = g_list_append(probed_plugins, plug);
	}
	
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
	p->handle = NULL;
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
	/* XXX CUI need to tell UI what happened, but not like this 
	   update_show_plugins(); */

	g_timeout_add(5000, unload_timeout, handle);
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
	case event_del_conversation:
		sprintf(buf, "event_del_conversation");
		break;
	case event_connecting:
		sprintf(buf, "event_connecting");
		break;
	default:
		sprintf(buf, "event_unknown");
		break;
	}
	return buf;
}

int plugin_event(enum gaim_event event, ...)
{
#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
#endif
	va_list arrg;
	void    *arg1 = NULL, 
		*arg2 = NULL,
		*arg3 = NULL, 
		*arg4 = NULL,
		*arg5 = NULL;


	debug_printf("%s\n", event_name(event));

#ifdef GAIM_PLUGINS
	while (c) {
		void (*cbfunc)(void *, ...);
		
		g = (struct gaim_callback *)c->data;
		if (g->event == event && g->function != NULL) {
			cbfunc=g->function;
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
				{
					int id;
					arg1 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					cbfunc(arg1, id, g->data);
				}
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
				{
					int id;
					arg1 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					arg3 = va_arg(arrg, void*);
					cbfunc(arg1, id, arg3, g->data);
				}
				break;
			case event_warned:
				{
					int id;
					arg1 = va_arg(arrg, void*);
					arg2 = va_arg(arrg, void*);
					id = va_arg(arrg, int);
					cbfunc(arg1, arg2, id, g->data);
				}
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
				{
					int id;
					arg1 = va_arg(arrg, void *);
					id = va_arg(arrg, int);

					arg3 = va_arg(arrg, void *);
					arg4 = va_arg(arrg, void *);
					cbfunc(arg1, id, arg3, arg4, g->data);
				}
				break;
				/* five args */
			case event_im_displayed_rcvd:
				{
					time_t time;
					arg1 = va_arg(arrg, void *);
					arg2 = va_arg(arrg, void *);
					arg3 = va_arg(arrg, void *);
					arg4 = va_arg(arrg, void *);
					time = va_arg(arrg, time_t);
					cbfunc(arg1, arg2, arg3, arg4, time, g->data);
				}
				break;
				default:
				debug_printf("unknown event %d\n", event);
				break;
			}
			va_end(arrg);
		}
		c = c->next;
	}
#endif /* GAIM_PLUGINS */
#ifdef USE_PERL
	va_start(arrg, event);
	arg1 = va_arg(arrg, void *);
	arg2 = va_arg(arrg, void *);
	arg3 = va_arg(arrg, void *);
	arg4 = va_arg(arrg, void *);
	arg5 = va_arg(arrg, void *);
	return perl_event(event, arg1, arg2, arg3, arg4, arg5);
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
		if (p->type == plugin) {
			if (g_module_symbol(p->handle, "gaim_plugin_remove", (gpointer *)&gaim_plugin_remove))
				gaim_plugin_remove();
		}
		g_free(p);
		c = c->next;
	}
}
#endif
