/*
 * win2ktrans 
 *
 * Copyright (C) 1998-2002, Rob Flynn <rob@marko.net> 
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

#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include "gaim.h"
#include "wintransparency.h"

int alpha;

static void SetWindowTransparency(HWND hWnd, int trans) 
{
	SetWindowLong(hWnd,GWL_EXSTYLE,GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);

	SetLayeredWindowAttributes(hWnd,0,trans,LWA_ALPHA);
}

GdkFilterReturn wgaim_window_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	MSG *msg = (MSG*)xevent;
	struct conversation *c = (struct conversation *)data;

	gdk_window_remove_filter(GTK_WIDGET(c->window)->window, wgaim_window_filter, c);

	SetWindowTransparency(msg->hwnd, alpha);

	return GDK_FILTER_REMOVE;
}

