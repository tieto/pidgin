/*
 * GtkIMHtml
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "util.h"
#include "gtkimhtml.h"
#include "gtksourceiter.h"
#include <gtk/gtk.h>
#include <glib/gerror.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#include <locale.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#endif

#include <pango/pango-font.h>

/* GTK+ < 2.2.2 hack, see ui.h for details. */
#ifndef GTK_WRAP_WORD_CHAR
#define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
#endif

#define TOOLTIP_TIMEOUT 500

static void insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml);
void gtk_imhtml_close_tags(GtkIMHtml *imhtml);
static void gtk_imhtml_link_drag_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y, GtkSelectionData *sd, guint info, guint t, GtkIMHtml *imhtml);

/* POINT_SIZE converts from AIM font sizes to point sizes.  It probably should be redone in such a
 * way that it base the sizes off the default font size rather than using arbitrary font sizes. */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (options & GTK_IMHTML_USE_POINTSIZE ? x : _point_sizes [MIN ((x), MAX_FONT_SIZE) - 1])
static gdouble _point_sizes [] = { .69444444, .8333333, 1, 1.2, 1.44, 1.728, 2.0736};

enum {
	TARGET_HTML,
	TARGET_UTF8_STRING,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_TEXT
};

enum {
	DRAG_URL
};

enum {
	URL_CLICKED,
	BUTTONS_UPDATE,
	TOGGLE_FORMAT,
	CLEAR_FORMAT,
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

GtkTargetEntry selection_targets[] = {
	{ "text/html", 0, TARGET_HTML },
	{ "UTF8_STRING", 0, TARGET_UTF8_STRING },
	{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
	{ "STRING", 0, TARGET_STRING },
	{ "TEXT", 0, TARGET_TEXT}};

GtkTargetEntry link_drag_drop_targets[] = {
	{"x-url/ftp", 0, DRAG_URL},
	{"x-url/http", 0, DRAG_URL},
	{"text/uri-list", 0, DRAG_URL},
	{"_NETSCAPE_URL", 0, DRAG_URL}};

static GtkSmileyTree*
gtk_smiley_tree_new ()
{
	return g_new0 (GtkSmileyTree, 1);
}

static void
gtk_smiley_tree_insert (GtkSmileyTree *tree,
			GtkIMHtmlSmiley *smiley)
{
	GtkSmileyTree *t = tree;
	const gchar *x = smiley->smile;

	if (!strlen (x))
		return;

	while (*x) {
		gchar *pos;
		gint index;

		if (!t->values)
			t->values = g_string_new ("");

		pos = strchr (t->values->str, *x);
		if (!pos) {
			t->values = g_string_append_c (t->values, *x);
			index = t->values->len - 1;
			t->children = g_realloc (t->children, t->values->len * sizeof (GtkSmileyTree *));
			t->children [index] = g_new0 (GtkSmileyTree, 1);
		} else
			index = GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str);

		t = t->children [index];

		x++;
	}

	t->image = smiley;
}


void gtk_smiley_tree_destroy (GtkSmileyTree *tree)
{
	GSList *list = g_slist_append (NULL, tree);

	while (list) {
		GtkSmileyTree *t = list->data;
		gint i;
		list = g_slist_remove(list, t);
		if (t && t->values) {
			for (i = 0; i < t->values->len; i++)
				list = g_slist_append (list, t->children [i]);
			g_string_free (t->values, TRUE);
			g_free (t->children);
		}
		g_free (t);
	}
}

static gboolean gtk_size_allocate_cb(GtkIMHtml *widget, GtkAllocation *alloc, gpointer user_data)
{
	GdkRectangle rect;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(widget), &rect);
	if(widget->old_rect.width != rect.width || widget->old_rect.height != rect.height){
		GList *iter = GTK_IMHTML(widget)->scalables;

		while(iter){
			GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(iter->data);
			scale->scale(scale, rect.width, rect.height);

			iter = iter->next;
		}
	}

	widget->old_rect = rect;
	return FALSE;
}

static gint
gtk_imhtml_tip_paint (GtkIMHtml *imhtml)
{
	PangoLayout *layout;

	g_return_val_if_fail(GTK_IS_IMHTML(imhtml), FALSE);

	layout = gtk_widget_create_pango_layout(imhtml->tip_window, imhtml->tip);

	gtk_paint_flat_box (imhtml->tip_window->style, imhtml->tip_window->window,
						GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, imhtml->tip_window,
						"tooltip", 0, 0, -1, -1);

	gtk_paint_layout (imhtml->tip_window->style, imhtml->tip_window->window, GTK_STATE_NORMAL,
					  FALSE, NULL, imhtml->tip_window, NULL, 4, 4, layout);

	g_object_unref(layout);
	return FALSE;
}

static gint
gtk_imhtml_tip (gpointer data)
{
	GtkIMHtml *imhtml = data;
	PangoFontMetrics *font;
	PangoLayout *layout;

	gint gap, x, y, h, w, scr_w, baseline_skip;

	g_return_val_if_fail(GTK_IS_IMHTML(imhtml), FALSE);

	if (!imhtml->tip || !GTK_WIDGET_DRAWABLE (GTK_WIDGET(imhtml))) {
		imhtml->tip_timer = 0;
		return FALSE;
	}

	if (imhtml->tip_window){
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}

	imhtml->tip_timer = 0;
	imhtml->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable (imhtml->tip_window, TRUE);
	gtk_window_set_resizable (GTK_WINDOW (imhtml->tip_window), FALSE);
	gtk_widget_set_name (imhtml->tip_window, "gtk-tooltips");
	g_signal_connect_swapped (G_OBJECT (imhtml->tip_window), "expose_event",
							  G_CALLBACK (gtk_imhtml_tip_paint), imhtml);

	gtk_widget_ensure_style (imhtml->tip_window);
	layout = gtk_widget_create_pango_layout(imhtml->tip_window, imhtml->tip);
	font = pango_font_get_metrics(pango_context_load_font(pango_layout_get_context(layout),
							      imhtml->tip_window->style->font_desc),
				      NULL);
	

	pango_layout_get_pixel_size(layout, &scr_w, NULL);
	gap = PANGO_PIXELS((pango_font_metrics_get_ascent(font) +
					   pango_font_metrics_get_descent(font))/ 4);

	if (gap < 2)
		gap = 2;
	baseline_skip = PANGO_PIXELS(pango_font_metrics_get_ascent(font) +
								pango_font_metrics_get_descent(font));
	w = 8 + scr_w;
	h = 8 + baseline_skip;

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	if (GTK_WIDGET_NO_WINDOW (GTK_WIDGET(imhtml)))
		y += GTK_WIDGET(imhtml)->allocation.y;

	scr_w = gdk_screen_width();

	x -= ((w >> 1) + 4);

	if ((x + w) > scr_w)
		x -= (x + w) - scr_w;
	else if (x < 0)
		x = 0;

	y = y + PANGO_PIXELS(pango_font_metrics_get_ascent(font) +
						pango_font_metrics_get_descent(font));

	gtk_widget_set_size_request (imhtml->tip_window, w, h);
	gtk_widget_show (imhtml->tip_window);
	gtk_window_move (GTK_WINDOW(imhtml->tip_window), x, y);

	pango_font_metrics_unref(font);
	g_object_unref(layout);

	return FALSE;
}

gboolean gtk_motion_event_notify(GtkWidget *imhtml, GdkEventMotion *event, gpointer data)
{
	GtkTextIter iter;
	GdkWindow *win = event->window;
	int x, y;
	char *tip = NULL;
	GSList *tags = NULL, *templist = NULL;
	gdk_window_get_pointer(GTK_WIDGET(imhtml)->window, NULL, NULL, NULL);
	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(imhtml), GTK_TEXT_WINDOW_WIDGET,
										  event->x, event->y, &x, &y);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, x, y);
	tags = gtk_text_iter_get_tags(&iter);

	templist = tags;
	while (templist) {
		GtkTextTag *tag = templist->data;
		tip = g_object_get_data(G_OBJECT(tag), "link_url");
		if (tip)
			break;
		templist = templist->next;
	}

	if (GTK_IMHTML(imhtml)->tip) {
		if ((tip == GTK_IMHTML(imhtml)->tip)) {
			return FALSE;
		}
		/* We've left the cell.  Remove the timeout and create a new one below */
		if (GTK_IMHTML(imhtml)->tip_window) {
			gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
			GTK_IMHTML(imhtml)->tip_window = NULL;
		}
		if (GTK_IMHTML(imhtml)->editable)
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->text_cursor);
		else
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->arrow_cursor);
		if (GTK_IMHTML(imhtml)->tip_timer)
			g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
		GTK_IMHTML(imhtml)->tip_timer = 0;
	}

	if(tip){
		if (!GTK_IMHTML(imhtml)->editable)
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->hand_cursor);
		GTK_IMHTML(imhtml)->tip_timer = g_timeout_add (TOOLTIP_TIMEOUT,
							       gtk_imhtml_tip, imhtml);
	}

	GTK_IMHTML(imhtml)->tip = tip;
	g_slist_free(tags);
	return FALSE;
}

gboolean gtk_leave_event_notify(GtkWidget *imhtml, GdkEventCrossing *event, gpointer data)
{
	/* when leaving the widget, clear any current & pending tooltips and restore the cursor */
	if (GTK_IMHTML(imhtml)->tip_window) {
		gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
		GTK_IMHTML(imhtml)->tip_window = NULL;
	}
	if (GTK_IMHTML(imhtml)->tip_timer) {
		g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
		GTK_IMHTML(imhtml)->tip_timer = 0;
	}
	if (GTK_IMHTML(imhtml)->editable)
		gdk_window_set_cursor(event->window, GTK_IMHTML(imhtml)->text_cursor);
	else
		gdk_window_set_cursor(event->window, GTK_IMHTML(imhtml)->arrow_cursor);

	/* propogate the event normally */
	return FALSE;
}

/*
 * XXX - This should be removed eventually.
 *
 * This function exists to work around a gross bug in GtkTextView.
 * Basically, we short circuit ctrl+a and ctrl+end because they make
 * el program go boom.
 *
 * It's supposed to be fixed in gtk2.2.  You can view the bug report at
 * http://bugzilla.gnome.org/show_bug.cgi?id=107939
 */

/*
 * I'm adding some keyboard shortcuts too.
 */

