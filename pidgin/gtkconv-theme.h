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

PidginConvTheme *pidgin_conversation_theme_load(const char *styledir);
PidginConvTheme *pidgin_conversation_theme_copy(const PidginConvTheme *theme);
void pidgin_conversation_theme_save_state(const PidginConvTheme *theme);
void pidgin_conversation_theme_read_info_plist(PidginConvTheme *theme, const char *variant);
char *pidgin_conversation_theme_get_variant(PidginConvTheme *theme);
GList *pidgin_conversation_theme_get_variants(PidginConvTheme *theme);
void pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant);

char *pidgin_conversation_theme_get_css(PidginConvTheme *theme);

void
pidgin_conversation_theme_apply(PidginConvTheme *theme, PurpleConversation *conv);

G_END_DECLS
#endif /* PIDGIN_CONV_THEME_H */

