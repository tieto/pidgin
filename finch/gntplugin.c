/**
 * @file gntplugin.c GNT Plugins API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <internal.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>
#include <gntutils.h>

#include "finch.h"

#include "debug.h"
#include "notify.h"
#include "request.h"

#include "gntplugin.h"
#include "gntrequest.h"

#define FINCH_PLUGIN_INFO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), FINCH_TYPE_PLUGIN_INFO, FinchPluginInfoPrivate))

typedef struct
{
	FinchPluginFrame get_pref_frame;
} FinchPluginInfoPrivate;

static struct
{
	GntWidget *tree;
	GntWidget *window;
	GntWidget *aboot;
	GntWidget *conf;
} plugins;

static GHashTable *confwins;

static GntWidget *process_pref_frame(PurplePluginPrefFrame *frame);

/* Class initializer function */
static void finch_plugin_info_class_init(FinchPluginInfoClass *klass)
{
	g_type_class_add_private(klass, sizeof(FinchPluginInfoPrivate));
}

GType
finch_plugin_info_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(FinchPluginInfoClass),
			.class_init = (GClassInitFunc)finch_plugin_info_class_init,
			.instance_size = sizeof(FinchPluginInfo),
		};

		type = g_type_register_static(PURPLE_TYPE_PLUGIN_INFO,
		                              "FinchPluginInfo", &info, 0);
	}

	return type;
}

static void
free_stringlist(GList *list)
{
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static void
decide_conf_button(GPluginPlugin *plugin)
{
	gboolean visible = FALSE;
	GPluginPluginInfo *info = gplugin_plugin_get_info(plugin);

	if (purple_plugin_is_loaded(plugin)) {
		if (FINCH_IS_PLUGIN_INFO(info)) {
			FinchPluginInfoPrivate *priv = FINCH_PLUGIN_INFO_GET_PRIVATE(info);
			if (priv->get_pref_frame)
				visible = TRUE;
		} else if (purple_plugin_info_get_pref_frame_callback(PURPLE_PLUGIN_INFO(info))) {
			visible = TRUE;
		}
	}
	g_object_unref(info);

	gnt_widget_set_visible(plugins.conf, visible);
	gnt_box_readjust(GNT_BOX(plugins.window));
	gnt_widget_draw(plugins.window);
}

static void
plugin_toggled_cb(GntWidget *tree, GPluginPlugin *plugin, gpointer null)
{
	if (gnt_tree_get_choice(GNT_TREE(tree), plugin))
	{
		if (!purple_plugin_load(plugin)) {
			purple_notify_error(NULL, _("ERROR"), _("loading plugin failed"), NULL);
			gnt_tree_set_choice(GNT_TREE(tree), plugin, FALSE);
		}
	}
	else
	{
		GntWidget *win;

		if (!purple_plugin_unload(plugin)) {
			purple_notify_error(NULL, _("ERROR"), _("unloading plugin failed"), NULL);
			purple_plugin_disable(plugin);
			gnt_tree_set_choice(GNT_TREE(tree), plugin, TRUE);
		}

		if (confwins && (win = g_hash_table_lookup(confwins, plugin)) != NULL)
		{
			gnt_widget_destroy(win);
		}
	}
	decide_conf_button(plugin);
	finch_plugins_save_loaded();
}

/* Xerox */
void
finch_plugins_save_loaded(void)
{
	purple_plugins_save_loaded("/finch/plugins/loaded");
}

static void
selection_changed(GntWidget *widget, gpointer old, gpointer current, gpointer null)
{
	GPluginPlugin *plugin = current;
	GPluginPluginInfo *info;
	char *text;
	GList *list = NULL, *iter = NULL;

	if (!plugin)
		return;

	/* If the selected plugin was unseen before, mark it as seen. But save the list
	 * only when the plugin list is closed. So if the user enables a plugin, and it
	 * crashes, it won't get marked as seen so the user can fix the bug and still
	 * quickly find the plugin in the list.
	 * I probably mean 'plugin developers' by 'users' here. */
	list = g_object_get_data(G_OBJECT(widget), "seen-list");
	if (list)
		iter = g_list_find_custom(list, gplugin_plugin_get_filename(plugin),
					(GCompareFunc)strcmp);
	if (!iter) {
		list = g_list_prepend(list, g_strdup(gplugin_plugin_get_filename(plugin)));
		g_object_set_data(G_OBJECT(widget), "seen-list", list);
	}

	info = gplugin_plugin_get_info(plugin);

	/* XXX: Use formatting and stuff */
	gnt_text_view_clear(GNT_TEXT_VIEW(plugins.aboot));
	text = g_strdup_printf(_("Name: %s\nVersion: %s\nDescription: %s\nAuthor: %s\nWebsite: %s\nFilename: %s\n"),
			SAFE(_(gplugin_plugin_info_get_name(info))),
			SAFE(_(gplugin_plugin_info_get_version(info))),
			SAFE(_(gplugin_plugin_info_get_description(info))),
			SAFE(_(gplugin_plugin_info_get_author(info))),
			SAFE(_(gplugin_plugin_info_get_website(info))),
			SAFE(gplugin_plugin_get_filename(plugin)));

	g_object_unref(info);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(plugins.aboot),
			text, GNT_TEXT_FLAG_NORMAL);
	gnt_text_view_scroll(GNT_TEXT_VIEW(plugins.aboot), 0);
	g_free(text);
	decide_conf_button(plugin);
}

