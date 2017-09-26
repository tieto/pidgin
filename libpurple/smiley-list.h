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

#ifndef PURPLE_SMILEY_LIST_H
#define PURPLE_SMILEY_LIST_H

/**
 * SECTION:smiley-list
 * @include:smiley-list.h
 * @section_id: libpurple-smiley-list
 * @short_description: a collection of smileys
 * @title: Smiley lists
 *
 * A #PurpleSmileyList is a handy storage for a set of #PurpleSmiley's.
 * It holds structures needed for parsing, accessing them by a shortcut, or
 * listing (either all, or by unique image).
 */

#include <glib-object.h>

#include "smiley.h"
#include "trie.h"

typedef struct _PurpleSmileyList PurpleSmileyList;
typedef struct _PurpleSmileyListClass PurpleSmileyListClass;

#define PURPLE_TYPE_SMILEY_LIST            (purple_smiley_list_get_type())
#define PURPLE_SMILEY_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyList))
#define PURPLE_SMILEY_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyListClass))
#define PURPLE_IS_SMILEY_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_SMILEY_LIST))
#define PURPLE_IS_SMILEY_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_SMILEY_LIST))
#define PURPLE_SMILEY_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SMILEY_LIST, PurpleSmileyListClass))

/**
 * PurpleSmileyList:
 *
 * A container for #PurpleSmiley's.
 */
struct _PurpleSmileyList {
	/*< private >*/
	GObject parent;
};

struct _PurpleSmileyListClass {
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
 * Returns: the #GType for a smiley list.
 */
GType purple_smiley_list_get_type(void);

/**
 * purple_smiley_list_new:
 *
 * Creates new #PurpleSmileyList. You might prefer using existing lists, like
 * #purple_smiley_custom_get_list or #purple_conversation_get_remote_smileys
 * (the latter is read-only, accessed with
 * #purple_conversation_add_remote_smiley and
 * #purple_conversation_get_remote_smiley).
 *
 * Returns: newly created #PurpleSmileyList.
 */
PurpleSmileyList *purple_smiley_list_new(void);

/**
 * purple_smiley_list_add:
 * @list: the smiley list.
 * @smiley: the smiley to be added.
 *
 * Adds the @smiley to the @list. A particular @smiley can only be added to
 * a single @list. Also, a @list can not contain multiple @smiley's with
 * the same shortcut.
 *
 * It increases the reference count of @smiley, if needed.
 *
 * Returns: %TRUE if the @smiley was successfully added to the list.
 */
gboolean purple_smiley_list_add(PurpleSmileyList *list, PurpleSmiley *smiley);

/**
 * purple_smiley_list_remove:
 * @list: the smiley list.
 * @smiley: the smiley to be removed.
 *
 * Removes a @smiley from the @list. If @smiley's reference count drops to zero,
 * it's destroyed.
 */
void purple_smiley_list_remove(PurpleSmileyList *list, PurpleSmiley *smiley);

/**
 * purple_smiley_list_is_empty:
 * @list: the smiley list.
 *
 * Checks, if the smiley @list is empty.
 *
 * Returns: %TRUE if the @list is empty, %FALSE otherwise.
 */
gboolean purple_smiley_list_is_empty(const PurpleSmileyList *list);

/**
 * purple_smiley_list_get_by_shortcut:
 * @list: the smiley list.
 * @shortcut: the textual representation of smiley to lookup.
 *
 * Retrieves a smiley with the specified @shortcut from the @list.
 *
 * Returns: a #PurpleSmiley if the smiley was found, %NULL otherwise.
 */
PurpleSmiley *purple_smiley_list_get_by_shortcut(PurpleSmileyList *list, const gchar *shortcut);

/**
 * purple_smiley_list_get_trie:
 * @list: the smiley list.
 *
 * Returns the #PurpleTrie for searching of #PurpleSmiley's being kept
 * in the @list. It holds markup escaped shortcuts, so if you want to search
 * in plain message, you have to escape it first. If you don't do this, it won't
 * find smileys containing special characters.
 *
 * Returns: (transfer none): a #PurpleTrie for contained smileys.
 */
PurpleTrie *purple_smiley_list_get_trie(PurpleSmileyList *list);

/**
 * purple_smiley_list_get_unique:
 * @list_: the smiley list.
 *
 * Returns the list of smileys with unique image file paths.
 *
 * Returns: (element-type PurpleSmiley) (transfer container): the list of unique smileys.
 */
GList *purple_smiley_list_get_unique(PurpleSmileyList *list_);

/**
 * purple_smiley_list_get_all:
 * @list_: the smiley list.
 *
 * Returns the list of all smileys added to the @list_.
 *
 * Returns: (element-type PurpleSmiley) (transfer container): the list of smileys.
 */
GList *purple_smiley_list_get_all(PurpleSmileyList *list_);

G_END_DECLS

#endif /* PURPLE_SMILEY_LIST_H */
