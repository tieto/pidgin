/*
 *					MXit Protocol libPurple Plugin
 *
 *					-- user password encryption --
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


/* password encryption */
#define		INITIAL_KEY		"6170383452343567"
#define		SECRET_HEADER	"<mxit/>"
#define		ENCRYPT_HEADER	"<mxitencrypted ver=\"5.2\"/>"


/*------------------------------------------------------------------------
 * Pad the secret data using ISO10126 Padding.
 *
 *  @param secret	The data to pad (caller must ensure buffer has enough space for padding)
 *  @return			The total number of 128-bit blocks used
 */
static int pad_secret_data( char* secret )
{
	int		blocks	= 0;
	int		passlen;
	int		padding;

	passlen = strlen( secret );
	blocks = ( passlen / 16 ) + 1;
	padding = ( blocks * 16 ) - passlen;
	secret[passlen] = 0x50;
	secret[(blocks * 16) - 1] = padding;

	return blocks;
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
	char		key[64];
	char		exkey[512];
	char		pass[64];
	char		encrypted[64];
	char*		base64;
	int			blocks;
	int			size;
	int			i;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_encrypt_password\n" );

	memset( encrypted, 0x00, sizeof( encrypted ) );
	memset( exkey, 0x00, sizeof( exkey ) );
	memset( pass, 0x58, sizeof( pass ) );
	pass[sizeof( pass ) - 1] = '\0';

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
	base64 = purple_base64_encode( (unsigned char*) encrypted, size );

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

	/* remove ISO10126 padding */
	{
		/* last byte indicates the number of padding bytes */
		unsigned int padding = decoded->str[decoded->len - 1];
		g_string_truncate( decoded, decoded->len - padding );
	}

	/* remove encryption header */
	g_string_erase( decoded, 0, strlen( SECRET_HEADER ) );

	return g_string_free( decoded, FALSE );
}
