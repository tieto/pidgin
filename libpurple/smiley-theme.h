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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_SMILEY_THEME_H_
#define _PURPLE_SMILEY_THEME_H_
/**
 * SECTION:smiley-theme
 * @include:smiley-theme.h
 * @section_id: libpurple-smiley-theme
 * @short_description: a categorized set of standard smileys
 * @title: Smiley themes
 *
 * A smiley theme is a set of standard smileys, that may be displayed in user's
 * message window instead of their textual representations. It may be
 * categorized depending on the selected protocol, as in #PidginSmileyTheme, but
 * it's up to the UI to choose behavior.
 */

#include <glib-object.h>

#include "smiley.h"
#include "smiley-list.h"

typedef struct _PurpleSmileyTheme PurpleSmileyTheme;
typedef struct _PurpleSmileyThemeClass PurpleSmileyThemeClass;

#define PURPLE_TYPE_SMILEY_THEME            (purple_smiley_theme_get_type())
#define PURPLE_SMILEY_THEME(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_SMILEY_THEME, PurpleSmileyTheme))
#define PURPLE_SMILEY_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SMILEY_THEME, PurpleSmileyThemeClass))
#define PURPLE_IS_SMILEY_THEME(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_SMILEY_THEME))
#define PURPLE_IS_SMILEY_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_SMILEY_THEME))
#define PURPLE_SMILEY_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SMILEY_THEME, PurpleSmileyThemeClass))

/**
 * PurpleSmileyTheme:
 *
 * An abstract class for smiley theme.
 */
struct _PurpleSmileyTheme
{
	/*< private >*/
	GObject parent;
};

/**
 * PurpleSmileyThemeClass:
 * @get_smileys: a callback for getting smiley list based on choosen category.
 *               The criteria for a category are being passed using the
 *               @ui_data parameter.
 * @activate: a callback being fired after activating the @theme. It may be used
 *            for loading its contents before using @get_smileys callback.
 *
 * Base class for #PurpleSmileyTheme objects.
 */
struct _PurpleSmileyThemeClass
{
	/*< private >*/
	GObjectClass parent_class;

	/*< public >*/
	PurpleSmileyList * (*get_smileys)(PurpleSmileyTheme *theme,
		gpointer ui_data);
	void (*activate)(PurpleSmileyTheme *theme);

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_smiley_theme_get_type:
 *
 * Returns: the #GType for a smiley list.
 */
GType
purple_smiley_theme_get_type(void);

/**
 * purple_smiley_theme_get_smileys:
 * @theme: the smiley theme.
 * @ui_data: the UI-passed criteria to choose a smiley set.
 *
 * Retrieves a smiley category based on UI-provided criteria.
 *
 * You might want to use <link linkend="libpurple-smiley-parser">smiley
 * parser</link> instead. It's mostly for the UI, prpls shouldn't use it.
 *
 * Returns: (transfer none): a #PurpleSmileyList with standard smileys to use.
 */
PurpleSmileyList *
purple_smiley_theme_get_smileys(PurpleSmileyTheme *theme, gpointer ui_data);

/**
 * purple_smiley_theme_set_current:
 * @theme: the smiley theme to be set as currently used. May be %NULL.
 *
 * Sets the new smiley theme to be used for displaying messages.
 */
void
purple_smiley_theme_set_current(PurpleSmileyTheme *theme);

/**
 * purple_smiley_theme_get_current:
 *
 * Returns the currently used smiley theme.
 *
 * Returns: the #PurpleSmileyTheme or %NULL, if none is set.
 */
PurpleSmileyTheme *
purple_smiley_theme_get_current(void);

/**
 * _purple_smiley_theme_uninit: (skip)
 *
 * Uninitializes the smileys theme subsystem.
 */
void
_purple_smiley_theme_uninit(void);

G_END_DECLS

#endif /* _PURPLE_SMILEY_THEME_H_ */
