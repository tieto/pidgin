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

typedef struct _GnBox			GntBox;
typedef struct _GnBoxClass		GntBoxClass;

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

struct _GnBox
{
	GntWidget parent;

	gboolean vertical;
	gboolean homogeneous;
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

struct _GnBoxClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_box_get_gtype(void);

#define gnt_vbox_new(homo) gnt_box_new(homo, TRUE)
#define gnt_hbox_new(homo) gnt_box_new(homo, FALSE)

GntWidget *gnt_box_new(gboolean homo, gboolean vert);

void gnt_box_add_widget(GntBox *box, GntWidget *widget);

void gnt_box_set_title(GntBox *box, const char *title);

void gnt_box_set_pad(GntBox *box, int pad);

void gnt_box_set_toplevel(GntBox *box, gboolean set);

void gnt_box_sync_children(GntBox *box);

void gnt_box_set_alignment(GntBox *box, GntAlignment alignment);

void gnt_box_remove(GntBox *box, GntWidget *widget); /* XXX: does NOT destroy widget */

void gnt_box_remove_all(GntBox *box);      /* Removes AND destroys all the widgets in it */

void gnt_box_readjust(GntBox *box);

G_END_DECLS

#endif /* GNT_BOX_H */

