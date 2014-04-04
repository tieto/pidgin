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
/**
 * SECTION:gtkwebview
 * @section_id: pidgin-gtkwebview
 * @short_description: <filename>gtkwebview.h</filename>
 * @title: WebKitWebView Wrapper
 *
 * Wrapper over the Gtk WebKitWebView component.
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define PIDGIN_TYPE_WEBVIEW            (pidgin_webview_get_type())
#define PIDGIN_WEBVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_WEBVIEW, PidginWebView))
#define PIDGIN_WEBVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_WEBVIEW, PidginWebViewClass))
#define PIDGIN_IS_WEBVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_WEBVIEW))
#define PIDGIN_IS_WEBVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_WEBVIEW))
#define PIDGIN_WEBVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_WEBVIEW, PidginWebViewClass))

typedef enum {
	PIDGIN_WEBVIEW_BOLD          = 1 << 0,
	PIDGIN_WEBVIEW_ITALIC        = 1 << 1,
	PIDGIN_WEBVIEW_UNDERLINE     = 1 << 2,
	PIDGIN_WEBVIEW_GROW          = 1 << 3,
	PIDGIN_WEBVIEW_SHRINK        = 1 << 4,
	PIDGIN_WEBVIEW_FACE          = 1 << 5,
	PIDGIN_WEBVIEW_FORECOLOR     = 1 << 6,
	PIDGIN_WEBVIEW_BACKCOLOR     = 1 << 7,
	PIDGIN_WEBVIEW_LINK          = 1 << 8,
	PIDGIN_WEBVIEW_IMAGE         = 1 << 9,
	PIDGIN_WEBVIEW_SMILEY        = 1 << 10,
	PIDGIN_WEBVIEW_LINKDESC      = 1 << 11,
	PIDGIN_WEBVIEW_STRIKE        = 1 << 12,
	/** Show custom smileys when appropriate. */
	PIDGIN_WEBVIEW_CUSTOM_SMILEY = 1 << 13,
	PIDGIN_WEBVIEW_ALL           = -1
} PidginWebViewButtons;

typedef enum {
	PIDGIN_WEBVIEW_ACTION_BOLD,
	PIDGIN_WEBVIEW_ACTION_ITALIC,
	PIDGIN_WEBVIEW_ACTION_UNDERLINE,
	PIDGIN_WEBVIEW_ACTION_STRIKE,
	PIDGIN_WEBVIEW_ACTION_LARGER,
#if 0
	PIDGIN_WEBVIEW_ACTION_NORMAL,
#endif
	PIDGIN_WEBVIEW_ACTION_SMALLER,
	PIDGIN_WEBVIEW_ACTION_FONTFACE,
	PIDGIN_WEBVIEW_ACTION_FGCOLOR,
	PIDGIN_WEBVIEW_ACTION_BGCOLOR,
	PIDGIN_WEBVIEW_ACTION_CLEAR,
	PIDGIN_WEBVIEW_ACTION_IMAGE,
	PIDGIN_WEBVIEW_ACTION_LINK,
	PIDGIN_WEBVIEW_ACTION_HR,
	PIDGIN_WEBVIEW_ACTION_SMILEY,
	PIDGIN_WEBVIEW_ACTION_ATTENTION
} PidginWebViewAction;

typedef struct _PidginWebView PidginWebView;
typedef struct _PidginWebViewClass PidginWebViewClass;

struct _PidginWebView
{
	WebKitWebView parent;
};

struct _PidginWebViewClass
{
	WebKitWebViewClass parent;

	GList *protocols;

	void (*buttons_update)(PidginWebView *, PidginWebViewButtons);
	void (*toggle_format)(PidginWebView *, PidginWebViewButtons);
	void (*clear_format)(PidginWebView *);
	void (*update_format)(PidginWebView *);
	void (*changed)(PidginWebView *);
	void (*html_appended)(PidginWebView *, WebKitDOMRange *);
};

G_BEGIN_DECLS

/**
 * pidgin_webview_get_type:
 *
 * Returns: The #GType for #PidginWebView widget
 */
