/*
 *					MXit Protocol libPurple Plugin
 *
 *			-- handle chunked data (multimedia messages) --
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

#include	"protocol.h"
#include	"mxit.h"
#include	"chunk.h"
#include	"filexfer.h"


/*========================================================================================================================
 * Data-Type encoding
 */

#if	0
#include	<byteswap.h>
#if (__BYTE_ORDER == __BIG_ENDIAN)
#define SWAP_64(x)  (x)
#else
#define SWAP_64(x)  bswap_64(x)
#endif
#endif

/*------------------------------------------------------------------------
 * Encode a single byte in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param value			The byte
 *  @return					The number of bytes added.
 */
static int add_int8( char* chunkdata, char value )
{
	*chunkdata = value;

	return sizeof( char );
}

/*------------------------------------------------------------------------
 * Encode a 16-bit value in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param value			The 16-bit value
 *  @return					The number of bytes added.
 */
static int add_int16( char* chunkdata, short value )
{
	value = htons( value );		/* network byte-order */
	memcpy( chunkdata, &value, sizeof( short ) );

	return sizeof( short );
}

/*------------------------------------------------------------------------
 * Encode a 32-bit value in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param value			The 32-bit value
 *  @return					The number of bytes added.
 */
static int add_int32( char* chunkdata, int value )
{
	value = htonl( value );		/* network byte-order */
	memcpy( chunkdata, &value, sizeof( int ) );

	return sizeof( int );
}

#if	0
/*------------------------------------------------------------------------
 * Encode a 64-bit value in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param value			The 64-bit value
 *  @return					The number of bytes added.
 */
static int add_int64( char* chunkdata, int64_t value )
{
	value = SWAP_64( value );	/* network byte-order */
	memcpy( chunkdata, &value, sizeof( int64_t ) );

	return sizeof( int64_t );
}
#endif

/*------------------------------------------------------------------------
 * Encode a block of data in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param data				The data to add
 *  @param datalen			The length of the data to add
 *  @return					The number of bytes added.
 */
static int add_data( char* chunkdata, const char* data, int datalen )
{
	memcpy( chunkdata, data, datalen );

	return datalen;
}

/*------------------------------------------------------------------------
 * Encode a string as UTF-8 in the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param str				The string to encode
 *  @return					The number of bytes in the string
 */
static int add_utf8_string( char* chunkdata, const char* str )
{
	int		pos		= 0;
	size_t	len		= strlen( str );

	/* utf8 string length [2 bytes] */
	pos += add_int16( &chunkdata[pos], len );

	/* utf8 string */
	pos += add_data( &chunkdata[pos], str, len );

	return pos;
}


/*========================================================================================================================
 * Data-Type decoding
 */

/*------------------------------------------------------------------------
 * Extract a single byte from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param value			The byte
 *  @return					The number of bytes extracted.
 */
static int get_int8( const char* chunkdata, size_t chunklen, char* value )
{
	if ( chunklen < sizeof( char ) )
		return 0;

	*value = *chunkdata;

	return sizeof( char );
}

/*------------------------------------------------------------------------
 * Extract a 16-bit value from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param value			The 16-bit value
 *  @return					The number of bytes extracted
 */
static int get_int16( const char* chunkdata, size_t chunklen, unsigned short* value )
{
	if ( chunklen < sizeof( short ) )
		return 0;

	*value = ntohs( *( (const short*) chunkdata ) );	/* host byte-order */

	return sizeof( short );
}

/*------------------------------------------------------------------------
 * Extract a 32-bit value from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param value			The 32-bit value
 *  @return					The number of bytes extracted
 */
static int get_int32( const char* chunkdata, size_t chunklen, unsigned int* value )
{
	if ( chunklen < sizeof( int ) )
		return 0;

	*value = ntohl( *( (const int*) chunkdata ) );	/* host byte-order */

	return sizeof( int );
}

#if	0
/*------------------------------------------------------------------------
 * Extract a 64-bit value from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param value			The 64-bit value
 *  @return					The number of bytes extracted
 */
