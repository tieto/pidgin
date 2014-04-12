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
 * @short_description: todo
 * @title: Image store
 *
 * TODO desc.
 */

#include "image.h"

#define PURPLE_IMAGE_STORE_PROTOCOL "purple-image:"
#define PURPLE_IMAGE_STORE_STOCK_PROTOCOL "purple-stock-image:"

G_BEGIN_DECLS

/* increases and maintains reference count */
guint
purple_image_store_add(PurpleImage *image);

/* doesn't change reference count and is removed from the store when obj is destroyed */
guint
purple_image_store_add_weak(PurpleImage *image);

/* increases reference count for five seconds */
guint
purple_image_store_add_temporary(PurpleImage *image);

PurpleImage *
purple_image_store_get(guint id);

PurpleImage *
purple_image_store_get_from_uri(const gchar *uri);

gchar *
purple_image_store_get_uri(PurpleImage *image);

void
_purple_image_store_init(void);

void
_purple_image_store_uninit(void);

G_END_DECLS

#endif /* _PURPLE_IMAGE_STORE_H_ */
