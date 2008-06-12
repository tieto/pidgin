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
/*****************************************************************************
 * Sound Theme Builder                                                      
 *****************************************************************************/

static PurpleSoundTheme *
purple_sound_loader_build(const gchar *dir)
{
	return NULL; /*TODO: unimplemented*/
}

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
purple_sound_theme_loader_class_init (PurpleThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_class = PURPLE_THEME_LOADER_CLASS(klass);
	
	loader_class->_purple_theme_loader_build = purple_sound_loader_build;
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


