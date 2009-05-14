/*
 * Icon Themes for Pidgin
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

#include "gtkicon-theme.h"
#include "pidginstock.h"

#include <gtk/gtk.h>

#define PIDGIN_ICON_THEME_GET_PRIVATE(Gobject) \
	((PidginIconThemePrivate *) ((PIDGIN_ICON_THEME(Gobject))->priv))

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	/* used to store filenames of diffrent icons */
	GHashTable *icon_files;
} PidginIconThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_icon_theme_init(GTypeInstance *instance,
		gpointer klass)
{
	PidginIconThemePrivate *priv;

	(PIDGIN_ICON_THEME(instance))->priv = g_new0(PidginIconThemePrivate, 1);

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(instance);

	priv->icon_files = g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, g_free);
}

static void
pidgin_icon_theme_finalize(GObject *obj)
{
	PidginIconThemePrivate *priv;

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(obj);

	g_hash_table_destroy(priv->icon_files);
	g_free(priv);

	parent_class->finalize(obj);
}

static void
pidgin_icon_theme_class_init(PidginIconThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = pidgin_icon_theme_finalize;
}

GType
pidgin_icon_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginIconThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_icon_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginIconTheme),
			0, /* n_preallocs */
			pidgin_icon_theme_init, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME,
				"PidginIconTheme", &info, G_TYPE_FLAG_ABSTRACT);
	}
	return type;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

const gchar *
pidgin_icon_theme_get_icon(PidginIconTheme *theme,
		const gchar *id)
{
	PidginIconThemePrivate *priv;

	g_return_val_if_fail(PIDGIN_IS_ICON_THEME(theme), NULL);

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(theme);

	return g_hash_table_lookup(priv->icon_files, id);
}

void
pidgin_icon_theme_set_icon(PidginIconTheme *theme,
		const gchar *id,
		const gchar *filename)
{
	PidginIconThemePrivate *priv;
	g_return_if_fail(PIDGIN_IS_ICON_THEME(theme));

	priv = PIDGIN_ICON_THEME_GET_PRIVATE(theme);

	if (filename != NULL)
		g_hash_table_replace(priv->icon_files,
				g_strdup(id), g_strdup(filename));
	else
		g_hash_table_remove(priv->icon_files, id);
}
