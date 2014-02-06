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
/**
 * SECTION:gtkblist-theme
 * @section_id: pidgin-gtkblist-theme
 * @short_description: <filename>gtkblist-theme.h</filename>
 * @title: Buddy List Theme API
 */

#ifndef PIDGIN_BLIST_THEME_H
#define PIDGIN_BLIST_THEME_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "theme.h"

typedef struct _PidginBlistTheme        PidginBlistTheme;
typedef struct _PidginBlistThemeClass   PidginBlistThemeClass;

#define PIDGIN_TYPE_BLIST_THEME            (pidgin_blist_theme_get_type ())
#define PIDGIN_BLIST_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_BLIST_THEME, PidginBlistTheme))
#define PIDGIN_BLIST_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_BLIST_THEME, PidginBlistThemeClass))
#define PIDGIN_IS_BLIST_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_BLIST_THEME))
#define PIDGIN_IS_BLIST_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_BLIST_THEME))
#define PIDGIN_BLIST_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_BLIST_THEME, PidginBlistThemeClass))

/**
 * PidginBlistTheme:
 *
 * A pidgin buddy list theme.
 * This is an object for Purple to represent a buddy list theme.
 *
 * PidginBlistTheme is a PurpleTheme Object.
 */
struct _PidginBlistTheme
{
	PurpleTheme parent;
};

struct _PidginBlistThemeClass
{
	PurpleThemeClass parent_class;
};

#if 0
typedef struct _PidginThemeFont PidginThemeFont;
struct _PidginThemeFont
{
	const gchar *font;
	const gchar *color;

};
#endif
typedef struct _PidginThemeFont PidginThemeFont;

typedef struct _PidginBlistLayout PidginBlistLayout;

struct _PidginBlistLayout
{
	gint status_icon;
	gint text;
	gint emblem;
	gint protocol_icon;
	gint buddy_icon;
	gboolean show_status;

};

G_BEGIN_DECLS

/**************************************************************************/
/* PidginThemeFont API                                                    */
/**************************************************************************/

/**
 * pidgin_theme_font_new:
 * @face:  The font face
 * @color: The color of the font
 *
 * Create a new PidginThemeFont.
 *
 * Returns: A newly created PidginThemeFont
 */
PidginThemeFont * pidgin_theme_font_new(const gchar *face, GdkColor *color);

/**
 * pidgin_theme_font_free:
 * @font: The theme font
 *
 * Frees a font and color pair
 */
void pidgin_theme_font_free(PidginThemeFont *font);

/**
 * pidgin_theme_font_set_font_face:
 * @font:  The PidginThemeFont
 * @face:  The font-face
 *
 * Set the font-face of a PidginThemeFont.
 */
void pidgin_theme_font_set_font_face(PidginThemeFont *font, const gchar *face);

/**
 * pidgin_theme_font_set_color:
 * @font:  The PidginThemeFont
 * @color: The color
 *
 * Set the color of a PidginThemeFont.
 */
void pidgin_theme_font_set_color(PidginThemeFont *font, const GdkColor *color);

/**
 * pidgin_theme_font_get_font_face:
 * @font:  The PidginThemeFont
 *
 * Get the font-face of a PidginThemeFont.
 *
 * Returns: The font-face, or NULL if none is set.
 */
const gchar * pidgin_theme_font_get_font_face(PidginThemeFont *font);

/**
 * pidgin_theme_font_get_color:
 * @font:  The PidginThemeFont
 *
 * Get the color of a PidginThemeFont as a GdkColor object.
 *
 * Returns: The color, or NULL if none is set.
 */
const GdkColor * pidgin_theme_font_get_color(PidginThemeFont *font);

/**
 * pidgin_theme_font_get_color_describe:
 * @font:  The PidginThemeFont
 *
 * Get the color of a PidginThemeFont.
 *
 * Returns: The color, or NULL if none is set.
 */
const gchar * pidgin_theme_font_get_color_describe(PidginThemeFont *font);

/**************************************************************************/
/* Purple Buddy List Theme API                                            */
/**************************************************************************/

/**
 * pidgin_blist_theme_get_type:
 *
 * Returns: The #GType for a blist theme.
 */
GType pidgin_blist_theme_get_type(void);

/* get methods */

