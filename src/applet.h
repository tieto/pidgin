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
#ifndef _APPLET_H_
#define _APPLET_H_
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
	away
};


/*
#define _MSG_OFFLINE_ "Offline"
#define _MSG_CONNECT_ "Connecting"
#define _MSG_ONLINE_ "Online"
#define _MSG_FONT_ "-*-helvetica-medium-r-*-*-*-80-*-*-*-*-*-*"
*/

#define GAIM_GNOME_DEVIL_OFFLINE "gaim/gnome/devil-offline.png"
#define GAIM_GNOME_DEVIL_CONNECT "gaim/gnome/devil-connect.png"
#define GAIM_GNOME_DEVIL_ONLINE "gaim/gnome/devil-online.png"

#define GAIM_GNOME_PENGUIN_OFFLINE "gaim/gnome/penguin-offline.png"
#define GAIM_GNOME_PENGUIN_CONNECT "gaim/gnome/penguin-connect.png"
#define GAIM_GNOME_PENGUIN_ONLINE "gaim/gnome/penguin-online.png"

#define GAIM_GNOME_OFFLINE_ICON "apple-red.png"
#define GAIM_GNOME_CONNECT_ICON "gnome-battery.png"
#define GAIM_GNOME_ONLINE_ICON "apple-green.png"

extern GtkWidget *applet;

extern gint init_applet_mgr();
extern void applet_do_signon(AppletWidget *, gpointer);
extern void make_buddy();
extern void cancel_logon();
extern gint applet_destroy_buddy(GtkWidget *, GdkEvent *, gpointer *);
extern void createOnlinePopup();

extern void set_user_state( enum gaim_user_states state );

extern void insert_applet_away();
extern void remove_applet_away();

extern void update_pixmaps();
extern void applet_set_tooltips(char *);

extern gboolean applet_buddy_show;

#endif /*USE_APPLET*/
#endif /*_APPLET_H_*/
