/**
 * @file cipher.h Purple Cipher API
 * @ingroup core
 * @see @ref cipher-signals
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
#ifndef PURPLE_CIPHER_H
#define PURPLE_CIPHER_H

#include <glib.h>
#include <string.h>

#include <glib-object.h>

#define PURPLE_TYPE_CIPHER				(purple_cipher_get_type())
#define PURPLE_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CIPHER, PurpleCipher))
#define PURPLE_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CIPHER, PurpleCipherClass))
#define PURPLE_IS_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CIPHER))
#define PURPLE_IS_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CIPHER))
#define PURPLE_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CIPHER, PurpleCipherClass))

typedef struct _PurpleCipher       PurpleCipher;
typedef struct _PurpleCipherClass  PurpleCipherClass;

#define PURPLE_TYPE_CIPHER_BATCH_MODE	(purple_cipher_batch_mode_get_type())

/**
 * PurpleCipherBatchMode:
 * @PURPLE_CIPHER_BATCH_MODE_ECB: Electronic Codebook Mode
 * @PURPLE_CIPHER_BATCH_MODE_CBC: Cipher Block Chaining Mode
 *
 * Modes for batch encrypters
 */
typedef enum  {
	PURPLE_CIPHER_BATCH_MODE_ECB, /*< nick=ECB Batch Mode >*/
	PURPLE_CIPHER_BATCH_MODE_CBC /*< nick=CBC Batch Mode >*/
} PurpleCipherBatchMode;

/**
 * PurpleCipher:
 *
 * Purple Cipher is an opaque data structure and should not be used directly.
 */
struct _PurpleCipher {
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleCipherClass:
 *
 * The base class for all #PurpleCipher's.
 */
struct _PurpleCipherClass {
	/*< private >*/
	GObjectClass parent_class;

	/** The reset function */
	void (*reset)(PurpleCipher *cipher);

	/** The reset state function */
	void (*reset_state)(PurpleCipher *cipher);

	/** The set initialization vector function */
	void (*set_iv)(PurpleCipher *cipher, guchar *iv, size_t len);

	/** The append data function */
	void (*append)(PurpleCipher *cipher, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(PurpleCipher *cipher, guchar digest[], size_t len);

	/** The get digest size function */
	size_t (*get_digest_size)(PurpleCipher *cipher);

	/** The encrypt function */
	ssize_t (*encrypt)(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The decrypt function */
	ssize_t (*decrypt)(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The set salt function */
	void (*set_salt)(PurpleCipher *cipher, const guchar *salt, size_t len);

	/** The get salt size function */
	size_t (*get_salt_size)(PurpleCipher *cipher);

	/** The set key function */
	void (*set_key)(PurpleCipher *cipher, const guchar *key, size_t len);

	/** The get key size function */
	size_t (*get_key_size)(PurpleCipher *cipher);

	/** The set batch mode function */
	void (*set_batch_mode)(PurpleCipher *cipher, PurpleCipherBatchMode mode);

	/** The get batch mode function */
	PurpleCipherBatchMode (*get_batch_mode)(PurpleCipher *cipher);

	/** The get block size function */
	size_t (*get_block_size)(PurpleCipher *cipher);

	/** The get cipher name function */
	const gchar* (*get_name)(PurpleCipher *cipher);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_cipher_get_type(void);
GType purple_cipher_batch_mode_get_type(void);

const gchar *purple_cipher_get_name(PurpleCipher *cipher);

void purple_cipher_reset(PurpleCipher *cipher);
void purple_cipher_reset_state(PurpleCipher *cipher);
void purple_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len);

void purple_cipher_append(PurpleCipher *cipher, const guchar *data, size_t len);
gboolean purple_cipher_digest(PurpleCipher *cipher, guchar digest[], size_t len);
gboolean purple_cipher_digest_to_str(PurpleCipher *cipher, gchar digest_s[], size_t len);
size_t purple_cipher_get_digest_size(PurpleCipher *cipher);

ssize_t purple_cipher_encrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);
ssize_t purple_cipher_decrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

void purple_cipher_set_salt(PurpleCipher *cipher, const guchar *salt, size_t len);
size_t purple_cipher_get_salt_size(PurpleCipher *cipher);

void purple_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len);
size_t purple_cipher_get_key_size(PurpleCipher *cipher);

void purple_cipher_set_batch_mode(PurpleCipher *cipher, PurpleCipherBatchMode mode);
PurpleCipherBatchMode purple_cipher_get_batch_mode(PurpleCipher *cipher);

size_t purple_cipher_get_block_size(PurpleCipher *cipher);

G_END_DECLS

#endif /* PURPLE_CIPHER_H */
