/**
 * @file gaimrc.c Gaim gtk resource control plugin.
 *
 * Copyright (C) 2005 Etan Reisner <deryni@eden.rutgers.edu>
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
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "version.h"

static guint pref_callback;

static const char *color_prefs[] = {
	"/plugins/gtk/gaimrc/color/GtkWidget::cursor-color",
	"/plugins/gtk/gaimrc/color/GtkWidget::secondary-cursor-color",
	"/plugins/gtk/gaimrc/color/GtkIMHtml::hyperlink-color"
};
static const char *color_prefs_set[] = {
	"/plugins/gtk/gaimrc/set/color/GtkWidget::cursor-color",
	"/plugins/gtk/gaimrc/set/color/GtkWidget::secondary-cursor-color",
	"/plugins/gtk/gaimrc/set/color/GtkIMHtml::hyperlink-color"
};
static const char *color_names[] = {
	N_("Cursor Color"),
	N_("Secondary Cursor Color"),
	N_("Hyperlink Color")
};
static GtkWidget *color_widgets[G_N_ELEMENTS(color_prefs)];

static const char *widget_size_prefs[] = {
	"/plugins/gtk/gaimrc/size/GtkTreeView::expander_size"
};
static const char *widget_size_prefs_set[] = {
	"/plugins/gtk/gaimrc/set/size/GtkTreeView::expander_size"
};
static const char *widget_size_names[] = {
	N_("GtkTreeView Expander Size")
};
static GtkWidget *widget_size_widgets[G_N_ELEMENTS(widget_size_prefs)];

static const char *font_prefs[] = {
	"/plugins/gtk/gaimrc/font/*gaim_gtkconv_entry",
	"/plugins/gtk/gaimrc/font/*gaim_gtkconv_imhtml",
	"/plugins/gtk/gaimrc/font/*gaim_gtklog_imhtml",
	"/plugins/gtk/gaimrc/font/*gaim_gtkrequest_imhtml",
	"/plugins/gtk/gaimrc/font/*gaim_gtknotify_imhtml",
};
static const char *font_prefs_set[] = {
	"/plugins/gtk/gaimrc/set/font/*gaim_gtkconv_entry",
	"/plugins/gtk/gaimrc/set/font/*gaim_gtkconv_imhtml",
	"/plugins/gtk/gaimrc/set/font/*gaim_gtklog_imhtml",
	"/plugins/gtk/gaimrc/set/font/*gaim_gtkrequest_imhtml",
	"/plugins/gtk/gaimrc/set/font/*gaim_gtknotify_imhtml",
};
static const char *font_names[] = {
	N_("Conversation Entry"),
	N_("Conversation History"),
	N_("Log Viewer"),
	N_("Request Dialog"),
	N_("Notify Dialog")
};
static GtkWidget *font_widgets[G_N_ELEMENTS(font_prefs)];

static void
gaimrc_make_changes()
{
	int i;
	GString *style_string = g_string_new("");
	char *prefbase = NULL;

	if (gaim_prefs_get_bool("/plugins/gtk/gaimrc/set/gtk-font-name")) {
		const char *pref = gaim_prefs_get_string("/plugins/gtk/gaimrc/gtk-font-name");
		g_string_append_printf(style_string, "gtk-font-name = \"%s\"\n", pref);
	}

	if (gaim_prefs_get_bool("/plugins/gtk/gaimrc/set/gtk-key-theme-name")) {
		const char *pref = gaim_prefs_get_string("/plugins/gtk/gaimrc/gtk-key-theme-name");
		g_string_append_printf(style_string, "gtk-key-theme-name = \"%s\"\n", pref);
	}

	g_string_append(style_string, "style \"gaimrc_style\" {\n");

	for (i = 0; i < G_N_ELEMENTS(color_prefs); i++) {
		if (gaim_prefs_get_bool(color_prefs_set[i])) {
			prefbase = g_path_get_basename(color_prefs[i]);
			g_string_append_printf(style_string,
			                       "%s = \"%s\"\n", prefbase,
			                       gaim_prefs_get_string(color_prefs[i]));
			g_free(prefbase);
		}
	}

	for (i = 0; i < G_N_ELEMENTS(widget_size_prefs); i++) {
		if (gaim_prefs_get_bool(widget_size_prefs_set[i])) {
			prefbase = g_path_get_basename(widget_size_prefs[i]);
			g_string_append_printf(style_string,
			                       "%s = \"%d\"\n", prefbase,
			                       gaim_prefs_get_int(widget_size_prefs[i]));
			g_free(prefbase);
		}
	}

	g_string_append(style_string, "}");
	g_string_append(style_string, "widget_class \"*\" style \"gaimrc_style\"\n");

	for (i = 0; i < G_N_ELEMENTS(font_prefs); i++) {
		if (gaim_prefs_get_bool(font_prefs_set[i])) {
			prefbase = g_path_get_basename(font_prefs[i]);
			g_string_append_printf(style_string,
			                       "style \"%s_style\"\n"
			                       "{font_name = \"%s\"}\n"
			                       "widget \"%s\""
			                       "style \"%s_style\"\n", prefbase,
			                       gaim_prefs_get_string(font_prefs[i]),
			                       prefbase, prefbase);
			g_free(prefbase);
		}
	}

	gtk_rc_parse_string(style_string->str);

	g_string_free(style_string, TRUE);
}

static void
gaimrc_pref_changed_cb(const char *name, GaimPrefType type, gpointer value,
                       gpointer data)
{
	GString *style_string = g_string_new("");
	char *prefbase = NULL;

	prefbase = g_path_get_basename(name);

	if (strncmp(name, "/plugins/gtk/gaimrc/color", 25) == 0) {
		g_string_printf(style_string,
		                "style \"gaimrc_style\" { %s = \"%s\" }"
		                "widget_class \"*\" style \"gaimrc_style\"",
		                prefbase, (char *)value);
	} else if (strncmp(name, "/plugins/gtk/gaimrc/size", 24) == 0) {
		g_string_printf(style_string,
		                "style \"gaimrc_style\" { %s = \"%d\" }"
		                "widget_class \"*\" style \"gaimrc_style\"",
		                prefbase, GPOINTER_TO_INT(value));
	} else if (strncmp(name, "/plugins/gtk/gaimrc/font", 24) == 0) {
		g_string_printf(style_string, "style \"%s_style\""
		                "{ font_name = \"%s\" } widget \"%s\""
		                "style \"%s_style\"",
		                prefbase, (char *)value, prefbase, prefbase);
	} else if (strncmp(name, "/plugins/gtk/gaimrc/set", 23) == 0) {
		if (value)
			gaimrc_make_changes();
		g_string_free(style_string, TRUE);

		return;
	} else {
		g_string_printf(style_string, "%s = \"%s\"",
		                prefbase, (char *)value);
	}
	gtk_rc_parse_string(style_string->str);

	g_string_free(style_string, TRUE);
	g_free(prefbase);
}

static void
gaimrc_color_response(GtkDialog *color_dialog, gint response, gpointer data)
{
	int subscript = GPOINTER_TO_INT(data);

	if (response == GTK_RESPONSE_OK) {
		GtkWidget *colorsel = GTK_COLOR_SELECTION_DIALOG(color_dialog)->colorsel;
		GdkColor color;
		char colorstr[8];

		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(colorsel), &color);

		g_snprintf(colorstr, sizeof(colorstr), "#%02X%02X%02X",
		           color.red/256, color.green/256, color.blue/256);

		gaim_prefs_set_string(color_prefs[subscript], colorstr);
	}
	gtk_widget_destroy(GTK_WIDGET(color_dialog));
}

static void
gaimrc_set_color(GtkWidget *widget, gpointer data)
{
	GtkWidget *color_dialog = NULL;
	GdkColor color;
	char title[128];
	int subscript = GPOINTER_TO_INT(data);

	g_snprintf(title, sizeof(title), _("Select Color for %s"),
	           _(color_names[GPOINTER_TO_INT(data)]));
	color_dialog = gtk_color_selection_dialog_new(_("Select Color"));
	g_signal_connect(G_OBJECT(color_dialog), "response",
	                 G_CALLBACK(gaimrc_color_response), data);

	if (gdk_color_parse(gaim_prefs_get_string(color_prefs[subscript]),
	                    &color)) {
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(color_dialog)->colorsel), &color);
	}

	gtk_window_present(GTK_WINDOW(color_dialog));
}

static void
gaimrc_font_response(GtkDialog *font_dialog, gint response, gpointer data)
{
	int subscript = GPOINTER_TO_INT(data);

	if (response == GTK_RESPONSE_OK) {
		char *fontname = NULL;

		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_dialog));

		gaim_prefs_set_string(font_prefs[subscript], fontname);
		g_free(fontname);
	}
	gtk_widget_destroy(GTK_WIDGET(font_dialog));
}

static void
gaimrc_set_font(GtkWidget *widget, gpointer data)
{
	GtkWidget *font_dialog = NULL;
	char title[128];
	int subscript = GPOINTER_TO_INT(data);

	g_snprintf(title, sizeof(title), _("Select Font for %s"),
	           _(font_names[subscript]));
	font_dialog = gtk_font_selection_dialog_new(title);
	g_signal_connect(G_OBJECT(font_dialog), "response",
	                 G_CALLBACK(gaimrc_font_response), data);

	/* TODO Figure out a way to test for the presence of a value in the
	 * actual pref
	if (gaim_prefs_get_bool(font_prefs[subscript])) {
		gtk_font_selection_set_font_name(GTK_FONT_SELECTION(GTK_FONT_SELECTION_DIALOG(font_dialog)->fontsel), gaim_prefs_get_string(font_prefs[subscript]));
	}
	*/

	gtk_window_present(GTK_WINDOW(font_dialog));
}

