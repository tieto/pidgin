/**
 * @file thememanager.h  Theme Manager API
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 */

#ifndef __PURPLE_THEME_MANAGER_H__
#define __PURPLE_THEME_MANAGER_H__

#include <glib-object.h>
#include <glib.h>
#include "theme.h"
#include "theme-loader.h"

typedef void (*PTFunc) (PurpleTheme *theme);

typedef struct _PurpleThemeManager PurpleThemeManager;
typedef struct _PurpleThemeManagerClass PurpleThemeManagerClass;

#define PURPLE_TYPE_THEME_MANAGER            (purple_theme_manager_get_type ())
#define PURPLE_THEME_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManager))
#define PURPLE_THEME_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManagerClass))
#define PURPLE_IS_THEME_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PURPLE_TYPE_THEME_MANAGER))
#define PURPLE_IS_THEME_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PURPLE_TYPE_THEME_MANAGER))
#define PURPLE_GET_THEME_MANAGER_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManagerClass))

struct _PurpleThemeManager {
	GObject parent;
};

struct _PurpleThemeManagerClass {
	GObjectClass parent_class;
};

/**************************************************************************/
/** @name Purple Theme Manager API                                        */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType purple_theme_manager_get_type (void);

/**
 * Initalizes the manager then add the loaders to the theme manager 
 * and builds with the given loaders 
 * @param 	the loaders to build with 
 */
void purple_theme_manager_init (PurpleThemeLoader *loader1, ...);

/**
 * Rebuilds all the themes in the theme manager
 * (removes all current themes but keeps the added loaders)
 */
void purple_theme_manager_refresh(void);

/**
 * Uninitalizes the manager then frees all the themes an loaders it is responsible for 
 */
void purple_theme_manager_uninit (void);

/**
 * Finds the PurpleTheme object stored by the theme manager
 * 
 * @param name		the name of the PurpleTheme
 * @param type 		the type of the PurpleTheme
 *
 * @returns 	The PurpleTheme or NULL if it wasn't found
 */
PurpleTheme *purple_theme_manager_find_theme(const gchar *name, const gchar *type);

/**
 * Adds a PurpleTheme to the theme manager, if the theme already exits it does nothing
 *
 * @param theme 	the PurpleTheme to add to the manager
 */
void purple_theme_manager_add_theme(PurpleTheme *theme);

/**
 * Removes a PurpleTheme from the theme manager, and frees the theme
 * @param theme 	the PurpleTheme to remove from the manager
 */
void purple_theme_manager_remove_theme(PurpleTheme *theme);

/**
 * Addes a Loader to the theme manager so it knows how to build themes
 * @param loader 	the PurpleThemeLoader to add
 */
void purple_theme_manager_register_type(PurpleThemeLoader *loader);

/**
 * Removes the loader and all themes of the same type from the loader
 * @param loader 	the PurpleThemeLoader to be removed
 */
void purple_theme_manager_unregister_type(PurpleThemeLoader *loader);

/**
 * Calles the given function on each purple theme
 *
 * @param func 		the PTFunc to be applied to each theme
 */
void purple_theme_manager_for_each_theme(PTFunc func); 

G_END_DECLS
#endif /* __PURPLE_THEME_MANAGER_H__ */

