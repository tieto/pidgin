/*
 * @file gtkwebview.c GTK+ WebKitWebView wrapper class.
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

#include "internal.h"
#include "debug.h"
#include "pidgin.h"

#include <gdk/gdkkeysyms.h>
#include "gtkwebview.h"

#include "gtk3compat.h"

#define MAX_FONT_SIZE 7
#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33 /* milliseconds */

#define GTK_WEBVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_WEBVIEW, GtkWebViewPriv))

enum {
	LOAD_HTML,
	LOAD_JS
};

enum {
	BUTTONS_UPDATE,
	TOGGLE_FORMAT,
	CLEAR_FORMAT,
	UPDATE_FORMAT,
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct _GtkWebViewPriv {
	/* Processing queues */
	gboolean is_loading;
	GQueue *load_queue;
	guint loader;

	/* Scroll adjustments */
	GtkAdjustment *vadj;
	guint scroll_src;
	GTimer *scroll_time;

	/* Format options */
	GtkWebViewButtons format_functions;
	struct {
		gboolean wbfo:1;	/* Whole buffer formatting only. */
		gboolean block_changed:1;
	} edit;

} GtkWebViewPriv;

/******************************************************************************
 * Globals
 *****************************************************************************/

static WebKitWebViewClass *parent_class = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/

static void
webview_resource_loading(WebKitWebView *webview,
                         WebKitWebFrame *frame,
                         WebKitWebResource *resource,
                         WebKitNetworkRequest *request,
                         WebKitNetworkResponse *response,
                         gpointer user_data)
{
	const gchar *uri;

	uri = webkit_network_request_get_uri(request);
	if (purple_str_has_prefix(uri, PURPLE_STORED_IMAGE_PROTOCOL)) {
		int id;
		PurpleStoredImage *img;
		const char *filename;

		uri += sizeof(PURPLE_STORED_IMAGE_PROTOCOL) - 1;
		id = strtoul(uri, NULL, 10);

		img = purple_imgstore_find_by_id(id);
		if (!img)
			return;

		filename = purple_imgstore_get_filename(img);
		if (filename && g_path_is_absolute(filename)) {
			char *tmp = g_strdup_printf("file://%s", filename);
			webkit_network_request_set_uri(request, tmp);
			g_free(tmp);
		} else {
			char *b64 = purple_base64_encode(purple_imgstore_get_data(img),
			                                 purple_imgstore_get_size(img));
			const char *type = purple_imgstore_get_extension(img);
			char *tmp = g_strdup_printf("data:image/%s;base64,%s", type, b64);
			webkit_network_request_set_uri(request, tmp);
			g_free(b64);
			g_free(tmp);
		}
	}
}

static gboolean
process_load_queue(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	int type;
	char *str;
	WebKitDOMDocument *doc;
	WebKitDOMHTMLElement *body;

	if (priv->is_loading) {
		priv->loader = 0;
		return FALSE;
	}
	if (!priv->load_queue || g_queue_is_empty(priv->load_queue)) {
		priv->loader = 0;
		return FALSE;
	}

	type = GPOINTER_TO_INT(g_queue_pop_head(priv->load_queue));
	str = g_queue_pop_head(priv->load_queue);

	switch (type) {
		case LOAD_HTML:
			doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
			body = webkit_dom_document_get_body(doc);
			webkit_dom_html_element_insert_adjacent_html(body, "beforeend",
			                                             str, NULL);
			break;

		case LOAD_JS:
			webkit_web_view_execute_script(WEBKIT_WEB_VIEW(webview), str);
			break;

		default:
			purple_debug_error("webview",
			                   "Got unknown loading queue type: %d\n", type);
			break;
	}

	g_free(str);

	return TRUE;
}

static void
webview_load_started(WebKitWebView *webview, WebKitWebFrame *frame,
                     gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	/* is there a better way to test for is_loading? */
	priv->is_loading = TRUE;
}

static void
webview_load_finished(WebKitWebView *webview, WebKitWebFrame *frame,
                      gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	priv->is_loading = FALSE;
	if (priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

static gboolean
webview_navigation_decision(WebKitWebView *webview,
                            WebKitWebFrame *frame,
                            WebKitNetworkRequest *request,
                            WebKitWebNavigationAction *navigation_action,
                            WebKitWebPolicyDecision *policy_decision,
                            gpointer userdata)
{
	const gchar *uri;
	WebKitWebNavigationReason reason;

	uri = webkit_network_request_get_uri(request);
	reason = webkit_web_navigation_action_get_reason(navigation_action);

	if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED) {
		/* the gtk imhtml way was to create an idle cb, not sure
		 * why, so right now just using purple_notify_uri directly */
		purple_notify_uri(NULL, uri);
		webkit_web_policy_decision_ignore(policy_decision);
	} else if (reason == WEBKIT_WEB_NAVIGATION_REASON_OTHER)
		webkit_web_policy_decision_use(policy_decision);
	else
		webkit_web_policy_decision_ignore(policy_decision);

	return TRUE;
}

/*
 * Smoothly scroll a WebView.
 *
 * @return TRUE if the window needs to be scrolled further, FALSE if we're at the bottom.
 */
static gboolean
smooth_scroll_cb(gpointer data)
{
	GtkWebViewPriv *priv = data;
	GtkAdjustment *adj;
	gdouble max_val;
	gdouble scroll_val;

	g_return_val_if_fail(priv->scroll_time != NULL, FALSE);

	adj = priv->vadj;
	max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
	scroll_val = gtk_adjustment_get_value(adj) +
	             ((max_val - gtk_adjustment_get_value(adj)) / 3);

	if (g_timer_elapsed(priv->scroll_time, NULL) > MAX_SCROLL_TIME
	 || scroll_val >= max_val) {
		/* time's up. jump to the end and kill the timer */
		gtk_adjustment_set_value(adj, max_val);
		g_timer_destroy(priv->scroll_time);
		priv->scroll_time = NULL;
		priv->scroll_src = 0;
		return FALSE;
	}

	/* scroll by 1/3rd the remaining distance */
	gtk_adjustment_set_value(adj, scroll_val);
	return TRUE;
}

static gboolean
scroll_idle_cb(gpointer data)
{
	GtkWebViewPriv *priv = data;
	GtkAdjustment *adj = priv->vadj;
	gdouble max_val;

	if (adj) {
		max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
		gtk_adjustment_set_value(adj, max_val);
	}

	priv->scroll_src = 0;
	return FALSE;
}

static void
emit_format_signal(GtkWebView *webview, GtkWebViewButtons buttons)
{
	g_object_ref(webview);
	g_signal_emit(webview, signals[TOGGLE_FORMAT], 0, buttons);
	g_object_unref(webview);
}

static void
do_formatting(GtkWebView *webview, const char *name, const char *value)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	WebKitDOMDocument *dom;
	WebKitDOMDOMWindow *win;
	WebKitDOMDOMSelection *sel = NULL;
	WebKitDOMRange *range = NULL;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	if (priv->edit.wbfo) {
		win = webkit_dom_document_get_default_view(dom);
		sel = webkit_dom_dom_window_get_selection(win);
		if (webkit_dom_dom_selection_get_range_count(sel) > 0)
			range = webkit_dom_dom_selection_get_range_at(sel, 0, NULL);
		webkit_web_view_select_all(WEBKIT_WEB_VIEW(webview));
	}

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, name, FALSE, value);
	priv->edit.block_changed = FALSE;

