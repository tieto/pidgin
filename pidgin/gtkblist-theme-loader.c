/*
 * GTKBlistThemeLoader for Pidgin
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

#include <stdlib.h>
#include <string.h>

#include "xmlnode.h"
#include "debug.h"
#include "util.h"

#include "gtkblist-theme-loader.h"
#include "gtkblist-theme.h"

/******************************************************************************
 * Globals
 *****************************************************************************/

#define DEFAULT_TEXT_COLOR "black"

/*****************************************************************************
 * Buddy List Theme Builder
 *****************************************************************************/

static PidginThemeFont *
pidgin_theme_font_parse(xmlnode *node)
{
	const char *font;
	const char *colordesc;
	GdkColor color;

	font = xmlnode_get_attrib(node, "font");

	if ((colordesc = xmlnode_get_attrib(node, "color")) == NULL ||
			!gdk_color_parse(colordesc, &color))
		gdk_color_parse(DEFAULT_TEXT_COLOR, &color);

	return pidgin_theme_font_new(font, &color);
}

static GdkColor *
parse_color(xmlnode *node, const char *tag)
{
	const char *temp = xmlnode_get_attrib(node, tag);
	GdkColor color;

	if (temp && gdk_color_parse(temp, &color)) {
		/* TODO: fix this for GTK+ 3.0 */
		/*gdk_colormap_alloc_color(gdk_colormap_get_system(), &color, FALSE, TRUE);*/
		return gdk_color_copy(&color);
	} else {
		return NULL;
	}
}

