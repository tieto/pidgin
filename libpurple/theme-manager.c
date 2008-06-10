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

#include <stdarg.h>
#include "theme-manager.h"

#define PURPLE_THEME_MANAGER_GET_PRIVATE(Gobject) \
	((PurpleThemeManagerPrivate *) ((PURPLE_THEME_MANAGER(Gobject))->priv))

/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	GHashTable *theme_table;
} PurpleThemeManagerPrivate;

/*****************************************************************************
 * GObject Stuff                                                     
 ****************************************************************************/

static void 
purple_theme_manager_finalize (GObject *obj)
{
	PurpleThemeManagerPrivate *priv;
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(obj);	

	g_hash_table_destroy(priv->theme_table);
}

static void
purple_theme_manager_class_init (PurpleThemeManagerClass *klass)
{
	/*PurpleThemeManagerClass *theme_manager_class = PURPLE_THEME_MANAGER_CLASS(klass);*/
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	/* 2.4 g_type_class_add_private(klass, sizeof(PurpleThemeManagerPrivate));*/

        obj_class->finalize = purple_theme_manager_finalize;
	
}

GType 
purple_theme_manager_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleThemeManagerClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)purple_theme_manager_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleThemeManager),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* Value Table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleThemeManagerType",
                                   &info, 0);
  }
  return type;
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
/* makes a key of <type> + '/' + <name> */
static gchar *
purple_theme_manager_make_key(const gchar *name, const gchar *type)
{
	g_return_val_if_fail(name, NULL);	
	g_return_val_if_fail(type, NULL);		
	return g_strconcat(type, '/', name, NULL);
}

/* returns TRUE if theme is of type "user_data" */ 
static gboolean
purple_theme_manager_is_theme_type(gchar *key,
                  gpointer value,
                  gchar *user_data)
{
	return g_str_has_prefix (key, g_strconcat(user_data, '/', NULL));
}

static gboolean
purple_theme_manager_is_theme(gchar *key,
                  gpointer value,
                  gchar *user_data)
{
	return PURPLE_IS_THEME(value);
}

static void
purple_theme_manager_function_wrapper(gchar *key,
                  		      gpointer value,
                		      PTFunc user_data)
{
	if(PURPLE_IS_THEME(value))
		(* user_data) (value);
}

static void
purple_theme_manager_build(PurpleThemeManager *self, const gchar *root)
{
	PurpleThemeManagerPrivate *priv;
	GDir *rdir;
	gchar *name, *type;
	GDir *dir;
	PurpleThemeLoader *loader;
	
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	rdir =  g_dir_open(root, 0, NULL);/*TODO: should have debug error?*/

	g_return_if_fail(rdir);

	/*TODO: This looks messy*/
	/* Parses directory by root/name/purple/type */
	while((name = g_strdup(g_dir_read_name (rdir)))){

		dir =  g_dir_open(g_strconcat(root, '/', name,
				  '/', "purple", NULL), 0, NULL);	
	
		if(dir) {
			while((type = g_strdup(g_dir_read_name (dir)))) {
				if((loader = g_hash_table_lookup (priv->theme_table, type)))
					purple_theme_manager_add_theme(self,
								       purple_theme_loader_build(loader,  g_strconcat(root, '/', name, '/',
												              "purple", '/', type, NULL)));

				g_free(type);
			}

			g_dir_close(dir);
		}

		g_free(name);		
	}

	g_dir_close(rdir);
	
}

/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

PurpleThemeManager * 
purple_theme_manager_new (PurpleThemeLoader *loader1, ...)
{
	PurpleThemeManager *self;
	PurpleThemeManagerPrivate *priv;
	va_list args;
	PurpleThemeLoader *loader;

	self = g_object_new(PURPLE_TYPE_THEME_MANAGER, NULL, NULL, NULL);
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	priv->theme_table = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   g_object_unref); /* purple_theme_finalize() ?*/

	va_start(args, loader1);
	for (loader = loader1; loader != NULL; loader = va_arg(args, PurpleThemeLoader *))
		purple_theme_manager_register_type(self, loader);
	va_end(args);

	/* TODO: add themes properly */
	purple_theme_manager_build(self, NULL);
	return self;
}


