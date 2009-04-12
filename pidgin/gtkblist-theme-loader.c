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

#include <stdlib.h>
#include <string.h>

#include "xmlnode.h"

#include "gtkblist-theme-loader.h"
#include "gtkblist-theme.h"

/******************************************************************************
 * Globals
 *****************************************************************************/

#define DEFAULT_TEXT_COLOR "black"

/*****************************************************************************
 * Buddy List Theme Builder
 *****************************************************************************/

static PurpleTheme *
pidgin_blist_loader_build(const gchar *dir)
{
	xmlnode *root_node = NULL, *sub_node, *sub_sub_node;
	gchar *filename_full, *data;
	const gchar *temp;
	gboolean success = TRUE;
	GdkColor bgcolor, expanded_bgcolor, collapsed_bgcolor, contact_color;
	GdkColor color;
	FontColorPair expanded, collapsed, contact, online, away, offline, idle, message, message_nick_said, status;
	PidginBlistLayout layout;
	PidginBlistTheme *theme;

	/* Find the theme file */
	g_return_val_if_fail(dir != NULL, NULL);
	filename_full = g_build_filename(dir, "theme.xml", NULL);

	if (g_file_test(filename_full, G_FILE_TEST_IS_REGULAR))
		root_node = xmlnode_from_file(dir, "theme.xml", "buddy list themes", "blist-loader");

	g_free(filename_full);
	g_return_val_if_fail(root_node != NULL, NULL);

	sub_node = xmlnode_get_child(root_node, "description");
	data = xmlnode_get_data(sub_node);

	/* <blist> */
	if ((success = (sub_node = xmlnode_get_child(root_node, "blist")) != NULL)) {
		if ((temp = xmlnode_get_attrib(sub_node, "color")) != NULL && gdk_color_parse(temp, &bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &bgcolor, FALSE, TRUE);
		else
			memset(&bgcolor, 0, sizeof(GdkColor));
	}

	/* <groups> */
	if ((success = (success && (sub_node = xmlnode_get_child(root_node, "groups")) != NULL
		     && (sub_sub_node = xmlnode_get_child(sub_node, "expanded")) != NULL)))
	{
		expanded.font = xmlnode_get_attrib(sub_sub_node, "font");

		if ((temp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(temp, &color))
			expanded.color = temp;
		else expanded.color = DEFAULT_TEXT_COLOR;

		if ((temp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(temp, &expanded_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &expanded_bgcolor, FALSE, TRUE);
		else
			memset(&expanded_bgcolor, 0, sizeof(GdkColor));
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "collapsed")) != NULL)))
	{
		collapsed.font = xmlnode_get_attrib(sub_sub_node, "font");

		if((temp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(temp, &color))
			collapsed.color = temp;
		else collapsed.color = DEFAULT_TEXT_COLOR;

		if ((temp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(temp, &collapsed_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &collapsed_bgcolor, FALSE, TRUE);
		else
			memset(&collapsed_bgcolor, 0, sizeof(GdkColor));
	}

	/* <buddys> */
	if ((success = (success && (sub_node = xmlnode_get_child(root_node, "buddys")) != NULL &&
		     (sub_sub_node = xmlnode_get_child(sub_node, "placement")) != NULL)))
	{
		layout.status_icon = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) : 0;
		layout.text = (temp = xmlnode_get_attrib(sub_sub_node, "name")) != NULL ? atoi(temp) : 1;
		layout.emblem = (temp = xmlnode_get_attrib(sub_sub_node, "emblem")) != NULL ? atoi(temp) : 2;
		layout.protocol_icon = (temp = xmlnode_get_attrib(sub_sub_node, "protocol_icon")) != NULL ? atoi(temp) : 3;
		layout.buddy_icon = (temp = xmlnode_get_attrib(sub_sub_node, "buddy_icon")) != NULL ? atoi(temp) : 4;
		layout.show_status = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) != 0 : 1;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "background")) != NULL))) {
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &contact_color))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &contact_color, FALSE, TRUE);
		else
			memset(&contact_color, 0, sizeof(GdkColor));
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "contact_text")) != NULL))) {
		contact.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			contact.color = temp;
		else contact.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "online_text")) != NULL))) {
		online.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			online.color = temp;
		else online.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "away_text")) != NULL))) {
		away.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			away.color = temp;
		else away.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "offline_text")) != NULL))) {
		offline.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			offline.color = temp;
		else offline.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "idle_text")) != NULL))) {
		idle.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			idle.color = temp;
		else idle.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "message_text")) != NULL))) {
		message.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			message.color = temp;
		else message.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "message_nick_said_text")) != NULL))) {
		message_nick_said.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			message_nick_said.color = temp;
		else message_nick_said.color = DEFAULT_TEXT_COLOR;
	}

	if ((success = (success && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "status_text")) != NULL))) {
		status.font = xmlnode_get_attrib(sub_sub_node, "font");
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			status.color = temp;
		else status.color = DEFAULT_TEXT_COLOR;
	}

	/* name is required for theme manager */
	success = (success && xmlnode_get_attrib(root_node, "name") != NULL);

	/* the new theme */
	theme = g_object_new(PIDGIN_TYPE_BLIST_THEME,
			"type", "blist",
			"name", xmlnode_get_attrib(root_node, "name"),
			"author", xmlnode_get_attrib(root_node, "author"),
			"image", xmlnode_get_attrib(root_node, "image"),
			"directory", dir,
			"description", data,
			"background-color", &bgcolor,
			"layout", &layout,
			"expanded-color", &expanded_bgcolor,
			"expanded-text", &expanded,
			"collapsed-color", &collapsed_bgcolor,
			"collapsed-text", &collapsed,
			"contact-color", &contact_color,
			"contact", &contact,
			"online", &online,
			"away", &away,
			"offline", &offline,
			"idle", &idle,
			"message", &message,
			"message_nick_said", &message_nick_said,
			"status", &status, NULL);

	xmlnode_free(root_node);
	g_free(data);

	/* malformed xml file - also frees all partial data*/
	if (!success) {
		g_object_unref(theme);
		theme = NULL;
	}

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
