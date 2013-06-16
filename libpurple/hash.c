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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "hash.h"
#include "debug.h"

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_hash_class_init(PurpleHashClass *klass) {
	klass->reset = NULL;
	klass->reset_state = NULL;
	klass->append = NULL;
	klass->digest = NULL;
	klass->get_digest_size = NULL;
	klass->get_block_size = NULL;
	klass->get_name = NULL;
}

/******************************************************************************
 * PurpleHash API
 *****************************************************************************/
const gchar *
purple_hash_get_name(PurpleHash *hash) {
	PurpleHashClass *klass = NULL;

	g_return_val_if_fail(hash, NULL);
	g_return_val_if_fail(PURPLE_IS_HASH(hash), NULL);

	klass = PURPLE_HASH_GET_CLASS(hash);
	g_return_val_if_fail(klass->get_name, NULL);

	return klass->get_name(hash);
}

GType
purple_hash_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleHashClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_hash_class_init,
			NULL,
			NULL,
			sizeof(PurpleHash),
			0,
			NULL,
			NULL
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "PurpleHash",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**
 * purple_hash_reset:
 * @hash: The hash to reset
 *
 * Resets a hash to it's default value
 *
 * @note If you have set an IV you will have to set it after resetting
 */
void
purple_hash_reset(PurpleHash *hash) {
	PurpleHashClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_HASH(hash));

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->reset)
		klass->reset(hash);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"reset method\n",
						klass->get_name ? klass->get_name(hash) : "");
}

/**
 * Resets a hash state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest context will be wiped out, but keys, ops or salt
 * will remain untouched.
 */
void
purple_hash_reset_state(PurpleHash *hash) {
	PurpleHashClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_HASH(hash));

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->reset_state)
		klass->reset_state(hash);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"reset_state method\n",
						klass->get_name ? klass->get_name(hash) : "");
}

/**
 * purple_hash_append:
 * @hash: The hash to append data to
 * @data: The data to append
 * @len: The length of the data
 *
 * Appends data to the hash
 */
void
purple_hash_append(PurpleHash *hash, const guchar *data,
								size_t len)
{
	PurpleHashClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_HASH(hash));

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->append)
		klass->append(hash, data, len);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"append method\n",
						klass->get_name ? klass->get_name(hash) : "");
}

/**
 * purple_hash_digest:
 * @hash: The hash to digest
 * @in_len: The length of the buffer
 * @digest: The return buffer for the digest
 * @out_len: The length of the returned value
 *
 * Digests a hash
 *
 * Return Value: TRUE if the digest was successful, FALSE otherwise.
 */
gboolean
purple_hash_digest(PurpleHash *hash, guchar digest[], size_t len)
{
	PurpleHashClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_HASH(hash), FALSE);

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->digest)
		return klass->digest(hash, digest, len);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"digest method\n",
						klass->get_name ? klass->get_name(hash) : "");

	return FALSE;
}

/**
 * purple_hash_digest_to_str:
 * @hash: The hash to get a digest from
 * @in_len: The length of the buffer
 * @digest_s: The return buffer for the string digest
 * @out_len: The length of the returned value
 *
 * Converts a guchar digest into a hex string
 *
 * Return Value: TRUE if the digest was successful, FALSE otherwise.
 */
gboolean
purple_hash_digest_to_str(PurpleHash *hash, gchar digest_s[], size_t len)
{
	/* 8k is a bit excessive, will tweak later. */
	guchar digest[BUF_LEN * 4];
	gint n = 0;
	size_t digest_size;

	g_return_val_if_fail(hash, FALSE);
	g_return_val_if_fail(digest_s, FALSE);

	digest_size = purple_hash_get_digest_size(hash);

	g_return_val_if_fail(digest_size <= BUF_LEN * 4, FALSE);

	if(!purple_hash_digest(hash, digest, sizeof(digest)))
		return FALSE;

	/* Every digest byte occupies 2 chars + the NUL at the end. */
	g_return_val_if_fail(digest_size * 2 + 1 <= len, FALSE);

	for(n = 0; n < digest_size; n++)
		sprintf(digest_s + (n * 2), "%02x", digest[n]);

	digest_s[n * 2] = '\0';

	return TRUE;
}

size_t
purple_hash_get_digest_size(PurpleHash *hash)
{
	PurpleHashClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_HASH(hash), FALSE);

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->get_digest_size)
		return klass->get_digest_size(hash);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"get_digest_size method\n",
						klass->get_name ? klass->get_name(hash) : "");

	return FALSE;
}

/**
 * purple_hash_get_block_size:
 * @hash: The hash whose block size to get
 *
 * Gets the block size of a hash
 *
 * Return Value: The block size of the hash
 */
size_t
purple_hash_get_block_size(PurpleHash *hash)
{
	PurpleHashClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_HASH(hash), -1);

	klass = PURPLE_HASH_GET_CLASS(hash);

	if(klass && klass->get_block_size)
		return klass->get_block_size(hash);
	else
		purple_debug_warning("hash", "the %s hash does not implement the "
						"get_block_size method\n",
						klass->get_name ? klass->get_name(hash) : "");

	return -1;
}
