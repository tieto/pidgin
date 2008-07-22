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
 *
 */

#include <stdlib.h>

#include "xmlnode.h"

#include "gtkblist-loader.h"
#include "gtkblist-theme.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
#define DEFAULT_TEXT_COLOR "black"
/*****************************************************************************
 * Buddy List Theme Builder                                                      
 *****************************************************************************/

static gpointer
pidgin_blist_loader_build(const gchar *dir)
{
	xmlnode *root_node, *sub_node, *sub_sub_node;
	gchar *filename, *filename_full, *data;
	const gchar *icon_theme = NULL, *tmp;
	gboolean sucess = TRUE;
	GdkColor *bgcolor, *expanded_bgcolor, *collapsed_bgcolor, *buddy_bgcolor1, *buddy_bgcolor2;
	GdkColor color;
	FontColorPair *expanded, *collapsed, *online, *away, *offline, *message, *status;
	gdouble transparency;
	PidginBlistLayout *layout;
	GDir *gdir;
	PidginBlistTheme *theme;

	/* Find the theme file */
	gdir = g_dir_open(dir, 0, NULL);
	g_return_val_if_fail(gdir != NULL, NULL);

	while ((filename = g_strdup(g_dir_read_name(gdir))) != NULL && ! g_str_has_suffix(filename, ".xml"))
		g_free(filename);
	
	if (filename == NULL){
		g_dir_close(gdir);
		return NULL;
	}
	
	/* Build the xml tree */
	filename_full = g_build_filename(dir, filename, NULL);

	root_node = xmlnode_from_file(dir, filename, "blist themes", "blist-loader");
	g_return_val_if_fail(root_node != NULL, NULL);

	/* init all structs and colors */
	bgcolor = g_new0(GdkColor, 1);
	expanded_bgcolor = g_new0(GdkColor, 1);
	collapsed_bgcolor = g_new0(GdkColor, 1); 
	buddy_bgcolor1 = g_new0(GdkColor, 1);
	buddy_bgcolor2 = g_new0(GdkColor, 1);

	layout = g_new0(PidginBlistLayout, 1);

	expanded = g_new0(FontColorPair, 1);
	collapsed = g_new0(FontColorPair, 1);
	online = g_new0(FontColorPair, 1);
	away = g_new0(FontColorPair, 1);
	offline = g_new0(FontColorPair, 1);
	message = g_new0(FontColorPair, 1); 
	status = g_new0(FontColorPair, 1);

	/* Parse the tree */	
	sub_node = xmlnode_get_child(root_node, "description");
	data = xmlnode_get_data(sub_node);

	/* <icon_theme> */
	if ((sucess = (sub_node = xmlnode_get_child(root_node, "icon_theme")) != NULL))
		icon_theme = xmlnode_get_attrib(sub_node, "name");

	/* <blist> */
	if ((sucess = sucess && (sub_node = xmlnode_get_child(root_node, "blist")) != NULL)) {
		transparency = atof(xmlnode_get_attrib(sub_node, "transparency"));

		if ((tmp = xmlnode_get_attrib(sub_node, "color")) != NULL && gdk_color_parse(tmp, bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), bgcolor, FALSE, TRUE);
		else {
			g_free(bgcolor);
			bgcolor = NULL;
		}
	}

	/* <groups> */
	if ((sucess = sucess && (sub_node = xmlnode_get_child(root_node, "groups")) != NULL
		     && (sub_sub_node = xmlnode_get_child(sub_node, "expanded")) != NULL)) {

		expanded->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));

		if ((tmp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(tmp, &color))
			expanded->color = g_strdup(tmp);
		else expanded->color = DEFAULT_TEXT_COLOR;
	

		if ((tmp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(tmp, expanded_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), expanded_bgcolor, FALSE, TRUE);
		else {
			g_free(expanded_bgcolor);
			expanded_bgcolor = NULL;
		}
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "collapsed")) != NULL)) {
		collapsed->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));

		if((tmp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(tmp, &color))
			collapsed->color = g_strdup(tmp);
		else collapsed->color = DEFAULT_TEXT_COLOR;

		if ((tmp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(tmp, collapsed_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), collapsed_bgcolor, FALSE, TRUE);
		else {
			g_free(collapsed_bgcolor);
			collapsed_bgcolor = NULL;
		}
	}

	/* <buddys> */
	if ((sucess = sucess && (sub_node = xmlnode_get_child(root_node, "buddys")) != NULL &&
		     (sub_sub_node = xmlnode_get_child(sub_node, "placement")) != NULL)) { 

		layout->status_icon = atoi(xmlnode_get_attrib(sub_sub_node, "status_icon"));
		layout->text = atoi(xmlnode_get_attrib(sub_sub_node, "name"));
		layout->emblem = atoi(xmlnode_get_attrib(sub_sub_node, "emblem"));
		layout->protocol_icon = atoi(xmlnode_get_attrib(sub_sub_node, "protocol_icon"));
		layout->buddy_icon = atoi(xmlnode_get_attrib(sub_sub_node, "buddy_icon"));		
		layout->show_status = atoi(xmlnode_get_attrib(sub_sub_node, "status_icon")) != 0;
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "background")) != NULL)) {

		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color1"), buddy_bgcolor1))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), buddy_bgcolor1, FALSE, TRUE);
		else {
			g_free(buddy_bgcolor1);
			buddy_bgcolor1 = NULL;
		}

		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color2"), buddy_bgcolor2))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), buddy_bgcolor2, FALSE, TRUE);
		else {
			g_free(buddy_bgcolor2);
			buddy_bgcolor2 = NULL;
		}
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "online_text")) != NULL)) {
		online->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &color))
			online->color = g_strdup(tmp);
		else online->color = DEFAULT_TEXT_COLOR;
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "away_text")) != NULL)) {
		away->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &color))
			away->color = g_strdup(tmp);
		else away->color = DEFAULT_TEXT_COLOR;
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "offline_text")) != NULL)) {
		offline->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &color))
			online->color = g_strdup(tmp);
		else online->color = DEFAULT_TEXT_COLOR;
	}
	
	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "message_text")) != NULL)) {
		message->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &color))
			message->color = g_strdup(tmp);
		else message->color = DEFAULT_TEXT_COLOR;
	}
	
	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "status_text")) != NULL)) {
		status->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), &color))
			status->color = g_strdup(tmp);
		else status->color = DEFAULT_TEXT_COLOR;
	}

	/* name is required for theme manager */
	sucess = sucess && xmlnode_get_attrib(root_node, "name") != NULL;

	/* the new theme */
	theme = g_object_new(PIDGIN_TYPE_BLIST_THEME,
			    "type", "blist",
			    "name", xmlnode_get_attrib(root_node, "name"),
			    "author", xmlnode_get_attrib(root_node, "author"),
			    "image", xmlnode_get_attrib(root_node, "image"),
			    "directory", dir,
			    "description", data,
			    "icon-theme", icon_theme,
			    "background-color", bgcolor,
			    "opacity", transparency,
			    "layout", layout,
			    "expanded-color", expanded_bgcolor,
			    "expanded-text", expanded,
			    "collapsed-color", collapsed_bgcolor,
			    "collapsed-text", collapsed,
			    "buddy-bgcolor1", buddy_bgcolor1,
			    "buddy-bgcolor2", buddy_bgcolor2,
			    "online", online,
			    "away", away,
			    "offline", offline,
			    "message", message,
			    "status", status, NULL);

	/* malformed xml file */
	if (!sucess) {
		g_object_unref(theme);
		theme = NULL;
	}
	
	xmlnode_free(sub_node);
	xmlnode_free(root_node);	
	g_dir_close(gdir);
	g_free(filename_full);
	g_free(data);
	return theme;
}

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
pidgin_blist_theme_loader_class_init (PidginBlistThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = pidgin_blist_loader_build;
}


GType 
pidgin_blist_theme_loader_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PidginBlistThemeLoaderClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)pidgin_blist_theme_loader_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PidginBlistThemeLoader),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (PURPLE_TYPE_THEME_LOADER,
                                   "PidginBlistThemeLoader",
                                   &info, 0);
  }
  return type;
}


