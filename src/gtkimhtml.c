/*
 * GtkIMHtml
 *
 * Copyright (C) 2000, Eric Warmenhoven <warmenhoven@yahoo.com>
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
#include "gtkimhtml.h"
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

#define TOOLTIP_TIMEOUT 500

static gboolean gtk_motion_event_notify(GtkWidget *imhtml, GdkEventMotion *event, gpointer user_data);
static gboolean gtk_leave_event_notify(GtkWidget *imhtml, GdkEventCrossing *event, gpointer user_data);

static gboolean gtk_size_allocate_cb(GtkWidget *widget, GtkAllocation *alloc, gpointer user_data);
static gint gtk_imhtml_tip (gpointer data);


/* POINT_SIZE converts from AIM font sizes to point sizes.  It probably should be redone in such a
 * way that it base the sizes off the default font size rather than using arbitrary font sizes. */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (_point_sizes [MIN ((x), MAX_FONT_SIZE) - 1])
static gint _point_sizes [] = { 8, 10, 12, 14, 20, 30, 40 };

/* The four elements present in a <FONT> tag contained in a struct */
typedef struct _FontDetail   FontDetail;
struct _FontDetail {
	gushort size;
	gchar *face;
	gchar *fore;
	gchar *back;
	gchar *sml;
};

struct _GtkSmileyTree {
	GString *values;
	GtkSmileyTree **children;
	GtkIMHtmlSmiley *image;
};

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
			index = (int) pos - (int) t->values->str;
		
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
		if (t->values) {
			for (i = 0; i < t->values->len; i++)
				list = g_slist_append (list, t->children [i]);
			g_string_free (t->values, TRUE);
			g_free (t->children);
		}
		g_free (t);
	}
}


static GtkTextViewClass *parent_class = NULL;


/* GtkIMHtml has one signal--URL_CLICKED */
enum {
	URL_CLICKED,
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

static void
gtk_imhtml_finalize (GObject *object)
{
	GtkIMHtml *imhtml = GTK_IMHTML(object);
	GList *scalables;

	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	gdk_cursor_unref(imhtml->hand_cursor);
	gdk_cursor_unref(imhtml->arrow_cursor);
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
	gobject_class->finalize = gtk_imhtml_finalize;
}

static void gtk_imhtml_init (GtkIMHtml *imhtml)
{
	GtkTextIter iter;
	imhtml->text_buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_get_end_iter (imhtml->text_buffer, &iter);
	imhtml->end = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, FALSE);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(imhtml), imhtml->text_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(imhtml), FALSE);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(imhtml), 5);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(imhtml), FALSE);
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
	
	/* When hovering over a link, we show the hand cursor--elsewhere we show the plain ol' pointer cursor */
	imhtml->hand_cursor = gdk_cursor_new (GDK_HAND2);
	imhtml->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	imhtml->show_smileys = TRUE;
	imhtml->show_comments = TRUE;

	imhtml->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)gtk_smiley_tree_destroy);
	imhtml->default_smilies = gtk_smiley_tree_new();

	g_signal_connect(G_OBJECT(imhtml), "size-allocate", G_CALLBACK(gtk_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "motion-notify-event", G_CALLBACK(gtk_motion_event_notify), NULL);
	g_signal_connect(G_OBJECT(imhtml), "leave-notify-event", G_CALLBACK(gtk_leave_event_notify), NULL);
	gtk_widget_add_events(GTK_WIDGET(imhtml), GDK_LEAVE_NOTIFY_MASK);

	imhtml->tip = NULL;
	imhtml->tip_timer = 0;
	imhtml->tip_window = NULL;

	imhtml->scalables = NULL;
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

	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
}

