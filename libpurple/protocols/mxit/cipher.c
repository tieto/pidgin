/*
 *					MXit Protocol libPurple Plugin
 *
 *						-- encryption --
 *
 *				Pieter Loubser	<libpurple@mxit.com>
 *
 *			(C) Copyright 2009	MXit Lifestyle (Pty) Ltd.
 *				<http://www.mxitlifestyle.com>
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

#include	"internal.h"
#include	"debug.h"
#include "libpurple/cipher.h"
#include "ciphers/aescipher.h"

#include	"mxit.h"
#include	"cipher.h"
#include	"aes.h"


/* encryption */
#define		INITIAL_KEY		"6170383452343567"
#define		SECRET_HEADER	"<mxit/>"
#define		ENCRYPT_HEADER	"<mxitencrypted ver=\"5.2\"/>"


/*------------------------------------------------------------------------
 * Add ISO10126 Padding to the data.
 *
 *  @param data		The data to pad.
 */
static void padding_add( GString* data )
{
	unsigned int blocks = ( data->len / 16 ) + 1;
	unsigned int padding = ( blocks * 16 ) - data->len;

	g_string_set_size( data, blocks * 16 );
	data->str[data->len - 1] = padding;
}


/*------------------------------------------------------------------------
 * Remove ISO10126 Padding from the data.
 *
 *  @param data		The data from which to remove padding.
 */
static void padding_remove( GString* data )
{
	unsigned int padding;

	if ( data->len == 0 )
		return;

	padding = data->str[data->len - 1];
	g_string_truncate( data, data->len - padding );
}


/*------------------------------------------------------------------------
 * Generate the Transport-Layer crypto key.
 *  (Note: this function is not-thread safe)
 *
 *  @param session	The MXit Session object
 *	@return			The transport-layer crypto key.
 */
static char* transport_layer_key( struct MXitSession* session )
{
	static char	key[16 + 1];
	const char*	password		= purple_connection_get_password( session->con );
	int			passlen			= strlen( password );

	/* initialize with initial key */
	g_strlcpy( key, INITIAL_KEY, sizeof( key ) );

	/* client key (8 bytes) */
	memcpy( key, session->clientkey, strlen( session->clientkey ) );

	/* add last 8 characters of the PIN (no padding if less characters) */
	if ( passlen <= 8 )
		memcpy( key + 8, password, passlen );
	else
		memcpy( key + 8, password + ( passlen - 8 ), 8 );

	return key;
}


/*------------------------------------------------------------------------
 * Encrypt the user's cleartext password using the AES 128-bit (ECB)
 *  encryption algorithm.
 *
 *  @param session	The MXit session object
 *  @return			The encrypted & encoded password.  Must be g_free'd when no longer needed.
 */
gchar *
mxit_encrypt_password(struct MXitSession* session)
{
	guchar key[16];
	size_t clientkey_len, header_len, pass_len, plaintext_len;
	const gchar *plaintext_passwd;
	guchar *plaintext;
	guchar encrypted[64]; /* shouldn't be longer than 17 */
	PurpleCipher *cipher;
	ssize_t encrypted_size;

	purple_debug_info(MXIT_PLUGIN_ID, "mxit_encrypt_password");

	/* build the AES encryption key */
	g_assert(strlen(INITIAL_KEY) == sizeof(key));
	memcpy(key, INITIAL_KEY, sizeof(key));
	clientkey_len = strlen(session->clientkey);
	if (clientkey_len > sizeof(key))
		clientkey_len = sizeof(key);
	memcpy(key, session->clientkey, clientkey_len);

	/* build the secret data to be encrypted: SECRET_HEADER + password */
	plaintext_passwd = purple_connection_get_password(session->con);
	g_return_val_if_fail(plaintext_passwd, NULL);
	pass_len = strlen(plaintext_passwd);
	header_len = strlen(SECRET_HEADER);
	/* Trailing NUL, just to be safe. But PKCS#7 seems to be enough. */
	plaintext_len = header_len + pass_len + 1;
	plaintext = g_new0(guchar, plaintext_len);
	memcpy(plaintext, SECRET_HEADER, header_len);
	memcpy(plaintext + header_len, plaintext_passwd, pass_len);

	/* encrypt */
	cipher = purple_aes_cipher_new();
	purple_cipher_set_key(cipher, key, sizeof(key));
	purple_cipher_set_batch_mode(cipher, PURPLE_CIPHER_BATCH_MODE_ECB);
	encrypted_size = purple_cipher_encrypt(cipher,
		plaintext, plaintext_len, encrypted, sizeof(encrypted));
	g_return_val_if_fail(encrypted_size > 0, NULL);

	return purple_base64_encode(encrypted, encrypted_size);
}


