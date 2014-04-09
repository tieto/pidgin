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

#include "debug.h"
#include "image.h"

#define PURPLE_IMAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_IMAGE, PurpleImagePrivate))

typedef struct {
	gchar *path;
	GString *contents;

	gboolean is_ready;
	gboolean has_failed;
} PurpleImagePrivate;

enum
{
	PROP_0,
	PROP_IS_READY,
	PROP_HAS_FAILED,
	PROP_LAST
};

enum
{
	SIG_READY,
	SIG_FAILED,
	SIG_LAST
};

static guint signals[SIG_LAST];
static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * Internal logic
 ******************************************************************************/

static void
has_failed(PurpleImage *image)
{
	gboolean ready_changed;
	PurpleImagePrivate *priv;
	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_if_fail(!priv->has_failed);

	priv->has_failed = TRUE;
	ready_changed = (priv->is_ready != FALSE);
	priv->is_ready = FALSE;

	if (priv->contents) {
		g_string_free(priv->contents, TRUE);
		priv->contents = NULL;
	}

	if (ready_changed) {
		g_object_notify_by_pspec(G_OBJECT(image),
			properties[PROP_IS_READY]);
	}
	g_object_notify_by_pspec(G_OBJECT(image), properties[PROP_HAS_FAILED]);
	g_signal_emit(image, signals[SIG_FAILED], 0);
}

static void
became_ready(PurpleImage *image)
{
	PurpleImagePrivate *priv;
	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_if_fail(!priv->has_failed);
	g_return_if_fail(!priv->is_ready);

	priv->is_ready = TRUE;

	g_object_notify_by_pspec(G_OBJECT(image), properties[PROP_IS_READY]);
	g_signal_emit(image, signals[SIG_READY], 0);
}

static void
steal_contents(PurpleImagePrivate *priv, gpointer data, gsize length)
{
	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->contents == NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(length > 0);

	priv->contents = g_string_new(NULL);
	g_free(priv->contents->str);
	priv->contents->str = data;
	priv->contents->len = priv->contents->allocated_len = length;
}

static void
fill_data(PurpleImage *image)
{
	PurpleImagePrivate *priv;
	GError *error = NULL;
	gchar *contents;
	gsize length;

	priv = PURPLE_IMAGE_GET_PRIVATE(image);
	if (priv->contents)
		return;

	if (!priv->is_ready)
		return;

	g_return_if_fail(priv->path);
	g_file_get_contents(priv->path, &contents, &length, &error);
	if (error) {
		purple_debug_error("image", "failed to read '%s' image: %s",
			priv->path, error->message);
		g_error_free(error);

		has_failed(image);
		return;
	}

	steal_contents(priv, contents, length);
}


/******************************************************************************
 * API implementation
 ******************************************************************************/

PurpleImage *
purple_image_new_from_file(const gchar *path)
{
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(g_file_test(path, G_FILE_TEST_EXISTS), NULL);

	return g_object_new(PURPLE_TYPE_IMAGE,
		"path", path,
		NULL);
}

PurpleImage *
purple_image_new_from_data(gpointer data, gsize length)
{
	PurpleImage *img;
	PurpleImagePrivate *priv;

	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(length > 0, NULL);

	img = g_object_new(PURPLE_TYPE_IMAGE, NULL);
	priv = PURPLE_IMAGE_GET_PRIVATE(img);

	steal_contents(priv, data, length);

	return img;
}

PurpleImage *
purple_image_transfer_new(void)
{
	PurpleImage *img;
	PurpleImagePrivate *priv;

	img = g_object_new(PURPLE_TYPE_IMAGE, NULL);
	priv = PURPLE_IMAGE_GET_PRIVATE(img);

	priv->is_ready = FALSE;
	priv->contents = g_string_new(NULL);

	return img;
}

