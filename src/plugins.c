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

#ifdef GAIM_PLUGINS

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"

#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/refresh.xpm"
#include "pixmaps/cancel.xpm"

#define PATHSIZE 1024	/* XXX: stolen from dialogs.c */


/* ------------------ Local Variables ------------------------ */

static GtkWidget *plugin_dialog = NULL;

static GtkWidget *pluglist = NULL;
static GtkWidget *plugtext = NULL;
static GtkWidget *plugwindow = NULL;
static GtkWidget *plugentry = NULL;

static GtkTooltips *tooltips = NULL;

static GtkWidget *config = NULL;
static guint confighandle = 0;
static GtkWidget *reload = NULL;
static GtkWidget *unload = NULL;
extern char *last_dir;

/* --------------- Function Declarations --------------------- */

void show_plugins(GtkWidget *, gpointer);

/* UI button callbacks */
static void unload_plugin_cb(GtkWidget *, gpointer);
static void plugin_reload_cb(GtkWidget *, gpointer);

static const gchar *plugin_makelistname(GModule *);

static void destroy_plugins(GtkWidget *, gpointer);
static void load_file(GtkWidget *, gpointer);
static void load_which_plugin(GtkWidget *, gpointer);
void update_show_plugins();
static void hide_plugins(GtkWidget *, gpointer);
static void clear_plugin_display();
#if GTK_CHECK_VERSION(1,3,0)
static struct gaim_plugin *get_selected_plugin(GtkWidget *);
static void select_plugin(GtkWidget *w, struct gaim_plugin *p);
static void list_clicked(GtkWidget *, gpointer);
#else
static void list_clicked(GtkWidget *, struct gaim_plugin *);
#endif

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
	const char *file;
	struct gaim_plugin *p;
	
	file = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(plugin_dialog));
	if (file_is_dir(file, plugin_dialog)) {
		return;
	}

	if (file)
		p = load_plugin(file);
	else
		p = NULL;

	if (plugin_dialog)
		gtk_widget_destroy(plugin_dialog);
	plugin_dialog = NULL;

	update_show_plugins();
	/* Select newly loaded plugin */
	if(p == NULL)
		return;
#if GTK_CHECK_VERSION(1,3,0)
	select_plugin(pluglist, p);
#else
	gtk_list_select_item(GTK_LIST(pluglist), g_list_index(plugins, p));
#endif
}

void show_plugins(GtkWidget *w, gpointer data)
{
	/* most of this code was shamelessly stolen from Glade */
	GtkWidget *mainvbox;
	GtkWidget *tophbox;
	GtkWidget *bothbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *scrolledwindow;
	GtkWidget *label;
	GtkWidget *add;
	GtkWidget *close;
#if GTK_CHECK_VERSION(1,3,0)
	/* stuff needed for GtkTreeView *pluglist */
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	/* needed for GtkTextView *plugtext */
	GtkTextBuffer *buffer;
#endif
	
	if (plugwindow)
		return;

	GAIM_DIALOG(plugwindow);
	gtk_window_set_wmclass(GTK_WINDOW(plugwindow), "plugins", "Gaim");
	gtk_widget_realize(plugwindow);
	aol_icon(plugwindow->window);
	gtk_window_set_title(GTK_WINDOW(plugwindow), _("Gaim - Plugins"));
#if !GTK_CHECK_VERSION(1,3,0)
	gtk_widget_set_usize(plugwindow, 515, 300);
#endif
	gtk_signal_connect(GTK_OBJECT(plugwindow), "destroy", GTK_SIGNAL_FUNC(hide_plugins), NULL);

	mainvbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(plugwindow), mainvbox);
	gtk_widget_show(mainvbox);

	/* Build the top */
	tophbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainvbox), tophbox, TRUE, TRUE, 0);
	gtk_widget_show(tophbox);

	/* Left side: frame with list of plugin file names */
	frame = gtk_frame_new(_("Loaded Plugins"));
	gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 0);
