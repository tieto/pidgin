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

#include "gtkblist-loader.h"
#include "gtkblist-theme.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
/*****************************************************************************
 * Sound Theme Builder                                                      
 *****************************************************************************/

static gpointer
pidgin_buddy_list_loader_build(const gchar *dir)
{
	xmlnode *root_node, *sub_node, *sub_sub_node;
	gchar *filename, *filename_full, *data;
	const gchar *icon_theme, *bgcolor, *expanded_bgcolor, *minimized_bgcolor, *buddy_bgcolor1, *buddy_bgcolor2;
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
	PidginBuddyListTheme *theme;

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

	/* <inon_theme> */
	sub_node = xmlnode_get_child(root_node, "icon_theme");
	icon_theme = xmlnode_get_attrib(sub_node, "name");

	/* <buddy_list> */
	sub_node = xmlnode_get_child(root_node, "buddy_list");
	bgcolor = xmlnode_get_attrib(sub_node, "color");
	transparency = atof(xmlnode_get_attrib(sub_node, "transparency"));

	/* <groups> */
	sub_node = xmlnode_get_child(root_node, "groups");
	sub_sub_node = xmlnode_get_child(root_node, "expanded");
	expanded->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	expanded->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));
	expanded_bgcolor = g_strdup(xmlnode_get_attrib(sub_sub_node, "background"));

	sub_sub_node = xmlnode_get_child(root_node, "minimized");
	minimized->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	minimized->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));
	minimized_bgcolor = xmlnode_get_attrib(sub_sub_node, "background");

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
	buddy_bgcolor1 = xmlnode_get_attrib(sub_sub_node, "color1");
	buddy_bgcolor2 = xmlnode_get_attrib(sub_sub_node, "color2");

	sub_sub_node = xmlnode_get_child(root_node, "online_text");
	online->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	online->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));

	sub_sub_node = xmlnode_get_child(root_node, "away_text");
	away->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	away->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));
	
	sub_sub_node = xmlnode_get_child(root_node, "offline_text");
	offline->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	offline->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));
	
	sub_sub_node = xmlnode_get_child(root_node, "message_text");
	message->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	message->color =	g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));
	
	sub_sub_node = xmlnode_get_child(root_node, "status_text");
	status->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
	status->color = g_strdup(xmlnode_get_attrib(sub_sub_node, "color"));

	/* the new theme */
	theme = g_object_new(PIDGIN_TYPE_BUDDY_LIST_THEME,
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
pidgin_buddy_list_theme_loader_class_init (PidginBuddyListThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = pidgin_buddy_list_loader_build;
}


GType 
pidgin_buddy_list_theme_loader_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PidginBuddyListThemeLoaderClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)pidgin_buddy_list_theme_loader_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PidginBuddyListThemeLoader),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (PURPLE_TYPE_THEME_LOADER,
                                   "PidginBuddyListThemeLoader",
                                   &info, 0);
  }
  return type;
}


