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
/**
 * SECTION:gtkwebview
 * @section_id: pidgin-gtkwebview
 * @short_description: <filename>gtkwebview.h</filename>
 * @title: WebKitWebView Wrapper
 *
 * Wrapper over the Gtk WebKitWebView component.
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

typedef enum {
	GTK_WEBVIEW_SMILEY_CUSTOM = 1 << 0
} GtkWebViewSmileyFlags;

typedef enum {
	GTK_WEBVIEW_ACTION_BOLD,
	GTK_WEBVIEW_ACTION_ITALIC,
	GTK_WEBVIEW_ACTION_UNDERLINE,
	GTK_WEBVIEW_ACTION_STRIKE,
	GTK_WEBVIEW_ACTION_LARGER,
#if 0
	GTK_WEBVIEW_ACTION_NORMAL,
#endif
	GTK_WEBVIEW_ACTION_SMALLER,
	GTK_WEBVIEW_ACTION_FONTFACE,
	GTK_WEBVIEW_ACTION_FGCOLOR,
	GTK_WEBVIEW_ACTION_BGCOLOR,
	GTK_WEBVIEW_ACTION_CLEAR,
	GTK_WEBVIEW_ACTION_IMAGE,
	GTK_WEBVIEW_ACTION_LINK,
	GTK_WEBVIEW_ACTION_HR,
	GTK_WEBVIEW_ACTION_SMILEY,
	GTK_WEBVIEW_ACTION_ATTENTION
} GtkWebViewAction;

typedef struct _GtkWebView GtkWebView;
typedef struct _GtkWebViewClass GtkWebViewClass;
typedef struct _GtkWebViewSmiley GtkWebViewSmiley;

struct _GtkWebView
{
	WebKitWebView parent;
};

struct _GtkWebViewClass
{
	WebKitWebViewClass parent;

	GList *protocols;

	void (*buttons_update)(GtkWebView *, GtkWebViewButtons);
	void (*toggle_format)(GtkWebView *, GtkWebViewButtons);
	void (*clear_format)(GtkWebView *);
	void (*update_format)(GtkWebView *);
	void (*changed)(GtkWebView *);
	void (*html_appended)(GtkWebView *, WebKitDOMRange *);
};

G_BEGIN_DECLS

/**
 * gtk_webview_get_type:
 *
 * Returns the GType for a GtkWebView widget
 *
 * Returns: The GType for GtkWebView widget
 */
GType gtk_webview_get_type(void);

/**
 * gtk_webview_new:
 * @editable: Whether this GtkWebView will be user-editable
 *
 * Create a new GtkWebView object
 *
 * Returns: A GtkWidget corresponding to the GtkWebView object
 */
GtkWidget *gtk_webview_new(gboolean editable);

/**
 * gtk_webview_append_html:
 * @webview: The GtkWebView object
 * @markup:  The html markup to append
 *
 * A very basic routine to append html, which can be considered
 * equivalent to a "document.write" using JavaScript.
 */
void gtk_webview_append_html(GtkWebView *webview, const char *markup);

/**
 * gtk_webview_load_html_string:
 * @webview: The GtkWebView object
 * @html:    The HTML content to load
 *
 * Requests loading of the given content.
 */
void gtk_webview_load_html_string(GtkWebView *webview, const char *html);

/**
 * gtk_webview_load_html_string_with_selection:
 * @webview: The GtkWebView object
 * @html:    The HTML content to load
 *
 * Requests loading of the given content and sets the selection. You must
 * include an anchor tag with id='caret' in the HTML string, which will be
 * used to set the selection. This tag is then removed so that querying the
 * WebView's HTML contents will no longer return it.
 */
void gtk_webview_load_html_string_with_selection(GtkWebView *webview, const char *html);

/**
 * gtk_webview_safe_execute_script:
 * @webview: The GtkWebView object
 * @script:  The script to execute
 *
 * Execute the JavaScript only after the webkit_webview_load_string
 * loads completely. We also guarantee that the scripts are executed
 * in the order they are called here. This is useful to avoid race
 * conditions when calling JS functions immediately after opening the
 * page.
 */
void gtk_webview_safe_execute_script(GtkWebView *webview, const char *script);

/**
 * gtk_webview_quote_js_string:
 * @str: The string to escape and quote
 *
 * A convenience routine to quote a string for use as a JavaScript
 * string. For instance, "hello 'world'" becomes "'hello \\'world\\''"
 *
 * Returns: The quoted string
 */
