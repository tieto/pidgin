/**
 * @file imgstore.h Utility functions for reference-counted in-memory
 *       image data.
 * @ingroup core
 * @see @ref imgstore-signals
 */

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
#ifndef _PURPLE_IMGSTORE_H_
#define _PURPLE_IMGSTORE_H_

#include <glib.h>

#define PURPLE_STORED_IMAGE_PROTOCOL "purple-image:"
#define PURPLE_STOCK_IMAGE_PROTOCOL "purple-stock-image:"

/**
 * PurpleStoredImage:
 *
 * A reference-counted immutable wrapper around an image's data and its
 * filename.
 */
typedef struct _PurpleStoredImage PurpleStoredImage;

#define PURPLE_TYPE_STORED_IMAGE  (purple_imgstore_get_type())

G_BEGIN_DECLS

/**
 * purple_imgstore_get_type:
 *
 * Returns the GType for the PurpleStoredImage boxed structure.
 * TODO Boxing of PurpleStoredImage is a temporary solution to having a GType
 *      for stored images. This should rather be a GObject instead of a GBoxed.
 */
GType purple_imgstore_get_type(void);

/**
 * purple_imgstore_new:
 * @data:      Pointer to the image data, which the imgstore will take
 *                  ownership of and free as appropriate.  If you want a
 *                  copy of the data, make it before calling this function.
 * @size:      Image data's size.
 * @filename:  Filename associated with image.  This is for your
 *                  convenience.  It could be the full path to the
 *                  image or, more commonly, the filename of the image
 *                  without any directory information.  It can also be
 *                  NULL, if you don't need to keep track of a filename.
 *                  If you intend to use this filename to write the file to
 *                  disk, make sure the filename is appropriately escaped.
 *                  You may wish to use purple_escape_filename().
 *
 * Create a new PurpleStoredImage.
 *
 * The image is not added to the image store and no ID is assigned.  If you
 * need to reference the image by an ID, use purple_imgstore_new_with_id()
 * instead.
 *
 * The caller owns a reference to this image and must dereference it with
 * purple_imgstore_unref() for it to be freed.
 *
 * Returns: The image, or NULL if the image could not be created (because of
 *         empty data or size).
 */
PurpleStoredImage *
purple_imgstore_new(gpointer data, size_t size, const char *filename);

/**
 * purple_imgstore_new_from_file:
 * @path: The path to the image.
 *
 * Create a PurpleStoredImage using purple_imgstore_new() by reading the
 * given filename from disk.
 *
 * The image is not added to the image store and no ID is assigned.  If you
 * need to reference the image by an ID, use purple_imgstore_new_with_id()
 * instead.
 *
 * Make sure the filename is appropriately escaped.  You may wish to use
 * purple_escape_filename().  The PurpleStoredImage's filename will be set
 * to the given path.
 *
 * The caller owns a reference to this image and must dereference it with
 * purple_imgstore_unref() for it to be freed.
 *
 * Returns: The image, or NULL if the image could not be created (because of
 *         empty data or size).
 */
PurpleStoredImage *
purple_imgstore_new_from_file(const char *path);

/**
 * purple_imgstore_new_with_id:
 * @data:      Pointer to the image data, which the imgstore will take
 *                  ownership of and free as appropriate.  If you want a
 *                  copy of the data, make it before calling this function.
 * @size:      Image data's size.
 * @filename:  Filename associated with image.  This is for your
 *                  convenience.  It could be the full path to the
 *                  image or, more commonly, the filename of the image
 *                  without any directory information.  It can also be
 *                  NULL, if you don't need to keep track of a filename.
 *                  If you intend to use this filename to write the file to
 *                  disk, make sure the filename is appropriately escaped.
 *                  You may wish to use purple_escape_filename()
 *
 * Create a PurpleStoredImage using purple_imgstore_new() and add the
 * image to the image store.  A unique ID will be assigned to the image.
 *
 * The caller owns a reference to the image and must dereference it with
 * purple_imgstore_unref() or purple_imgstore_unref_by_id() for it to be
 * freed.
 *
 * Returns: ID for the image.  This is a unique number that can be used
 *         within libpurple to reference the image.  0 is returned if the
 *         image could not be created (because of empty data or size).
 */