static int get_int64( const char* chunkdata, size_t chunklen, int64_t* value )
{
	if ( chunklen < sizeof( int64_t ) )
		return 0;

	*value = SWAP_64( *( (const int64_t*) chunkdata ) );	/* host byte-order */

	return sizeof( int64_t );
}
#endif

/*------------------------------------------------------------------------
 * Copy a block of data from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param dest				Where to store the extract data
 *  @param datalen			The length of the data to extract
 *  @return					The number of bytes extracted
 */
static int get_data( const char* chunkdata, size_t chunklen, char* dest, size_t datalen )
{
	if ( chunklen < datalen )
		return 0;

	memcpy( dest, chunkdata, datalen );

	return datalen;
}

/*------------------------------------------------------------------------
 * Extract a UTF-8 encoded string from the chunked data.
 *
 *  @param chunkdata		The chunked-data buffer
 *  @param chunklen			The amount of data available in the buffer.
 *  @param str				A pointer to extracted string.  Must be g_free()'d.
 *  @param maxstrlen		Maximum size of destination buffer.
 *  @return					The number of bytes consumed
 */
static int get_utf8_string( const char* chunkdata, size_t chunklen, char* str, size_t maxstrlen )
{
	size_t			pos = 0;
	unsigned short	len = 0;
	size_t			skip = 0;

	/* string length [2 bytes] */
	pos += get_int16( &chunkdata[pos], chunklen - pos, &len );

	if ( ( len + pos ) > chunklen ) {
		/* string length is longer than chunk size */
		return 0;
	}
	else if ( len > maxstrlen ) {
		/* possible buffer overflow */
		purple_debug_error( MXIT_PLUGIN_ID, "Buffer overflow detected (get_utf8_string)\n" );
		skip = len - maxstrlen;
		len = maxstrlen;
	}

	/* string data */
	pos += get_data( &chunkdata[pos], chunklen - pos, str, len );
	str[len] = '\0';		/* terminate string */

	return pos + skip;
}


/*========================================================================================================================
 * Chunked Data encoding
 */

/*------------------------------------------------------------------------
 * Encode a "reject file" chunk.  (Chunk type 7)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param fileid			A unique ID that identifies this file
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_reject( char* chunkdata, const char* fileid )
{
	size_t	pos		= 0;

	/* file id [8 bytes] */
	pos += add_data( &chunkdata[pos], fileid, MXIT_CHUNK_FILEID_LEN );

	/* rejection reason [1 byte] */
	pos += add_int8( &chunkdata[pos], REJECT_BY_USER );

	/* rejection description [UTF-8 (optional)] */
	pos += add_utf8_string( &chunkdata[pos], "" );

	return pos;
}


/*------------------------------------------------------------------------
 * Encode a "get file" request chunk.  (Chunk type 8)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param fileid			A unique ID that identifies this file
 *  @param filesize			The number of bytes to retrieve
 *  @param offset			The start offset in the file
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_get( char* chunkdata, const char* fileid, size_t filesize, size_t offset )
{
	size_t	pos		= 0;

	/* file id [8 bytes] */
	pos += add_data( &chunkdata[pos], fileid, MXIT_CHUNK_FILEID_LEN );

	/* offset [4 bytes] */
	pos += add_int32( &chunkdata[pos], offset );

	/* length [4 bytes] */
	pos += add_int32( &chunkdata[pos], filesize );

	return pos;
}


/*------------------------------------------------------------------------
 * Encode a "received file" chunk.  (Chunk type 9)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param fileid			A unique ID that identifies this file
 *  @param status			The status of the file transfer (see chunk.h)
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_received( char* chunkdata, const char* fileid, unsigned char status )
{
	size_t	pos		= 0;

	/* file id [8 bytes] */
	pos += add_data( &chunkdata[pos], fileid, MXIT_CHUNK_FILEID_LEN );

	/* status [1 byte] */
	pos += add_int8( &chunkdata[pos], status );

	return pos;
}


