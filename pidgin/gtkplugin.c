/**
 * @file gtkplugin.c GTK+ Plugins support
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "internal.h"
#include "pidgin.h"
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "gtkutils.h"
#include "debug.h"
#include "prefs.h"
#include "request.h"
#include "pidgintooltip.h"

#include <string.h>

#define PIDGIN_RESPONSE_CONFIGURE 98121

static void plugin_toggled_stage_two(PurplePlugin *plug, GtkTreeModel *model,
                                  GtkTreeIter *iter, gboolean unload);

static GtkWidget *expander = NULL;
static GtkWidget *plugin_dialog = NULL;

static GtkLabel *plugin_name = NULL;
static GtkTextBuffer *plugin_desc = NULL;
static GtkLabel *plugin_error = NULL;
static GtkLabel *plugin_author = NULL;
static GtkLabel *plugin_website = NULL;
static gchar *plugin_website_uri = NULL;
static GtkLabel *plugin_filename = NULL;

static GtkWidget *pref_button = NULL;
static GHashTable *plugin_pref_dialogs = NULL;

GtkWidget *
pidgin_plugin_get_config_frame(PurplePlugin *plugin)
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
			purple_debug_warning("gtkplugin",
				"Plugin %s contains both, ui_info and "
				"prefs_info preferences; prefs_info will be "
				"ignored.", plugin->info->name);
		}
	}

	if (config == NULL && plugin->info->prefs_info
		&& plugin->info->prefs_info->get_plugin_pref_frame)
	{
		PurplePluginPrefFrame *frame;

		frame = plugin->info->prefs_info->get_plugin_pref_frame(plugin);

		config = pidgin_plugin_pref_create_frame(frame);

		plugin->info->prefs_info->frame = frame;
	}

	return config;
}

void
pidgin_plugins_save(void)
{
	purple_plugins_save_loaded(PIDGIN_PREFS_ROOT "/plugins/loaded");
}

static void
update_plugin_list(void *data)
{
	GtkListStore *ls = GTK_LIST_STORE(data);
	GtkTreeIter iter;
	GList *probes;
	PurplePlugin *plug;

	gtk_list_store_clear(ls);
	purple_plugins_probe(G_MODULE_SUFFIX);

	for (probes = purple_plugins_get_all();
		 probes != NULL;
		 probes = probes->next)
	{
		char *name;
		char *version;
		char *summary;
		char *desc;
		plug = probes->data;

		if (plug->info->type == PURPLE_PLUGIN_LOADER) {
			GList *cur;
			for (cur = PURPLE_PLUGIN_LOADER_INFO(plug)->exts; cur != NULL;
					 cur = cur->next)
				purple_plugins_probe(cur->data);
			continue;
		} else if (plug->info->type != PURPLE_PLUGIN_STANDARD ||
			(plug->info->flags & PURPLE_PLUGIN_FLAG_INVISIBLE)) {
			continue;
		}

		gtk_list_store_append (ls, &iter);

		if (plug->info->name) {
			name = g_markup_escape_text(_(plug->info->name), -1);
		} else {
			char *tmp = g_path_get_basename(plug->path);
			name = g_markup_escape_text(tmp, -1);
			g_free(tmp);
		}
		version = g_markup_escape_text(purple_plugin_get_version(plug), -1);
		summary = g_markup_escape_text(purple_plugin_get_summary(plug), -1);

		desc = g_strdup_printf("<b>%s</b> %s\n%s", name,
				       version,
				       summary);
		g_free(name);
		g_free(version);
		g_free(summary);

		gtk_list_store_set(ls, &iter,
				   0, purple_plugin_is_loaded(plug),
				   1, desc,
				   2, plug,
				   3, purple_plugin_is_unloadable(plug),
				   -1);
		g_free(desc);
	}
}

static void plugin_loading_common(PurplePlugin *plugin, GtkTreeView *view, gboolean loaded)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(view);

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			PurplePlugin *plug;
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

static void plugin_load_cb(PurplePlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, TRUE);
}

static void plugin_unload_cb(PurplePlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, FALSE);
}

static void pref_dialog_response_cb(GtkWidget *d, int response, PurplePlugin *plug)
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

		if (plug->info->prefs_info && plug->info->prefs_info->frame) {
			purple_plugin_pref_frame_destroy(plug->info->prefs_info->frame);
			plug->info->prefs_info->frame = NULL;
		}

		break;
	}
}

static void plugin_unload_confirm_cb(gpointer *data)
{
	PurplePlugin *plugin = (PurplePlugin *)data[0];
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
	PurplePlugin *plug;
	GtkWidget *dialog = NULL;

	gtk_tree_model_get_iter(model, iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, iter, 2, &plug, -1);

	/* Apparently, GTK+ won't honor the sensitive flag on cell renderers for booleans. */
	if (purple_plugin_is_unloadable(plug))
	{
		g_free(iter);
		return;
	}

	if (!purple_plugin_is_loaded(plug))
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		purple_plugin_load(plug);
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
				PurplePlugin *dep_plugin = purple_plugins_find_with_id(dep_name);
				g_return_if_fail(dep_plugin != NULL);

				g_string_append_printf(tmp, "\n\t%s\n", purple_plugin_get_name(dep_plugin));
			}

			cb_data = g_new(gpointer, 3);
			cb_data[0] = plug;
			cb_data[1] = model;
			cb_data[2] = iter;

			purple_request_action(plugin_dialog, NULL,
			                    _("Multiple plugins will be unloaded."),
			                    tmp->str, 0,
								NULL, NULL, NULL,
								cb_data, 2,
			                    _("Unload Plugins"), G_CALLBACK(plugin_unload_confirm_cb),
			                    _("Cancel"), g_free);
			g_string_free(tmp, TRUE);
		}
		else
			plugin_toggled_stage_two(plug, model, iter, TRUE);
	}
}

