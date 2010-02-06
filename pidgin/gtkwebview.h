/**
 * @file gtkwebview.h Wrapper over the Gtk WebKitWebView component
 * @ingroup pidgin
 */

/* Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PIDGIN_WEBVIEW_H_
#define _PIDGIN_WEBVIEW_H_

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "notify.h"

#define GTK_TYPE_WEBVIEW            (gtk_webview_get_type())
#define GTK_WEBVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_WEBVIEW, GtkWebView))
#define GTK_WEBVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_WEBVIEW, GtkWebViewClass))
#define GTK_IS_WEBVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_WEBVIEW))
#define GTK_IS_WEBVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_WEBVIEW))


struct GtkWebViewPriv;

struct _GtkWebView
{
	WebKitWebView webkit_web_view;

	/*< private >*/
	struct GtkWebViewPriv* priv;
};

typedef struct _GtkWebView GtkWebView;

struct _GtkWebViewClass
{
	WebKitWebViewClass parent;
};

typedef struct _GtkWebViewClass GtkWebViewClass;


/**
 * Returns the GType for a GtkWebView widget
 *
 * @return the GType for GtkWebView widget
 */
GType gtk_webview_get_type (void);

/**
 * Create a new GtkWebView object
 *
 * @return a GtkWidget corresponding to the GtkWebView object
 */
GtkWidget* gtk_webview_new (void);

/**
 * Set the vertical adjustment for the GtkWebView.
 *
 * @param webview  The GtkWebView.
 * @param vadj     The GtkAdjustment that control the webview.
 */
void gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj);

/**
 * A very basic routine to append html, which can be considered
 * equivalent to a "document.write" using JavaScript.
 *
 * @param webview The GtkWebView object
 * @param markup  The html markup to append
 */
void gtk_webview_append_html (GtkWebView *webview, const char* markup);

/**
 * Rather than use webkit_webview_load_string, this routine
 * parses and displays the <img id=?> tags that make use of the
 * Pidgin imgstore.
 *
 * @param webview The GtkWebView object
 * @param html    The HTML content to load
 */
void gtk_webview_load_html_string_with_imgstore (GtkWebView* webview, const char* html);

/**
 * (To be changed, right now it just tests whether an append has been
 * called since the last clear or since the Widget was created. So it
 * does not test for load_string's called in between.
 *
 * @param webview The GtkWebView object
 * 
 * @return gboolean indicating whether the webview is empty.
 */
gboolean gtk_webview_is_empty (GtkWebView *webview);

/**
 * Execute the JavaScript only after the webkit_webview_load_string
 * loads completely. We also guarantee that the scripts are executed
 * in the order they are called here.This is useful to avoid race
 * conditions when calls JS functions immediately after opening the
 * page.
 *
 * @param webview the GtkWebView object
 * @param script   the script to execute
 */
void gtk_webview_safe_execute_script (GtkWebView *webview, const char* script);

/**
 * A convenience routine to quote a string for use as a JavaScript 
 * string. For instance, "hello 'world'" becomes "'hello \\'world\\''"
 *
 * @param str The string to escape and quote
 *
 * @return the quoted string.
 */
char* gtk_webview_quote_js_string (const char* str);

/**
 * Scrolls the Webview to the end of its contents.
 *
 * @param webview The GtkWebView.
 * @param smoth   A boolean indicating if smooth scrolling should be used.
 */
void gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth);

#endif /* _PIDGIN_WEBVIEW_H_ */
