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

#include "smiley.h"
#include "dbus-maybe.h"

#define PURPLE_SMILEY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY, PurpleSmileyPrivate))

typedef struct {
	gchar *shortcut;
} PurpleSmileyPrivate;

enum
{
	PROP_0,
	PROP_SHORTCUT,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

/*******************************************************************************
 * Helpers
 ******************************************************************************/
static void
_purple_smiley_set_shortcut(PurpleSmiley *smiley, const gchar *shortcut) {
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	g_free(priv->shortcut);

	priv->shortcut = g_strdup(shortcut);

	g_object_notify(G_OBJECT(smiley), "shortcut");
}

/*******************************************************************************
 * Object stuff
 ******************************************************************************/
G_DEFINE_TYPE_WITH_PRIVATE(PurpleSmiley, purple_smiley, PURPLE_TYPE_IMAGE);

static void
purple_smiley_init(PurpleSmiley *smiley) {
	PURPLE_DBUS_REGISTER_POINTER(smiley, PurpleSmiley);
}

static void
purple_smiley_finalize(GObject *obj) {
	PurpleSmileyPrivate *priv = PURPLE_SMILEY_GET_PRIVATE(obj);

	g_free(priv->shortcut);

	PURPLE_DBUS_UNREGISTER_POINTER(smiley);

	G_OBJECT_CLASS(purple_smiley_parent_class)->finalize(obj);
}

static void
purple_smiley_get_property(GObject *obj, guint param_id, GValue *value,
                           GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(obj);

	switch (param_id) {
		case PROP_SHORTCUT:
			g_value_set_string(value, purple_smiley_get_shortcut(smiley));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_smiley_set_property(GObject *obj, guint param_id, const GValue *value,
                           GParamSpec *pspec)
{
	PurpleSmiley *smiley = PURPLE_SMILEY(obj);

	switch (param_id) {
		case PROP_SHORTCUT:
			_purple_smiley_set_shortcut(smiley, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_smiley_class_init(PurpleSmileyClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_smiley_get_property;
	obj_class->set_property = purple_smiley_set_property;
	obj_class->finalize = purple_smiley_finalize;

	properties[PROP_SHORTCUT] = g_param_spec_string(
		"shortcut",
		"Shortcut",
		"A non-escaped textual representation of a smiley.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/*******************************************************************************
 * API
 ******************************************************************************/
PurpleSmiley *
purple_smiley_new(const gchar *shortcut, const gchar *path)
{
	PurpleSmiley *smiley = NULL;
	GBytes *bytes = NULL;
	gchar *contents = NULL;
	gsize length = 0;

	g_return_val_if_fail(shortcut != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	if(!g_file_get_contents(path, &contents, &length, NULL)) {
		return NULL;
	}

	bytes = g_bytes_new_take(contents, length);

	smiley = g_object_new(
		PURPLE_TYPE_SMILEY,
		"path", path,
		"contents", bytes,
		"shortcut", shortcut,
		NULL
	);

	g_bytes_unref(bytes);

	return smiley;
}

PurpleSmiley *
purple_smiley_new_from_data(const gchar *shortcut,
                            const guint8 *data,
                            gsize length)
{
	PurpleSmiley *smiley = NULL;
	GBytes *bytes = NULL;

	g_return_val_if_fail(shortcut != NULL, NULL);

	bytes = g_bytes_new(data, length);

	smiley = g_object_new(
		PURPLE_TYPE_SMILEY,
		"shortcut", shortcut,
		"contents", bytes,
		NULL
	);

	g_bytes_unref(bytes);

	return smiley;

}

PurpleSmiley *
purple_smiley_new_remote(const gchar *shortcut) {
	g_return_val_if_fail(shortcut != NULL, NULL);

	return g_object_new(
		PURPLE_TYPE_SMILEY,
		"shortcut", shortcut,
		NULL
	);
}

const gchar *
purple_smiley_get_shortcut(const PurpleSmiley *smiley) {
	PurpleSmileyPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_SMILEY(smiley), NULL);

	priv = PURPLE_SMILEY_GET_PRIVATE(smiley);

	return priv->shortcut;
}
