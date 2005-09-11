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

// INCLUDES =============================================================================================

#include <string.h>

#include "whiteboard.h"
#include "prpl.h"

// DATATYPES ============================================================================================

// GLOBALS ==============================================================================================

static GaimWhiteboardUiOps	*whiteboard_ui_ops	= NULL;
//static GaimWhiteboardPrplOps	*whiteboard_prpl_ops	= NULL;

static GList			*wbList			= NULL;

//static gboolean		auto_accept		= TRUE;

// FUNCTIONS ============================================================================================

void gaim_whiteboard_set_ui_ops( GaimWhiteboardUiOps *ops )
{
	whiteboard_ui_ops = ops;
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_set_prpl_ops( GaimWhiteboard *wb, GaimWhiteboardPrplOps *ops )
{
	wb->prpl_ops = ops;
}

// ------------------------------------------------------------------------------------------------------

GaimWhiteboard *gaim_whiteboard_create( GaimAccount *account, char *who, int state )
{
	//g_print( "gaim_whiteboard_create()\n" );
	
	GaimPluginProtocolInfo *prpl_info;
	GaimWhiteboard *wb	= g_new0( GaimWhiteboard, 1 );

	wb->account		= account;
	wb->state		= state;
	wb->who			= g_strdup( who );
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO( account->gc->prpl );
	gaim_whiteboard_set_prpl_ops( wb, prpl_info->whiteboard_prpl_ops );
	
	// Start up protocol specifics
	if( wb->prpl_ops && wb->prpl_ops->start )
		wb->prpl_ops->start( wb );
	
	wbList			= g_list_append( wbList, ( gpointer )( wb ) );
	
	return( wb );
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_destroy( GaimWhiteboard *wb )
{
	//g_print( "gaim_whiteboard_destroy()\n" );

	if( wb->ui_data )
	{
		//g_print( "---wb->ui_data = %p\n", wb->ui_data );
		
		// Destroy frontend
		if( whiteboard_ui_ops && whiteboard_ui_ops->destroy )
			whiteboard_ui_ops->destroy( wb );
	}
	
	// Do protocol specific session ending procedures
	if( wb->prpl_ops && wb->prpl_ops->end )
		wb->prpl_ops->end( wb );
	
	if( wb )
	{
		//g_print( "---wb = %p\n", wb );
		
		wb->account	= NULL;
		wb->state	= 0;
	
		if( wb->who )
			g_free( wb->who );
			
		wbList = g_list_remove( wbList, wb );
		
		g_free( wb );
		wb = NULL;
	}
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_start( GaimWhiteboard *wb )
{
	//g_print( "gaim_whiteboard_start()\n" );
	
	// Create frontend for whiteboard
	if( whiteboard_ui_ops && whiteboard_ui_ops->create )
		whiteboard_ui_ops->create( wb );
}

// ------------------------------------------------------------------------------------------------------

// Looks through the list of whiteboard sessions for one that is between usernames 'me' and 'who'
// Returns a pointer to a matching whiteboard session; if none match, it returns NULL
GaimWhiteboard *gaim_whiteboard_get_session( GaimAccount *account, char *who )
{
	//g_print( "gaim_whiteboard_get_session()\n" );
	
	GaimWhiteboard	*wb	= NULL;
	
	GList		*l	= wbList;
	
	// Look for a whiteboard session between the local user and the remote user
	while( l )
	{
		wb = l->data;
		
		if( !strcmp( gaim_account_get_username( wb->account ), gaim_account_get_username( account ) ) &&
			     !strcmp( wb->who, who ) )
			return( wb );
		
		l = l->next;	
	}
	
	return( NULL );
}

// ------------------------------------------------------------------------------------------------------

GList *gaim_whiteboard_draw_list_destroy( GList *draw_list )
{
	//g_print( "gaim_whiteboard_draw_list_destroy()\n" );
	
	if( draw_list == NULL )
		return( NULL );	
	else
	{
		// Destroy the contents of this list
		int	*n = NULL;
		GList	*l = draw_list;
		while( l )
		{
			n = l->data;
			
			if( n )
				g_free( n );
			
			l = l->next;
		}
		
		g_list_free( draw_list );
		draw_list = NULL;
	}
	
	return( draw_list );
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_set_dimensions( GaimWhiteboard *wb, int width, int height )
{
	if( whiteboard_ui_ops && whiteboard_ui_ops->set_dimensions )
		whiteboard_ui_ops->set_dimensions( wb, width, height );
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_draw_point( GaimWhiteboard *wb, int x, int y, int color, int size )
{
	if( whiteboard_ui_ops && whiteboard_ui_ops->draw_point )
		whiteboard_ui_ops->draw_point( wb, x, y, color, size );
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_draw_line( GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size )
{
	if( whiteboard_ui_ops && whiteboard_ui_ops->draw_line )
		whiteboard_ui_ops->draw_line( wb, x1, y1, x2, y2, color, size );
}

// ------------------------------------------------------------------------------------------------------

void gaim_whiteboard_clear( GaimWhiteboard *wb )
{
	if( whiteboard_ui_ops && whiteboard_ui_ops->clear )
		whiteboard_ui_ops->clear( wb );
}

// ------------------------------------------------------------------------------------------------------
