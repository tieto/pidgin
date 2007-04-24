/**
 * @file imgstore.h IM Image Store API
 * @ingroup core
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <glib.h>
#include "debug.h"
#include "imgstore.h"
#include "util.h"

static GHashTable *imgstore;
static int nextid = 0;

/**
 * Stored image
 *
 * NOTE: purple_imgstore_add() creates these without zeroing the memory, so
 * NOTE: make sure to update that function when adding members.
 */
struct _PurpleStoredImage
{
	int id;
	guint8 refcount;
	size_t size;		/**< The image data's size.	*/
	char *filename;		/**< The filename (for the UI)	*/
	gpointer data;		/**< The image data.		*/
};

PurpleStoredImage *
purple_imgstore_add(gconstpointer data, size_t size, const char *filename)
{
	PurpleStoredImage *img;

	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(size > 0, 0);

	img = g_new(PurpleStoredImage, 1);
	img->data = g_memdup(data, size);
	img->size = size;
	img->filename = g_strdup(filename);
	img->refcount = 1;
	img->id = 0;

	return img;
}

int
purple_imgstore_add_with_id(gconstpointer data, size_t size, const char *filename)
{
	PurpleStoredImage *img = purple_imgstore_add(data, size, filename);
	img->id = ++nextid;

	g_hash_table_insert(imgstore, GINT_TO_POINTER(img->id), img);

	return img->id;
}

PurpleStoredImage *purple_imgstore_find_by_id(int id) {
	PurpleStoredImage *img = g_hash_table_lookup(imgstore, GINT_TO_POINTER(id));

	if (img != NULL)
		purple_debug_misc("imgstore", "retrieved image id %d\n", img->id);

	return img;
}

gconstpointer purple_imgstore_get_data(PurpleStoredImage *img) {
	return img->data;
}

size_t purple_imgstore_get_size(PurpleStoredImage *img)
{
	return img->size;
}

const char *purple_imgstore_get_filename(PurpleStoredImage *img)
{
	return img->filename;
}

const char *purple_imgstore_get_extension(PurpleStoredImage *img)
{
	return purple_util_get_image_extension(img->data, img->size);
}

void purple_imgstore_ref_by_id(int id)
{
	PurpleStoredImage *img = purple_imgstore_find_by_id(id);

	g_return_if_fail(img != NULL);

	purple_imgstore_ref(img);
}

void purple_imgstore_unref_by_id(int id)
{
	PurpleStoredImage *img = purple_imgstore_find_by_id(id);

	g_return_if_fail(img != NULL);

	purple_imgstore_unref(img);
}

PurpleStoredImage *
purple_imgstore_ref(PurpleStoredImage *img)
{
	g_return_val_if_fail(img != NULL, NULL);

	img->refcount++;

	return img;
}

PurpleStoredImage *
purple_imgstore_unref(PurpleStoredImage *img)
{
	if (img == NULL)
		return NULL;

	g_return_val_if_fail(img->refcount > 0, NULL);

	img->refcount--;

	if (img->refcount == 0)
	{
		purple_signal_emit(purple_imgstore_get_handle(),
		                   "image-deleting", img);
		if (img->id)
			g_hash_table_remove(imgstore, GINT_TO_POINTER(img->id));

		g_free(img->data);
		g_free(img->filename);
		g_free(img);
	}

	return img;
}

void *
purple_imgstore_get_handle()
{
	static int handle;

	return &handle;
}

void
purple_imgstore_init()
{
	void *handle = purple_imgstore_get_handle();

	purple_signal_register(handle, "image-deleting",
	                       purple_marshal_VOID__POINTER, NULL,
	                       1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_STORED_IMAGE));

	imgstore = g_hash_table_new(g_int_hash, g_int_equal);
}

void
purple_imgstore_uninit()
{
	g_hash_table_destroy(imgstore);

	purple_signals_unregister_by_instance(purple_blist_get_handle());
}
