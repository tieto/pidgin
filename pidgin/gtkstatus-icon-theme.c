/*
 * Status Icon Themes for Pidgin
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

#include "gtkstatus-icon-theme.h"

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_status_icon_theme_finalize(GObject *obj)
{
	parent_class->finalize(obj);
}

static void
pidgin_status_icon_theme_class_init(PidginStatusIconThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = pidgin_status_icon_theme_finalize;
}

GType
pidgin_status_icon_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (PidginStatusIconThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_status_icon_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginStatusIconTheme),
			0, /* n_preallocs */
			NULL,
			NULL, /* value table */
		};
		type = g_type_register_static(PIDGIN_TYPE_ICON_THEME,
				"PidginStatusIconTheme", &info, 0);
	}
	return type;
}
