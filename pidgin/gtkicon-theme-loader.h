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
/**
 * SECTION:gtkicon-theme-loader
 * @section_id: pidgin-gtkicon-theme-loader
 * @short_description: <filename>gtkicon-theme-loader.h</filename>
 * @title: Pidgin Icon Theme Loader Class
 */

#ifndef PIDGIN_ICON_THEME_LOADER_H
#define PIDGIN_ICON_THEME_LOADER_H

#include <glib.h>
#include <glib-object.h>
#include "theme-loader.h"

typedef struct _PidginIconThemeLoader       PidginIconThemeLoader;
typedef struct _PidginIconThemeLoaderClass  PidginIconThemeLoaderClass;

#define PIDGIN_TYPE_ICON_THEME_LOADER            (pidgin_icon_theme_loader_get_type ())
#define PIDGIN_ICON_THEME_LOADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_ICON_THEME_LOADER, PidginIconThemeLoader))
#define PIDGIN_ICON_THEME_LOADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_ICON_THEME_LOADER, PidginIconThemeLoaderClass))
#define PIDGIN_IS_ICON_THEME_LOADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_ICON_THEME_LOADER))
#define PIDGIN_IS_ICON_THEME_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_ICON_THEME_LOADER))
#define PIDGIN_ICON_THEME_LOADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_ICON_THEME_LOADER, PidginIconThemeLoaderClass))

/**
 * PidginIconThemeLoader:
 *
 * A pidgin icon theme loader. Extends PurpleThemeLoader (theme-loader.h)
 * This is a class designed to build icon themes
 */
struct _PidginIconThemeLoader
{
	PurpleThemeLoader parent;
};

struct _PidginIconThemeLoaderClass
{
	PurpleThemeLoaderClass parent_class;
};

/**************************************************************************/
/* Pidgin Icon Theme-Loader API                                           */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * pidgin_icon_theme_loader_get_type:
 *
 * Returns: The #GType for an icon theme loader.
 */
GType pidgin_icon_theme_loader_get_type(void);

G_END_DECLS

#endif /* PIDGIN_ICON_THEME_LOADER_H */