char *gtk_webview_quote_js_string(const char *str);

/**
 * gtk_webview_set_vadjustment:
 * @webview:  The GtkWebView object
 * @vadj:     The GtkAdjustment that control the webview
 *
 * Set the vertical adjustment for the GtkWebView.
 */
void gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj);

/**
 * gtk_webview_scroll_to_end:
 * @webview: The GtkWebView object
 * @smooth:  A boolean indicating if smooth scrolling should be used
 *
 * Scrolls the Webview to the end of its contents.
 */
void gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth);

/**
 * gtk_webview_set_autoscroll:
 * @webview: The GtkWebView object
 * @scroll:  Whether to automatically scroll
 *
 * Set whether the GtkWebView stays at its end when HTML content is appended. If
 * not already at the end before appending, then scrolling will not occur.
 */
void gtk_webview_set_autoscroll(GtkWebView *webview, gboolean scroll);

/**
 * gtk_webview_get_autoscroll:
 * @webview: The GtkWebView object
 *
 * Set whether the GtkWebView stays at its end when HTML content is appended. If
 * not already at the end before appending, then scrolling will not occur.
 *
 * Returns: Whether to automatically scroll
 */
gboolean gtk_webview_get_autoscroll(GtkWebView *webview);

/**
 * gtk_webview_page_up:
 * @webview: The GtkWebView.
 *
 * Scrolls a GtkWebView up by one page.
 */
void gtk_webview_page_up(GtkWebView *webview);

/**
 * gtk_webview_page_down:
 * @webview: The GtkWebView.
 *
 * Scrolls a GtkWebView down by one page.
 */
void gtk_webview_page_down(GtkWebView *webview);

/**
 * gtk_webview_setup_entry:
 * @webview: The GtkWebView.
 * @flags:   The connection flags describing the allowed formatting.
 *
 * Setup formatting for a GtkWebView depending on the flags specified.
 */
void gtk_webview_setup_entry(GtkWebView *webview, PurpleConnectionFlags flags);

/**
 * pidgin_webview_set_spellcheck:
 * @webview: The GtkWebView.
 * @enable:  Whether to enable or disable spell-checking.
 *
 * Setup spell-checking on a GtkWebView.
 */
void pidgin_webview_set_spellcheck(GtkWebView *webview, gboolean enable);

/**
 * gtk_webview_set_whole_buffer_formatting_only:
 * @webview: The GtkWebView
 * @wbfo:    %TRUE to enable the mode, or %FALSE otherwise.
 *
 * Enables or disables whole buffer formatting only (wbfo) in a GtkWebView.
 * In this mode formatting options to the buffer take effect for the entire
 * buffer instead of specific text.
 */
void gtk_webview_set_whole_buffer_formatting_only(GtkWebView *webview,
                                                  gboolean wbfo);

/**
 * gtk_webview_set_format_functions:
 * @webview: The GtkWebView
 * @buttons: A GtkWebViewButtons bitmask indicating which functions to use
 *
 * Indicates which formatting functions to enable and disable in a GtkWebView.
 */
void gtk_webview_set_format_functions(GtkWebView *webview,
                                      GtkWebViewButtons buttons);

/**
 * gtk_webview_activate_anchor:
 * @link:   The WebKitDOMHTMLAnchorElement object
 *
 * Activates a WebKitDOMHTMLAnchorElement object. This triggers the navigation
 * signals, and marks the link as visited (when possible).
 */
void gtk_webview_activate_anchor(WebKitDOMHTMLAnchorElement *link);

/**
 * gtk_webview_class_register_protocol:
 * @name:      The name of the protocol (e.g. http://)
 * @activate:  The callback to trigger when the protocol text is clicked.
 *                  Removes any current protocol definition if %NULL. The
 *                  callback should return %TRUE if the link was activated
 *                  properly, %FALSE otherwise.
 * @context_menu:  The callback to trigger when the context menu is popped
 *                      up on the protocol text. The callback should return
 *                      %TRUE if the request for context menu was processed
 *                      successfully, %FALSE otherwise.
 *
 * Register a protocol with the GtkWebView widget. Registering a protocol would
 * allow certain text to be clickable.
 *
 * Returns:  %TRUE if the protocol was successfully registered
 *          (or unregistered, when \a activate is %NULL)
 */