gboolean gtk_key_pressed_cb(GtkIMHtml *imhtml, GdkEventKey *event, gpointer data)
{	
	char buf[7];
	buf[0] = '\0';
	GObject *object;

	if (event->state & GDK_CONTROL_MASK)
		switch (event->keyval) {
		case 'a':
			return TRUE;
			break;
			
		case GDK_Home:
			return TRUE;
			break;

		case GDK_End:
			return TRUE;
			break;
		
		case 'b':  /* ctrl-b is GDK_Left, which moves backwards. */
		case 'B':
			if (imhtml->format_functions & GTK_IMHTML_BOLD) {
				gtk_imhtml_toggle_bold(imhtml);
				object = g_object_ref(G_OBJECT(imhtml));
				g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_BOLD);
				g_object_unref(object);
			}
			return TRUE;
			break;
			
		case 'f':
		case 'F':
			/*set_toggle(gtkconv->toolbar.font,
			  !gtk_toggle_button_get_active(
			  GTK_TOGGLE_BUTTON(gtkconv->toolbar.font)));*/
			
			return TRUE;
			break;
			
		case 'i':
		case 'I':
			if (imhtml->format_functions & GTK_IMHTML_ITALIC)
				gtk_imhtml_toggle_italic(imhtml);
			return TRUE;
			break;
			
		case 'u':  /* ctrl-u is GDK_Clear, which clears the line. */
		case 'U':
			if (imhtml->format_functions & GTK_IMHTML_UNDERLINE)
				gtk_imhtml_toggle_underline(imhtml);
			return TRUE;
			break;
			
		case '-':
			if (imhtml->format_functions & GTK_IMHTML_SHRINK)
				gtk_imhtml_font_shrink(imhtml);
			return TRUE;
			break;
			
		case '=':
		case '+':
			if (imhtml->format_functions & GTK_IMHTML_GROW)
				gtk_imhtml_font_grow(imhtml);
			return TRUE;
			break;
			
		case '1': strcpy(buf, ":-)");  break;
		case '2': strcpy(buf, ":-(");  break;
		case '3': strcpy(buf, ";-)");  break;
		case '4': strcpy(buf, ":-P");  break;
		case '5': strcpy(buf, "=-O");  break;
		case '6': strcpy(buf, ":-*");  break;
		case '7': strcpy(buf, ">:o");  break;
		case '8': strcpy(buf, "8-)");  break;
		case '!': strcpy(buf, ":-$");  break;
		case '@': strcpy(buf, ":-!");  break;
		case '#': strcpy(buf, ":-[");  break;
		case '$': strcpy(buf, "O:-)"); break;
		case '%': strcpy(buf, ":-/");  break;
		case '^': strcpy(buf, ":'(");  break;
		case '&': strcpy(buf, ":-X");  break;
		case '*': strcpy(buf, ":-D");  break;
		}
	if (*buf) {
		gtk_imhtml_insert_smiley(imhtml, NULL, buf);//->account->protocol_id, buf);
		return TRUE;
	}	
	return FALSE;
}

#if GTK_CHECK_VERSION(2,2,0)
static void gtk_imhtml_clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, GtkIMHtml *imhtml) {
	GtkTextIter start, end;
	GtkTextMark *sel = gtk_text_buffer_get_selection_bound(imhtml->text_buffer);
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	char *text;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, sel);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &end, ins);


	if (info == TARGET_HTML) {
		int len;
		char *selection;
		GString *str = g_string_new(NULL);
		text = gtk_imhtml_get_markup_range(imhtml, &start, &end);

		/* Mozilla asks that we start our text/html with the Unicode byte order mark */
		str = g_string_append_unichar(str, 0xfeff);
		str = g_string_append(str, text);
		str = g_string_append_unichar(str, 0x0000);
		selection = g_convert(str->str, str->len, "UCS-2", "UTF-8", NULL, &len, NULL);
		gtk_selection_data_set (selection_data, gdk_atom_intern("text/html", FALSE), 16, selection, len);
		g_string_free(str, TRUE);
		g_free(selection);
	} else {
		text = gtk_text_buffer_get_text(imhtml->text_buffer, &start, &end, FALSE);
		gtk_selection_data_set_text(selection_data, text, strlen(text));
	}
	g_free(text);
}

static void gtk_imhtml_primary_clipboard_clear(GtkClipboard *clipboard, GtkIMHtml *imhtml)
{
	GtkTextIter insert;
	GtkTextIter selection_bound;

	gtk_text_buffer_get_iter_at_mark (imhtml->text_buffer, &insert,
					  gtk_text_buffer_get_mark (imhtml->text_buffer, "insert"));
	gtk_text_buffer_get_iter_at_mark (imhtml->text_buffer, &selection_bound,
					  gtk_text_buffer_get_mark (imhtml->text_buffer, "selection_bound"));

	if (!gtk_text_iter_equal (&insert, &selection_bound))
		gtk_text_buffer_move_mark (imhtml->text_buffer,
					   gtk_text_buffer_get_mark (imhtml->text_buffer, "selection_bound"),
					   &insert);
}

static void copy_clipboard_cb(GtkIMHtml *imhtml, GtkClipboard *clipboard)
{
	gtk_clipboard_set_with_owner(gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD),
				     selection_targets, sizeof(selection_targets) / sizeof(GtkTargetEntry),
				     (GtkClipboardGetFunc)gtk_imhtml_clipboard_get,
				     (GtkClipboardClearFunc)NULL, G_OBJECT(imhtml));

	g_signal_stop_emission_by_name(imhtml, "copy-clipboard");
}

static void paste_received_cb (GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data)
{
	char *text;
	guint16 c;
	GtkIMHtml *imhtml = data;

	if (!gtk_text_view_get_editable(GTK_TEXT_VIEW(imhtml)))
		return;

	if (selection_data->length < 0) {
		text = gtk_clipboard_wait_for_text(clipboard);

		if (text == NULL)
			return;

	} else {
		text = g_malloc((selection_data->format / 8) * selection_data->length);
		memcpy(text, selection_data->data, selection_data->length * (selection_data->format / 8));
	}

	memcpy (&c, text, 2);
	if (c == 0xfeff) {
		/* This is UCS2 */
		char *utf8 = g_convert(text+2, (selection_data->length * (selection_data->format / 8)) - 2, "UTF-8", "UCS-2", NULL, NULL, NULL);
		g_free(text);
		text = utf8;
	}
	gtk_imhtml_close_tags(imhtml);
	gtk_imhtml_append_text_with_images(imhtml, text, GTK_IMHTML_NO_NEWLINE, NULL);
}


static void paste_clipboard_cb(GtkIMHtml *imhtml, gpointer blah)
{

	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_contents(clipboard, gdk_atom_intern("text/html", FALSE),
				       paste_received_cb, imhtml);
	g_signal_stop_emission_by_name(imhtml, "paste-clipboard");
}

static gboolean button_release_cb(GtkIMHtml *imhtml, GdkEventButton event, gpointer the_foibles_of_man)
{
	GtkClipboard *clipboard;
	if (event.button == 1) {
		if ((clipboard = gtk_widget_get_clipboard (GTK_WIDGET (imhtml),
							   GDK_SELECTION_PRIMARY)))
			gtk_text_buffer_remove_selection_clipboard (imhtml->text_buffer, clipboard);
		gtk_clipboard_set_with_owner(gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY),
					     selection_targets, sizeof(selection_targets) / sizeof(GtkTargetEntry),
					     (GtkClipboardGetFunc)gtk_imhtml_clipboard_get,
					     (GtkClipboardClearFunc)gtk_imhtml_primary_clipboard_clear, G_OBJECT(imhtml));
	}
	return FALSE;
}
#endif


static GtkTextViewClass *parent_class = NULL;

static void
gtk_imhtml_finalize (GObject *object)
{
	GtkIMHtml *imhtml = GTK_IMHTML(object);
	GList *scalables;

	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	gdk_cursor_unref(imhtml->hand_cursor);
	gdk_cursor_unref(imhtml->arrow_cursor);
	gdk_cursor_unref(imhtml->text_cursor);
	if(imhtml->tip_window){
		gtk_widget_destroy(imhtml->tip_window);
	}
	if(imhtml->tip_timer)
		gtk_timeout_remove(imhtml->tip_timer);

	for(scalables = imhtml->scalables; scalables; scalables = scalables->next) {
		GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(scalables->data);
		scale->free(scale);
	}

	g_list_free(imhtml->scalables);
	G_OBJECT_CLASS(parent_class)->finalize (object);
}

