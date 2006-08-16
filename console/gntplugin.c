#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>

#include "notify.h"

#include "gntgaim.h"
#include "gntplugin.h"

static struct
{
	GntWidget *tree;
	GntWidget *window;
	GntWidget *aboot;
	GntWidget *conf;
} plugins;

static void
decide_conf_button(GaimPlugin *plugin)
{
	if (gaim_plugin_is_loaded(plugin) && 
		((GAIM_IS_GNT_PLUGIN(plugin) &&
			GAIM_GNT_PLUGIN_UI_INFO(plugin) != NULL) ||
		(plugin->info->prefs_info &&
			plugin->info->prefs_info->get_plugin_pref_frame)))
		gnt_widget_set_visible(plugins.conf, TRUE);
	else
		gnt_widget_set_visible(plugins.conf, FALSE);

	gnt_box_readjust(GNT_BOX(plugins.window));
	gnt_widget_draw(plugins.window);
}

static void
plugin_toggled_cb(GntWidget *tree, GaimPlugin *plugin, gpointer null)
{
	if (gnt_tree_get_choice(GNT_TREE(tree), plugin))
	{
		if(!gaim_plugin_load(plugin))
			gaim_notify_error(NULL, "ERROR", "loading plugin failed", NULL);
	}
	else
	{
		if (!gaim_plugin_unload(plugin))
			gaim_notify_error(NULL, "ERROR", "unloading plugin failed", NULL);
	}
	decide_conf_button(plugin);
	gg_plugins_save_loaded();
}

/* Xerox */
void
gg_plugins_save_loaded(void)
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

	gaim_prefs_set_string_list("/gaim/gnt/plugins/loaded", files);
	g_list_free(files);
}

static void
selection_changed(GntWidget *widget, gpointer old, gpointer current, gpointer null)
{
	GaimPlugin *plugin = current;
	char *text;

	/* XXX: Use formatting and stuff */
	gnt_text_view_clear(GNT_TEXT_VIEW(plugins.aboot));
	text = g_strdup_printf(_("Name: %s\nVersion: %s\nDescription: %s\nAuthor: %s\nWebsite: %s\nFilename: %s\n"),
			plugin->info->name, plugin->info->version, plugin->info->description,
			plugin->info->author, plugin->info->homepage, plugin->path);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(plugins.aboot),
			text, GNT_TEXT_FLAG_NORMAL);
	gnt_text_view_scroll(GNT_TEXT_VIEW(plugins.aboot), 0);
	g_free(text);
	decide_conf_button(plugin);
}

static void
reset_plugin_window(GntWidget *window, gpointer null)
{
	plugins.window = NULL;
	plugins.tree = NULL;
	plugins.aboot = NULL;
}

static int
plugin_compare(GaimPlugin *p1, GaimPlugin *p2)
{
	char *s1 = g_utf8_strup(p1->info->name, -1);
	char *s2 = g_utf8_strup(p2->info->name, -1);
	int ret = g_utf8_collate(s1, s2);
	g_free(s1);
	g_free(s2);
	return ret;
}

static void
configure_plugin_cb(GntWidget *button, gpointer null)
{
	GaimPlugin *plugin;
	GGPluginFrame callback;

	g_return_if_fail(plugins.tree != NULL);

	plugin = gnt_tree_get_selection_data(GNT_TREE(plugins.tree));
	if (!gaim_plugin_is_loaded(plugin))
	{
		gaim_notify_error(plugin, _("Error"),
			_("Plugin need to be loaded before you can configure it."), NULL);
		return;
	}

	if (GAIM_IS_GNT_PLUGIN(plugin) &&
			(callback = GAIM_GNT_PLUGIN_UI_INFO(plugin)) != NULL)
	{
		GntWidget *window = gnt_vbox_new(FALSE);
		GntWidget *box, *button;

		gnt_box_set_toplevel(GNT_BOX(window), TRUE);
		gnt_box_set_title(GNT_BOX(window), plugin->info->name);
		gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

		box = callback();
		gnt_box_add_widget(GNT_BOX(window), box);

		box = gnt_hbox_new(FALSE);
		gnt_box_add_widget(GNT_BOX(window), box);

		button = gnt_button_new(_("Close"));
		gnt_box_add_widget(GNT_BOX(box), button);
		g_signal_connect_swapped(G_OBJECT(button), "activate",
				G_CALLBACK(gnt_widget_destroy), window);

		gnt_widget_show(window);  /* XXX: This window needs to be closed when the plugin is unloaded */
	}
	else if (plugin->info->prefs_info &&
			plugin->info->prefs_info->get_plugin_pref_frame)
	{
		gaim_notify_info(plugin, _("..."),
			_("Still need to do something about this."), NULL);
		return;
	}
	else
	{
		gaim_notify_info(plugin, _("Error"),
			_("No configuration options for this plugin."), NULL);
		return;
	}
}

void gg_plugins_show_all()
{
	GntWidget *window, *tree, *box, *aboot, *button;
	GList *iter;
	if (plugins.window)
		return;

	gaim_plugins_probe(G_MODULE_SUFFIX);

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
	gnt_widget_set_size(aboot, 40, 20);
	gnt_box_add_widget(GNT_BOX(box), aboot);

	for (iter = gaim_plugins_get_all(); iter; iter = iter->next)
	{
		GaimPlugin *plug = iter->data;

		if (plug->info->type != GAIM_PLUGIN_STANDARD ||
			(plug->info->flags & GAIM_PLUGIN_FLAG_INVISIBLE) ||
			plug->error)
			continue;

		gnt_tree_add_choice(GNT_TREE(tree), plug,
				gnt_tree_create_row(GNT_TREE(tree), plug->info->name), NULL, NULL);
		gnt_tree_set_choice(GNT_TREE(tree), plug, gaim_plugin_is_loaded(plug));
	}
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 30);
	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(plugin_toggled_cb), NULL);
	g_signal_connect(G_OBJECT(tree), "selection_changed", G_CALLBACK(selection_changed), NULL);

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);

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

