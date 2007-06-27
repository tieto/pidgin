/**
 * @file gntbox.h Box API
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GNT_BOX_H
#define GNT_BOX_H

#include "gnt.h"
#include "gntwidget.h"

#define GNT_TYPE_BOX				(gnt_box_get_gtype())
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

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntBoxClass
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
GType gnt_box_get_gtype(void);

#define gnt_vbox_new(homo) gnt_box_new(homo, TRUE)
#define gnt_hbox_new(homo) gnt_box_new(homo, FALSE)

/**
 * 
 * @param homo
 * @param vert
 *
 * @return
 */
GntWidget * gnt_box_new(gboolean homo, gboolean vert);

/**
 * 
 * @param box
 * @param widget
 */
void gnt_box_add_widget(GntBox *box, GntWidget *widget);

/**
 * 
 * @param box
 * @param title
 */
void gnt_box_set_title(GntBox *box, const char *title);

/**
 * 
 * @param box
 * @param pad
 */
void gnt_box_set_pad(GntBox *box, int pad);

/**
 * 
 * @param box
 * @param set
 */
void gnt_box_set_toplevel(GntBox *box, gboolean set);

/**
 * 
 * @param box
 */
void gnt_box_sync_children(GntBox *box);

/**
 * 
 * @param box
 * @param alignment
 */
void gnt_box_set_alignment(GntBox *box, GntAlignment alignment);

/**
 * 
 * @param box
 * @param widget
 */
void gnt_box_remove(GntBox *box, GntWidget *widget);

 /* XXX: does NOT destroy widget */

/**
 * 
 * @param box
 */
void gnt_box_remove_all(GntBox *box);

      /* Removes AND destroys all the widgets in it */

/**
 * 
 * @param box
 */
void gnt_box_readjust(GntBox *box);

/**
 * 
 * @param box
 * @param fill
 */
void gnt_box_set_fill(GntBox *box, gboolean fill);

/**
 * 
 * @param box
 * @param dir
 */
void gnt_box_move_focus(GntBox *box, int dir);

  /* +1 to move forward, -1 for backward */

/**
 * 
 * @param box
 * @param widget
 */
void gnt_box_give_focus_to_child(GntBox *box, GntWidget *widget);

G_END_DECLS

#endif /* GNT_BOX_H */

