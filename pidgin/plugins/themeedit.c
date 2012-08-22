/* Pidgin
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
#include "version.h"

#include "gtk3compat.h"

#include "theme-manager.h"

#include "gtkblist.h"
#include "gtkblist-theme.h"
#include "gtkutils.h"
#include "gtkplugin.h"

#define PLUGIN_ID "gtk-theme-editor"

#include "themeedit-icon.h"

static gboolean
prop_type_is_color(PidginBlistTheme *theme, const char *prop)
{
	PidginBlistThemeClass *klass = PIDGIN_BLIST_THEME_GET_CLASS(theme);
	GParamSpec *spec = g_object_class_find_property(G_OBJECT_CLASS(klass), prop);

	return G_IS_PARAM_SPEC_BOXED(spec);
}

#ifdef NOT_SADRUL
static void
save_blist_theme(GtkWidget *w, GtkWidget *window)
{
	/* TODO: SAVE! */
	gtk_widget_destroy(window);
}
#endif

static void
close_blist_theme(GtkWidget *w, GtkWidget *window)
{
	gtk_widget_destroy(window);
}

static void
theme_color_selected(GtkDialog *dialog, gint response, const char *prop)
{
	if (response == GTK_RESPONSE_OK) {
		GtkWidget *colorsel;
		GdkColor color;
		PidginBlistTheme *theme;

#if GTK_CHECK_VERSION(2,14,0)
		colorsel =
			gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(dialog));
#else
		colorsel = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
#endif
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(colorsel), &color);

		theme = pidgin_blist_get_theme();

		if (prop_type_is_color(theme, prop)) {
			g_object_set(G_OBJECT(theme), prop, &color, NULL);
		} else {
			PidginThemeFont *font = NULL;
			g_object_get(G_OBJECT(theme), prop, &font, NULL);
			if (!font) {
				font = pidgin_theme_font_new(NULL, &color);
				g_object_set(G_OBJECT(theme), prop, font, NULL);
				pidgin_theme_font_free(font);
			} else {
				pidgin_theme_font_set_color(font, &color);
			}
		}
		pidgin_blist_set_theme(theme);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
theme_font_face_selected(GtkWidget *dialog, gint response, gpointer font)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		const char *fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dialog));
		pidgin_theme_font_set_font_face(font, fontname);
		pidgin_blist_refresh(purple_get_blist());
	}
	gtk_widget_destroy(dialog);
}

static void
theme_font_select_face(GtkWidget *widget, gpointer prop)
{
	GtkWindow *window;
	GtkWidget *dialog;
	PidginBlistTheme *theme;
	PidginThemeFont *font = NULL;
	const char *face;

	theme = pidgin_blist_get_theme();
	g_object_get(G_OBJECT(theme), prop, &font, NULL);

	if (!font) {
		font = pidgin_theme_font_new(NULL, NULL);
		g_object_set(G_OBJECT(theme), prop, font, NULL);
		pidgin_theme_font_free(font);
		g_object_get(G_OBJECT(theme), prop, &font, NULL);
	}

	face = pidgin_theme_font_get_font_face(font);
	window = GTK_WINDOW(gtk_widget_get_toplevel(widget));
	dialog = gtk_font_chooser_dialog_new(_("Select Font"), window);
	if (face && *face)
		gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dialog), face);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(theme_font_face_selected),
			font);
	gtk_widget_show_all(dialog);
}

static void
theme_color_select(GtkWidget *widget, gpointer prop)
{
	GtkWidget *dialog;
	PidginBlistTheme *theme;
	const GdkColor *color = NULL;

	theme = pidgin_blist_get_theme();

	if (prop_type_is_color(theme, prop)) {
		g_object_get(G_OBJECT(theme), prop, &color, NULL);
	} else {
		PidginThemeFont *pair = NULL;
		g_object_get(G_OBJECT(theme), prop, &pair, NULL);
		if (pair)
			color = pidgin_theme_font_get_color(pair);
	}

	dialog = gtk_color_selection_dialog_new(_("Select Color"));
#if GTK_CHECK_VERSION(2,14,0)
	if (color)
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(
			gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(dialog))),
			color);
#else
	if (color)
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel),
				color);
#endif
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(theme_color_selected),
			prop);

	gtk_widget_show_all(dialog);
}

static GtkWidget *
pidgin_theme_create_color_selector(const char *text, const char *blurb, const char *prop,
		GtkSizeGroup *sizegroup)
{
	GtkWidget *color;
	GtkWidget *hbox, *label;

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);

	label = gtk_label_new(_(text));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sizegroup, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(label, blurb);
#endif

	color = pidgin_pixbuf_button_from_stock("", GTK_STOCK_SELECT_COLOR,
			PIDGIN_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(color), "clicked", G_CALLBACK(theme_color_select),
			(gpointer)prop);
	gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 0);

	return hbox;
}