#if !GTK_CHECK_VERSION(1,3,0)
	gtk_widget_set_usize(frame, 140, -1);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(frame), 6);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.05, 0.5);
	gtk_widget_show(frame);

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(frame), scrolledwindow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrolledwindow);
#if GTK_CHECK_VERSION(1,3,0)
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);

	/* Create & show plugin list */
	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	pluglist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pluglist), FALSE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("text", 
				renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pluglist), column);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), pluglist);

	g_object_unref(G_OBJECT(store));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pluglist));
	g_signal_connect(G_OBJECT(selection), "changed", 
			G_CALLBACK(list_clicked),
			NULL);
#else
	pluglist = gtk_list_new();
	gtk_list_set_selection_mode(GTK_LIST(pluglist), GTK_SELECTION_BROWSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow), 
			pluglist);
#endif /* GTK_CHECK_VERSION */

	gtk_widget_show(pluglist);

	/* Right side: frame with description and the filepath of plugin */
	frame = gtk_frame_new(_("Selected Plugin"));
	gtk_box_pack_start(GTK_BOX(tophbox), frame, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 6);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.05, 0.5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrolledwindow);
#if GTK_CHECK_VERSION(1,3,0)
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);		
	
	/* Create & show the plugin description widget */
	plugtext = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(plugtext), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(plugtext), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), plugtext);
	gtk_widget_set_size_request(GTK_WIDGET(plugtext), -1, 200);
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugtext));
	gtk_text_buffer_create_tag(buffer, "bold", "weight", 
			PANGO_WEIGHT_BOLD, NULL);
#else
	plugtext = gtk_text_new(NULL, NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow), 
				plugtext);
	gtk_text_set_word_wrap(GTK_TEXT(plugtext), TRUE);
	gtk_text_set_editable(GTK_TEXT(plugtext), FALSE);
#endif
	gtk_widget_show(plugtext);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Filepath:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	plugentry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), plugentry, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(plugentry), FALSE);
	gtk_widget_show(plugentry);

	/* Build the bottom button bar */
	bothbox = gtk_hbox_new(TRUE, 3);
	gtk_box_pack_start(GTK_BOX(mainvbox), bothbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_widget_show(bothbox);

	if (!tooltips)
		tooltips = gtk_tooltips_new();

	add = picture_button(plugwindow, _("Load"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(load_file), NULL);
	gtk_box_pack_start_defaults(GTK_BOX(bothbox), add);
	gtk_tooltips_set_tip(tooltips, add, _("Load a plugin from a file"), "");

	config = picture_button(plugwindow, _("Configure"), gnome_preferences_xpm);
	gtk_widget_set_sensitive(config, FALSE);
	gtk_box_pack_start_defaults(GTK_BOX(bothbox), config);
	gtk_tooltips_set_tip(tooltips, config, _("Configure settings of the selected plugin"), "");

	reload = picture_button(plugwindow, _("Reload"), refresh_xpm);
	gtk_widget_set_sensitive(reload, FALSE);
	gtk_signal_connect(GTK_OBJECT(reload), "clicked", GTK_SIGNAL_FUNC(plugin_reload_cb), NULL);
	gtk_box_pack_start_defaults(GTK_BOX(bothbox), reload);
	gtk_tooltips_set_tip(tooltips, reload, _("Reload the selected plugin"), "");

	unload = picture_button(plugwindow, _("Unload"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(unload), "clicked", GTK_SIGNAL_FUNC(unload_plugin_cb), pluglist);
	gtk_box_pack_start_defaults(GTK_BOX(bothbox), unload);
	gtk_tooltips_set_tip(tooltips, unload, _("Unload the selected plugin"), "");

	close = picture_button(plugwindow, _("Close"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(hide_plugins), NULL);
	gtk_box_pack_start_defaults(GTK_BOX(bothbox), close);
	gtk_tooltips_set_tip(tooltips, close, _("Close this window"), "");

	update_show_plugins();
	gtk_widget_show(plugwindow);
}

void update_show_plugins()
{
	GList *plugs = plugins;
	struct gaim_plugin *p;
#if GTK_CHECK_VERSION(1,3,0)
	int pnum = 0;
	GtkListStore *store;
	GtkTreeIter iter;
#else
	GtkWidget *label;
	GtkWidget *list_item;
	GtkWidget *hbox;
#endif
	
	if (plugwindow == NULL)
		return;

#if GTK_CHECK_VERSION(1,3,0)
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(pluglist)));
	gtk_list_store_clear(store);
#else
	gtk_list_clear_items(GTK_LIST(pluglist), 0, -1);
#endif
	while (plugs) {
		p = (struct gaim_plugin *)plugs->data;
#if GTK_CHECK_VERSION(1,3,0)
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, plugin_makelistname(p->handle), -1);
		gtk_list_store_set(store, &iter, 1, pnum++, -1);
#else
		label = gtk_label_new(plugin_makelistname(p->handle));
		hbox = gtk_hbox_new(FALSE, 0);  /* for left justification */
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), hbox);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", 
				GTK_SIGNAL_FUNC(list_clicked), p);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p);
		
		gtk_widget_show(hbox);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(pluglist), list_item);
		gtk_widget_show(list_item);
