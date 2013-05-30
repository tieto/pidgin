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

#define PURPLE_CIPHER(obj)			((PurpleCipher *)(obj))			/**< PurpleCipher typecast helper			*/
#define PURPLE_CIPHER_OPS(obj)		((PurpleCipherOps *)(obj))		/**< PurpleCipherInfo typecase helper		*/
#define PURPLE_CIPHER_CONTEXT(obj)	((PurpleCipherContext *)(obj))	/**< PurpleCipherContext typecast helper	*/

typedef struct _PurpleCipher			PurpleCipher;			/**< A handle to a PurpleCipher	*/
typedef struct _PurpleCipherOps		PurpleCipherOps;		/**< Ops for a PurpleCipher		*/
typedef struct _PurpleCipherContext	PurpleCipherContext;	/**< A context for a PurpleCipher	*/

/**
 * Modes for batch encrypters
 */
typedef enum {
	PURPLE_CIPHER_BATCH_MODE_ECB,
	PURPLE_CIPHER_BATCH_MODE_CBC
} PurpleCipherBatchMode;

/**
 * The operation flags for a cipher
 */
typedef enum {
	PURPLE_CIPHER_CAPS_SET_OPT          = 1 << 1,   /**< Set option flag */
	PURPLE_CIPHER_CAPS_GET_OPT          = 1 << 2,   /**< Get option flag */
	PURPLE_CIPHER_CAPS_INIT             = 1 << 3,   /**< Init flag */
	PURPLE_CIPHER_CAPS_RESET            = 1 << 4,   /**< Reset flag */
	PURPLE_CIPHER_CAPS_RESET_STATE      = 1 << 5,   /**< Reset state flag */
	PURPLE_CIPHER_CAPS_UNINIT           = 1 << 6,   /**< Uninit flag */
	PURPLE_CIPHER_CAPS_SET_IV           = 1 << 7,   /**< Set IV flag */
	PURPLE_CIPHER_CAPS_APPEND           = 1 << 8,   /**< Append flag */
	PURPLE_CIPHER_CAPS_DIGEST           = 1 << 9,   /**< Digest flag */
	PURPLE_CIPHER_CAPS_GET_DIGEST_SIZE  = 1 << 10,   /**< The get digest size flag */
	PURPLE_CIPHER_CAPS_ENCRYPT          = 1 << 11,  /**< Encrypt flag */
	PURPLE_CIPHER_CAPS_DECRYPT          = 1 << 12,  /**< Decrypt flag */
	PURPLE_CIPHER_CAPS_SET_SALT         = 1 << 13,  /**< Set salt flag */
	PURPLE_CIPHER_CAPS_GET_SALT_SIZE    = 1 << 14,  /**< Get salt size flag */
	PURPLE_CIPHER_CAPS_SET_KEY          = 1 << 15,  /**< Set key flag */
	PURPLE_CIPHER_CAPS_GET_KEY_SIZE     = 1 << 16,  /**< Get key size flag */
	PURPLE_CIPHER_CAPS_SET_BATCH_MODE   = 1 << 17,  /**< Set batch mode flag */
	PURPLE_CIPHER_CAPS_GET_BATCH_MODE   = 1 << 18,  /**< Get batch mode flag */
	PURPLE_CIPHER_CAPS_GET_BLOCK_SIZE   = 1 << 19,  /**< The get block size flag */
	PURPLE_CIPHER_CAPS_UNKNOWN          = 1 << 20   /**< Unknown */
} PurpleCipherCaps;

/**
 * The operations of a cipher.  Every cipher must implement one of these.
 */
struct _PurpleCipherOps {
	/** The set option function	*/
	void (*set_option)(PurpleCipherContext *context, const gchar *name, void *value);

	/** The get option function */
	void *(*get_option)(PurpleCipherContext *context, const gchar *name);

	/** The init function */
	void (*init)(PurpleCipherContext *context, void *extra);

	/** The reset function */
	void (*reset)(PurpleCipherContext *context, void *extra);

