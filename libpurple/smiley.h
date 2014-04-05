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
 * @section_id: libpurple-smiley
 * @short_description: <filename>smiley.h</filename>
 * @title: Smiley API
 */

#include "imgstore.h"

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
 * A generic smiley.
 *
 * This contains common part of things Purple will need to know about a smiley.
 */
struct _PurpleSmiley
{
	/*< private >*/
	GObject parent;
};

struct _PurpleSmileyClass
{
	/*< private >*/
	GObjectClass parent_class;

	PurpleStoredImage * (*get_image)(PurpleSmiley *smiley);

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_smiley_get_type:
 *
 * Returns: The #GType for a smiley.
 */
GType
purple_smiley_get_type(void);

/**************************************************************************/
/* Smiley API                                                             */
/**************************************************************************/

/**
 * purple_smiley_new:
 * @shortcut: The smiley shortcut.
 * @path: The image file path.
 *
 * Creates new shortcut, which is ready to display.
 *
 * Returns: The shortcut.
 */
PurpleSmiley *
purple_smiley_new(const gchar *shortcut, const gchar *path);

/**
 * purple_smiley_get_shortcut:
 * @smiley: The smiley.
 *
 * Returns the smiley's associated shortcut (e.g. "(homer)" or ":-)").
 *
 * Returns: The shortcut.
 */
const gchar *
purple_smiley_get_shortcut(const PurpleSmiley *smiley);

gboolean
purple_smiley_is_ready(const PurpleSmiley *smiley);

/**
 * purple_smiley_get_path:
 * @smiley:  The custom smiley.
 *
 * Returns a full path to a smiley file.
 *
 * A smiley may not be saved to disk (the path will be NULL), but could still be
 * accessible using purple_smiley_get_data.
 *
 * Returns: A full path to the file, or %NULL under various conditions.
 *          The caller should use g_free to free the returned string.
 */
const gchar *
purple_smiley_get_path(PurpleSmiley *smiley);

PurpleStoredImage *
purple_smiley_get_image(PurpleSmiley *smiley);

G_END_DECLS

#endif /* _PURPLE_SMILEY_H_ */
