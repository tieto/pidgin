/**
 * @file gtkplugin.c GTK+ Plugins support
 * @ingroup gtkui
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
#include "internal.h"
#include "gtkgaim.h"
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "debug.h"
#include "prefs.h"

#include <string.h>

#define GAIM_RESPONSE_CONFIGURE 98121

static GtkWidget *expander = NULL;
static GtkWidget *plugin_dialog = NULL;
static GtkWidget *plugin_details = NULL;
static GtkWidget *pref_button = NULL;
static GHashTable *plugin_pref_dialogs = NULL;

GtkWidget *
gaim_gtk_plugin_get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *config = NULL;

	g_return_val_if_fail(plugin != NULL, NULL);

	if (GAIM_IS_GTK_PLUGIN(plugin) && plugin->info->ui_info
		&& GAIM_GTK_PLUGIN_UI_INFO(plugin)->get_config_frame)
	{
		GaimGtkPluginUiInfo *ui_info;

		ui_info = GAIM_GTK_PLUGIN_UI_INFO(plugin);

		config = ui_info->get_config_frame(plugin);

		if (plugin->info->prefs_info
			&& plugin->info->prefs_info->get_plugin_pref_frame)
		{
			gaim_debug_warning("gtkplugin",
				"Plugin %s contains both, ui_info and "
				"prefs_info preferences; prefs_info will be "
				"ignored.", plugin->info->name);
		}
	}

	if (config == NULL && plugin->info->prefs_info
		&& plugin->info->prefs_info->get_plugin_pref_frame)
	{
		GaimPluginPrefFrame *frame;

		frame = plugin->info->prefs_info->get_plugin_pref_frame(plugin);

		config = gaim_gtk_plugin_pref_create_frame(frame);
	}

	return config;
}

void
gaim_gtk_plugins_save(void)
{
	GList *pl;
	GList *files = NULL;
	GaimPlugin *p;

	for (pl = gaim_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		p = pl->data;

		if (p->info->type != GAIM_PLUGIN_PROTOCOL &&
			p->info->type != GAIM_PLUGIN_LOADER) {

			files = g_list_append(files, p->path);
		}
	}

	gaim_prefs_set_string_list("/gaim/gtk/plugins/loaded", files);
	g_list_free(files);
}

static void
update_plugin_list(void *data)
{
	GtkListStore *ls = GTK_LIST_STORE(data);
	GtkTreeIter iter;
	GList *probes;
	GaimPlugin *plug;

	gtk_list_store_clear(ls);
	gaim_plugins_probe(G_MODULE_SUFFIX);

	for (probes = gaim_plugins_get_all();
		 probes != NULL;
		 probes = probes->next)
	{
		char *desc;
		plug = probes->data;

		if (plug->info->type != GAIM_PLUGIN_STANDARD ||
			(plug->info->flags & GAIM_PLUGIN_FLAG_INVISIBLE))
		{
			continue;
		}

		gtk_list_store_append (ls, &iter);
		desc = g_strdup_printf("<b>%s</b> %s\n%s", plug->info->name ? _(plug->info->name) : g_basename(plug->path),
				       plug->info->version,
				       _(plug->info->summary));
		gtk_list_store_set(ls, &iter,
				   0, gaim_plugin_is_loaded(plug),
				   1, desc,
				   2, plug, -1);
		g_free(desc);
	}
}

static void pref_dialog_response_cb(GtkWidget *d, int response, GaimPlugin *plug)
{
	switch (response) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		g_hash_table_remove(plugin_pref_dialogs, plug);
		if (g_hash_table_size(plugin_pref_dialogs) == 0) {
			g_hash_table_destroy(plugin_pref_dialogs);
			plugin_pref_dialogs = NULL;
		}
		gtk_widget_destroy(d);
		break;
	}
}

static void plugin_load (GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	GaimPlugin *plug;
	gchar buf[1024];
	gchar *name = NULL, *description = NULL;
	GtkWidget *dialog = NULL;

	GdkCursor *wait = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor(plugin_dialog->window, wait);
	gdk_cursor_unref(wait);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 2, &plug, -1);

	if (!gaim_plugin_is_loaded(plug))
		gaim_plugin_load(plug);
	else {
		if (plugin_pref_dialogs != NULL &&
			(dialog = g_hash_table_lookup(plugin_pref_dialogs, plug)))
			pref_dialog_response_cb(dialog, GTK_RESPONSE_DELETE_EVENT, plug);
		gaim_plugin_unload(plug);
	}

	gtk_widget_set_sensitive(pref_button,
		gaim_plugin_is_loaded(plug)
		&& ((GAIM_IS_GTK_PLUGIN(plug) && plug->info->ui_info
			&& GAIM_GTK_PLUGIN_UI_INFO(plug)->get_config_frame)
		 || (plug->info->prefs_info
			&& plug->info->prefs_info->get_plugin_pref_frame)));

	gdk_window_set_cursor(plugin_dialog->window, NULL);

	name = g_markup_escape_text(_(plug->info->name), -1);
	description = g_markup_escape_text(_(plug->info->description), -1);

	if (plug->error != NULL) {
		gchar *error = g_markup_escape_text(plug->error, -1);
		gchar *desc;
		g_snprintf(buf, sizeof(buf),
				   "<span size=\"larger\">%s %s</span>\n\n"
				   "<span weight=\"bold\" color=\"red\">%s</span>\n\n"
				   "%s",
				   name, plug->info->version, error, description);
		desc = g_strdup_printf("<b>%s</b> %s\n<span weight=\"bold\" color=\"red\"%s</span>",
			       plug->info->name, plug->info->version, error);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    1, desc,
				    -1);
		g_free(desc);
		g_free(error);
		gtk_label_set_markup(GTK_LABEL(plugin_details), buf);
	}
	g_free(name);
	g_free(description);


	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    0, gaim_plugin_is_loaded(plug),
			    -1);

	gtk_tree_path_free(path);
	gaim_gtk_plugins_save();
}

static gboolean ensure_plugin_visible(void *data)
{
	GtkTreeSelection *sel = GTK_TREE_SELECTION(data);
	GtkTreeView *tv = gtk_tree_selection_get_tree_view(sel);
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreePath *path;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
		return FALSE;
	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_scroll_to_cell(gtk_tree_selection_get_tree_view(sel), path, NULL, FALSE, 0, 0);
	gtk_tree_path_free(path);
	return FALSE;
}

static void prefs_plugin_sel (GtkTreeSelection *sel, GtkTreeModel *model)
{
	gchar *buf, *pname, *pdesc, *pauth, *pweb;
	GtkTreeIter  iter;
	GValue val;
	GaimPlugin *plug;

	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		/* Clear the old plugin details */
		gtk_label_set_markup(GTK_LABEL(plugin_details), "");
		gtk_widget_set_sensitive(pref_button, FALSE);

		/* Collapse and disable the expander widget */
		gtk_expander_set_expanded(GTK_EXPANDER(expander), FALSE);
		gtk_widget_set_sensitive(expander, FALSE);

		return;
	}

	gtk_widget_set_sensitive(expander, TRUE);

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 2, &val);
	plug = g_value_get_pointer(&val);

	pname = g_markup_escape_text(_(plug->info->name), -1);
	pdesc = (plug->info->description) ?
			g_markup_escape_text(_(plug->info->description), -1) : NULL;
	pauth = (plug->info->author) ?
			g_markup_escape_text(_(plug->info->author), -1) : NULL;
	pweb = (plug->info->homepage) ?
		   g_markup_escape_text(_(plug->info->homepage), -1) : NULL;
	buf = g_strdup_printf(
		   _("%s%s"
		     "<span weight=\"bold\">Written by:</span>\t%s\n"
		     "<span weight=\"bold\">Web site:</span>\t\t%s\n"
		     "<span weight=\"bold\">File name:</span>\t%s"),
		   pdesc ? pdesc : "", pdesc ? "\n\n" : "",
		   pauth ? pauth : "", pweb ? pweb : "", plug->path);

	gtk_widget_set_sensitive(pref_button,
		gaim_plugin_is_loaded(plug)
		&& ((GAIM_IS_GTK_PLUGIN(plug) && plug->info->ui_info
			&& GAIM_GTK_PLUGIN_UI_INFO(plug)->get_config_frame)
		 || (plug->info->prefs_info
			&& plug->info->prefs_info->get_plugin_pref_frame)));

	gtk_label_set_markup(GTK_LABEL(plugin_details), buf);

	/* Make sure the selected plugin is still visible */
	g_idle_add(ensure_plugin_visible, sel);


	g_value_unset(&val);
	g_free(buf);
	g_free(pname);
	g_free(pdesc);
	g_free(pauth);
	g_free(pweb);
}

