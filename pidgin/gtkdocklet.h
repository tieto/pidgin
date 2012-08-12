/*
 * System tray icon (aka docklet) plugin for Purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifndef _GTKDOCKLET_H_
#define _GTKDOCKLET_H_

G_BEGIN_DECLS

/**
 * Returns the GtkStatusIcon used for the docklet.
 */
GtkStatusIcon *pidgin_docklet_get_status_icon(void);

void pidgin_docklet_init(void);
void pidgin_docklet_uninit(void);
void*pidgin_docklet_get_handle(void);

G_END_DECLS

#endif /* _GTKDOCKLET_H_ */