static void
gaimrc_font_response_special(GtkDialog *font_dialog, gint response,
                             gpointer data)
{
	if (response == GTK_RESPONSE_OK) {
		char *fontname = NULL;

		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_dialog));

		gaim_prefs_set_string("/plugins/gtk/gaimrc/gtk-font-name",
		                      fontname);
		g_free(fontname);
	}
	gtk_widget_destroy(GTK_WIDGET(font_dialog));
}

static void
gaimrc_set_font_special(GtkWidget *widget, gpointer data)
{
	GtkWidget *font_dialog = NULL;
	const char *font = NULL;

	font_dialog = gtk_font_selection_dialog_new(_("Select Interface Font"));
	g_signal_connect(G_OBJECT(font_dialog), "response",
	                 G_CALLBACK(gaimrc_font_response_special), NULL);

	font = gaim_prefs_get_string("/plugins/gtk/gaimrc/gtk-font-name");
	/* TODO Figure out a way to test for the presence of a value in the
	 * actual pref
	printf("font - %s.\n", font);
	if (font != NULL && font != "") {
		gtk_font_selection_set_font_name(GTK_FONT_SELECTION(GTK_FONT_SELECTION_DIALOG(font_dialog)->fontsel), gaim_prefs_get_string("/plugins/gtk/gaimrc/gtk-font-name"));
	}
	*/

	gtk_window_present(GTK_WINDOW(font_dialog));
}

