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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "smiley-theme.h"

#include "dbus-maybe.h"
#include "debug.h"

static PurpleSmileyTheme *current = NULL;

/*******************************************************************************
 * API implementation
 ******************************************************************************/

PurpleSmileyList *
purple_smiley_theme_get_smileys(PurpleSmileyTheme *theme, gpointer ui_data)
{
	PurpleSmileyThemeClass *klass;

	g_return_val_if_fail(PURPLE_IS_SMILEY_THEME(theme), NULL);
	klass = PURPLE_SMILEY_THEME_GET_CLASS(theme);
	g_return_val_if_fail(klass != NULL, NULL);
	g_return_val_if_fail(klass->get_smileys != NULL, NULL);

	return klass->get_smileys(theme, ui_data);
}

void
purple_smiley_theme_set_current(PurpleSmileyTheme *theme)
{
	g_return_if_fail(theme == NULL || PURPLE_IS_SMILEY_THEME(theme));

	if (theme)
		g_object_ref(theme);
	if (current)
		g_object_unref(current);
	current = theme;
}

PurpleSmileyTheme *
purple_smiley_theme_get_current(void)
{
	return current;
}

void
purple_smiley_theme_uninit(void)
{
	purple_smiley_theme_set_current(NULL);
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

GType
purple_smiley_theme_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyThemeClass),
			.instance_size = sizeof(PurpleSmileyTheme),
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmileyTheme", &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}
