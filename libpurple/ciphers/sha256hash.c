/*
 * purple
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
#include "sha256hash.h"

/*******************************************************************************
 * Structs
 ******************************************************************************/
#define PURPLE_SHA256_HASH_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SHA256_HASH, PurpleSHA256HashPrivate))

typedef struct {
  GChecksum *checksum;
} PurpleSHA256HashPrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Hash Stuff
 *****************************************************************************/

static void
purple_sha256_hash_reset(PurpleHash *hash)
{
	PurpleSHA256Hash *sha256_hash = PURPLE_SHA256_HASH(hash);
	PurpleSHA256HashPrivate *priv = PURPLE_SHA256_HASH_GET_PRIVATE(sha256_hash);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->checksum != NULL);

	g_checksum_reset(priv->checksum);
}

static void
purple_sha256_hash_append(PurpleHash *hash, const guchar *data,
								gsize len)
{
	PurpleSHA256Hash *sha256_hash = PURPLE_SHA256_HASH(hash);
	PurpleSHA256HashPrivate *priv = PURPLE_SHA256_HASH_GET_PRIVATE(sha256_hash);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->checksum != NULL);

	while (len >= G_MAXSSIZE) {
		g_checksum_update(priv->checksum, data, G_MAXSSIZE);
		len -= G_MAXSSIZE;
		data += G_MAXSSIZE;
	}

	if (len)
		g_checksum_update(priv->checksum, data, len);
}

static gboolean
purple_sha256_hash_digest(PurpleHash *hash, guchar *digest, size_t buff_len)
{
	PurpleSHA256Hash *sha256_hash = PURPLE_SHA256_HASH(hash);
	PurpleSHA256HashPrivate *priv = PURPLE_SHA256_HASH_GET_PRIVATE(sha256_hash);

	const gssize required_len = g_checksum_type_get_length(G_CHECKSUM_SHA256);
	gsize digest_len = buff_len;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->checksum != NULL, FALSE);

	g_return_val_if_fail(required_len >= 0, FALSE);
	g_return_val_if_fail(buff_len >= (gsize)required_len, FALSE);

	g_checksum_get_digest(priv->checksum, digest, &digest_len);

	if (digest_len != (gsize)required_len)
		return FALSE;

	purple_sha256_hash_reset(hash);

	return TRUE;
}

static size_t
purple_sha256_hash_get_block_size(PurpleHash *hash)
{
	return 64;
}

static size_t
purple_sha256_hash_get_digest_size(PurpleHash *hash)
{
	return g_checksum_type_get_length(G_CHECKSUM_SHA256);
}

static const gchar*
purple_sha256_hash_get_name(PurpleHash *hash)
{
	return "sha256";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/

static void
purple_sha256_hash_finalize(GObject *obj)
{
	PurpleSHA256Hash *sha256_hash = PURPLE_SHA256_HASH(obj);
	PurpleSHA256HashPrivate *priv = PURPLE_SHA256_HASH_GET_PRIVATE(sha256_hash);

	if (priv->checksum)
		g_checksum_free(priv->checksum);

	parent_class->finalize(obj);
}

static void
purple_sha256_hash_class_init(PurpleSHA256HashClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleHashClass *hash_class = PURPLE_HASH_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_sha256_hash_finalize;

	hash_class->reset = purple_sha256_hash_reset;
	hash_class->reset_state = purple_sha256_hash_reset;
	hash_class->append = purple_sha256_hash_append;
	hash_class->digest = purple_sha256_hash_digest;
	hash_class->get_digest_size = purple_sha256_hash_get_digest_size;
	hash_class->get_block_size = purple_sha256_hash_get_block_size;
	hash_class->get_name = purple_sha256_hash_get_name;

	g_type_class_add_private(klass, sizeof(PurpleSHA256HashPrivate));
}

static void
purple_sha256_hash_init(PurpleHash *hash)
{
	PurpleSHA256Hash *sha256_hash = PURPLE_SHA256_HASH(hash);
	PurpleSHA256HashPrivate *priv = PURPLE_SHA256_HASH_GET_PRIVATE(sha256_hash);

	priv->checksum = g_checksum_new(G_CHECKSUM_SHA256);

	purple_sha256_hash_reset(hash);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_sha256_hash_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleSHA256HashClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_sha256_hash_class_init,
			NULL,
			NULL,
			sizeof(PurpleSHA256Hash),
			0,
			(GInstanceInitFunc)purple_sha256_hash_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_HASH,
									  "PurpleSHA256Hash",
									  &info, 0);
	}

	return type;
}

PurpleHash *
purple_sha256_hash_new(void) {
	return g_object_new(PURPLE_TYPE_SHA256_HASH, NULL);
}
