/**
 * @file gntlabel.h Label API
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

#ifndef GNT_LABEL_H
#define GNT_LABEL_H

#include "gnt.h"
#include "gntwidget.h"
#include "gnttextview.h"

#define GNT_TYPE_LABEL				(gnt_label_get_gtype())
#define GNT_LABEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_LABEL, GntLabel))
#define GNT_LABEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_LABEL, GntLabelClass))
#define GNT_IS_LABEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_LABEL))
#define GNT_IS_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_LABEL))
#define GNT_LABEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_LABEL, GntLabelClass))

typedef struct _GntLabel			GntLabel;
typedef struct _GntLabelClass	GntLabelClass;

struct _GntLabel
{
	GntWidget parent;

	char *text;
	GntTextFormatFlags flags;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntLabelClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return GType for GntLabel.
 */
GType gnt_label_get_gtype(void);

/**
 * Create a new GntLabel.
 *
 * @param text  The text of the label.
 *
 * @return  The newly created label.
 */
GntWidget * gnt_label_new(const char *text);

/**
 * Create a new label with specified text attributes.
 *
 * @param text    The text.
 * @param flags   Text attributes for the text.
 *
 * @return  The newly created label.
 */
GntWidget * gnt_label_new_with_format(const char *text, GntTextFormatFlags flags);

/**
 * Change the text of a label.
 *
 * @param label  The label.
 * @param text   The new text to set in the label.
 */
void gnt_label_set_text(GntLabel *label, const char *text);

G_END_DECLS

#endif /* GNT_LABEL_H */