/* Boring GTK stuff */
static void gtk_imhtml_class_init (GtkIMHtmlClass *class)
{
	GtkObjectClass *object_class;
	GObjectClass   *gobject_class;
	object_class = (GtkObjectClass*) class;
	gobject_class = (GObjectClass*) class;
	parent_class = gtk_type_class(GTK_TYPE_TEXT_VIEW);
	signals[URL_CLICKED] = g_signal_new("url_clicked",
						G_TYPE_FROM_CLASS(gobject_class),
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET(GtkIMHtmlClass, url_clicked),
						NULL,
						0,
						g_cclosure_marshal_VOID__POINTER,
						G_TYPE_NONE, 1,
						G_TYPE_POINTER);
	signals[BUTTONS_UPDATE] = g_signal_new("format_functions_update",
					       G_TYPE_FROM_CLASS(gobject_class),
					       G_SIGNAL_RUN_FIRST,
					       G_STRUCT_OFFSET(GtkIMHtmlClass, buttons_update),
					       NULL,
					       0,
					       g_cclosure_marshal_VOID__POINTER,
					       G_TYPE_NONE, 1,
					       G_TYPE_INT);
	signals[TOGGLE_FORMAT] = g_signal_new("format_function_toggle",
					      G_TYPE_FROM_CLASS(gobject_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET(GtkIMHtmlClass, toggle_format),
					      NULL,
					      0,
					      g_cclosure_marshal_VOID__POINTER,
					      G_TYPE_NONE, 1, 
					      G_TYPE_INT);
	signals[CLEAR_FORMAT] = g_signal_new("format_function_clear",
					      G_TYPE_FROM_CLASS(gobject_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET(GtkIMHtmlClass, clear_format),
					      NULL,
					      0,
					      g_cclosure_marshal_VOID__POINTER,
					     G_TYPE_NONE, 0);
	gobject_class->finalize = gtk_imhtml_finalize;
}

static void gtk_imhtml_init (GtkIMHtml *imhtml)
{
	GtkTextIter iter;
	imhtml->text_buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_get_end_iter (imhtml->text_buffer, &iter);
	imhtml->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, FALSE);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(imhtml), imhtml->text_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(imhtml), 5);
	/*gtk_text_view_set_indent(GTK_TEXT_VIEW(imhtml), -15);*/
	/*gtk_text_view_set_justification(GTK_TEXT_VIEW(imhtml), GTK_JUSTIFY_FILL);*/

	/* These tags will be used often and can be reused--we create them on init and then apply them by name
	 * other tags (color, size, face, etc.) will have to be created and applied dynamically */
	gtk_text_buffer_create_tag(imhtml->text_buffer, "BOLD", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "ITALICS", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "UNDERLINE", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "STRIKE", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUB", "rise", -5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUP", "rise", 5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "PRE", "family", "Monospace", NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "search", "background", "#22ff00", "weight", "bold", NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "LINK", "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	/* When hovering over a link, we show the hand cursor--elsewhere we show the plain ol' pointer cursor */
	imhtml->hand_cursor = gdk_cursor_new (GDK_HAND2);
	imhtml->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	imhtml->text_cursor = gdk_cursor_new (GDK_XTERM);

	imhtml->show_smileys = TRUE;
	imhtml->show_comments = TRUE;

	imhtml->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)gtk_smiley_tree_destroy);
	imhtml->default_smilies = gtk_smiley_tree_new();

	g_signal_connect(G_OBJECT(imhtml), "size-allocate", G_CALLBACK(gtk_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "motion-notify-event", G_CALLBACK(gtk_motion_event_notify), NULL);
	g_signal_connect(G_OBJECT(imhtml), "leave-notify-event", G_CALLBACK(gtk_leave_event_notify), NULL);
	g_signal_connect(G_OBJECT(imhtml), "key_press_event", G_CALLBACK(gtk_key_pressed_cb), NULL);
	g_signal_connect_after(G_OBJECT(imhtml->text_buffer), "insert-text", G_CALLBACK(insert_cb), imhtml);

	gtk_drag_dest_set(GTK_WIDGET(imhtml), 0,
			  link_drag_drop_targets, sizeof(link_drag_drop_targets) / sizeof(GtkTargetEntry),
			  GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(imhtml), "drag_data_received", G_CALLBACK(gtk_imhtml_link_drag_rcv_cb), imhtml);

#if 0 /* Remove buggy copy-and-paste for 0.76 */
#if GTK_CHECK_VERSION(2,2,0)
	g_signal_connect(G_OBJECT(imhtml), "copy-clipboard", G_CALLBACK(copy_clipboard_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "paste-clipboard", G_CALLBACK(paste_clipboard_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "button-release-event", G_CALLBACK(button_release_cb), imhtml);
#endif
#endif
	gtk_widget_add_events(GTK_WIDGET(imhtml), GDK_LEAVE_NOTIFY_MASK);

	imhtml->tip = NULL;
	imhtml->tip_timer = 0;
	imhtml->tip_window = NULL;

	imhtml->edit.bold = NULL;
	imhtml->edit.italic = NULL;
	imhtml->edit.underline = NULL;
	imhtml->edit.forecolor = NULL;
	imhtml->edit.backcolor = NULL;
	imhtml->edit.fontface = NULL;
	imhtml->edit.sizespan = NULL;
	imhtml->edit.fontsize = 3;

	imhtml->format_spans = NULL;

	imhtml->scalables = NULL;

	gtk_imhtml_set_editable(imhtml, FALSE);
}

GtkWidget *gtk_imhtml_new(void *a, void *b)
{
	return GTK_WIDGET(g_object_new(gtk_imhtml_get_type(), NULL));
}

GType gtk_imhtml_get_type()
{
	static GType imhtml_type = 0;

	if (!imhtml_type) {
		static const GTypeInfo imhtml_info = {
			sizeof(GtkIMHtmlClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_imhtml_class_init,
			NULL,
			NULL,
			sizeof (GtkIMHtml),
			0,
			(GInstanceInitFunc) gtk_imhtml_init
		};

		imhtml_type = g_type_register_static(gtk_text_view_get_type(),
				"GtkIMHtml", &imhtml_info, 0);
	}

	return imhtml_type;
}

struct url_data {
	GObject *object;
	gchar *url;
};

static void url_open(GtkWidget *w, struct url_data *data) {
	if(!data) return;
	g_signal_emit(data->object, signals[URL_CLICKED], 0, data->url);

	g_object_unref(data->object);
	g_free(data->url);
	g_free(data);
}

static void url_copy(GtkWidget *w, gchar *url) {
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);

	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
}

/* The callback for an event on a link tag. */
gboolean tag_event(GtkTextTag *tag, GObject *imhtml, GdkEvent *event, GtkTextIter *arg2, char *url) {
	GdkEventButton *event_button = (GdkEventButton *) event;
	if (GTK_IMHTML(imhtml)->editable)
		return FALSE;
	if (event->type == GDK_BUTTON_RELEASE) {
		if (event_button->button == 1) {
			GtkTextIter start, end;
			/* we shouldn't open a URL if the user has selected something: */
			gtk_text_buffer_get_selection_bounds(
						gtk_text_iter_get_buffer(arg2),	&start, &end);
			if(gtk_text_iter_get_offset(&start) !=
					gtk_text_iter_get_offset(&end))
				return FALSE;

			/* A link was clicked--we emit the "url_clicked" signal
			 * with the URL as the argument */
			g_signal_emit(imhtml, signals[URL_CLICKED], 0, url);
			return FALSE;
		} else if(event_button->button == 3) {
			GtkWidget *img, *item, *menu;
			struct url_data *tempdata = g_new(struct url_data, 1);
			tempdata->object = g_object_ref(imhtml);
			tempdata->url = g_strdup(url);

			/* Don't want the tooltip around if user right-clicked on link */
			if (GTK_IMHTML(imhtml)->tip_window) {
				gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
				GTK_IMHTML(imhtml)->tip_window = NULL;
			}
			if (GTK_IMHTML(imhtml)->tip_timer) {
				g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
				GTK_IMHTML(imhtml)->tip_timer = 0;
			}
			if (GTK_IMHTML(imhtml)->editable)
				gdk_window_set_cursor(event_button->window, GTK_IMHTML(imhtml)->text_cursor);
			else
				gdk_window_set_cursor(event_button->window, GTK_IMHTML(imhtml)->arrow_cursor);
			menu = gtk_menu_new();

			/* buttons and such */

			if (!strncmp(url, "mailto:", 7))
			{
				/* Copy E-Mail Address */
				img = gtk_image_new_from_stock(GTK_STOCK_COPY,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Copy E-Mail Address"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_copy), url + 7);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			}
			else
			{
				/* Copy Link Location */
				img = gtk_image_new_from_stock(GTK_STOCK_COPY,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Copy Link Location"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_copy), url);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

				/* Open Link in Browser */
				img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Open Link in Browser"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_open), tempdata);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			}


			gtk_widget_show_all(menu);
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
							event_button->button, event_button->time);

			return TRUE;
		}
	}
	if(event->type == GDK_BUTTON_PRESS && event_button->button == 3)
		return TRUE; /* Clicking the right mouse button on a link shouldn't
						be caught by the regular GtkTextView menu */
	else
		return FALSE; /* Let clicks go through if we didn't catch anything */
}

static void
gtk_imhtml_link_drag_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
 			    GtkSelectionData *sd, guint info, guint t, GtkIMHtml *imhtml)
{
	if(gtk_imhtml_get_editable(imhtml) && sd->data){
		gchar **links;
		gchar *link;

		gaim_str_strip_cr(sd->data);

		links = g_strsplit(sd->data, "\n", 0);
		while((link = *links++) != NULL){
			if(gaim_str_has_prefix(link, "http://") ||
			   gaim_str_has_prefix(link, "https://") ||
			   gaim_str_has_prefix(link, "ftp://")){
				gtk_imhtml_insert_link(imhtml, link, link);
			} else if (link=='\0') {
				/* Ignore blank lines */
			} else {
				/* Special reasons, aka images being put in via other tag, etc. */
			}
		}

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	} else {
		gtk_drag_finish(dc, FALSE, FALSE, t);
	}
}

/* this isn't used yet
static void
gtk_smiley_tree_remove (GtkSmileyTree     *tree,
			GtkIMHtmlSmiley   *smiley)
{
	GtkSmileyTree *t = tree;
	const gchar *x = smiley->smile;
	gint len = 0;

	while (*x) {
		gchar *pos;

		if (!t->values)
			return;

		pos = strchr (t->values->str, *x);
		if (pos)
			t = t->children [(int) pos - (int) t->values->str];
		else
			return;

		x++; len++;
	}

	if (t->image) {
		t->image = NULL;
	}
}
*/


static gint
gtk_smiley_tree_lookup (GtkSmileyTree *tree,
			const gchar   *text)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;
	gint len = 0;

	while (*x) {
		gchar *pos;

		if (!t->values)
			break;

		pos = strchr (t->values->str, *x);
		if (pos)
			t = t->children [GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str)];
		else
			break;

		x++; len++;
	}

	if (t->image)
		return len;

	return 0;
}

void
gtk_imhtml_associate_smiley (GtkIMHtml       *imhtml,
			     gchar           *sml,
			     GtkIMHtmlSmiley *smiley)
{
	GtkSmileyTree *tree;
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	if (sml == NULL)
		tree = imhtml->default_smilies;
	else if ((tree = g_hash_table_lookup(imhtml->smiley_data, sml))) {
	} else {
		tree = gtk_smiley_tree_new();
		g_hash_table_insert(imhtml->smiley_data, g_strdup(sml), tree);
	}

	gtk_smiley_tree_insert (tree, smiley);
}

static gboolean
gtk_imhtml_is_smiley (GtkIMHtml   *imhtml,
		      GSList      *fonts,
		      const gchar *text,
		      gint        *len)
{
	GtkSmileyTree *tree;
	GtkIMHtmlFontDetail *font;
	char *sml = NULL;

	if (fonts) {
		font = fonts->data;
		sml = font->sml;
	}

	if (sml == NULL)
		tree = imhtml->default_smilies;
	else {
		tree = g_hash_table_lookup(imhtml->smiley_data, sml);
	}
	if (tree == NULL)
		return FALSE;

	*len = gtk_smiley_tree_lookup (tree, text);
	return (*len > 0);
}

GdkPixbufAnimation *
gtk_smiley_tree_image (GtkIMHtml     *imhtml,
		       const gchar   *sml,
		       const gchar   *text)
{
	GtkSmileyTree *t;
	const gchar *x = text;
	if (sml == NULL)
		t = imhtml->default_smilies;
	else
		t = g_hash_table_lookup(imhtml->smiley_data, sml);


	if (t == NULL)
		return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;

	while (*x) {
		gchar *pos;

		if (!t->values) {
			return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;
		}

		pos = strchr (t->values->str, *x);
		if (pos) {
			t = t->children [GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str)];
		} else {
			return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;
		}
		x++;
	}

	if (!t->image->icon)
		t->image->icon = gdk_pixbuf_animation_new_from_file(t->image->file, NULL);

	return t->image->icon;
}
#define VALID_TAG(x)	if (!g_ascii_strncasecmp (string, x ">", strlen (x ">"))) {	\
				*tag = g_strndup (string, strlen (x));		\
				*len = strlen (x) + 1;				\
				return TRUE;					\
			}							\
			(*type)++

#define VALID_OPT_TAG(x)	if (!g_ascii_strncasecmp (string, x " ", strlen (x " "))) {	\
					const gchar *c = string + strlen (x " ");	\
					gchar e = '"';					\
					gboolean quote = FALSE;				\
					while (*c) {					\
						if (*c == '"' || *c == '\'') {		\
							if (quote && (*c == e))		\
								quote = !quote;		\
							else if (!quote) {		\
								quote = !quote;		\
								e = *c;			\
							}				\
						} else if (!quote && (*c == '>'))	\
							break;				\
						c++;					\
					}						\
					if (*c) {					\
						*tag = g_strndup (string, c - string);	\
						*len = c - string + 1;			\
						return TRUE;				\
					}						\
				}							\
				(*type)++