gboolean gtk_webview_class_register_protocol(const char *name,
		gboolean (*activate)(GtkWebView *webview, const char *uri),
		gboolean (*context_menu)(GtkWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu));

/**
 * gtk_webview_get_format_functions:
 * @webview: The GtkWebView
 *
 * Returns which formatting functions are enabled in a GtkWebView.
 *
 * Returns: A GtkWebViewButtons bitmask indicating which functions to are enabled
 */
GtkWebViewButtons gtk_webview_get_format_functions(GtkWebView *webview);

/**
 * gtk_webview_get_current_format:
 * @webview:       The GtkWebView
 * @bold:          The boolean to set for bold or %NULL.
 * @italic:        The boolean to set for italic or %NULL.
 * @underline:     The boolean to set for underline or %NULL.
 * @strikethrough: The boolean to set for strikethrough or %NULL.
 *
 * Sets each boolean to %TRUE or %FALSE to indicate if that formatting
 * option is enabled at the current position in a GtkWebView.
 */
void gtk_webview_get_current_format(GtkWebView *webview, gboolean *bold,
                                    gboolean *italic, gboolean *underline,
                                    gboolean *strikethrough);

/**
 * gtk_webview_get_current_fontface:
 * @webview: The GtkWebView
 *
 * Returns a string containing the selected font face at the current position
 * in a GtkWebView.
 *
 * Returns: A string containing the font face or %NULL if none is set.
 */
char *gtk_webview_get_current_fontface(GtkWebView *webview);

/**
 * gtk_webview_get_current_forecolor:
 * @webview: The GtkWebView
 *
 * Returns a string containing the selected foreground color at the current
 * position in a GtkWebView.
 *
 * Returns: A string containing the foreground color or %NULL if none is set.
 */
char *gtk_webview_get_current_forecolor(GtkWebView *webview);

/**
 * gtk_webview_get_current_backcolor:
 * @webview: The GtkWebView
 *
 * Returns a string containing the selected font background color at the current
 * position in a GtkWebView.
 *
 * Returns: A string containing the background color or %NULL if none is set.
 */
char *gtk_webview_get_current_backcolor(GtkWebView *webview);

/**
 * gtk_webview_get_current_fontsize:
 * @webview: The GtkWebView
 *
 * Returns a integer containing the selected HTML font size at the current
 * position in a GtkWebView.
 *
 * Returns: The HTML font size.
 */
gint gtk_webview_get_current_fontsize(GtkWebView *webview);

/**
 * gtk_webview_get_head_html:
 * @webview: The GtkWebView
 *
 * Gets the content of the head element of a GtkWebView as HTML.
 *
 * Returns: The HTML from the head element.
 */
gchar *gtk_webview_get_head_html(GtkWebView *webview);

/**
 * gtk_webview_get_body_html:
 * @webview: The GtkWebView
 *
 * Gets the HTML content of a GtkWebView.
 *
 * Returns: The HTML that is currently displayed.
 */
gchar *gtk_webview_get_body_html(GtkWebView *webview);

/**
 * gtk_webview_get_body_text:
 * @webview: The GtkWebView
 *
 * Gets the text content of a GtkWebView.
 *
 * Returns: The HTML-free text that is currently displayed.
 */
gchar *gtk_webview_get_body_text(GtkWebView *webview);

/**
 * gtk_webview_get_selected_text:
 * @webview: The GtkWebView
 *
 * Gets the selected text of a GtkWebView.
 *
 * Returns: The HTML-free text that is currently selected, or NULL if nothing is
 *         currently selected.
 */
gchar *gtk_webview_get_selected_text(GtkWebView *webview);

/**
 * gtk_webview_get_caret:
 * @webview:       The GtkWebView
 * @container_ret: A pointer to a pointer to a WebKitDOMNode. This pointer
 *                      will be set to the container the caret is in. Set to
 *                      %NULL if a range is selected.
 * @pos_ret:       A pointer to a glong. This value will be set to the
 *                      position of the caret in the container. Set to -1 if a
 *                      range is selected.
 *
 * Gets the container of the caret, along with its position in the container
 * from a GtkWebView.
 */
void gtk_webview_get_caret(GtkWebView *webview, WebKitDOMNode **container_ret,
		glong *pos_ret);

/**
 * gtk_webview_set_caret:
 * @webview:   The GtkWebView
 * @container: The WebKitDOMNode to set the caret in
 * @pos:       The position of the caret in the container
 *
 * Sets the caret position in container, in a GtkWebView.
 */
