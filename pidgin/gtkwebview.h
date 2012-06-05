/**
 * @file gtkwebview.h Wrapper over the Gtk WebKitWebView component
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
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
 *
 */

#ifndef _PIDGIN_WEBVIEW_H_
#define _PIDGIN_WEBVIEW_H_

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define GTK_TYPE_WEBVIEW            (gtk_webview_get_type())
#define GTK_WEBVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_WEBVIEW, GtkWebView))
#define GTK_WEBVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_WEBVIEW, GtkWebViewClass))
#define GTK_IS_WEBVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_WEBVIEW))
#define GTK_IS_WEBVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_WEBVIEW))
#define GTK_WEBVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_WEBVIEW, GtkWebViewClass))

typedef enum {
	GTK_WEBVIEW_BOLD          = 1 << 0,
	GTK_WEBVIEW_ITALIC        = 1 << 1,
	GTK_WEBVIEW_UNDERLINE     = 1 << 2,
	GTK_WEBVIEW_GROW          = 1 << 3,
	GTK_WEBVIEW_SHRINK        = 1 << 4,
	GTK_WEBVIEW_FACE          = 1 << 5,
	GTK_WEBVIEW_FORECOLOR     = 1 << 6,
	GTK_WEBVIEW_BACKCOLOR     = 1 << 7,
	GTK_WEBVIEW_LINK          = 1 << 8,
	GTK_WEBVIEW_IMAGE         = 1 << 9,
	GTK_WEBVIEW_SMILEY        = 1 << 10,
	GTK_WEBVIEW_LINKDESC      = 1 << 11,
	GTK_WEBVIEW_STRIKE        = 1 << 12,
	/** Show custom smileys when appropriate. */
	GTK_WEBVIEW_CUSTOM_SMILEY = 1 << 13,
	GTK_WEBVIEW_ALL           = -1
} GtkWebViewButtons;

typedef struct _GtkWebView GtkWebView;
typedef struct _GtkWebViewClass GtkWebViewClass;

struct _GtkWebView
{
	WebKitWebView parent;
};

struct _GtkWebViewClass
{
	WebKitWebViewClass parent;

	void (*buttons_update)(GtkWebView *, GtkWebViewButtons);
	void (*toggle_format)(GtkWebView *, GtkWebViewButtons);
	void (*clear_format)(GtkWebView *);
	void (*update_format)(GtkWebView *);
	void (*changed)(GtkWebView *);
};

G_BEGIN_DECLS

/**
 * Returns the GType for a GtkWebView widget
 *
 * @return The GType for GtkWebView widget
 */
GType gtk_webview_get_type(void);

/**
 * Create a new GtkWebView object
 *
 * @return A GtkWidget corresponding to the GtkWebView object
 */
GtkWidget *gtk_webview_new(void);

/**
 * TODO WEBKIT: Right now this just tests whether an append has been called
 * since the last clear or since the Widget was created.  So it does not
 * test for load_string's called in between.
 *
 * @param webview The GtkWebView object
 *
 * @return gboolean indicating whether the webview is empty
 */
gboolean gtk_webview_is_empty(GtkWebView *webview);

/**
 * A very basic routine to append html, which can be considered
 * equivalent to a "document.write" using JavaScript.
 *
 * @param webview The GtkWebView object
 * @param markup  The html markup to append
 */
void gtk_webview_append_html(GtkWebView *webview, const char *markup);

/**
 * Rather than use webkit_webview_load_string, this routine
 * parses and displays the \<img id=?\> tags that make use of the
 * Pidgin imgstore.
 *
 * @param webview The GtkWebView object
 * @param html    The HTML content to load
 */
void gtk_webview_load_html_string(GtkWebView *webview, const char *html);

/**
 * Execute the JavaScript only after the webkit_webview_load_string
 * loads completely. We also guarantee that the scripts are executed
 * in the order they are called here. This is useful to avoid race
 * conditions when calling JS functions immediately after opening the
 * page.
 *
 * @param webview The GtkWebView object
 * @param script  The script to execute
 */