static gboolean
gtk_imhtml_is_amp_escape (const gchar *string,
			  gchar       **replace,
			  gint        *length)
{
	static char buf[7];
	g_return_val_if_fail (string != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (!g_ascii_strncasecmp (string, "&amp;", 5)) {
		*replace = "&";
		*length = 5;
	} else if (!g_ascii_strncasecmp (string, "&lt;", 4)) {
		*replace = "<";
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&gt;", 4)) {
		*replace = ">";
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&nbsp;", 6)) {
		*replace = " ";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&copy;", 6)) {
		*replace = "©";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&quot;", 6)) {
		*replace = "\"";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&reg;", 5)) {
		*replace = "®";
		*length = 5;
	} else if (!g_ascii_strncasecmp (string, "&apos;", 6)) {
		*replace = "\'";
		*length = 6;
	} else if (*(string + 1) == '#') {
		guint pound = 0;
		if ((sscanf (string, "&#%u;", &pound) == 1) && pound != 0) {
			int buflen;
			if (*(string + 3 + (gint)log10 (pound)) != ';')
				return FALSE;
			buflen = g_unichar_to_utf8((gunichar)pound, buf);
			buf[buflen] = '\0';
			*replace = buf;
			*length = 2;
			while (isdigit ((gint) string [*length])) (*length)++;
			if (string [*length] == ';') (*length)++;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
gtk_imhtml_is_tag (const gchar *string,
		   gchar      **tag,
		   gint        *len,
		   gint        *type)
{
	char *close;
	*type = 1;


	if (!(close = strchr (string, '>')))
		return FALSE;

	VALID_TAG ("B");
	VALID_TAG ("BOLD");
	VALID_TAG ("/B");
	VALID_TAG ("/BOLD");
	VALID_TAG ("I");
	VALID_TAG ("ITALIC");
	VALID_TAG ("/I");
	VALID_TAG ("/ITALIC");
	VALID_TAG ("U");
	VALID_TAG ("UNDERLINE");
	VALID_TAG ("/U");
	VALID_TAG ("/UNDERLINE");
	VALID_TAG ("S");
	VALID_TAG ("STRIKE");
	VALID_TAG ("/S");
	VALID_TAG ("/STRIKE");
	VALID_TAG ("SUB");
	VALID_TAG ("/SUB");
	VALID_TAG ("SUP");
	VALID_TAG ("/SUP");
	VALID_TAG ("PRE");
	VALID_TAG ("/PRE");
	VALID_TAG ("TITLE");
	VALID_TAG ("/TITLE");
	VALID_TAG ("BR");
	VALID_TAG ("HR");
	VALID_TAG ("/FONT");
	VALID_TAG ("/A");
	VALID_TAG ("P");
	VALID_TAG ("/P");
	VALID_TAG ("H3");
	VALID_TAG ("/H3");
	VALID_TAG ("HTML");
	VALID_TAG ("/HTML");
	VALID_TAG ("BODY");
	VALID_TAG ("/BODY");
	VALID_TAG ("FONT");
	VALID_TAG ("HEAD");
	VALID_TAG ("/HEAD");
	VALID_TAG ("BINARY");
	VALID_TAG ("/BINARY");

	VALID_OPT_TAG ("HR");
	VALID_OPT_TAG ("FONT");
	VALID_OPT_TAG ("BODY");
	VALID_OPT_TAG ("A");
	VALID_OPT_TAG ("IMG");
	VALID_OPT_TAG ("P");
	VALID_OPT_TAG ("H3");
	VALID_OPT_TAG ("HTML");

	VALID_TAG ("CITE");
	VALID_TAG ("/CITE");
	VALID_TAG ("EM");
	VALID_TAG ("/EM");
	VALID_TAG ("STRONG");
	VALID_TAG ("/STRONG");

	VALID_OPT_TAG ("SPAN");
	VALID_TAG ("/SPAN");
	VALID_TAG ("BR/"); /* hack until gtkimhtml handles things better */
	VALID_TAG ("IMG");
	VALID_TAG("SPAN");
	VALID_OPT_TAG("BR");

	if (!g_ascii_strncasecmp(string, "!--", strlen ("!--"))) {
		gchar *e = strstr (string + strlen("!--"), "-->");
		if (e) {
			*len = e - string + strlen ("-->");
			*tag = g_strndup (string + strlen ("!--"), *len - strlen ("!---->"));
			return TRUE;
		}
	}

	*type = -1;
	*len = close - string + 1;
	*tag = g_strndup(string, *len - 1);
	return TRUE;
}

static gchar*
gtk_imhtml_get_html_opt (gchar       *tag,
			 const gchar *opt)
{
	gchar *t = tag;
	gchar *e, *a;
	gchar *val;
	gint len;
	gchar *c;
	GString *ret;

	while (g_ascii_strncasecmp (t, opt, strlen (opt))) {
		gboolean quote = FALSE;
		if (*t == '\0') break;
		while (*t && !((*t == ' ') && !quote)) {
			if (*t == '\"')
				quote = ! quote;
			t++;
		}
		while (*t && (*t == ' ')) t++;
	}

	if (!g_ascii_strncasecmp (t, opt, strlen (opt))) {
		t += strlen (opt);
	} else {
		return NULL;
	}

	if ((*t == '\"') || (*t == '\'')) {
		e = a = ++t;
		while (*e && (*e != *(t - 1))) e++;
		if  (*e == '\0') {
			return NULL;
		} else
			val = g_strndup(a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		val = g_strndup(a, e - a);
	}

	ret = g_string_new("");
	e = val;
	while(*e) {
		if(gtk_imhtml_is_amp_escape(e, &c, &len)) {
			ret = g_string_append(ret, c);
			e += len;
		} else {
			ret = g_string_append_c(ret, *e);
			e++;
		}
	}

	g_free(val);
	val = ret->str;
	g_string_free(ret, FALSE);
	return val;
}

/* Inline CSS Support - Douglas Thrift */
static gchar*
gtk_imhtml_get_css_opt (gchar       *style,
			 const gchar *opt)
{
	gchar *t = style;
	gchar *e, *a;
	gchar *val;
	gint len;
	gchar *c;
	GString *ret;

	while (g_ascii_strncasecmp (t, opt, strlen (opt))) {
/*		gboolean quote = FALSE; */
		if (*t == '\0') break;
		while (*t && !((*t == ' ') /*&& !quote*/)) {
/*			if (*t == '\"')
				quote = ! quote; */
			t++;
		}
		while (*t && (*t == ' ')) t++;
	}

	if (!g_ascii_strncasecmp (t, opt, strlen (opt))) {
		t += strlen (opt);
	} else {
		return NULL;
	}

/*	if ((*t == '\"') || (*t == '\'')) {
		e = a = ++t;
		while (*e && (*e != *(t - 1))) e++;
		if  (*e == '\0') {
			return NULL;
		} else
			val = g_strndup(a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		val = g_strndup(a, e - a);
	}*/

	e = a = t;
	while (*e && *e != ';') e++;
	val = g_strndup(a, e - a);

	ret = g_string_new("");
	e = val;
	while(*e) {
		if(gtk_imhtml_is_amp_escape(e, &c, &len)) {
			ret = g_string_append(ret, c);
			e += len;
		} else {
			ret = g_string_append_c(ret, *e);
			e++;
		}
	}

	g_free(val);
	val = ret->str;
	g_string_free(ret, FALSE);
	return val;
}

static const char *accepted_protocols[] = {
	"http://",
	"https://",
	"ftp://"
};
                                                                                                              
static const int accepted_protocols_size = 3;

/* returns if the beginning of the text is a protocol. If it is the protocol, returns the length so
   the caller knows how long the protocol string is. */
int gtk_imhtml_is_protocol(const char *text)
{
	gint i;

	for(i=0; i<accepted_protocols_size; i++){
		if( strncasecmp(text, accepted_protocols[i], strlen(accepted_protocols[i])) == 0  ){
			return strlen(accepted_protocols[i]);
		}
	}
	return 0;
}

GString* gtk_imhtml_append_text_with_images (GtkIMHtml        *imhtml,
					     const gchar      *text,
					     GtkIMHtmlOptions  options,
					     GSList           *images)
{
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter insert;
	GdkRectangle rect;
	gint pos = 0;
	GString *str = NULL;
	GtkTextIter iter;
	GtkTextMark *mark;
	gchar *ws;
	gchar *tag;
	gchar *url = NULL;
	gchar *bg = NULL;
	gint len;
	gint tlen, smilelen, wpos=0;
	gint type;
	const gchar *c;
	gchar *amp;
	gint len_protocol;

	guint	bold = 0,
		italics = 0,
		underline = 0,
		strike = 0,
		sub = 0,
		sup = 0,
		title = 0,
		pre = 0;

	GSList *fonts = NULL;
	GtkIMHtmlScalable *scalable = NULL;
	int y, height;

	printf("Appending: %s\n", text);

	g_return_val_if_fail (imhtml != NULL, NULL);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	c = text;
	len = strlen(text);
	ws = g_malloc(len + 1);
	ws[0] = 0;

	if (options & GTK_IMHTML_RETURN_LOG)
		str = g_string_new("");

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &insert, ins);

	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter);
	mark = gtk_text_buffer_create_mark (imhtml->text_buffer, NULL, &iter, /* right grav */ FALSE);

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(imhtml), &iter, &y, &height);

#if GTK_CHECK_VERSION(2,2,0)
	gtk_imhtml_primary_clipboard_clear(NULL, imhtml);
#endif
	gtk_text_buffer_move_mark (imhtml->text_buffer,
				   gtk_text_buffer_get_mark (imhtml->text_buffer, "insert"),
				   &iter);

	if(((y + height) - (rect.y + rect.height)) > height
	   && gtk_text_buffer_get_char_count(imhtml->text_buffer)){
		options |= GTK_IMHTML_NO_SCROLL;
	}

	while (pos < len) {
		if (*c == '<' && gtk_imhtml_is_tag (c + 1, &tag, &tlen, &type)) {
			c++;
			pos++;
			ws[wpos] = '\0';
			switch (type)
				{
				case 1:		/* B */
				case 2:		/* BOLD */
				case 54:	/* STRONG */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					if (bold == 0)
						gtk_imhtml_toggle_bold(imhtml);
					bold++;
					ws[0] = '\0'; wpos = 0;
					break;
				case 3:		/* /B */
				case 4:		/* /BOLD */
				case 55:	/* /STRONG */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;

					if (bold)
						bold--;
					if (bold == 0)
						gtk_imhtml_toggle_bold(imhtml);
					break;
				case 5:		/* I */
				case 6:		/* ITALIC */
				case 52:	/* EM */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (italics == 0)
						gtk_imhtml_toggle_italic(imhtml);
					italics++;
					break;
				case 7:		/* /I */
				case 8:		/* /ITALIC */
				case 53:	/* /EM */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (italics)
						italics--;
					if (italics == 0)
						gtk_imhtml_toggle_italic(imhtml);
					break;
				case 9:		/* U */
				case 10:	/* UNDERLINE */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (underline == 0)
						gtk_imhtml_toggle_underline(imhtml);
					underline++;
					break;
				case 11:	/* /U */
				case 12:	/* /UNDERLINE */
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (underline)
						underline--;
					if (underline == 0)
						gtk_imhtml_toggle_underline(imhtml);
					break;
				case 13:	/* S */
				case 14:	/* STRIKE */
					/* NEW_BIT (NEW_TEXT_BIT); */
					strike++;
					break;
				case 15:	/* /S */
				case 16:	/* /STRIKE */
					/* NEW_BIT (NEW_TEXT_BIT); */
					if (strike)
						strike--;
					break;
				case 17:	/* SUB */
					/* NEW_BIT (NEW_TEXT_BIT); */
					sub++;
					break;
				case 18:	/* /SUB */
					/* NEW_BIT (NEW_TEXT_BIT); */
					if (sub)
						sub--;
					break;
				case 19:	/* SUP */
					/* NEW_BIT (NEW_TEXT_BIT); */
					sup++;
				break;
				case 20:	/* /SUP */
					/* NEW_BIT (NEW_TEXT_BIT); */
					if (sup)
						sup--;
					break;
				case 21:	/* PRE */
					/* NEW_BIT (NEW_TEXT_BIT); */
					pre++;
					break;
				case 22:	/* /PRE */
					/* NEW_BIT (NEW_TEXT_BIT); */
					if (pre)
						pre--;
					break;
				case 23:	/* TITLE */
					/* NEW_BIT (NEW_TEXT_BIT); */
					title++;
					break;
				case 24:	/* /TITLE */
					if (title) {
						if (options & GTK_IMHTML_NO_TITLE) {
							wpos = 0;
							ws [wpos] = '\0';
						}
						title--;
					}
					break;
				case 25:	/* BR */
				case 58:	/* BR/ */
				case 61:	/* BR (opt) */
					ws[wpos] = '\n';
					wpos++;
					/* NEW_BIT (NEW_TEXT_BIT); */
					break;
				case 26:        /* HR */
				case 42:        /* HR (opt) */
					ws[wpos++] = '\n';
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					scalable = gtk_imhtml_hr_new();
					gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
					scalable->add_to(scalable, imhtml, &iter);
					scalable->scale(scalable, rect.width, rect.height);
					imhtml->scalables = g_list_append(imhtml->scalables, scalable);
					ws[0] = '\0'; wpos = 0;
					ws[wpos++] = '\n';

					break;
				case 27:	/* /FONT */
					if (fonts) {
						GtkIMHtmlFontDetail *font = fonts->data;
						if (url)
							gtk_imhtml_insert_link(imhtml, url, ws);
						else
							gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */
					
						if (font->face) {
							gtk_imhtml_toggle_fontface(imhtml, NULL);
							g_free (font->face);
						}
						if (font->fore) {
							gtk_imhtml_toggle_forecolor(imhtml, NULL);
							g_free (font->fore);
						}
						if (font->back) {
							gtk_imhtml_toggle_backcolor(imhtml, NULL);
							g_free (font->back);
						}
						if (font->sml)
							g_free (font->sml);
						g_free (font);

						if (font->size != 3)
							gtk_imhtml_font_set_size(imhtml, 3);

						fonts = fonts->next;
						if (fonts) {
							GtkIMHtmlFontDetail *font = fonts->data;
							
							if (font->face) 
								gtk_imhtml_toggle_fontface(imhtml, font->face);
							if (font->fore) 
								gtk_imhtml_toggle_forecolor(imhtml, font->fore);
							if (font->back) 
								gtk_imhtml_toggle_backcolor(imhtml, font->back);
							if (font->size != 3)
								gtk_imhtml_font_set_size(imhtml, font->size);
						}
					}
						break;
				case 28:        /* /A    */
					if (url) {
						gtk_imhtml_insert_link(imhtml, url, ws);
						g_free(url);
						ws[0] = '\0'; wpos = 0;
						url = NULL;
						ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
						gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
					}
					break;

				case 29:	/* P */
				case 30:	/* /P */
				case 31:	/* H3 */
				case 32:	/* /H3 */
				case 33:	/* HTML */
				case 34:	/* /HTML */
				case 35:	/* BODY */
				case 36:	/* /BODY */
				case 37:	/* FONT */
				case 38:	/* HEAD */
				case 39:	/* /HEAD */
				case 40:	/* BINARY */
				case 41:	/* /BINARY */
					break;
				case 43:	/* FONT (opt) */
					{
						gchar *color, *back, *face, *size, *sml;
						GtkIMHtmlFontDetail *font, *oldfont = NULL;
						color = gtk_imhtml_get_html_opt (tag, "COLOR=");
						back = gtk_imhtml_get_html_opt (tag, "BACK=");
						face = gtk_imhtml_get_html_opt (tag, "FACE=");
						size = gtk_imhtml_get_html_opt (tag, "SIZE=");
						sml = gtk_imhtml_get_html_opt (tag, "SML=");
						if (!(color || back || face || size || sml))
							break;

						if (url)
							gtk_imhtml_insert_link(imhtml, url, ws);
						else
							gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;

						font = g_new0 (GtkIMHtmlFontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS)) {
							font->fore = color;
							gtk_imhtml_toggle_forecolor(imhtml, font->fore);
						}	
						//else if (oldfont && oldfont->fore)
						//	font->fore = g_strdup(oldfont->fore);
						
						if (back && !(options & GTK_IMHTML_NO_COLOURS)) {
							font->back = back;
							gtk_imhtml_toggle_backcolor(imhtml, font->back);
						}
						//else if (oldfont && oldfont->back)
						//	font->back = g_strdup(oldfont->back);
						
						if (face && !(options & GTK_IMHTML_NO_FONTS)) {
							font->face = face;
							gtk_imhtml_toggle_fontface(imhtml, font->face);
						}
						//else if (oldfont && oldfont->face)
						//		font->face = g_strdup(oldfont->face);

						if (sml)
							font->sml = sml;
						else if (oldfont && oldfont->sml)
							font->sml = g_strdup(oldfont->sml);

						if (size && !(options & GTK_IMHTML_NO_SIZES)) {
							if (*size == '+') {
								sscanf (size + 1, "%hd", &font->size);
								font->size += 3;
							} else if (*size == '-') {
								sscanf (size + 1, "%hd", &font->size);
								font->size = MAX (0, 3 - font->size);
							} else if (isdigit (*size)) {
								sscanf (size, "%hd", &font->size);
					}
							if (font->size > 100)
								font->size = 100;
						} else if (oldfont)
							font->size = oldfont->size;
						else
							font->size = 3;
						gtk_imhtml_font_set_size(imhtml, font->size);
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 44:	/* BODY (opt) */
					if (!(options & GTK_IMHTML_NO_COLOURS)) {
						char *bgcolor = gtk_imhtml_get_html_opt (tag, "BGCOLOR=");
						if (bgcolor) {
							if (url)
								gtk_imhtml_insert_link(imhtml, url, ws);
							else
								gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
							ws[0] = '\0'; wpos = 0;
							/* NEW_BIT(NEW_TEXT_BIT); */
							if (bg)
								g_free(bg);
							bg = bgcolor;
							gtk_imhtml_toggle_backcolor(imhtml, bg);
						}
					}
					break;
				case 45:	/* A (opt) */
					{
						gchar *href = gtk_imhtml_get_html_opt (tag, "HREF=");
						if (href) {
							if (url) {
								gtk_imhtml_insert_link(imhtml, url, ws);
								g_free(url);
							} else
								gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
							ws[0] = '\0'; wpos = 0;
							url = href;
						}
					}
					break;
				case 46:	/* IMG (opt) */
				case 59:	/* IMG */
					{
						GdkPixbuf *img = NULL;
						const gchar *filename = NULL;

						if (images && images->data) {
							img = images->data;
							images = images->next;
							filename = g_object_get_data(G_OBJECT(img), "filename");
							g_object_ref(G_OBJECT(img));
						} else {
							img = gtk_widget_render_icon(GTK_WIDGET(imhtml),
							    GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_BUTTON,
							    "gtkimhtml-missing-image");
						}

						scalable = gtk_imhtml_image_new(img, filename);
						/* NEW_BIT(NEW_SCALABLE_BIT); */
						g_object_unref(G_OBJECT(img));
					}
				case 47:	/* P (opt) */
				case 48:	/* H3 (opt) */
				case 49:	/* HTML (opt) */
				case 50:	/* CITE */
				case 51:	/* /CITE */
				case 56:	/* SPAN (opt) */
					/* Inline CSS Support - Douglas Thrift
					 *
					 * color
					 * font-family
					 * font-size
					 */
					{
						gchar *style, *color, *family, *size;
						GtkIMHtmlFontDetail *font, *oldfont = NULL;
						style = gtk_imhtml_get_html_opt (tag, "style=");

						if (!style) break;

						color = gtk_imhtml_get_css_opt (style, "color: ");
						family = gtk_imhtml_get_css_opt (style,
							"font-family: ");
						size = gtk_imhtml_get_css_opt (style, "font-size: ");

						if (!(color || family || size)) {
							g_free(style);
							break;
						}

						if (url)
							gtk_imhtml_insert_link(imhtml, url, ws);
						else
							gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */

						font = g_new0 (GtkIMHtmlFontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS))
							font->fore = color;
						else if (oldfont && oldfont->fore)
							font->fore = g_strdup(oldfont->fore);

						if (oldfont && oldfont->back)
							font->back = g_strdup(oldfont->back);

						if (family && !(options & GTK_IMHTML_NO_FONTS))
							font->face = family;
						else if (oldfont && oldfont->face)
							font->face = g_strdup(oldfont->face);
						if (font->face && (atoi(font->face) > 100)) {
							g_free(font->face);
							font->face = g_strdup("100");
						}

						if (oldfont && oldfont->sml)
							font->sml = g_strdup(oldfont->sml);

						if (size && !(options & GTK_IMHTML_NO_SIZES)) {
							if (g_ascii_strcasecmp(size, "smaller") == 0)
							{
								font->size = 2;
							}
							else if (g_ascii_strcasecmp(size, "larger") == 0)
							{
								font->size = 4;
							}
							else
							{
								font->size = 3;
							}
						} else if (oldfont)
							font->size = oldfont->size;

						g_free(style);
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 57:	/* /SPAN */
					/* Inline CSS Support - Douglas Thrift */
					if (fonts) {
						GtkIMHtmlFontDetail *font = fonts->data;
						if (url)
							gtk_imhtml_insert_link(imhtml, url, ws);
						else
							gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */
						fonts = g_slist_remove (fonts, font);
						if (font->face)
							g_free (font->face);
						if (font->fore)
							g_free (font->fore);
						if (font->back)
							g_free (font->back);
						if (font->sml)
							g_free (font->sml);
						g_free (font);
					}
					break;
				case 60:    /* SPAN */
					break;
				case 62:	/* comment */
					/* NEW_BIT (NEW_TEXT_BIT); */
					ws[wpos] = '\0';
					if (url)
						gtk_imhtml_insert_link(imhtml, url, ws);
					else
						gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
					if (imhtml->show_comments)
						wpos = g_snprintf (ws, len, "%s", tag);
					/* NEW_BIT (NEW_COMMENT_BIT); */
					break;
				default:
					break;
				}
			c += tlen;
			pos += tlen;
			if(tag)
				g_free(tag); /* This was allocated back in VALID_TAG() */
		} else if (*c == '&' && gtk_imhtml_is_amp_escape (c, &amp, &tlen)) {
			while(*amp) {
				ws [wpos++] = *amp++;
			}
			c += tlen;
			pos += tlen;
		} else if (*c == '\n') {
			if (!(options & GTK_IMHTML_NO_NEWLINE)) {
				ws[wpos] = '\n';
				wpos++;
				if (url)
					gtk_imhtml_insert_link(imhtml, url, ws);
				else
					gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
				ws[0] = '\0';
				wpos = 0;
				/* NEW_BIT (NEW_TEXT_BIT); */
			}
			c++;
			pos++;
		} else if ((len_protocol = gtk_imhtml_is_protocol(c)) > 0){
			while(len_protocol--){
				/* Skip the next len_protocol characters, but make sure they're 
				   copied into the ws array.
				*/
				 ws [wpos++] = *c++;
				 pos++;
			}
		} else if (imhtml->show_smileys && (gtk_imhtml_is_smiley (imhtml, fonts, c, &smilelen) || gtk_imhtml_is_smiley(imhtml, NULL, c, &smilelen))) {
			GtkIMHtmlFontDetail *fd;

			gchar *sml = NULL;
			if (fonts) {
				fd = fonts->data;
				sml = fd->sml;
			}
			if (url)
				gtk_imhtml_insert_link(imhtml, url, ws);
			else {
				gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
			}
			wpos = g_snprintf (ws, smilelen + 1, "%s", c);
			gtk_imhtml_insert_smiley(imhtml, sml, ws);

			ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
			gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);

			c += smilelen;
			pos += smilelen;
			wpos = 0;
			ws[0] = 0;
		} else if (*c) {
			ws [wpos++] = *c++;
			pos++;
		} else {
			break;
		}
	}
	if (url)
		gtk_imhtml_insert_link(imhtml, url, ws);
	else
		gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, wpos);
	ws[0] = '\0'; wpos = 0;

	/* NEW_BIT(NEW_TEXT_BIT); */
	if (url) {
		g_free (url);
		if (str)
			str = g_string_append (str, "</A>");
	}

	while (fonts) {
		GtkIMHtmlFontDetail *font = fonts->data;
		fonts = g_slist_remove (fonts, font);
		if (font->face)
			g_free (font->face);
		if (font->fore)
			g_free (font->fore);
		if (font->back)
			g_free (font->back);
		if (font->sml)
			g_free (font->sml);
		g_free (font);
		if (str)
			str = g_string_append (str, "</FONT>");
	}

	if (str) {
		while (bold) {
			str = g_string_append (str, "</B>");
			bold--;
		}
		while (italics) {
			str = g_string_append (str, "</I>");
			italics--;
		}
		while (underline) {
			str = g_string_append (str, "</U>");
			underline--;
		}
		while (strike) {
			str = g_string_append (str, "</S>");
			strike--;
		}
		while (sub) {
			str = g_string_append (str, "</SUB>");
			sub--;
		}
		while (sup) {
			str = g_string_append (str, "</SUP>");
			sup--;
		}
		while (title) {
			str = g_string_append (str, "</TITLE>");
			title--;
		}
		while (pre) {
			str = g_string_append (str, "</PRE>");
			pre--;
		}
	}
	g_free (ws);
	if(bg)
		g_free(bg);
	gtk_imhtml_close_tags(imhtml);
	if (!(options & GTK_IMHTML_NO_SCROLL))
		gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (imhtml), mark,
					      0, TRUE, 0.0, 1.0);
	gtk_text_buffer_delete_mark (imhtml->text_buffer, mark);
	gtk_text_buffer_move_mark (imhtml->text_buffer,
				   gtk_text_buffer_get_mark (imhtml->text_buffer, "insert"),
				   &iter);
	return str;
}