static void
reset_plugin_window(GntWidget *window, gpointer null)
{
	GList *list = g_object_get_data(G_OBJECT(plugins.tree), "seen-list");
	purple_prefs_set_path_list("/finch/plugins/seen", list);
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);

	plugins.window = NULL;
	plugins.tree = NULL;
	plugins.aboot = NULL;
}

static int
plugin_compare(GPluginPlugin *p1, GPluginPlugin *p2)
{
	GPluginPluginInfo *info1 = gplugin_plugin_get_info(p1);
	GPluginPluginInfo *info2 = gplugin_plugin_get_info(p2);
	char *s1 = g_utf8_strup(gplugin_plugin_info_get_name(info1), -1);
	char *s2 = g_utf8_strup(gplugin_plugin_info_get_name(info2), -1);

	int ret = g_utf8_collate(s1, s2);

	g_free(s1);
	g_free(s2);
	g_object_unref(info1);
	g_object_unref(info2);

	return ret;
}

static void
confwin_init(void)
{
	confwins = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void
remove_confwin(GntWidget *window, gpointer plugin)
{
	g_hash_table_remove(confwins, plugin);
}

static void
configure_plugin_cb(GntWidget *button, gpointer null)
{
	GPluginPlugin *plugin;
	PurplePluginInfo *info;
	FinchPluginInfoPrivate *priv = NULL;
	FinchPluginFrame callback;

	g_return_if_fail(plugins.tree != NULL);

	plugin = gnt_tree_get_selection_data(GNT_TREE(plugins.tree));
	if (!purple_plugin_is_loaded(plugin))
	{
		purple_notify_error(plugin, _("Error"),
			_("Plugin need to be loaded before you can configure it."), NULL);
		return;
	}

	if (confwins && g_hash_table_lookup(confwins, plugin))
		return;

	info = PURPLE_PLUGIN_INFO(gplugin_plugin_get_info(plugin));
	if (FINCH_IS_PLUGIN_INFO(info))
		priv = FINCH_PLUGIN_INFO_GET_PRIVATE(info);

	if (priv && (callback = priv->get_pref_frame) != NULL)
	{
		GntWidget *window = gnt_vbox_new(FALSE);
		GntWidget *box, *button;

		gnt_box_set_toplevel(GNT_BOX(window), TRUE);
		gnt_box_set_title(GNT_BOX(window),
				gplugin_plugin_info_get_name(GPLUGIN_PLUGIN_INFO(info)));
		gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

		box = callback();
		gnt_box_add_widget(GNT_BOX(window), box);

		box = gnt_hbox_new(FALSE);
		gnt_box_add_widget(GNT_BOX(window), box);

		button = gnt_button_new(_("Close"));
		gnt_box_add_widget(GNT_BOX(box), button);
		g_signal_connect_swapped(G_OBJECT(button), "activate",
				G_CALLBACK(gnt_widget_destroy), window);
		g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(remove_confwin), plugin);

		gnt_widget_show(window);

		if (confwins == NULL)
			confwin_init();
		g_hash_table_insert(confwins, plugin, window);
	}
	else if (purple_plugin_info_get_pref_frame_callback(info))
	{
		PurplePluginPrefFrameCallback get_pref_frame = purple_plugin_info_get_pref_frame_callback(info);
		GntWidget *win = process_pref_frame(get_pref_frame(plugin));
		if (confwins == NULL)
			confwin_init();
		g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(remove_confwin), plugin);
		g_hash_table_insert(confwins, plugin, win);
	}
	else
	{
		purple_notify_info(plugin, _("Error"),
			_("No configuration options for this plugin."), NULL);
	}

	g_object_unref(info);
}