void gtk_webview_safe_execute_script(GtkWebView *webview, const char *script);

/**
 * A convenience routine to quote a string for use as a JavaScript
 * string. For instance, "hello 'world'" becomes "'hello \\'world\\''"
 *
 * @param str The string to escape and quote
 *
 * @return The quoted string
 */
char *gtk_webview_quote_js_string(const char *str);

/**
 * Set the vertical adjustment for the GtkWebView.
 *
 * @param webview  The GtkWebView object
 * @param vadj     The GtkAdjustment that control the webview
 */
void gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj);

/**
 * Scrolls the Webview to the end of its contents.
 *
 * @param webview The GtkWebView object
 * @param smooth  A boolean indicating if smooth scrolling should be used
 */
void gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth);

/**
 * Scrolls a GtkWebView up by one page.
 *
 * @param webview The GtkWebView.
 */
void gtk_webview_page_up(GtkWebView *webview);

/**
 * Scrolls a GtkWebView down by one page.
 *
 * @param webview The GtkWebView.
 */
void gtk_webview_page_down(GtkWebView *webview);

/**
 * Enables or disables editing in a GtkWebView.
 *
 * @param webview  The GtkWebView
 * @param editable @c TRUE to make the widget editable, or @c FALSE otherwise.
 */
void gtk_webview_set_editable(GtkWebView *webview, gboolean editable);

/**
 * Setup formatting for a GtkWebView depending on the flags specified.
 *
 * @param webview The GtkWebView.
 * @param flags   The connection flags describing the allowed formatting.
 */
void gtk_webview_setup_entry(GtkWebView *webview, PurpleConnectionFlags flags);

/**
 * Enables or disables whole buffer formatting only (wbfo) in a GtkWebView.
 * In this mode formatting options to the buffer take effect for the entire
 * buffer instead of specific text.
 *
 * @param webview The GtkWebView
 * @param wbfo    @c TRUE to enable the mode, or @c FALSE otherwise.
 */
void gtk_webview_set_whole_buffer_formatting_only(GtkWebView *webview,
                                                  gboolean wbfo);

/**
 * Indicates which formatting functions to enable and disable in a GtkWebView.
 *
 * @param webview The GtkWebView
 * @param buttons A GtkWebViewButtons bitmask indicating which functions to use
 */
void gtk_webview_set_format_functions(GtkWebView *webview,
                                      GtkWebViewButtons buttons);

/**
 * Returns which formatting functions are enabled in a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return A GtkWebViewButtons bitmask indicating which functions to are enabled
 */
GtkWebViewButtons gtk_webview_get_format_functions(GtkWebView *webview);

/**
 * Sets each boolean to @c TRUE or @c FALSE to indicate if that formatting
 * option is enabled at the current position in a GtkWebView.
 *
 * @param webview       The GtkWebView
 * @param bold          The boolean to set for bold or @c NULL.
 * @param italic        The boolean to set for italic or @c NULL.
 * @param underline     The boolean to set for underline or @c NULL.
 * @param strikethrough The boolean to set for strikethrough or @c NULL.
 */
void gtk_webview_get_current_format(GtkWebView *webview, gboolean *bold,
                                    gboolean *italic, gboolean *underline,
                                    gboolean *strike);

/**
 * Returns a string containing the selected font face at the current position
 * in a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return A string containing the font face or @c NULL if none is set.
 */
char *gtk_webview_get_current_fontface(GtkWebView *webview);

/**
 * Returns a string containing the selected foreground color at the current
 * position in a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return A string containing the foreground color or @c NULL if none is set.
 */
char *gtk_webview_get_current_forecolor(GtkWebView *webview);

/**
 * Returns a string containing the selected font background color at the current
 * position in a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return A string containing the background color or @c NULL if none is set.
 */
