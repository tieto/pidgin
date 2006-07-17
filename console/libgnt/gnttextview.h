#ifndef GNT_TEXT_VIEW_H
#define GNT_TEXT_VIEW_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_TEXTVIEW				(gnt_text_view_get_gtype())
#define GNT_TEXT_VIEW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_TEXTVIEW, GntTextView))
#define GNT_TEXT_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_TEXTVIEW, GntTextViewClass))
#define GNT_IS_TEXTVIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_TEXTVIEW))
#define GNT_IS_TEXTVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_TEXTVIEW))
#define GNT_TEXT_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_TEXTVIEW, GntTextViewClass))

#define GNT_TEXT_VIEW_FLAGS(obj)				(GNT_TEXT_VIEW(obj)->priv.flags)
#define GNT_TEXT_VIEW_SET_FLAGS(obj, flags)		(GNT_TEXT_VIEW_FLAGS(obj) |= flags)
#define GNT_TEXT_VIEW_UNSET_FLAGS(obj, flags)	(GNT_TEXT_VIEW_FLAGS(obj) &= ~(flags))

typedef struct _GnTextView			GntTextView;
typedef struct _GnTextViewPriv		GntTextViewPriv;
typedef struct _GnTextViewClass		GntTextViewClass;

struct _GnTextView
{
	GntWidget parent;

	GList *list;        /* List of GntTextLine */
};

typedef enum
{
	GNT_TEXT_FLAG_BOLD        = 1 << 0,
	GNT_TEXT_FLAG_UNDERLINE   = 1 << 1,
	GNT_TEXT_FLAG_BLINK       = 1 << 2,
	GNT_TEXT_FLAG_DIM         = 1 << 3,
	GNT_TEXT_FLAG_HIGHLIGHT   = 1 << 4,
} GntTextFormatFlags;

struct _GnTextViewClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_text_view_get_gtype(void);

/* XXX: For now, don't set a textview to have any border.
 *      If you want borders real bad, put it in a box. */
GntWidget *gnt_text_view_new();

/* scroll > 0 means scroll up, < 0 means scroll down, == 0 means scroll to the end */
void gnt_text_view_scroll(GntTextView *view, int scroll);

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextFormatFlags flags);

/* Move the cursor to the beginning of the next line and resets text-attributes.
 * It first completes the current line with the current text-attributes. */
void gnt_text_view_next_line(GntTextView *view);

chtype gnt_text_format_flag_to_chtype(GntTextFormatFlags flags);

G_END_DECLS

#endif /* GNT_TEXT_VIEW_H */