GType pidgin_webview_get_type(void);

/**
 * pidgin_webview_new:
 * @editable: Whether this PidginWebView will be user-editable
 *
 * Create a new PidginWebView object
 *
 * Returns: A GtkWidget corresponding to the PidginWebView object
 */
GtkWidget *pidgin_webview_new(gboolean editable);

/**
 * pidgin_webview_append_html:
 * @webview: The PidginWebView object
 * @markup:  The html markup to append
 *
 * A very basic routine to append html, which can be considered
 * equivalent to a "document.write" using JavaScript.
 */
void pidgin_webview_append_html(PidginWebView *webview, const char *markup);

/**
 * pidgin_webview_load_html_string:
 * @webview: The PidginWebView object
 * @html:    The HTML content to load
 *
 * Requests loading of the given content.
 */
void pidgin_webview_load_html_string(PidginWebView *webview, const char *html);

/**
 * pidgin_webview_load_html_string_with_selection:
 * @webview: The PidginWebView object
 * @html:    The HTML content to load
 *
 * Requests loading of the given content and sets the selection. You must
 * include an anchor tag with id='caret' in the HTML string, which will be
 * used to set the selection. This tag is then removed so that querying the
 * WebView's HTML contents will no longer return it.
 */
void pidgin_webview_load_html_string_with_selection(PidginWebView *webview, const char *html);

/**
 * pidgin_webview_safe_execute_script:
 * @webview: The PidginWebView object
 * @script:  The script to execute
 *
 * Execute the JavaScript only after the webkit_webview_load_string
 * loads completely. We also guarantee that the scripts are executed
 * in the order they are called here. This is useful to avoid race
 * conditions when calling JS functions immediately after opening the
 * page.
 */
void pidgin_webview_safe_execute_script(PidginWebView *webview, const char *script);

/**
 * pidgin_webview_quote_js_string:
 * @str: The string to escape and quote
 *
 * A convenience routine to quote a string for use as a JavaScript
 * string. For instance, "hello 'world'" becomes "'hello \\'world\\''"
 *
 * Returns: The quoted string
 */
char *pidgin_webview_quote_js_string(const char *str);

/**
 * pidgin_webview_set_vadjustment:
 * @webview:  The PidginWebView object
 * @vadj:     The GtkAdjustment that control the webview
 *
 * Set the vertical adjustment for the PidginWebView.
 */
void pidgin_webview_set_vadjustment(PidginWebView *webview, GtkAdjustment *vadj);

/**
 * pidgin_webview_scroll_to_end:
 * @webview: The PidginWebView object
 * @smooth:  A boolean indicating if smooth scrolling should be used
 *
 * Scrolls the Webview to the end of its contents.
 */
void pidgin_webview_scroll_to_end(PidginWebView *webview, gboolean smooth);

/**
 * pidgin_webview_set_autoscroll:
 * @webview: The PidginWebView object
 * @scroll:  Whether to automatically scroll
 *
 * Set whether the PidginWebView stays at its end when HTML content is appended. If
 * not already at the end before appending, then scrolling will not occur.
 */
void pidgin_webview_set_autoscroll(PidginWebView *webview, gboolean scroll);

/**
 * pidgin_webview_get_autoscroll:
 * @webview: The PidginWebView object
 *
 * Set whether the PidginWebView stays at its end when HTML content is appended. If
 * not already at the end before appending, then scrolling will not occur.
 *
 * Returns: Whether to automatically scroll
 */
gboolean pidgin_webview_get_autoscroll(PidginWebView *webview);

/**
 * pidgin_webview_page_up:
 * @webview: The PidginWebView.
 *
 * Scrolls a PidginWebView up by one page.
 */
void pidgin_webview_page_up(PidginWebView *webview);

/**
 * pidgin_webview_page_down:
 * @webview: The PidginWebView.
 *
 * Scrolls a PidginWebView down by one page.
 */
void pidgin_webview_page_down(PidginWebView *webview);

/**
 * pidgin_webview_setup_entry:
 * @webview: The PidginWebView.
 * @flags:   The connection flags describing the allowed formatting.
 *
 * Setup formatting for a PidginWebView depending on the flags specified.
 */