#if 0
static void
install_selected_file_cb(gpointer handle, const char *filename)
{
	/* Try to init the selected file.
	 * If it succeeds, try to make a copy of the file in $USERDIR/plugins/.
	 * If the copy succeeds, unload and destroy the plugin in the original
	 *  location and init+load the new one.
	 * Select the plugin in the plugin list.
	 */
	char *path;
	GPluginPlugin *plugin;

	g_return_if_fail(plugins.window);

	plugin = purple_plugin_probe(filename);
	if (!plugin) {
		purple_notify_error(handle, _("Error loading plugin"),
				_("The selected file is not a valid plugin."),
				_("Please open the debug window and try again to see the exact error message."));
		return;
	}
	if (g_list_find(gnt_tree_get_rows(GNT_TREE(plugins.tree)), plugin)) {
		purple_plugin_load(plugin);
		gnt_tree_set_choice(GNT_TREE(plugins.tree), plugin, purple_plugin_is_loaded(plugin));
		gnt_tree_set_selected(GNT_TREE(plugins.tree), plugin);
		return;
	}

	path = g_build_filename(purple_user_dir(), "plugins", NULL);
	if (purple_build_dir(path, S_IRUSR | S_IWUSR | S_IXUSR) == 0) {
		char *content = NULL;
		gsize length = 0;

		if (g_file_get_contents(filename, &content, &length, NULL)) {
			char *file = g_path_get_basename(filename);
			g_free(path);
			path = g_build_filename(purple_user_dir(), "plugins", file, NULL);
			if (purple_util_write_data_to_file_absolute(path, content, length)) {
				purple_plugin_destroy(plugin);
				plugin = purple_plugin_probe(path);
				if (!plugin) {
					purple_debug_warning("gntplugin", "This is really strange. %s can be loaded, but %s can't!\n",
							filename, path);
					g_unlink(path);
					plugin = purple_plugin_probe(filename);
				}
			} else {
			}
		}
		g_free(content);
	}
	g_free(path);

	purple_plugin_load(plugin);

	if (plugin->info->type == PURPLE_PLUGIN_LOADER) {
		GList *cur;
		for (cur = PURPLE_PLUGIN_LOADER_INFO(plugin)->exts; cur != NULL;
				cur = cur->next)
			purple_plugins_probe(cur->data);
		return;
	}

	if (plugin->info->type != PURPLE_PLUGIN_STANDARD ||
			(plugin->info->flags & PURPLE_PLUGIN_FLAG_INVISIBLE) ||
			plugin->error)
		return;

	gnt_tree_add_choice(GNT_TREE(plugins.tree), plugin,
			gnt_tree_create_row(GNT_TREE(plugins.tree), plugin->info->name), NULL, NULL);
	gnt_tree_set_choice(GNT_TREE(plugins.tree), plugin, purple_plugin_is_loaded(plugin));
	gnt_tree_set_row_flags(GNT_TREE(plugins.tree), plugin, GNT_TEXT_FLAG_BOLD);
	gnt_tree_set_selected(GNT_TREE(plugins.tree), plugin);
}

