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
#include "../config.h"
#endif

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"

#ifdef GAIM_PLUGINS

#include <dlfcn.h>

/* ------------------ Global Variables ----------------------- */

GList *plugins = NULL;
GList *callbacks = NULL;

/* ------------------ Local Variables ------------------------ */

static GtkWidget *plugin_dialog = NULL;

static GtkWidget *pluglist;
static GtkWidget *plugtext;
static GtkWidget *plugwindow;

static GtkWidget *config = NULL;
static guint confighandle = 0;
static char *last_dir = NULL;

/* --------------- Function Declarations --------------------- */

void show_plugins(GtkWidget *, gpointer);
void load_plugin(char *);

void gaim_signal_connect(GModule *, enum gaim_event, void *, void *);
void gaim_signal_disconnect(GModule *, enum gaim_event, void *);
void gaim_plugin_unload(GModule *);

static void destroy_plugins(GtkWidget *, gpointer);
static void load_file(GtkWidget *, gpointer);
static void load_which_plugin(GtkWidget *, gpointer);
static void unload(GtkWidget *, gpointer);
static void unload_immediate(GModule *);
static void list_clicked(GtkWidget *, struct gaim_plugin *);
static void update_show_plugins();
static void hide_plugins(GtkWidget *, gpointer);

/* ------------------ Code Below ---------------------------- */

static void destroy_plugins(GtkWidget *w, gpointer data)
{
	if (plugin_dialog)
		gtk_widget_destroy(plugin_dialog);
	plugin_dialog = NULL;
}

static void load_file(GtkWidget *w, gpointer data)
{
	gchar *buf;

	if (plugin_dialog) {
		gtk_widget_show(plugin_dialog);
		gdk_window_raise(plugin_dialog->window);
		return;
	}

	plugin_dialog = gtk_file_selection_new(_("Gaim - Plugin List"));

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(plugin_dialog));

	if (!last_dir)
		buf = g_strdup(LIBDIR);
	else
		buf = g_strconcat(last_dir, G_DIR_SEPARATOR_S, NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(plugin_dialog), buf);
	gtk_file_selection_complete(GTK_FILE_SELECTION(plugin_dialog), "*.so");
	gtk_signal_connect(GTK_OBJECT(plugin_dialog), "destroy",
			   GTK_SIGNAL_FUNC(destroy_plugins), plugin_dialog);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugin_dialog)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(load_which_plugin), NULL);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugin_dialog)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(destroy_plugins), NULL);

	g_free(buf);
	gtk_widget_show(plugin_dialog);
	gdk_window_raise(plugin_dialog->window);
}

static void load_which_plugin(GtkWidget *w, gpointer data)
{
	char *file;

	file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(plugin_dialog));
	if (file_is_dir(file, plugin_dialog)) {
		return;
	}

	if (file)
		load_plugin(file);

	if (plugin_dialog)
		gtk_widget_destroy(plugin_dialog);
	plugin_dialog = NULL;
}

