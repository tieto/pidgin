/**
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
 * 
 *
 * @return
 */
GType gnt_combo_box_get_gtype(void);

/**
 * 
 *
 * @return
 */
GntWidget * gnt_combo_box_new(void);

/**
 * 
 * @param box
 * @param key
 * @param text
 */
void gnt_combo_box_add_data(GntComboBox *box, gpointer key, const char *text);

/**
 * 
 * @param box
 * @param key
 */
void gnt_combo_box_remove(GntComboBox *box, gpointer key);

/**
 * 
 * @param box
 */
void gnt_combo_box_remove_all(GntComboBox *box);

/**
 * 
 * @param box
 *
 * @return
 */
gpointer gnt_combo_box_get_selected_data(GntComboBox *box);

/**
 * 
 * @param box
 * @param key
 */
void gnt_combo_box_set_selected(GntComboBox *box, gpointer key);

G_END_DECLS

#endif /* GNT_COMBO_BOX_H */
