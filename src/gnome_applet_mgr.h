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

#define GAIM_GNOME_DEVIL_OFFLINE "gaim/gnome/devil-offline.png"
#define GAIM_GNOME_DEVIL_CONNECT "gaim/gnome/devil-connect.png"
#define GAIM_GNOME_DEVIL_ONLINE "gaim/gnome/devil-online.png"

#define GAIM_GNOME_PENGUIN_OFFLINE "gaim/gnome/penguin-offline.png"
#define GAIM_GNOME_PENGUIN_CONNECT "gaim/gnome/penguin-connect.png"
#define GAIM_GNOME_PENGUIN_ONLINE "gaim/gnome/penguin-online.png"

gint init_applet_mgr();

void setUserState( enum gaim_user_states state );
enum gaim_user_states getUserState();

void AppletCancelLogon(); /* Used to cancel a logon and reset the applet */ 

void set_applet_draw_open();
void set_applet_draw_closed();

void insert_applet_away();
void remove_applet_away();

void update_pixmaps();


#endif /*USE_APPLET*/
#endif /*_GAIMGNOMEAPPLETMGR_H_*/