/* The callback for an event on a link tag. */
gboolean tag_event(GtkTextTag *tag, GObject *arg1, GdkEvent *event, GtkTextIter *arg2, char *url) {
	GdkEventButton *event_button = (GdkEventButton *) event;

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
			g_signal_emit(arg1, signals[URL_CLICKED], 0, url);
			return FALSE;
		} else if(event_button->button == 3) {
			GtkWidget *img, *item, *menu;
			struct url_data *tempdata = g_new(struct url_data, 1);
			tempdata->object = g_object_ref(arg1);
			tempdata->url = g_strdup(url);

			menu = gtk_menu_new();

			/* buttons and such */
			img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
			item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Link Location"));
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(url_copy),
					url);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

			img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
			item = gtk_image_menu_item_new_with_mnemonic(_("_Open Link in Browser"));
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(url_open),
					tempdata);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

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
		gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->arrow_cursor);
		if (GTK_IMHTML(imhtml)->tip_timer)
			g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
		GTK_IMHTML(imhtml)->tip_timer = 0;
	}
	
	if(tip){
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
	gdk_window_set_cursor(event->window, GTK_IMHTML(imhtml)->arrow_cursor);

	/* propogate the event normally */
	return FALSE;
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
			t = t->children [(int) pos - (int) t->values->str];
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
	FontDetail *font;
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

GdkPixbuf*
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
			t = t->children [(int) pos - (int) t->values->str];
		} else {
			return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;
		}
		x++;
	}

	if (!t->image->icon)
		t->image->icon = gdk_pixbuf_new_from_file(t->image->file, NULL);

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
			  gchar       *replace,
			  gint        *length)
{
	g_return_val_if_fail (string != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (!g_ascii_strncasecmp (string, "&amp;", 5)) {
		*replace = '&';
		*length = 5;
	} else if (!g_ascii_strncasecmp (string, "&lt;", 4)) {
		*replace = '<';
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&gt;", 4)) {
		*replace = '>';
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&nbsp;", 6)) {
		*replace = ' ';
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&copy;", 6)) {
		*replace = '©'; /* was: 'Â©' */
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&quot;", 6)) {
		*replace = '\"';
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&reg;", 5)) {
		*replace = '®'; /* was: 'Â®' */
		*length = 5;
	} else if (*(string + 1) == '#') {
		guint pound = 0;
		if ((sscanf (string, "&#%u;", &pound) == 1) && pound != 0) {
			if (*(string + 3 + (gint)log10 (pound)) != ';')
				return FALSE;
			*replace = (gchar)pound;
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
	*type = 1;

	if (!strchr (string, '>'))
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

	if (!g_ascii_strncasecmp(string, "!--", strlen ("!--"))) {
		gchar *e = strstr (string + strlen("!--"), "-->");
		if (e) {
			*len = e - string + strlen ("-->");
			*tag = g_strndup (string + strlen ("!--"), *len - strlen ("!---->"));
			return TRUE;
		}
	}

	return FALSE;
}

static gchar*
gtk_imhtml_get_html_opt (gchar       *tag,
			 const gchar *opt)
{
	gchar *t = tag;
	gchar *e, *a;

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
			return g_strndup (a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		return g_strndup (a, e - a);
	}
}



#define NEW_TEXT_BIT 0
#define NEW_COMMENT_BIT 2
#define NEW_SCALABLE_BIT 1
#define NEW_BIT(x)	ws [wpos] = '\0'; \
                        mark2 = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, TRUE); \
                        gtk_text_buffer_insert(imhtml->text_buffer, &iter, ws, -1); \
						gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter); \
                        gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &siter, mark2); \
                        gtk_text_buffer_delete_mark(imhtml->text_buffer, mark2); \
                        if (bold) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &siter, &iter); \
						if (italics) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &siter, &iter); \
                        if (underline) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &siter, &iter); \
                        if (strike) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "STRIKE", &siter, &iter); \
                        if (sub) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "SUB", &siter, &iter); \
                        if (sup) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "SUP", &siter, &iter); \
                        if (pre) \
                                 gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "PRE", &siter, &iter); \
                        if (bg) { \
                                texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "background", bg, NULL); \
                                gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                        } \
                        if (fonts) { \
                                 FontDetail *fd = fonts->data; \
                                 if (fd->fore) { \
	                                    texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "foreground", fd->fore, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->back) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "background", fd->back, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->face) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "font", fd->face, NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                                 if (fd->size) { \
                                            texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "size-points", (double)POINT_SIZE(fd->size), NULL); \
                                            gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 } \
                        } \
                        if (url) { \
                                 texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL); \
                                 g_signal_connect(G_OBJECT(texttag), "event", G_CALLBACK(tag_event), strdup(url)); \
                                 gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                                 texttag = gtk_text_buffer_create_tag(imhtml->text_buffer, NULL, NULL); \
                                 g_object_set_data(G_OBJECT(texttag), "link_url", g_strdup(url)); \
                                 gtk_text_buffer_apply_tag(imhtml->text_buffer, texttag, &siter, &iter); \
                        } \
                        wpos = 0; \
                        ws[0] = 0; \
                        gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter); \
                        if (x == NEW_SCALABLE_BIT) { \
								GdkRectangle rect; \
								gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect); \
								scalable->add_to(scalable, imhtml, &iter); \
								scalable->scale(scalable, rect.width, rect.height); \
								imhtml->scalables = g_list_append(imhtml->scalables, scalable); \
								gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter); \
                        } \



