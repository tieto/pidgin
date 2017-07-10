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

	GBytes *contents;

	const gchar *extension;
	const gchar *mime;
	gchar *gen_filename;
	gchar *friendly_filename;
} PurpleImagePrivate;

enum {
	PROP_0,
	PROP_PATH,
	PROP_CONTENTS,
	PROP_SIZE,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * Helpers
 ******************************************************************************/
static void
_purple_image_set_path(PurpleImage *image, const gchar *path) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_free(priv->path);

	priv->path = g_strdup(path);
}

static void
_purple_image_set_contents(PurpleImage *image, GBytes *bytes) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->contents)
		g_bytes_unref(priv->contents);

	priv->contents = (bytes) ? g_bytes_ref(bytes) : NULL;
}

/******************************************************************************
 * Object stuff
 ******************************************************************************/
G_DEFINE_TYPE_WITH_PRIVATE(PurpleImage, purple_image, G_TYPE_OBJECT);

static void
purple_image_init(PurpleImage *image) {
}

static void
purple_image_finalize(GObject *obj) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(obj);

	if(priv->contents)
		g_bytes_unref(priv->contents);

	g_free(priv->path);
	g_free(priv->gen_filename);
	g_free(priv->friendly_filename);

	G_OBJECT_CLASS(purple_image_parent_class)->finalize(obj);
}

static void
purple_image_set_property(GObject *obj, guint param_id,
                          const GValue *value, GParamSpec *pspec)
{
	PurpleImage *image = PURPLE_IMAGE(obj);

	switch (param_id) {
		case PROP_PATH:
			_purple_image_set_path(image, g_value_get_string(value));
			break;
		case PROP_CONTENTS:
			_purple_image_set_contents(image, g_value_get_boxed(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_image_get_property(GObject *obj, guint param_id, GValue *value,
                          GParamSpec *pspec)
{
	PurpleImage *image = PURPLE_IMAGE(obj);

	switch (param_id) {
		case PROP_PATH:
			g_value_set_string(value, purple_image_get_path(image));
			break;
		case PROP_CONTENTS:
			g_value_set_boxed(value, purple_image_get_contents(image));
			break;
		case PROP_SIZE:
			g_value_set_uint64(value, purple_image_get_data_size(image));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_image_class_init(PurpleImageClass *klass) {
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	gobj_class->finalize = purple_image_finalize;
	gobj_class->get_property = purple_image_get_property;
	gobj_class->set_property = purple_image_set_property;

	properties[PROP_PATH] = g_param_spec_string(
		"path",
		"path",
		"The filepath for the image if one was provided",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_CONTENTS] = g_param_spec_boxed(
		"contents",
		"contents",
		"The contents of the image stored in a GBytes",
		G_TYPE_BYTES,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_SIZE] = g_param_spec_uint64(
		"size",
		"size",
		"The size of the image in bytes",
		0,
		G_MAXUINT64,
		0,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
}

/******************************************************************************
 * API
 ******************************************************************************/
PurpleImage *
purple_image_new_from_bytes(GBytes *bytes) {
	return g_object_new(
		PURPLE_TYPE_IMAGE,
		"contents", bytes,
		NULL
	);
}

PurpleImage *
purple_image_new_from_file(const gchar *path, GError **error) {
	PurpleImage *image = NULL;
	GBytes *bytes = NULL;
	gchar *contents = NULL;
	gsize length = 0;

	if(!g_file_get_contents(path, &contents, &length, error)) {
		return NULL;
	}

	bytes = g_bytes_new_take(contents, length);

	image = g_object_new(
		PURPLE_TYPE_IMAGE,
		"contents", bytes,
		"path", path,
		NULL
	);

	g_bytes_unref(bytes);

	return image;
}

PurpleImage *
purple_image_new_from_data(const guint8 *data, gsize length) {
	PurpleImage *image;
	GBytes *bytes = NULL;

	bytes = g_bytes_new(data, length);

	image = purple_image_new_from_bytes(bytes);

	g_bytes_unref(bytes);

	return image;
}

PurpleImage *
purple_image_new_take_data(guint8 *data, gsize length) {
	PurpleImage *image;
	GBytes *bytes = NULL;

	bytes = g_bytes_new_take(data, length);

	image = purple_image_new_from_bytes(bytes);

	g_bytes_unref(bytes);

	return image;
}

gboolean
purple_image_save(PurpleImage *image, const gchar *path) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gconstpointer data;
	gsize len;
	gboolean succ;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(path != NULL, FALSE);
	g_return_val_if_fail(path[0] != '\0', FALSE);

	data = purple_image_get_data(image);
	len = purple_image_get_data_size(image);

	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(len > 0, FALSE);

	succ = purple_util_write_data_to_file_absolute(path, data, len);
	if (succ && priv->path == NULL)
		priv->path = g_strdup(path);

	return succ;
}

GBytes *
purple_image_get_contents(const PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->contents)
		return g_bytes_ref(priv->contents);

	return NULL;
}

const gchar *
purple_image_get_path(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->path ? priv->path : purple_image_generate_filename(image);
}

gsize
purple_image_get_data_size(PurpleImage *image) {
	PurpleImagePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), 0);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->contents)
		return g_bytes_get_size(priv->contents);

	return 0;
}

gconstpointer
purple_image_get_data(PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->contents)
		return g_bytes_get_data(priv->contents, NULL);

	return NULL;
}

const gchar *
purple_image_get_extension(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gconstpointer data;

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->extension)
		return priv->extension;

	if (purple_image_get_data_size(image) < 4)
		return NULL;

	data = purple_image_get_data(image);
	g_assert(data != NULL);

	if (memcmp(data, "GIF8", 4) == 0)
		return priv->extension = "gif";
	if (memcmp(data, "\xff\xd8\xff", 3) == 0) /* 4th may be e0 through ef */
		return priv->extension = "jpg";
	if (memcmp(data, "\x89PNG", 4) == 0)
		return priv->extension = "png";
	if (memcmp(data, "MM", 2) == 0)
		return priv->extension = "tif";
	if (memcmp(data, "II", 2) == 0)
		return priv->extension = "tif";
	if (memcmp(data, "BM", 2) == 0)
		return priv->extension = "bmp";
	if (memcmp(data, "\x00\x00\x01\x00", 4) == 0)
		return priv->extension = "ico";

	return NULL;
}