	if (priv->edit.wbfo) {
		if (range) {
			webkit_dom_dom_selection_remove_all_ranges(sel);
			webkit_dom_dom_selection_add_range(sel, range);
		} else {
			webkit_dom_dom_selection_collapse_to_end(sel, NULL);
		}
	}
}

static void
webview_font_shrink(GtkWebView *webview)
{
	gint fontsize;
	char *tmp;

	fontsize = gtk_webview_get_current_fontsize(webview);
	fontsize = MAX(fontsize - 1, 1);

	tmp = g_strdup_printf("%d", fontsize);
	do_formatting(webview, "fontSize", tmp);
	g_free(tmp);
}

static void
webview_font_grow(GtkWebView *webview)
{
	gint fontsize;
	char *tmp;

	fontsize = gtk_webview_get_current_fontsize(webview);
	fontsize = MIN(fontsize + 1, MAX_FONT_SIZE);

	tmp = g_strdup_printf("%d", fontsize);
	do_formatting(webview, "fontSize", tmp);
	g_free(tmp);
}

static void
webview_clear_formatting(GtkWebView *webview)
{
	if (!webkit_web_view_get_editable(WEBKIT_WEB_VIEW(webview)))
		return;

	do_formatting(webview, "removeFormat", "");
	do_formatting(webview, "unlink", "");
	do_formatting(webview, "backColor", "inherit");
}

