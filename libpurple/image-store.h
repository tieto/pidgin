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

#ifndef _PURPLE_IMAGE_STORE_H_
#define _PURPLE_IMAGE_STORE_H_
/**
 * SECTION:image-store
 * @include:image-store.h
 * @section_id: libpurple-image-store
 * @short_description: a global, temporary storage for images
 * @title: Image store
 *
 * Image store references #PurpleImage's by a simple integer value.
 * It's a convenient way to embed images in HTML-like strings.
 *
 * Integer ID's are tracked for being valid - when a image is destroyed, it's
 * also removed from the store. If the application runs for a very long time and
 * all possible id numbers from integer range are utilized, it will use
 * previously released numbers.
 */

#include "image.h"

/**
 * PURPLE_IMAGE_STORE_PROTOCOL:
 *
 * A global URI prefix for images stored in this subsystem.
 */
#define PURPLE_IMAGE_STORE_PROTOCOL "purple-image:"

/**
 * PURPLE_IMAGE_STORE_STOCK_PROTOCOL:
 *
 * A global URI prefix for stock images, with names defined by libpurple and
 * contents defined by the UI.
 */
#define PURPLE_IMAGE_STORE_STOCK_PROTOCOL "purple-stock-image:"

G_BEGIN_DECLS

/**
 * purple_image_store_add:
 * @image: the image.
 *
 * Permanently adds an image to the store. If the @image is already in the
 * store, it will return its current id.
 *
 * This function increases @image's reference count, so it won't be destroyed
 * until image store subsystem is shut down. Don't decrease @image's reference
 * count by yourself - if you don't want the store to hold the reference,
 * use #purple_image_store_add_weak.
 *
 * Returns: the unique identifier for the @image.
 */
guint
purple_image_store_add(PurpleImage *image);

/**
 * purple_image_store_add_weak:
 * @image: the image.
 *
 * Adds an image to the store without increasing reference count. It will be
 * removed from the store when @image gets destroyed.
 *
 * If the @image is already in the store, it will return its current id.
 *
 * Returns: the unique identifier for the @image.
 */
guint
purple_image_store_add_weak(PurpleImage *image);

/**
 * purple_image_store_add_temporary:
 * @image: the image.
 *
 * Adds an image to the store to be used in a short period of time.
 * If the @image is already in the store, it will just return its current id.
 *
 * Increases reference count for the @image for a time long enough to display
 * the @image by the UI. In current implementation it's five seconds, but may be
 * changed in the future, so if you need more sophisticated reference
 * management, implement it on your own.
 *
 * Returns: the unique identifier for the @image.
 */
guint
purple_image_store_add_temporary(PurpleImage *image);

/**
 * purple_image_store_get:
 * @id: the unique identifier of an image.
 *
 * Finds the image with a certain identifier within a store.
 *
 * Returns: (transfer none): the image referenced by @id, or %NULL if it
 *          doesn't exists.
 */
PurpleImage *
purple_image_store_get(guint id);

/**
 * purple_image_store_get_from_uri:
 * @uri: the URI of a potential #PurpleImage. Should not be %NULL.
 *
 * Checks, if the @uri is pointing to any #PurpleImage by referring
 * to #PURPLE_IMAGE_STORE_PROTOCOL and returns the image, if it's valid.
 *
 * The function doesn't throw any warning, if the @uri is for any
 * other protocol.
 *
 * Returns: (transfer none): the image referenced by @uri, or %NULL if it
 *          doesn't point to any valid image.
 */
PurpleImage *
purple_image_store_get_from_uri(const gchar *uri);

/**
 * purple_image_store_get_uri:
 * @image: the image.
 *
 * Generates an URI for the @image, to be retrieved using
 * #purple_image_store_get_from_uri.
 *
 * Returns: (transfer full): the URI for the @image. Should be #g_free'd when
 * you done using it.
 */
gchar *
purple_image_store_get_uri(PurpleImage *image);

/**
 * _purple_image_store_init: (skip)
 *
 * Initializes the image store subsystem.
 */
void
_purple_image_store_init(void);

/**
 * _purple_image_store_uninit: (skip)
 *
 * Uninitializes the image store subsystem.
 */
void
_purple_image_store_uninit(void);

G_END_DECLS

#endif /* _PURPLE_IMAGE_STORE_H_ */
