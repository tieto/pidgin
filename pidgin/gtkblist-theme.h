/**
 * @file gtkblist-theme.h GTK+ Buddy List Theme API
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

#ifndef _PIDGIN_BUDDY_LIST_THEME_H_
#define _PIDGIN_BUDDY_LIST_THEME_H_

#include <glib.h>
#include <glib-object.h>
#include "theme.h"
#include "sound.h"

/**
 * extends PurpleTheme (theme.h)
 * A pidgin buddy list theme.
 * This is an object for Purple to represent a sound theme.
 *
 * PidginBuddyListTheme is a PurpleTheme Object.
 */
typedef struct _PidginBuddyListTheme        PidginBuddyListTheme;
typedef struct _PidginBuddyListThemeClass   PidginBuddyListThemeClass;

#define PIDGIN_TYPE_BUDDY_LIST_THEME		  	(pidgin_buddy_list_theme_get_type ())
#define PIDGIN_BUDDY_LIST_THEME(obj)		  	(G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME, PidginBuddyListTheme))
#define PIDGIN_BUDDY_LIST_THEME_CLASS(klass)	  	(G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_BUDDY_LIST_THEME, PidginBuddyListThemeClass))
#define PIDGIN_IS_BUDDY_LIST_THEME(obj)	  		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME))
#define PIDGIN_IS_BUDDY_LIST_THEME_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_BUDDY_LIST_THEME))
#define PIDGIN_BUDDY_LIST_THEME_GET_CLASS(obj)  	(G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_BUDDY_LIST_THEME, PidginBuddyListThemeClass))

struct _PidginBuddyListTheme
{
	PurpleTheme parent;
	gpointer priv;
};

struct _PidginBuddyListThemeClass
{
	PurpleThemeClass parent_class;
};

typedef struct
{
	gchar *font;
	gchar *color;

} font_color_pair;

typedef struct
{
	gint icon;
	gint text;
	gint status_icon;
	gint protocol_icon;
	gint emblem;
	gboolean show_status; 

} blist_layout;


/**************************************************************************/
/** @name Purple Sound Theme API                                          */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_buddy_list_theme_get_type(void);

/**
 * Returns the icon theme to be used with the buddy list theme
 *
 * @returns 	the icon theme
 */
const gchar *pidgin_buddy_list_theme_get_icon_theme(PidginBuddyListTheme *theme);

/**
 * Returns the opacity of the buddy list window
 * (0.0 or clear to 1.0 fully Opaque)
 *
 * @returns 	the opacity
 */
gdouble pidgin_buddy_list_theme_get_opacity(PidginBuddyListTheme *theme);

/**
 * Returns the layout to be used with the buddy list
 *
 * @returns 	the buddy list layout
 */
const blist_layout *pidgin_buddy_list_theme_get_layout(PidginBuddyListTheme *theme);

/**
 * Returns the background color to be used with expanded groups
 *
 * @returns 	a color
 */
const gchar *pidgin_buddy_list_theme_get_expanded_background_color(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used with expanded groups
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_expanded_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the background color to be used with minimized groups
 *
 * @returns 	a color
 */
const gchar *pidgin_buddy_list_theme_get_minimized_background_color(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used with minimized groups
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_minimized_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the 1st color to be used for buddys
 *
 * @returns 	a color
 */
const gchar *pidgin_buddy_list_theme_get_buddy_color_1(PidginBuddyListTheme *theme);

/**
 * Returns the 2nd color to be used for buddies
 *
 * @returns 	a color
 */
const gchar *pidgin_buddy_list_theme_get_buddy_color_2(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used for online buddies
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_online_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used for away and idle buddies
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_away_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used for offline buddies
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_offline_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used for buddies with unread messages
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_unread_message_text_info(PidginBuddyListTheme *theme);

/**
 * Returns the text font and color to be used for a buddy's status message 
 *
 * @returns 	a font and color pair
 */
const font_color_pair *pidgin_buddy_list_theme_get_status_text_info(PidginBuddyListTheme *theme);

G_END_DECLS
#endif /* _PIDGIN_BUDDY_LIST_THEME_H_ */