static void
install_plugin_cb(GntWidget *w, gpointer null)
{
	static int handle;

	purple_request_close_with_handle(&handle);
	purple_request_file(&handle, _("Select plugin to install"), NULL,
			FALSE, G_CALLBACK(install_selected_file_cb), NULL,
			NULL, NULL, NULL, &handle);
	g_signal_connect_swapped(G_OBJECT(w), "destroy", G_CALLBACK(purple_request_close_with_handle), &handle);
}
#endif

void finch_plugins_show_all(void)
{
	GntWidget *window, *tree, *box, *aboot, *button;
	GList *plugin_list, *iter;
	GList *seen;

	if (plugins.window) {
		gnt_window_present(plugins.window);
		return;
	}

	gplugin_plugin_manager_refresh();

	plugins.window = window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("Plugins"));
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new(_("You can (un)load plugins from the following list.")));
	gnt_box_add_widget(GNT_BOX(window), gnt_hline_new());

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);
	gnt_box_add_widget(GNT_BOX(window), gnt_hline_new());

	gnt_box_set_pad(GNT_BOX(box), 0);
	plugins.tree = tree = gnt_tree_new();
	gnt_tree_set_compare_func(GNT_TREE(tree), (GCompareFunc)plugin_compare);
	GNT_WIDGET_SET_FLAGS(tree, GNT_WIDGET_NO_BORDER);
	gnt_box_add_widget(GNT_BOX(box), tree);
	gnt_box_add_widget(GNT_BOX(box), gnt_vline_new());

	plugins.aboot = aboot = gnt_text_view_new();
	gnt_text_view_set_flag(GNT_TEXT_VIEW(aboot), GNT_TEXT_VIEW_TOP_ALIGN);
	gnt_widget_set_size(aboot, 40, 20);
	gnt_box_add_widget(GNT_BOX(box), aboot);

	seen = purple_prefs_get_path_list("/finch/plugins/seen");

	plugin_list = purple_plugins_find_all();
	for (iter = plugin_list; iter; iter = iter->next)
	{
		GPluginPlugin *plug = iter->data;
#if 0
		if (plug->info->type == PURPLE_PLUGIN_LOADER) {
			GList *cur;
			for (cur = PURPLE_PLUGIN_LOADER_INFO(plug)->exts; cur != NULL;
					 cur = cur->next)
				purple_plugins_probe(cur->data);
			continue;
		}

		if (plug->info->type != PURPLE_PLUGIN_STANDARD ||
			(plug->info->flags & PURPLE_PLUGIN_FLAG_INVISIBLE) ||
			plug->error)
			continue;
#endif
		GPluginPluginInfo *info = gplugin_plugin_get_info(plug);

		gnt_tree_add_choice(GNT_TREE(tree), plug,
				gnt_tree_create_row(GNT_TREE(tree), gplugin_plugin_info_get_name(info)), NULL, NULL);
		gnt_tree_set_choice(GNT_TREE(tree), plug, purple_plugin_is_loaded(plug));
		if (!g_list_find_custom(seen, gplugin_plugin_get_filename(plug),
				(GCompareFunc)strcmp))
			gnt_tree_set_row_flags(GNT_TREE(tree), plug, GNT_TEXT_FLAG_BOLD);

		g_object_unref(info);
	}
	purple_plugins_free_found_list(plugin_list);

	gnt_tree_set_col_width(GNT_TREE(tree), 0, 30);
	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(plugin_toggled_cb), NULL);
	g_signal_connect(G_OBJECT(tree), "selection_changed", G_CALLBACK(selection_changed), NULL);
	g_object_set_data(G_OBJECT(tree), "seen-list", seen);

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);

#if 0
	button = gnt_button_new(_("Install Plugin..."));
	gnt_box_add_widget(GNT_BOX(box), button);
	gnt_util_set_trigger_widget(GNT_WIDGET(tree), GNT_KEY_INS, button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(install_plugin_cb), NULL);
#endif

	button = gnt_button_new(_("Close"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate",
			G_CALLBACK(gnt_widget_destroy), window);

	plugins.conf = button = gnt_button_new(_("Configure Plugin"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(configure_plugin_cb), NULL);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(reset_plugin_window), NULL);

	gnt_widget_show(window);

	decide_conf_button(gnt_tree_get_selection_data(GNT_TREE(tree)));
}

