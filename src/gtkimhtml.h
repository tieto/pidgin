/**
 * @file gtkimhtml.h GTK+ IM/HTML rendering component
 * @ingroup gtkui
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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

#ifndef __GTK_IMHTML_H
#define __GTK_IMHTML_H

#include <gdk/gdk.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkimage.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_TYPE_IMHTML            (gtk_imhtml_get_type ())
#define GTK_IMHTML(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_IMHTML, GtkIMHtml))
#define GTK_IMHTML_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMHTML, GtkIMHtmlClass))
#define GTK_IS_IMHTML(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMHTML))
#define GTK_IS_IMHTML_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMHTML))
#define GTK_IMHTML_SCALABLE(obj)   ((GtkIMHtmlScalable *)obj)

typedef struct _GtkIMHtml			GtkIMHtml;
typedef struct _GtkIMHtmlClass		GtkIMHtmlClass;
typedef struct _GtkIMHtmlFontDetail	GtkIMHtmlFontDetail;	/* The five elements contained in a FONT tag */
typedef struct _GtkSmileyTree		GtkSmileyTree;
typedef struct _GtkIMHtmlSmiley		GtkIMHtmlSmiley;
typedef struct _GtkIMHtmlScalable	GtkIMHtmlScalable;
typedef struct _GtkIMHtmlImage		GtkIMHtmlImage;
typedef struct _GtkIMHtmlHr			GtkIMHtmlHr;

typedef enum {
	GTK_IMHTML_BOLD =      1 << 0,
	GTK_IMHTML_ITALIC =    1 << 1,
	GTK_IMHTML_UNDERLINE = 1 << 2,
	GTK_IMHTML_GROW =      1 << 3,
	GTK_IMHTML_SHRINK =    1 << 4,
	GTK_IMHTML_FACE =      1 << 5,
	GTK_IMHTML_FORECOLOR = 1 << 6,
	GTK_IMHTML_BACKCOLOR = 1 << 7,
	GTK_IMHTML_LINK =      1 << 8,
	GTK_IMHTML_IMAGE =     1 << 9,
	GTK_IMHTML_SMILEY =    1 << 10,
	GTK_IMHTML_ALL =      -1
} GtkIMHtmlButtons;

struct _GtkIMHtml {
	GtkTextView text_view;
	GtkTextBuffer *text_buffer;
	GtkTextMark *scrollpoint;
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
	GdkCursor *text_cursor;
	GHashTable *smiley_data;
	GtkSmileyTree *default_smilies;
	char *protocol_name;

	gboolean show_comments;

	gboolean html_shortcuts;
	gboolean smiley_shortcuts;

	GtkWidget *tip_window;
	char *tip;
	guint tip_timer;

	GList *scalables;
	GdkRectangle old_rect;

	gchar *search_string;

	gboolean editable;
	GtkIMHtmlButtons format_functions;
	gboolean wbfo;	/* Whole buffer formatting only. */

	gint insert_offset;

	struct {
		gboolean bold:1;
		gboolean italic:1;
		gboolean underline:1;
		gchar *forecolor;
		gchar *backcolor;
		gchar *fontface;
		int fontsize;
		GtkTextTag *link;
	} edit;

	double zoom;
	int original_fsize;

	char *clipboard_text_string;
	char *clipboard_html_string;
};

struct _GtkIMHtmlClass {
	GtkTextViewClass parent_class;

	void (*url_clicked)(GtkIMHtml *, const gchar *);
	void (*buttons_update)(GtkIMHtml *, GtkIMHtmlButtons);
	void (*toggle_format)(GtkIMHtml *, GtkIMHtmlButtons);
	void (*clear_format)(GtkIMHtml *);
	void (*update_format)(GtkIMHtml *);
};

struct _GtkIMHtmlFontDetail {
	gushort size;
	gchar *face;
	gchar *fore;
	gchar *back;
	gchar *sml;
    gboolean underline;
};

struct _GtkSmileyTree {
	GString *values;
	GtkSmileyTree **children;
	GtkIMHtmlSmiley *image;
};

struct _GtkIMHtmlSmiley {
	gchar *smile;
	gchar *file;
	GdkPixbufAnimation *icon;
	gboolean hidden;
};

struct _GtkIMHtmlScalable {
	void (*scale)(struct _GtkIMHtmlScalable *, int, int);
	void (*add_to)(struct _GtkIMHtmlScalable *, GtkIMHtml *, GtkTextIter *);
	void (*free)(struct _GtkIMHtmlScalable *);
};

struct _GtkIMHtmlImage {
	GtkIMHtmlScalable scalable;
	GtkImage *image;
	GdkPixbuf *pixbuf;
	GtkTextMark *mark;
	gchar *filename;
	int width;
	int height;
};

struct _GtkIMHtmlHr {
	GtkIMHtmlScalable scalable;
	GtkWidget *sep;
};

typedef enum {
	GTK_IMHTML_NO_COLOURS    = 1 << 0,
	GTK_IMHTML_NO_FONTS      = 1 << 1,
	GTK_IMHTML_NO_COMMENTS   = 1 << 2, /* Remove */
	GTK_IMHTML_NO_TITLE      = 1 << 3,
	GTK_IMHTML_NO_NEWLINE    = 1 << 4,
	GTK_IMHTML_NO_SIZES      = 1 << 5,
	GTK_IMHTML_NO_SCROLL     = 1 << 6,
	GTK_IMHTML_RETURN_LOG    = 1 << 7,
	GTK_IMHTML_USE_POINTSIZE = 1 << 8
} GtkIMHtmlOptions;