static void
webview_toggle_format(GtkWebView *webview, GtkWebViewButtons buttons)
{
	/* since this function is the handler for the formatting keystrokes,
	   we need to check here that the formatting attempted is permitted */
	buttons &= gtk_webview_get_format_functions(webview);

	switch (buttons) {
	case GTK_WEBVIEW_BOLD:
		do_formatting(webview, "bold", "");
		break;
	case GTK_WEBVIEW_ITALIC:
		do_formatting(webview, "italic", "");
		break;
	case GTK_WEBVIEW_UNDERLINE:
		do_formatting(webview, "underline", "");
		break;
	case GTK_WEBVIEW_STRIKE:
		do_formatting(webview, "strikethrough", "");
		break;
	case GTK_WEBVIEW_SHRINK:
		webview_font_shrink(webview);
		break;
	case GTK_WEBVIEW_GROW:
		webview_font_grow(webview);
		break;
	default:
		break;
	}
}

static void
editable_input_cb(GtkWebView *webview, gpointer data)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	if (!priv->edit.block_changed && gtk_widget_is_sensitive(GTK_WIDGET(webview)))
		g_signal_emit(webview, signals[CHANGED], 0);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

GtkWidget *
gtk_webview_new(void)
{
	return GTK_WIDGET(g_object_new(gtk_webview_get_type(), NULL));
}

static void
gtk_webview_finalize(GObject *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	gpointer temp;

	if (priv->loader)
		g_source_remove(priv->loader);

	while (!g_queue_is_empty(priv->load_queue)) {
		temp = g_queue_pop_head(priv->load_queue);
		temp = g_queue_pop_head(priv->load_queue);
		g_free(temp);
	}
	g_queue_free(priv->load_queue);

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(webview));
}

static void
gtk_webview_class_init(GtkWebViewClass *klass, gpointer userdata)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	parent_class = g_type_class_ref(webkit_web_view_get_type());
	gobject_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(GtkWebViewPriv));

	signals[BUTTONS_UPDATE] = g_signal_new("allowed-formats-updated",
	                                       G_TYPE_FROM_CLASS(gobject_class),
	                                       G_SIGNAL_RUN_FIRST,
	                                       G_STRUCT_OFFSET(GtkWebViewClass, buttons_update),
	                                       NULL, 0, g_cclosure_marshal_VOID__INT,
	                                       G_TYPE_NONE, 1, G_TYPE_INT);
	signals[TOGGLE_FORMAT] = g_signal_new("format-toggled",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
	                                      G_STRUCT_OFFSET(GtkWebViewClass, toggle_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__INT,
	                                      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[CLEAR_FORMAT] = g_signal_new("format-cleared",
	                                     G_TYPE_FROM_CLASS(gobject_class),
	                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
	                                     G_STRUCT_OFFSET(GtkWebViewClass, clear_format),
	                                     NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                     G_TYPE_NONE, 0);
	signals[UPDATE_FORMAT] = g_signal_new("format-updated",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_FIRST,
	                                      G_STRUCT_OFFSET(GtkWebViewClass, update_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                      G_TYPE_NONE, 0);
	signals[CHANGED] = g_signal_new("changed",
	                                G_TYPE_FROM_CLASS(gobject_class),
	                                G_SIGNAL_RUN_FIRST,
	                                G_STRUCT_OFFSET(GtkWebViewClass, changed),
	                                NULL, NULL, g_cclosure_marshal_VOID__VOID,
	                                G_TYPE_NONE, 0);

	klass->toggle_format = webview_toggle_format;
	klass->clear_format = webview_clear_formatting;

	gobject_class->finalize = gtk_webview_finalize;

	binding_set = gtk_binding_set_by_class(parent_class);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_b, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_BOLD);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_i, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_ITALIC);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_u, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_UNDERLINE);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_plus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_equal, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_minus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_SHRINK);

	binding_set = gtk_binding_set_by_class(klass);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_r, GDK_CONTROL_MASK,
	                             "format-cleared", 0);
}

