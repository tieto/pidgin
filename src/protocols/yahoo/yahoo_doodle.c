/*
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
 *
 */

// INCLUDES ============================================================================================

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "cipher.h"
#include "cmds.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"

#include "yahoo.h"
#include "yahoo_packet.h"
#include "yahoo_friend.h"
#include "yahoochat.h"
#include "ycht.h"
#include "yahoo_auth.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"

#include "whiteboard.h"
#include "yahoo_doodle.h"

// GLOBALS =============================================================================================

const int DefaultColorRGB24[]	=
{
	DOODLE_COLOR_RED,
	DOODLE_COLOR_ORANGE,
	DOODLE_COLOR_YELLOW,
	DOODLE_COLOR_GREEN,
	DOODLE_COLOR_CYAN,
	DOODLE_COLOR_BLUE,
	DOODLE_COLOR_VIOLET,
	DOODLE_COLOR_PURPLE,
	DOODLE_COLOR_TAN,
	DOODLE_COLOR_BROWN,
	DOODLE_COLOR_BLACK,
	DOODLE_COLOR_GREY,
	DOODLE_COLOR_WHITE
};

// FUNCTIONS ============================================================================================

GaimCmdRet yahoo_doodle_gaim_cmd_start( GaimConversation *conv, const char *cmd, char **args, char **error, void *data )
{
	GaimAccount *account;
	GaimConnection *gc;
	char *to;
	GaimWhiteboard *wb;

	if( *args && args[0] )
		return( GAIM_CMD_RET_FAILED );
	
	account	= gaim_conversation_get_account( conv );
	gc		= gaim_account_get_connection( account );
	to		= ( char* )( gaim_conversation_get_name( conv ) );
	wb		= gaim_whiteboard_get_session( account, to );
	
	// NOTE Functionalize this code?
	
	if( wb == NULL )
	{
		// Insert this 'session' in the list.  At this point, it's only a requested session.	
		wb = gaim_whiteboard_create( account, to, DOODLE_STATE_REQUESTING );
	}
	//else
	//	; // NOTE Perhaps some careful handling of remote assumed established sessions
	
	yahoo_doodle_command_send_request( gc, to );
	yahoo_doodle_command_send_ready( gc, to );
	
	// Write a local message to this conversation showing that
	// a request for a Doodle session has been made
	gaim_conv_im_write( GAIM_CONV_IM( conv ), "", _("Sent Doodle request."),
			    GAIM_MESSAGE_NICK | GAIM_MESSAGE_RECV, time( NULL ) );
	
	return( GAIM_CMD_RET_OK );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_process( GaimConnection *gc, char *me, char *from, char *command, char *message )
{
//	g_print( "-----------------------------------------------\n" );
//	g_print( "%s : %s : %s -> %s\n", from, imv, command, message );
	
	// Now check to see what sort of Doodle message it is
	int cmd = atoi( command );

	switch( cmd )
	{
		case DOODLE_CMD_REQUEST:
		{
			yahoo_doodle_command_got_request( gc, from );
		} break;
		
		case DOODLE_CMD_READY:
		{
			yahoo_doodle_command_got_ready( gc, from );
		} break;
		
		case DOODLE_CMD_CLEAR:
		{
			yahoo_doodle_command_got_clear( gc, from );
		} break;
		
		case DOODLE_CMD_DRAW:
		{
			yahoo_doodle_command_got_draw( gc, from, message );
		} break;
		
		case DOODLE_CMD_EXTRA:
		{
			yahoo_doodle_command_got_extra( gc, from, message );
		} break;
		
		case DOODLE_CMD_CONFIRM:
		{
			yahoo_doodle_command_got_confirm( gc, from );
		} break;
	}
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_request( GaimConnection *gc, char *from )
{	
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got REQUEST (%s)\n", from );
	
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
	
	// If a session with the remote user doesn't exist
	if( wb == NULL )
	{
		// Ask user if he/she wishes to accept the request for a doodle session
		// TODO Ask local user to start Doodle session with remote user
		// NOTE This if/else statement won't work right--must use dialog results
		
		/*	char dialog_message[64];
		g_sprintf( dialog_message, "%s is requesting to start a Doodle session with you.", from );
			
		gaim_notify_message( NULL, GAIM_NOTIFY_MSG_INFO, "Doodle",
		dialog_message, NULL, NULL, NULL );
		*/
		
		wb = gaim_whiteboard_create( account, from, DOODLE_STATE_REQUESTED );
		
		yahoo_doodle_command_send_request( gc, from );
	}
	
	// TODO Might be required to clear the canvas of an existing doodle session at this point
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_ready( GaimConnection *gc, char *from )
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got READY (%s)\n", from );
	
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
							
	if( wb == NULL )
		return;
	
	if( wb->state == DOODLE_STATE_REQUESTING )
	{
		gaim_whiteboard_start( wb );
		
		wb->state = DOODLE_STATE_ESTABLISHED;
		
		yahoo_doodle_command_send_confirm( gc, from );
	}
	
	if( wb->state == DOODLE_STATE_ESTABLISHED )
	{
		// Ask whether to save picture too
		
		gaim_whiteboard_clear( wb );
	}
	
	// NOTE Not sure about this... I am trying to handle if the remote user already
	// thinks we're in a session with them (when their chat message contains the doodle;11 imv key)
	if( wb->state == DOODLE_STATE_REQUESTED )
	{
		g_print( "Hmmmm\n" );
		
		//gaim_whiteboard_start( wb );
		yahoo_doodle_command_send_request( gc, from );
	}
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_draw( GaimConnection *gc, char *from, char *message )
{
	GaimAccount *account;
	GaimWhiteboard *wb;
	int *token;
	int length;
	char *token_end;
	GList *d_list;
	int *n;
	GList *l;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got DRAW (%s)\n", from );
	
	g_print( "Draw Message:  %s\n", message );
	
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
	
	if( wb == NULL )
		return;
	
	// TODO Functionalize
	// Convert drawing packet message to an integer list
	
	token		= NULL;
	length		= strlen( message );
	
	d_list		= NULL;	// a local list of drawing info
	
	// Check to see if the message begans and ends with quotes
	if( ( message[0] != '\"' ) || ( message[length - 1] != '\"' ) )
		return;
	
	// Truncate the quotations off of our message (why the hell did they add them anyways!?)
	message[length - 1]	= ',';
	message			= message + 1;
	
	// Traverse and extract all integers divided by commas
	while( ( token_end = strchr( message, ',' ) ) )
	{
		token_end[0]	= 0;
		
		token		= g_new0( int, 1 );
		
		*token		= atoi( message );
		
		d_list		= g_list_append( d_list, ( gpointer )( token ) );
		
		message		= token_end + 1;
	}
	
	yahoo_doodle_draw_stroke( wb, d_list );
	
	//goodle_doodle_session_set_canvas_as_icon( ds );
	
	// Remove that shit
	n = NULL;
	l = d_list;
	while( l )
	{
		n = l->data;
		
		g_free( n );
		
		l = l->next;
	}
	
	g_list_free( d_list );
	d_list = NULL;
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_clear( GaimConnection *gc, char *from )
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got CLEAR (%s)\n", from );
	
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
	
	if( wb == NULL )
		return;
	
	if( wb->state == DOODLE_STATE_ESTABLISHED )
	{
		// TODO Ask user whether to save the image before clearing it
		
		gaim_whiteboard_clear( wb );
	}
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_extra( GaimConnection *gc, char *from, char *message )
{
	g_print( "-----------------------------------------------\n" );
	g_print( "Got EXTRA (%s)\n", from );
	
	// I do not like these 'extra' features, so I'll only handle them in one way,
	// which is returning them with the command/packet to turn them off
	
	yahoo_doodle_command_send_extra( gc, from, DOODLE_EXTRA_NONE );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_confirm( GaimConnection *gc, char *from )
{	
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got CONFIRM (%s)\n", from );
	
	// Get the doodle session
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
	
	if( wb == NULL )
		return;
	
	// TODO Combine the following IF's?
	
	// Check if we requested a doodle session
	if( wb->state == DOODLE_STATE_REQUESTING )
	{	
		wb->state = DOODLE_STATE_ESTABLISHED;
		
		gaim_whiteboard_start( wb );
		
		yahoo_doodle_command_send_confirm( gc, from );
	}
	
	// Check if we accepted a request for a doodle session
	if( wb->state == DOODLE_STATE_REQUESTED )
	{	
		wb->state = DOODLE_STATE_ESTABLISHED;
		
		gaim_whiteboard_start( wb );
	}
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_got_shutdown( GaimConnection *gc, char *from )
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_print( "-----------------------------------------------\n" );
	g_print( "Got SHUTDOWN (%s)\n", from );
	
	account	= gaim_connection_get_account( gc );
	
	// Only handle this if local client requested Doodle session (else local client would have sent one)
	wb		= gaim_whiteboard_get_session( account, from );
	
	// TODO Ask if user wants to save picture before the session is closed
	
	// If this session doesn't exist, don't try and kill it
	if( wb == NULL )
		return;
	else
	{
		gaim_whiteboard_destroy( wb );
		
		//yahoo_doodle_command_send_shutdown( gc, from );
	}
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_request( GaimConnection *gc, char *to )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent REQUEST (%s)\n", to );
	
	yd = gc->proto_data;

	// Make and send an acknowledge (ready) Doodle packet
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		"1" );
	yahoo_packet_hash_str( pkt, 13,		"1" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"1" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_ready( GaimConnection *gc, char *to )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent READY (%s)\n", to );
	
	yd = gc->proto_data;
	
	// Make and send a request to start a Doodle session
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		"" );
	yahoo_packet_hash_str( pkt, 13,		"0" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"0" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_draw( GaimConnection *gc, char *to, char *message )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent DRAW (%s)\n", to );
	
	yd = gc->proto_data;
	
	// Make and send a drawing packet
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		message );
	yahoo_packet_hash_str( pkt, 13,		"3" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"1" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_clear( GaimConnection *gc, char *to )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent CLEAR (%s)\n", to );
	
	yd = gc->proto_data;
	
	// Make and send a request to clear packet
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		" " );
	yahoo_packet_hash_str( pkt, 13,		"2" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"1" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_extra( GaimConnection *gc, char *to, char *message )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent EXTRA (%s)\n", to );
	
	yd = gc->proto_data;
		
	// Send out a request to use a specified 'extra' feature (message)
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		message );
	yahoo_packet_hash_str( pkt, 13,		"4" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"1" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_confirm( GaimConnection *gc, char *to )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent CONFIRM (%s)\n", to );
	
	yd = gc->proto_data;
	
	// Send ready packet (that local client accepted and is ready)
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		( char* )( gaim_account_get_username( gc->account ) ) );
	yahoo_packet_hash_str( pkt, 14,		"1" );
	yahoo_packet_hash_str( pkt, 13,		"5" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		"doodle;11" );
	yahoo_packet_hash_str( pkt, 64,		"1" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_command_send_shutdown( GaimConnection *gc, char *to )
{
	struct yahoo_data	*yd;
	struct yahoo_packet	*pkt;

	g_print( "-----------------------------------------------\n" );
	g_print( "Sent SHUTDOWN (%s)\n", to );
	
	yd = gc->proto_data;
	
	// Declare that you are ending the Doodle session
	pkt = yahoo_packet_new( YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0 );
	yahoo_packet_hash_str( pkt, 49,		"IMVIRONMENT" );
	yahoo_packet_hash_str( pkt, 1,		gaim_account_get_username( gc->account ) );
	yahoo_packet_hash_str( pkt, 14,		"" );
	yahoo_packet_hash_str( pkt, 13,		"0" );
	yahoo_packet_hash_str( pkt, 5,		to );
	yahoo_packet_hash_str( pkt, 63,		";0" );
	yahoo_packet_hash_str( pkt, 64,		"0" );
	yahoo_packet_hash_str( pkt, 1002,	"1" );
	
	yahoo_packet_send_and_free( pkt, yd );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_start( GaimWhiteboard *wb )
{
	doodle_session *ds	= g_new0( doodle_session, 1 );

	//g_print( "yahoo_doodle_start()\n" );
	
	// Set default brush size and color
	ds->brush_size		= DOODLE_BRUSH_MEDIUM;
	ds->brush_color		= 0;	// black
	
	wb->proto_data		= ds;
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_end( GaimWhiteboard *wb )
{
	GaimConnection *gc = gaim_account_get_connection( wb->account );
	doodle_session *ds;
	
	//g_print( "yahoo_doodle_end()\n" );
	
	yahoo_doodle_command_send_shutdown( gc, wb->who );
	
	ds = wb->proto_data;
	if( ds )
		g_free( ds );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_get_dimensions( GaimWhiteboard *wb, int *width, int *height )
{
	// Standard Doodle canvases are of one size:  368x256
	*width	= DOODLE_CANVAS_WIDTH;
	*height	= DOODLE_CANVAS_HEIGHT;
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_send_draw_list( GaimWhiteboard *wb, GList *draw_list )
{
	doodle_session	*ds		= wb->proto_data;
	char 		*message	= yahoo_doodle_build_draw_string( ds, draw_list );
	
	//g_print( "yahoo_doodle_send_draw_list()\n" );
	
	if( message )
		yahoo_doodle_command_send_draw( wb->account->gc, wb->who, message );

}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_clear( GaimWhiteboard *wb )
{
	yahoo_doodle_command_send_clear( wb->account->gc, wb->who );
}

// ------------------------------------------------------------------------------------------------------

void yahoo_doodle_draw_stroke( GaimWhiteboard *wb, GList *draw_list )
{
	// Traverse through the list and draw the points and lines
	
	GList	*l = draw_list;
	
	int	*n = NULL;
	
	int	brush_color;
	int	brush_size;
	int	x;
	int	y;
	
	int	dx, dy;
	
	int count = 0;
	
	//g_print( "Drawing: color=%d, size=%d, (%d,%d)\n", brush_color, brush_size, x, y );
	
	n = l->data; brush_color = *n; l = l->next;
	n = l->data; brush_size	 = *n; l = l->next;
	n = l->data; x		 = *n; l = l->next;
	n = l->data; y		 = *n; l = l->next;
	
	// Pray this works and pray that the list has an even number of elements
	while( l )
	{
		count++;
		
		n = l->data; dx	= *n; l = l->next;
		n = l->data; dy	= *n; l = l->next;
		
		gaim_whiteboard_draw_line( wb,
					   x, y,
					   x + dx, y + dy,
					   brush_color, brush_size );
		
		x = x + dx;
		y = y + dy;
	}
	
	//g_print( "Counted %d deltas\n", count );
}

// ------------------------------------------------------------------------------------------------------

char *yahoo_doodle_build_draw_string( doodle_session *ds, GList *draw_list )
{
	//g_print( "yahoo_doodle_build_draw_string()\n" );
	
	GList		*l = draw_list;
	
	int		*n = NULL;
	
	static char	message[1024];		// Hope that 1024 is enough
	char		token_string[16];	// Token string extracted from draw list
	
	if( draw_list == NULL )
		return( NULL );	
	
	strcpy( message, "\"" );
	
	sprintf( token_string, "%d,%d,", ds->brush_color, ds->brush_size );
	strcat( message, token_string );
	
	// Pray this works and pray that the list has an even number of elements
	while( l )
	{
		n = l->data;
		
		sprintf( token_string, "%d,", *n );
		
		// This check prevents overflow
		if( ( strlen( message ) + strlen( token_string ) ) < 1024 )
			strcat( message, token_string );
		else
			break;
		
		l = l->next;
	}
	
	message[strlen( message ) - 1]	= '\"';
	//message[strlen( message )]	= 0;
	//message[511]			= 0;
	
	//g_print( "Draw Message:  %s\n", message );
	
	return( message );
}

// ------------------------------------------------------------------------------------------------------

