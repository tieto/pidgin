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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "image-store.h"

#include "eventloop.h"
#include "glibcompat.h"
#include "util.h"

#define TEMP_IMAGE_TIMEOUT 5

static GHashTable *id_to_image = NULL;
static guint last_id = 0;

/* keys: timeout handle */
static GHashTable *temp_images = NULL;

/* keys: img id */
static GSList *perm_images = NULL;

static void
image_reset_id(gpointer _id)
{
	g_return_if_fail(id_to_image != NULL);

	g_hash_table_remove(id_to_image, _id);
}

static guint
image_set_id(PurpleImage *image)
{
	/* Use the next unused id number. We do it in a loop on the off chance
	 * that next id wraps back around to 0 and the hash table still contains
	 * entries from the first time around.
	 */
	while (TRUE) {
		last_id++;

		if (G_UNLIKELY(last_id == 0))
			continue;

		if (purple_image_store_get(last_id) == NULL)
			break;
	}

	g_object_set_data_full(G_OBJECT(image), "purple-image-store-id",
		GINT_TO_POINTER(last_id), image_reset_id);
	g_hash_table_insert(id_to_image, GINT_TO_POINTER(last_id), image);
	return last_id;
}

static guint
image_get_id(PurpleImage *image)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(image),
		"purple-image-store-id"));
}

guint
purple_image_store_add(PurpleImage *image)
{
	guint id;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), 0);

	id = image_get_id(image);
	if (id > 0)
		return id;

	id = image_set_id(image);

	g_object_ref(image);
	perm_images = g_slist_prepend(perm_images, image);

	return id;
}

guint
purple_image_store_add_weak(PurpleImage *image)
{
	guint id;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), 0);

	id = image_get_id(image);
	if (id > 0)
		return id;

	return image_set_id(image);
}

static gboolean
remove_temporary(gpointer _image)
{
	PurpleImage *image = _image;
	guint handle;

	handle = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(image),
		"purple-image-store-handle"));

	g_hash_table_remove(temp_images, GINT_TO_POINTER(handle));

	return G_SOURCE_REMOVE;
}

static void
cancel_temporary(gpointer key, gpointer value, gpointer _unused)
{
	purple_timeout_remove(GPOINTER_TO_INT(key));
}

guint
purple_image_store_add_temporary(PurpleImage *image)
{
	guint id;
	guint handle;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), 0);

	id = image_get_id(image);
	/* XXX: add_temporary doesn't extend previous temporary call, sorry */
	if (id > 0)
		return id;

	id = image_set_id(image);

	g_object_ref(image);
	handle = purple_timeout_add_seconds(TEMP_IMAGE_TIMEOUT,
		remove_temporary, image);
	g_object_set_data(G_OBJECT(image), "purple-image-store-handle",
		GINT_TO_POINTER(handle));
	g_hash_table_insert(temp_images, GINT_TO_POINTER(handle), image);

	return id;
}

PurpleImage *
purple_image_store_get(guint id)
{
	return g_hash_table_lookup(id_to_image, GINT_TO_POINTER(id));
}

/* TODO: handle PURPLE_IMAGE_STORE_STOCK_PROTOCOL */
PurpleImage *
purple_image_store_get_from_uri(const gchar *uri)
{
	guint64 longid;
	guint id;
	gchar *endptr;
	gchar endchar;

	g_return_val_if_fail(uri != NULL, NULL);

	if (!purple_str_has_prefix(uri, PURPLE_IMAGE_STORE_PROTOCOL))
		return NULL;

	uri += sizeof(PURPLE_IMAGE_STORE_PROTOCOL) - 1;
	if (uri[0] == '-')
		return NULL;

	longid = g_ascii_strtoull(uri, &endptr, 10);
	endchar = endptr[0];
	if (endchar != '\0' && endchar != '"' &&
		endchar != '\'' && endchar != ' ')
	{
		return NULL;
	}

	id = longid;
	if (id != longid)
		return NULL;

	return purple_image_store_get(id);
}

void
_purple_image_store_init(void)
{
	id_to_image = g_hash_table_new(g_direct_hash, g_direct_equal);
	temp_images = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, g_object_unref);
}

void
_purple_image_store_uninit(void)
{
	g_slist_free_full(perm_images, g_object_unref);
	perm_images = NULL;

	g_hash_table_foreach(temp_images, cancel_temporary, NULL);
	g_hash_table_destroy(temp_images);
	temp_images = NULL;

	g_hash_table_destroy(id_to_image);
	id_to_image = NULL;
}