static void
gtk_webview_init(GtkWebView *webview, gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	priv->load_queue = g_queue_new();

	g_signal_connect(G_OBJECT(webview), "navigation-policy-decision-requested",
	                 G_CALLBACK(webview_navigation_decision), NULL);

	g_signal_connect(G_OBJECT(webview), "load-started",
	                 G_CALLBACK(webview_load_started), NULL);

	g_signal_connect(G_OBJECT(webview), "load-finished",
	                 G_CALLBACK(webview_load_finished), NULL);

	g_signal_connect(G_OBJECT(webview), "resource-request-starting",
	                 G_CALLBACK(webview_resource_loading), NULL);
}

GType
gtk_webview_get_type(void)
{
	static GType mview_type = 0;
	if (G_UNLIKELY(mview_type == 0)) {
		static const GTypeInfo mview_info = {
			sizeof(GtkWebViewClass),
			NULL,
			NULL,
			(GClassInitFunc)gtk_webview_class_init,
			NULL,
			NULL,
			sizeof(GtkWebView),
			0,
			(GInstanceInitFunc)gtk_webview_init,
			NULL
		};
		mview_type = g_type_register_static(webkit_web_view_get_type(),
				"GtkWebView", &mview_info, 0);
	}
	return mview_type;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

char *
gtk_webview_quote_js_string(const char *text)
{
	GString *str = g_string_new("\"");
	const char *cur = text;

	while (cur && *cur) {
		switch (*cur) {
			case '\\':
				g_string_append(str, "\\\\");
				break;
			case '\"':
				g_string_append(str, "\\\"");
				break;
			case '\r':
				g_string_append(str, "<br/>");
				break;
			case '\n':
				break;
			default:
				g_string_append_c(str, *cur);
		}
		cur++;
	}

	g_string_append_c(str, '"');

	return g_string_free(str, FALSE);
}

void
gtk_webview_safe_execute_script(GtkWebView *webview, const char *script)
{
	GtkWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->load_queue, GINT_TO_POINTER(LOAD_JS));
	g_queue_push_tail(priv->load_queue, g_strdup(script));
	if (!priv->is_loading && priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

void
gtk_webview_load_html_string(GtkWebView *webview, const char *html)
{
	g_return_if_fail(webview != NULL);

	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), html, NULL, NULL,
	                            "file:///");
}

void
gtk_webview_load_html_string_with_selection(GtkWebView *webview, const char *html)
{
	g_return_if_fail(webview != NULL);

	gtk_webview_load_html_string(webview, html);
	gtk_webview_safe_execute_script(webview,
		"var s = window.getSelection();"
		"var r = document.createRange();"
		"var n = document.getElementById('caret');"
		"r.selectNodeContents(n);"
		"var f = r.extractContents();"
		"r.selectNode(n);"
		"r.insertNode(f);"
		"n.parentNode.removeChild(n);"
		"s.removeAllRanges();"
		"s.addRange(r);");
}

