/**
 * @file gtkicon-theme.h  Pidgin Icon Theme  Class API
 */

/* pidgin
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
 */

#ifndef PIDGIN_ICON_THEME_H
#define PIDGIN_ICON_THEME_H

#include <glib.h>
#include <glib-object.h>
#include "theme.h"

/**
 * extends PurpleTheme (theme.h)
 * A pidgin icon theme.
 * This object represents a Pidgin icon theme.
 *
 * PidginIconTheme is a PurpleTheme Object.
 */
typedef struct _PidginIconTheme        PidginIconTheme;
typedef struct _PidginIconThemeClass   PidginIconThemeClass;

#define PIDGIN_TYPE_ICON_THEME            (pidgin_icon_theme_get_type ())
#define PIDGIN_ICON_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_ICON_THEME, PidginIconTheme))
#define PIDGIN_ICON_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_ICON_THEME, PidginIconThemeClass))
#define PIDGIN_IS_ICON_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_ICON_THEME))
#define PIDGIN_IS_ICON_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_ICON_THEME))
#define PIDGIN_ICON_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_ICON_THEME, PidginIconThemeClass))

struct _PidginIconTheme
{
	PurpleTheme parent;
};

struct _PidginIconThemeClass
{
	PurpleThemeClass parent_class;
};

/**************************************************************************/
/** @name Pidgin Icon Theme API                                          */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_icon_theme_get_type(void);

/**
 * Returns a copy of the filename for the icon event or NULL if it is not set
 *
 * @param theme     the theme
 * @param event		the pidgin icon event to look up
 *
 * @returns the filename of the icon event
 */
const gchar *pidgin_icon_theme_get_icon(PidginIconTheme *theme,
		const gchar *event);

/**
 * Sets the filename for a given icon id, setting the icon to NULL will remove the icon from the theme
 *
 * @param theme         the theme
 * @param icon_id		a string representing what the icon is to be used for
 * @param filename		the name of the file to be used for the given id
 */
void pidgin_icon_theme_set_icon(PidginIconTheme *theme,
		const gchar *icon_id,
		const gchar *filename);

G_END_DECLS
#endif /* PIDGIN_ICON_THEME_H */
