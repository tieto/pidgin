#ifndef GNT_BUTTON_H
#define GNT_BUTTON_H

#include <glib.h>
#include <glib-object.h>
#include "gnt.h"
#include "gntwidget.h"

#define GNT_TYPE_BUTTON				(gnt_button_get_gtype())
#define GNT_BUTTON(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_BUTTON, GntButton))
#define GNT_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_BUTTON, GntButtonClass))
#define GNT_IS_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_BUTTON))
#define GNT_IS_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_BUTTON))
#define GNT_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_BUTTON, GntButtonClass))

typedef struct _GntButton			GntButton;
typedef struct _GntButtonPriv		GntButtonPriv;
typedef struct _GntButtonClass		GntButtonClass;

struct _GntButtonPriv
{
	char *text;
};

struct _GntButton
{
	GntWidget parent;

	GntButtonPriv *priv;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntButtonClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_button_get_gtype(void);

GntWidget *gnt_button_new(const char *text);

G_END_DECLS

#endif /* GNT_BUTTON_H */