const gchar *
purple_image_get_mimetype(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	const gchar *ext = purple_image_get_extension(image);

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->mime)
		return priv->mime;

	g_return_val_if_fail(ext != NULL, NULL);

	if (g_strcmp0(ext, "gif") == 0)
		return priv->mime = "image/gif";
	if (g_strcmp0(ext, "jpg") == 0)
		return priv->mime = "image/jpeg";
	if (g_strcmp0(ext, "png") == 0)
		return priv->mime = "image/png";
	if (g_strcmp0(ext, "tif") == 0)
		return priv->mime = "image/tiff";
	if (g_strcmp0(ext, "bmp") == 0)
		return priv->mime = "image/bmp";
	if (g_strcmp0(ext, "ico") == 0)
		return priv->mime = "image/vnd.microsoft.icon";

	return NULL;
}

const gchar *
purple_image_generate_filename(PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;
	gconstpointer data;
	gsize len;
	const gchar *ext = NULL;
	gchar *checksum;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if (priv->gen_filename)
		return priv->gen_filename;

	/* grab the image's data and size of that data */
	data = purple_image_get_data(image);
	len = purple_image_get_data_size(image);

	/* create a checksum of it and use it as the start of our filename */
	checksum = g_compute_checksum_for_data(G_CHECKSUM_SHA1, data, len);

	/* if the image has a known format, set the extension appropriately */
	ext = purple_image_get_extension(image);
	if(ext != NULL) {
		priv->gen_filename = g_strdup_printf("%s.%s", checksum, ext);
		g_free(checksum);
	} else {
		priv->gen_filename = checksum;
	}

	return priv->gen_filename;
}

void
purple_image_set_friendly_filename(PurpleImage *image, const gchar *filename) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gchar *newname;
	const gchar *escaped;

	g_return_if_fail(priv != NULL);

	newname = g_path_get_basename(filename);
	escaped = purple_escape_filename(newname);
	g_free(newname);
	newname = NULL;

	if (g_strcmp0(escaped, "") == 0 || g_strcmp0(escaped, ".") == 0 ||
		g_strcmp0(escaped, G_DIR_SEPARATOR_S) == 0 ||
		g_strcmp0(escaped, "/") == 0 || g_strcmp0(escaped, "\\") == 0)
	{
		escaped = NULL;
	}

	g_free(priv->friendly_filename);
	priv->friendly_filename = g_strdup(escaped);
}

const gchar *
purple_image_get_friendly_filename(PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->friendly_filename) {
		return priv->friendly_filename;
	}

	return purple_image_generate_filename(image);
}
 