void pidgin_webview_setup_entry(PidginWebView *webview, PurpleConnectionFlags flags);

/**
 * pidgin_webview_set_spellcheck:
 * @webview: The PidginWebView.
 * @enable:  Whether to enable or disable spell-checking.
 *
 * Setup spell-checking on a PidginWebView.
 */
void pidgin_webview_set_spellcheck(PidginWebView *webview, gboolean enable);

/**
 * pidgin_webview_set_whole_buffer_formatting_only:
 * @webview: The PidginWebView
 * @wbfo:    %TRUE to enable the mode, or %FALSE otherwise.
 *
 * Enables or disables whole buffer formatting only (wbfo) in a PidginWebView.
 * In this mode formatting options to the buffer take effect for the entire
 * buffer instead of specific text.
 */
void pidgin_webview_set_whole_buffer_formatting_only(PidginWebView *webview,
                                                  gboolean wbfo);

/**
 * pidgin_webview_set_format_functions:
 * @webview: The PidginWebView
 * @buttons: A PidginWebViewButtons bitmask indicating which functions to use
 *
 * Indicates which formatting functions to enable and disable in a PidginWebView.
 */
void pidgin_webview_set_format_functions(PidginWebView *webview,
                                      PidginWebViewButtons buttons);

/**
 * pidgin_webview_activate_anchor:
 * @link:   The WebKitDOMHTMLAnchorElement object
 *
 * Activates a WebKitDOMHTMLAnchorElement object. This triggers the navigation
 * signals, and marks the link as visited (when possible).
 */
void pidgin_webview_activate_anchor(WebKitDOMHTMLAnchorElement *link);

/**
 * pidgin_webview_class_register_protocol:
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
 * Register a protocol with the PidginWebView widget. Registering a protocol would
 * allow certain text to be clickable.
 *
 * Returns:  %TRUE if the protocol was successfully registered
 *          (or unregistered, when \a activate is %NULL)
 */
gboolean pidgin_webview_class_register_protocol(const char *name,
		gboolean (*activate)(PidginWebView *webview, const char *uri),
		gboolean (*context_menu)(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu));

/**
 * pidgin_webview_get_format_functions:
 * @webview: The PidginWebView
 *
 * Returns which formatting functions are enabled in a PidginWebView.
 *
 * Returns: A PidginWebViewButtons bitmask indicating which functions to are enabled
 */
PidginWebViewButtons pidgin_webview_get_format_functions(PidginWebView *webview);

/**
 * pidgin_webview_get_current_format:
 * @webview:       The PidginWebView
 * @bold:          The boolean to set for bold or %NULL.
 * @italic:        The boolean to set for italic or %NULL.
 * @underline:     The boolean to set for underline or %NULL.
 * @strikethrough: The boolean to set for strikethrough or %NULL.
 *
 * Sets each boolean to %TRUE or %FALSE to indicate if that formatting
 * option is enabled at the current position in a PidginWebView.
 */
void pidgin_webview_get_current_format(PidginWebView *webview, gboolean *bold,
                                    gboolean *italic, gboolean *underline,
                                    gboolean *strikethrough);

/**
 * pidgin_webview_get_current_fontface:
 * @webview: The PidginWebView
 *
 * Returns a string containing the selected font face at the current position
 * in a PidginWebView.
 *
 * Returns: A string containing the font face or %NULL if none is set.
 */
char *pidgin_webview_get_current_fontface(PidginWebView *webview);

/**
 * pidgin_webview_get_current_forecolor:
 * @webview: The PidginWebView
 *
 * Returns a string containing the selected foreground color at the current
 * position in a PidginWebView.
 *
 * Returns: A string containing the foreground color or %NULL if none is set.
 */
char *pidgin_webview_get_current_forecolor(PidginWebView *webview);

/**
 * pidgin_webview_get_current_backcolor:
 * @webview: The PidginWebView
 *
 * Returns a string containing the selected font background color at the current
 * position in a PidginWebView.
 *
 * Returns: A string containing the background color or %NULL if none is set.
 */
