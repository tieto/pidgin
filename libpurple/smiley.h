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

#ifndef PURPLE_SMILEY_H
#define PURPLE_SMILEY_H

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
 * remote data.
 *
 * The #PurpleSmiley:shortcut is always unescaped, but <link linkend="libpurple-smiley-parser">smiley parser</link>
 * may deal with special characters.
 */

#include "image.h"

#include <glib-object.h>

typedef struct _PurpleSmiley PurpleSmiley;
typedef struct _PurpleSmileyClass PurpleSmileyClass;

#define PURPLE_TYPE_SMILEY            (purple_smiley_get_type())
#define PURPLE_SMILEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_SMILEY, PurpleSmiley))
#define PURPLE_SMILEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SMILEY, PurpleSmileyClass))
#define PURPLE_IS_SMILEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_SMILEY))
#define PURPLE_IS_SMILEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_SMILEY))
#define PURPLE_SMILEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SMILEY, PurpleSmileyClass))

/**
 * PurpleSmiley:
 *
 * A generic smiley. It can either be a theme smiley, or a custom smiley.
 */
struct _PurpleSmiley {
	/*< private >*/
	PurpleImage parent;
};

/**
 * PurpleSmileyClass:
 *
 * Base class for #PurpleSmiley objects.
 */
struct _PurpleSmileyClass {
	/*< private >*/
	PurpleImageClass parent_class;

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
GType purple_smiley_get_type(void);

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
PurpleSmiley *purple_smiley_new(const gchar *shortcut, const gchar *path);

/**
 * purple_smiley_new_from_data:
 * @shortcut: The smiley shortcut (unescaped).
 * @data: The raw data of the image.
 * @length: The length of @data in bytes.
 *
 * Creates new smiley from @data.
 *
 * Returns: A new #PurpleSmiley.
 */
PurpleSmiley *purple_smiley_new_from_data(const gchar *shortcut, const guint8 *data, gsize length);

/**
 * purple_smiley_new_remote:
 * @shortcut: the smiley shortcut (unescaped).
 *
 * Creates new remote smiley. It's not bound to any conversation, so most
 * probably you might want to use
 * #purple_conversation_add_remote_smiley instead.
 *
 * Returns: the new remote #PurpleSmiley.
 */
PurpleSmiley *purple_smiley_new_remote(const gchar *shortcut);

/**
 * purple_smiley_get_shortcut:
 * @smiley: the smiley.
 *
 * Returns the @smiley's associated shortcut (e.g. <literal>(homer)</literal> or
 * <literal>:-)</literal>).
 *
 * Returns: the unescaped shortcut.
 */
const gchar *purple_smiley_get_shortcut(const PurpleSmiley *smiley);

G_END_DECLS

#endif /* PURPLE_SMILEY_H */
