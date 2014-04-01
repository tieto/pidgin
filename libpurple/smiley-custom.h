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

#ifndef _PURPLE_SMILEY_CUSTOM_H_
#define _PURPLE_SMILEY_CUSTOM_H_

#include "smiley.h"
#include "trie.h"

PurpleSmiley *
purple_smiley_custom_add(PurpleStoredImage *img, const gchar *shortcut);

void
purple_smiley_custom_remove(PurpleSmiley *smiley);

GList *
purple_smiley_custom_get_all(void);

PurpleTrie *
purple_smiley_custom_get_trie(void);

void
purple_smiley_custom_init(void);

void
purple_smiley_custom_uninit(void);

#endif /* _PURPLE_SMILEY_CUSTOM_H_ */