GString* gtk_imhtml_append_text (GtkIMHtml        *imhtml,
				 const gchar      *text,
				 gint              len,
				 GtkIMHtmlOptions  options)
{
	gint pos = 0;
	GString *str = NULL;
	GtkTextIter iter, siter;
	GtkTextMark *mark, *mark2;
	GtkTextTag *texttag;
	gchar *ws;
	gchar *tag;
	gchar *url = NULL;
	gchar *bg = NULL;
	gint tlen, smilelen, wpos=0;
	gint type;
	const gchar *c;
	gchar amp;

	guint	bold = 0,
		italics = 0,
		underline = 0,
		strike = 0,
		sub = 0,
		sup = 0,
		title = 0,
		pre = 0;	

	GSList *fonts = NULL;

	GdkRectangle rect;
	int y, height;

	GtkIMHtmlScalable *scalable = NULL;

	g_return_val_if_fail (imhtml != NULL, NULL);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (len != 0, NULL);
	
	c = text;
	if (len == -1)
		len = strlen(text);
	ws = g_malloc(len + 1);
	ws[0] = 0;

	if (options & GTK_IMHTML_RETURN_LOG)
		str = g_string_new("");

	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter);
	mark = gtk_text_buffer_create_mark (imhtml->text_buffer, NULL, &iter, /* right grav */ FALSE);

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);	
	gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(imhtml), &iter, &y, &height);

	if(((y + height) - (rect.y + rect.height)) > height 
	   && gtk_text_buffer_get_char_count(imhtml->text_buffer)){
		options |= GTK_IMHTML_NO_SCROLL;
	}

	while (pos < len) {
		if (*c == '<' && gtk_imhtml_is_tag (c + 1, &tag, &tlen, &type)) {
			c++;
			pos++;
			switch (type) 
				{
				case 1:		/* B */
				case 2:		/* BOLD */
					NEW_BIT (NEW_TEXT_BIT);
					bold++;
					break;
				case 3:		/* /B */
				case 4:		/* /BOLD */
					NEW_BIT (NEW_TEXT_BIT);
					if (bold)
						bold--;
					break;
				case 5:		/* I */
				case 6:		/* ITALIC */
					NEW_BIT (NEW_TEXT_BIT);
					italics++;
					break;
				case 7:		/* /I */
				case 8:		/* /ITALIC */
					NEW_BIT (NEW_TEXT_BIT);
					if (italics)
						italics--;
					break;
				case 9:		/* U */
				case 10:	/* UNDERLINE */
					NEW_BIT (NEW_TEXT_BIT);
					underline++;
					break;
				case 11:	/* /U */
				case 12:	/* /UNDERLINE */
					NEW_BIT (NEW_TEXT_BIT);
					if (underline)
						underline--;
					break;
				case 13:	/* S */
				case 14:	/* STRIKE */
					NEW_BIT (NEW_TEXT_BIT);
					strike++;
					break;
				case 15:	/* /S */
				case 16:	/* /STRIKE */
					NEW_BIT (NEW_TEXT_BIT);
					if (strike)
						strike--;
					break;
				case 17:	/* SUB */
					NEW_BIT (NEW_TEXT_BIT);
					sub++;
					break;
				case 18:	/* /SUB */
					NEW_BIT (NEW_TEXT_BIT);
					if (sub)
						sub--;
					break;
				case 19:	/* SUP */
					NEW_BIT (NEW_TEXT_BIT);
					sup++;
				break;
				case 20:	/* /SUP */
					NEW_BIT (NEW_TEXT_BIT);
					if (sup)
						sup--;
					break;
				case 21:	/* PRE */
					NEW_BIT (NEW_TEXT_BIT);
					pre++;
					break;
				case 22:	/* /PRE */
					NEW_BIT (NEW_TEXT_BIT);
					if (pre)
						pre--;
					break;
				case 23:	/* TITLE */
					NEW_BIT (NEW_TEXT_BIT);
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
					ws[wpos] = '\n';
					wpos++;
					NEW_BIT (NEW_TEXT_BIT);
					break;	
				case 26:        /* HR */
				case 42:        /* HR (opt) */
					ws[wpos++] = '\n';
					scalable = gaim_hr_new();
					NEW_BIT(NEW_SCALABLE_BIT);
					ws[wpos++] = '\n';
					break;
				case 27:	/* /FONT */
					if (fonts) {
						FontDetail *font = fonts->data;
						NEW_BIT (NEW_TEXT_BIT);
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
				case 28:        /* /A    */
					if (url) {
						NEW_BIT(NEW_TEXT_BIT);
						g_free(url);
						url = NULL;
						break;
					}
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
					break; 
				case 40:        /* BINARY */
					NEW_BIT (NEW_TEXT_BIT);
					break;
				case 41:        /* /BINARY */
					break;
				case 43:	/* FONT (opt) */
					{
						gchar *color, *back, *face, *size, *sml;
						FontDetail *font, *oldfont = NULL;
						color = gtk_imhtml_get_html_opt (tag, "COLOR=");
						back = gtk_imhtml_get_html_opt (tag, "BACK=");
						face = gtk_imhtml_get_html_opt (tag, "FACE=");
						size = gtk_imhtml_get_html_opt (tag, "SIZE=");
						sml = gtk_imhtml_get_html_opt (tag, "SML=");
						if (!(color || back || face || size || sml))
							break;
						
						NEW_BIT (NEW_TEXT_BIT);
						
						font = g_new0 (FontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS))
							font->fore = color;
						else if (oldfont && oldfont->fore)
							font->fore = g_strdup(oldfont->fore);

						if (back && !(options & GTK_IMHTML_NO_COLOURS))
							font->back = back;
						else if (oldfont && oldfont->back)
							font->back = g_strdup(oldfont->back);
						
						if (face && !(options & GTK_IMHTML_NO_FONTS))
							font->face = face;
						else if (oldfont && oldfont->face)
							font->face = g_strdup(oldfont->face);
						if (font->face && (atoi(font->face) > 100)) {
							g_free(font->face);
							font->face = g_strdup("100");
						}

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
						} else if (oldfont)
							font->size = oldfont->size;
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 44:	/* BODY (opt) */
					if (!(options & GTK_IMHTML_NO_COLOURS)) {
						char *bgcolor = gtk_imhtml_get_html_opt (tag, "BGCOLOR=");
						if (bgcolor) {
							NEW_BIT(NEW_TEXT_BIT);
							if (bg)
								g_free(bg);
							bg = bgcolor;
						}
					}
					break;
				case 45:	/* A (opt) */
					{
						gchar *href = gtk_imhtml_get_html_opt (tag, "HREF=");
						if (href) {
							NEW_BIT (NEW_TEXT_BIT);
							if (url)
								g_free (url);
							url = href;
						}
					}
					break;
				case 46:	/* IMG (opt) */
					{
						gchar *src = gtk_imhtml_get_html_opt (tag, "SRC=");
						gchar *id = gtk_imhtml_get_html_opt (tag, "ID=");
						gchar *datasize = gtk_imhtml_get_html_opt (tag, "DATASIZE=");
						gint im_len = datasize?atoi(datasize):0;

						if (src && id && im_len && im_len <= len - pos) {
							/* This is an embedded IM image, or is it? */
							char *tmp = NULL;
							const char *alltext;
							guchar *imagedata = NULL;

							GdkPixbufLoader *load;
							GdkPixbuf *imagepb = NULL;
							GError *error = NULL;

							tmp = g_strdup_printf("<DATA ID=\"%s\" SIZE=\"%s\">", id, datasize);
							alltext = strstr(c, tmp);
							imagedata = g_memdup(alltext + strlen(tmp), im_len);

							g_free(tmp);

							load = gdk_pixbuf_loader_new();
							if (!gdk_pixbuf_loader_write(load, imagedata, im_len, &error)){
								fprintf(stderr, "IM Image corrupted or unreadable.: %s\n", error->message);
							} else {
								imagepb = gdk_pixbuf_loader_get_pixbuf(load);
								if (imagepb) {
									scalable = gaim_im_image_new(imagepb);
									NEW_BIT(NEW_SCALABLE_BIT);
								}
							}

							gdk_pixbuf_loader_close(load, NULL);


							g_free(imagedata);
							g_free(id);
							g_free(datasize);
							g_free(src);

							break;
						}
						g_free(id);
						g_free(datasize);
						g_free(src);
					}
				case 47:	/* P (opt) */
				case 48:	/* H3 (opt) */
					break;
				case 49:	/* comment */
					NEW_BIT (NEW_TEXT_BIT);
					if (imhtml->show_comments)
						wpos = g_snprintf (ws, len, "%s", tag);
					NEW_BIT (NEW_COMMENT_BIT);
					break;
				default:
					break;	
				}
			c += tlen;
			pos += tlen;
			if(tag)
				g_free(tag); /* This was allocated back in VALID_TAG() */
		} else if (*c == '&' && gtk_imhtml_is_amp_escape (c, &amp, &tlen)) {
			ws [wpos++] = amp;
			c += tlen;
			pos += tlen;
		} else if (*c == '\n') {
			if (!(options & GTK_IMHTML_NO_NEWLINE)) {
				ws[wpos] = '\n';
				wpos++;
				NEW_BIT (NEW_TEXT_BIT);
			}
			c++;
			pos++;
		} else if (imhtml->show_smileys && (gtk_imhtml_is_smiley (imhtml, fonts, c, &smilelen) || gtk_imhtml_is_smiley(imhtml, NULL, c, &smilelen))) {
			FontDetail *fd;
			gchar *sml = NULL;
			if (fonts) {
				fd = fonts->data;
				sml = fd->sml;
			}
			NEW_BIT (NEW_TEXT_BIT);
			wpos = g_snprintf (ws, smilelen + 1, "%s", c);
			gtk_text_buffer_insert_pixbuf(imhtml->text_buffer, &iter, gtk_smiley_tree_image (imhtml, sml, ws));
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
	
	NEW_BIT(NEW_TEXT_BIT);
	if (url) {
		g_free (url);
		if (str)
			str = g_string_append (str, "</A>");
	}
	
	while (fonts) {
		FontDetail *font = fonts->data;
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
	if (!(options & GTK_IMHTML_NO_SCROLL))
		gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (imhtml), mark,
					      0, TRUE, 0.0, 1.0);
	gtk_text_buffer_delete_mark (imhtml->text_buffer, mark);
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
	GtkTextIter start, end;
	
	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	gtk_text_buffer_delete(imhtml->text_buffer, &start, &end);
}

void gtk_imhtml_page_up (GtkIMHtml *imhtml)
{

}
void       gtk_imhtml_page_down        (GtkIMHtml        *imhtml){}

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

static gboolean gtk_size_allocate_cb(GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	static GdkRectangle old_rect = {0,0,0,0};
	GdkRectangle rect;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(widget), &rect);

	if(old_rect.width && (old_rect.width != rect.width || old_rect.height != rect.height)){
		GList *iter = GTK_IMHTML(widget)->scalables;

		while(iter){
			GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(iter->data);
			scale->scale(scale, rect.width, rect.height);

			iter = iter->next;
		}
	}

	old_rect = rect;
	return FALSE;
}

