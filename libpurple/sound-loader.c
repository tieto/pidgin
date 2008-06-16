/*
 * SoundThemeLoader for LibPurple
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

#include "sound-loader.h"
#include "util.h"
#include "xmlnode.h"

/******************************************************************************
 * Globals
 *****************************************************************************/

static PurpleThemeLoaderClass *parent_class = NULL;

/*****************************************************************************
 * Sound Theme Builder                                                      
 *****************************************************************************/
#define THEME_SUFFIX		".xml"
#define THEME_NAME		"name"
#define THEME_AUTHOR		"author"
#define THEME_IMAGE		"image"
#define THEME_DESCRIPTION	"description"
#define THEME_SOUND_EVENT	"event"
#define THEME_EVENT_NAME	"name"
#define THEME_EVENT_FILE	"file"

static PurpleSoundTheme *
purple_sound_loader_build(const gchar *dir)
{
	xmlnode *root_node, *sub_node;
	gchar *filename, *filename_full, *image, *data;
	GDir *gdir;
	PurpleSoundTheme *theme;

	/* Find the theme file */
	gdir = g_dir_open(dir, 0, NULL);
	g_return_val_if_fail(gdir != NULL, NULL);

	while ((filename = g_strdup(g_dir_read_name(gdir))) != NULL && ! g_str_has_suffix(filename, THEME_SUFFIX))
		g_free(filename);
	
	g_return_val_if_fail(filename != NULL, NULL);
	
	/* Build the xml tree */
	filename_full = g_build_filename(dir, filename, NULL);
	
	root_node = xmlnode_from_file(dir, filename, "sound themes", "sound-loader");
	g_return_val_if_fail(root_node != NULL, NULL);

	/* Parse the tree */
	theme = g_object_new(PURPLE_TYPE_SOUND_THEME, "type", "sound", NULL);
		
	purple_theme_set_name(theme->parent, xmlnode_get_attrib(root_node, THEME_NAME));
	purple_theme_set_author(theme->parent, xmlnode_get_attrib(root_node, THEME_AUTHOR));

	image = g_build_filename(dir, xmlnode_get_attrib(root_node, THEME_IMAGE), NULL);
	
	sub_node = xmlnode_get_child(root_node, THEME_DESCRIPTION);
	data = xmlnode_get_data(sub_node);
	purple_theme_set_description(theme->parent, data);
	xmlnode_free(sub_node);

	while ((sub_node = xmlnode_get_child(root_node, THEME_SOUND_EVENT)) != NULL){
		purple_sound_theme_set_file(theme,
					    xmlnode_get_attrib(root_node, THEME_EVENT_NAME),
					    xmlnode_get_attrib(root_node, THEME_EVENT_FILE));

		xmlnode_free(sub_node);
	}

	xmlnode_free(root_node);	
	g_dir_close(gdir);
	g_free(filename_full);
	g_free(image);
	g_free(data);
	return theme;
}

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
purple_sound_theme_loader_class_init (PurpleSoundThemeLoaderClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);
	
	/* TODO: fix warning */
	parent_class->purple_theme_loader_build = purple_sound_loader_build;
}


GType 
purple_sound_theme_loader_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleSoundThemeLoaderClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)purple_sound_theme_loader_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleSoundThemeLoader),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleSoundThemeLoaderType",
                                   &info, 0);
  }
  return type;
}


