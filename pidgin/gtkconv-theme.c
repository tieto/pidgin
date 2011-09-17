/*
 * Conversation Themes for Pidgin
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

#include "gtkconv-theme.h"
#include "pidginstock.h"

#include <gtk/gtk.h>

#define PIDGIN_CONV_THEME_GET_PRIVATE(Gobject) \
	(G_TYPE_INSTANCE_GET_PRIVATE((Gobject), PIDGIN_TYPE_CONV_THEME, PidginConvThemePrivate))

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	/* Boilerplate... */
	gpointer unused;
} PidginConvThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_conv_theme_init(GTypeInstance *instance,
		gpointer klass)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(instance);

	priv->unused = NULL; /* Boilerplate... */
}

static void
pidgin_conv_theme_finalize(GObject *obj)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(obj);

	parent_class->finalize(obj);
}

static void
pidgin_conv_theme_class_init(PidginIconThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = pidgin_conv_theme_finalize;
}

GType
pidgin_conversation_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginConvThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_conv_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginConvTheme),
			0, /* n_preallocs */
			pidgin_conv_theme_init, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME,
				"PidginConvTheme", &info, G_TYPE_FLAG_ABSTRACT);
	}
	return type;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