gsize
purple_image_get_data_size(PurpleImage *image)
{
	PurpleImagePrivate *priv;
	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_val_if_fail(priv != NULL, 0);
	g_return_val_if_fail(priv->is_ready, 0);

	fill_data(image);
	g_return_val_if_fail(priv->contents, 0);

	return priv->contents->len;
}

gpointer
purple_image_get_data(PurpleImage *image)
{
	PurpleImagePrivate *priv;
	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(priv->is_ready, NULL);

	fill_data(image);
	g_return_val_if_fail(priv->contents, NULL);

	return priv->contents->str;
}

void
purple_image_transfer_write(PurpleImage *image, const gpointer data,
	gsize length)
{
	PurpleImagePrivate *priv =
		PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!priv->has_failed);
	g_return_if_fail(!priv->is_ready);
	g_return_if_fail(priv->contents != NULL);
	g_return_if_fail(data != NULL || length == 0);

	if (length == 0)
		return;

	g_string_append_len(priv->contents, (const gchar*)data, length);
}

void
purple_image_transfer_close(PurpleImage *image)
{
	PurpleImagePrivate *priv =
		PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!priv->has_failed);
	g_return_if_fail(!priv->is_ready);
	g_return_if_fail(priv->contents != NULL);

	if (priv->contents->len == 0) {
		purple_debug_error("image", "image is empty");
		has_failed(image);
		return;
	}

	became_ready(image);
}

void
purple_image_transfer_failed(PurpleImage *image)
{
	PurpleImagePrivate *priv =
		PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!priv->has_failed);
	g_return_if_fail(!priv->is_ready);

	has_failed(image);
}

/******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_image_init(GTypeInstance *instance, gpointer klass)
{
	PurpleImage *image = PURPLE_IMAGE(instance);
	PurpleImagePrivate *priv =
		PURPLE_IMAGE_GET_PRIVATE(image);

	priv->contents = NULL;
	priv->is_ready = TRUE;
	priv->has_failed = FALSE;
}

static void
purple_image_finalize(GObject *obj)
{
	PurpleImage *image = PURPLE_IMAGE(obj);
	PurpleImagePrivate *priv =
		PURPLE_IMAGE_GET_PRIVATE(image);

	if (priv->contents)
		g_string_free(priv->contents, TRUE);
	g_free(priv->path);
}

static void
purple_image_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleImage *image = PURPLE_IMAGE(object);
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);

	switch (par_id) {
		case PROP_IS_READY:
			g_value_set_boolean(value, priv->is_ready);
			break;
		case PROP_HAS_FAILED:
			g_value_set_boolean(value, priv->has_failed);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_image_class_init(PurpleImageClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(PurpleImagePrivate));

	gobj_class->finalize = purple_image_finalize;

	gobj_class->get_property = purple_image_get_property;

	properties[PROP_IS_READY] = g_param_spec_boolean("is-ready",
		"Is ready", "The image is ready to be displayed. Image may "
		"change the state to failed in a single case: if it's backed "
		"by a file and that file fails to load",
		TRUE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_HAS_FAILED] = g_param_spec_boolean("has-failed",
		"Has hailed", "The remote host has failed to send the image",
		FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);

	/**
	 * PurpleImage::failed:
	 * @image: a image that failed to transfer.
	 *
	 * Called when a @image fails to be transferred. It's guaranteed to be
	 * fired at most once for a particular @image.
	 */
	signals[SIG_FAILED] = g_signal_new("failed", G_OBJECT_CLASS_TYPE(klass),
		0, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	/**
	 * PurpleImage::ready:
	 * @image: a image that became ready.
	 *
	 * Called when a @image becames ready to be displayed. It's guaranteed to be
	 * fired at most once for a particular @image.
	 */
	signals[SIG_READY] = g_signal_new("ready", G_OBJECT_CLASS_TYPE(klass),
		0, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GType
purple_image_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleImageClass),
			.class_init = (GClassInitFunc)purple_image_class_init,
			.instance_size = sizeof(PurpleImage),
			.instance_init = purple_image_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleImage", &info, 0);
	}

	return type;
}