void gtk_imhtml_remove_smileys(GtkIMHtml *imhtml)
{
	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	imhtml->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)gtk_smiley_tree_destroy);
	imhtml->default_smilies = gtk_smiley_tree_new();
}
void       gtk_imhtml_show_smileys     (GtkIMHtml        *imhtml,
					gboolean          show)
{
	imhtml->show_smileys = show;
}

void       gtk_imhtml_show_comments    (GtkIMHtml        *imhtml,
					gboolean          show)
{
	imhtml->show_comments = show;
}

void
gtk_imhtml_clear (GtkIMHtml *imhtml)
{
	GList *del;
	GtkTextIter start, end;
	GObject *object = g_object_ref(G_OBJECT(imhtml));

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	gtk_text_buffer_delete(imhtml->text_buffer, &start, &end);

	for(del = imhtml->format_spans; del; del = del->next) {
		GtkIMHtmlFormatSpan *span = del->data;
		if (span->start_tag)
			g_free(span->start_tag);
		if (span->end_tag)
			g_free(span->end_tag);
		g_free(span);
	}
	g_list_free(imhtml->format_spans);
	imhtml->format_spans = NULL;

	for(del = imhtml->scalables; del; del = del->next) {
		GtkIMHtmlScalable *scale = del->data;
		scale->free(scale);
	}
	g_list_free(imhtml->scalables);
	imhtml->scalables = NULL;

	imhtml->edit.bold = NULL;
	imhtml->edit.italic = NULL;
	imhtml->edit.underline = NULL;
	imhtml->edit.fontface = NULL;
	imhtml->edit.forecolor = NULL;
	imhtml->edit.backcolor = NULL;
	imhtml->edit.sizespan = NULL;
	imhtml->edit.fontsize = 3;
	printf("Emiting signal\n");
	g_signal_emit(object, signals[CLEAR_FORMAT], 0);
	g_object_unref(object);
}

