/*
 * Themes for LibPurple
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

#include "theme.h"

#define PURPLE_THEME_GET_PRIVATE(PurpleTheme) \
	((PurpleThemePrivate *) ((PurpleTheme)->priv))


/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *name;
	gchar *author;
	gchar *type;
	gchar *dir;
	PurpleStoredImage *img;
} PurpleThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

/******************************************************************************
 * Enums
 *****************************************************************************/
#define PROP_NAME_S "name"
#define PROP_AUTHOR_S "author"
#define PROP_TYPE_S "type"
#define PROP_DIR_S "dir"
#define PROP_IMAGE_S "image"

enum {
	PROP_ZERO = 0,
	PROP_NAME,
	PROP_AUTHOR,
	PROP_TYPE,
	PROP_DIR,
	PROP_IMAGE
};


/******************************************************************************
 * GObject Stuff                                                              *
 *****************************************************************************/

static void
purple_theme_get_property(GObject *obj, guint param_id, GValue *value,
						 GParamSpec *psec)
{
	PurpleTheme *theme = PURPLE_THEME(obj);

	switch(param_id) {
		case PROP_NAME:
			g_value_set_string(value, purple_theme_get_name(theme));
			break;
		case PROP_AUTHOR:
			g_value_set_string(value, purple_theme_get_author(theme));
			break;
		case PROP_TYPE:
			g_value_set_string(value, purple_theme_get_type_string(theme));
			break;
		case PROP_DIR:
			g_value_set_string(value, purple_theme_get_dir(theme));
			break;
		case PROP_IMAGE:
			g_value_set_pointer(value, purple_theme_get_image(theme));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_set_property(GObject *obj, guint param_id, const GValue *value,
						 GParamSpec *psec)
{
	PurpleTheme *theme = PURPLE_THEME(obj);

	switch(param_id) {
		case PROP_NAME:
			purple_theme_set_name(theme, g_value_get_string(value));
			break;
		case PROP_AUTHOR:
			purple_theme_set_author(theme, g_value_get_string(value));
			break;
		case PROP_DIR:
			purple_theme_set_dir(theme, g_value_get_string(value));
			break;
		case PROP_IMAGE:
			purple_theme_set_image(theme, g_value_get_pointer(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_class_init (PurpleThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GParamSpec *pspec;

	/* 2.4
	 * g_type_class_add_private(klass, sizeof(PurpleThemePrivate)); */

	obj_class->get_property = purple_theme_get_property;
	obj_class->set_property = purple_theme_set_property;
	
	/* NAME */
	pspec = g_param_spec_string(PROP_NAME_S, "Name",
				    "The name of the theme",
				    NULL,
				    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property(obj_class, PROP_NAME, pspec);
	/* AUTHOR */
	pspec = g_param_spec_string(PROP_AUTHOR_S, "Author",
				    "The author of the theme",
				    NULL,
				    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property(obj_class, PROP_AUTHOR, pspec);
	/* TYPE STRING (read only) */
	pspec = g_param_spec_string(PROP_TYPE_S, "Type",
				    "The string represtenting the type of the theme",
				    NULL,
				    G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(obj_class, PROP_TYPE, pspec);
	/* DIRECTORY */
	pspec = g_param_spec_string(PROP_DIR_S, "Directory",
				    "The directory that contains the theme and all its files",
				    NULL,
				    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property(obj_class, PROP_DIR, pspec);
	/* PREVIEW IMAGE */
	pspec = g_param_spec_pointer(PROP_IMAGE_S, "Image",
				    "A preview image of the theme",
				    G_PARAM_READABLE);
	g_object_class_install_property(obj_class, PROP_IMAGE, pspec);
}


GType 
purple_theme_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleThemeClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)purple_theme_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleTheme),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleThemeType",
                                   &info, G_TYPE_FLAG_ABSTRACT);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

gchar *
purple_theme_get_name(PurpleTheme *theme)
{
	PurpleThemePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = PURPLE_THEME_GET_PRIVATE(theme);
	return priv->name;
}

void
purple_theme_set_name(PurpleTheme *theme, const gchar *name)
{
	PurpleThemePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = PURPLE_THEME_GET_PRIVATE(theme);

	g_free(priv->name);
	priv->name = g_strdup (name);
}

gchar *
purple_theme_get_author(PurpleTheme *theme)
{
	PurpleThemePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = PURPLE_THEME_GET_PRIVATE(theme);
	return priv->author;
}

void
purple_theme_set_author(PurpleTheme *theme, const gchar *author)
{
	PurpleThemePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = PURPLE_THEME_GET_PRIVATE(theme);

	g_free(priv->author);
	priv->author = g_strdup (author);
}

gchar *
purple_theme_get_type_string(PurpleTheme *theme)
{
	PurpleThemePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = PURPLE_THEME_GET_PRIVATE(theme);
	return priv->type;
}

gchar *
purple_theme_get_dir(PurpleTheme *theme) 
{
	PurpleThemePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = PURPLE_THEME_GET_PRIVATE(theme);
	return priv->dir;
}

void
purple_theme_set_dir(PurpleTheme *theme, const gchar *dir)
{
	PurpleThemePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = PURPLE_THEME_GET_PRIVATE(theme);

	g_free(priv->dir);
	priv->dir = g_strdup (dir);
}

PurpleStoredImage *
purple_theme_get_image(PurpleTheme *theme)
{
	PurpleThemePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = PURPLE_THEME_GET_PRIVATE(theme);

	return purple_imgstore_ref(priv->img);
}

void 
purple_theme_set_image(PurpleTheme *theme, PurpleStoredImage *img)
{	
	PurpleThemePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = PURPLE_THEME_GET_PRIVATE(theme);

	purple_imgstore_unref(priv->img);
	priv->img = img;
}
