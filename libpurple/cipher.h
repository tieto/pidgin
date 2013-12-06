/**
 * @file cipher.h Purple Cipher and Hash API
 * @ingroup core
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
#include <glib-object.h>
#include <string.h>

#define PURPLE_TYPE_CIPHER				(purple_cipher_get_type())
#define PURPLE_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CIPHER, PurpleCipher))
#define PURPLE_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CIPHER, PurpleCipherClass))
#define PURPLE_IS_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CIPHER))
#define PURPLE_IS_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CIPHER))
#define PURPLE_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CIPHER, PurpleCipherClass))

typedef struct _PurpleCipher       PurpleCipher;
typedef struct _PurpleCipherClass  PurpleCipherClass;

#define PURPLE_TYPE_HASH				(purple_hash_get_type())
#define PURPLE_HASH(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_HASH, PurpleHash))
#define PURPLE_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_HASH, PurpleHashClass))
#define PURPLE_IS_HASH(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_HASH))
#define PURPLE_IS_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_HASH))
#define PURPLE_HASH_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_HASH, PurpleHashClass))

typedef struct _PurpleHash       PurpleHash;
typedef struct _PurpleHashClass  PurpleHashClass;

/**
 * PurpleCipherBatchMode:
 * @PURPLE_CIPHER_BATCH_MODE_ECB: Electronic Codebook Mode
 * @PURPLE_CIPHER_BATCH_MODE_CBC: Cipher Block Chaining Mode
 *
 * Modes for batch encrypters
 */
typedef enum {
	PURPLE_CIPHER_BATCH_MODE_ECB,
	PURPLE_CIPHER_BATCH_MODE_CBC
} PurpleCipherBatchMode;

/**
 * PurpleCipher:
 *
 * Purple Cipher is an opaque data structure and should not be used directly.
 */
struct _PurpleCipher {
	GObject gparent;
};

/**
 * PurpleCipherClass:
 *
 * The base class for all #PurpleCipher's.
 */
struct _PurpleCipherClass {
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

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleHash:
 *
 * Purple Hash is an opaque data structure and should not be used directly.
 */
struct _PurpleHash {
	GObject gparent;
};

/**
 * PurpleHashClass:
 *
 * The base class for all #PurpleHash's.
 */
struct _PurpleHashClass {
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

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/*****************************************************************************/
/** @name PurpleCipher API                                                   */
/*****************************************************************************/
/*@{*/

/**
 * Returns the GType for the Cipher object.
 */
GType purple_cipher_get_type(void);

/**
 * Resets a cipher to it's default value
 * @note If you have set an IV you will have to set it after resetting
 *
 * @param cipher  The cipher
 */
void purple_cipher_reset(PurpleCipher *cipher);

/**
 * Resets a cipher state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest will be wiped out, but keys, ops or salt
 * will remain untouched.
 *
 * @param cipher  The cipher
 */
void purple_cipher_reset_state(PurpleCipher *cipher);

/**
 * Sets the initialization vector for a cipher
 * @note This should only be called right after a cipher is created or reset
 *
 * @param cipher  The cipher
 * @param iv      The initialization vector to set
 * @param len     The len of the IV
 */
void purple_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len);

/**
 * Appends data to the cipher context
 *
 * @param cipher  The cipher
 * @param data    The data to append
 * @param len     The length of the data
 */
void purple_cipher_append(PurpleCipher *cipher, const guchar *data, size_t len);

/**
 * Digests a cipher context
 *
 * @param cipher  The cipher
 * @param digest  The return buffer for the digest
 * @param len     The length of the buffer
 */
gboolean purple_cipher_digest(PurpleCipher *cipher, guchar digest[], size_t len);

/**
 * Converts a guchar digest into a hex string
 *
 * @param cipher   The cipher
 * @param digest_s The return buffer for the string digest
 * @param len      The length of the buffer
 */
gboolean purple_cipher_digest_to_str(PurpleCipher *cipher, gchar digest_s[], size_t len);

/**
 * Gets the digest size of a cipher
 *
 * @param cipher The cipher whose digest size to get
 *
 * @return The digest size of the cipher
 */
size_t purple_cipher_get_digest_size(PurpleCipher *cipher);

/**
 * Encrypts data using the cipher
 *
 * @param cipher   The cipher
 * @param input    The data to encrypt
 * @param in_len   The length of the data
 * @param output   The output buffer
 * @param out_size The size of the output buffer
 *
 * @return A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_encrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * Decrypts data using the cipher
 *
 * @param cipher   The cipher
 * @param input    The data to encrypt
 * @param in_len   The length of the returned value
 * @param output   The output buffer
 * @param out_size The size of the output buffer
 *
 * @return A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_decrypt(PurpleCipher *cipher, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * Sets the salt on a cipher
 *
 * @param cipher  The cipher whose salt to set
 * @param salt    The salt
 * @param len     The length of the salt
 */
void purple_cipher_set_salt(PurpleCipher *cipher, const guchar *salt, size_t len);

/**
 * Sets the key on a cipher
 *
 * @param cipher  The cipher whose key to set
 * @param key     The key
 * @param len     The size of the key
 */
void purple_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len);

