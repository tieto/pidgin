/**
 * @file gtkblist-loader.h  Pidgin Buddy List Theme Loader Class API
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

#ifndef _PIDGIN_BUDDY_LIST_THEME_LOADER_H_
#define _PIDGIN_BUDDY_LIST_THEME_LOADER_H_

#include <glib.h>
#include <glib-object.h>
#include "theme-loader.h"

/**
 * A pidgin buddy list theme loader. extends PurpleThemeLoader (theme-loader.h)
 * This is a class designed to build sound themes
 *
 * PidginBuddyListThemeLoader is a GObject.
 */
typedef struct _PidginBuddyListThemeLoader        PidginBuddyListThemeLoader;
typedef struct _PidginBuddyListThemeLoaderClass   PidginBuddyListThemeLoaderClass;

#define PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER			  (pidgin_buddy_list_theme_loader_get_type ())
#define PIDGIN_BUDDY_LIST_THEME_LOADER(obj)			  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER, PidginBuddyListThemeLoader))
#define PIDGIN_BUDDY_LIST_THEME_LOADER_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER, PidginBuddyListThemeLoaderClass))
#define PIDGIN_IS_BUDDY_LIST_THEME_LOADER(obj)	  	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER))
#define PIDGIN_IS_BUDDY_LIST_THEME_LOADER_CLASS(klass) 	  (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER))
#define PIDGIN_BUDDY_LIST_THEME_LOADER_GET_CLASS(obj)  	  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME_LOADER, PidginBuddyListThemeLoaderClass))

struct _PidginBuddyListThemeLoader
{
	PurpleThemeLoader parent;
};

struct _PidginBuddyListThemeLoaderClass
{
	PurpleThemeLoaderClass parent_class;
};

/**************************************************************************/
/** @name Buddy List Theme-Loader API                                     */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_buddy_list_theme_loader_get_type(void);

G_END_DECLS
#endif /* _PIDGIN_BUDDY_LIST_THEME_LOADER_H_ */