static void plugin_dialog_response_cb(GtkWidget *d, int response, GtkTreeSelection *sel)
{
	GaimPlugin *plug;
	GtkWidget *dialog, *box;
	GtkTreeModel *model;
	GValue val;
	GtkTreeIter iter;

	switch (response) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		gtk_widget_destroy(d);
		if (plugin_pref_dialogs != NULL) {
			g_hash_table_destroy(plugin_pref_dialogs);
			plugin_pref_dialogs = NULL;
		}
		plugin_dialog = NULL;
		break;
	case GAIM_RESPONSE_CONFIGURE:
		if (! gtk_tree_selection_get_selected (sel, &model, &iter))
			return;
		val.g_type = 0;
		gtk_tree_model_get_value(model, &iter, 2, &val);
		plug = g_value_get_pointer(&val);
		if (plug == NULL)
			break;
		if (plugin_pref_dialogs != NULL &&
			g_hash_table_lookup(plugin_pref_dialogs, plug))
			break;
		box = gaim_gtk_plugin_get_config_frame(plug);
		if (box == NULL)
			break;

		dialog = gtk_dialog_new_with_buttons(GAIM_ALERT_TITLE, GTK_WINDOW(d),
						     GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
						     NULL);
		if (plugin_pref_dialogs == NULL)
			plugin_pref_dialogs = g_hash_table_new(NULL, NULL);

		g_hash_table_insert(plugin_pref_dialogs, plug, dialog);

		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(pref_dialog_response_cb), plug);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), box);
		gtk_window_set_role(GTK_WINDOW(dialog), "plugin_config");
		gtk_window_set_title(GTK_WINDOW(dialog), _(gaim_plugin_get_name(plug)));
		gtk_widget_show_all(dialog);
		g_value_unset(&val);
		break;
	}
}