int purple_imgstore_new_with_id(gpointer data, size_t size, const char *filename);

/**
 * purple_imgstore_find_by_id:
 * @id: The ID for the image.
 *
 * Retrieve an image from the store. The caller does not own a
 * reference to the image.
 *
 * Returns: A pointer to the requested image, or NULL if it was not found.
 */
PurpleStoredImage *purple_imgstore_find_by_id(int id);

/**
 * purple_imgstore_get_data:
 * @img: The Image.
 *
 * Retrieves a pointer to the image's data.
 *
 * Returns: A pointer to the data, which must not
 *         be freed or modified.
 */
gconstpointer purple_imgstore_get_data(PurpleStoredImage *img);

/**
 * purple_imgstore_get_size:
 * @img: The Image.
 *
 * Retrieves the length of the image's data.
 *
 * Returns: The size of the data that the pointer returned by
 *         purple_imgstore_get_data points to.
 */
size_t purple_imgstore_get_size(PurpleStoredImage *img);

/**
 * purple_imgstore_get_filename:
 * @img: The image.
 *
 * Retrieves a pointer to the image's filename.  If you intend to use this
 * filename to write the file to disk, make sure the filename was
 * appropriately escaped when you created the PurpleStoredImage.  You may
 * wish to use purple_escape_filename().
 *
 * Returns: A pointer to the filename, which must not
 *         be freed or modified.
 */
const char *purple_imgstore_get_filename(const PurpleStoredImage *img);

/**
 * purple_imgstore_get_extension:
 * @img: The image.
 *
 * Looks at the magic numbers of the image data (the first few bytes)
 * and returns an extension corresponding to the image's file type.
 *
 * Returns: The image's extension (for example "png") or "icon"
 *         if unknown.
 */
const char *purple_imgstore_get_extension(PurpleStoredImage *img);

/**
 * purple_imgstore_ref:
 * @img: The image.
 *
 * Increment the reference count.
 *
 * Returns: @img
 */
PurpleStoredImage *
purple_imgstore_ref(PurpleStoredImage *img);

/**
 * purple_imgstore_unref:
 * @img: The image.
 *
 * Decrement the reference count.
 *
 * If the reference count reaches zero, the image will be freed.
 *
 * Returns: @img or %NULL if the reference count reached zero.
 */
PurpleStoredImage *
purple_imgstore_unref(PurpleStoredImage *img);

/**
 * purple_imgstore_ref_by_id:
 * @id: The ID for the image.
 *
 * Increment the reference count using an ID.
 *
 * This is a convience wrapper for purple_imgstore_find_by_id() and
 * purple_imgstore_ref(), so if you have a PurpleStoredImage, it'll
 * be more efficient to call purple_imgstore_ref() directly.
 */
void purple_imgstore_ref_by_id(int id);

/**
 * purple_imgstore_unref_by_id:
 * @id: The ID for the image.
 *
 * Decrement the reference count using an ID.
 *
 * This is a convience wrapper for purple_imgstore_find_by_id() and
 * purple_imgstore_unref(), so if you have a PurpleStoredImage, it'll
 * be more efficient to call purple_imgstore_unref() directly.
 */
void purple_imgstore_unref_by_id(int id);

/**
 * purple_imgstore_get_handle:
 *
 * Returns the image store subsystem handle.
 *
 * Returns: The subsystem handle.
 */
void *purple_imgstore_get_handle(void);

/**
 * purple_imgstore_init:
 *
 * Initializes the image store subsystem.
 */
void purple_imgstore_init(void);

/**
 * purple_imgstore_uninit:
 *
 * Uninitializes the image store subsystem.
 */
void purple_imgstore_uninit(void);

G_END_DECLS

#endif /* _PURPLE_IMGSTORE_H_ */
