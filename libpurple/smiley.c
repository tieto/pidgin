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

#include "internal.h"
#include "glibcompat.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "smiley.h"
#include "util.h"
#include "xmlnode.h"

#define PURPLE_SMILEY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY, PurpleSmileyPrivate))

typedef struct {
	gchar *shortcut;
	gchar *path;
	PurpleImage *image;
	gboolean is_ready;
} PurpleSmileyPrivate;

enum
{
	PROP_0,
	PROP_SHORTCUT,
	PROP_IS_READY,
	PROP_PATH,
	PROP_LAST
};

enum
{
	SIG_READY,
	SIG_LAST
};

static GObjectClass *parent_class;
static guint signals[SIG_LAST];
static GParamSpec *properties[PROP_LAST];

/*******************************************************************************
 * API implementation
 ******************************************************************************/

PurpleSmiley *
purple_smiley_new(const gchar *shortcut, const gchar *path)
{
	g_return_val_if_fail(shortcut != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	return g_object_new(PURPLE_TYPE_SMILEY,
		"shortcut", shortcut,
		"is-ready", TRUE,
		"path", path,
		NULL);
}

const gchar *
purple_smiley_get_shortcut(const PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->shortcut;
}

gboolean
purple_smiley_is_ready(const PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->is_ready;
}

const gchar *
purple_smiley_get_path(PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->path;
}

static PurpleImage *
purple_smiley_get_image_impl(PurpleSmiley *smiley)
{
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);
	const gchar *path;

	g_return_val_if_fail(priv != NULL, FALSE);

	if (priv->image)
		return priv->image;

	path = purple_smiley_get_path(smiley);

	if (!path) {
		purple_debug_error("smiley", "Can't get smiley data "
			"without a path");
		return NULL;
	}

	priv->image = purple_image_new_from_file(path, TRUE);
	if (!priv->image) {
		purple_debug_error("smiley", "Couldn't load smiley data ");
		return NULL;
	}
	return priv->image;
}

PurpleImage *
purple_smiley_get_image(PurpleSmiley *smiley)
{
	PurpleSmileyClass *klass;

	g_return_val_if_fail(PURPLE_IS_SMILEY(smiley), NULL);
	klass = PURPLE_SMILEY_GET_CLASS(smiley);
	g_return_val_if_fail(klass != NULL, NULL);
	g_return_val_if_fail(klass->get_image != NULL, NULL);

	return klass->get_image(smiley);
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_smiley_init(GTypeInstance *instance, gpointer klass)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(instance);
	PURPLE_DBUS_REGISTER_POINTER(smiley, PurpleSmiley);
}

static void
purple_smiley_finalize(GObject *obj)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(obj);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_free(priv->shortcut);
	g_free(priv->path);

	if (priv->image)
		g_object_unref(priv->image);

	PURPLE_DBUS_UNREGISTER_POINTER(smiley);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_smiley_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(object);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	switch (par_id) {
		case PROP_SHORTCUT:
			g_value_set_string(value, priv->shortcut);
			break;
		case PROP_IS_READY:
			g_value_set_boolean(value, priv->is_ready);
			break;
		case PROP_PATH:
			g_value_set_string(value, priv->path);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(object);
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	switch (par_id) {
		case PROP_SHORTCUT:
			g_free(priv->shortcut);
			priv->shortcut = g_strdup(g_value_get_string(value));
			break;
		case PROP_IS_READY:
			priv->is_ready = g_value_get_boolean(value);
			break;
		case PROP_PATH:
			g_free(priv->path);
			priv->path = g_strdup(g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_class_init(PurpleSmileyClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);
	PurpleSmileyClass *ps_class = PURPLE_SMILEY_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleSmileyPrivate));

	gobj_class->get_property = purple_smiley_get_property;
	gobj_class->set_property = purple_smiley_set_property;
	gobj_class->finalize = purple_smiley_finalize;

	ps_class->get_image = purple_smiley_get_image_impl;

	properties[PROP_SHORTCUT] = g_param_spec_string("shortcut", "Shortcut",
		"A non-escaped textual representation of a smiley.", NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_IS_READY] = g_param_spec_boolean("is-ready", "Is ready",
		"Determines, if a smiley is ready to be displayed.", TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PATH] = g_param_spec_string("path", "Path",
		"The full path to the smiley image file.", NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);

	/**
	 * PurpleSmiley::ready:
	 * @smiley: a smiley that became ready.
	 *
	 * Called when a @smiley becames ready to be displayed. It's guaranteed to be
	 * fired at most once for a particular @smiley.
	 */
	signals[SIG_READY] = g_signal_new("ready", G_OBJECT_CLASS_TYPE(klass),
		0, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GType
purple_smiley_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyClass),
			.class_init = (GClassInitFunc)purple_smiley_class_init,
			.instance_size = sizeof(PurpleSmiley),
			.instance_init = purple_smiley_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmiley", &info, 0);
	}

	return type;
}