void gtk_imhtml_page_up (GtkIMHtml *imhtml)
{
	GdkRectangle rect;
	GtkTextIter iter;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, rect.x,
									   rect.y - rect.height);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &iter, 0, TRUE, 0, 0);

}
void gtk_imhtml_page_down (GtkIMHtml *imhtml)
{
	GdkRectangle rect;
	GtkTextIter iter;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, rect.x,
									   rect.y + rect.height);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &iter, 0, TRUE, 0, 0);
}

/* GtkIMHtmlScalable, gtk_imhtml_image, gtk_imhtml_hr */
GtkIMHtmlScalable *gtk_imhtml_image_new(GdkPixbuf *img, const gchar *filename)
{
	GtkIMHtmlImage *im_image = g_malloc(sizeof(GtkIMHtmlImage));
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_pixbuf(img));

	GTK_IMHTML_SCALABLE(im_image)->scale = gtk_imhtml_image_scale;
	GTK_IMHTML_SCALABLE(im_image)->add_to = gtk_imhtml_image_add_to;
	GTK_IMHTML_SCALABLE(im_image)->free = gtk_imhtml_image_free;

	im_image->pixbuf = img;
	im_image->image = image;
	im_image->width = gdk_pixbuf_get_width(img);
	im_image->height = gdk_pixbuf_get_height(img);
	im_image->mark = NULL;
	im_image->filename = filename ? g_strdup(filename) : NULL;

	g_object_ref(img);
	return GTK_IMHTML_SCALABLE(im_image);
}

void gtk_imhtml_image_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;

	if(image->width > width || image->height > height){
		GdkPixbuf *new_image = NULL;
		float factor;
		int new_width = image->width, new_height = image->height;

		if(image->width > width){
			factor = (float)(width)/image->width;
			new_width = width;
			new_height = image->height * factor;
		}
		if(new_height > height){
			factor = (float)(height)/new_height;
			new_height = height;
			new_width = new_width * factor;
		}

		new_image = gdk_pixbuf_scale_simple(image->pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(image->image, new_image);
		g_object_unref(G_OBJECT(new_image));
	}
}

static void write_img_to_file(GtkWidget *w, GtkFileSelection *sel)
{
	const gchar *filename = gtk_file_selection_get_filename(sel);
	gchar *dirname;
	GtkIMHtmlImage *image = g_object_get_data(G_OBJECT(sel), "GtkIMHtmlImage");
	gchar *type = NULL;
	GError *error = NULL;
#if GTK_CHECK_VERSION(2,2,0)
	GSList *formats = gdk_pixbuf_get_formats();
#else
	char *basename = g_path_get_basename(filename);
	char *ext = strrchr(basename, '.');
#endif

	if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (filename[strlen(filename) - 1] != '/') {
			dirname = g_strconcat(filename, "/", NULL);
		} else {
			dirname = g_strdup(filename);
		}
		gtk_file_selection_set_filename(sel, dirname);
		g_free(dirname);
		return;
	}

#if GTK_CHECK_VERSION(2,2,0)
	while(formats){
		GdkPixbufFormat *format = formats->data;
		gchar **extensions = gdk_pixbuf_format_get_extensions(format);
		gpointer p = extensions;

		while(gdk_pixbuf_format_is_writable(format) && extensions && extensions[0]){
			gchar *fmt_ext = extensions[0];
			const gchar* file_ext = filename + strlen(filename) - strlen(fmt_ext);

			if(!strcmp(fmt_ext, file_ext)){
				type = gdk_pixbuf_format_get_name(format);
				break;
			}

			extensions++;
		}

		g_strfreev(p);

		if(type)
			break;

		formats = formats->next;
	}

	g_slist_free(formats);
#else
	/* this is really ugly code, but I think it will work */
	if(ext) {
		ext++;
		if(!g_ascii_strcasecmp(ext, "jpeg") || !g_ascii_strcasecmp(ext, "jpg"))
			type = g_strdup("jpeg");
		else if(!g_ascii_strcasecmp(ext, "png"))
			type = g_strdup("png");
	}

	g_free(basename);
#endif

	/* If I can't find a valid type, I will just tell the user about it and then assume
	   it's a png */
	if(!type){
		gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
						_("Unable to guess the image type based on the file extension supplied.  Defaulting to PNG."));
		type = g_strdup("png");
	}

	gdk_pixbuf_save(image->pixbuf, filename, type, &error, NULL);

	if(error){
		gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Error saving image: %s"), error->message);
		g_error_free(error);
	}

	g_free(type);
}

static void gtk_imhtml_image_save(GtkWidget *w, GtkIMHtmlImage *image)
{
	GtkWidget *sel = gtk_file_selection_new(_("Save Image"));

	if (image->filename)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(sel), image->filename);
	g_object_set_data(G_OBJECT(sel), "GtkIMHtmlImage", image);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button), "clicked",
					 G_CALLBACK(write_img_to_file), sel);

	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), sel);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->cancel_button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), sel);

	gtk_widget_show(sel);
}

static gboolean gtk_imhtml_image_clicked(GtkWidget *w, GdkEvent *event, GtkIMHtmlImage *image)
{
	GdkEventButton *event_button = (GdkEventButton *) event;

	if (event->type == GDK_BUTTON_RELEASE) {
		if(event_button->button == 3) {
			GtkWidget *img, *item, *menu;
			gchar *text = g_strdup_printf(_("_Save Image..."));
			menu = gtk_menu_new();

			/* buttons and such */
			img = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
			item = gtk_image_menu_item_new_with_mnemonic(text);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gtk_imhtml_image_save), image);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

			gtk_widget_show_all(menu);
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
							event_button->button, event_button->time);

			g_free(text);
			return TRUE;
		}
	}
	if(event->type == GDK_BUTTON_PRESS && event_button->button == 3)
		return TRUE; /* Clicking the right mouse button on a link shouldn't
						be caught by the regular GtkTextView menu */
	else
		return FALSE; /* Let clicks go through if we didn't catch anything */

}
void gtk_imhtml_image_free(GtkIMHtmlScalable *scale)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;

	g_object_unref(image->pixbuf);
	if (image->filename)
		g_free(image->filename);
	g_free(scale);
}

