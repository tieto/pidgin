/**************************************************************
**
** GaimGnomeAppletMgr
** Author - Quinticent (John Palmieri: johnp@martianrock.com)
**
** Purpose - Takes over the task of managing the GNOME applet
**           code and provides a centralized codebase for
**	     GNOME integration for Gaim.
**
** Legal Stuff -
**
** gaim 
**
** Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**************************************************************/
#ifndef _GAIMGNOMEAPPLETMGR_H_
#define _GAIMGNOMEAPPLETMGR_H_
#ifdef USE_APPLET

#include <gnome.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <applet-widget.h>

enum gaim_user_states {
	offline = 0,
	signing_on,
	online,
	unread_message_pending,
	away
};


#define _MSG_OFFLINE_ "Offline"
#define _MSG_CONNECT_ "Connecting"
#define _MSG_ONLINE_ "Online"
#define _MSG_FONT_ "-adobe-helvetica-medium-r-normal-*-*-80-*-*-p-*-iso8859-1"

#define GAIM_GNOME_PIXMAP_DIR "/usr/share/pixmaps/gaim/gnome/"

/*this should be configurable instead of hard coded.*/
#if 0
#define GAIM_GNOME_OFFLINE_ICON "devil-offline.png"
#define GAIM_GNOME_CONNECT_ICON "devil-connect.png"
#define GAIM_GNOME_ONLINE_ICON "devil-online.png"

#else 
#define GAIM_GNOME_OFFLINE_ICON "penguin-offline.png"
#define GAIM_GNOME_CONNECT_ICON "penguin-connect.png"
#define GAIM_GNOME_ONLINE_ICON "penguin-online.png"
#endif

gint InitAppletMgr();                                              /* Initializes and creates applet */

void setUserState( enum gaim_user_states state );    /* Set the state the user is in (Online, Offline, etc.) */

void setTotalBuddies( gint num );						    /* For future use to display the total number of buddies within the applet */

void setNumBuddiesOnline( gint num );					/* For future use to display the total number of buddies currently online, within the applet */ 

enum gaim_user_states getUserState();					/* Returns the current state the user is in */

gint getTotalBuddies();											/* Returns the total number of buddys set by setTotalBuddies */

gint getNumBuddiesOnline();									/* Returns the total number of buddys set by setNumBuddiesOnline */

void AppletCancelLogon();                                   /* Used to cancel a logon and reset the applet	*/ 

void set_applet_draw_open();								/* Indicates that the code has a window open that can be controlled by clicking on the applet */

void set_applet_draw_closed();								/* indicates that the code has closed the window that is controled by clicking on the applet */

void show_away_mess( AppletWidget *widget, gpointer data );



#endif /*USE_APPLET*/
#endif /*_GAIMGNOMEAPPLETMGR_H_*/