static GntWidget*
process_pref_frame(PurplePluginPrefFrame *frame)
{
	PurpleRequestField *field;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group = NULL;
	GList *prefs;
	GList *stringlist = NULL;
	GntWidget *ret = NULL;

	fields = purple_request_fields_new();

	for (prefs = purple_plugin_pref_frame_get_prefs(frame); prefs; prefs = prefs->next) {
		PurplePluginPref *pref = prefs->data;
		PurplePrefType type;
		const char *name = purple_plugin_pref_get_name(pref);
		const char *label = purple_plugin_pref_get_label(pref);
		if(name == NULL) {
			if(label == NULL)
				continue;

			if(purple_plugin_pref_get_type(pref) == PURPLE_PLUGIN_PREF_INFO) {
				field = purple_request_field_label_new("*", purple_plugin_pref_get_label(pref));
				purple_request_field_group_add_field(group, field);
			} else {
				group = purple_request_field_group_new(label);
				purple_request_fields_add_group(fields, group);
			}
			continue;
		}

		field = NULL;
		type = purple_prefs_get_type(name);
		if(purple_plugin_pref_get_type(pref) == PURPLE_PLUGIN_PREF_CHOICE) {
			GList *list = purple_plugin_pref_get_choices(pref);
			gpointer current_value = NULL;

			switch(type) {
				case PURPLE_PREF_BOOLEAN:
					current_value = g_strdup_printf("%d", (int)purple_prefs_get_bool(name));
					break;
				case PURPLE_PREF_INT:
					current_value = g_strdup_printf("%d", (int)purple_prefs_get_int(name));
					break;
				case PURPLE_PREF_STRING:
					current_value = g_strdup(purple_prefs_get_string(name));
					break;
				default:
					continue;
			}

			field = purple_request_field_list_new(name, label);
			purple_request_field_list_set_multi_select(field, FALSE);
			while (list && list->next) {
				const char *label = list->data;
				char *value = NULL;
				switch(type) {
					case PURPLE_PREF_BOOLEAN:
						value = g_strdup_printf("%d", GPOINTER_TO_INT(list->next->data));
						break;
					case PURPLE_PREF_INT:
						value = g_strdup_printf("%d", GPOINTER_TO_INT(list->next->data));
						break;
					case PURPLE_PREF_STRING:
						value = g_strdup(list->next->data);
						break;
					default:
						break;
				}
				stringlist = g_list_prepend(stringlist, value);
				purple_request_field_list_add_icon(field, label, NULL, value);
				if (strcmp(value, current_value) == 0)
					purple_request_field_list_add_selected(field, label);
				list = list->next->next;
			}
			g_free(current_value);
		} else {
			switch(type) {
				case PURPLE_PREF_BOOLEAN:
					field = purple_request_field_bool_new(name, label, purple_prefs_get_bool(name));
					break;
				case PURPLE_PREF_INT:
					field = purple_request_field_int_new(name, label, purple_prefs_get_int(name), INT_MIN, INT_MAX);
					break;
				case PURPLE_PREF_STRING:
					field = purple_request_field_string_new(name, label, purple_prefs_get_string(name),
							purple_plugin_pref_get_format_type(pref) & PURPLE_STRING_FORMAT_TYPE_MULTILINE);
					break;
				default:
					break;
			}
		}

		if (field) {
			if (group == NULL) {
				group = purple_request_field_group_new(_("Preferences"));
				purple_request_fields_add_group(fields, group);
			}
			purple_request_field_group_add_field(group, field);
		}
	}

	ret = purple_request_fields(NULL, _("Preferences"), NULL, NULL, fields,
			_("Save"), G_CALLBACK(finch_request_save_in_prefs), _("Cancel"), NULL,
			NULL, NULL, NULL,
			NULL);
	g_signal_connect_swapped(G_OBJECT(ret), "destroy", G_CALLBACK(free_stringlist), stringlist);
	return ret;
}

