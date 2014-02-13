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
theme_color_selected(GtkColorButton *button, const char *prop)
{
	GdkColor color;
	PidginBlistTheme *theme;

	pidgin_color_chooser_get_rgb(GTK_COLOR_CHOOSER(button), &color);

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

static void
theme_font_face_selected(GtkFontButton *button, PidginThemeFont *font)
{
	const char *fontname = gtk_font_button_get_font_name(button);
	pidgin_theme_font_set_font_face(font, fontname);
	pidgin_blist_refresh(purple_blist_get_buddy_list());
}

static GtkWidget *
theme_font_select_face_widget(const char *prop)
{
	GtkWidget *widget;
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
	widget = gtk_font_button_new();
	gtk_font_button_set_title(GTK_FONT_BUTTON(widget), _("Select Font"));
	if (face && *face)
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), face);
	g_signal_connect(G_OBJECT(widget), "font-set", G_CALLBACK(theme_font_face_selected),
			font);

	return widget;
}

static GtkWidget *
theme_color_select_widget(const char *prop)
{
	GtkWidget *widget;
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

	widget = gtk_color_button_new();
	gtk_color_button_set_title(GTK_COLOR_BUTTON(widget), _("Select Color"));
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(widget), FALSE);
	if (color)
		pidgin_color_chooser_set_rgb(GTK_COLOR_CHOOSER(widget), color);
	g_signal_connect(G_OBJECT(widget), "color-set",
	                 G_CALLBACK(theme_color_selected), (gpointer)prop);

	return widget;
}

static GtkWidget *
pidgin_theme_create_color_selector(const char *text, const char *blurb, const char *prop,
		GtkSizeGroup *sizegroup)
{
	GtkWidget *color;
	GtkWidget *hbox, *label;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_CAT_SPACE);

	label = gtk_label_new(_(text));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sizegroup, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(label, blurb);

	color = theme_color_select_widget(prop);
	gtk_box_pack_start(GTK_BOX(hbox), color, FALSE, FALSE, 0);

	return hbox;
}

static GtkWidget *
pidgin_theme_create_font_selector(const char *text, const char *blurb, const char *prop,
		GtkSizeGroup *sizegroup)
{
	GtkWidget *color, *font;
	GtkWidget *hbox, *label;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_CAT_SPACE);

	label = gtk_label_new(_(text));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sizegroup, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(label, blurb);

	font = theme_font_select_face_widget(prop);
	gtk_box_pack_start(GTK_BOX(hbox), font, FALSE, FALSE, 0);

	color = theme_color_select_widget(prop);
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
					"message-nick-said",
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

static GList *
actions(PurplePlugin *plugin)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("Edit Buddylist Theme"), pidgin_blist_theme_edit);
	l = g_list_append(l, act);
	act = purple_plugin_action_new(_("Edit Icon Theme"), pidgin_icon_theme_edit);
	l = g_list_append(l, act);

	return l;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Sadrul Habib Chowdhury <imadil@gmail.com>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           PLUGIN_ID,
		"name",         N_("Pidgin Theme Editor"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Theming"),
		"summary",      N_("Pidgin Theme Editor"),
		"description",  N_("Pidgin Theme Editor."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"actions-cb",   actions,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(themeeditor, plugin_query, plugin_load, plugin_unload);
