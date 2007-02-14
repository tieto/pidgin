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
#include "pidgin.h"
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "gtkutils.h"
#include "debug.h"
#include "prefs.h"
#include "request.h"

#include <string.h>

#define GAIM_RESPONSE_CONFIGURE 98121

static void plugin_toggled_stage_two(GaimPlugin *plug, GtkTreeModel *model,
                                  GtkTreeIter *iter, gboolean unload);

static GtkWidget *expander = NULL;
static GtkWidget *plugin_dialog = NULL;
static GtkWidget *plugin_details = NULL;
static GtkWidget *pref_button = NULL;
static GHashTable *plugin_pref_dialogs = NULL;

GtkWidget *
pidgin_plugin_get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *config = NULL;

	g_return_val_if_fail(plugin != NULL, NULL);

	if (PIDGIN_IS_PIDGIN_PLUGIN(plugin) && plugin->info->ui_info
		&& PIDGIN_PLUGIN_UI_INFO(plugin)->get_config_frame)
	{
		PidginPluginUiInfo *ui_info;

		ui_info = PIDGIN_PLUGIN_UI_INFO(plugin);

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

		config = pidgin_plugin_pref_create_frame(frame);

		/* XXX According to bug #1407047 this broke saving pluging preferences, I'll look at fixing it correctly later.
		gaim_plugin_pref_frame_destroy(frame);
		*/
	}

	return config;
}

void
pidgin_plugins_save(void)
{
	gaim_plugins_save_loaded("/gaim/gtk/plugins/loaded");
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
		char *name;
		char *version;
		char *summary;
		char *desc;
		plug = probes->data;

		if (plug->info->type == GAIM_PLUGIN_LOADER) {
			GList *cur;
			for (cur = GAIM_PLUGIN_LOADER_INFO(plug)->exts; cur != NULL;
					 cur = cur->next)
				gaim_plugins_probe(cur->data);
			continue;
		} else if (plug->info->type != GAIM_PLUGIN_STANDARD ||
			(plug->info->flags & GAIM_PLUGIN_FLAG_INVISIBLE)) {
			continue;
		}

		gtk_list_store_append (ls, &iter);

		name = g_markup_escape_text(plug->info->name ? _(plug->info->name) : g_basename(plug->path), -1);
		version = g_markup_escape_text(plug->info->version, -1);
		summary = g_markup_escape_text(_(plug->info->summary), -1);

		desc = g_strdup_printf("<b>%s</b> %s\n%s", name,
				       version,
				       summary);
		g_free(name);
		g_free(version);
		g_free(summary);

		gtk_list_store_set(ls, &iter,
				   0, gaim_plugin_is_loaded(plug),
				   1, desc,
				   2, plug,
				   3, gaim_plugin_is_unloadable(plug),
				   -1);
		g_free(desc);
	}
}

static void plugin_loading_common(GaimPlugin *plugin, GtkTreeView *view, gboolean loaded)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(view);

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			GaimPlugin *plug;
			GtkTreeSelection *sel;

			gtk_tree_model_get(model, &iter, 2, &plug, -1);

			if (plug != plugin)
				continue;

			gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, loaded, -1);

			/* If the loaded/unloaded plugin is the selected row,
			 * update the pref_button. */
			sel = gtk_tree_view_get_selection(view);
			if (gtk_tree_selection_get_selected(sel, &model, &iter))
			{
				gtk_tree_model_get(model, &iter, 2, &plug, -1);
				if (plug == plugin)
				{
					gtk_widget_set_sensitive(pref_button,
						loaded
						&& ((PIDGIN_IS_PIDGIN_PLUGIN(plug) && plug->info->ui_info
							&& PIDGIN_PLUGIN_UI_INFO(plug)->get_config_frame)
						 || (plug->info->prefs_info
							&& plug->info->prefs_info->get_plugin_pref_frame)));
				}
			}

			break;
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

static void plugin_load_cb(GaimPlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, TRUE);
}

static void plugin_unload_cb(GaimPlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, FALSE);
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

static void plugin_unload_confirm_cb(gpointer *data)
{
	GaimPlugin *plugin = (GaimPlugin *)data[0];
	GtkTreeModel *model = (GtkTreeModel *)data[1];
	GtkTreeIter *iter = (GtkTreeIter *)data[2];

	plugin_toggled_stage_two(plugin, model, iter, TRUE);

	g_free(data);
}

static void plugin_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter *iter = g_new(GtkTreeIter, 1);
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	GaimPlugin *plug;
	GtkWidget *dialog = NULL;

	gtk_tree_model_get_iter(model, iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, iter, 2, &plug, -1);

	/* Apparently, GTK+ won't honor the sensitive flag on cell renderers for booleans. */
	if (gaim_plugin_is_unloadable(plug))
	{
		g_free(iter);
		return;
	}

	if (!gaim_plugin_is_loaded(plug))
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		gaim_plugin_load(plug);
		plugin_toggled_stage_two(plug, model, iter, FALSE);

		pidgin_clear_cursor(plugin_dialog);
	}
	else
	{
		if (plugin_pref_dialogs != NULL &&
			(dialog = g_hash_table_lookup(plugin_pref_dialogs, plug)))
			pref_dialog_response_cb(dialog, GTK_RESPONSE_DELETE_EVENT, plug);

		if (plug->dependent_plugins != NULL)
		{
			GString *tmp = g_string_new(_("The following plugins will be unloaded."));
			GList *l;
			gpointer *cb_data;

			for (l = plug->dependent_plugins ; l != NULL ; l = l->next)
			{
				const char *dep_name = (const char *)l->data;
				GaimPlugin *dep_plugin = gaim_plugins_find_with_id(dep_name);
				g_return_if_fail(dep_plugin != NULL);

				g_string_append_printf(tmp, "\n\t%s\n", _(dep_plugin->info->name));
			}

			cb_data = g_new(gpointer, 3);
			cb_data[0] = plug;
			cb_data[1] = model;
			cb_data[2] = iter;

			gaim_request_action(plugin_dialog, NULL,
			                    _("Multiple plugins will be unloaded."),
			                    tmp->str, 0, cb_data, 2,
			                    _("Unload Plugins"), G_CALLBACK(plugin_unload_confirm_cb),
			                    _("Cancel"), g_free);
			g_string_free(tmp, TRUE);
		}
		else
			plugin_toggled_stage_two(plug, model, iter, TRUE);
	}
}

