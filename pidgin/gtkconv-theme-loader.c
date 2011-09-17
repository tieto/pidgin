/*
 * PidginConvThemeLoader for Pidgin
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

#include "gtkconv-theme-loader.h"
#include "gtkconv-theme.h"

#include "xmlnode.h"
#include "debug.h"

/*****************************************************************************
 * Conversation Theme Builder
 *****************************************************************************/

static PurpleTheme *
pidgin_conv_loader_build(const gchar *dir)
{
	PidginConvTheme *theme = NULL;

	/* Find the theme file */
	g_return_val_if_fail(dir != NULL, NULL);

	return PURPLE_THEME(theme);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_conv_theme_loader_class_init(PidginConvThemeLoaderClass *klass)
{
	PurpleThemeLoaderClass *loader_klass = PURPLE_THEME_LOADER_CLASS(klass);

	loader_klass->purple_theme_loader_build = pidgin_conv_loader_build;
}


GType
pidgin_conversation_theme_loader_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginConvThemeLoaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_conv_theme_loader_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginConvThemeLoader),
			0, /* n_preallocs */
			NULL, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME_LOADER,
				"PidginConvThemeLoader", &info, 0);
	}
	return type;
}