static void plugin_toggled_stage_two(PurplePlugin *plug, GtkTreeModel *model, GtkTreeIter *iter, gboolean unload)
{
	if (unload)
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		if (!purple_plugin_unload(plug))
		{
			const char *primary = _("Could not unload plugin");
			const char *reload = _("The plugin could not be unloaded now, but will be disabled at the next startup.");

			if (!plug->error)
			{
				purple_notify_warning(NULL, NULL, primary, reload);
			}
			else
			{
				char *tmp = g_strdup_printf("%s\n\n%s", reload, plug->error);
				purple_notify_warning(NULL, NULL, primary, tmp);
				g_free(tmp);
			}

			purple_plugin_disable(plug);
		}

		pidgin_clear_cursor(plugin_dialog);
	}

	gtk_widget_set_sensitive(pref_button,
		purple_plugin_is_loaded(plug)
		&& ((PIDGIN_IS_PIDGIN_PLUGIN(plug) && plug->info->ui_info
			&& PIDGIN_PLUGIN_UI_INFO(plug)->get_config_frame)
		 || (plug->info->prefs_info
			&& plug->info->prefs_info->get_plugin_pref_frame)));

	if (plug->error != NULL)
	{
		gchar *name = g_markup_escape_text(purple_plugin_get_name(plug), -1);

		gchar *error = g_markup_escape_text(plug->error, -1);
		gchar *text;

		text = g_strdup_printf(
			"<b>%s</b> %s\n<span weight=\"bold\" color=\"red\"%s</span>",
			purple_plugin_get_name(plug), purple_plugin_get_version(plug), error);
		gtk_list_store_set(GTK_LIST_STORE (model), iter,
				   1, text,
				   -1);
		g_free(text);

		text = g_strdup_printf(
			"<span weight=\"bold\" color=\"red\">%s</span>",
			error);
		gtk_label_set_markup(plugin_error, text);
		g_free(text);

		g_free(error);
		g_free(name);
	}

	gtk_list_store_set(GTK_LIST_STORE (model), iter,
	                   0, purple_plugin_is_loaded(plug),
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
	gchar *buf, *tmp, *name, *version;
	GtkTreeIter  iter;
	GValue val;
	PurplePlugin *plug;

	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
	{
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

	name = g_markup_escape_text(purple_plugin_get_name(plug), -1);
	version = g_markup_escape_text(purple_plugin_get_version(plug), -1);
	buf = g_strdup_printf(
		"<span size=\"larger\" weight=\"bold\">%s</span> "
		"<span size=\"smaller\">%s</span>",
		name, version);
	gtk_label_set_markup(plugin_name, buf);
	g_free(name);
	g_free(version);
	g_free(buf);

	gtk_text_buffer_set_text(plugin_desc, purple_plugin_get_description(plug), -1);
	gtk_label_set_text(plugin_author, purple_plugin_get_author(plug));
	gtk_label_set_text(plugin_filename, plug->path);

	g_free(plugin_website_uri);
	plugin_website_uri = g_strdup(purple_plugin_get_homepage(plug));
	if (plugin_website_uri)
	{
		tmp = g_markup_escape_text(plugin_website_uri, -1);
		buf = g_strdup_printf("<span underline=\"single\" "
			"foreground=\"blue\">%s</span>", tmp);
		gtk_label_set_markup(plugin_website, buf);
		g_free(tmp);
		g_free(buf);
	}
	else
	{
		gtk_label_set_text(plugin_website, NULL);
	}

	if (plug->error == NULL)
	{
		gtk_label_set_text(plugin_error, NULL);
	}
	else
	{
		tmp = g_markup_escape_text(plug->error, -1);
		buf = g_strdup_printf(
			_("<span foreground=\"red\" weight=\"bold\">"
			  "Error: %s\n"
			  "Check the plugin website for an update."
			  "</span>"),
			tmp);
		gtk_label_set_markup(plugin_error, buf);
		g_free(buf);
		g_free(tmp);
	}

	gtk_widget_set_sensitive(pref_button,
		purple_plugin_is_loaded(plug)
		&& ((PIDGIN_IS_PIDGIN_PLUGIN(plug) && plug->info->ui_info
			&& PIDGIN_PLUGIN_UI_INFO(plug)->get_config_frame)
		 || (plug->info->prefs_info
			&& plug->info->prefs_info->get_plugin_pref_frame)));

	/* Make sure the selected plugin is still visible */
	g_idle_add(ensure_plugin_visible, sel);

	g_value_unset(&val);
}

static void plugin_dialog_response_cb(GtkWidget *d, int response, GtkTreeSelection *sel)
{
	PurplePlugin *plug;
	GtkWidget *dialog, *box;
	GtkTreeModel *model;
	GValue val;
	GtkTreeIter iter;

	switch (response) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		purple_request_close_with_handle(plugin_dialog);
		purple_signals_disconnect_by_handle(plugin_dialog);
		gtk_widget_destroy(d);
		if (plugin_pref_dialogs != NULL) {
			g_hash_table_destroy(plugin_pref_dialogs);
			plugin_pref_dialogs = NULL;
		}
		plugin_dialog = NULL;
		break;
	case PIDGIN_RESPONSE_CONFIGURE:
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

		dialog = gtk_dialog_new_with_buttons(PIDGIN_ALERT_TITLE, GTK_WINDOW(d),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
						     NULL);
		if (plugin_pref_dialogs == NULL)
			plugin_pref_dialogs = g_hash_table_new(NULL, NULL);

		g_hash_table_insert(plugin_pref_dialogs, plug, dialog);

		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(pref_dialog_response_cb), plug);
		gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), box);
		gtk_window_set_role(GTK_WINDOW(dialog), "plugin_config");
		gtk_window_set_title(GTK_WINDOW(dialog), _(purple_plugin_get_name(plug)));
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
	PurplePlugin *plugin;
	GtkTreeModel *model;

	sel = gtk_tree_view_get_selection(view);

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, 2, &plugin, -1);

	if (!purple_plugin_is_loaded(plugin))
		return;

	/* Now show the pref-dialog for the plugin */
	plugin_dialog_response_cb(dialog, PIDGIN_RESPONSE_CONFIGURE, sel);
}