void
gtk_webview_append_html(GtkWebView *webview, const char *html)
{
	GtkWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->load_queue, GINT_TO_POINTER(LOAD_HTML));
	g_queue_push_tail(priv->load_queue, g_strdup(html));
	if (!priv->is_loading && priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

void
gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj)
{
	GtkWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	priv->vadj = vadj;
}

void
gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth)
{
	GtkWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	if (priv->scroll_time)
		g_timer_destroy(priv->scroll_time);
	if (priv->scroll_src)
		g_source_remove(priv->scroll_src);
	if (smooth) {
		priv->scroll_time = g_timer_new();
		priv->scroll_src = g_timeout_add_full(G_PRIORITY_LOW, SCROLL_DELAY, smooth_scroll_cb, priv, NULL);
	} else {
		priv->scroll_time = NULL;
		priv->scroll_src = g_idle_add_full(G_PRIORITY_LOW, scroll_idle_cb, priv, NULL);
	}
}

void
gtk_webview_page_up(GtkWebView *webview)
{
	GtkWebViewPriv *priv;
	GtkAdjustment *vadj;
	gdouble scroll_val;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	vadj = priv->vadj;
	scroll_val = gtk_adjustment_get_value(vadj) - gtk_adjustment_get_page_size(vadj);
	scroll_val = MAX(scroll_val, gtk_adjustment_get_lower(vadj));

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
gtk_webview_page_down(GtkWebView *webview)
{
	GtkWebViewPriv *priv;
	GtkAdjustment *vadj;
	gdouble scroll_val;
	gdouble page_size;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	vadj = priv->vadj;
	page_size = gtk_adjustment_get_page_size(vadj);
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, gtk_adjustment_get_upper(vadj) - page_size);

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
gtk_webview_set_editable(GtkWebView *webview, gboolean editable)
{
	GtkWebViewPriv *priv;
	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	webkit_web_view_set_editable(WEBKIT_WEB_VIEW(webview), editable);

	if (editable) {
		g_signal_connect(G_OBJECT(webview), "user-changed-contents",
		                 G_CALLBACK(editable_input_cb), NULL);
	} else {
		g_signal_handlers_disconnect_by_func(G_OBJECT(webview),
		                                     G_CALLBACK(editable_input_cb),
		                                     NULL);
	}

	priv->format_functions = GTK_WEBVIEW_ALL;
}

void
gtk_webview_setup_entry(GtkWebView *webview, PurpleConnectionFlags flags)
{
	GtkWebViewButtons buttons;

	g_return_if_fail(webview != NULL);

	if (flags & PURPLE_CONNECTION_HTML) {
		gboolean bold, italic, underline, strike;

		buttons = GTK_WEBVIEW_ALL;

		if (flags & PURPLE_CONNECTION_NO_BGCOLOR)
			buttons &= ~GTK_WEBVIEW_BACKCOLOR;
		if (flags & PURPLE_CONNECTION_NO_FONTSIZE)
		{
			buttons &= ~GTK_WEBVIEW_GROW;
			buttons &= ~GTK_WEBVIEW_SHRINK;
		}
		if (flags & PURPLE_CONNECTION_NO_URLDESC)
			buttons &= ~GTK_WEBVIEW_LINKDESC;

		gtk_webview_get_current_format(webview, &bold, &italic, &underline, &strike);

		gtk_webview_set_format_functions(webview, GTK_WEBVIEW_ALL);
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold") != bold)
			gtk_webview_toggle_bold(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic") != italic)
			gtk_webview_toggle_italic(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline") != underline)
			gtk_webview_toggle_underline(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike") != strike)
			gtk_webview_toggle_strike(webview);

		gtk_webview_toggle_fontface(webview,
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/font_face"));

		if (!(flags & PURPLE_CONNECTION_NO_FONTSIZE))
		{
			int size = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/font_size");

			/* 3 is the default. */
			if (size != 3)
				gtk_webview_font_set_size(webview, size);
		}

		gtk_webview_toggle_forecolor(webview,
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"));

		if (!(flags & PURPLE_CONNECTION_NO_BGCOLOR)) {
			gtk_webview_toggle_backcolor(webview,
				purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"));
		} else {
			gtk_webview_toggle_backcolor(webview, "");
		}		

		if (flags & PURPLE_CONNECTION_FORMATTING_WBFO)
			gtk_webview_set_whole_buffer_formatting_only(webview, TRUE);
		else
			gtk_webview_set_whole_buffer_formatting_only(webview, FALSE);
	} else {
		buttons = GTK_WEBVIEW_SMILEY | GTK_WEBVIEW_IMAGE;
		webview_clear_formatting(webview);
	}

	if (flags & PURPLE_CONNECTION_NO_IMAGES)
		buttons &= ~GTK_WEBVIEW_IMAGE;

	if (flags & PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY)
		buttons |= GTK_WEBVIEW_CUSTOM_SMILEY;
	else
		buttons &= ~GTK_WEBVIEW_CUSTOM_SMILEY;

	gtk_webview_set_format_functions(webview, buttons);
}

void
pidgin_webview_set_spellcheck(GtkWebView *webview, gboolean enable)
{
	WebKitWebSettings *settings;

	g_return_if_fail(webview != NULL);
	
	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
	g_object_set(G_OBJECT(settings), "enable-spell-checking", enable, NULL);
	webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webview), settings);
}

void
gtk_webview_set_whole_buffer_formatting_only(GtkWebView *webview, gboolean wbfo)
{
	GtkWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	priv->edit.wbfo = wbfo;
}

void
gtk_webview_set_format_functions(GtkWebView *webview, GtkWebViewButtons buttons)
{
	GtkWebViewPriv *priv;
	GObject *object;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	object = g_object_ref(G_OBJECT(webview));
	priv->format_functions = buttons;
	g_signal_emit(object, signals[BUTTONS_UPDATE], 0, buttons);
	g_object_unref(object);
}

gchar *
gtk_webview_get_head_html(GtkWebView *webview)
{
	WebKitDOMDocument *doc;
	WebKitDOMHTMLHeadElement *head;
	gchar *html;

	g_return_val_if_fail(webview != NULL, NULL);

	doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	head = webkit_dom_document_get_head(doc);
	html = webkit_dom_html_element_get_inner_html(WEBKIT_DOM_HTML_ELEMENT(head));

	return html;
}

gchar *
gtk_webview_get_body_html(GtkWebView *webview)
{
	WebKitDOMDocument *doc;
	WebKitDOMHTMLElement *body;
	gchar *html;

	g_return_val_if_fail(webview != NULL, NULL);

	doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	body = webkit_dom_document_get_body(doc);
	html = webkit_dom_html_element_get_inner_html(body);

	return html;
}

gchar *
gtk_webview_get_body_text(GtkWebView *webview)
{
	WebKitDOMDocument *doc;
	WebKitDOMHTMLElement *body;
	gchar *text;

	g_return_val_if_fail(webview != NULL, NULL);

	doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	body = webkit_dom_document_get_body(doc);
	text = webkit_dom_html_element_get_inner_text(body);

	return text;
}

gchar *
gtk_webview_get_selected_text(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	WebKitDOMDOMWindow *win;
	WebKitDOMDOMSelection *sel;
	WebKitDOMRange *range;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	win = webkit_dom_document_get_default_view(dom);
	sel = webkit_dom_dom_window_get_selection(win);
	range = webkit_dom_dom_selection_get_range_at(sel, 0, NULL);

	return webkit_dom_range_get_text(range);
}

GtkWebViewButtons
gtk_webview_get_format_functions(GtkWebView *webview)
{
	GtkWebViewPriv *priv;

	g_return_val_if_fail(webview != NULL, 0);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	return priv->format_functions;
}

void
gtk_webview_get_current_format(GtkWebView *webview, gboolean *bold,
                               gboolean *italic, gboolean *underline,
                               gboolean *strike)
{
	WebKitDOMDocument *dom;

	g_return_if_fail(webview != NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	if (bold)
		*bold = webkit_dom_document_query_command_state(dom, "bold");
	if (italic)
		*italic = webkit_dom_document_query_command_state(dom, "italic");
	if (underline)
		*underline = webkit_dom_document_query_command_state(dom, "underline");
	if (strike)
		*strike = webkit_dom_document_query_command_state(dom, "strikethrough");
}

char *
gtk_webview_get_current_fontface(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "fontName");
}

char *
gtk_webview_get_current_forecolor(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "foreColor");
}

char *
gtk_webview_get_current_backcolor(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "backColor");
}

gint
gtk_webview_get_current_fontsize(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	gchar *text;
	gint size;

	g_return_val_if_fail(webview != NULL, 0);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	text = webkit_dom_document_query_command_value(dom, "fontSize");
	size = atoi(text);
	g_free(text);

	return size;
}

gboolean
gtk_webview_get_editable(GtkWebView *webview)
{
	return webkit_web_view_get_editable(WEBKIT_WEB_VIEW(webview));
}

void
gtk_webview_clear_formatting(GtkWebView *webview)
{
	GObject *object;

	g_return_if_fail(webview != NULL);

	object = g_object_ref(G_OBJECT(webview));
	g_signal_emit(object, signals[CLEAR_FORMAT], 0);
	g_object_unref(object);
}

void
gtk_webview_toggle_bold(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_BOLD);
}

void
gtk_webview_toggle_italic(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_ITALIC);
}

void
gtk_webview_toggle_underline(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_UNDERLINE);
}

