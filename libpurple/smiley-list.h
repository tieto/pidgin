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

#ifndef _PURPLE_SMILEY_LIST_H_
#define _PURPLE_SMILEY_LIST_H_

#include <glib-object.h>

#include "smiley.h"
#include "trie.h"

typedef struct _PurpleSmileyList PurpleSmileyList;
typedef struct _PurpleSmileyListClass PurpleSmileyListClass;

#define PURPLE_TYPE_SMILEY_LIST            (purple_smiley_list_get_type())
#define PURPLE_SMILEY_LIST(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyList))
#define PURPLE_SMILEY_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyListClass))
#define PURPLE_IS_SMILEY_LIST(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_SMILEY_LIST))
#define PURPLE_IS_SMILEY_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_SMILEY_LIST))
#define PURPLE_SMILEY_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyListClass))

/**
 * PurpleSmileyList:
 *
 * A list of smileys.
 */
struct _PurpleSmileyList
{
	/*< private >*/
	GObject parent;
};

struct _PurpleSmileyListClass
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
 * purple_smiley_list_get_type:
 *
 * Returns: The #GType for a smiley list.
 */
GType
purple_smiley_list_get_type(void);

PurpleSmileyList *
purple_smiley_list_new(void);

gboolean
purple_smiley_list_add(PurpleSmileyList *list, PurpleSmiley *smiley);

void
purple_smiley_list_remove(PurpleSmileyList *list, PurpleSmiley *smiley);

gboolean
purple_smiley_list_is_empty(PurpleSmileyList *list);

PurpleSmiley *
purple_smiley_list_get_by_shortcut(PurpleSmileyList *list,
	const gchar *shortcut);

/* keys are HTML escaped shortcuts */
PurpleTrie *
purple_smiley_list_get_trie(PurpleSmileyList *list);

GList *
purple_smiley_list_get_unique(PurpleSmileyList *list_);

GList *
purple_smiley_list_get_all(PurpleSmileyList *list_);

G_END_DECLS

#endif /* _PURPLE_SMILEY_H_ */