void gtk_webview_set_caret(GtkWebView *webview, WebKitDOMNode *container,
		glong pos);

/**
 * gtk_webview_clear_formatting:
 * @webview: The GtkWebView
 *
 * Clear all the formatting on a GtkWebView.
 */
void gtk_webview_clear_formatting(GtkWebView *webview);

/**
 * gtk_webview_toggle_bold:
 * @webview: The GtkWebView
 *
 * Toggles bold at the cursor location or selection in a GtkWebView.
 */
void gtk_webview_toggle_bold(GtkWebView *webview);

/**
 * gtk_webview_toggle_italic:
 * @webview: The GtkWebView
 *
 * Toggles italic at the cursor location or selection in a GtkWebView.
 */
void gtk_webview_toggle_italic(GtkWebView *webview);

/**
 * gtk_webview_toggle_underline:
 * @webview: The GtkWebView
 *
 * Toggles underline at the cursor location or selection in a GtkWebView.
 */
void gtk_webview_toggle_underline(GtkWebView *webview);

/**
 * gtk_webview_toggle_strike:
 * @webview: The GtkWebView
 *
 * Toggles strikethrough at the cursor location or selection in a GtkWebView.
 */
void gtk_webview_toggle_strike(GtkWebView *webview);

/**
 * gtk_webview_toggle_forecolor:
 * @webview: The GtkWebView
 * @color:  The HTML-style color, or %NULL or "" to clear the color.
 *
 * Toggles a foreground color at the current location or selection in a
 * GtkWebView.
 *
 * Returns: %TRUE if a color was set, or %FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_forecolor(GtkWebView *webview, const char *color);

/**
 * gtk_webview_toggle_backcolor:
 * @webview: The GtkWebView
 * @color:  The HTML-style color, or %NULL or "" to clear the color.
 *
 * Toggles a background color at the current location or selection in a
 * GtkWebView.
 *
 * Returns: %TRUE if a color was set, or %FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_backcolor(GtkWebView *webview, const char *color);

/**
 * gtk_webview_toggle_fontface:
 * @webview: The GtkWebView
 * @face:   The font face name, or %NULL or "" to clear the font.
 *
 * Toggles a font face at the current location or selection in a GtkWebView.
 *
 * Returns: %TRUE if a font name was set, or %FALSE if it was cleared.
 */
gboolean gtk_webview_toggle_fontface(GtkWebView *webview, const char *face);

/**
 * gtk_webview_font_set_size:
 * @webview: The GtkWebView
 * @size:   The HTML font size to use.
 *
 * Sets the font size at the current location or selection in a GtkWebView.
 */
void gtk_webview_font_set_size(GtkWebView *webview, gint size);

/**
 * gtk_webview_font_shrink:
 * @webview: The GtkWebView
 *
 * Decreases the font size by 1 at the current location or selection in a
 * GtkWebView.
 */
void gtk_webview_font_shrink(GtkWebView *webview);

/**
 * gtk_webview_font_grow:
 * @webview: The GtkWebView
 *
 * Increases the font size by 1 at the current location or selection in a
 * GtkWebView.
 */
void gtk_webview_font_grow(GtkWebView *webview);

/**
 * gtk_webview_insert_hr:
 * @webview: The GtkWebView
 *
 * Inserts a horizontal rule at the current location or selection in a
 * GtkWebView.
 */
void gtk_webview_insert_hr(GtkWebView *webview);

/**
 * gtk_webview_insert_link:
 * @webview: The GtkWebView
 * @url:     The URL of the link
 * @desc:    The text description of the link. If not supplied, the URL is
 *                used instead.
 *
 * Inserts a link at the current location or selection in a GtkWebView.
 */
void gtk_webview_insert_link(GtkWebView *webview, const char *url, const char *desc);

/**
 * gtk_webview_insert_image:
 * @webview: The GtkWebView
 * @id:      The PurpleStoredImage id
 *
 * Inserts an image at the current location or selection in a GtkWebView.
 */
void gtk_webview_insert_image(GtkWebView *webview, int id);

/**
 * gtk_webview_get_protocol_name:
 * @webview: The GtkWebView
 *
 * Gets the protocol name associated with this GtkWebView.
 */
const char *gtk_webview_get_protocol_name(GtkWebView *webview);