void load_plugin(char *filename)
{
	struct gaim_plugin *plug;
	GList *c = plugins;
	char *(*gaim_plugin_init)(GModule *);
	char *(*cfunc)();
	char *error;
	char *retval;

	if (!g_module_supported())
		return;
	if (filename && !strlen(filename))
		return;

	while (filename && c) {
		plug = (struct gaim_plugin *)c->data;
		if (!strcmp(filename, g_module_name(plug->handle))) {
			void (*gaim_plugin_remove)();
			if (g_module_symbol(plug->handle, "gaim_plugin_remove",
					    (gpointer *)&gaim_plugin_remove))
				(*gaim_plugin_remove)();

			unload_immediate(plug->handle);
			c = plugins;
		} else
			c = g_list_next(c);
	}
	plug = g_malloc(sizeof *plug);

	if (filename) {
		if (last_dir)
			g_free(last_dir);
		last_dir = g_dirname(filename);
	}

	debug_printf("Loading %s\n", filename);
	plug->handle = g_module_open(filename, 0);
	if (!plug->handle) {
		error = (char *)g_module_error();
		do_error_dialog(error, _("Plugin Error"));
		g_free(plug);
		return;
	}

	if (!g_module_symbol(plug->handle, "gaim_plugin_init", (gpointer *)&gaim_plugin_init)) {
		do_error_dialog(g_module_error(), _("Plugin Error"));
		g_module_close(plug->handle);
		g_free(plug);
		return;
	}

	retval = (*gaim_plugin_init)(plug->handle);
	debug_printf("loaded plugin returned %d\n", retval);
	if (retval) {
		GList *c = callbacks;
		struct gaim_callback *g;
		while (c) {
			g = (struct gaim_callback *)c->data;
			if (g->handle == plug->handle) {
				callbacks = g_list_remove(callbacks, c->data);
				debug_printf("Removing callback, %d remain\n", g_list_length(callbacks));
				c = callbacks;
				if (c == NULL) {
					break;
				}
			} else {
				c = g_list_next(c);
			}
		}
		do_error_dialog(retval, _("Plugin Error"));
		g_module_close(plug->handle);
		g_free(plug);
		return;
	}

	plugins = g_list_append(plugins, plug);

	if (g_module_symbol(plug->handle, "name", (gpointer *)&cfunc))
		plug->name = (*cfunc)();
	else
		plug->name = NULL;

	if (g_module_symbol(plug->handle, "description", (gpointer *)&cfunc))
		plug->description = (*cfunc)();
	else
		plug->description = NULL;

	update_show_plugins();
	save_prefs();
}

char *gaim_plugin_init(GModule *mod)
{
	void (*asdf)(void *);
	char *(*gem)();
	g_module_symbol(mod, "do_stuff", (gpointer *)&asdf);
	g_module_symbol(mod, "gem", (gpointer *)&gem);
	asdf(gem);
	return NULL;
}

void show_plugins(GtkWidget *w, gpointer data)
{
	/* most of this code was shamelessly stolen from prefs.c */
	GtkWidget *page;
	GtkWidget *topbox;
	GtkWidget *botbox;
	GtkWidget *sw;
	GtkWidget *label;
	GtkWidget *list_item;
	GtkWidget *sw2;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *close;
	GList *plugs = plugins;
	struct gaim_plugin *p;

	if (plugwindow)
		return;

	plugwindow = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(plugwindow), "plugins", "Gaim");
	gtk_widget_realize(plugwindow);
	aol_icon(plugwindow->window);
	gtk_container_border_width(GTK_CONTAINER(plugwindow), 5);
	gtk_window_set_title(GTK_WINDOW(plugwindow), _("Gaim - Plugins"));
	gtk_widget_set_usize(plugwindow, 400, 250);
	gtk_signal_connect(GTK_OBJECT(plugwindow), "destroy", GTK_SIGNAL_FUNC(hide_plugins), NULL);

	page = gtk_vbox_new(FALSE, 5);
	topbox = gtk_hbox_new(FALSE, 5);
	botbox = gtk_hbox_new(FALSE, 5);

	/* Left side: list of plugin file names */
	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	pluglist = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), pluglist);
	gtk_box_pack_start(GTK_BOX(topbox), sw2, TRUE, TRUE, 0);

	/* Right side: the text description of the plugin */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	plugtext = gtk_text_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), plugtext);
	gtk_box_pack_start(GTK_BOX(topbox), sw, TRUE, TRUE, 0);
	gtk_text_set_word_wrap(GTK_TEXT(plugtext), TRUE);
	gtk_text_set_editable(GTK_TEXT(plugtext), FALSE);

	/* Build the bottom button bar */
	add = gtk_button_new_with_label(_("Load Plugin"));
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(load_file), NULL);
	gtk_box_pack_start(GTK_BOX(botbox), add, TRUE, TRUE, 0);
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(add), GTK_RELIEF_NONE);

	config = gtk_button_new_with_label(_("Configure Plugin"));
	gtk_widget_set_sensitive(config, 0);
	gtk_box_pack_start(GTK_BOX(botbox), config, TRUE, TRUE, 0);
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(config), GTK_RELIEF_NONE);

	remove = gtk_button_new_with_label(_("Unload Plugin"));
	gtk_signal_connect(GTK_OBJECT(remove), "clicked", GTK_SIGNAL_FUNC(unload), pluglist);
	gtk_box_pack_start(GTK_BOX(botbox), remove, TRUE, TRUE, 0);
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(remove), GTK_RELIEF_NONE);

	close = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(hide_plugins), NULL);
	gtk_box_pack_start(GTK_BOX(botbox), close, TRUE, TRUE, 0);
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);

	gtk_box_pack_start(GTK_BOX(page), topbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), botbox, FALSE, FALSE, 0);

	while (plugs) {
		p = (struct gaim_plugin *)plugs->data;
		label = gtk_label_new(g_module_name(p->handle));
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(list_clicked), p);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p);

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(pluglist), list_item);
		gtk_widget_show(list_item);

		plugs = g_list_next(plugs);
	}

	/* Make the first item selected */
	if (plugins != NULL)
		gtk_list_select_item(GTK_LIST(pluglist), 0);

	gtk_widget_show(page);
	gtk_widget_show(topbox);
	gtk_widget_show(botbox);
	gtk_widget_show(sw);
	gtk_widget_show(sw2);
	gtk_widget_show(pluglist);
	gtk_widget_show(plugtext);
	gtk_widget_show(add);
	gtk_widget_show(config);
	gtk_widget_show(remove);
	gtk_widget_show(close);

	gtk_container_add(GTK_CONTAINER(plugwindow), page);
	gtk_widget_show(plugwindow);
}