GtkType    gtk_imhtml_get_type         (void);
GtkWidget* gtk_imhtml_new              (void *, void *);

void       gtk_imhtml_set_adjustments  (GtkIMHtml *imhtml,
					GtkAdjustment *hadj,
					GtkAdjustment *vadj);

void       gtk_imhtml_associate_smiley (GtkIMHtml *imhtml,
					gchar *sml, GtkIMHtmlSmiley *smiley);

void       gtk_imhtml_remove_smileys   (GtkIMHtml *imhtml);

void       gtk_imhtml_show_comments    (GtkIMHtml *imhtml, gboolean show);

void       gtk_imhtml_html_shortcuts(GtkIMHtml *imhtml, gboolean allow);

void       gtk_imhtml_smiley_shortcuts (GtkIMHtml *imhtml, gboolean allow);

void       gtk_imhtml_set_protocol_name(GtkIMHtml *imhtml, gchar *protocol_name);

#define    gtk_imhtml_append_text(x, y, z) \
 gtk_imhtml_append_text_with_images(x, y, z, NULL)

void gtk_imhtml_append_text_with_images (GtkIMHtml *imhtml,
					       const gchar *text,
					       GtkIMHtmlOptions options,
					       GSList *images);
void gtk_imhtml_insert_html_at_iter(GtkIMHtml        *imhtml,
                                    const gchar      *text,
                                    GtkIMHtmlOptions  options,
                                    GtkTextIter      *iter);
void gtk_imhtml_scroll_to_end(GtkIMHtml *imhtml);
void       gtk_imhtml_clear            (GtkIMHtml *imhtml);
void       gtk_imhtml_page_up          (GtkIMHtml *imhtml);
void       gtk_imhtml_page_down        (GtkIMHtml *imhtml);

void gtk_imhtml_font_zoom(GtkIMHtml *imhtml, double zoom);

GtkIMHtmlScalable *gtk_imhtml_scalable_new();
GtkIMHtmlScalable *gtk_imhtml_image_new(GdkPixbuf *img, const gchar *filename);
void gtk_imhtml_image_free(GtkIMHtmlScalable *);
void gtk_imhtml_image_scale(GtkIMHtmlScalable *, int, int);
void gtk_imhtml_image_add_to(GtkIMHtmlScalable *, GtkIMHtml *, GtkTextIter *);

GtkIMHtmlScalable *gtk_imhtml_hr_new();
void gtk_imhtml_hr_free(GtkIMHtmlScalable *);
void gtk_imhtml_hr_scale(GtkIMHtmlScalable *, int, int);
void gtk_imhtml_hr_add_to(GtkIMHtmlScalable *, GtkIMHtml *, GtkTextIter *);

/* Search functions */
gboolean gtk_imhtml_search_find(GtkIMHtml *imhtml, const gchar *text);
void gtk_imhtml_search_clear(GtkIMHtml *imhtml);

/* Editable stuff */
void gtk_imhtml_set_editable(GtkIMHtml *imhtml, gboolean editable);
void gtk_imhtml_set_whole_buffer_formatting_only(GtkIMHtml *imhtml, gboolean wbfo);
void gtk_imhtml_set_format_functions(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons);
GtkIMHtmlButtons gtk_imhtml_get_format_functions(GtkIMHtml *imhtml);
void gtk_imhtml_get_current_format(GtkIMHtml *imhtml, gboolean *bold, gboolean *italic, gboolean *underline);
gboolean gtk_imhtml_get_editable(GtkIMHtml *imhtml);
gboolean gtk_imhtml_toggle_bold(GtkIMHtml *imhtml);
gboolean gtk_imhtml_toggle_italic(GtkIMHtml *imhtml);
gboolean gtk_imhtml_toggle_underline(GtkIMHtml *imhtml);
gboolean gtk_imhtml_toggle_forecolor(GtkIMHtml *imhtml, const char *color);
gboolean gtk_imhtml_toggle_backcolor(GtkIMHtml *imhtml, const char *color);
gboolean gtk_imhtml_toggle_fontface(GtkIMHtml *imhtml, const char *face);
void gtk_imhtml_toggle_link(GtkIMHtml *imhtml, const char *url);
void gtk_imhtml_insert_link(GtkIMHtml *imhtml, GtkTextMark *mark, const char *url, const char *text);
void gtk_imhtml_insert_smiley(GtkIMHtml *imhtml, const char *sml, char *smiley);
void gtk_imhtml_insert_smiley_at_iter(GtkIMHtml *imhtml, const char *sml, char *smiley, GtkTextIter *iter);
void gtk_imhtml_font_set_size(GtkIMHtml *imhtml, gint size);
void gtk_imhtml_font_shrink(GtkIMHtml *imhtml);
void gtk_imhtml_font_grow(GtkIMHtml *imhtml);
char *gtk_imhtml_get_markup_range(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *end);
char *gtk_imhtml_get_markup(GtkIMHtml *imhtml);
/* returns a null terminated array of pointers to null terminated strings, each string for each line */
/* g_strfreev() should be called on it */
char **gtk_imhtml_get_markup_lines(GtkIMHtml *imhtml);
char *gtk_imhtml_get_text(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *stop);

#ifdef __cplusplus
}
#endif

#endif