static GtkWidget *
pidgin_theme_create_font_selector(const char *text, const char *blurb, const char *prop,
		GtkSizeGroup *sizegroup)
{
	GtkWidget *color, *font;
	GtkWidget *hbox, *label;

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);

	label = gtk_label_new(_(text));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sizegroup, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(label, blurb);
#endif

	font = pidgin_pixbuf_button_from_stock("", GTK_STOCK_SELECT_FONT,
			PIDGIN_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(font), "clicked", G_CALLBACK(theme_font_select_face),
			(gpointer)prop);
	gtk_box_pack_start(GTK_BOX(hbox), font, FALSE, FALSE, 0);

	color = pidgin_pixbuf_button_from_stock("", GTK_STOCK_SELECT_COLOR,
			PIDGIN_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(color), "clicked", G_CALLBACK(theme_color_select),
			(gpointer)prop);
	gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 0);

	return hbox;
}

static void
pidgin_blist_theme_edit(PurplePluginAction *unused)
{
	GtkWidget *dialog;
	GtkWidget *box;
	GtkSizeGroup *group;
	PidginBlistTheme *theme;
	GObjectClass *klass;
	int i, j;
	static struct {
		const char *header;
		const char *props[12];
	} sections[] = {
		{N_("Contact"), {
					"contact-color",
					"contact",
					"online",
					"away",
					"offline",
					"idle",
					"message",
					"message_nick_said",
					"status",
					NULL
				}
		},
		{N_("Group"), {
				      "expanded-color",
				      "expanded-text",
				      "collapsed-color",
				      "collapsed-text",
				      NULL
			      }
		},
		{ NULL, { } }
	};

	dialog = pidgin_create_dialog(_("Pidgin Buddylist Theme Editor"), 0, "theme-editor-blist", FALSE);
	box = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(dialog), FALSE, PIDGIN_HIG_BOX_SPACE);

	theme = pidgin_blist_get_theme();
	if (!theme) {
		const char *author;
#ifndef _WIN32
		author = getlogin();
#else
		author = "user";
#endif
		theme = g_object_new(PIDGIN_TYPE_BLIST_THEME, "type", "blist",
				"author", author,
				NULL);
		pidgin_blist_set_theme(theme);
	}
	klass = G_OBJECT_CLASS(PIDGIN_BLIST_THEME_GET_CLASS(theme));

	group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	for (i = 0; sections[i].header; i++) {
		GtkWidget *vbox;
		GtkWidget *hbox;
		GParamSpec *spec;

		vbox = pidgin_make_frame(box, _(sections[i].header));
		for (j = 0; sections[i].props[j]; j++) {
			const char *label;
			const char *blurb;
			spec = g_object_class_find_property(klass, sections[i].props[j]);
			label = g_param_spec_get_nick(spec);
			blurb = g_param_spec_get_blurb(spec);
			if (G_IS_PARAM_SPEC_BOXED(spec)) {
				hbox = pidgin_theme_create_color_selector(label, blurb,
						sections[i].props[j], group);
				gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
			} else {
				hbox = pidgin_theme_create_font_selector(label, blurb,
						sections[i].props[j], group);
				gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
			}
		}
	}

#ifdef NOT_SADRUL
	pidgin_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, G_CALLBACK(save_blist_theme), dialog);
#endif
	pidgin_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, G_CALLBACK(close_blist_theme), dialog);

	gtk_widget_show_all(dialog);

	g_object_unref(group);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("Edit Buddylist Theme"), pidgin_blist_theme_edit);
	l = g_list_append(l, act);
	act = purple_plugin_action_new(_("Edit Icon Theme"), pidgin_icon_theme_edit);
	l = g_list_append(l, act);

	return l;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                /**< type           */
	PIDGIN_PLUGIN_TYPE,                    /**< ui_requirement */
	0,                                     /**< flags          */
	NULL,                                  /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,               /**< priority       */

	PLUGIN_ID,                             /**< id             */
	N_("Pidgin Theme Editor"),             /**< name           */
	DISPLAY_VERSION,                       /**< version        */
	/**  summary        */
	N_("Pidgin Theme Editor."),
	/**  description    */
	N_("Pidgin Theme Editor"),
	"Sadrul Habib Chowdhury <imadil@gmail.com>",        /**< author         */
	PURPLE_WEBSITE,                        /**< homepage       */

	plugin_load,                           /**< load           */
	NULL,                                  /**< unload         */
	NULL,                                  /**< destroy        */

	NULL,                                  /**< ui_info        */
	NULL,                                  /**< extra_info     */
	NULL,
	actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(themeeditor, init_plugin, info)
