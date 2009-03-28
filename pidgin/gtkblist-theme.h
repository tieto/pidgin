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

#ifndef PIDGIN_BLIST_THEME_H
#define PIDGIN_BLIST_THEME_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "theme.h"

/**
 * A pidgin buddy list theme.
 * This is an object for Purple to represent a buddy list theme.
 *
 * PidginBlistTheme is a PurpleTheme Object.
 */
typedef struct _PidginBlistTheme        PidginBlistTheme;
typedef struct _PidginBlistThemeClass   PidginBlistThemeClass;

#define PIDGIN_TYPE_BLIST_THEME            (pidgin_blist_theme_get_type ())
#define PIDGIN_BLIST_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_BLIST_THEME, PidginBlistTheme))
#define PIDGIN_BLIST_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_BLIST_THEME, PidginBlistThemeClass))
#define PIDGIN_IS_BLIST_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_BLIST_THEME))
#define PIDGIN_IS_BLIST_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_BLIST_THEME))
#define PIDGIN_BLIST_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_BLIST_THEME, PidginBlistThemeClass))

struct _PidginBlistTheme
{
	PurpleTheme parent;
	gpointer priv;
};

struct _PidginBlistThemeClass
{
	PurpleThemeClass parent_class;
};

typedef struct
{
	gchar *font;
	gchar *color;

} FontColorPair;

typedef struct
{
	gint status_icon;
	gint text;
	gint emblem;
	gint protocol_icon;
	gint buddy_icon;
	gboolean show_status;

} PidginBlistLayout;

/**************************************************************************/
/** @name FontColorPair API                                               */
/**************************************************************************/

/**
 * Frees a font and color pair
 */
void free_font_and_color(FontColorPair *pair);

/**************************************************************************/
/** @name Purple Buddy List Theme API                                     */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_blist_theme_get_type(void);

/* get methods */

/**
 * Returns the background color of the buddy list.
 *
 * @param theme The theme.
 *
 * @returns A gdk color.
 */
 GdkColor *pidgin_blist_theme_get_background_color(PidginBlistTheme *theme);

/**
 * Returns the opacity of the buddy list window
 * (0.0 or clear to 1.0 fully opaque).
 *
 * @param theme The theme.
 *
 * @returns The opacity
 */
gdouble pidgin_blist_theme_get_opacity(PidginBlistTheme *theme);

/**
 * Returns the layout to be used with the buddy list.
 *
 * @param theme The theme.
 *
 * @returns The buddy list layout.
 */
 PidginBlistLayout *pidgin_blist_theme_get_layout(PidginBlistTheme *theme);

/**
 * Returns the background color to be used with expanded groups.
 *
 * @param theme The theme.
 *
 * @returns A gdk color.
 */
 GdkColor *pidgin_blist_theme_get_expanded_background_color(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used with expanded groups.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_expanded_text_info(PidginBlistTheme *theme);

/**
 * Returns the background color to be used with collapsed groups.
 *
 * @param theme The theme.
 *
 * @returns A gdk color.
 */
 GdkColor *pidgin_blist_theme_get_collapsed_background_color(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used with collapsed groups.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_collapsed_text_info(PidginBlistTheme *theme);

/**
 * Returns the colors to be used for contacts and chats.
 *
 * @param theme The theme.
 *
 * @returns A gdkcolor for contacts and chats.
 */
 GdkColor *pidgin_blist_theme_get_contact_color(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for expanded contacts.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_contact_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for online buddies.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_online_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for away and idle buddies.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_away_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for offline buddies.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_offline_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for idle buddies.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_idle_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for buddies with unread messages.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_unread_message_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for chats with unread messages
 * that mention your nick.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_unread_message_nick_said_text_info(PidginBlistTheme *theme);

/**
 * Returns the text font and color to be used for a buddy's status message.
 *
 * @param theme The theme.
 *
 * @returns A font and color pair.
 */
 FontColorPair *pidgin_blist_theme_get_status_text_info(PidginBlistTheme *theme);

/* Set Methods */

/**
 * Sets the background color to be used for this buddy list theme.
 *
 * @param theme The theme.
 * @param color The new background color.
 */
void pidgin_blist_theme_set_background_color(PidginBlistTheme *theme, GdkColor *color);

/**
 * Sets the opacity to be used for this buddy list theme.
 *
 * @param theme The theme.
 * @param opacity The new opacity setting.
 */
void pidgin_blist_theme_set_opacity(PidginBlistTheme *theme, gdouble opacity);

/**
 * Sets the buddy list layout to be used for this buddy list theme.
 *
 * @param theme The theme.
 * @param layout The new layout.
 */
void pidgin_blist_theme_set_layout(PidginBlistTheme *theme, PidginBlistLayout *layout);

/**
 * Sets the background color to be used for expanded groups.
 *
 * @param theme The theme.
 * @param color The new background color.
 */
void pidgin_blist_theme_set_expanded_background_color(PidginBlistTheme *theme, GdkColor *color);

/**
 * Sets the text color and font to be used for expanded groups.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_expanded_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the background color to be used for collapsed groups.
 *
 * @param theme The theme.
 * @param color The new background color.
 */
void pidgin_blist_theme_set_collapsed_background_color(PidginBlistTheme *theme, GdkColor *color);

/**
 * Sets the text color and font to be used for expanded groups.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_collapsed_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the background color to be used for contacts and chats.
 *
 * @param theme The theme.
 * @param color The color to use for contacts and chats.
 */
void pidgin_blist_theme_set_contact_color(PidginBlistTheme *theme, GdkColor *color);

/**
 * Sets the text color and font to be used for expanded contacts.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_contact_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for online buddies.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_online_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for away and idle buddies.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_away_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for offline buddies.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_offline_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for idle buddies.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_idle_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for buddies with unread messages.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_unread_message_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for a chat with unread messages
 * that mention your nick.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_unread_message_nick_said_text_info(PidginBlistTheme *theme, FontColorPair *pair);

/**
 * Sets the text color and font to be used for buddy status messages.
 *
 * @param theme The theme.
 * @param pair The new text font and color pair.
 */
void pidgin_blist_theme_set_status_text_info(PidginBlistTheme *theme, FontColorPair *pair);

G_END_DECLS
#endif /* PIDGIN_BLIST_THEME_H */
