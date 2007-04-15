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

typedef struct _GnLabel			GntLabel;
typedef struct _GnLabelClass	GntLabelClass;

struct _GnLabel
{
	GntWidget parent;

	char *text;
	GntTextFormatFlags flags;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GnLabelClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_label_get_gtype(void);

GntWidget *gnt_label_new(const char *text);

GntWidget *gnt_label_new_with_format(const char *text, GntTextFormatFlags flags);

void gnt_label_set_text(GntLabel *label, const char *text);

G_END_DECLS

#endif /* GNT_LABEL_H */