static gboolean
pidgin_plugins_paint_tooltip(GtkWidget *tipwindow, cairo_t *cr, gpointer data)
{
	PangoLayout *layout = g_object_get_data(G_OBJECT(tipwindow), "tooltip-plugin");
	gtk_paint_layout(gtk_widget_get_style(tipwindow), cr, GTK_STATE_NORMAL, FALSE,
			tipwindow, "tooltip",
			6, 6, layout);

	return TRUE;
}

static gboolean
pidgin_plugins_create_tooltip(GtkWidget *tipwindow, GtkTreePath *path,
		gpointer data, int *w, int *h)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW(data);
	PurplePlugin *plugin = NULL;
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	PangoLayout *layout;
	int width, height;
	char *markup, *name, *desc, *author;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return FALSE;

	gtk_tree_model_get(model, &iter, 2, &plugin, -1);

	markup = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>\n<b>%s:</b> %s\n<b>%s:</b> %s",
			name = g_markup_escape_text(purple_plugin_get_name(plugin), -1),
			_("Description"), desc = g_markup_escape_text(purple_plugin_get_description(plugin), -1),
			_("Author"), author = g_markup_escape_text(purple_plugin_get_author(plugin), -1));

	layout = gtk_widget_create_pango_layout(tipwindow, NULL);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 600000);
	pango_layout_get_size(layout, &width, &height);
	g_object_set_data_full(G_OBJECT(tipwindow), "tooltip-plugin", layout, g_object_unref);

	if (w)
		*w = PANGO_PIXELS(width) + 12;
	if (h)
		*h = PANGO_PIXELS(height) + 12;

	g_free(markup);
	g_free(name);
	g_free(desc);
	g_free(author);

	return TRUE;
}