void gtk_imhtml_image_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;
	GtkWidget *box = gtk_event_box_new();
	GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);

	gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(image->image));

	gtk_widget_show(GTK_WIDGET(image->image));
	gtk_widget_show(box);

	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), box, anchor);
	g_signal_connect(G_OBJECT(box), "event", G_CALLBACK(gtk_imhtml_image_clicked), image);
}

GtkIMHtmlScalable *gtk_imhtml_hr_new()
{
	GtkIMHtmlHr *hr = g_malloc(sizeof(GtkIMHtmlHr));

	GTK_IMHTML_SCALABLE(hr)->scale = gtk_imhtml_hr_scale;
	GTK_IMHTML_SCALABLE(hr)->add_to = gtk_imhtml_hr_add_to;
	GTK_IMHTML_SCALABLE(hr)->free = gtk_imhtml_hr_free;

	hr->sep = gtk_hseparator_new();
	gtk_widget_set_size_request(hr->sep, 5000, 2);
	gtk_widget_show(hr->sep);

	return GTK_IMHTML_SCALABLE(hr);
}

void gtk_imhtml_hr_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	gtk_widget_set_size_request(((GtkIMHtmlHr *)scale)->sep, width, 2);
}

void gtk_imhtml_hr_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	GtkIMHtmlHr *hr = (GtkIMHtmlHr *)scale;
	GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);
	g_object_set_data(G_OBJECT(anchor), "text_tag", "<hr>");
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), hr->sep, anchor);
}

void gtk_imhtml_hr_free(GtkIMHtmlScalable *scale)
{
	g_free(scale);
}

gboolean gtk_imhtml_search_find(GtkIMHtml *imhtml, const gchar *text)
{
	GtkTextIter iter, start, end;
	gboolean new_search = TRUE;

	g_return_val_if_fail(imhtml != NULL, FALSE);
	g_return_val_if_fail(text != NULL, FALSE);

	if (imhtml->search_string && !strcmp(text, imhtml->search_string))
		new_search = FALSE;

	if (new_search) {
		gtk_imhtml_search_clear(imhtml);
		gtk_text_buffer_get_start_iter(imhtml->text_buffer, &iter);
	} else {
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter,
						 gtk_text_buffer_get_mark(imhtml->text_buffer, "search"));
	}
	imhtml->search_string = g_strdup(text);

	if (gtk_source_iter_forward_search(&iter, imhtml->search_string,
					   GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_CASE_INSENSITIVE,
					 &start, &end, NULL)) {

		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &start, 0, TRUE, 0, 0);
		gtk_text_buffer_create_mark(imhtml->text_buffer, "search", &end, FALSE);
		if (new_search) {
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "search", &iter, &end);
			do
				gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "search", &start, &end);
			while (gtk_source_iter_forward_search(&end, imhtml->search_string,
							      GTK_SOURCE_SEARCH_VISIBLE_ONLY |
							      GTK_SOURCE_SEARCH_CASE_INSENSITIVE,
							      &start, &end, NULL));
		}
		return TRUE;
	}

	gtk_imhtml_search_clear(imhtml);

	return FALSE;
}

void gtk_imhtml_search_clear(GtkIMHtml *imhtml)
{
	GtkTextIter start, end;

	g_return_if_fail(imhtml != NULL);

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);

	gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "search", &start, &end);
	if (imhtml->search_string)
		g_free(imhtml->search_string);
	imhtml->search_string = NULL;
}

/* Editable stuff */
static void insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span = NULL;
	GtkTextIter end;

	gtk_text_iter_forward_chars(iter, len);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	gtk_text_iter_forward_char(&end);

	if (!gtk_text_iter_equal(&end, iter))
		return;


	if ((span = imhtml->edit.bold)) {
		GtkTextIter bold;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &bold, span->start);
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &bold, iter);
	}

	if ((span = imhtml->edit.italic)) {
		GtkTextIter italic;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &italic, span->start);
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &italic,
				iter);
	}

	if ((span = imhtml->edit.underline)) {
		GtkTextIter underline;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &underline, span->start);
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &underline,
				iter);
	}

	if ((span = imhtml->edit.forecolor)) {
		GtkTextIter fore;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &fore, span->start);
		gtk_text_buffer_apply_tag(imhtml->text_buffer, span->tag, &fore, iter);
	}

	if ((span = imhtml->edit.backcolor)) {
		GtkTextIter back;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &back, span->start);
		gtk_text_buffer_apply_tag(imhtml->text_buffer, span->tag, &back, iter);
	}

	if ((span = imhtml->edit.fontface)) {
		GtkTextIter face;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &face, span->start);
		gtk_text_buffer_apply_tag(imhtml->text_buffer, span->tag, &face, iter);
	}

	if ((span = imhtml->edit.sizespan)) {
		GtkTextIter size;
		/* We create the tags here so that one can grow font or shrink font several times
		 * in a row without creating unnecessary tags */
		if (span->tag == NULL) {
			span->tag = gtk_text_buffer_create_tag
				(imhtml->text_buffer, NULL, "scale", (double)_point_sizes [imhtml->edit.fontsize-1], NULL);
			span->start_tag = g_strdup_printf("<font size=\"%d\">", imhtml->edit.fontsize);
			span->end_tag = g_strdup("</font>");
		}
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &size, span->start);
		gtk_text_buffer_apply_tag(imhtml->text_buffer, span->tag, &size, iter);
	}
}

void gtk_imhtml_set_editable(GtkIMHtml *imhtml, gboolean editable)
{
	gtk_text_view_set_editable(GTK_TEXT_VIEW(imhtml), editable);
	/*
	 * We need a visible caret for accessibility, so mouseless
	 * people can highlight stuff.
	 */
	/* gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(imhtml), editable); */
	imhtml->editable = editable;
	imhtml->format_functions = !editable ? 0 : -1;
}

void gtk_imhtml_set_format_functions(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons)
{
	GObject *object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[BUTTONS_UPDATE], 0, buttons);
	imhtml->format_functions = buttons;
	g_object_unref(object);
}

gboolean gtk_imhtml_get_editable(GtkIMHtml *imhtml)
{
	return imhtml->editable;
}

gboolean gtk_imhtml_toggle_bold(GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (!imhtml->edit.bold) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup("<b>");
		span->end = NULL;
		span->end_tag = g_strdup("</b>");
		span->buffer = imhtml->text_buffer;
		span->tag =  gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), "BOLD");
		imhtml->edit.bold = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		GtkTextIter start;
		span = imhtml->edit.bold;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, span->start);
		if (gtk_text_iter_equal(&start, &iter)) { /* Format turned off before any text was entered, so remove the tag */
			imhtml->format_spans = g_list_remove(imhtml->format_spans, span);
			if (span->start_tag)
				g_free(span->start_tag);
			if (span->end_tag)
				g_free(span->end_tag);
			g_free(span);
		}
		imhtml->edit.bold = NULL;
	}
	return imhtml->edit.bold != NULL;
}

gboolean gtk_imhtml_toggle_italic(GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (!imhtml->edit.italic) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup("<i>");
		span->end = NULL;
		span->end_tag = g_strdup("</i>");
		span->buffer = imhtml->text_buffer;
		span->tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), "ITALIC");
		imhtml->edit.italic = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		GtkTextIter start;
		span = imhtml->edit.italic;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, span->start);
		if (gtk_text_iter_equal(&start, &iter)) { /* Format turned off before any text was entered, so remove the tag */
			imhtml->format_spans = g_list_remove(imhtml->format_spans, span);
			if (span->start_tag)
				g_free(span->start_tag);
			if (span->end_tag)
				g_free(span->end_tag);
			g_free(span);
		}
		imhtml->edit.italic = NULL;
	}
	return imhtml->edit.italic != NULL;
}

gboolean gtk_imhtml_toggle_underline(GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (!imhtml->edit.underline) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup("<u>");
		span->end = NULL;
		span->end_tag = g_strdup("</u>");
		span->buffer = imhtml->text_buffer;
		span->tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), "UNDERLINE");
		imhtml->edit.underline = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		GtkTextIter start;
		span = imhtml->edit.underline;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, span->start);
		if (gtk_text_iter_equal(&start, &iter)) { /* Format turned off before any text was entered, so remove the tag */
			imhtml->format_spans = g_list_remove(imhtml->format_spans, span);
			if (span->start_tag)
				g_free(span->start_tag);
			if (span->end_tag)
				g_free(span->end_tag);
			g_free(span);
		}
		imhtml->edit.underline = NULL;
	}
	return imhtml->edit.underline != NULL;
}

void gtk_imhtml_font_set_size(GtkIMHtml *imhtml, gint size)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);

	imhtml->edit.fontsize = size;

	if (imhtml->edit.sizespan) {
		GtkTextIter iter2;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter2, imhtml->edit.sizespan->start);
		if (gtk_text_iter_equal(&iter2, &iter))
			return;
		span = imhtml->edit.sizespan;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	}
	if (size != -1) {
		span = g_malloc0(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->end = NULL;
		span->buffer = imhtml->text_buffer;
		span->tag = NULL;
		imhtml->edit.sizespan = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	}
}

void gtk_imhtml_font_shrink(GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (imhtml->edit.fontsize == 1)
		return;

	imhtml->edit.fontsize--;

	if (imhtml->edit.sizespan) {
		GtkTextIter iter2;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter2, imhtml->edit.sizespan->start);
		if (gtk_text_iter_equal(&iter2, &iter))
			return;
		span = imhtml->edit.sizespan;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	}

	span = g_malloc0(sizeof(GtkIMHtmlFormatSpan));
	span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	span->end = NULL;
	span->buffer = imhtml->text_buffer;
	span->tag = NULL;
	imhtml->edit.sizespan = span;
	imhtml->format_spans = g_list_append(imhtml->format_spans, span);
}

void gtk_imhtml_font_grow(GtkIMHtml *imhtml)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (imhtml->edit.fontsize == MAX_FONT_SIZE)
		return;

	imhtml->edit.fontsize++;
	if (imhtml->edit.sizespan) {
		GtkTextIter iter2;
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter2, imhtml->edit.sizespan->start);
		if (gtk_text_iter_equal(&iter2, &iter))
			return;
		span = imhtml->edit.sizespan;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	}

	span = g_malloc0(sizeof(GtkIMHtmlFormatSpan));
	span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	span->end = NULL;
	span->tag = NULL;
	span->buffer = imhtml->text_buffer;
	imhtml->edit.sizespan = span;
	imhtml->format_spans = g_list_append(imhtml->format_spans, span);
}

