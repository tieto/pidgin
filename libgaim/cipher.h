/**
 * @file cipher.h Gaim Cipher API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef GAIM_CIPHER_H
#define GAIM_CIPHER_H

#include <glib.h>

#define GAIM_CIPHER(obj)			((GaimCipher *)(obj))			/**< GaimCipher typecast helper			*/
#define GAIM_CIPHER_OPS(obj)		((GaimCipherOps *)(obj))		/**< GaimCipherInfo typecase helper		*/
#define GAIM_CIPHER_CONTEXT(obj)	((GaimCipherContext *)(obj))	/**< GaimCipherContext typecast helper	*/

typedef struct _GaimCipher			GaimCipher;			/**< A handle to a GaimCipher	*/
typedef struct _GaimCipherOps		GaimCipherOps;		/**< Ops for a GaimCipher		*/
typedef struct _GaimCipherContext	GaimCipherContext;	/**< A context for a GaimCipher	*/


/**
 * The operation flags for a cipher
 */
typedef enum _GaimCipherCaps {
	GAIM_CIPHER_CAPS_SET_OPT			= 1 << 1,		/**< Set option flag	*/
	GAIM_CIPHER_CAPS_GET_OPT			= 1 << 2,		/**< Get option flag	*/
	GAIM_CIPHER_CAPS_INIT				= 1 << 3,		/**< Init flag			*/
	GAIM_CIPHER_CAPS_RESET				= 1 << 4,		/**< Reset flag			*/
	GAIM_CIPHER_CAPS_UNINIT				= 1 << 5,		/**< Uninit flag		*/
	GAIM_CIPHER_CAPS_SET_IV				= 1 << 6,		/**< Set IV flag		*/
	GAIM_CIPHER_CAPS_APPEND				= 1 << 7,		/**< Append flag		*/
	GAIM_CIPHER_CAPS_DIGEST				= 1 << 8,		/**< Digest flag		*/
	GAIM_CIPHER_CAPS_ENCRYPT			= 1 << 9,		/**< Encrypt flag		*/
	GAIM_CIPHER_CAPS_DECRYPT			= 1 << 10,		/**< Decrypt flag		*/
	GAIM_CIPHER_CAPS_SET_SALT			= 1 << 11,		/**< Set salt flag		*/
	GAIM_CIPHER_CAPS_GET_SALT_SIZE		= 1 << 12,		/**< Get salt size flag	*/
	GAIM_CIPHER_CAPS_SET_KEY			= 1 << 13,		/**< Set key flag		*/
	GAIM_CIPHER_CAPS_GET_KEY_SIZE		= 1 << 14,		/**< Get key size flag	*/
	GAIM_CIPHER_CAPS_UNKNOWN			= 1 << 16		/**< Unknown			*/
} GaimCipherCaps;

/**
 * The operations of a cipher.  Every cipher must implement one of these.
 */
struct _GaimCipherOps {
	/** The set option function	*/
	void (*set_option)(GaimCipherContext *context, const gchar *name, void *value);

	/** The get option function */
	void *(*get_option)(GaimCipherContext *context, const gchar *name);

	/** The init function */
	void (*init)(GaimCipherContext *context, void *extra);

	/** The reset function */
	void (*reset)(GaimCipherContext *context, void *extra);

	/** The uninit function */
	void (*uninit)(GaimCipherContext *context);

	/** The set initialization vector function */
	void (*set_iv)(GaimCipherContext *context, guchar *iv, size_t len);

	/** The append data function */
	void (*append)(GaimCipherContext *context, const guchar *data, size_t len);

	/** The digest function */
	gboolean (*digest)(GaimCipherContext *context, size_t in_len, guchar digest[], size_t *out_len);

	/** The encrypt function */
	int (*encrypt)(GaimCipherContext *context, const guchar data[], size_t len, guchar output[], size_t *outlen);

	/** The decrypt function */
	int (*decrypt)(GaimCipherContext *context, const guchar data[], size_t len, guchar output[], size_t *outlen);

	/** The set salt function */
	void (*set_salt)(GaimCipherContext *context, guchar *salt);

	/** The get salt size function */
	size_t (*get_salt_size)(GaimCipherContext *context);

