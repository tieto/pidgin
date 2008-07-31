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
	const gchar *icon_theme = NULL, *temp;
	gboolean sucess = TRUE;
	GdkColor *bgcolor, *expanded_bgcolor, *collapsed_bgcolor, *contact_color;
	GdkColor color;
	FontColorPair *expanded, *collapsed, *contact, *online, *away, *offline, *idle, *message, *status;
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

	layout = g_new0(PidginBlistLayout, 1);

	contact_color = g_new0(GdkColor, 1);

	expanded = g_new0(FontColorPair, 1);
	collapsed = g_new0(FontColorPair, 1);
	contact = g_new0(FontColorPair, 1);
	online = g_new0(FontColorPair, 1);
	away = g_new0(FontColorPair, 1);
	offline = g_new0(FontColorPair, 1);
	idle = g_new0(FontColorPair, 1);
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
		if ((temp = xmlnode_get_attrib(sub_node, "color")) != NULL && gdk_color_parse(temp, bgcolor))
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

		if ((temp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(temp, &color))
			expanded->color = g_strdup(temp);
		else expanded->color = g_strdup(DEFAULT_TEXT_COLOR);
	

		if ((temp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(temp, expanded_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), expanded_bgcolor, FALSE, TRUE);
		else {
			g_free(expanded_bgcolor);
			expanded_bgcolor = NULL;
		}
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "collapsed")) != NULL)) {

		collapsed->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));

		if((temp = xmlnode_get_attrib(sub_sub_node, "text_color")) != NULL && gdk_color_parse(temp, &color))
			collapsed->color = g_strdup(temp);
		else collapsed->color = g_strdup(DEFAULT_TEXT_COLOR);

		if ((temp = xmlnode_get_attrib(sub_sub_node, "background")) != NULL && gdk_color_parse(temp, collapsed_bgcolor))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), collapsed_bgcolor, FALSE, TRUE);
		else {
			g_free(collapsed_bgcolor);
			collapsed_bgcolor = NULL;
		}
	}

	/* <buddys> */
	if ((sucess = sucess && (sub_node = xmlnode_get_child(root_node, "buddys")) != NULL &&
		     (sub_sub_node = xmlnode_get_child(sub_node, "placement")) != NULL)) { 

		layout->status_icon = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) : 0;
		layout->text = (temp = xmlnode_get_attrib(sub_sub_node, "name")) != NULL ? atoi(temp) : 1;
		layout->emblem = (temp = xmlnode_get_attrib(sub_sub_node, "emblem")) != NULL ? atoi(temp) : 2;
		layout->protocol_icon = (temp = xmlnode_get_attrib(sub_sub_node, "protocol_icon")) != NULL ? atoi(temp) : 3;
		layout->buddy_icon = (temp = xmlnode_get_attrib(sub_sub_node, "buddy_icon")) != NULL ? atoi(temp) : 4;		
		layout->show_status = (temp = xmlnode_get_attrib(sub_sub_node, "status_icon")) != NULL ? atoi(temp) != 0 : 1;
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "background")) != NULL)) {
		if(gdk_color_parse(xmlnode_get_attrib(sub_sub_node, "color"), contact_color))
			gdk_colormap_alloc_color(gdk_colormap_get_system(), contact_color, FALSE, TRUE);
		else {
			g_free(contact_color);
			contact_color = NULL;
		}
	}
	
	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "contact_text")) != NULL)) {
		online->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			contact->color = g_strdup(temp);
		else contact->color = g_strdup(DEFAULT_TEXT_COLOR);
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "online_text")) != NULL)) {
		online->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			online->color = g_strdup(temp);
		else online->color = g_strdup(DEFAULT_TEXT_COLOR);
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "away_text")) != NULL)) {
		away->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			away->color = g_strdup(temp);
		else away->color = g_strdup(DEFAULT_TEXT_COLOR);
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "offline_text")) != NULL)) {
		offline->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			online->color = g_strdup(temp);
		else online->color = g_strdup(DEFAULT_TEXT_COLOR);
	}

	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "idle_text")) != NULL)) {
		idle->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			idle->color = g_strdup(temp);
		else online->color = g_strdup(DEFAULT_TEXT_COLOR);
	}
	
	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "message_text")) != NULL)) {
		message->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			message->color = g_strdup(temp);
		else message->color = g_strdup(DEFAULT_TEXT_COLOR);
	}
	
	if ((sucess = sucess && sub_node != NULL && (sub_sub_node = xmlnode_get_child(sub_node, "status_text")) != NULL)) {
		status->font = g_strdup(xmlnode_get_attrib(sub_sub_node, "font"));
		if(gdk_color_parse(temp = xmlnode_get_attrib(sub_sub_node, "color"), &color))
			status->color = g_strdup(temp);
		else status->color = g_strdup(DEFAULT_TEXT_COLOR);
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
			    "layout", layout,
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