#endif
		plugs = g_list_next(plugs);
	}

	clear_plugin_display();
}

static void unload_plugin_cb(GtkWidget *w, gpointer data)
{
	struct gaim_plugin *p;
#if GTK_CHECK_VERSION(1,3,0)
	p = get_selected_plugin(pluglist);
	if(p == NULL)
		return;
#else
	GList *i;
	
	i = GTK_LIST(pluglist)->selection;

	if (i == NULL)
		return;

	p = gtk_object_get_user_data(GTK_OBJECT(i->data));
#endif
	unload_plugin(p);
	update_show_plugins();
}

static void plugin_reload_cb(GtkWidget *w, gpointer data)
{
	struct gaim_plugin *p;
#if GTK_CHECK_VERSION(1,3,0)
	p = get_selected_plugin(pluglist);
	if(p == NULL)
		return;
	p = reload_plugin(p);
#else
	GList *i;

	i = GTK_LIST(pluglist)->selection;
	if (i == NULL)
		return;

	/* Just pass off plugin to the actual function */
	p = reload_plugin(gtk_object_get_user_data(GTK_OBJECT(i->data)));
#endif
	update_show_plugins();

	/* Try and reselect the plugin in list */
	if (!pluglist)
		return;
#if GTK_CHECK_VERSION(1,3,0)
	select_plugin(pluglist, p);
#else
	gtk_list_select_item(GTK_LIST(pluglist), g_list_index(plugins, p));
#endif
}


#if GTK_CHECK_VERSION(1,3,0)
static void list_clicked(GtkWidget *w, gpointer data)
#else
static void list_clicked(GtkWidget *w, struct gaim_plugin *p)
#endif
{
	void (*gaim_plugin_config)();
#if GTK_CHECK_VERSION(1,3,0)
	struct gaim_plugin *p;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
#else
	gchar *temp;
	guint text_len;
#endif

	if (confighandle != 0) {
		gtk_signal_disconnect(GTK_OBJECT(config), confighandle);
		confighandle = 0;
	}

#if GTK_CHECK_VERSION(1,3,0)
	p = get_selected_plugin(pluglist);
	if(p == NULL) { /* No selected plugin */
		clear_plugin_display();
		return;
	}

	/* Set text and filepath widgets */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugtext));
	gtk_text_buffer_set_text(buffer, "", -1);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "Name:", -1, 
					"bold", NULL);
	gtk_text_buffer_insert(buffer, &iter, "   ", -1); 
	gtk_text_buffer_insert(buffer, &iter, (p->name != NULL) ? p->name : "", -1);
	gtk_text_buffer_insert(buffer, &iter, "\n\n", -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "Description:", -1,
					"bold", NULL);
	gtk_text_buffer_insert(buffer, &iter, "\n", -1);
	gtk_text_buffer_insert(buffer, &iter, 
					(p->description != NULL) ? p->description : "", -1);

	gtk_entry_set_text(GTK_ENTRY(plugentry), g_module_name(p->handle));
#else
	text_len = gtk_text_get_length(GTK_TEXT(plugtext));
	gtk_text_set_point(GTK_TEXT(plugtext), 0);
	gtk_text_forward_delete(GTK_TEXT(plugtext), text_len);

	temp = g_strdup_printf("Name:   %s\n\nDescription:\n%s",
				(p->name != NULL) ? p->name : "",
				(p->description != NULL) ? p->description : "");
	gtk_text_insert(GTK_TEXT(plugtext), NULL, NULL, NULL, temp, -1);
	g_free(temp);
	gtk_entry_set_text(GTK_ENTRY(plugentry), g_module_name(p->handle));
