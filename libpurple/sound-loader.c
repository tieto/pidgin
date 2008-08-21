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
#include "sound-theme.h"
#include "util.h"
#include "xmlnode.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
/*****************************************************************************
 * Sound Theme Builder                                                      
 *****************************************************************************/

static PurpleTheme *
purple_sound_loader_build(const gchar *dir)
{
	xmlnode *root_node, *sub_node;
	gchar *filename, *filename_full, *data;
	GDir *gdir;
	PurpleSoundTheme *theme = NULL;

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

	if (xmlnode_get_attrib(root_node, "name") != NULL) {
		theme = g_object_new(PURPLE_TYPE_SOUND_THEME,
				    "type", "sound",
				    "name", xmlnode_get_attrib(root_node, "name"),
				    "author", xmlnode_get_attrib(root_node, "author"),
				    "image", xmlnode_get_attrib(root_node, "image"),
				    "directory", dir,
				    "description", data, NULL);
	
		xmlnode_free(sub_node);

		while ((sub_node = xmlnode_get_child(root_node, "event")) != NULL){
			purple_sound_theme_set_file(theme,
						    xmlnode_get_attrib(sub_node, "name"),
						    xmlnode_get_attrib(sub_node, "file"));
			xmlnode_free(sub_node);
		}
	}

	xmlnode_free(root_node);	
	g_dir_close(gdir);
	g_free(filename_full);
	g_free(data);
	return PURPLE_THEME(theme);
}

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
purple_sound_theme_loader_class_init (PurpleSoundThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = purple_sound_loader_build;
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
    type = g_type_register_static (PURPLE_TYPE_THEME_LOADER,
                                   "PurpleSoundThemeLoader",
                                   &info, 0);
  }
  return type;
}


