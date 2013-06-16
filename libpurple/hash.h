/**
 * @file hash.h Purple Hash API
 * @ingroup core
 * @see @ref hash-signals
 */

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
#ifndef PURPLE_HASH_H
#define PURPLE_HASH_H

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#include "internal.h"

#define PURPLE_TYPE_HASH				(purple_hash_get_type())
#define PURPLE_HASH(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_HASH, PurpleHash))
#define PURPLE_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_HASH, PurpleHashClass))
#define PURPLE_IS_HASH(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_HASH))
#define PURPLE_IS_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_HASH))
#define PURPLE_HASH_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_HASH, PurpleHashClass))

typedef struct _PurpleHash       		PurpleHash;
typedef struct _PurpleHashClass  		PurpleHashClass;

/**
 * PurpleHash:
 *
 * Purple Hash is an opaque data structure and should not be used directly.
 */
struct _PurpleHash {
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleHashClass:
 *
 * The base class for all #PurpleHash's.
 */
struct _PurpleHashClass {
	/*< private >*/
	GObjectClass parent_class;

	/** The reset function */
	void (*reset)(PurpleHash *hash);

	/** The reset state function */
	void (*reset_state)(PurpleHash *hash);

	/** The append data function */
	void (*append)(PurpleHash *hash, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(PurpleHash *hash, guchar digest[], size_t len);

	/** The get digest size function */
	size_t (*get_digest_size)(PurpleHash *hash);

	/** The get block size function */
	size_t (*get_block_size)(PurpleHash *hash);

	/** The get hash name function */
	const gchar* (*get_name)(PurpleHash *hash);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_hash_get_type(void);

const gchar *purple_hash_get_name(PurpleHash *hash);

void purple_hash_reset(PurpleHash *hash);
void purple_hash_reset_state(PurpleHash *hash);

void purple_hash_append(PurpleHash *hash, const guchar *data, size_t len);
gboolean purple_hash_digest(PurpleHash *hash, guchar digest[], size_t len);
gboolean purple_hash_digest_to_str(PurpleHash *hash, gchar digest_s[], size_t len);
size_t purple_hash_get_digest_size(PurpleHash *hash);

size_t purple_hash_get_block_size(PurpleHash *hash);

G_END_DECLS

#endif /* PURPLE_HASH_H */
