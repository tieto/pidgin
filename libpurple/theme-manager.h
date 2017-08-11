/*
 * purple
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

#ifndef PURPLE_THEME_MANAGER_H
#define PURPLE_THEME_MANAGER_H
/**
 * SECTION:theme-manager
 * @section_id: libpurple-theme-manager
 * @short_description: <filename>theme-manager.h</filename>
 * @title: Theme Manager API
 */

#include <glib-object.h>
#include <glib.h>
#include "theme.h"
#include "theme-loader.h"

typedef void (*PurpleThemeFunc) (PurpleTheme *);

typedef struct _PurpleThemeManager PurpleThemeManager;
typedef struct _PurpleThemeManagerClass PurpleThemeManagerClass;

#define PURPLE_TYPE_THEME_MANAGER            (purple_theme_manager_get_type())
#define PURPLE_THEME_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManager))
#define PURPLE_THEME_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManagerClass))
#define PURPLE_IS_THEME_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_THEME_MANAGER))
#define PURPLE_IS_THEME_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_THEME_MANAGER))
#define PURPLE_GET_THEME_MANAGER_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_THEME_MANAGER, PurpleThemeManagerClass))

struct _PurpleThemeManager {
	GObject parent;
};

struct _PurpleThemeManagerClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

/**************************************************************************/
/* Purple Theme Manager API                                               */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * purple_theme_manager_get_type:
 *
 * Returns: The #GType for theme manager.
 */
GType purple_theme_manager_get_type(void);

/**
 * purple_theme_manager_init:
 *
 * Initalizes the theme manager.
 */
void purple_theme_manager_init(void);

/**
 * purple_theme_manager_uninit:
 *
 * Uninitalizes the manager then frees all the themes and loaders it is
 * responsible for.
 */
void purple_theme_manager_uninit(void);

/**
 * purple_theme_manager_refresh:
 *
 * Rebuilds all the themes in the theme manager.
 * (Removes all current themes but keeps the added loaders.)
 */
void purple_theme_manager_refresh(void);

/**
 * purple_theme_manager_find_theme:
 * @name: The name of the PurpleTheme.
 * @type: The type of the PurpleTheme.
 *
 * Finds the PurpleTheme object stored by the theme manager.
 *
 * Returns: (transfer none): The PurpleTheme, or NULL if it wasn't found.
 */
PurpleTheme *purple_theme_manager_find_theme(const gchar *name, const gchar *type);

/**
 * purple_theme_manager_add_theme:
 * @theme: The PurpleTheme to add to the manager.
 *
 * Adds a PurpleTheme to the theme manager.  If the theme already exists
 * then this function does nothing.
 */
void purple_theme_manager_add_theme(PurpleTheme *theme);

/**
 * purple_theme_manager_remove_theme:
 * @theme: The PurpleTheme to remove from the manager.
 *
 * Removes a PurpleTheme from the theme manager and frees the theme.
 */
void purple_theme_manager_remove_theme(PurpleTheme *theme);

/**
 * purple_theme_manager_register_type:
 * @loader: The PurpleThemeLoader to add.
 *
 * Adds a loader to the theme manager so it knows how to build themes.
 */
void purple_theme_manager_register_type(PurpleThemeLoader *loader);

/**
 * purple_theme_manager_unregister_type:
 * @loader: The PurpleThemeLoader to be removed.
 *
 * Removes the loader and all themes of the same type from the loader.
 */
void purple_theme_manager_unregister_type(PurpleThemeLoader *loader);

/**
 * purple_theme_manager_for_each_theme:
 * @func: (scope call): The PurpleThemeFunc to be applied to each theme.
 *
 * Calls the given function on each purple theme.
 */
void purple_theme_manager_for_each_theme(PurpleThemeFunc func);

/**
 * purple_theme_manager_load_theme:
 * @theme_dir:	the directory of the theme to load
 * @type:		the type of theme to load
 *
 * Loads a theme of the given type without adding it to the manager
 *
 * Returns: (transfer full): The newly loaded theme.
 */
PurpleTheme *purple_theme_manager_load_theme(const gchar *theme_dir, const gchar *type);

G_END_DECLS

#endif /* PURPLE_THEME_MANAGER_H */