static PurpleTheme *
pidgin_blist_loader_build(const gchar *theme_dir)
{
	xmlnode *root_node = NULL, *sub_node, *sub_sub_node;
	gchar *dir, *filename_full, *data = NULL;
	const gchar *temp, *name;
	gboolean success = TRUE;
	GdkColor *bgcolor, *expanded_bgcolor, *collapsed_bgcolor, *contact_color;
	PidginThemeFont *expanded, *collapsed, *contact, *online, *away, *offline, *idle, *message, *message_nick_said, *status;
	PidginBlistLayout layout;
	PidginBlistTheme *theme;
	int i;
	struct {
		const char *tag;
		PidginThemeFont **font;
	} lookups[] = {
		{"contact_text", &contact},
		{"online_text", &online},
		{"away_text", &away},
		{"offline_text", &offline},
		{"idle_text", &idle},
		{"message_text", &message},
		{"message_nick_said_text", &message_nick_said},
		{"status_text", &status},
		{NULL, NULL}
	};

	bgcolor = expanded_bgcolor = collapsed_bgcolor = contact_color = NULL;
	expanded          = NULL;
	collapsed         = NULL;
	contact           = NULL;
	online            = NULL;
	away              = NULL;
	offline           = NULL;
	idle              = NULL;
	message           = NULL;
	message_nick_said = NULL;
	status            = NULL;

	/* Find the theme file */
	g_return_val_if_fail(theme_dir != NULL, NULL);
	dir = g_build_filename(theme_dir, "purple", "blist", NULL);
	filename_full = g_build_filename(dir, "theme.xml", NULL);

	if (g_file_test(filename_full, G_FILE_TEST_IS_REGULAR))
		root_node = xmlnode_from_file(dir, "theme.xml", "buddy list themes", "blist-loader");

	g_free(filename_full);
	if (root_node == NULL) {
		g_free(dir);
		return NULL;
	}

	sub_node = xmlnode_get_child(root_node, "description");
	data = xmlnode_get_data(sub_node);

	name = xmlnode_get_attrib(root_node, "name");

	/* <blist> */
	success = name && purple_strequal(xmlnode_get_attrib(root_node, "type"), "pidgin buddy list");

	if (!success)
		purple_debug_warning("gtkblist-theme-loader", "Missing attribute or problem with the root element\n");

	if (success) {
		if ((success = (sub_node = xmlnode_get_child(root_node, "blist")) != NULL))
			bgcolor = parse_color(sub_node, "color");
		else
			purple_debug_warning("gtkblist-theme-loader", "Missing or problem with tags: <blist>.\n");
	}

	/* <groups> */
	if (success) {
		if ((success = (sub_node = xmlnode_get_child(root_node, "groups")) != NULL
			     && (sub_sub_node = xmlnode_get_child(sub_node, "expanded")) != NULL)) {
			expanded = pidgin_theme_font_parse(sub_sub_node);
			expanded_bgcolor = parse_color(sub_sub_node, "background");
		} else
			purple_debug_warning("gtkblist-theme-loader", "Missing or problem with tags: <groups> <expanded>.\n");
	}

	if (success) {
		if ((success = sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "collapsed")) != NULL)) {
			collapsed = pidgin_theme_font_parse(sub_sub_node);
			collapsed_bgcolor = parse_color(sub_sub_node, "background");
		} else
			purple_debug_warning("gtkblist-theme-loader", "Missing or problem with tags: <groups> <collapsed>.\n");
	}

	/* <buddys> */
	if (success) {
		if ((success = (sub_node = xmlnode_get_child(root_node, "buddys")) != NULL &&
			     (sub_sub_node = xmlnode_get_child(sub_node, "placement")) != NULL)) {

			layout.status_icon = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) : 0;
			layout.text = (temp = xmlnode_get_attrib(sub_sub_node, "name")) != NULL ? atoi(temp) : 1;
			layout.emblem = (temp = xmlnode_get_attrib(sub_sub_node, "emblem")) != NULL ? atoi(temp) : 2;
			layout.protocol_icon = (temp = xmlnode_get_attrib(sub_sub_node, "protocol_icon")) != NULL ? atoi(temp) : 3;
			layout.buddy_icon = (temp = xmlnode_get_attrib(sub_sub_node, "buddy_icon")) != NULL ? atoi(temp) : 4;
			layout.show_status = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) != 0 : 1;

		} else purple_debug_warning("gtkblist-theme-loader", "Missing or problem with tags: <buddys> <placement>.\n");
	}

	if (success) {
		if ((success = (sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "background")) != NULL)))
			contact_color = parse_color(sub_sub_node, "color");
		else
			purple_debug_warning("gtkblist-theme-loader", "Missing or problem with tags: <buddys> <background>.\n");
	}

	for (i = 0; success && lookups[i].tag; i++) {
		if ((success = (sub_node != NULL &&
						(sub_sub_node = xmlnode_get_child(sub_node, lookups[i].tag)) != NULL))) {
			*(lookups[i].font) = pidgin_theme_font_parse(sub_sub_node);
		} else {
			*(lookups[i].font) = NULL;
		}
	}

	/* name is required for theme manager */
	success = (success && xmlnode_get_attrib(root_node, "name") != NULL);

	/* the new theme */
	theme = g_object_new(PIDGIN_TYPE_BLIST_THEME,
			"type", "blist",
			"name", name,
			"author", xmlnode_get_attrib(root_node, "author"),
			"image", xmlnode_get_attrib(root_node, "image"),
			"directory", dir,
			"description", data,
			"background-color", bgcolor,
			"layout", &layout,
			"expanded-color", expanded_bgcolor,
			"expanded-text", expanded,
			"collapsed-color", collapsed_bgcolor,
			"collapsed-text", collapsed,
			"contact-color", contact_color,
			"contact", contact,
			"online", online,
			"away", away,
			"offline", offline,
			"idle", idle,
			"message", message,
			"message_nick_said", message_nick_said,
			"status", status, NULL);

	for (i = 0; lookups[i].tag; i++) {
		if (*lookups[i].font) {
			pidgin_theme_font_free(*lookups[i].font);
		}
	}

	pidgin_theme_font_free(expanded);
	pidgin_theme_font_free(collapsed);

	xmlnode_free(root_node);
	g_free(data);
	g_free(dir);

	/* malformed xml file - also frees all partial data*/
	if (!success) {
		g_object_unref(theme);
		theme = NULL;
	}

	if (bgcolor)
		gdk_color_free(bgcolor);
	if (expanded_bgcolor)
		gdk_color_free(expanded_bgcolor);
	if (collapsed_bgcolor)
		gdk_color_free(collapsed_bgcolor);
	if (contact_color)
		gdk_color_free(contact_color);

	return PURPLE_THEME(theme);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_blist_theme_loader_class_init(PidginBlistThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = pidgin_blist_loader_build;
}

GType
pidgin_blist_theme_loader_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginBlistThemeLoaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_blist_theme_loader_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginBlistThemeLoader),
			0, /* n_preallocs */
			NULL, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME_LOADER,
				"PidginBlistThemeLoader", &info, 0);
	}
	return type;
}
