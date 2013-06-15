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
#include "md5hash.h"

/*******************************************************************************
 * Structs
 ******************************************************************************/
#define PURPLE_MD5_HASH_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MD5_HASH, PurpleMD5HashPrivate))

typedef struct {
  GChecksum *checksum;
} PurpleMD5HashPrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Hash Stuff
 *****************************************************************************/

static void
purple_md5_hash_reset(PurpleHash *hash)
{
	PurpleMD5Hash *md5_hash = PURPLE_MD5_HASH(hash);
	PurpleMD5HashPrivate *priv = PURPLE_MD5_HASH_GET_PRIVATE(md5_hash);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->checksum != NULL);

	g_checksum_reset(priv->checksum);
}

static void
purple_md5_hash_append(PurpleHash *hash, const guchar *data,
								gsize len)
{
	PurpleMD5Hash *md5_hash = PURPLE_MD5_HASH(hash);
	PurpleMD5HashPrivate *priv = PURPLE_MD5_HASH_GET_PRIVATE(md5_hash);

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
purple_md5_hash_digest(PurpleHash *hash, guchar *digest, size_t buff_len)
{
	PurpleMD5Hash *md5_hash = PURPLE_MD5_HASH(hash);
	PurpleMD5HashPrivate *priv = PURPLE_MD5_HASH_GET_PRIVATE(md5_hash);

	const gssize required_len = g_checksum_type_get_length(G_CHECKSUM_MD5);
	gsize digest_len = buff_len;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->checksum != NULL, FALSE);
	g_return_val_if_fail(buff_len >= required_len, FALSE);

	g_checksum_get_digest(priv->checksum, digest, &digest_len);

	if (digest_len != required_len)
		return FALSE;

	purple_md5_hash_reset(hash);

	return TRUE;
}

static size_t
purple_md5_hash_get_block_size(PurpleHash *hash)
{
	return 64;
}

static size_t
purple_md5_hash_get_digest_size(PurpleHash *hash)
{
	return g_checksum_type_get_length(G_CHECKSUM_MD5);
}

static const gchar*
purple_md5_hash_get_name(PurpleHash *hash)
{
	return "md5";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/

static void
purple_md5_hash_finalize(GObject *obj)
{
	PurpleMD5Hash *md5_hash = PURPLE_MD5_HASH(obj);
	PurpleMD5HashPrivate *priv = PURPLE_MD5_HASH_GET_PRIVATE(md5_hash);

	if (priv->checksum)
		g_checksum_free(priv->checksum);

	parent_class->finalize(obj);
}

static void
purple_md5_hash_class_init(PurpleMD5HashClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleHashClass *hash_class = PURPLE_HASH_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_md5_hash_finalize;

	hash_class->reset = purple_md5_hash_reset;
	hash_class->reset_state = purple_md5_hash_reset;
	hash_class->append = purple_md5_hash_append;
	hash_class->digest = purple_md5_hash_digest;
	hash_class->get_digest_size = purple_md5_hash_get_digest_size;
	hash_class->get_block_size = purple_md5_hash_get_block_size;
	hash_class->get_name = purple_md5_hash_get_name;

	g_type_class_add_private(klass, sizeof(PurpleMD5HashPrivate));
}

static void
purple_md5_hash_init(PurpleHash *hash)
{
	PurpleMD5Hash *md5_hash = PURPLE_MD5_HASH(hash);
	PurpleMD5HashPrivate *priv = PURPLE_MD5_HASH_GET_PRIVATE(md5_hash);

	priv->checksum = g_checksum_new(G_CHECKSUM_MD5);

	purple_md5_hash_reset(hash);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_md5_hash_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMD5HashClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_md5_hash_class_init,
			NULL,
			NULL,
			sizeof(PurpleMD5Hash),
			0,
			(GInstanceInitFunc)purple_md5_hash_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_HASH,
									  "PurpleMD5Hash",
									  &info, 0);
	}

	return type;
}

PurpleHash *
purple_md5_hash_new(void) {
	return g_object_new(PURPLE_TYPE_MD5_HASH, NULL);
}
