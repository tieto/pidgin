/*
 * gaim - WinGaim Options Plugin
 *
 * File: gtkappbar.h
 * Date: August 2, 2003
 * Description: Appbar functionality for Windows GTK+ applications
 * 
 * Copyright (C) 2003, Herman Bloggs <hermanator12002@yahoo.com>
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
#ifndef _GTKAPPBAR_H_
#define _GTKAPPBAR_H_

typedef struct {
        GtkWidget *win;
        RECT docked_rect;
        UINT undocked_height;
        UINT side;
        gboolean docked;
        gboolean docking;
        gboolean registered;
        GList *dock_cbs;
} GtkAppBar;

typedef void (*GtkAppBarDockCB)(gboolean);

GtkAppBar *gtk_appbar_add(GtkWidget *win);
void gtk_appbar_remove(GtkAppBar *ab);
void gtk_appbar_dock(GtkAppBar *ab, UINT side);
void gtk_appbar_add_dock_cb(GtkAppBar *ab, GtkAppBarDockCB dock_cb);

#endif /* _GTKAPPBAR_H_ */
