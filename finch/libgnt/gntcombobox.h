/**
 * @file gntcombobox.h Combobox API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_COMBO_BOX_H
#define GNT_COMBO_BOX_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntwidget.h"

#define GNT_TYPE_COMBO_BOX				(gnt_combo_box_get_gtype())
#define GNT_COMBO_BOX(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_COMBO_BOX, GntComboBox))
#define GNT_COMBO_BOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_COMBO_BOX, GntComboBoxClass))
#define GNT_IS_COMBO_BOX(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_COMBO_BOX))
#define GNT_IS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_COMBO_BOX))
#define GNT_COMBO_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_COMBO_BOX, GntComboBoxClass))

#define GNT_COMBO_BOX_FLAGS(obj)				(GNT_COMBO_BOX(obj)->priv.flags)
#define GNT_COMBO_BOX_SET_FLAGS(obj, flags)		(GNT_COMBO_BOX_FLAGS(obj) |= flags)
#define GNT_COMBO_BOX_UNSET_FLAGS(obj, flags)	(GNT_COMBO_BOX_FLAGS(obj) &= ~(flags))

typedef struct _GntComboBox			GntComboBox;
typedef struct _GntComboBoxPriv		GntComboBoxPriv;
typedef struct _GntComboBoxClass		GntComboBoxClass;

struct _GntComboBox
{
	GntWidget parent;

	GntWidget *dropdown;   /* This is a GntTree */

	void *selected;        /* Currently selected key */
};

struct _GntComboBoxClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return  Get the GType for GntComboBox
 */
GType gnt_combo_box_get_gtype(void);

/**
 * Create a new GntComboBox
 *
 * @return A new GntComboBox
 */
GntWidget * gnt_combo_box_new(void);

/**
 * Add an entry
 *
 * @param box The GntComboBox
 * @param key The data
 * @param text The text to display
 */
void gnt_combo_box_add_data(GntComboBox *box, gpointer key, const char *text);

/**
 * Remove an entry
 * 
 * @param box The GntComboBox
 * @param key The data to be removed
 */
void gnt_combo_box_remove(GntComboBox *box, gpointer key);

/**
 * Remove all entries
 *
 * @param box The GntComboBox
 */
void gnt_combo_box_remove_all(GntComboBox *box);

/**
 * Get the data that is currently selected
 *
 * @param box The GntComboBox
 *
 * @return The data of the currently selected entry
 */
gpointer gnt_combo_box_get_selected_data(GntComboBox *box);

/**
 * Set the current selection to a specific entry
 *
 * @param box The GntComboBox
 * @param key The data to be set to
 */
void gnt_combo_box_set_selected(GntComboBox *box, gpointer key);

G_END_DECLS

#endif /* GNT_COMBO_BOX_H */