#endif
	/* Find out if this plug-in has a configuration function */
	if (g_module_symbol(p->handle, "gaim_plugin_config", (gpointer *)&gaim_plugin_config)) {
		confighandle = gtk_signal_connect(GTK_OBJECT(config), "clicked",
						  GTK_SIGNAL_FUNC(gaim_plugin_config), NULL);
		gtk_widget_set_sensitive(config, TRUE);
	} else {
		confighandle = 0;
		gtk_widget_set_sensitive(config, FALSE);
	}

	gtk_widget_set_sensitive(reload, TRUE);
	gtk_widget_set_sensitive(unload, TRUE);
}

static void hide_plugins(GtkWidget *w, gpointer data)
{
	if (plugwindow)
		gtk_widget_destroy(plugwindow);
	plugwindow = NULL;
	config = NULL;
	confighandle = 0;
}

static const gchar *plugin_makelistname(GModule *module)
{
	static gchar filename[PATHSIZE];
	const gchar *filepath = (char *)g_module_name(module);
	const char *cp;

	if (filepath == NULL || strlen(filepath) == 0)
		return NULL;
	
	if ((cp = strrchr(filepath, '/')) == NULL || *++cp == '\0')
		cp = filepath;

	strncpy(filename, cp, sizeof(filename));
	filename[sizeof(filename) - 1] = '\0';

	/* Try to pretty name by removing any trailing ".so" */
	if (strlen(filename) > 3 &&
	    strncmp(filename + strlen(filename) - 3, ".so", 3) == 0)
		filename[strlen(filename) - 3] = '\0';

	return filename;
}		

#if GTK_CHECK_VERSION(1,3,0)
static struct gaim_plugin *get_selected_plugin(GtkWidget *w) {
	/* Given the pluglist widget, this will return a pointer to the plugin
	 * currently selected in the list, and NULL if none is selected. */
	gint index;
	GList *plugs = plugins;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(w));

	/* Get list index of selected plugin */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
	if(!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	gtk_tree_model_get(model, &iter, 1, &index, -1);
				
	/* Get plugin entry from index */
	plugs = g_list_nth(plugins, index);
	if(plugs == NULL)
		return NULL;
	else
		return (struct gaim_plugin *)plugs->data;
}

static void select_plugin(GtkWidget *w, struct gaim_plugin *p) {
	/* Given the pluglist widget and a plugin, this will try to select
	 * entry in the list which corresponds with the plugin. */
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(w));
	gchar temp[10];
	
	if(g_list_index(plugins, p) == -1)
		return;

	snprintf(temp, 10, "%d", g_list_index(plugins, p));
	gtk_tree_model_get_iter_from_string(model, 
			&iter, temp);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
	gtk_tree_selection_select_iter(sel, &iter);
}
#endif /* GTK_CHECK_VERSION */

static void clear_plugin_display() {
#if GTK_CHECK_VERSION(1,3,0)
	GtkTreeSelection *selection;
	GtkTextBuffer *buffer;
	
	/* Clear the plugin display if nothing's selected */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pluglist));
	if(gtk_tree_selection_get_selected(selection, NULL, NULL) == FALSE) {
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugtext));
		gtk_text_buffer_set_text(buffer, "", -1);
		gtk_entry_set_text(GTK_ENTRY(plugentry), "");

		gtk_widget_set_sensitive(config, FALSE);
		gtk_widget_set_sensitive(reload, FALSE);
		gtk_widget_set_sensitive(unload, FALSE);
	}
#else
	/* Clear the display if nothing's selected */
	if (GTK_LIST(pluglist)->selection == NULL) {
		guint text_len = gtk_text_get_length(GTK_TEXT(plugtext));
		gtk_text_set_point(GTK_TEXT(plugtext), 0);
		gtk_text_forward_delete(GTK_TEXT(plugtext), text_len);
		gtk_entry_set_text(GTK_ENTRY(plugentry), "");

		gtk_widget_set_sensitive(config, FALSE);
		gtk_widget_set_sensitive(reload, FALSE);
		gtk_widget_set_sensitive(unload, FALSE);
	}
#endif
}

#endif
