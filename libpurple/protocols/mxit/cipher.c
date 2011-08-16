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

#include    "internal.h"
#include	"purple.h"

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
	int			passlen			= strlen( session->acc->password );

	/* initialize with initial key */
	g_strlcpy( key, INITIAL_KEY, sizeof( key ) );

	/* client key (8 bytes) */
	memcpy( key, session->clientkey, strlen( session->clientkey ) );

	/* add last 8 characters of the PIN (no padding if less characters) */
	if ( passlen <= 8 )
		memcpy( key + 8, session->acc->password, passlen );
	else
		memcpy( key + 8, session->acc->password + ( passlen - 8 ), 8 );

	return key;
}


/*------------------------------------------------------------------------
 * Encrypt the user's cleartext password using the AES 128-bit (ECB)
 *  encryption algorithm.
 *
 *  @param session	The MXit session object
 *  @return			The encrypted & encoded password.  Must be g_free'd when no longer needed.
 */
char* mxit_encrypt_password( struct MXitSession* session )
{
	char		key[16 + 1];
	char		exkey[512];
	GString*	pass			= NULL;
	char		encrypted[64];
	char*		base64;
	int			i;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_encrypt_password\n" );

	memset( encrypted, 0x00, sizeof( encrypted ) );

	/* build the custom AES encryption key */
	g_strlcpy( key, INITIAL_KEY, sizeof( key ) );
	memcpy( key, session->clientkey, strlen( session->clientkey ) );
	ExpandKey( (unsigned char*) key, (unsigned char*) exkey );

	/* build the custom data to be encrypted */
	g_strlcpy( pass, SECRET_HEADER, sizeof( pass ) );
	strcat( pass, session->acc->password );

	/* pad the secret data */
	blocks = pad_secret_data( pass );
	size = blocks * 16;

	/* now encrypt the password. we encrypt each block separately (ECB mode) */
	for ( i = 0; i < size; i += 16 )
		Encrypt( (unsigned char*) pass + i, (unsigned char*) exkey, (unsigned char*) encrypted + i );

	/* now base64 encode the encrypted password */
	base64 = purple_base64_encode( (unsigned char*) encrypted, pass->len );

	g_string_free( pass, TRUE );

	return base64;
}


/*------------------------------------------------------------------------
 * Decrypt a transport-layer encryptede message.
 *
 *  @param session	The MXit session object
 *	@param message	The encrypted message data.
 *  @return			The decrypted message.  Must be g_free'd when no longer needed.
 */
char* mxit_decrypt_message( struct MXitSession* session, char* message )
{
	gsize		raw_len;
	guchar*		raw_message;
	char		key[64];
	int			pwdlen		= strlen( session->acc->password );
	char		exkey[512];
	int			i;
	GString*	decoded		= NULL;

	/* remove optional header: <mxitencrypted ver="5.2"/> */
	if ( strncmp( message, ENCRYPT_HEADER, strlen( ENCRYPT_HEADER ) ) == 0 )
		message += strlen( ENCRYPT_HEADER );

	/* base64 decode the message */
	raw_message = purple_base64_decode( message, &raw_len );

	/* build the key - Client key, appended with last 8 characters of the PIN. (no padding) */
	memset( key, 0x00, sizeof( key ) );
	memcpy( key, session->clientkey, strlen( session->clientkey ) );
	if ( pwdlen <= 8 )
		strcat( key, session->acc->password );
	else
		strncat( key, session->acc->password + ( pwdlen - 8 ), 8 );
	ExpandKey( (unsigned char*) key, (unsigned char*) exkey );

	/* decode each block */
	decoded = g_string_sized_new( raw_len );
	for ( i = 0; i < raw_len; i += 16 ) {
		char	block[16];

		Decrypt( (unsigned char*) raw_message + i, (unsigned char*) exkey, (unsigned char*) block );
		g_string_append_len( decoded, block, 16 );
	}

	g_free( raw_message );

	purple_debug_info( MXIT_PLUGIN_ID, "decrypted: '%s'\n", decoded->str );

	/* check that the decrypted message starts with header: <mxit/> */
	if ( strncmp( decoded->str, SECRET_HEADER, strlen( SECRET_HEADER ) != 0 ) ) {
		g_string_free( decoded, TRUE );
		return NULL;			/* message could not be decoded */
	}
	g_string_erase( decoded, 0, strlen( SECRET_HEADER ) );		/* remove header */

	/* remove ISO10126 padding */
// TODO

	return g_string_free( decoded, FALSE );
}
