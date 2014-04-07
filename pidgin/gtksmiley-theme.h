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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PIDGIN_SMILEY_THEME_H_
#define _PIDGIN_SMILEY_THEME_H_
/**
 * SECTION:gtksmiley-theme
 * @include:gtksmiley-theme.h
 * @section_id: pidgin-smiley-theme
 * @short_description: a per-protocol categorized sets of standard smileys
 * @title: Pidgin's smiley themes
 *
 * This class implements a per-protocol based #PurpleSmileyTheme.
 */

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "conversation.h"
#include "smiley-theme.h"

typedef struct _PidginSmileyTheme PidginSmileyTheme;
typedef struct _PidginSmileyThemeClass PidginSmileyThemeClass;

#define PIDGIN_TYPE_SMILEY_THEME            (pidgin_smiley_theme_get_type())
#define PIDGIN_SMILEY_THEME(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyTheme))
#define PIDGIN_SMILEY_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyThemeClass))
#define PIDGIN_IS_SMILEY_THEME(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PIDGIN_TYPE_SMILEY_THEME))
#define PIDGIN_IS_SMILEY_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_SMILEY_THEME))
#define PIDGIN_SMILEY_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyThemeClass))

/**
 * PidginSmileyTheme:
 *
 * An implementation of a smiley theme.
 */
struct _PidginSmileyTheme
{
	/*< private >*/
	PurpleSmileyTheme parent;
};

/**
 * PidginSmileyThemeClass:
 *
 * Base class for #PidginSmileyTheme objects.
 */
struct _PidginSmileyThemeClass
{
	/*< private >*/
	PurpleSmileyThemeClass parent_class;

	void (*pidgin_reserved1)(void);
	void (*pidgin_reserved2)(void);
	void (*pidgin_reserved3)(void);
	void (*pidgin_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * pidgin_smiley_theme_get_type:
 *
 * Returns: the #GType for a smiley list.
 */
GType
pidgin_smiley_theme_get_type(void);

/**
 * pidgin_smiley_theme_get_name:
 * @theme: the smiley theme.
 *
 * Returns the name for a @theme. Valid themes always have the name set.
 *
 * Returns: (transfer none): the name string, or %NULL if error occured.
 */
const gchar *
pidgin_smiley_theme_get_name(PidginSmileyTheme *theme);

/**
 * pidgin_smiley_theme_get_description:
 * @theme: the smiley theme.
 *
 * Returns the description for a @theme.
 *
 * Returns: (transfer none): the description string, or %NULL if it's not
 *          set or error occured.
 */
const gchar *
pidgin_smiley_theme_get_description(PidginSmileyTheme *theme);

/**
 * pidgin_smiley_theme_get_icon:
 * @theme: the smiley theme.
 *
 * Returns the @theme's icon image, possibly loading it from the disk (and
 * adding it to the cache).
 *
 * Returns: (transfer none): the @theme's icon image.
 */
GdkPixbuf *
pidgin_smiley_theme_get_icon(PidginSmileyTheme *theme);

/**
 * pidgin_smiley_theme_get_author:
 * @theme: the smiley theme.
 *
 * Returns the autor of @theme.
 *
 * Returns: (transfer none): the author string, or %NULL if it's not
 *          set or error occured.
 */
const gchar *
pidgin_smiley_theme_get_author(PidginSmileyTheme *theme);

/**
 * pidgin_smiley_theme_for_conv:
 * @conv: the conversation.
 *
 * Gets the smiley list for a @conv based on current theme.
 *
 * Returns: (transfer none): the smiley list, or %NULL if there
 *          is no smiley theme set.
 */
PurpleSmileyList *
pidgin_smiley_theme_for_conv(PurpleConversation *conv);

/**
 * pidgin_smiley_theme_get_all:
 *
 * Returns the list of currently available smiley themes.
 *
 * Returns: (transfer none): the #GList of #PidginSmileyTheme's.
 */
GList *
pidgin_smiley_theme_get_all(void);

/**
 * _pidgin_smiley_theme_init: (skip)
 *
 * Initializes the Pidgin's smiley theme subsystem.
 */
void
_pidgin_smiley_theme_init(void);

/**
 * _pidgin_smiley_theme_uninit: (skip)
 *
 * Unitializes the Pidgin's smiley theme subsystem.
 */
void
_pidgin_smiley_theme_uninit(void);

G_END_DECLS

#endif /* _PIDGIN_SMILEY_THEME_H_ */