/**
 * gtk_webview_set_protocol_name:
 * @webview:       The GtkWebView
 * @protocol_name: The protocol name to associate with the GtkWebView
 *
 * Associates a protocol name with a GtkWebView.
 */
void gtk_webview_set_protocol_name(GtkWebView *webview, const char *protocol_name);

/**
 * gtk_webview_smiley_create:
 * @file:      The image file for the smiley
 * @shortcut:  The key shortcut for the smiley
 * @hide:      %TRUE if the smiley should be hidden in the smiley dialog,
 *                  %FALSE otherwise
 * @flags:     The smiley flags
 *
 * Create a new GtkWebViewSmiley.
 *
 * Returns: The newly created smiley
 */
GtkWebViewSmiley *gtk_webview_smiley_create(const char *file,
                                            const char *shortcut,
                                            gboolean hide,
                                            GtkWebViewSmileyFlags flags);

/**
 * gtk_webview_smiley_reload:
 * @smiley:    The smiley to reload
 *
 * Reload the image data for the smiley.
 */
void gtk_webview_smiley_reload(GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_destroy:
 * @smiley:    The smiley to destroy
 *
 * Destroy a GtkWebViewSmiley.
 */
void gtk_webview_smiley_destroy(GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_get_smile:
 * @smiley:    The smiley
 *
 * Returns the text associated with a smiley.
 *
 * Returns: The text
 */
const char *gtk_webview_smiley_get_smile(const GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_get_file:
 * @smiley:    The smiley
 *
 * Returns the file associated with a smiley.
 *
 * Returns: The file
 */
const char *gtk_webview_smiley_get_file(const GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_get_hidden:
 * @smiley:    The smiley
 *
 * Returns the invisibility of a smiley.
 *
 * Returns: The hidden status
 */
gboolean gtk_webview_smiley_get_hidden(const GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_get_flags:
 * @smiley:    The smiley
 *
 * Returns the flags associated with a smiley.
 *
 * Returns: The flags
 */
GtkWebViewSmileyFlags gtk_webview_smiley_get_flags(const GtkWebViewSmiley *smiley);

/**
 * gtk_webview_smiley_find:
 * @webview: The GtkWebView
 * @sml:     The name of the smiley category
 * @text:    The text associated with the smiley
 *
 * Returns the smiley object associated with the text.
 */
GtkWebViewSmiley *gtk_webview_smiley_find(GtkWebView *webview, const char *sml,
                                          const char *text);

/**
 * gtk_webview_associate_smiley:
 * @webview: The GtkWebView
 * @sml:     The name of the smiley category
 * @smiley:  The GtkWebViewSmiley to associate
 *
 * Associates a smiley with a GtkWebView.
 */
void gtk_webview_associate_smiley(GtkWebView *webview, const char *sml,
                                  GtkWebViewSmiley *smiley);

/**
 * gtk_webview_remove_smileys:
 * @webview: The GtkWebView.
 *
 * Removes all smileys associated with a GtkWebView.
 */
void gtk_webview_remove_smileys(GtkWebView *webview);

/**
 * gtk_webview_insert_smiley:
 * @webview: The GtkWebView
 * @sml:     The category of the smiley
 * @smiley:  The text of the smiley to insert
 *
 * Inserts a smiley at the current location or selection in a GtkWebView.
 */
void gtk_webview_insert_smiley(GtkWebView *webview, const char *sml,
                               const char *smiley);

/**
 * gtk_webview_show_toolbar:
 * @webview: The GtkWebView.
 *
 * Makes the toolbar associated with a GtkWebView visible.
 */
void gtk_webview_show_toolbar(GtkWebView *webview);

/**
 * gtk_webview_hide_toolbar:
 * @webview: The GtkWebView.
 *
 * Makes the toolbar associated with a GtkWebView invisible.
 */
void gtk_webview_hide_toolbar(GtkWebView *webview);

/**
 * gtk_webview_activate_toolbar:
 * @webview: The GtkWebView
 * @action:  The GtkWebViewAction
 *
 * Activate an action on the toolbar associated with a GtkWebView.
 */
void gtk_webview_activate_toolbar(GtkWebView *webview, GtkWebViewAction action);

/* Do not use. */
void
gtk_webview_set_toolbar(GtkWebView *webview, GtkWidget *toolbar);

G_END_DECLS

#endif /* _PIDGIN_WEBVIEW_H_ */