static gboolean
gaimrc_plugin_load(GaimPlugin *plugin)
{
	gaimrc_make_changes();

	gaim_prefs_connect_callback(plugin, "/plugins/gtk/gaimrc",
	                            gaimrc_pref_changed_cb, NULL);

	return TRUE;
}

static gboolean
gaimrc_plugin_unload(GaimPlugin *plugin)
{
	gaim_prefs_disconnect_callback(pref_callback);

	return TRUE;
}

static GtkWidget *
gaimrc_get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret = NULL, *frame = NULL, *hbox = NULL;
	/*
	GtkWidget *check = NULL, *widget = NULL, *label = NULL;
	*/
	GtkWidget *check = NULL, *widget = NULL;
	GtkSizeGroup *sg = NULL;
	/*
	char sample[7] = "Sample";
	*/
	int i;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	frame = gaim_gtk_make_frame(ret, "General");
	/* interface font */
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);

	check = gaim_gtk_prefs_checkbox(_("Gtk interface font"),
	                                "/plugins/gtk/gaimrc/set/gtk-font-name",
	                                hbox);
	gtk_size_group_add_widget(sg, check);

	widget = gaim_pixbuf_button_from_stock("", GTK_STOCK_SELECT_FONT,
	                                       GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(widget,
	                         gaim_prefs_get_bool("/plugins/gtk/gaimrc/set/gtk-font-name"));
	g_signal_connect(G_OBJECT(check), "toggled",
	                 G_CALLBACK(gaim_gtk_toggle_sensitive), widget);
	g_signal_connect(G_OBJECT(widget), "clicked",
	                 G_CALLBACK(gaimrc_set_font_special), NULL);

	/* key theme name */
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);

	check = gaim_gtk_prefs_checkbox(_("Gtk text shortcut theme"),
	                                "/plugins/gtk/gaimrc/set/gtk-key-theme-name",
	                                hbox);
	gtk_size_group_add_widget(sg, check);

	widget = gaim_gtk_prefs_labeled_entry(hbox, "",
	                                      "/plugins/gtk/gaimrc/gtk-key-theme-name",
	                                      NULL);
	gtk_widget_set_sensitive(widget,
	                         gaim_prefs_get_bool("/plugins/gtk/gaimrc/set/gtk-key-theme-name"));
	g_signal_connect(G_OBJECT(check), "toggled",
	                 G_CALLBACK(gaim_gtk_toggle_sensitive), widget);

	frame = gaim_gtk_make_frame(ret, "Interface colors");
	/* imhtml stuff */
	for (i = 0; i < G_N_ELEMENTS(color_prefs); i++) {
		hbox = gtk_hbox_new(FALSE, 18);
		gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);

		check = gaim_gtk_prefs_checkbox(_(color_names[i]),
		                                color_prefs_set[i], hbox);
		gtk_size_group_add_widget(sg, check);

		color_widgets[i] = gaim_pixbuf_button_from_stock("", GTK_STOCK_SELECT_COLOR, GAIM_BUTTON_HORIZONTAL);
		gtk_size_group_add_widget(sg, color_widgets[i]);
		gtk_box_pack_start(GTK_BOX(hbox), color_widgets[i], FALSE,
		                   FALSE, 0);
		gtk_widget_set_sensitive(color_widgets[i],
		                         gaim_prefs_get_bool(color_prefs_set[i]));
		g_signal_connect(G_OBJECT(check), "toggled",
		                 G_CALLBACK(gaim_gtk_toggle_sensitive),
		                 color_widgets[i]);
		g_signal_connect(G_OBJECT(color_widgets[i]), "clicked",
		                 G_CALLBACK(gaimrc_set_color),
		                 GINT_TO_POINTER(i));
	}

	frame = gaim_gtk_make_frame(ret, "Widget Sizes");
	/* widget size stuff */
	for (i = 0; i < G_N_ELEMENTS(widget_size_prefs); i++) {
		hbox = gtk_hbox_new(FALSE, 18);
		gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);

		check = gaim_gtk_prefs_checkbox(_(widget_size_names[i]),
		                                widget_size_prefs_set[i], hbox);
		gtk_size_group_add_widget(sg, check);

		widget_size_widgets[i] = gaim_gtk_prefs_labeled_spin_button(hbox, "", widget_size_prefs[i], 0, 50, sg);
		gtk_widget_set_sensitive(widget_size_widgets[i],
		                         gaim_prefs_get_bool(widget_size_prefs_set[i]));
		g_signal_connect(G_OBJECT(check), "toggled",
		                 G_CALLBACK(gaim_gtk_toggle_sensitive),
		                 widget_size_widgets[i]);
	}

	frame = gaim_gtk_make_frame(ret, "Fonts");
	/* imhtml font stuff */
	for (i = 0; i < G_N_ELEMENTS(font_prefs); i++) {
		hbox = gtk_hbox_new(FALSE, 18);
		gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);

		check = gaim_gtk_prefs_checkbox(_(font_names[i]),
		                                font_prefs_set[i], hbox);
		gtk_size_group_add_widget(sg, check);

		font_widgets[i] = gaim_pixbuf_button_from_stock("", GTK_STOCK_SELECT_FONT, GAIM_BUTTON_HORIZONTAL);
		gtk_size_group_add_widget(sg, font_widgets[i]);
		gtk_box_pack_start(GTK_BOX(hbox), font_widgets[i], FALSE,
		                   FALSE, 0);
		gtk_widget_set_sensitive(font_widgets[i],
		                         gaim_prefs_get_bool(font_prefs_set[i]));
		g_signal_connect(G_OBJECT(check), "toggled",
		                 G_CALLBACK(gaim_gtk_toggle_sensitive),
		                 font_widgets[i]);
		g_signal_connect(G_OBJECT(font_widgets[i]), "clicked",
		                 G_CALLBACK(gaimrc_set_font), GINT_TO_POINTER(i));
	}

	gtk_widget_show_all(ret);
	return ret;
}

