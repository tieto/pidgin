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
 *
*/

#include "internal.h"

#include "dbus-maybe.h"
#include "debug.h"
#include "imgstore.h"
#include "util.h"

PurpleStoredImage *
purple_imgstore_new(gpointer data, size_t size, const char *filename)
{
	return purple_image_new_from_data(data, size);
}

PurpleStoredImage *
purple_imgstore_new_from_file(const char *path)
{
	return purple_image_new_from_file(path, TRUE);
}

int
purple_imgstore_new_with_id(gpointer data, size_t size, const char *filename)
{
	PurpleImage *image = purple_image_new_from_data(data, size);

	return purple_image_store_add(image);
}

int
purple_imgstore_add_with_id(PurpleStoredImage *image)
{
	return purple_image_store_add(image);
}

PurpleStoredImage *purple_imgstore_find_by_id(int id)
{
	return purple_image_store_get(id);
}

gconstpointer purple_imgstore_get_data(PurpleStoredImage *img)
{
	return purple_image_get_data(img);
}

size_t purple_imgstore_get_size(PurpleStoredImage *img)
{
	return purple_image_get_size(img);
}

const char *purple_imgstore_get_filename(const PurpleStoredImage *img)
{
	return purple_image_get_path((PurpleImage *)img);
}

const char *purple_imgstore_get_extension(PurpleStoredImage *img)
{
	return purple_image_get_extension(img);
}

void purple_imgstore_ref_by_id(int id)
{
	PurpleImage *img = purple_image_store_get(id);

	g_return_if_fail(img != NULL);

	g_object_ref(img);
}

void purple_imgstore_unref_by_id(int id)
{
	PurpleImage *img = purple_image_store_get(id);

	g_return_if_fail(img != NULL);

	g_object_unref(img);
}

PurpleStoredImage *
purple_imgstore_ref(PurpleStoredImage *img)
{
	g_object_ref(img);

	return img;
}

void
purple_imgstore_unref(PurpleStoredImage *img)
{
	if (img == NULL) {
		purple_debug_warning("imgstore",
			"purple_imgstore_unref: img empty");
		return;
	}
	g_object_unref(img);
}

void *
purple_imgstore_get_handle()
{
	static int handle;

	return &handle;
}
