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
/*****************************************************************************
 * Sound Theme Builder                                                      
 *****************************************************************************/

static gpointer
pidgin_blist_loader_build(const gchar *dir)
{
	xmlnode *root_node, *sub_node, *sub_sub_node;
	gchar *filename, *filename_full, *data;
	const gchar *icon_theme;
	GdkColor *bgcolor = NULL, 
		 *expanded_bgcolor = NULL,
		 *minimized_bgcolor = NULL, 
		 *buddy_bgcolor1 = NULL,
		 *buddy_bgcolor2 = NULL;
	font_color_pair *expanded = g_new0(font_color_pair, 1), 
			*minimized = g_new0(font_color_pair, 1), 
			*online = g_new0(font_color_pair, 1),
			*away = g_new0(font_color_pair, 1),
			*offline = g_new0(font_color_pair, 1), 
			*message = g_new0(font_color_pair, 1), 
			*status = g_new0(font_color_pair, 1);
	gdouble transparency;
	blist_layout *layout = g_new0(blist_layout, 1);
	GDir *gdir;
	PidginBlistTheme *theme;

	/* Find the theme file */
	gdir = g_dir_open(dir, 0, NULL);
	g_return_val_if_fail(gdir != NULL, NULL);

	while ((filename = g_strdup(g_dir_read_name(gdir))) != NULL && ! g_str_has_suffix(filename, ".xml"))
		g_free(filename);
	
	g_return_val_if_fail(filename != NULL, NULL);
	
	/* Build the xml tree */
	filename_full = g_build_filename(dir, filename, NULL);

	root_node = xmlnode_from_file(dir, filename, "sound themes", "sound-loader");
	g_return_val_if_fail(root_node != NULL, NULL);

	/* Parse the tree */	
	sub_node = xmlnode_get_child(root_node, "description");
	data = xmlnode_get_data(sub_node);

	/* <icon_theme> */
	sub_node = xmlnode_get_child(root_node, "icon_theme");
	icon_theme = xmlnode_get_attrib(sub_node, "name");

	/* <blist> */
	sub_node = xmlnode_get_child(root_node, "blist");
	transparency = atof(xmlnode_get_attrib(sub_node, "transparency"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_node, "color"), bgcolor))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), bgcolor, FALSE, TRUE);
	

	/* <groups> */
	sub_node = xmlnode_get_child(root_node, "groups");
	sub_sub_node = xmlnode_get_child(root_node, "expanded");
	expanded->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), expanded->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), expanded->color, FALSE, TRUE);
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "background"), expanded_bgcolor))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), expanded_bgcolor, FALSE, TRUE);

	sub_sub_node = xmlnode_get_child(root_node, "minimized");
	minimized->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), minimized->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), minimized->color, FALSE, TRUE);
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "background"), minimized_bgcolor))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), minimized_bgcolor, FALSE, TRUE);

	/* <buddys> */
	sub_node = xmlnode_get_child(root_node, "buddys");
	sub_sub_node = xmlnode_get_child(root_node, "placement");
	layout->buddy_icon = atoi(xmlnode_get_attrib(sub_sub_node, "status_icon"));
	layout->text = atoi(xmlnode_get_attrib(sub_sub_node, "name"));
	layout->buddy_icon = atoi(xmlnode_get_attrib(sub_sub_node, "buddy_icon"));
	layout->protocol_icon = atoi(xmlnode_get_attrib(sub_sub_node, "protocol_icon"));
	layout->emblem = atoi(xmlnode_get_attrib(sub_sub_node, "emblem"));
	layout->show_status = (gboolean) atoi(xmlnode_get_attrib(sub_sub_node, "status_icon"));

	sub_sub_node = xmlnode_get_child(root_node, "background");
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color1"), buddy_bgcolor1))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), buddy_bgcolor1, FALSE, TRUE);
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color2"), buddy_bgcolor2))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), buddy_bgcolor2, FALSE, TRUE);

	sub_sub_node = xmlnode_get_child(root_node, "online_text");
	online->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), (online->color)))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), (online->color), FALSE, TRUE);

	sub_sub_node = xmlnode_get_child(root_node, "away_text");
	away->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), away->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), away->color, FALSE, TRUE);
	
	sub_sub_node = xmlnode_get_child(root_node, "offline_text");
	offline->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), offline->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), offline->color, FALSE, TRUE);
	
	sub_sub_node = xmlnode_get_child(root_node, "message_text");
	message->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), message->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), message->color, FALSE, TRUE);
	
	sub_sub_node = xmlnode_get_child(root_node, "status_text");
	status->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), status->color))
		gdk_colormap_alloc_color(gdk_colormap_get_system(), status->color, FALSE, TRUE);

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
			    "minimized-color", minimized_bgcolor,
			    "minimized-text", minimized,
			    "buddy-bgcolor1", buddy_bgcolor1,
			    "buddy-bgcolor2", buddy_bgcolor2,
			    "online", online,
			    "away", away,
			    "offline", offline,
			    "message", message,
			    "status", status, NULL);
	
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