static void plugin_toggled_stage_two(GaimPlugin *plug, GtkTreeModel *model, GtkTreeIter *iter, gboolean unload)
{
	gchar *name = NULL;
	gchar *description = NULL;

	if (unload)
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		gaim_plugin_unload(plug);

		pidgin_clear_cursor(plugin_dialog);
	}

	gtk_widget_set_sensitive(pref_button,
		gaim_plugin_is_loaded(plug)
		&& ((PIDGIN_IS_PIDGIN_PLUGIN(plug) && plug->info->ui_info
			&& PIDGIN_PLUGIN_UI_INFO(plug)->get_config_frame)
		 || (plug->info->prefs_info
			&& plug->info->prefs_info->get_plugin_pref_frame)));

	name = g_markup_escape_text(_(plug->info->name), -1);
	description = g_markup_escape_text(_(plug->info->description), -1);

	if (plug->error != NULL) {
		gchar *error = g_markup_escape_text(plug->error, -1);
		gchar *desc;
		gchar *text = g_strdup_printf(
				   "<span size=\"larger\">%s %s</span>\n\n"
				   "<span weight=\"bold\" color=\"red\">%s</span>\n\n"
				   "%s",
				   name, plug->info->version, error, description);
		desc = g_strdup_printf("<b>%s</b> %s\n<span weight=\"bold\" color=\"red\"%s</span>",
			       plug->info->name, plug->info->version, error);
		gtk_list_store_set(GTK_LIST_STORE (model), iter,
				   1, desc,
				   -1);
		g_free(desc);
		g_free(error);
		gtk_label_set_markup(GTK_LABEL(plugin_details), text);
		g_free(text);
	}
	g_free(name);
	g_free(description);


	gtk_list_store_set(GTK_LIST_STORE (model), iter,
	                   0, gaim_plugin_is_loaded(plug),
	                   -1);
	g_free(iter);

	pidgin_plugins_save();
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
		     "<span weight=\"bold\">Website:</span>\t\t%s\n"
		     "<span weight=\"bold\">Filename:</span>\t\t%s"),
		   pdesc ? pdesc : "", pdesc ? "\n\n" : "",
		   pauth ? pauth : "", pweb ? pweb : "", plug->path);

	if (plug->error != NULL)
	{
		char *tmp = g_strdup_printf(
			_("%s\n"
			  "<span foreground=\"#ff0000\" weight=\"bold\">"
			  "Error: %s\n"
			  "Check the plugin website for an update."
			  "</span>"),
			buf, plug->error);
		g_free(buf);
		buf = tmp;
	}

	gtk_widget_set_sensitive(pref_button,
		gaim_plugin_is_loaded(plug)
		&& ((PIDGIN_IS_PIDGIN_PLUGIN(plug) && plug->info->ui_info
			&& PIDGIN_PLUGIN_UI_INFO(plug)->get_config_frame)
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
		gaim_request_close_with_handle(plugin_dialog);
		gaim_signals_disconnect_by_handle(plugin_dialog);
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
		box = pidgin_plugin_get_config_frame(plug);
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
show_plugin_prefs_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, GtkWidget *dialog)
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
	plugin_dialog_response_cb(dialog, GAIM_RESPONSE_CONFIGURE, sel);
}

void pidgin_plugin_dialog_show()
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
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(plugin_dialog)->vbox), sw, TRUE, TRUE, 0);

	ls = gtk_list_store_new(4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),
					     1, GTK_SORT_ASCENDING);

	update_plugin_list(ls);

	event_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(event_view), TRUE);

	g_signal_connect(G_OBJECT(event_view), "row-activated",
				G_CALLBACK(show_plugin_prefs_cb), plugin_dialog);

	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-load", plugin_dialog,
	                    GAIM_CALLBACK(plugin_load_cb), event_view);
	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-unload", plugin_dialog,
	                    GAIM_CALLBACK(plugin_unload_cb), event_view);

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));

	col = gtk_tree_view_column_new_with_attributes (_("Enabled"),
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 0);
	g_signal_connect(G_OBJECT(rend), "toggled",
			 G_CALLBACK(plugin_toggled), ls);

	rendt = gtk_cell_renderer_text_new();
	g_object_set(rendt,
		     "foreground", "#c0c0c0",
		     NULL);
	col = gtk_tree_view_column_new_with_attributes (_("Name"),
							rendt,
							"markup", 1,
							"foreground-set", 3,
							NULL);
#if GTK_CHECK_VERSION(2,6,0)
	gtk_tree_view_column_set_expand (col, TRUE);
	g_object_set(rendt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 1);
	g_object_unref(G_OBJECT(ls));
	gtk_container_add(GTK_CONTAINER(sw), event_view);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(event_view), 1);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(event_view),
				pidgin_tree_view_search_equal_func, NULL, NULL);

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