/* GtkIMHtmlScalable, gaim_im_image, gaim_hr */
GtkIMHtmlScalable *gaim_im_image_new(GdkPixbuf *img)
{
	gaim_im_image *im_image = g_malloc(sizeof(gaim_im_image));

	GTK_IMHTML_SCALABLE(im_image)->scale = gaim_im_image_scale;
	GTK_IMHTML_SCALABLE(im_image)->add_to = gaim_im_image_add_to;
	GTK_IMHTML_SCALABLE(im_image)->free = gaim_im_image_free;
	im_image->image = img;
	im_image->width = gdk_pixbuf_get_width(img);
	im_image->height = gdk_pixbuf_get_height(img);
	im_image->imhtml = NULL;
	im_image->mark = NULL;

	return GTK_IMHTML_SCALABLE(im_image);
}

void gaim_im_image_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	gaim_im_image *image = (gaim_im_image *)scale;

	if(image->width > width || image->height > height){
		GdkPixbuf *new_image = NULL;
		GtkTextIter start, end;
		float factor;
		int new_width = image->width, new_height = image->height;

		gtk_text_buffer_get_iter_at_mark(image->imhtml->text_buffer, &start, image->mark);
		end = start;
		gtk_text_iter_forward_char(&end);
		gtk_text_buffer_delete(image->imhtml->text_buffer, &start, &end);

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

		gtk_text_buffer_get_iter_at_mark(image->imhtml->text_buffer, &start, image->mark);
		new_image = gdk_pixbuf_scale_simple(image->image, new_width, new_height, GDK_INTERP_BILINEAR);
		gtk_text_buffer_insert_pixbuf(image->imhtml->text_buffer, &start, new_image);

		g_object_unref(G_OBJECT(new_image));
	}
}

