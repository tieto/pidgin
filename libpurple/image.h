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

#ifndef _PURPLE_IMAGE_H_
#define _PURPLE_IMAGE_H_
/**
 * SECTION:image
 * @include:image.h
 * @section_id: libpurple-image
 * @short_description: implementation-independent image container.
 * @title: Images
 *
 * TODO desc.
 */

#include <glib-object.h>

typedef struct _PurpleImage PurpleImage;
typedef struct _PurpleImageClass PurpleImageClass;

#define PURPLE_TYPE_IMAGE            (purple_image_get_type())
#define PURPLE_IMAGE(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_IMAGE, PurpleImage))
#define PURPLE_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_IMAGE, PurpleImageClass))
#define PURPLE_IS_IMAGE(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_IMAGE))
#define PURPLE_IS_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_IMAGE))
#define PURPLE_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_IMAGE, PurpleImageClass))

/**
 * PurpleImage:
 *
 * TODO desc
 */
struct _PurpleImage
{
	/*< private >*/
	GObject parent;
};

/**
 * PurpleImageClass:
 *
 * Base class for #PurpleImage objects.
 */
struct _PurpleImageClass
{
	/*< private >*/
	GObjectClass parent_class;

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_image_get_type:
 *
 * Returns: the #GType for an image.
 */
GType
purple_image_get_type(void);

PurpleImage *
purple_image_new_from_file(const gchar *path, gboolean be_eager);

PurpleImage *
purple_image_new_from_data(gpointer data, gsize length);

const gchar *
purple_image_get_path(PurpleImage *image);

gboolean
purple_image_is_ready(PurpleImage *image);

gboolean
purple_image_has_failed(PurpleImage *image);

gsize
purple_image_get_data_size(PurpleImage *image);

gpointer
purple_image_get_data(PurpleImage *image);

const gchar *
purple_image_get_extenstion(PurpleImage *image);

const gchar *
purple_image_get_mimetype(PurpleImage *image);

PurpleImage *
purple_image_transfer_new(void);

void
purple_image_transfer_write(PurpleImage *image, const gpointer data,
	gsize length);

void
purple_image_transfer_close(PurpleImage *image);

void
purple_image_transfer_failed(PurpleImage *image);

G_END_DECLS

#endif /* _PURPLE_IMAGE_H_ */
