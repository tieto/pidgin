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
 * A list of smileys.
 */
struct _PurpleSmileyTheme
{
	/*< private >*/
	GObject parent;
};

struct _PurpleSmileyThemeClass
{
	/*< private >*/
	GObjectClass parent_class;

	PurpleSmileyList * (*get_smileys)(PurpleSmileyTheme *theme,
		gpointer ui_data);
	void (*activate)(PurpleSmileyTheme *theme);

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_smiley_theme_get_type:
 *
 * Returns: The #GType for a smiley list.
 */
GType
purple_smiley_theme_get_type(void);

PurpleSmileyList *
purple_smiley_theme_get_smileys(PurpleSmileyTheme *theme, gpointer ui_data);

void
purple_smiley_theme_set_current(PurpleSmileyTheme *theme);

PurpleSmileyTheme *
purple_smiley_theme_get_current(void);

void
purple_smiley_theme_uninit(void);

G_END_DECLS

#endif /* _PURPLE_SMILEY_THEME_H_ */