	/** The reset state function */
	void (*reset_state)(PurpleCipherContext *context, void *extra);

	/** The uninit function */
	void (*uninit)(PurpleCipherContext *context);

	/** The set initialization vector function */
	void (*set_iv)(PurpleCipherContext *context, guchar *iv, size_t len);

	/** The append data function */
	void (*append)(PurpleCipherContext *context, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(PurpleCipherContext *context, guchar digest[], size_t len);

	/** The get digest size function */
	size_t (*get_digest_size)(PurpleCipherContext *context);

	/** The encrypt function */
	ssize_t (*encrypt)(PurpleCipherContext *context, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The decrypt function */
	ssize_t (*decrypt)(PurpleCipherContext *context, const guchar input[], size_t in_len, guchar output[], size_t out_size);

	/** The set salt function */
	void (*set_salt)(PurpleCipherContext *context, const guchar *salt, size_t len);

	/** The get salt size function */
	size_t (*get_salt_size)(PurpleCipherContext *context);

	/** The set key function */
	void (*set_key)(PurpleCipherContext *context, const guchar *key, size_t len);

	/** The get key size function */
	size_t (*get_key_size)(PurpleCipherContext *context);

	/** The set batch mode function */
	void (*set_batch_mode)(PurpleCipherContext *context, PurpleCipherBatchMode mode);

	/** The get batch mode function */
	PurpleCipherBatchMode (*get_batch_mode)(PurpleCipherContext *context);

	/** The get block size function */
	size_t (*get_block_size)(PurpleCipherContext *context);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/*****************************************************************************/
/** @name PurpleCipher API													 */
/*****************************************************************************/
/*@{*/

/**
 * Gets a cipher's name
 *
 * @param cipher The cipher handle
 *
 * @return The cipher's name
 */
const gchar *purple_cipher_get_name(PurpleCipher *cipher);

/**
 * Gets a cipher's capabilities
 *
 * @param cipher The cipher handle
 *
 * @return The cipher's info
 */
guint purple_cipher_get_capabilities(PurpleCipher *cipher);

/**
 * Gets a digest from a cipher
 *
 * @param name     The cipher's name
 * @param data     The data to hash
 * @param data_len The length of the data
 * @param digest   The returned digest
 * @param out_size The size of digest buffer
 *
 * @return The count of bytes written, or -1 if failed
 */
ssize_t purple_cipher_digest_region(const gchar *name, const guchar *data, size_t data_len, guchar digest[], size_t out_size);

/*@}*/
/******************************************************************************/
/** @name PurpleCiphers API													  */
/******************************************************************************/
/*@{*/

/**
 * Finds a cipher by it's name
 *
 * @param name The name of the cipher to find
 *
 * @return The cipher handle or @c NULL
 */
PurpleCipher *purple_ciphers_find_cipher(const gchar *name);

/**
 * Registers a cipher as a usable cipher
 *
 * @param name The name of the new cipher
 * @param ops  The cipher ops to register
 *
 * @return The handle to the new cipher or @c NULL if it failed
 */
PurpleCipher *purple_ciphers_register_cipher(const gchar *name, PurpleCipherOps *ops);

/**
 * Unregisters a cipher
 *
 * @param cipher The cipher handle to unregister
 *
 * @return Whether or not the cipher was successfully unloaded
 */
gboolean purple_ciphers_unregister_cipher(PurpleCipher *cipher);

/**
 * Gets the list of ciphers
 *
 * @return The list of available ciphers
 * @note This list should not be modified, it is owned by the cipher core
 */
GList *purple_ciphers_get_ciphers(void);

/*@}*/
/******************************************************************************/
/** @name PurpleCipher Subsystem API											  */
/******************************************************************************/
/*@{*/

/**
 * Gets the handle to the cipher subsystem
 *
 * @return The handle to the cipher subsystem
 */
gpointer purple_ciphers_get_handle(void);

/**
 * Initializes the cipher core
 */
void purple_ciphers_init(void);

/**
 * Uninitializes the cipher core
 */
void purple_ciphers_uninit(void);

/*@}*/
/******************************************************************************/
/** @name PurpleCipherContext API												  */
/******************************************************************************/
/*@{*/

/**
 * Sets the value an option on a cipher context
 *
 * @param context The cipher context
 * @param name    The name of the option
 * @param value   The value to set
 */
void purple_cipher_context_set_option(PurpleCipherContext *context, const gchar *name, gpointer value);

/**
 * Gets the vale of an option on a cipher context
 *
 * @param context The cipher context
 * @param name    The name of the option
 * @return The value of the option
 */
gpointer purple_cipher_context_get_option(PurpleCipherContext *context, const gchar *name);

/**
 * Creates a new cipher context and initializes it
 *
 * @param cipher The cipher to use
 * @param extra  Extra data for the specific cipher
 *
 * @return The new cipher context
 */
PurpleCipherContext *purple_cipher_context_new(PurpleCipher *cipher, void *extra);

/**
 * Creates a new cipher context by the cipher name and initializes it
 *
 * @param name  The cipher's name
 * @param extra Extra data for the specific cipher
 *
 * @return The new cipher context
 */
PurpleCipherContext *purple_cipher_context_new_by_name(const gchar *name, void *extra);

/**
 * Resets a cipher context to it's default value
 * @note If you have set an IV you will have to set it after resetting
 *
 * @param context The context to reset
 * @param extra   Extra data for the specific cipher
 */
void purple_cipher_context_reset(PurpleCipherContext *context, gpointer extra);

/**
 * Resets a cipher state to it's default value, but doesn't touch stateless
 * configuration.
 *
 * That means, IV and digest context will be wiped out, but keys, ops or salt
 * will remain untouched.
 *
 * @param context The context to reset
 * @param extra   Extra data for the specific cipher
 */
void purple_cipher_context_reset_state(PurpleCipherContext *context, gpointer extra);

/**
 * Destorys a cipher context and deinitializes it
 *
 * @param context The cipher context to destory
 */
void purple_cipher_context_destroy(PurpleCipherContext *context);

/**
 * Sets the initialization vector for a context
 * @note This should only be called right after a cipher context is created or reset
 *
 * @param context The context to set the IV to
 * @param iv      The initialization vector to set
 * @param len     The len of the IV
 */
void purple_cipher_context_set_iv(PurpleCipherContext *context, guchar *iv, size_t len);

/**
 * Appends data to the context
 *
 * @param context The context to append data to
 * @param data    The data to append
 * @param len     The length of the data
 */
void purple_cipher_context_append(PurpleCipherContext *context, const guchar *data, size_t len);

/**
 * Digests a context
 *
 * @param context The context to digest
 * @param digest  The return buffer for the digest
 * @param len     The length of the buffer
 */
gboolean purple_cipher_context_digest(PurpleCipherContext *context, guchar digest[], size_t len);

/**
 * Converts a guchar digest into a hex string
 *
 * @param context  The context to get a digest from
 * @param digest_s The return buffer for the string digest
 * @param len      The length of the buffer
 */
gboolean purple_cipher_context_digest_to_str(PurpleCipherContext *context, gchar digest_s[], size_t len);

/**
 * Gets the digest size of a context
 *
 * @param context The context whose digest size to get
 *
 * @return The digest size of the context
 */
size_t purple_cipher_context_get_digest_size(PurpleCipherContext *context);

/**
 * Encrypts data using the context
 *
 * @param context  The context
 * @param input    The data to encrypt
 * @param in_len   The length of the data
 * @param output   The output buffer
 * @param out_size The size of the output buffer
 *
 * @return A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_context_encrypt(PurpleCipherContext *context, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * Decrypts data using the context
 *
 * @param context  The context
 * @param input    The data to encrypt
 * @param in_len   The length of the returned value
 * @param output   The output buffer
 * @param out_size The size of the output buffer
 *
 * @return A length of data that was outputed or -1, if failed
 */
ssize_t purple_cipher_context_decrypt(PurpleCipherContext *context, const guchar input[], size_t in_len, guchar output[], size_t out_size);

/**
 * Sets the salt on a context
 *
 * @param context The context whose salt to set
 * @param salt    The salt
 * @param len     The length of the salt
 */
void purple_cipher_context_set_salt(PurpleCipherContext *context, const guchar *salt, size_t len);

/**
 * Gets the size of the salt if the cipher supports it
 *
 * @param context The context whose salt size to get
 *
 * @return The size of the salt
 */
size_t purple_cipher_context_get_salt_size(PurpleCipherContext *context);

/**
 * Sets the key on a context
 *
 * @param context The context whose key to set
 * @param key     The key
 * @param len     The size of the key
 */
void purple_cipher_context_set_key(PurpleCipherContext *context, const guchar *key, size_t len);

/**
 * Gets the size of the key if the cipher supports it
 *
 * @param context The context whose key size to get
 *
 * @return The size of the key
 */
size_t purple_cipher_context_get_key_size(PurpleCipherContext *context);

/**
 * Sets the batch mode of a context
 *
 * @param context The context whose batch mode to set
 * @param mode    The batch mode under which the cipher should operate
 *
 */
void purple_cipher_context_set_batch_mode(PurpleCipherContext *context, PurpleCipherBatchMode mode);

/**
 * Gets the batch mode of a context
 *
 * @param context The context whose batch mode to get
 *
 * @return The batch mode under which the cipher is operating
 */
PurpleCipherBatchMode purple_cipher_context_get_batch_mode(PurpleCipherContext *context);

/**
 * Gets the block size of a context
 *
 * @param context The context whose block size to get
 *
 * @return The block size of the context
 */
size_t purple_cipher_context_get_block_size(PurpleCipherContext *context);

/**
 * Sets the cipher data for a context
 *
 * @param context The context whose cipher data to set
 * @param data    The cipher data to set
 */
void purple_cipher_context_set_data(PurpleCipherContext *context, gpointer data);

/**
 * Gets the cipher data for a context
 *
 * @param context The context whose cipher data to get
 *
 * @return The cipher data
 */
gpointer purple_cipher_context_get_data(PurpleCipherContext *context);

/*@}*/
/*****************************************************************************/
/** @name Purple Cipher HTTP Digest Helper Functions							 */
/*****************************************************************************/
/*@{*/

/**
 * Calculates a session key for HTTP Digest authentation
 *
 * See RFC 2617 for more information.
 *
 * @param algorithm    The hash algorithm to use
 * @param username     The username provided by the user
 * @param realm        The authentication realm provided by the server
 * @param password     The password provided by the user
 * @param nonce        The nonce provided by the server
 * @param client_nonce The nonce provided by the client
 *
 * @return The session key, or @c NULL if an error occurred.
 */
gchar *purple_cipher_http_digest_calculate_session_key(
		const gchar *algorithm, const gchar *username,
		const gchar *realm, const gchar *password,
		const gchar *nonce, const gchar *client_nonce);

/** Calculate a response for HTTP Digest authentication
 *
 * See RFC 2617 for more information.
 *
 * @param algorithm         The hash algorithm to use
 * @param method            The HTTP method in use
 * @param digest_uri        The URI from the initial request
 * @param qop               The "quality of protection"
 * @param entity            The entity body
 * @param nonce             The nonce provided by the server
 * @param nonce_count       The nonce count
 * @param client_nonce      The nonce provided by the client
 * @param session_key       The session key from purple_cipher_http_digest_calculate_session_key()
 *
 * @return The hashed response, or @c NULL if an error occurred.
 */
gchar *purple_cipher_http_digest_calculate_response(
		const gchar *algorithm, const gchar *method,
		const gchar *digest_uri, const gchar *qop,
		const gchar *entity, const gchar *nonce,
		const gchar *nonce_count, const gchar *client_nonce,
		const gchar *session_key);

/*@}*/

G_END_DECLS

#endif /* PURPLE_CIPHER_H */
