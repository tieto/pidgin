/**
 * @file gtkconv-theme.h  Pidgin Conversation Theme  Class API
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

#ifndef PIDGIN_CONV_THEME_H
#define PIDGIN_CONV_THEME_H

#include <glib.h>
#include <glib-object.h>
#include "conversation.h"
#include "theme.h"

/**
 * extends PurpleTheme (theme.h)
 * A pidgin icon theme.
 * This object represents a Pidgin icon theme.
 *
 * PidginConvTheme is a PurpleTheme Object.
 */
typedef struct _PidginConvTheme        PidginConvTheme;
typedef struct _PidginConvThemeClass   PidginConvThemeClass;

#define PIDGIN_TYPE_CONV_THEME            (pidgin_conversation_theme_get_type ())
#define PIDGIN_CONV_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_CONV_THEME, PidginConvTheme))
#define PIDGIN_CONV_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_CONV_THEME, PidginConvThemeClass))
#define PIDGIN_IS_CONV_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_CONV_THEME))
#define PIDGIN_IS_CONV_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_CONV_THEME))
#define PIDGIN_CONV_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_CONV_THEME, PidginConvThemeClass))

struct _PidginConvTheme
{
	PurpleTheme parent;
};

struct _PidginConvThemeClass
{
	PurpleThemeClass parent_class;
};

/**************************************************************************/
/** @name Pidgin Conversation Theme API                                   */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * GObject foo.
 * @internal.
 */
GType pidgin_conversation_theme_get_type(void);

/**
 * Get the Info.plist hash table from a conversation theme.
 *
 * @param theme The conversation theme
 *
 * @return The hash table. Keys are strings as outlined for message styles,
 *         values are GValue*s. This is an internal structure. Take a ref if
 *         necessary, but don't destroy it yourself.
 */
const GHashTable *pidgin_conversation_theme_get_info(const PidginConvTheme *theme);

/**
 * Set the Info.plist hash table for a conversation theme.
 *
 * @param theme The conversation theme
 * @param info  The new hash table. The theme will take ownership of this hash
 *              table. Do not use it yourself afterwards with holding a ref.
 *              For key and value specifications, @see pidgin_conversation_theme_get_info.
 *
 */
void pidgin_conversation_theme_set_info(PidginConvTheme *theme, GHashTable *info);

/**
 * Add an available variant name to a conversation theme.
 *
 * @param theme   The conversation theme
 * @param variant The name of the variant
 *
 * @Note The conversation theme will take ownership of the variant name string.
 *       This function should normally only be called by the theme loader.
 */
void pidgin_conversation_theme_add_variant(PidginConvTheme *theme, char *variant);

/**
 * Get the currently set variant name for a conversation theme.
 *
 * @param theme The conversation theme
 *
 * @return The current variant name.
 */
const char *pidgin_conversation_theme_get_variant(PidginConvTheme *theme);

/**
 * Set the variant name for a conversation theme.
 *
 * @param theme   The conversation theme
 * @param variant The name of the variant
 *
 */
void pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant);

/**
 * Get a list of available variants for a conversation theme.
 *
 * @param theme The conversation theme
 *
 * @return The list of variants. This GList and the string data are owned by
 *         the theme and should not be freed by the caller.
 */
const GList *pidgin_conversation_theme_get_variants(PidginConvTheme *theme);

PidginConvTheme *pidgin_conversation_theme_copy(const PidginConvTheme *theme);

char *pidgin_conversation_theme_get_css(PidginConvTheme *theme);

void
pidgin_conversation_theme_apply(PidginConvTheme *theme, PurpleConversation *conv);

G_END_DECLS
#endif /* PIDGIN_CONV_THEME_H */

