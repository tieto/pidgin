/**
 * @file desktopitem.h Functions for managing .desktop files
 * @ingroup core
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

/*
 * The following code has been adapted from gnome-desktop-item.[ch],
 * as found on gnome-desktop-2.8.1.
 *
 *   Copyright (C) 2004 by Alceste Scalas <alceste.scalas@gmx.net>.
 *
 * Original copyright notice:
 *
 * Copyright (C) 1999, 2000 Red Hat Inc.
 * Copyright (C) 2001 Sid Vicious
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GAIM_DESKTOP_ITEM_H_
#define _GAIM_DESKTOP_ITEM_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GAIM_DESKTOP_ITEM_TYPE_NULL = 0 /* This means its NULL, that is, not
					  * set */,
	GAIM_DESKTOP_ITEM_TYPE_OTHER /* This means it's not one of the below
					 strings types, and you must get the
					 Type attribute. */,

	/* These are the standard compliant types: */
	GAIM_DESKTOP_ITEM_TYPE_APPLICATION,
	GAIM_DESKTOP_ITEM_TYPE_LINK,
	GAIM_DESKTOP_ITEM_TYPE_FSDEVICE,
	GAIM_DESKTOP_ITEM_TYPE_MIME_TYPE,
	GAIM_DESKTOP_ITEM_TYPE_DIRECTORY,
	GAIM_DESKTOP_ITEM_TYPE_SERVICE,
	GAIM_DESKTOP_ITEM_TYPE_SERVICE_TYPE
} GaimDesktopItemType;

typedef struct _GaimDesktopItem GaimDesktopItem;

#define GAIM_TYPE_DESKTOP_ITEM         (gaim_desktop_item_get_type ())
GType gaim_desktop_item_get_type       (void);

/* standard */
#define GAIM_DESKTOP_ITEM_ENCODING	"Encoding" /* string */
#define GAIM_DESKTOP_ITEM_VERSION	"Version"  /* numeric */
#define GAIM_DESKTOP_ITEM_NAME		"Name" /* localestring */
#define GAIM_DESKTOP_ITEM_GENERIC_NAME	"GenericName" /* localestring */
#define GAIM_DESKTOP_ITEM_TYPE		"Type" /* string */
#define GAIM_DESKTOP_ITEM_FILE_PATTERN "FilePattern" /* regexp(s) */
#define GAIM_DESKTOP_ITEM_TRY_EXEC	"TryExec" /* string */
#define GAIM_DESKTOP_ITEM_NO_DISPLAY	"NoDisplay" /* boolean */
#define GAIM_DESKTOP_ITEM_COMMENT	"Comment" /* localestring */
#define GAIM_DESKTOP_ITEM_EXEC		"Exec" /* string */
#define GAIM_DESKTOP_ITEM_ACTIONS	"Actions" /* strings */
#define GAIM_DESKTOP_ITEM_ICON		"Icon" /* string */
#define GAIM_DESKTOP_ITEM_MINI_ICON	"MiniIcon" /* string */
#define GAIM_DESKTOP_ITEM_HIDDEN	"Hidden" /* boolean */
#define GAIM_DESKTOP_ITEM_PATH		"Path" /* string */
#define GAIM_DESKTOP_ITEM_TERMINAL	"Terminal" /* boolean */
#define GAIM_DESKTOP_ITEM_TERMINAL_OPTIONS "TerminalOptions" /* string */
#define GAIM_DESKTOP_ITEM_SWALLOW_TITLE "SwallowTitle" /* string */
#define GAIM_DESKTOP_ITEM_SWALLOW_EXEC	"SwallowExec" /* string */
#define GAIM_DESKTOP_ITEM_MIME_TYPE	"MimeType" /* regexp(s) */
#define GAIM_DESKTOP_ITEM_PATTERNS	"Patterns" /* regexp(s) */
#define GAIM_DESKTOP_ITEM_DEFAULT_APP	"DefaultApp" /* string */
#define GAIM_DESKTOP_ITEM_DEV		"Dev" /* string */
#define GAIM_DESKTOP_ITEM_FS_TYPE	"FSType" /* string */
#define GAIM_DESKTOP_ITEM_MOUNT_POINT	"MountPoint" /* string */
#define GAIM_DESKTOP_ITEM_READ_ONLY	"ReadOnly" /* boolean */
#define GAIM_DESKTOP_ITEM_UNMOUNT_ICON "UnmountIcon" /* string */
#define GAIM_DESKTOP_ITEM_SORT_ORDER	"SortOrder" /* strings */
#define GAIM_DESKTOP_ITEM_URL		"URL" /* string */
#define GAIM_DESKTOP_ITEM_DOC_PATH	"X-GNOME-DocPath" /* string */

/**
 * This function loads 'filename' and turns it into a GaimDesktopItem.
 *
 * @param filename The filename or directory path to load the GaimDesktopItem from
 *
 * @return The newly loaded item, or NULL on error.
 */
GaimDesktopItem *gaim_desktop_item_new_from_file (const char *filename);

/**
 * Gets the type attribute (the 'Type' field) of the item.  This should
 * usually be 'Application' for an application, but it can be 'Directory'
 * for a directory description.  There are other types available as well.
 * The type usually indicates how the desktop item should be handeled and
 * how the 'Exec' field should be handeled.
 *
 * @param item A desktop item
 *
 * @return The type of the specified 'item'. The returned memory
 * remains owned by the GaimDesktopItem and should not be freed.
 */
GaimDesktopItemType gaim_desktop_item_get_entry_type (const GaimDesktopItem *item);

/**
 * Gets the value of an attribute of the item, as a string.
 *
 * @param item A desktop item
 * @param attr The attribute to look for
 *
 * @return The value of the specified item attribute.
 */
const char *gaim_desktop_item_get_string (const GaimDesktopItem *item,
					  const char *attr);

/**
 * Creates a copy of a GaimDesktopItem.  The new copy has a refcount of 1.
 * Note: Section stack is NOT copied.
 *
 * @param item The item to be copied
 *
 * @return The new copy 
 */
GaimDesktopItem *gaim_desktop_item_copy (const GaimDesktopItem *item);

/**
 * Decreases the reference count of the specified item, and destroys
 * the item if there are no more references left.
 *
 * @param item A desktop item
 */
void gaim_desktop_item_unref (GaimDesktopItem *item);

G_END_DECLS

#endif /* _GAIM_DESKTOP_ITEM_H_ */