/**
 * pidgin_blist_theme_get_background_color:
 * @theme:  The PidginBlist theme.
 *
 * Returns the background color of the buddy list.
 *
 * Returns: A gdk color.
 */
 GdkColor *pidgin_blist_theme_get_background_color(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_opacity:
 * @theme:  The PidginBlist theme.
 *
 * Returns the opacity of the buddy list window
 * (0.0 or clear to 1.0 fully opaque).
 *
 * Returns: The opacity
 */
gdouble pidgin_blist_theme_get_opacity(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_layout:
 * @theme:  The PidginBlist theme.
 *
 * Returns the layout to be used with the buddy list.
 *
 * Returns: The buddy list layout.
 */
PidginBlistLayout *pidgin_blist_theme_get_layout(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_expanded_background_color:
 * @theme:  The PidginBlist theme.
 *
 * Returns the background color to be used with expanded groups.
 *
 * Returns: A gdk color.
 */
GdkColor *pidgin_blist_theme_get_expanded_background_color(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_expanded_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used with expanded groups.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_expanded_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_collapsed_background_color:
 * @theme:  The PidginBlist theme.
 *
 * Returns the background color to be used with collapsed groups.
 *
 * Returns: A gdk color.
 */
GdkColor *pidgin_blist_theme_get_collapsed_background_color(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_collapsed_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used with collapsed groups.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_collapsed_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_contact_color:
 * @theme:  The PidginBlist theme.
 *
 * Returns the colors to be used for contacts and chats.
 *
 * Returns: A gdkcolor for contacts and chats.
 */
GdkColor *pidgin_blist_theme_get_contact_color(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_contact_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for expanded contacts.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_contact_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_online_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for online buddies.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_online_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_away_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for away and idle buddies.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_away_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_offline_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for offline buddies.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_offline_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_idle_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for idle buddies.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_idle_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_unread_message_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for buddies with unread messages.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_unread_message_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_unread_message_nick_said_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for chats with unread messages
 * that mention your nick.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_unread_message_nick_said_text_info(PidginBlistTheme *theme);

/**
 * pidgin_blist_theme_get_status_text_info:
 * @theme:  The PidginBlist theme.
 *
 * Returns the text font and color to be used for a buddy's status message.
 *
 * Returns: A font and color pair.
 */
PidginThemeFont *pidgin_blist_theme_get_status_text_info(PidginBlistTheme *theme);

/* Set Methods */

/**
 * pidgin_blist_theme_set_background_color:
 * @theme:  The PidginBlist theme.
 * @color: The new background color.
 *
 * Sets the background color to be used for this buddy list theme.
 */
void pidgin_blist_theme_set_background_color(PidginBlistTheme *theme, const GdkColor *color);

/**
 * pidgin_blist_theme_set_opacity:
 * @theme:  The PidginBlist theme.
 * @opacity: The new opacity setting.
 *
 * Sets the opacity to be used for this buddy list theme.
 */
void pidgin_blist_theme_set_opacity(PidginBlistTheme *theme, gdouble opacity);

/**
 * pidgin_blist_theme_set_layout:
 * @theme:  The PidginBlist theme.
 * @layout: The new layout.
 *
 * Sets the buddy list layout to be used for this buddy list theme.
 */
void pidgin_blist_theme_set_layout(PidginBlistTheme *theme, const PidginBlistLayout *layout);

/**
 * pidgin_blist_theme_set_expanded_background_color:
 * @theme:  The PidginBlist theme.
 * @color: The new background color.
 *
 * Sets the background color to be used for expanded groups.
 */
void pidgin_blist_theme_set_expanded_background_color(PidginBlistTheme *theme, const GdkColor *color);

/**
 * pidgin_blist_theme_set_expanded_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for expanded groups.
 */
void pidgin_blist_theme_set_expanded_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_collapsed_background_color:
 * @theme:  The PidginBlist theme.
 * @color: The new background color.
 *
 * Sets the background color to be used for collapsed groups.
 */
void pidgin_blist_theme_set_collapsed_background_color(PidginBlistTheme *theme, const GdkColor *color);

/**
 * pidgin_blist_theme_set_collapsed_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for expanded groups.
 */
void pidgin_blist_theme_set_collapsed_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_contact_color:
 * @theme:  The PidginBlist theme.
 * @color: The color to use for contacts and chats.
 *
 * Sets the background color to be used for contacts and chats.
 */
void pidgin_blist_theme_set_contact_color(PidginBlistTheme *theme, const GdkColor *color);

/**
 * pidgin_blist_theme_set_contact_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for expanded contacts.
 */
void pidgin_blist_theme_set_contact_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_online_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for online buddies.
 */
void pidgin_blist_theme_set_online_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_away_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for away and idle buddies.
 */
void pidgin_blist_theme_set_away_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_offline_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for offline buddies.
 */
void pidgin_blist_theme_set_offline_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_idle_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for idle buddies.
 */
void pidgin_blist_theme_set_idle_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_unread_message_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for buddies with unread messages.
 */
void pidgin_blist_theme_set_unread_message_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_unread_message_nick_said_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for a chat with unread messages
 * that mention your nick.
 */
void pidgin_blist_theme_set_unread_message_nick_said_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

/**
 * pidgin_blist_theme_set_status_text_info:
 * @theme:  The PidginBlist theme.
 * @pair: The new text font and color pair.
 *
 * Sets the text color and font to be used for buddy status messages.
 */
void pidgin_blist_theme_set_status_text_info(PidginBlistTheme *theme, const PidginThemeFont *pair);

G_END_DECLS

#endif /* PIDGIN_BLIST_THEME_H */
