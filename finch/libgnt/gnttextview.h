#ifndef GNT_TEXT_VIEW_H
#define GNT_TEXT_VIEW_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_TEXT_VIEW				(gnt_text_view_get_gtype())
#define GNT_TEXT_VIEW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_TEXT_VIEW, GntTextView))
#define GNT_TEXT_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_TEXT_VIEW, GntTextViewClass))
#define GNT_IS_TEXT_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_TEXT_VIEW))
#define GNT_IS_TEXT_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_TEXT_VIEW))
#define GNT_TEXT_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_TEXT_VIEW, GntTextViewClass))

#define GNT_TEXT_VIEW_FLAGS(obj)				(GNT_TEXT_VIEW(obj)->priv.flags)
#define GNT_TEXT_VIEW_SET_FLAGS(obj, flags)		(GNT_TEXT_VIEW_FLAGS(obj) |= flags)
#define GNT_TEXT_VIEW_UNSET_FLAGS(obj, flags)	(GNT_TEXT_VIEW_FLAGS(obj) &= ~(flags))

typedef struct _GntTextView			GntTextView;
typedef struct _GntTextViewPriv		GntTextViewPriv;
typedef struct _GntTextViewClass		GntTextViewClass;

struct _GntTextView
{
	GntWidget parent;

	GString *string;
	GList *list;        /* List of GntTextLine */

	GList *tags;       /* A list of tags */
};

typedef enum
{
	GNT_TEXT_FLAG_NORMAL      = 0,
	GNT_TEXT_FLAG_BOLD        = 1 << 0,
	GNT_TEXT_FLAG_UNDERLINE   = 1 << 1,
	GNT_TEXT_FLAG_BLINK       = 1 << 2,
	GNT_TEXT_FLAG_DIM         = 1 << 3,
	GNT_TEXT_FLAG_HIGHLIGHT   = 1 << 4,
} GntTextFormatFlags;

struct _GntTextViewClass
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
GntWidget *gnt_text_view_new(void);

/* scroll > 0 means scroll up, < 0 means scroll down, == 0 means scroll to the end */
void gnt_text_view_scroll(GntTextView *view, int scroll);

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextFormatFlags flags);

void gnt_text_view_append_text_with_tag(GntTextView *view, const char *text, GntTextFormatFlags flags, const char *tag);

/* Move the cursor to the beginning of the next line and resets text-attributes.
 * It first completes the current line with the current text-attributes. */
void gnt_text_view_next_line(GntTextView *view);

chtype gnt_text_format_flag_to_chtype(GntTextFormatFlags flags);

void gnt_text_view_clear(GntTextView *view);

int gnt_text_view_get_lines_below(GntTextView *view);

int gnt_text_view_get_lines_above(GntTextView *view);

/* If text is NULL, then the tag is removed. */
int gnt_text_view_tag_change(GntTextView *view, const char *name, const char *text, gboolean all);

void gnt_text_view_attach_scroll_widget(GntTextView *view, GntWidget *widget);

G_END_DECLS

#endif /* GNT_TEXT_VIEW_H */