char *pidgin_webview_get_current_backcolor(PidginWebView *webview);

/**
 * pidgin_webview_get_current_fontsize:
 * @webview: The PidginWebView
 *
 * Returns a integer containing the selected HTML font size at the current
 * position in a PidginWebView.
 *
 * Returns: The HTML font size.
 */
gint pidgin_webview_get_current_fontsize(PidginWebView *webview);

/**
 * pidgin_webview_get_head_html:
 * @webview: The PidginWebView
 *
 * Gets the content of the head element of a PidginWebView as HTML.
 *
 * Returns: The HTML from the head element.
 */
gchar *pidgin_webview_get_head_html(PidginWebView *webview);

/**
 * pidgin_webview_get_body_html:
 * @webview: The PidginWebView
 *
 * Gets the HTML content of a PidginWebView.
 *
 * Returns: The HTML that is currently displayed.
 */
gchar *pidgin_webview_get_body_html(PidginWebView *webview);

/**
 * pidgin_webview_get_body_text:
 * @webview: The PidginWebView
 *
 * Gets the text content of a PidginWebView.
 *
 * Returns: The HTML-free text that is currently displayed.
 */
gchar *pidgin_webview_get_body_text(PidginWebView *webview);

/**
 * pidgin_webview_get_selected_text:
 * @webview: The PidginWebView
 *
 * Gets the selected text of a PidginWebView.
 *
 * Returns: The HTML-free text that is currently selected, or NULL if nothing is
 *         currently selected.
 */
gchar *pidgin_webview_get_selected_text(PidginWebView *webview);

/**
 * pidgin_webview_get_caret:
 * @webview:       The PidginWebView
 * @container_ret: A pointer to a pointer to a WebKitDOMNode. This pointer
 *                      will be set to the container the caret is in. Set to
 *                      %NULL if a range is selected.
 * @pos_ret:       A pointer to a glong. This value will be set to the
 *                      position of the caret in the container. Set to -1 if a
 *                      range is selected.
 *
 * Gets the container of the caret, along with its position in the container
 * from a PidginWebView.
 */
void pidgin_webview_get_caret(PidginWebView *webview, WebKitDOMNode **container_ret,
		glong *pos_ret);

/**
 * pidgin_webview_set_caret:
 * @webview:   The PidginWebView
 * @container: The WebKitDOMNode to set the caret in
 * @pos:       The position of the caret in the container
 *
 * Sets the caret position in container, in a PidginWebView.
 */
void pidgin_webview_set_caret(PidginWebView *webview, WebKitDOMNode *container,
		glong pos);

/**
 * pidgin_webview_clear_formatting:
 * @webview: The PidginWebView
 *
 * Clear all the formatting on a PidginWebView.
 */
void pidgin_webview_clear_formatting(PidginWebView *webview);

/**
 * pidgin_webview_toggle_bold:
 * @webview: The PidginWebView
 *
 * Toggles bold at the cursor location or selection in a PidginWebView.
 */
void pidgin_webview_toggle_bold(PidginWebView *webview);

/**
 * pidgin_webview_toggle_italic:
 * @webview: The PidginWebView
 *
 * Toggles italic at the cursor location or selection in a PidginWebView.
 */
void pidgin_webview_toggle_italic(PidginWebView *webview);

/**
 * pidgin_webview_toggle_underline:
 * @webview: The PidginWebView
 *
 * Toggles underline at the cursor location or selection in a PidginWebView.
 */
void pidgin_webview_toggle_underline(PidginWebView *webview);

/**
 * pidgin_webview_toggle_strike:
 * @webview: The PidginWebView
 *
 * Toggles strikethrough at the cursor location or selection in a PidginWebView.
 */
void pidgin_webview_toggle_strike(PidginWebView *webview);

/**
 * pidgin_webview_toggle_forecolor:
 * @webview: The PidginWebView
 * @color:  The HTML-style color, or %NULL or "" to clear the color.
 *
 * Toggles a foreground color at the current location or selection in a
 * PidginWebView.
 *
 * Returns: %TRUE if a color was set, or %FALSE if it was cleared.
 */
