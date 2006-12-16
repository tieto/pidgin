/* 
 * System tray icon (aka docklet) plugin for Gaim
 * 
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _DOCKLET_H_
#define _DOCKLET_H_

typedef enum
{
	DOCKLET_STATUS_OFFLINE,
	DOCKLET_STATUS_ONLINE,
	DOCKLET_STATUS_ONLINE_PENDING,
	DOCKLET_STATUS_AWAY,
	DOCKLET_STATUS_AWAY_PENDING,
	DOCKLET_STATUS_CONNECTING
} DockletStatus;

struct docklet_ui_ops
{
	void (*create)(void);
	void (*destroy)(void);
	void (*update_icon)(DockletStatus);
	void (*blank_icon)(void);
	void (*set_tooltip)(gchar *);
	GtkMenuPositionFunc position_menu;
};

/* useful for setting idle callbacks that will be cleaned up */
extern GaimPlugin *handle;

/* functions in docklet.c */
void docklet_clicked(int);
void docklet_embedded(void);
void docklet_remove(void);
void docklet_set_ui_ops(struct docklet_ui_ops *);
void docklet_unload(void);

/* function in docklet-{x11,win32}.c */
void docklet_ui_init(void);

#endif /* _DOCKLET_H_ */