gboolean gtk_imhtml_toggle_forecolor(GtkIMHtml *imhtml, const char *color)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (color) { //!imhtml->edit.forecolor) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup_printf("<font color=\"%s\">", color);
		span->end = NULL;
		span->end_tag = g_strdup("</font>");
		span->buffer = imhtml->text_buffer;
		span->tag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "foreground", color, NULL);
		imhtml->edit.forecolor = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		span = imhtml->edit.forecolor;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		imhtml->edit.forecolor = NULL;
	}


	return imhtml->edit.forecolor != NULL;
}

gboolean gtk_imhtml_toggle_backcolor(GtkIMHtml *imhtml, const char *color)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (color) { //!imhtml->edit.backcolor) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup_printf("<font back=\"%s\">", color);
		span->end = NULL;
		span->end_tag = g_strdup("</font>");
		span->buffer = imhtml->text_buffer;
		span->tag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "background", color, NULL);
		imhtml->edit.backcolor = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		span = imhtml->edit.backcolor;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		imhtml->edit.backcolor = NULL;
	}
	return imhtml->edit.backcolor != NULL;
}

gboolean gtk_imhtml_toggle_fontface(GtkIMHtml *imhtml, const char *face)
{
	GtkIMHtmlFormatSpan *span;
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	if (face) { //!imhtml->edit.fontface) {
		span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
		span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		span->start_tag = g_strdup_printf("<font face=\"%s\">", face);
		span->end = NULL;
		span->end_tag = g_strdup("</font>");
		span->buffer = imhtml->text_buffer;
		span->tag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "family", face, NULL);
		imhtml->edit.fontface = span;
		imhtml->format_spans = g_list_append(imhtml->format_spans, span);
	} else {
		span = imhtml->edit.fontface;
		span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
		imhtml->edit.fontface = NULL;
	}
	return imhtml->edit.fontface != NULL;
}

void gtk_imhtml_insert_link(GtkIMHtml *imhtml, const char *url, const char *text)
{
	GtkIMHtmlFormatSpan *span = g_malloc(sizeof(GtkIMHtmlFormatSpan));
	GtkTextMark *mark = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	GtkTextTag *tag, *linktag;

	tag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, NULL);
	g_object_set_data(G_OBJECT(tag), "link_url", g_strdup(url));

	linktag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), "LINK");

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, mark);
	span->start = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	span->buffer = imhtml->text_buffer;
	span->start_tag = g_strdup_printf("<a href=\"%s\">", url);
	span->end_tag = g_strdup("</a>");
	g_signal_connect(G_OBJECT(tag), "event", G_CALLBACK(tag_event), g_strdup(url));

	gtk_text_buffer_insert_with_tags(imhtml->text_buffer, &iter, text, strlen(text), linktag, tag, NULL);
	span->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE);
	imhtml->format_spans = g_list_append(imhtml->format_spans, span);
}

void gtk_imhtml_insert_smiley(GtkIMHtml *imhtml, const char *sml, char *smiley)
{
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	GtkTextIter iter;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbufAnimation *annipixbuf = NULL;
	GtkWidget *icon = NULL;
	GtkTextChildAnchor *anchor;

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, ins);
	anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, &iter);
	g_object_set_data(G_OBJECT(anchor), "text_tag", g_strdup(smiley));

	annipixbuf = gtk_smiley_tree_image(imhtml, sml, smiley);
	if(annipixbuf) {
		if(gdk_pixbuf_animation_is_static_image(annipixbuf)) {
			pixbuf = gdk_pixbuf_animation_get_static_image(annipixbuf);
			if(pixbuf)
				icon = gtk_image_new_from_pixbuf(pixbuf);
		} else {
			icon = gtk_image_new_from_animation(annipixbuf);
		}
	}

	if (icon) {
		gtk_widget_show(icon);
		gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), icon, anchor);
	}
}

int span_compare_begin(const GtkIMHtmlFormatSpan *a, const GtkIMHtmlFormatSpan *b, GtkTextBuffer *buffer)
{
	GtkTextIter ia, ib;
	gtk_text_buffer_get_iter_at_mark(buffer, &ia, a->start);
	gtk_text_buffer_get_iter_at_mark(buffer, &ib, b->start);
	return gtk_text_iter_compare(&ia, &ib);
}

int span_compare_end(GtkIMHtmlFormatSpan *a, GtkIMHtmlFormatSpan *b)
{
	GtkTextIter ia, ib;
	gtk_text_buffer_get_iter_at_mark(a->buffer, &ia, a->start);
	gtk_text_buffer_get_iter_at_mark(b->buffer, &ib, b->start);
	/* The -1 here makes it so that if I have two spans that close at the same point, the
	 * span added second will be closed first, as in <b><i>Hello</i></b>.  Without this,
	 * it would be <b><i>Hello</b></i> */
	return gtk_text_iter_compare(&ia, &ib) - 1;
}

/* Basic notion here: traverse through the text buffer one-by-one, non-character elements, such
 * as smileys and IM images are represented by the Unicode "unknown" character.  Handle them.  Else
 * check the list of formatted strings, sorted by the position of the starting tags and apply them as
 * needed.  After applying the start tags, add the end tags to the "closers" list, which is sorted by
 * location of ending tags.  These get applied in a similar fashion.  Finally, replace <, >, &, and "
 * with their HTML equivilent. */
char *gtk_imhtml_get_markup_range(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *end)
{
	gunichar c;
	GtkIMHtmlFormatSpan *sspan = NULL, *espan = NULL;
	GtkTextIter iter, siter, eiter;
	GList *starters = imhtml->format_spans;
	GList *closers = NULL;
	GString *str = g_string_new("");
	g_list_sort_with_data(starters, (GCompareDataFunc)span_compare_begin, imhtml->text_buffer);

	gtk_text_iter_order(start, end);
	iter = *start;


	/* Initialize these to the end iter */
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &siter);
	eiter = siter;

	if (starters) {
		while (starters) {
			GtkTextIter tagend;
			sspan = (GtkIMHtmlFormatSpan*)starters->data;
			gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &siter, sspan->start);
			if (gtk_text_iter_compare(&siter, start) > 0)
				break;
			if (sspan->end)
				gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &tagend, sspan->end);
			if (sspan->end == NULL || gtk_text_iter_compare(&tagend, start) > 0) {
				str = g_string_append(str, sspan->start_tag);
				closers = g_list_insert_sorted(closers, sspan, (GCompareFunc)span_compare_end);
				espan = (GtkIMHtmlFormatSpan*)closers->data;
				/*
				 * When sending an IM, the following line causes the following warning:
				 *   Gtk: file gtktextbuffer.c: line 1794
				 *     (gtk_text_buffer_get_iter_at_mark): assertion `GTK_IS_TEXT_MARK (mark)' failed
				 *
				 * The callback path thingy to get here is:
				 * gtkconv.c, send(), "buf = gtk_imhtml_get_markup(GTK_IMHTML(gtkconv->entry));"
				 * gtkimhtml.c, gtk_imthml_get_markup(), "return gtk_imhtml_get_markup_range(imhtml, &start, &end);"
				 *
				 * I don't really know anything about gtkimhtml, but it almost seems like
				 * the line above this comments expects to find a closing html tag, but
				 * can't, for some reason.  The warning depends on how much HTML I send
				 * in my message, kind of.
				 */
				gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &eiter, espan->end);
			}
			sspan = (GtkIMHtmlFormatSpan*)starters->data;
			gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &siter, sspan->start);
			starters = starters->next;
		}
		if (!starters) {
			gtk_text_buffer_get_end_iter(imhtml->text_buffer, &siter);
			sspan = NULL;
		}
	}

	while ((c = gtk_text_iter_get_char(&iter)) != 0 && !gtk_text_iter_equal(&iter, end)) {
		if (c == 0xFFFC) {
			GtkTextChildAnchor* anchor = gtk_text_iter_get_child_anchor(&iter);
			char *text = g_object_get_data(G_OBJECT(anchor), "text_tag");
			str = g_string_append(str, text);
		} else {
			while (gtk_text_iter_equal(&eiter, &iter)) {
				/* This is where we shall insert the ending tag of
				 * this format span */
				str = g_string_append(str, espan->end_tag);
				closers = g_list_remove(closers, espan);
				if (!closers) {
					espan = NULL;
					gtk_text_buffer_get_end_iter(imhtml->text_buffer, &eiter);
				} else {
					espan = (GtkIMHtmlFormatSpan*)closers->data;
					gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &eiter, espan->end);
				}
			}
			while (gtk_text_iter_equal(&siter, &iter)) {
				/* This is where we shall insert the starting tag of
				 * this format span */
				str = g_string_append(str, sspan->start_tag);
				if (sspan->end) {
					closers = g_list_insert_sorted(closers, sspan, (GCompareFunc)span_compare_end);
					espan = (GtkIMHtmlFormatSpan*)closers->data;
					gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &eiter, espan->end);

				}
				starters = starters->next;
				if (starters) {
					sspan = (GtkIMHtmlFormatSpan*)starters->data;
					gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &siter, sspan->start);
				} else {
					sspan = NULL;
					gtk_text_buffer_get_end_iter(imhtml->text_buffer, &siter);
				}

			}

			if (c == '<')
				str = g_string_append(str, "&lt;");
			else if (c == '>')
				str = g_string_append(str, "&gt;");
			else if (c == '&')
				str = g_string_append(str, "&amp;");
			else if (c == '"')
				str = g_string_append(str, "&quot;");
			else if (c == '\n')
				str = g_string_append(str, "<br>");
			else
				str = g_string_append_unichar(str, c);
		}
		gtk_text_iter_forward_char(&iter);
	}
	while (closers) {
		GtkIMHtmlFormatSpan *span = (GtkIMHtmlFormatSpan*)closers->data;
		str = g_string_append(str, span->end_tag);
		closers = g_list_remove(closers, span);

	}
	printf("gotten: %s\n", str->str);
	return g_string_free(str, FALSE);
}

void gtk_imhtml_close_tags(GtkIMHtml *imhtml)
{

	if (imhtml->edit.bold)
		gtk_imhtml_toggle_bold(imhtml);

	if (imhtml->edit.italic)
		gtk_imhtml_toggle_italic(imhtml);

	if (imhtml->edit.underline)
		gtk_imhtml_toggle_underline(imhtml);

	if (imhtml->edit.forecolor)
		gtk_imhtml_toggle_forecolor(imhtml, NULL);

	if (imhtml->edit.backcolor)
		gtk_imhtml_toggle_backcolor(imhtml, NULL);

	if (imhtml->edit.fontface)
		gtk_imhtml_toggle_fontface(imhtml, NULL);

	if (imhtml->edit.sizespan)
		gtk_imhtml_font_set_size(imhtml, -1);

}

char *gtk_imhtml_get_markup(GtkIMHtml *imhtml)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	return gtk_imhtml_get_markup_range(imhtml, &start, &end);
}

char *gtk_imhtml_get_text(GtkIMHtml *imhtml)
{
	GtkTextIter start_iter, end_iter;
	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start_iter);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end_iter);
	return gtk_text_buffer_get_text(imhtml->text_buffer, &start_iter, &end_iter, FALSE);

}

