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

#ifndef _PURPLE_SMILEY_H_
#define _PURPLE_SMILEY_H_
/**
 * SECTION:smiley
 * @include:smiley.h
 * @section_id: libpurple-smiley
 * @short_description: a link between emoticon image and its textual representation
 * @title: Smileys
 *
 * A #PurpleSmiley is a base class for associating emoticon images and their
 * textual representation. It's intended for various smiley-related tasks:
 * parsing the text against them, displaying in the smiley selector, or handling
 * remote data (using #PurpleRemoteSmiley).
 *
 * The #PurpleSmiley:shortcut is always unescaped, but <link linkend="libpurple-smiley-parser">smiley parser</link>
 * may deal with special characters.
 */

#include "image.h"

#include <glib-object.h>

typedef struct _PurpleSmiley PurpleSmiley;
typedef struct _PurpleSmileyClass PurpleSmileyClass;

#define PURPLE_TYPE_SMILEY            (purple_smiley_get_type())
#define PURPLE_SMILEY(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_SMILEY, PurpleSmiley))
#define PURPLE_SMILEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SMILEY, PurpleSmileyClass))
#define PURPLE_IS_SMILEY(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_SMILEY))
#define PURPLE_IS_SMILEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_SMILEY))
#define PURPLE_SMILEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SMILEY, PurpleSmileyClass))

/**
 * PurpleSmiley:
 *
 * A generic smiley. It can either be a theme smiley, or a custom smiley.
 */
struct _PurpleSmiley
{
	/*< private >*/
	GObject parent;
};

/**
 * PurpleSmileyClass:
 * @get_image: gets image contents for a @smiley. May not require
 *             #PurpleSmiley:path being set. See #purple_smiley_get_image.
 *
 * Base class for #PurpleSmiley objects.
 */
struct _PurpleSmileyClass
{
	/*< private >*/
	GObjectClass parent_class;

	/*< public >*/
	PurpleImage * (*get_image)(PurpleSmiley *smiley);

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_smiley_get_type:
 *
 * Returns: the #GType for a smiley.
 */
GType
purple_smiley_get_type(void);

/**
 * purple_smiley_new:
 * @shortcut: the smiley shortcut (unescaped).
 * @path: the smiley image file path.
 *
 * Creates new smiley, which is ready to display (its file exists
 * and is a valid image).
 *
 * Returns: the new #PurpleSmiley.
 */
PurpleSmiley *
purple_smiley_new(const gchar *shortcut, const gchar *path);

/**
 * Consider using #purple_conversation_add_remote_smiley.
 */
PurpleSmiley *
purple_smiley_new_remote(const gchar *shortcut);

/**
 * purple_smiley_get_shortcut:
 * @smiley: the smiley.
 *
 * Returns the @smiley's associated shortcut (e.g. <literal>(homer)</literal> or
 * <literal>:-)</literal>).
 *
 * Returns: the unescaped shortcut.
 */
const gchar *
purple_smiley_get_shortcut(const PurpleSmiley *smiley);

/**
 * purple_smiley_get_image:
 * @smiley: the smiley.
 *
 * Returns (and possibly loads) the image contents for a @smiley.
 * If you want to save it, increase a ref count for the returned object.
 *
 * Returns: (transfer none): the image contents for a @smiley.
 */
PurpleImage *
purple_smiley_get_image(PurpleSmiley *smiley);

G_END_DECLS

#endif /* _PURPLE_SMILEY_H_ */
