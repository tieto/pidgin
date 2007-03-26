#ifndef GNT_LINE_H
#define GNT_LINE_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_LINE				(gnt_line_get_gtype())
#define GNT_LINE(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_LINE, GntLine))
#define GNT_LINE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_LINE, GntLineClass))
#define GNT_IS_LINE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_LINE))
#define GNT_IS_LINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_LINE))
#define GNT_LINE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_LINE, GntLineClass))

#define GNT_LINE_FLAGS(obj)				(GNT_LINE(obj)->priv.flags)
#define GNT_LINE_SET_FLAGS(obj, flags)		(GNT_LINE_FLAGS(obj) |= flags)
#define GNT_LINE_UNSET_FLAGS(obj, flags)	(GNT_LINE_FLAGS(obj) &= ~(flags))

typedef struct _GntLine			GntLine;
typedef struct _GntLinePriv		GntLinePriv;
typedef struct _GntLineClass		GntLineClass;

struct _GntLine
{
	GntWidget parent;

	gboolean vertical;
};

struct _GntLineClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_line_get_gtype(void);

#define gnt_hline_new() gnt_line_new(FALSE)
#define gnt_vline_new() gnt_line_new(TRUE)

GntWidget *gnt_line_new(gboolean vertical);

G_END_DECLS

#endif /* GNT_LINE_H */
