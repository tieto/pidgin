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

#include "gtksmiley-theme.h"

#define PIDGIN_SMILEY_THEME_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_SMILEY_THEME, \
	PurpleSmileyThemePrivate))

typedef struct
{
} PurpleSmileyThemePrivate;

static GObjectClass *parent_class;

/*******************************************************************************
 * API implementation
 ******************************************************************************/

static PurpleSmileyList *
pidgin_smiley_theme_get_smileys_impl(PurpleSmileyTheme *theme, gpointer ui_data)
{
	return NULL;
}

void
pidgin_smiley_theme_init(void)
{
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
pidgin_smiley_theme_finalize(GObject *obj)
{
	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
pidgin_smiley_theme_class_init(PurpleSmileyListClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);
	PurpleSmileyThemeClass *pst_class = PURPLE_SMILEY_THEME_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleSmileyThemePrivate));

	gobj_class->finalize = pidgin_smiley_theme_finalize;

	pst_class->get_smileys = pidgin_smiley_theme_get_smileys_impl;
}

GType
purple_smiley_theme_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyThemeClass),
			.class_init = (GClassInitFunc)pidgin_smiley_theme_class_init,
			.instance_size = sizeof(PurpleSmileyTheme),
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmileyTheme", &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}