/*------------------------------------------------------------------------
 * Encode a "send file direct" chunk.  (Chunk type 10)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param username			The username of the recipient
 *  @param filename			The name of the file being sent
 *  @param data				The file contents
 *  @param datalen			The size of the file contents
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_senddirect( char* chunkdata, const char* username, const char* filename, const unsigned char* data, size_t datalen )
{
	size_t		pos		= 0;
	const char*	mime	= NULL;

	/* data length [4 bytes] */
	pos += add_int32( &chunkdata[pos], datalen );

	/* number of username(s) [2 bytes] */
	pos += add_int16( &chunkdata[pos], 1 );

	/* username(s) [UTF-8] */
	pos += add_utf8_string( &chunkdata[pos], username );

	/* filename [UTF-8] */
	pos += add_utf8_string( &chunkdata[pos], filename );

	/* file mime type [UTF-8] */
	mime = file_mime_type( filename, (const char*) data, datalen );
	pos += add_utf8_string( &chunkdata[pos], mime );

	/* human readable description [UTF-8 (optional)] */
	pos += add_utf8_string( &chunkdata[pos], "" );

	/* crc [4 bytes] (0 = optional) */
	pos += add_int32( &chunkdata[pos], 0 );

	/* the actual file data */
	pos += add_data( &chunkdata[pos], (const char *) data, datalen );

	return pos;
}


/*------------------------------------------------------------------------
 * Encode a "set avatar" chunk.  (Chunk type 13)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param data				The avatar data
 *  @param datalen			The size of the avatar data
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_set_avatar( char* chunkdata, const unsigned char* data, size_t datalen )
{
	char	fileid[MXIT_CHUNK_FILEID_LEN];
	size_t	pos = 0;

	/* id [8 bytes] */
	memset( &fileid, 0, sizeof( fileid ) );		/* set to 0 for file upload */
	pos += add_data( &chunkdata[pos], fileid, MXIT_CHUNK_FILEID_LEN );

	/* size [4 bytes] */
	pos += add_int32( &chunkdata[pos], datalen );

	/* crc [4 bytes] (0 = optional) */
	pos += add_int32( &chunkdata[pos], 0 );

	/* the actual file data */
	pos += add_data( &chunkdata[pos], (const char *) data, datalen );

	return pos;
}


/*------------------------------------------------------------------------
 * Encode a "get avatar" chunk.  (Chunk type 14)
 *
 *  @param chunkdata		Chunked-data buffer
 *  @param mxitId			The username who's avatar to download
 *  @param avatarId			The Id of the avatar image (as string)
 *  @return					The number of bytes encoded in the buffer
 */
size_t mxit_chunk_create_get_avatar( char* chunkdata, const char* mxitId, const char* avatarId )
{
	size_t	pos = 0;

	/* number of avatars [4 bytes] */
	pos += add_int32( &chunkdata[pos], 1 );

	/* username [UTF-8] */
	pos += add_utf8_string( &chunkdata[pos], mxitId );

	/* avatar id [UTF-8] */
	pos += add_utf8_string( &chunkdata[pos], avatarId );

	/* avatar format [UTF-8] */
	pos += add_utf8_string( &chunkdata[pos], MXIT_AVATAR_TYPE );

	/* avatar bit depth [1 byte] */
	pos += add_int8( &chunkdata[pos], MXIT_AVATAR_BITDEPT );

	/* number of sizes [2 bytes] */
	pos += add_int16( &chunkdata[pos], 1 );

	/* image size [4 bytes] */
	pos += add_int32( &chunkdata[pos], MXIT_AVATAR_SIZE );

	return pos;
}


/*========================================================================================================================
 * Chunked Data decoding
 */