void gaim_im_image_free(GtkIMHtmlScalable *scale)
{
	gaim_im_image *image = (gaim_im_image *)scale;

	g_object_unref(image->image);
	g_free(scale);
}

void gaim_im_image_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	gaim_im_image *image = (gaim_im_image *)scale;

	image->mark = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, iter, TRUE);
	gtk_text_buffer_insert_pixbuf(imhtml->text_buffer, iter, image->image);
	image->imhtml = imhtml;
}

GtkIMHtmlScalable *gaim_hr_new()
{
	gaim_hr *hr = g_malloc(sizeof(gaim_hr));

	GTK_IMHTML_SCALABLE(hr)->scale = gaim_hr_scale;
	GTK_IMHTML_SCALABLE(hr)->add_to = gaim_hr_add_to;
	GTK_IMHTML_SCALABLE(hr)->free = gaim_hr_free;

	hr->sep = gtk_hseparator_new();
	gtk_widget_set_size_request(hr->sep, 5000, 2);
	gtk_widget_show(hr->sep);

	return GTK_IMHTML_SCALABLE(hr);
}

void gaim_hr_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	/* FIXME: we need to figure out why this causes infinite looping
	gtk_widget_set_size_request(((gaim_hr *)scale)->sep, width, 2);
	*/
}

void gaim_hr_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	gaim_hr *hr = (gaim_hr *)scale;
	GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);

	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), hr->sep, anchor);
}

void gaim_hr_free(GtkIMHtmlScalable *scale)
{
/*	gtk_widget_destroy(((gaim_hr *)scale)->sep); */
	g_free(scale);
}
