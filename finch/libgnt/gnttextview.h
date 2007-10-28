/**
 * @file gnttextview.h Textview API
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

typedef enum
{
	GNT_TEXT_VIEW_NO_SCROLL     = 1 << 0,
	GNT_TEXT_VIEW_WRAP_CHAR     = 1 << 1,
	GNT_TEXT_VIEW_TOP_ALIGN     = 1 << 2,
} GntTextViewFlag;

struct _GntTextView
{
	GntWidget parent;

	GString *string;
	GList *list;        /* List of GntTextLine */

	GList *tags;       /* A list of tags */
	GntTextViewFlag flags;
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

/**
 * @return  GType for GntTextView.
 */
GType gnt_text_view_get_gtype(void);

/**
 * Create a new textview.
 *
 * @return The newly created textview.
 */
GntWidget * gnt_text_view_new(void);

/**
 * Scroll the textview.
 * @param view     The textview to scroll.
 * @param scroll   scroll > 0 means scroll up, < 0 means scroll down, == 0 means scroll to the end.
 */
void gnt_text_view_scroll(GntTextView *view, int scroll);

/**
 * Append new text in a textview.
 *
 * @param view   The textview.
 * @param text   The text to append to the textview.
 * @param flags  The text-flags to apply to the new text.
 */
void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextFormatFlags flags);

/**
 * Append text in the textview, with some identifier (tag) for the added text.
 *
 * @param view   The textview.
 * @param text   The text to append.
 * @param flags  The text-flags to apply to the new text.
 * @param tag    The tag for the appended text, so it can be changed later (@see gnt_text_view_tag_change)
 */
void gnt_text_view_append_text_with_tag(GntTextView *view, const char *text, GntTextFormatFlags flags, const char *tag);

/**
 * Move the cursor to the beginning of the next line and resets text-attributes.
 * It first completes the current line with the current text-attributes.
 *
 * @param view  The textview.
 */
void gnt_text_view_next_line(GntTextView *view);

/**
 * Convert GNT-text formats to ncurses-text attributes.
 *
 * @param flags  The GNT text format.
 *
 * @return  Nucrses text attribute.
 */
chtype gnt_text_format_flag_to_chtype(GntTextFormatFlags flags);

/**
 * Clear the contents of the textview.
 *
 * @param view  The textview.
 */
void gnt_text_view_clear(GntTextView *view);

/**
 * The number of lines below the bottom-most visible line.
 *
 * @param view  The textview.
 *
 * @return  Number of lines below the bottom-most visible line.
 */
int gnt_text_view_get_lines_below(GntTextView *view);

/**
 * The number of lines above the topmost visible line.
 *
 * @param view  The textview.
 *
 * @return  Number of lines above the topmost visible line.
 */
int gnt_text_view_get_lines_above(GntTextView *view);

/**
 * Change the text of a tag.
 *
 * @param view   The textview.
 * @param name   The name of the tag.
 * @param text   The new text for the text. If 'text' is @c NULL, the tag is removed.
 * @param all    @c TRUE if all of the instancess of the tag should be changed, @c FALSE if
 *               only the first instance should be changed.
 *
 * @return  The number of instances changed.
 */
int gnt_text_view_tag_change(GntTextView *view, const char *name, const char *text, gboolean all);

/**
 * Setup hooks so that pressing up/down/page-up/page-down keys when 'widget' is
 * in focus scrolls the textview.
 *
 * @param view    The textview.
 * @param widget  The trigger widget.
 */
void gnt_text_view_attach_scroll_widget(GntTextView *view, GntWidget *widget);

/**
 * Setup appropriate hooks so that pressing some keys when the 'pager' widget
 * is in focus triggers the PAGER to popup with the contents of the textview
 * in it.
 *
 * The default key-combination to trigger the pager is a-v, and the default
 * PAGER application is $PAGER. Both can be changed in ~/.gntrc like this:
 *
 * @code
 * [pager]
 * key = a-v
 * path = /path/to/pager
 * @endcode
 *
 * @param view    The textview.
 * @param pager   The widget to trigger the PAGER.
 */
void gnt_text_view_attach_pager_widget(GntTextView *view, GntWidget *pager);

/**
 * Setup appropriate hooks so that pressing some keys when 'widget'
 * is in focus triggers the EDITOR to popup with the contents of the textview
 * in it.
 *
 * The default key-combination to trigger the pager is a-e, and the default
 * EDITOR application is $EDITOR. Both can be changed in ~/.gntrc like this:
 *
 * @code
 * [editor]
 * key = a-e
 * path = /path/to/editor
 * @endcode
 *
 * @param view     The textview.
 * @param widget   The widget to trigger the EDITOR.
 */
void gnt_text_view_attach_editor_widget(GntTextView *view, GntWidget *widget);

/**
 * Set a GntTextViewFlag for the textview widget.
 *
 * @param view  The textview widget
 * @param flag  The flag to set
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_text_view_set_flag(GntTextView *view, GntTextViewFlag flag);

G_END_DECLS

#endif /* GNT_TEXT_VIEW_H */