void
gtk_webview_toggle_strike(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_STRIKE);
}

gboolean
gtk_webview_toggle_forecolor(GtkWebView *webview, const char *color)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "foreColor", color);
	emit_format_signal(webview, GTK_WEBVIEW_FORECOLOR);

	return FALSE;
}

gboolean
gtk_webview_toggle_backcolor(GtkWebView *webview, const char *color)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "backColor", color);
	emit_format_signal(webview, GTK_WEBVIEW_BACKCOLOR);

	return FALSE;
}

gboolean
gtk_webview_toggle_fontface(GtkWebView *webview, const char *face)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "fontName", face);
	emit_format_signal(webview, GTK_WEBVIEW_FACE);

	return FALSE;
}

void
gtk_webview_font_set_size(GtkWebView *webview, gint size)
{
	char *tmp;

	g_return_if_fail(webview != NULL);

	tmp = g_strdup_printf("%d", size);
	do_formatting(webview, "fontSize", tmp);
	emit_format_signal(webview, GTK_WEBVIEW_SHRINK|GTK_WEBVIEW_GROW);
	g_free(tmp);
}

void
gtk_webview_font_shrink(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_SHRINK);
}

void
gtk_webview_font_grow(GtkWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, GTK_WEBVIEW_GROW);
}

void
gtk_webview_insert_hr(GtkWebView *webview)
{
	GtkWebViewPriv *priv;
	WebKitDOMDocument *dom;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHorizontalRule", FALSE, "");
	priv->edit.block_changed = FALSE;
}

void
gtk_webview_insert_link(GtkWebView *webview, const char *url, const char *desc)
{
	GtkWebViewPriv *priv;
	WebKitDOMDocument *dom;
	char *link;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	link = g_strdup_printf("<a href='%s'>%s</a>", url, desc ? desc : url);

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHTML", FALSE, link);
	priv->edit.block_changed = FALSE;
	g_free(link);
}

void
gtk_webview_insert_image(GtkWebView *webview, int id)
{
	GtkWebViewPriv *priv;
	WebKitDOMDocument *dom;
	char *img;

	g_return_if_fail(webview != NULL);

	priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	img = g_strdup_printf("<img src='" PURPLE_STORED_IMAGE_PROTOCOL "%d'/>",
	                      id);

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHTML", FALSE, img);
	priv->edit.block_changed = FALSE;
	g_free(img);
}

