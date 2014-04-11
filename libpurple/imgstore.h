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

/* XXX: this file is in progress of deleting */

#include "image.h"
#include "image-store.h"

#define PURPLE_STORED_IMAGE_PROTOCOL PURPLE_IMAGE_STORE_PROTOCOL
#define PURPLE_STOCK_IMAGE_PROTOCOL PURPLE_IMAGE_STORE_STOCK_PROTOCOL

typedef struct _PurpleImage PurpleStoredImage;

#define PURPLE_TYPE_STORED_IMAGE PURPLE_TYPE_IMAGE

#define PURPLE_IS_STORED_IMAGE(image) PURPLE_IS_IMAGE(image)

PurpleStoredImage *
purple_imgstore_new(gpointer data, size_t size, const char *filename);

PurpleStoredImage *
purple_imgstore_new_from_file(const char *path);

int purple_imgstore_new_with_id(gpointer data, size_t size, const char *filename);

int
purple_imgstore_add_with_id(PurpleStoredImage *image);

PurpleStoredImage *purple_imgstore_find_by_id(int id);

gconstpointer purple_imgstore_get_data(PurpleStoredImage *img);

size_t purple_imgstore_get_size(PurpleStoredImage *img);

const char *purple_imgstore_get_filename(const PurpleStoredImage *img);

const char *purple_imgstore_get_extension(PurpleStoredImage *img);

PurpleStoredImage *
purple_imgstore_ref(PurpleStoredImage *img);

void
purple_imgstore_unref(PurpleStoredImage *img);

void purple_imgstore_ref_by_id(int id);

void purple_imgstore_unref_by_id(int id);

void *purple_imgstore_get_handle(void);

#endif /* _PURPLE_IMGSTORE_H_ */