static gboolean
website_button_motion_cb(GtkWidget *button, GdkEventCrossing *event,
                          gpointer unused)
{
	if (plugin_website_uri) {
		pidgin_set_cursor(button, GDK_HAND2);
		return TRUE;
	}
	return FALSE;
}

static gboolean
website_button_clicked_cb(GtkButton *button, GdkEventButton *event,
                          gpointer unused)
{
	if (plugin_website_uri) {
		purple_notify_uri(NULL, plugin_website_uri);
		return TRUE;
	}
	return FALSE;
}

static GtkWidget *
create_details()
{
	GtkBox *vbox = GTK_BOX(gtk_vbox_new(FALSE, 3));
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkWidget *label, *view, *website_button;

	plugin_name = GTK_LABEL(gtk_label_new(NULL));
	gtk_misc_set_alignment(GTK_MISC(plugin_name), 0, 0);
	gtk_label_set_line_wrap(plugin_name, FALSE);
	gtk_label_set_selectable(plugin_name, TRUE);
	gtk_box_pack_start(vbox, GTK_WIDGET(plugin_name), FALSE, FALSE, 0);

	view = gtk_text_view_new();
	plugin_desc = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	g_object_set(view, "wrap-mode", GTK_WRAP_WORD,
	                   "editable",  FALSE,
	                   "left-margin",  PIDGIN_HIG_CAT_SPACE,
	                   "right-margin", PIDGIN_HIG_CAT_SPACE,
	                   NULL);
	gtk_box_pack_start(vbox, view, TRUE, TRUE, 0);

	plugin_error = GTK_LABEL(gtk_label_new(NULL));
	gtk_misc_set_alignment(GTK_MISC(plugin_error), 0, 0);
	gtk_label_set_line_wrap(plugin_error, FALSE);
	gtk_label_set_selectable(plugin_error, TRUE);
	gtk_box_pack_start(vbox, GTK_WIDGET(plugin_error), FALSE, FALSE, 0);

	plugin_author = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(plugin_author, FALSE);
	gtk_misc_set_alignment(GTK_MISC(plugin_author), 0, 0);
	gtk_label_set_selectable(plugin_author, TRUE);
	pidgin_add_widget_to_vbox(vbox, "", sg,
		GTK_WIDGET(plugin_author), TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Written by:</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	website_button = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(website_button), FALSE);

	plugin_website = GTK_LABEL(gtk_label_new(NULL));
	g_object_set(G_OBJECT(plugin_website),
		"ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	gtk_misc_set_alignment(GTK_MISC(plugin_website), 0, 0);
	gtk_container_add(GTK_CONTAINER(website_button),
		GTK_WIDGET(plugin_website));
	g_signal_connect(website_button, "button-release-event",
		G_CALLBACK(website_button_clicked_cb), NULL);
	g_signal_connect(website_button, "enter-notify-event",
		G_CALLBACK(website_button_motion_cb), NULL);
	g_signal_connect(website_button, "leave-notify-event",
		G_CALLBACK(pidgin_clear_cursor), NULL);

	pidgin_add_widget_to_vbox(vbox, "", sg, website_button, TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Web site:</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	plugin_filename = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(plugin_filename, FALSE);
	gtk_misc_set_alignment(GTK_MISC(plugin_filename), 0, 0);
	gtk_label_set_selectable(plugin_filename, TRUE);
	pidgin_add_widget_to_vbox(vbox, "", sg,
		GTK_WIDGET(plugin_filename), TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Filename:</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	g_object_unref(sg);

	return GTK_WIDGET(vbox);
}


void pidgin_plugin_dialog_show()
{
	GtkWidget *event_view;
	GtkListStore *ls;
	GtkCellRenderer *rend, *rendt;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	if (plugin_dialog != NULL) {
		gtk_window_present(GTK_WINDOW(plugin_dialog));
		return;
	}

	plugin_dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(plugin_dialog), _("Plugins"));
	pref_button = gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						_("Configure Pl_ugin"), PIDGIN_RESPONSE_CONFIGURE);
	gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_widget_set_sensitive(pref_button, FALSE);
	gtk_window_set_role(GTK_WINDOW(plugin_dialog), "plugins");

	ls = gtk_list_store_new(4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),
					     1, GTK_SORT_ASCENDING);

	update_plugin_list(ls);

	event_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(event_view), TRUE);

	g_signal_connect(G_OBJECT(event_view), "row-activated",
				G_CALLBACK(show_plugin_prefs_cb), plugin_dialog);

	purple_signal_connect(purple_plugins_get_handle(), "plugin-load", plugin_dialog,
	                    PURPLE_CALLBACK(plugin_load_cb), event_view);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload", plugin_dialog,
	                    PURPLE_CALLBACK(plugin_unload_cb), event_view);

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
	gtk_tree_view_column_set_expand (col, TRUE);
	g_object_set(rendt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 1);
	g_object_unref(G_OBJECT(ls));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(plugin_dialog))),
		pidgin_make_scrollable(event_view, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1), 
		TRUE, TRUE, 0);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(event_view), 1);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(event_view),
				pidgin_tree_view_search_equal_func, NULL, NULL);

	pidgin_tooltip_setup_for_treeview(event_view, event_view,
			pidgin_plugins_create_tooltip,
			pidgin_plugins_paint_tooltip);


	expander = gtk_expander_new(_("<b>Plugin Details</b>"));
	gtk_expander_set_use_markup(GTK_EXPANDER(expander), TRUE);
	gtk_widget_set_sensitive(expander, FALSE);
	gtk_container_add(GTK_CONTAINER(expander), create_details());
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(plugin_dialog))),
	                    expander, FALSE, FALSE, 0);


	g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (prefs_plugin_sel), NULL);
	g_signal_connect(G_OBJECT(plugin_dialog), "response", G_CALLBACK(plugin_dialog_response_cb), sel);
	gtk_window_set_default_size(GTK_WINDOW(plugin_dialog), 430, 530);

	pidgin_auto_parent_window(plugin_dialog);

	gtk_widget_show_all(plugin_dialog);
}
