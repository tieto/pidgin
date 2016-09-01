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

#ifndef GNT_BOX_H
#define GNT_BOX_H
/**
 * SECTION:gntbox
 * @section_id: libgnt-gntbox
 * @short_description: <filename>gntbox.h</filename>
 * @title: Box
 */

#include "gnt.h"
#include "gntwidget.h"

#define GNT_TYPE_BOX				(gnt_box_get_type())
#define GNT_BOX(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_BOX, GntBox))
#define GNT_BOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_BOX, GntBoxClass))
#define GNT_IS_BOX(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_BOX))
#define GNT_IS_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_BOX))
#define GNT_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_BOX, GntBoxClass))

typedef struct _GntBox			GntBox;
typedef struct _GntBoxClass		GntBoxClass;

typedef enum
{
	/* These for vertical boxes */
	GNT_ALIGN_LEFT,
	GNT_ALIGN_RIGHT,

	GNT_ALIGN_MID,

	/* These for horizontal boxes */
	GNT_ALIGN_TOP,
	GNT_ALIGN_BOTTOM
} GntAlignment;

struct _GntBox
{
	GntWidget parent;

	gboolean vertical;
	gboolean homogeneous;
	gboolean fill;
	GList *list;		/* List of widgets */

	GntWidget *active;
	int pad;			/* Number of spaces to use between widgets */
	GntAlignment alignment;  /* How are the widgets going to be aligned? */

	char *title;
	GList *focus;		/* List of widgets to cycle focus (only valid for parent boxes) */

	/*< private >*/
    void *res1;
    void *res2;
    void *res3;
    void *res4;
};

struct _GntBoxClass
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
 * gnt_box_get_type:
 *
 * The GType for GntBox.
 *
 * Returns: The GType.
 */
GType gnt_box_get_type(void);

#define gnt_vbox_new(homo) gnt_box_new(homo, TRUE)
#define gnt_hbox_new(homo) gnt_box_new(homo, FALSE)

/**
 * gnt_box_new:
 * @homo:  If %TRUE, all the widgets in it will have the same width (or height)
 * @vert:  Whether the widgets in it should be stacked vertically (if %TRUE)
 *              or horizontally (if %FALSE).
 *
 * Create a new GntBox.
 *
 * Returns: The new GntBox.
 */
GntWidget * gnt_box_new(gboolean homo, gboolean vert);

/**
 * gnt_box_add_widget:
 * @box:     The box
 * @widget:  The widget to add
 *
 * Add a widget in the box.
 */
void gnt_box_add_widget(GntBox *box, GntWidget *widget);

/**
 * gnt_box_add_widget_in_front:
 * @box:     The box
 * @widget:  The widget to add
 *
 * Add a widget in the box at its front.
 *
 * Since: 2.8.0
 */
void gnt_box_add_widget_in_front(GntBox *box, GntWidget *widget);

/**
 * gnt_box_set_title:
 * @box:    The box
 * @title:	 The title to set
 *
 * Set a title for the box.
 */
void gnt_box_set_title(GntBox *box, const char *title);

/**
 * gnt_box_set_pad:
 * @box: The box
 * @pad: The padding to use
 *
 * Set the padding to use between the widgets in the box.
 */
void gnt_box_set_pad(GntBox *box, int pad);

/**
 * gnt_box_set_toplevel:
 * @box: The box
 * @set: %TRUE if it's a toplevel box, %FALSE otherwise.
 *
 * Set whether it's a toplevel box (ie, a window) or not. If a box is toplevel,
 * then it will show borders, the title (if set) and shadow (if enabled in
 * <filename>.gntrc</filename>)
 */
void gnt_box_set_toplevel(GntBox *box, gboolean set);

/**
 * gnt_box_sync_children:
 * @box: The box
 *
 * Reposition and refresh the widgets in the box.
 */
void gnt_box_sync_children(GntBox *box);

/**
 * gnt_box_set_alignment:
 * @box:       The box
 * @alignment: The alignment to use
 *
 * Set the alignment for the widgets in the box.
 */
void gnt_box_set_alignment(GntBox *box, GntAlignment alignment);

/**
 * gnt_box_remove:
 * @box:       The box
 * @widget:    The widget to remove
 *
 * Remove a widget from the box. Calling this does NOT destroy the removed widget.
 */
void gnt_box_remove(GntBox *box, GntWidget *widget);

/**
 * gnt_box_remove_all:
 * @box: The box
 *
 * Remove all widgets from the box. This DOES destroy all widgets in the box.
 */
void gnt_box_remove_all(GntBox *box);

/**
 * gnt_box_readjust:
 * @box:  The box
 *
 * Readjust the size of each child widget, reposition the child widgets and
 * recalculate the size of the box.
 */
void gnt_box_readjust(GntBox *box);

/**
 * gnt_box_set_fill:
 * @box:   The box
 * @fill:  Whether the child widgets should fill the empty space
 *
 * Set whether the widgets in the box should fill the empty spaces.
 */
void gnt_box_set_fill(GntBox *box, gboolean fill);

/**
 * gnt_box_move_focus:
 * @box: The box
 * @dir: The direction. If it's 1, then the focus is moved forwards, if it's
 *            -1, the focus is moved backwards.
 *
 * Move the focus from one widget to the other.
 */
void gnt_box_move_focus(GntBox *box, int dir);

/**
 * gnt_box_give_focus_to_child:
 * @box:       The box
 * @widget:    The child widget to give focus
 *
 * Give focus to a specific child widget.
 */
void gnt_box_give_focus_to_child(GntBox *box, GntWidget *widget);

G_END_DECLS

#endif /* GNT_BOX_H */