void
purple_theme_manager_register_type(PurpleThemeManager *self,
				   PurpleThemeLoader *loader)
{
	PurpleThemeManagerPrivate *priv = NULL;
	gchar *type;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	g_return_if_fail(PURPLE_IS_THEME_LOADER(loader));
		
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	type = purple_theme_loader_get_type_string(loader);
	g_return_if_fail(type);

	/* if something is already there do nothing */
	if(! g_hash_table_lookup (priv->theme_table, type)) 
		g_hash_table_insert(priv->theme_table, type, loader);
	
	g_free(type);
}

void
purple_theme_manager_unregister_type(PurpleThemeManager *self,
				   PurpleThemeLoader *loader)
{
	PurpleThemeManagerPrivate *priv = NULL;
	gchar *type;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	g_return_if_fail(PURPLE_IS_THEME_LOADER(loader));
	
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	type = purple_theme_loader_get_type_string(loader);
	g_return_if_fail(type);

	if(g_hash_table_lookup (priv->theme_table, type) == loader){

		g_hash_table_remove (priv->theme_table, type);

		g_hash_table_foreach_remove (priv->theme_table,
                	                     (GHRFunc) purple_theme_manager_is_theme_type,
                	                     type);		
	}/* only free if given registered loader */

	g_free(type);
}

PurpleTheme *
purple_theme_manager_find_theme(PurpleThemeManager *self,
				const gchar *name,
				const gchar *type)
{
	PurpleThemeManagerPrivate *priv = NULL;
	g_return_val_if_fail(PURPLE_IS_THEME_MANAGER(self), NULL);
	g_return_val_if_fail(name, NULL);
	g_return_val_if_fail(type, NULL);

	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);
	return g_hash_table_lookup (priv->theme_table,
				    purple_theme_manager_make_key(name, type));
}


void 
purple_theme_manager_add_theme(PurpleThemeManager *self,
			       PurpleTheme *theme)
{
	PurpleThemeManagerPrivate *priv;
	gchar *key;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	g_return_if_fail(PURPLE_IS_THEME(theme));
	
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	key = purple_theme_manager_make_key(purple_theme_get_name(theme),
			   purple_theme_get_type_string(theme));

	g_return_if_fail(key);
	
	/* if something is already there do nothing */
	if(! g_hash_table_lookup (priv->theme_table, key)) 
		g_hash_table_insert(priv->theme_table, key, theme);
	
	g_free(key);	
}

void
purple_theme_manager_remove_theme(PurpleThemeManager *self,
				  PurpleTheme *theme)
{
	PurpleThemeManagerPrivate *priv = NULL;
	gchar *key;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	g_return_if_fail(PURPLE_IS_THEME(theme));
	
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);

	key = purple_theme_manager_make_key(purple_theme_get_name(theme),
				  purple_theme_get_type_string(theme));

	g_return_if_fail(key);

	g_hash_table_remove(priv->theme_table, key);	

	g_free(key);	
}

void 
purple_theme_manager_rebuild(PurpleThemeManager *self)
{
	PurpleThemeManagerPrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);
	
	g_hash_table_foreach_remove (priv->theme_table,
                	             (GHRFunc) purple_theme_manager_is_theme,
                	             NULL);	
	
	/* TODO: this also needs to be fixed the same as new */
	purple_theme_manager_build(self, NULL);

}

void 
purple_theme_manager_for_each_theme(PurpleThemeManager *self, PTFunc func)
{
	PurpleThemeManagerPrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME_MANAGER(self));
	g_return_if_fail(func);

	priv = PURPLE_THEME_MANAGER_GET_PRIVATE(self);	

	g_hash_table_foreach(priv->theme_table,
			     (GHFunc) purple_theme_manager_function_wrapper,
			     func);
}