static GaimGtkPluginUiInfo gaimrc_ui_info =
{
	gaimrc_get_config_frame
};

static GaimPluginInfo gaimrc_info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	"gaimrc",
	N_("Gaim GTK+ Theme Control"),
	VERSION,
	N_("Provides access to commonly used gtkrc settings."),
	N_("Provides access to commonly used gtkrc settings."),
	"Etan Reisner <deryni@eden.rutgers.edu>",
	GAIM_WEBSITE,
	gaimrc_plugin_load,
	gaimrc_plugin_unload,
	NULL,
	&gaimrc_ui_info,
	NULL,
	NULL,
	NULL
};

static void
gaimrc_init(GaimPlugin *plugin)
{
	int i;

	gaim_prefs_add_none("/plugins");
	gaim_prefs_add_none("/plugins/gtk");
	gaim_prefs_add_none("/plugins/gtk/gaimrc");
	gaim_prefs_add_none("/plugins/gtk/gaimrc/set");

	gaim_prefs_add_string("/plugins/gtk/gaimrc/gtk-font-name", "");
	gaim_prefs_add_bool("/plugins/gtk/gaimrc/set/gtk-font-name", FALSE);

	gaim_prefs_add_string("/plugins/gtk/gaimrc/gtk-key-theme-name", "");
	gaim_prefs_add_bool("/plugins/gtk/gaimrc/set/gtk-key-theme-name", FALSE);

	gaim_prefs_add_none("/plugins/gtk/gaimrc/color");
	gaim_prefs_add_none("/plugins/gtk/gaimrc/set/color");
	for (i = 0; i < G_N_ELEMENTS(color_prefs); i++) {
		gaim_prefs_add_string(color_prefs[i], "");
		gaim_prefs_add_bool(color_prefs_set[i], FALSE);
	}

	gaim_prefs_add_none("/plugins/gtk/gaimrc/size");
	gaim_prefs_add_none("/plugins/gtk/gaimrc/set/size");
	for (i = 0; i < G_N_ELEMENTS(widget_size_prefs); i++) {
		gaim_prefs_add_int(widget_size_prefs[i], 0);
		gaim_prefs_add_bool(widget_size_prefs_set[i], FALSE);
	}

	gaim_prefs_add_none("/plugins/gtk/gaimrc/font");
	gaim_prefs_add_none("/plugins/gtk/gaimrc/set/font");
	for (i = 0; i < G_N_ELEMENTS(font_prefs); i++) {
		gaim_prefs_add_string(font_prefs[i], "");
		gaim_prefs_add_bool(font_prefs_set[i], FALSE);
	}
}

GAIM_INIT_PLUGIN(gaimrc, gaimrc_init, gaimrc_info)
