/**
 * @file gtkdnd-hints.h GTK+ Drag-and-Drop arrow hints
 * @ingroup gtkui
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _GAIM_DND_HINTS_H_
#define _GAIM_DND_HINTS_H_

#include <glib.h>
#include <gtk/gtkwidget.h>

/**
 * Conversation drag-and-drop arrow types.
 */
typedef enum
{
	HINT_ARROW_UP,    /**< Up arrow.    */
	HINT_ARROW_DOWN,  /**< Down arrow.  */
	HINT_ARROW_LEFT,  /**< Left arrow.  */
	HINT_ARROW_RIGHT  /**< Right arrow. */

} DndHintWindowId;

/**
 * Conversation drag-and-drop arrow positions.
 */
typedef enum {

	HINT_POSITION_RIGHT,  /**< Position to the right of a tab.  */
	HINT_POSITION_LEFT,   /**< Position to the left of a tab.   */
	HINT_POSITION_TOP,    /**< Position above a tab.            */
	HINT_POSITION_BOTTOM, /**< Position below a tab.            */
	HINT_POSITION_CENTER  /**< Position in the center of a tab. */

} DndHintPosition;

/**
 * Shows a drag-and-drop hint at the specified location.
 *
 * @param id The ID of the hint to show.
 * @param x  The X location to show it at.
 * @param y  The Y location to show it at.
 */
void dnd_hints_show(DndHintWindowId id, gint x, gint y);

/**
 * Hides the specified drag-and-drop hint.
 *
 * @param id The ID of the hint to hide.
 */
void dnd_hints_hide(DndHintWindowId id);

/**
 * Hides all drag-and-drop hints.
 */
void dnd_hints_hide_all(void);

/**
 * Shows a drag-and-drop hint relative to a widget.
 *
 * @param id     The ID of the hint.
 * @param widget The widget that the hint is relative to.
 * @param horiz  The horizontal relative position.
 * @param vert   The vertical relative position.
 */
void dnd_hints_show_relative(DndHintWindowId id, GtkWidget *widget,
							 DndHintPosition horiz, DndHintPosition vert);

#endif /* _GAIM_DND_HINTS_H_ */