static void
show_plugin_prefs_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer null)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GaimPlugin *plugin;
	GtkTreeModel *model;

	sel = gtk_tree_view_get_selection(view);

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, 2, &plugin, -1);

	if (!gaim_plugin_is_loaded(plugin))
		return;

	/* Now show the pref-dialog for the plugin */
	plugin_dialog_response_cb(NULL, GAIM_RESPONSE_CONFIGURE, sel);
}

void gaim_gtk_plugin_dialog_show()
{
	GtkWidget *sw;
	GtkWidget *event_view;
	GtkListStore *ls;
	GtkCellRenderer *rend, *rendt;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	if (plugin_dialog != NULL) {
		gtk_window_present(GTK_WINDOW(plugin_dialog));
		return;
	}

	plugin_dialog = gtk_dialog_new_with_buttons(_("Plugins"),
						    NULL,
						    GTK_DIALOG_NO_SEPARATOR,
						    NULL);
	pref_button = gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						_("Configure Pl_ugin"), GAIM_RESPONSE_CONFIGURE);
	gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_widget_set_sensitive(pref_button, FALSE);
	gtk_window_set_role(GTK_WINDOW(plugin_dialog), "plugins");

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(plugin_dialog)->vbox), sw, TRUE, TRUE, 0);

	ls = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),
					     1, GTK_SORT_ASCENDING);

	update_plugin_list(ls);

	event_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(ls));

	g_signal_connect(G_OBJECT(event_view), "row-activated",
				G_CALLBACK(show_plugin_prefs_cb), event_view);

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));

	col = gtk_tree_view_column_new_with_attributes (_("Enabled"),
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 0);
	g_signal_connect (G_OBJECT(rend), "toggled",
			  G_CALLBACK(plugin_load), ls);

	rendt = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes (_("Name"),
							rendt,
							"markup", 1,
							NULL);
#if GTK_CHECK_VERSION(2,6,0)
	gtk_tree_view_column_set_expand (col, TRUE);
	g_object_set(rendt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 1);
	g_object_unref(G_OBJECT(ls));
	gtk_container_add(GTK_CONTAINER(sw), event_view);

	expander = gtk_expander_new(_("<b>Plugin Details</b>"));
	gtk_expander_set_use_markup(GTK_EXPANDER(expander), TRUE);
	plugin_details = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(plugin_details), TRUE);
	gtk_container_add(GTK_CONTAINER(expander), plugin_details);
	gtk_widget_set_sensitive(expander, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(plugin_dialog)->vbox), expander, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (prefs_plugin_sel), NULL);
	g_signal_connect(G_OBJECT(plugin_dialog), "response", G_CALLBACK(plugin_dialog_response_cb), sel);
	gtk_window_set_default_size(GTK_WINDOW(plugin_dialog), 430, 430);
	gtk_widget_show_all(plugin_dialog);
}