void update_show_plugins()
{
	GList *plugs = plugins;
	struct gaim_plugin *p;
	GtkWidget *label;
	GtkWidget *list_item;

	if (plugwindow == NULL)
		return;

	gtk_list_clear_items(GTK_LIST(pluglist), 0, -1);
	while (plugs) {
		p = (struct gaim_plugin *)plugs->data;
		label = gtk_label_new(g_module_name(p->handle));
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(list_clicked), p);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p);

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(pluglist), list_item);
		gtk_widget_show(list_item);
		plugs = g_list_next(plugs);
	}
	if (plugins != NULL)
		gtk_list_select_item(GTK_LIST(pluglist), 0);
	else {
		gtk_text_set_point(GTK_TEXT(plugtext), 0);
		gtk_text_forward_delete(GTK_TEXT(plugtext), gtk_text_get_length(GTK_TEXT(plugtext)));
	}
}

void unload(GtkWidget *w, gpointer data)
{
	GList *i;
	struct gaim_plugin *p;
	void (*gaim_plugin_remove)();

	i = GTK_LIST(pluglist)->selection;

	if (i == NULL)
		return;

	p = gtk_object_get_user_data(GTK_OBJECT(i->data));

	/* Attempt to call the plugin's remove function (if there) */
	if (g_module_symbol(p->handle, "gaim_plugin_remove", (gpointer *)&gaim_plugin_remove))
		(*gaim_plugin_remove)();

	unload_immediate(p->handle);
}

static void unload_for_real(void *handle)
{
	GList *i;
	struct gaim_plugin *p = NULL;
	GList *c = callbacks;
	struct gaim_callback *g;

	i = plugins;
	while (i) {
		p = (struct gaim_plugin *)i->data;
		if (handle == p->handle)
			break;
		p = NULL;
		i = g_list_next(i);
	}

	if (!p)
		return;

	debug_printf("Unloading %s\n", g_module_name(p->handle));

	debug_printf("%d callbacks to search\n", g_list_length(callbacks));

	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->handle == p->handle) {
			callbacks = g_list_remove(callbacks, c->data);
			g_free(g);
			debug_printf("Removing callback, %d remain\n", g_list_length(callbacks));
			c = callbacks;
			if (c == NULL) {
				break;
			}
		} else {
			c = g_list_next(c);
		}
	}

	plugins = g_list_remove(plugins, p);
	g_free(p);
	if (config)
		gtk_widget_set_sensitive(config, 0);
	update_show_plugins();
	save_prefs();
}

