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
/**
 * SECTION:gntlabel
 * @section_id: libgnt-gntlabel
 * @short_description: <filename>gntlabel.h</filename>
 * @title: Label
 */

#ifndef GNT_LABEL_H
#define GNT_LABEL_H

#include "gnt.h"
#include "gntwidget.h"
#include "gnttextview.h"

#define GNT_TYPE_LABEL				(gnt_label_get_type())
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

	/*< private >*/
    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntLabelClass
{
	GntWidgetClass parent;

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_label_get_type:
 *
 * Returns: GType for GntLabel.
 */
GType gnt_label_get_type(void);

/**
 * gnt_label_new:
 * @text:  The text of the label.
 *
 * Create a new GntLabel.
 *
 * Returns:  The newly created label.
 */
GntWidget * gnt_label_new(const char *text);

/**
 * gnt_label_new_with_format:
 * @text:    The text.
 * @flags:   Text attributes for the text.
 *
 * Create a new label with specified text attributes.
 *
 * Returns:  The newly created label.
 */
GntWidget * gnt_label_new_with_format(const char *text, GntTextFormatFlags flags);

/**
 * gnt_label_set_text:
 * @label:  The label.
 * @text:   The new text to set in the label.
 *
 * Change the text of a label.
 */
void gnt_label_set_text(GntLabel *label, const char *text);

G_END_DECLS

#endif /* GNT_LABEL_H */