gboolean pidgin_webview_toggle_forecolor(PidginWebView *webview, const char *color);

/**
 * pidgin_webview_toggle_backcolor:
 * @webview: The PidginWebView
 * @color:  The HTML-style color, or %NULL or "" to clear the color.
 *
 * Toggles a background color at the current location or selection in a
 * PidginWebView.
 *
 * Returns: %TRUE if a color was set, or %FALSE if it was cleared.
 */
gboolean pidgin_webview_toggle_backcolor(PidginWebView *webview, const char *color);

/**
 * pidgin_webview_toggle_fontface:
 * @webview: The PidginWebView
 * @face:   The font face name, or %NULL or "" to clear the font.
 *
 * Toggles a font face at the current location or selection in a PidginWebView.
 *
 * Returns: %TRUE if a font name was set, or %FALSE if it was cleared.
 */
gboolean pidgin_webview_toggle_fontface(PidginWebView *webview, const char *face);

/**
 * pidgin_webview_font_set_size:
 * @webview: The PidginWebView
 * @size:   The HTML font size to use.
 *
 * Sets the font size at the current location or selection in a PidginWebView.
 */
void pidgin_webview_font_set_size(PidginWebView *webview, gint size);

/**
 * pidgin_webview_font_shrink:
 * @webview: The PidginWebView
 *
 * Decreases the font size by 1 at the current location or selection in a
 * PidginWebView.
 */
void pidgin_webview_font_shrink(PidginWebView *webview);

/**
 * pidgin_webview_font_grow:
 * @webview: The PidginWebView
 *
 * Increases the font size by 1 at the current location or selection in a
 * PidginWebView.
 */
void pidgin_webview_font_grow(PidginWebView *webview);

/**
 * pidgin_webview_insert_hr:
 * @webview: The PidginWebView
 *
 * Inserts a horizontal rule at the current location or selection in a
 * PidginWebView.
 */
void pidgin_webview_insert_hr(PidginWebView *webview);

/**
 * pidgin_webview_insert_link:
 * @webview: The PidginWebView
 * @url:     The URL of the link
 * @desc:    The text description of the link. If not supplied, the URL is
 *                used instead.
 *
 * Inserts a link at the current location or selection in a PidginWebView.
 */
void pidgin_webview_insert_link(PidginWebView *webview, const char *url, const char *desc);

/**
 * pidgin_webview_insert_image:
 * @webview: The PidginWebView
 * @id:      The PurpleStoredImage id
 *
 * Inserts an image at the current location or selection in a PidginWebView.
 */
void pidgin_webview_insert_image(PidginWebView *webview, int id);

/**
 * pidgin_webview_get_protocol_name:
 * @webview: The PidginWebView
 *
 * Gets the protocol name associated with this PidginWebView.
 */
const char *pidgin_webview_get_protocol_name(PidginWebView *webview);

/**
 * pidgin_webview_show_toolbar:
 * @webview: The PidginWebView.
 *
 * Makes the toolbar associated with a PidginWebView visible.
 */
void pidgin_webview_show_toolbar(PidginWebView *webview);

/**
 * pidgin_webview_hide_toolbar:
 * @webview: The PidginWebView.
 *
 * Makes the toolbar associated with a PidginWebView invisible.
 */
void pidgin_webview_hide_toolbar(PidginWebView *webview);

/**
 * pidgin_webview_activate_toolbar:
 * @webview: The PidginWebView
 * @action:  The PidginWebViewAction
 *
 * Activate an action on the toolbar associated with a PidginWebView.
 */
void pidgin_webview_activate_toolbar(PidginWebView *webview, PidginWebViewAction action);

void
pidgin_webview_switch_active_conversation(PidginWebView *webview,
	PurpleConversation *conv);

/* Do not use. */
void
pidgin_webview_set_toolbar(PidginWebView *webview, GtkWidget *toolbar);

G_END_DECLS

#endif /* _PIDGIN_WEBVIEW_H_ */