/*------------------------------------------------------------------------
 * Parse a received "offer file" chunk.  (Chunk 6)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param offer			Decoded offerfile information
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_offer( char* chunkdata, size_t datalen, struct offerfile_chunk* offer )
{
	size_t pos = 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_offer (%zu bytes)\n", datalen );

	memset( offer, 0, sizeof( struct offerfile_chunk ) );

	/* id [8 bytes] */
	pos += get_data( &chunkdata[pos], datalen - pos, offer->fileid, 8);

	/* from username [UTF-8] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, offer->username, sizeof( offer->username ) );
	mxit_strip_domain( offer->username );

	/* file size [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(offer->filesize) );

	/* filename [UTF-8] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, offer->filename, sizeof( offer->filename ) );

	/* mime type [UTF-8] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, offer->mimetype, sizeof( offer->mimetype ) );

	/* timestamp [8 bytes] */
	/* not used by libPurple */

	/* file description [UTF-8] */
	/* not used by libPurple */

	/* file alternative [UTF-8] */
	/* not used by libPurple */

	/* flags [4 bytes] */
	/* not used by libPurple */

	return TRUE;
}


/*------------------------------------------------------------------------
 * Parse a received "get file" response chunk.  (Chunk 8)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param offer			Decoded getfile information
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_get( char* chunkdata, size_t datalen, struct getfile_chunk* getfile )
{
	size_t pos = 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_file (%zu bytes)\n", datalen );

	memset( getfile, 0, sizeof( struct getfile_chunk ) );

	/* ensure that the chunk size is atleast the minimum size for a "get file" chunk */
	if ( datalen < 20 )
		return FALSE;

	/* id [8 bytes] */
	pos += get_data( &chunkdata[pos], datalen - pos, getfile->fileid, 8 );

	/* offset [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(getfile->offset) );

	/* file length [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(getfile->length) );

	/* crc [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(getfile->crc) );

	/* check length does not exceed chunked data length */
	if ( getfile->length > datalen - pos )
		return FALSE;

	/* file data */
	if ( getfile->length > 0 )
		getfile->data = &chunkdata[pos];

	return TRUE;
}


/*------------------------------------------------------------------------
 * Parse a received splash screen chunk.  (Chunk 2)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param splash			Decoded splash image information
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_splash( char* chunkdata, size_t datalen, struct splash_chunk* splash )
{
	size_t pos = 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_splash (%zu bytes)\n", datalen );

	memset( splash, 0, sizeof( struct splash_chunk ) );

	/* ensure that the chunk size is atleast the minimum size for a "splash screen" chunk */
	if ( datalen < 6 )
		return FALSE;

	/* anchor [1 byte] */
	pos += get_int8( &chunkdata[pos], datalen - pos, &(splash->anchor) );

	/* time to show [1 byte] */
	pos += get_int8( &chunkdata[pos], datalen - pos, &(splash->showtime) );

	/* background color [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(splash->bgcolor) );

	/* file data */
	if ( pos < datalen )
		splash->data = &chunkdata[pos];

	/* data length */
	splash->datalen = datalen - pos;

	return TRUE;
}