void unload_immediate(GModule *handle)
{
	unload_for_real(handle);
	g_module_close(handle);
}

static gint unload_timeout(GModule *handle)
{
	g_module_close(handle);
	return FALSE;
}

void gaim_plugin_unload(GModule *handle)
{
	unload_for_real(handle);
	gtk_timeout_add(5000, (GtkFunction)unload_timeout, handle);
}

void list_clicked(GtkWidget *w, struct gaim_plugin *p)
{
	gchar *temp;
	guint text_len;
	void (*gaim_plugin_config)();

	if (confighandle != 0)
		gtk_signal_disconnect(GTK_OBJECT(config), confighandle);
	text_len = gtk_text_get_length(GTK_TEXT(plugtext));
	gtk_text_set_point(GTK_TEXT(plugtext), 0);
	gtk_text_forward_delete(GTK_TEXT(plugtext), text_len);

	temp = g_strdup_printf("%s\n%s", p->name, p->description);
	gtk_text_insert(GTK_TEXT(plugtext), NULL, NULL, NULL, temp, -1);
	g_free(temp);

	/* Find out if this plug-in has a configuration function */
	if (g_module_symbol(p->handle, "gaim_plugin_config", (gpointer *)&gaim_plugin_config)) {
		confighandle = gtk_signal_connect(GTK_OBJECT(config), "clicked",
						  GTK_SIGNAL_FUNC(gaim_plugin_config), NULL);
		gtk_widget_set_sensitive(config, 1);
	} else {
		confighandle = 0;
		gtk_widget_set_sensitive(config, 0);
	}
}

void hide_plugins(GtkWidget *w, gpointer data)
{
	if (plugwindow)
		gtk_widget_destroy(plugwindow);
	plugwindow = NULL;
	config = NULL;
	confighandle = 0;
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
	default:
		sprintf(buf, "event_unknown");
		break;
	}
	return buf;
}