	/** The set key function */
	void (*set_key)(GaimCipherContext *context, const guchar *key);

	/** The get key size function */
	size_t (*get_key_size)(GaimCipherContext *context);
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*****************************************************************************/
/** @name GaimCipher API													 */
/*****************************************************************************/
/*@{*/

/**
 * Gets a cipher's name
 *
 * @param cipher The cipher handle
 *
 * @return The cipher's name
 */
const gchar *gaim_cipher_get_name(GaimCipher *cipher);

/**
 * Gets a cipher's capabilities
 *
 * @param cipher The cipher handle
 *
 * @return The cipher's info
 */
guint gaim_cipher_get_capabilities(GaimCipher *cipher);

/**
 * Gets a digest from a cipher
 *
 * @param name     The cipher's name
 * @param data     The data to hash
 * @param data_len The length of the data
 * @param in_len   The length of the buffer
 * @param digest   The returned digest
 * @param out_len  The length written
 *
 * @return @c TRUE if successful, @c FALSE otherwise
 */
gboolean gaim_cipher_digest_region(const gchar *name, const guchar *data, size_t data_len, size_t in_len, guchar digest[], size_t *out_len);

/*@}*/
/******************************************************************************/
/** @name GaimCiphers API													  */
/******************************************************************************/
/*@{*/

/**
 * Finds a cipher by it's name
 *
 * @param name The name of the cipher to find
 *
 * @return The cipher handle or @c NULL
 */
GaimCipher *gaim_ciphers_find_cipher(const gchar *name);

/**
 * Registers a cipher as a usable cipher
 *
 * @param name The name of the new cipher
 * @param ops  The cipher ops to register
 *
 * @return The handle to the new cipher or @c NULL if it failed
 */
GaimCipher *gaim_ciphers_register_cipher(const gchar *name, GaimCipherOps *ops);

/**
 * Unregisters a cipher
 *
 * @param cipher The cipher handle to unregister
 *
 * @return Whether or not the cipher was successfully unloaded
 */
gboolean gaim_ciphers_unregister_cipher(GaimCipher *cipher);

/**
 * Gets the list of ciphers
 *
 * @return The list of available ciphers
 * @note This list should not be modified, it is owned by the cipher core
 */
GList *gaim_ciphers_get_ciphers(void);

/*@}*/
/******************************************************************************/
/** @name GaimCipher Subsystem API											  */
/******************************************************************************/
/*@{*/

/**
 * Gets the handle to the cipher subsystem
 *
 * @return The handle to the cipher subsystem
 */
gpointer gaim_ciphers_get_handle(void);

/**
 * Initializes the cipher core
 */
void gaim_ciphers_init(void);

/**
 * Uninitializes the cipher core
 */
void gaim_ciphers_uninit(void);

/*@}*/
/******************************************************************************/
/** @name GaimCipherContext API												  */
/******************************************************************************/
/*@{*/

/**
 * Sets the value an option on a cipher context
 *
 * @param context The cipher context
 * @param name    The name of the option
 * @param value   The value to set
 */
void gaim_cipher_context_set_option(GaimCipherContext *context, const gchar *name, gpointer value);

/**
 * Gets the vale of an option on a cipher context
 *
 * @param context The cipher context
 * @param name    The name of the option
 * @return The value of the option
 */
gpointer gaim_cipher_context_get_option(GaimCipherContext *context, const gchar *name);

/**
 * Creates a new cipher context and initializes it
 *
 * @param cipher The cipher to use
 * @param extra  Extra data for the specific cipher
 *
 * @return The new cipher context
 */
GaimCipherContext *gaim_cipher_context_new(GaimCipher *cipher, void *extra);

/**
 * Creates a new cipher context by the cipher name and initializes it
 *
 * @param name  The cipher's name
 * @param extra Extra data for the specific cipher
 *
 * @return The new cipher context
 */
GaimCipherContext *gaim_cipher_context_new_by_name(const gchar *name, void *extra);

/**
 * Resets a cipher context to it's default value
 * @note If you have set an IV you will have to set it after resetting
 *
 * @param context The context to reset
 * @param extra   Extra data for the specific cipher
 */
void gaim_cipher_context_reset(GaimCipherContext *context, gpointer extra);

/**
 * Destorys a cipher context and deinitializes it
 *
 * @param context The cipher context to destory
 */
void gaim_cipher_context_destroy(GaimCipherContext *context);

/**
 * Sets the initialization vector for a context
 * @note This should only be called right after a cipher context is created or reset
 *
 * @param context The context to set the IV to
 * @param iv      The initialization vector to set
 * @param len     The len of the IV
 */
void gaim_cipher_context_set_iv(GaimCipherContext *context, guchar *iv, size_t len);

/**
 * Appends data to the context
 *
 * @param context The context to append data to
 * @param data    The data to append
 * @param len     The length of the data
 */
void gaim_cipher_context_append(GaimCipherContext *context, const guchar *data, size_t len);

/**
 * Digests a context
 *
 * @param context The context to digest
 * @param in_len  The length of the buffer
 * @param digest  The return buffer for the digest
 * @param out_len The length of the returned value
 */
gboolean gaim_cipher_context_digest(GaimCipherContext *context, size_t in_len, guchar digest[], size_t *out_len);

/**
 * Converts a guchar digest into a hex string
 *
 * @param context  The context to get a digest from
 * @param in_len   The length of the buffer
 * @param digest_s The return buffer for the string digest
 * @param out_len  The length of the returned value
 */
gboolean gaim_cipher_context_digest_to_str(GaimCipherContext *context, size_t in_len, gchar digest_s[], size_t *out_len);

/**
 * Encrypts data using the context
 *
 * @param context The context
 * @param data    The data to encrypt
 * @param len     The length of the data
 * @param output  The output buffer
 * @param outlen  The len of data that was outputed
 *
 * @return A cipher specific status code
 */
gint gaim_cipher_context_encrypt(GaimCipherContext *context, const guchar data[], size_t len, guchar output[], size_t *outlen);

/**
 * Decrypts data using the context
 *
 * @param context The context
 * @param data    The data to encrypt
 * @param len     The length of the returned value
 * @param output  The output buffer
 * @param outlen  The len of data that was outputed
 *
 * @return A cipher specific status code
 */
gint gaim_cipher_context_decrypt(GaimCipherContext *context, const guchar data[], size_t len, guchar output[], size_t *outlen);

/**
 * Sets the salt on a context
 *
 * @param context The context who's salt to set
 * @param salt    The salt
 */
void gaim_cipher_context_set_salt(GaimCipherContext *context, guchar *salt);

/**
 * Gets the size of the salt if the cipher supports it
 *
 * @param context The context who's salt size to get
 *
 * @return The size of the salt
 */
size_t gaim_cipher_context_get_salt_size(GaimCipherContext *context);

/**
 * Sets the key on a context
 *
 * @param context The context who's key to set
 * @param key     The key
 */
void gaim_cipher_context_set_key(GaimCipherContext *context, const guchar *key);

/**
 * Gets the key size for a context
 *
 * @param context The context who's key size to get
 *
 * @return The size of the key
 */
size_t gaim_cipher_context_get_key_size(GaimCipherContext *context);

/**
 * Sets the cipher data for a context
 *
 * @param context The context who's cipher data to set
 * @param data    The cipher data to set
 */
void gaim_cipher_context_set_data(GaimCipherContext *context, gpointer data);

/**
 * Gets the cipher data for a context
 *
 * @param context The context who's cipher data to get
 *
 * @return The cipher data
 */
gpointer gaim_cipher_context_get_data(GaimCipherContext *context);

/*@}*/
/*****************************************************************************/
/** @name Gaim Cipher HTTP Digest Helper Functions							 */
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
gchar *gaim_cipher_http_digest_calculate_session_key(
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
 * @param session_key       The session key from gaim_cipher_http_digest_calculate_session_key()
 *
 * @return The hashed response, or @c NULL if an error occurred.
 */
gchar *gaim_cipher_http_digest_calculate_response(
		const gchar *algorithm, const gchar *method,
		const gchar *digest_uri, const gchar *qop,
		const gchar *entity, const gchar *nonce,
		const gchar *nonce_count, const gchar *client_nonce,
		const gchar *session_key);

/*@}*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GAIM_CIPHER_H */
