#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gtk_imhtml_new(a, b)
*/

/* This can't work at the moment since I don't have a typemap for
 * Gtk::TextIter.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_insert_html_at_iter(imhtml, text, options, iter)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * text
	Gaim::GtkUI::IMHtml::Options options
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_delete(imhtml, start, end)
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_insert_link(imhtml, mark, url, text)
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextMark mark
	const char * url
	const char * text
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_insert_smiley_at_iter(imhtml, sml, smiley, iter)
	Gaim::GtkUI::IMHtml imhtml
	const char * sml
	char * smiley
	Gtk::TextIter iter

void
gtk_imhtml_insert_image_at_iter(imhtml, id, iter)
	Gaim::GtkUI::IMHtml imhtml
	int id
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
gchar_own *
gtk_imhtml_get_markup_range(imhtml, start, end)
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
gchar_own *
gtk_imhtml_get_text(imhtml, start, end)
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gdk::Pixbuf.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gaim::GtkUI::IMHtml::Scalable
gtk_imhtml_image_new(img, filename, id)
	Gdk::Pixbuf img
	const gchar * filename
	int id
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_image_add_to(scale, imhtml, iter)
	Gaim::GtkUI::IMHtml::Scalable scale
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_hr_add_to(scale, imhtml, iter)
	Gaim::GtkUI::IMHtml::Scalable scale
	Gaim::GtkUI::IMHtml imhtml
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for gboolean *.
void
gtk_imhtml_get_current_format(imhtml, bold, italic, underline)
	Gaim::GtkUI::IMHtml imhtml
	gboolean * bold
	gboolean * italic
	gboolean * underline
*/

MODULE = Gaim::GtkUI::IMHtml  PACKAGE = Gaim::GtkUI::IMHtml  PREFIX = gtk_imhtml_
PROTOTYPES: ENABLE

Gaim::GtkUI::IMHtml::Smiley
gtk_imhtml_smiley_get(imhtml, sml, text)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * sml
	const gchar * text

void
gtk_imhtml_associate_smiley(imhtml, sml, smiley)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * sml
	Gaim::GtkUI::IMHtml::Smiley smiley

void
gtk_imhtml_remove_smileys(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_set_funcs(imhtml, f)
	Gaim::GtkUI::IMHtml imhtml
	Gaim::GtkUI::IMHtml::Funcs f

void
gtk_imhtml_show_comments(imhtml, show)
	Gaim::GtkUI::IMHtml imhtml
	gboolean show

const char *
gtk_imhtml_get_protocol_name(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_set_protocol_name(imhtml, protocol_name)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * protocol_name

void
gtk_imhtml_append_text(imhtml, text, options)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * text
	Gaim::GtkUI::IMHtml::Options options

void
gtk_imhtml_append_text_with_images(imhtml, text, options, unused = NULL)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * text
	Gaim::GtkUI::IMHtml::Options options
	SV *unused
PREINIT:
	GSList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(unused));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_slist_append(t_GL, SvPV(*av_fetch((AV *)SvRV(unused), i, 0), t_sl));
	}
	gtk_imhtml_append_text_with_images(imhtml, text, options, t_GL);

void
gtk_imhtml_scroll_to_end(imhtml, smooth)
	Gaim::GtkUI::IMHtml imhtml
	gboolean smooth

void
gtk_imhtml_clear(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_page_up(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_page_down(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_set_editable(imhtml, editable)
	Gaim::GtkUI::IMHtml imhtml
	gboolean editable

void
gtk_imhtml_set_whole_buffer_formatting_only(imhtml, wbo)
	Gaim::GtkUI::IMHtml imhtml
	gboolean wbo

void
gtk_imhtml_set_format_functions(imhtml, buttons)
	Gaim::GtkUI::IMHtml imhtml
	Gaim::GtkUI::IMHtml::Buttons buttons

Gaim::GtkUI::IMHtml::Buttons
gtk_imhtml_get_format_functions(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_fontface(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_forecolor(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_backcolor(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_background(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gint
gtk_imhtml_get_current_fontsize(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gboolean
gtk_imhtml_get_editable(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_clear_formatting(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_toggle_bold(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_toggle_italic(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_toggle_underline(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_toggle_strike(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_toggle_forecolor(imhtml, color)
	Gaim::GtkUI::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_backcolor(imhtml, color)
	Gaim::GtkUI::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_background(imhtml, color)
	Gaim::GtkUI::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_fontface(imhtml, face)
	Gaim::GtkUI::IMHtml imhtml
	const char * face

void
gtk_imhtml_toggle_link(imhtml, url)
	Gaim::GtkUI::IMHtml imhtml
	const char * url

void
gtk_imhtml_insert_smiley(imhtml, sml, smiley)
	Gaim::GtkUI::IMHtml imhtml
	const char * sml
	char * smiley

void
gtk_imhtml_font_set_size(imhtml, size)
	Gaim::GtkUI::IMHtml imhtml
	gint size

void
gtk_imhtml_font_shrink(imhtml)
	Gaim::GtkUI::IMHtml imhtml

void
gtk_imhtml_font_grow(imhtml)
	Gaim::GtkUI::IMHtml imhtml

gchar_own *
gtk_imhtml_get_markup(imhtml)
	Gaim::GtkUI::IMHtml imhtml

# /* ETAN Test this, and document well that it returns an arrayref */
void
gtk_imhtml_get_markup_lines(imhtml)
	Gaim::GtkUI::IMHtml imhtml
PREINIT:
	gint i;
	AV *lines;
	gchar **bufs;
PPCODE:
	bufs = gtk_imhtml_get_markup_lines(imhtml);
	lines = newAV();
	for (i = 0; bufs[i] != NULL; i++) {
	    av_push(lines, newSVpv(bufs[i], 0));
	}
	XPUSHs(sv_2mortal(newRV_noinc((SV *)lines)));

MODULE = Gaim::GtkUI::IMHtml  PACKAGE = Gaim::GtkUI::IMHtml::Scalable  PREFIX = gtk_imhtml_image_
PROTOTYPES: ENABLE

void
gtk_imhtml_image_free(scale)
	Gaim::GtkUI::IMHtml::Scalable scale

void
gtk_imhtml_image_scale(scale, width, height)
	Gaim::GtkUI::IMHtml::Scalable scale
	int width
	int height

MODULE = Gaim::GtkUI::IMHtml  PACKAGE = Gaim::GtkUI::IMHtml::Hr  PREFIX = gtk_imhtml_hr_
PROTOTYPES: ENABLE

Gaim::GtkUI::IMHtml::Scalable
gtk_imhtml_hr_new()

void
gtk_imhtml_hr_free(scale)
	Gaim::GtkUI::IMHtml::Scalable scale

void
gtk_imhtml_hr_scale(scale, width, height)
	Gaim::GtkUI::IMHtml::Scalable scale
	int width
	int height

MODULE = Gaim::GtkUI::IMHtml  PACKAGE = Gaim::GtkUI::IMHtml::Search  PREFIX = gtk_imhtml_search_
PROTOTYPES: ENABLE

gboolean
gtk_imhtml_search_find(imhtml, text)
	Gaim::GtkUI::IMHtml imhtml
	const gchar * text

void
gtk_imhtml_search_clear(imhtml)
	Gaim::GtkUI::IMHtml imhtml
