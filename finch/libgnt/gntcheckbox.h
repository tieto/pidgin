/**
 * @file gntcheckbox.h Checkbox API
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

#ifndef GNT_CHECK_BOX_H
#define GNT_CHECK_BOX_H

#include "gntbutton.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_CHECK_BOX				(gnt_check_box_get_gtype())
#define GNT_CHECK_BOX(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_CHECK_BOX, GntCheckBox))
#define GNT_CHECK_BOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_CHECK_BOX, GntCheckBoxClass))
#define GNT_IS_CHECK_BOX(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_CHECK_BOX))
#define GNT_IS_CHECK_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_CHECK_BOX))
#define GNT_CHECK_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_CHECK_BOX, GntCheckBoxClass))

#define GNT_CHECK_BOX_FLAGS(obj)				(GNT_CHECK_BOX(obj)->priv.flags)
#define GNT_CHECK_BOX_SET_FLAGS(obj, flags)		(GNT_CHECK_BOX_FLAGS(obj) |= flags)
#define GNT_CHECK_BOX_UNSET_FLAGS(obj, flags)	(GNT_CHECK_BOX_FLAGS(obj) &= ~(flags))

typedef struct _GntCheckBox			GntCheckBox;
typedef struct _GntCheckBoxPriv		GntCheckBoxPriv;
typedef struct _GntCheckBoxClass		GntCheckBoxClass;

struct _GntCheckBox
{
	GntButton parent;
	gboolean checked;
};

struct _GntCheckBoxClass
{
	GntButtonClass parent;

	void (*toggled)(void);

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return GType for GntCheckBox
 */
GType gnt_check_box_get_gtype(void);

/**
 * Create a new checkbox.
 *
 * @param text The text for the checkbox.
 *
 * @return  The newly created checkbox.
 */
GntWidget * gnt_check_box_new(const char *text);

/**
 * Set whether the checkbox should be checked or not.
 *
 * @param box   The checkbox.
 * @param set   @c TRUE if the checkbox should be selected, @c FALSE otherwise.
 */
void gnt_check_box_set_checked(GntCheckBox *box, gboolean set);

/**
 * Return the checked state of the checkbox.
 *
 * @param box  The checkbox.
 *
 * @return     @c TRUE if the checkbox is selected, @c FALSE otherwise.
 */
gboolean gnt_check_box_get_checked(GntCheckBox *box);

G_END_DECLS

#endif /* GNT_CHECK_BOX_H */
