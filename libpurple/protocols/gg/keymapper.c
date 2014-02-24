/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#include "keymapper.h"

/* The problem: we want to convert 64-bit unique integers into unique gpointer
 * keys (that may be 32-bit or 64-bit, or whatever else). We also want to
 * convert it back.
 *
 * The idea: let's store every value in our internal memory. Then, its address
 * can be also an unique key. We also need a map, to quickly figure out the
 * address for any previously stored value.
 *
 * The naming problem: values becames the keys and vice-versa.
 */

/* TODO
 * For a 64-bit gpointer, keymapper could just do nothing and return the value
 * as a key. But it have to be figured out at a compile time.
 */

struct _ggp_keymapper
{
	/* Table keys: pointers to 64-bit mapped *values*.
	 * Table values: keys (gpointers) corresponding to mapped values.
	 *
	 * Ultimately, both keys and values are the same pointers.
	 *
	 * Yes, it's hard to comment it well enough.
	 */
	GHashTable *val_to_key;
};

ggp_keymapper *
ggp_keymapper_new(void)
{
	ggp_keymapper *km;

	km = g_new0(ggp_keymapper, 1);
	km->val_to_key = g_hash_table_new_full(g_int64_hash, g_int64_equal,
		g_free, NULL);

	return km;
}

void
ggp_keymapper_free(ggp_keymapper *km)
{
	if (km == NULL)
		return;

	g_hash_table_destroy(km->val_to_key);
	g_free(km);
}

gpointer
ggp_keymapper_to_key(ggp_keymapper *km, guint64 val)
{
	guint64 *key;

	g_return_val_if_fail(km != NULL, NULL);

	key = g_hash_table_lookup(km->val_to_key, &val);
	if (key)
		return key;

	key = g_new(guint64, 1);
	*key = val;

	g_hash_table_insert(km->val_to_key, key, key);

	return key;
}

guint64
ggp_keymapper_from_key(ggp_keymapper *km, gpointer key)
{
	g_return_val_if_fail(km != NULL, 0);
	g_return_val_if_fail(key != NULL, 0);

	return *((guint64*)key);
}