/*------------------------------------------------------------------------
 * Decrypt a message using transport-layer encryption.
 *
 *  @param session	The MXit session object
 *	@param message	The encrypted message data (is base64-encoded).
 *  @return			The decrypted message.  Must be g_free'd when no longer needed.
 */
char* mxit_decrypt_message( struct MXitSession* session, char* message )
{
	guchar*			raw_message;
	gsize			raw_len;
	char			exkey[512];
	GString*		decoded		= NULL;
	unsigned int	i;

	/* remove optional header: <mxitencrypted ver="5.2"/> */
	if ( strncmp( message, ENCRYPT_HEADER, strlen( ENCRYPT_HEADER ) ) == 0 )
		message += strlen( ENCRYPT_HEADER );

	/* base64 decode the message */
	raw_message = purple_base64_decode( message, &raw_len );

	/* AES-encrypted data is always blocks of 16 bytes */
	if ( ( raw_len == 0 ) || ( raw_len % 16 != 0 ) )
		return NULL;

	/* build the AES key */
	ExpandKey( (unsigned char*) transport_layer_key( session ), (unsigned char*) exkey );

	/* AES decrypt each block */
	decoded = g_string_sized_new( raw_len );
	for ( i = 0; i < raw_len; i += 16 ) {
		char	block[16];

		Decrypt( (unsigned char*) raw_message + i, (unsigned char*) exkey, (unsigned char*) block );
		g_string_append_len( decoded, block, 16 );
	}
	g_free( raw_message );

	/* check that the decrypted message starts with header: <mxit/> */
	if ( strncmp( decoded->str, SECRET_HEADER, strlen( SECRET_HEADER ) != 0 ) ) {
		g_string_free( decoded, TRUE );
		return NULL;			/* message could not be decrypted */
	}

	/* remove ISO10126 padding */
	padding_remove( decoded );

	/* remove encryption header */
	g_string_erase( decoded, 0, strlen( SECRET_HEADER ) );

	return g_string_free( decoded, FALSE );
}


/*------------------------------------------------------------------------
 * Encrypt a message using transport-layer encryption.
 *
 *  @param session	The MXit session object
 *	@param message	The message data.
 *  @return			The encrypted message.  Must be g_free'd when no longer needed.
 */
char* mxit_encrypt_message( struct MXitSession* session, char* message )
{
	GString*		raw_message	= NULL;
	char			exkey[512];
	GString*		encoded		= NULL;
	gchar*			base64;
	unsigned int	i;

	purple_debug_info( MXIT_PLUGIN_ID, "encrypt message: '%s'\n", message );

	/* append encryption header to message data */
	raw_message = g_string_new( SECRET_HEADER );
	g_string_append( raw_message, message );
	padding_add( raw_message );		/* add ISO10126 padding */

	/* build the AES key */
	ExpandKey( (unsigned char*) transport_layer_key( session ), (unsigned char*) exkey );

	/* AES encrypt each block */
	encoded = g_string_sized_new( raw_message->len );
	for ( i = 0; i < raw_message->len; i += 16 ) {
		char	block[16];

		Encrypt( (unsigned char*) raw_message->str + i, (unsigned char*) exkey, (unsigned char*) block );
		g_string_append_len( encoded, block, 16 );
	}
	g_string_free( raw_message, TRUE );

	/* base64 encode the encrypted message */
	base64 = purple_base64_encode( (unsigned char *) encoded->str, encoded->len );
	g_string_free( encoded, TRUE );

	purple_debug_info( MXIT_PLUGIN_ID, "encrypted message: '%s'\n", base64 );

	return base64;
}