/**
 * Gets the size of the key if the cipher supports it
 *
 * @param cipher The cipher whose key size to get
 *
 * @return The size of the key
 */
size_t purple_cipher_get_key_size(PurpleCipher *cipher);

/**
 * Sets the batch mode of a cipher
 *
 * @param cipher  The cipher whose batch mode to set
 * @param mode    The batch mode under which the cipher should operate
 *
 */
void purple_cipher_set_batch_mode(PurpleCipher *cipher, PurpleCipherBatchMode mode);

/**
 * Gets the batch mode of a cipher
 *
 * @param cipher The cipher whose batch mode to get
 *
 * @return The batch mode under which the cipher is operating
 */
PurpleCipherBatchMode purple_cipher_get_batch_mode(PurpleCipher *cipher);

/**
 * Gets the block size of a cipher
 *
 * @param cipher The cipher whose block size to get
 *
 * @return The block size of the cipher
 */
size_t purple_cipher_get_block_size(PurpleCipher *cipher);

/*@}*/

/*****************************************************************************/
/** @name PurpleHash API                                                     */
/*****************************************************************************/
/*@{*/

/**
 * Returns the GType for the Hash object.
 */
GType purple_hash_get_type(void);

/**
 * Resets a hash to it's default value
 * @note If you have set an IV you will have to set it after resetting
 *
 * @param hash  The hash
 */
void purple_hash_reset(PurpleHash *hash);

/**
 * Resets a hash state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest will be wiped out, but keys, ops or salt
 * will remain untouched.
 *
 * @param hash  The hash
 */
void purple_hash_reset_state(PurpleHash *hash);

/**
 * Appends data to the hash context
 *
 * @param hash    The hash
 * @param data    The data to append
 * @param len     The length of the data
 */
void purple_hash_append(PurpleHash *hash, const guchar *data, size_t len);

/**
 * Digests a hash context
 *
 * @param hash    The hash
 * @param digest  The return buffer for the digest
 * @param len     The length of the buffer
 */
gboolean purple_hash_digest(PurpleHash *hash, guchar digest[], size_t len);

/**
 * Converts a guchar digest into a hex string
 *
 * @param hash     The hash
 * @param digest_s The return buffer for the string digest
 * @param len      The length of the buffer
 */
gboolean purple_hash_digest_to_str(PurpleHash *hash, gchar digest_s[], size_t len);

/**
 * Gets the digest size of a hash
 *
 * @param hash The hash whose digest size to get
 *
 * @return The digest size of the hash
 */
size_t purple_hash_get_digest_size(PurpleHash *hash);

/**
 * Gets the block size of a hash
 *
 * @param hash The hash whose block size to get
 *
 * @return The block size of the hash
 */
size_t purple_hash_get_block_size(PurpleHash *hash);

/*@}*/

G_END_DECLS

#endif /* PURPLE_CIPHER_H */