int plugin_event(enum gaim_event event, void *arg1, void *arg2, void *arg3, void *arg4)
{
#ifdef USE_PERL
	char buf[BUF_LONG];
	char *tmp;
#endif
#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;

	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event && g->function !=NULL) {
			switch (event) {

				/* struct gaim_connection * */
			case event_signon:
			case event_signoff:
				{
					void (*function) (struct gaim_connection *, void *) =
					    g->function;
					(*function)(arg1, g->data);
				}
				break;

				/* no args */
			case event_blist_update:
			case event_quit:
				{
					void (*function)(void *) = g->function;
					(*function)(g->data);
				}
				break;

				/* struct gaim_connection *, char **, char ** */
			case event_im_recv:
				{
					void (*function)(struct gaim_connection *, char **, char **,
							  void *) = g->function;
					(*function)(arg1, arg2, arg3, g->data);
				}
				break;

				/* struct gaim_connection *, char *, char ** */
			case event_im_send:
			case event_chat_send:
				{
					void (*function)(struct gaim_connection *, char *, char **,
							  void *) = g->function;
					(*function)(arg1, arg2, arg3, g->data);
				}
				break;

				/* struct gaim_connection *, char * */
			case event_chat_join:
			case event_chat_leave:
			case event_buddy_signon:
			case event_buddy_signoff:
			case event_buddy_away:
			case event_buddy_back:
			case event_buddy_idle:
			case event_buddy_unidle:
				{
					void (*function)(struct gaim_connection *, char *, void *) =
					    g->function;
					(*function)(arg1, arg2, g->data);
				}
				break;

				/* char * */
			case event_new_conversation:
				{
					void (*function)(char *, void *) = g->function;
					(*function)(arg1, g->data);
				}
				break;

				/* struct gaim_connection *, char *, char *, char * */
			case event_chat_invited:
			case event_chat_recv:
				{
					void (*function)(struct gaim_connection *, char *, char *,
							  char *, void *) = g->function;
					(*function)(arg1, arg2, arg3, arg4, g->data);
				}
				break;

				/* struct gaim_connection *, char *, char * */
			case event_chat_buddy_join:
			case event_chat_buddy_leave:
			case event_away:
			case event_back:
				{
					void (*function)(struct gaim_connection *, char *, char *,
							  void *) = g->function;
					(*function)(arg1, arg2, arg3, g->data);
				}
				break;

				/* struct gaim_connection *, char *, int */
			case event_warned:
				{
					void (*function)(struct gaim_connection *, char *, int,
							void *) = g->function;
					(*function)(arg1, arg2, (int)arg3, g->data);
				}
				break;

				/* int */
			case event_error:
				{
					void (*function)(int, void *) = g->function;
					(*function)((int)arg1, g->data);
				}
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
	switch (event) {
	case event_signon:
		g_snprintf(buf, sizeof buf, "\"%s\"", ((struct gaim_connection *)arg1)->username);
		break;
	case event_signoff:
		g_snprintf(buf, sizeof buf, "\"%s\"", ((struct gaim_connection *)arg1)->username);
		break;
	case event_away:
		g_snprintf(buf, sizeof buf, "\"%s\"", ((struct gaim_connection *)arg1)->username);
		break;
	case event_back:
		g_snprintf(buf, sizeof buf, "\"%s\"", ((struct gaim_connection *)arg1)->username);
		break;
	case event_im_recv:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\" %s",
			   ((struct gaim_connection *)arg1)->username,
			   *(char **)arg2 ? *(char **)arg2 : "(null)",
			   *(char **)arg3 ? *(char **)arg3 : "(null)");
		break;
	case event_im_send:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\" %s",
			   ((struct gaim_connection *)arg1)->username, (char *)arg2,
			   *(char **)arg3 ? *(char **)arg3 : "(null)");
		break;
	case event_buddy_signon:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_buddy_signoff:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_buddy_away:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_buddy_back:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_buddy_idle:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_buddy_unidle:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_blist_update:
		buf[0] = 0;
		break;
	case event_chat_invited:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\" %s", (char *)arg2, (char *)arg3,
			   (char *)arg4);
		break;
	case event_chat_join:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_chat_leave:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg2);
		break;
	case event_chat_buddy_join:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\"", (char *)arg2, (char *)arg3);
		break;
	case event_chat_buddy_leave:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\"", (char *)arg2, (char *)arg3);
		break;
	case event_chat_recv:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\" %s", (char *)arg2, (char *)arg3,
			   (char *)arg4);
		break;
	case event_chat_send:
		g_snprintf(buf, sizeof buf, "\"%s\" %s", (char *)arg2,
				*(char **)arg3 ? *(char **)arg3 : "(null)");
		break;
	case event_warned:
		g_snprintf(buf, sizeof buf, "\"%s\" \"%s\" %d",
			   ((struct gaim_connection *)arg1)->username, (char *)arg2, (int)arg3);
		break;
	case event_error:
		g_snprintf(buf, sizeof buf, "%d", (int)arg1);
		break;
	case event_quit:
		buf[0] = 0;
		break;
	case event_new_conversation:
		g_snprintf(buf, sizeof buf, "\"%s\"", (char *)arg1);
		break;
	default:
		break;
	}
	tmp = event_name(event);
	debug_printf("%s: %s\n", tmp, buf);
	return perl_event(tmp, buf);
#else
	return 0;
#endif
}
