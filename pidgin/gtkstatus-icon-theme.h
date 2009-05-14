/**
 * @file gtkstatus-icon-theme.h  Pidgin Icon Theme  Class API
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

#ifndef PIDGIN_STATUS_ICON_THEME_H
#define PIDGIN_STATUS_ICON_THEME_H

#include <glib-object.h>
#include "gtkicon-theme.h"

/**
 * extends PidginIconTheme (gtkicon-theme.h)
 * A pidgin status icon theme.
 * This object represents a Pidgin status icon theme.
 *
 * PidginStatusIconTheme is a PidginIconTheme Object.
 */
typedef struct _PidginStatusIconTheme        PidginStatusIconTheme;
typedef struct _PidginStatusIconThemeClass   PidginStatusIconThemeClass;

#define PIDGIN_TYPE_STATUS_ICON_THEME            (pidgin_status_icon_theme_get_type ())
#define PIDGIN_STATUS_ICON_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_STATUS_ICON_THEME, PidginStatusIconTheme))
#define PIDGIN_STATUS_ICON_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_STATUS_ICON_THEME, PidginStatusIconThemeClass))
#define PIDGIN_IS_STATUS_ICON_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_STATUS_ICON_THEME))
#define PIDGIN_IS_STATUS_ICON_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_STATUS_ICON_THEME))
#define PIDGIN_STATUS_ICON_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_STATUS_ICON_THEME, PidginStatusIconThemeClass))

struct _PidginStatusIconTheme
{
	PidginIconTheme parent;
};

struct _PidginStatusIconThemeClass
{
	PidginIconThemeClass parent_class;
};

/**************************************************************************/
/** @name Pidgin Status Icon Theme API                                          */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_status_icon_theme_get_type(void);

G_END_DECLS
#endif /* PIDGIN_STATUS_ICON_THEME_H */
