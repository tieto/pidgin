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
ggp_keymapper_to_key(ggp_keymapper *km, uint64_t val)
{
	uint64_t *key;

	g_return_val_if_fail(km != NULL, NULL);

	key = g_hash_table_lookup(km->val_to_key, &val);
	if (key)
		return key;

	key = g_new(uint64_t, 1);
	*key = val;

	g_hash_table_insert(km->val_to_key, key, key);

	return key;
}

uint64_t
ggp_keymapper_from_key(ggp_keymapper *km, gpointer key)
{
	g_return_val_if_fail(km != NULL, 0);
	g_return_val_if_fail(key != NULL, 0);

	return *((uint64_t*)key);
}
