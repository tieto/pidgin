/*
 * Sound Themes for LibPurple
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

#include "sound-theme.h"

#define PURPLE_SOUND_THEME_GET_PRIVATE(Gobject) \
	((PurpleSoundThemePrivate *) ((PURPLE_SOUND_THEME(Gobject))->priv))


/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	/* used to store filenames of diffrent sounds */
	GHashTable *sound_files;
} PurpleSoundThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Enums
 *****************************************************************************/

/******************************************************************************
 * GObject Stuff                                                              
 *****************************************************************************/

static void
purple_sound_theme_init(GTypeInstance *instance,
			gpointer klass)
{
	PurpleSoundThemePrivate *priv;
	GObject *obj = (GObject *)instance;

	priv = PURPLE_SOUND_THEME_GET_PRIVATE(obj);

	priv->sound_files = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   g_free,
						   g_free);
}

static void 
purple_sound_theme_finalize (GObject *obj)
{
	PurpleSoundThemePrivate *priv;

	priv = PURPLE_SOUND_THEME_GET_PRIVATE(obj);
	
	g_hash_table_destroy(priv->sound_files);

	parent_class->finalize (obj);
}

static void
purple_sound_theme_class_init (PurpleSoundThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

        obj_class->finalize = purple_sound_theme_finalize;
}

GType 
purple_sound_theme_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleSoundThemeClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)purple_sound_theme_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleSoundTheme),
      0,      /* n_preallocs */
      purple_sound_theme_init,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleSoundThemeType",
                                   &info, 0);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      
 *****************************************************************************/

gchar *
purple_sound_theme_get_file(PurpleSoundTheme *theme,
			    const gchar *event)
{
	PurpleSoundThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_SOUND_THEME(theme), NULL);

	priv = PURPLE_SOUND_THEME_GET_PRIVATE(theme);
	
	return g_hash_table_lookup(priv->sound_files, event);
}

gchar *
purple_sound_theme_get_file_full(PurpleSoundTheme *theme,
				 const gchar *event)
{
	gchar *dir, *fname, *full;

	g_return_val_if_fail(PURPLE_IS_SOUND_THEME(theme), NULL);

	dir = purple_theme_get_dir(theme->parent);
	fname = purple_sound_theme_get_file(theme, event);
	full = g_strconcat (dir, '/',fname, NULL);

	g_free(dir);
	g_free(fname);

	return full;
}

void 
purple_sound_theme_set_file(PurpleSoundTheme *theme,
			    const gchar *event, 
			    const gchar *fname)
{
	PurpleSoundThemePrivate *priv;
	g_return_if_fail(PURPLE_IS_SOUND_THEME(theme));

	priv = PURPLE_SOUND_THEME_GET_PRIVATE(theme);
	
	if (fname)g_hash_table_replace(priv->sound_files,
                 	             g_strdup(event),
                        	     g_strdup(fname));
	else g_hash_table_remove(priv->sound_files, event);
}