/*------------------------------------------------------------------------
 * Parse a received "custom resource" chunk.  (Chunk 1)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param offer			Decoded custom resource
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_cr( char* chunkdata, size_t datalen, struct cr_chunk* cr )
{
	size_t			pos			= 0;
	unsigned int	chunkslen	= 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_cr (%zu bytes)\n", datalen );

	memset( cr, 0, sizeof( struct cr_chunk ) );

	/* id [UTF-8] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, cr->id, sizeof( cr->id ) );

	/* handle [UTF-8] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, cr->handle, sizeof( cr->handle ) );

	/* operation [1 byte] */
	pos += get_int8( &chunkdata[pos], datalen - pos, &(cr->operation) );

	/* total length of all the chunks that are included [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &chunkslen );

	/* ensure the chunks size does not exceed the data size */
	if ( pos + chunkslen > datalen )
		return FALSE;

	/* parse the resource chunks */
	while ( chunkslen >= MXIT_CHUNK_HEADER_SIZE ) {
		gchar*	chunk		= &chunkdata[pos];
		guint32	chunksize	= chunk_length( chunk );

		/* check chunk size against length of received data */
		if ( pos + MXIT_CHUNK_HEADER_SIZE + chunksize > datalen )
			return FALSE;

		switch ( chunk_type( chunk ) ) {
			case CP_CHUNK_SPLASH :			/* splash image */
				{
					struct splash_chunk* splash = g_new0( struct splash_chunk, 1 );

					if ( mxit_chunk_parse_splash( chunk_data( chunk ), chunksize, splash ) )
						cr->resources = g_list_append( cr->resources, splash );
					else
						g_free( splash );
					break;
				}
			case CP_CHUNK_CLICK :			/* splash click */
				{
					struct splash_click_chunk* click = g_new0( struct splash_click_chunk, 1 );

					cr->resources = g_list_append( cr->resources, click );
					break;
				}
			default:
				purple_debug_info( MXIT_PLUGIN_ID, "Unsupported custom resource chunk received (%i)\n", chunk_type( chunk) );
		}

		/* skip over data to next resource chunk */
		pos += MXIT_CHUNK_HEADER_SIZE + chunksize;
		chunkslen -= ( MXIT_CHUNK_HEADER_SIZE + chunksize );
	}

	return TRUE;
}


/*------------------------------------------------------------------------
 * Parse a received "send file direct" response chunk.  (Chunk 10)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param sendfile			Decoded sendfile information
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_sendfile( char* chunkdata, size_t datalen, struct sendfile_chunk* sendfile )
{
	size_t			pos		= 0;
	unsigned short	entries	= 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_sendfile (%zu bytes)\n", datalen );

	memset( sendfile, 0, sizeof( struct sendfile_chunk ) );

	/* number of entries [2 bytes] */
	pos += get_int16( &chunkdata[pos], datalen - pos, &entries );

	if ( entries < 1 )		/* no data */
		return FALSE;

	/* contactAddress [UTF-8 string] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, sendfile->username, sizeof( sendfile->username ) );

	/* status [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(sendfile->status) );

	/* status message [UTF-8 string] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, sendfile->statusmsg, sizeof( sendfile->statusmsg ) );

	return TRUE;
}


/*------------------------------------------------------------------------
 * Parse a received "get avatar" response chunk.  (Chunk 14)
 *
 *  @param chunkdata		Chunked data buffer
 *  @param datalen			The length of the chunked data
 *  @param avatar			Decoded avatar information
 *  @return					TRUE if successfully parsed, otherwise FALSE
 */
gboolean mxit_chunk_parse_get_avatar( char* chunkdata, size_t datalen, struct getavatar_chunk* avatar )
{
	size_t			pos			= 0;
	unsigned int	numfiles	= 0;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_chunk_parse_get_avatar (%zu bytes)\n", datalen );

	memset( avatar, 0, sizeof( struct getavatar_chunk ) );

	/* number of files [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &numfiles );

	if ( numfiles < 1 )		/* no data */
		return FALSE;

	/* mxitId [UTF-8 string] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, avatar->mxitid, sizeof( avatar->mxitid ) );

	/* avatar id [UTF-8 string] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, avatar->avatarid, sizeof( avatar->avatarid ) );

	/* format [UTF-8 string] */
	pos += get_utf8_string( &chunkdata[pos], datalen - pos, avatar->format, sizeof( avatar->format ) );

	/* bit depth [1 byte] */
	pos += get_int8( &chunkdata[pos], datalen - pos, &(avatar->bitdepth) );

	/* crc [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(avatar->crc) );

	/* width [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(avatar->width) );

	/* height [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(avatar->height) );

	/* file length [4 bytes] */
	pos += get_int32( &chunkdata[pos], datalen - pos, &(avatar->length) );

	/* check length does not exceed chunked data length */
	if ( avatar->length > datalen - pos )
		return FALSE;

	/* file data */
	if ( avatar->length > 0 )
		avatar->data = &chunkdata[pos];

	return TRUE;
}