char *gtk_webview_get_current_backcolor(GtkWebView *webview);

/**
 * Returns a integer containing the selected HTML font size at the current
 * position in a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return The HTML font size.
 */
gint gtk_webview_get_current_fontsize(GtkWebView *webview);

/**
 * Checks whether a GtkWebView is marked as editable.
 *
 * @param webview The GtkWebView
 *
 * @return @c TRUE if the IM/HTML is editable, or @c FALSE otherwise.
 */
gboolean gtk_webview_get_editable(GtkWebView *webview);

/**
 * Gets the content of the head element of a GtkWebView as HTML.
 *
 * @param webview The GtkWebView
 *
 * @return The HTML from the head element.
 */
gchar *gtk_webview_get_head_html(GtkWebView *webview);

/**
 * Gets the HTML content of a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return The HTML that is currently displayed.
 */
gchar *gtk_webview_get_body_html(GtkWebView *webview);

/**
 * Gets the text content of a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return The HTML-free text that is currently displayed.
 */
gchar *gtk_webview_get_body_text(GtkWebView *webview);

/**
 * Gets the selected text of a GtkWebView.
 *
 * @param webview The GtkWebView
 *
 * @return The HTML-free text that is currently selected, or NULL if nothing is
 *         currently selected.
 */
gchar *gtk_webview_get_selected_text(GtkWebView *webview);

/**
 * Clear all the formatting on a GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_clear_formatting(GtkWebView *webview);

/**
 * Toggles bold at the cursor location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_toggle_bold(GtkWebView *webview);

/**
 * Toggles italic at the cursor location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_toggle_italic(GtkWebView *webview);

/**
 * Toggles underline at the cursor location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_toggle_underline(GtkWebView *webview);

/**
 * Toggles strikethrough at the cursor location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_toggle_strike(GtkWebView *webview);

/**
 * Toggles a foreground color at the current location or selection in a
 * GtkWebView.
 *
 * @param webview The GtkWebView
 * @param color  The HTML-style color, or @c NULL or "" to clear the color.
 *
 * @return @c TRUE if a color was set, or @c FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_forecolor(GtkWebView *webview, const char *color);

/**
 * Toggles a background color at the current location or selection in a
 * GtkWebView.
 *
 * @param webview The GtkWebView
 * @param color  The HTML-style color, or @c NULL or "" to clear the color.
 *
 * @return @c TRUE if a color was set, or @c FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_backcolor(GtkWebView *webview, const char *color);

/**
 * Toggles a font face at the current location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 * @param face   The font face name, or @c NULL or "" to clear the font.
 *
 * @return @c TRUE if a font name was set, or @c FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_fontface(GtkWebView *webview, const char *face);

/**
 * Sets the font size at the current location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 * @param size   The HTML font size to use.
 */
void gtk_webview_font_set_size(GtkWebView *webview, gint size);

/**
 * Decreases the font size by 1 at the current location or selection in a
 * GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_font_shrink(GtkWebView *webview);

/**
 * Increases the font size by 1 at the current location or selection in a
 * GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_font_grow(GtkWebView *webview);

/**
 * Inserts a horizontal rule at the current location or selection in a
 * GtkWebView.
 *
 * @param webview The GtkWebView
 */
void gtk_webview_insert_hr(GtkWebView *webview);

/**
 * Inserts a link at the current location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 * @param url     The URL of the link
 * @param desc    The text description of the link. If not supplied, the URL is
 *                used instead.
 */
void gtk_webview_insert_link(GtkWebView *webview, const char *url, const char *desc);

/**
 * Inserts an image at the current location or selection in a GtkWebView.
 *
 * @param webview The GtkWebView
 * @param id      The PurpleStoredImage id
 */
void gtk_webview_insert_image(GtkWebView *webview, int id);

G_END_DECLS

#endif /* _PIDGIN_WEBVIEW_H_ */

