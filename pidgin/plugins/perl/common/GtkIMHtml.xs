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
	Pidgin::IMHtml imhtml
	const gchar * text
	Pidgin::IMHtml::Options options
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_delete(imhtml, start, end)
	Pidgin::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_insert_link(imhtml, mark, url, text)
	Pidgin::IMHtml imhtml
	Gtk::TextMark mark
	const char * url
	const char * text
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_insert_smiley_at_iter(imhtml, sml, smiley, iter)
	Pidgin::IMHtml imhtml
	const char * sml
	char * smiley
	Gtk::TextIter iter

void
gtk_imhtml_insert_image_at_iter(imhtml, id, iter)
	Pidgin::IMHtml imhtml
	int id
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
gchar_own *
gtk_imhtml_get_markup_range(imhtml, start, end)
	Pidgin::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
gchar_own *
gtk_imhtml_get_text(imhtml, start, end)
	Pidgin::IMHtml imhtml
	Gtk::TextIter start
	Gtk::TextIter end
*/

/* This can't work at the moment since I don't have a typemap for Gdk::Pixbuf.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Pidgin::IMHtml::Scalable
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
	Pidgin::IMHtml::Scalable scale
	Pidgin::IMHtml imhtml
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gtk_imhtml_hr_add_to(scale, imhtml, iter)
	Pidgin::IMHtml::Scalable scale
	Pidgin::IMHtml imhtml
	Gtk::TextIter iter
*/

/* This can't work at the moment since I don't have a typemap for gboolean *.
void
gtk_imhtml_get_current_format(imhtml, bold, italic, underline)
	Pidgin::IMHtml imhtml
	gboolean * bold
	gboolean * italic
	gboolean * underline
*/

MODULE = Pidgin::IMHtml  PACKAGE = Pidgin::IMHtml  PREFIX = gtk_imhtml_
PROTOTYPES: ENABLE

Pidgin::IMHtml::Smiley
gtk_imhtml_smiley_get(imhtml, sml, text)
	Pidgin::IMHtml imhtml
	const gchar * sml
	const gchar * text

void
gtk_imhtml_associate_smiley(imhtml, sml, smiley)
	Pidgin::IMHtml imhtml
	const gchar * sml
	Pidgin::IMHtml::Smiley smiley

void
gtk_imhtml_remove_smileys(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_set_funcs(imhtml, f)
	Pidgin::IMHtml imhtml
	Pidgin::IMHtml::Funcs f

void
gtk_imhtml_show_comments(imhtml, show)
	Pidgin::IMHtml imhtml
	gboolean show

const char *
gtk_imhtml_get_protocol_name(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_set_protocol_name(imhtml, protocol_name)
	Pidgin::IMHtml imhtml
	const gchar * protocol_name

void
gtk_imhtml_append_text(imhtml, text, options)
	Pidgin::IMHtml imhtml
	const gchar * text
	Pidgin::IMHtml::Options options

void
gtk_imhtml_append_text_with_images(imhtml, text, options, unused = NULL)
	Pidgin::IMHtml imhtml
	const gchar * text
	Pidgin::IMHtml::Options options
	SV *unused
PREINIT:
	GSList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(unused));

	for (i = 0; i <= t_len; i++) {
		STRLEN t_sl;
		t_GL = g_slist_append(t_GL, SvPV(*av_fetch((AV *)SvRV(unused), i, 0), t_sl));
	}
	gtk_imhtml_append_text_with_images(imhtml, text, options, t_GL);

void
gtk_imhtml_scroll_to_end(imhtml, smooth)
	Pidgin::IMHtml imhtml
	gboolean smooth

void
gtk_imhtml_clear(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_page_up(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_page_down(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_set_editable(imhtml, editable)
	Pidgin::IMHtml imhtml
	gboolean editable

void
gtk_imhtml_set_whole_buffer_formatting_only(imhtml, wbo)
	Pidgin::IMHtml imhtml
	gboolean wbo

void
gtk_imhtml_set_format_functions(imhtml, buttons)
	Pidgin::IMHtml imhtml
	Pidgin::IMHtml::Buttons buttons

Pidgin::IMHtml::Buttons
gtk_imhtml_get_format_functions(imhtml)
	Pidgin::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_fontface(imhtml)
	Pidgin::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_forecolor(imhtml)
	Pidgin::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_backcolor(imhtml)
	Pidgin::IMHtml imhtml

gchar_own *
gtk_imhtml_get_current_background(imhtml)
	Pidgin::IMHtml imhtml

gint
gtk_imhtml_get_current_fontsize(imhtml)
	Pidgin::IMHtml imhtml

gboolean
gtk_imhtml_get_editable(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_clear_formatting(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_toggle_bold(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_toggle_italic(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_toggle_underline(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_toggle_strike(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_toggle_forecolor(imhtml, color)
	Pidgin::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_backcolor(imhtml, color)
	Pidgin::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_background(imhtml, color)
	Pidgin::IMHtml imhtml
	const char * color

void
gtk_imhtml_toggle_fontface(imhtml, face)
	Pidgin::IMHtml imhtml
	const char * face

void
gtk_imhtml_toggle_link(imhtml, url)
	Pidgin::IMHtml imhtml
	const char * url

void
gtk_imhtml_insert_smiley(imhtml, sml, smiley)
	Pidgin::IMHtml imhtml
	const char * sml
	char * smiley

void
gtk_imhtml_font_set_size(imhtml, size)
	Pidgin::IMHtml imhtml
	gint size

void
gtk_imhtml_font_shrink(imhtml)
	Pidgin::IMHtml imhtml

void
gtk_imhtml_font_grow(imhtml)
	Pidgin::IMHtml imhtml

gchar_own *
gtk_imhtml_get_markup(imhtml)
	Pidgin::IMHtml imhtml

# /* ETAN Test this, and document well that it returns an arrayref */
void
gtk_imhtml_get_markup_lines(imhtml)
	Pidgin::IMHtml imhtml
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

MODULE = Pidgin::IMHtml  PACKAGE = Pidgin::IMHtml::Scalable  PREFIX = gtk_imhtml_image_
PROTOTYPES: ENABLE

void
gtk_imhtml_image_free(scale)
	Pidgin::IMHtml::Scalable scale

void
gtk_imhtml_image_scale(scale, width, height)
	Pidgin::IMHtml::Scalable scale
	int width
	int height

MODULE = Pidgin::IMHtml  PACKAGE = Pidgin::IMHtml::Hr  PREFIX = gtk_imhtml_hr_
PROTOTYPES: ENABLE

Pidgin::IMHtml::Scalable
gtk_imhtml_hr_new()

void
gtk_imhtml_hr_free(scale)
	Pidgin::IMHtml::Scalable scale

void
gtk_imhtml_hr_scale(scale, width, height)
	Pidgin::IMHtml::Scalable scale
	int width
	int height

MODULE = Pidgin::IMHtml  PACKAGE = Pidgin::IMHtml::Search  PREFIX = gtk_imhtml_search_
PROTOTYPES: ENABLE

gboolean
gtk_imhtml_search_find(imhtml, text)
	Pidgin::IMHtml imhtml
	const gchar * text

void
gtk_imhtml_search_clear(imhtml)
	Pidgin::IMHtml imhtml
