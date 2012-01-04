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
#include "pidgin.h"

#include "gtkwebview.h"

#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33 /* milliseconds */

#define GTK_WEBVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_WEBVIEW, GtkWebViewPriv))

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct _GtkWebViewPriv {
	GHashTable *images; /**< a map from id to temporary file for the image */
	gboolean empty;     /**< whether anything has been appended **/

	/* JS execute queue */
	GQueue *js_queue;
	gboolean is_loading;

	/* Scroll adjustments */
	GtkAdjustment *vadj;
	guint scroll_src;
	GTimer *scroll_time;
} GtkWebViewPriv;

/******************************************************************************
 * Globals
 *****************************************************************************/

static WebKitWebViewClass *parent_class = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/

static const char *
get_image_src_from_id(GtkWebViewPriv *priv, int id)
{
	char *src;
	PurpleStoredImage *img;

	if (priv->images) {
		/* Check for already loaded image */
		src = (char *)g_hash_table_lookup(priv->images, GINT_TO_POINTER(id));
		if (src)
			return src;
	} else {
		priv->images = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		                                     NULL, g_free);
	}

	/* Find image in store */
	img = purple_imgstore_find_by_id(id);

	src = (char *)purple_imgstore_get_filename(img);
	if (src) {
		src = g_strdup_printf("file://%s", src);
	} else {
		char *tmp;
		tmp = purple_base64_encode(purple_imgstore_get_data(img),
		                           purple_imgstore_get_size(img));
		src = g_strdup_printf("data:base64,%s", tmp);
		g_free(tmp);
	}

	g_hash_table_insert(priv->images, GINT_TO_POINTER(id), src);

	return src;
}

/*
 * Replace all <img id=""> tags with <img src="">. I hoped to never
 * write any HTML parsing code, but I'm forced to do this, until
 * purple changes the way it works.
 */
static char *
replace_img_id_with_src(GtkWebViewPriv *priv, const char *html)
{
	GString *buffer = g_string_new(NULL);
	const char *cur = html;
	char *id;
	int nid;

	while (*cur) {
		const char *img = strstr(cur, "<img");
		if (!img) {
			g_string_append(buffer, cur);
			break;
		} else
			g_string_append_len(buffer, cur, img - cur);

		cur = strstr(img, "/>");
		if (!cur)
			cur = strstr(img, ">");

		if (!cur) { /* invalid html? */
			g_string_printf(buffer, "%s", html);
			break;
		}

		if (strstr(img, "src=") || !strstr(img, "id=")) {
			g_string_printf(buffer, "%s", html);
			break;
		}

		/*
		 * if this is valid HTML, then I can be sure that it
		 * has an id= and does not have an src=, since
		 * '=' cannot appear in parameters.
		 */

		id = strstr(img, "id=") + 3;

		/* *id can't be \0, since a ">" appears after this */
		if (isdigit(*id))
			nid = atoi(id);
		else
			nid = atoi(id + 1);

		/* let's dump this, tag and then dump the src information */
		g_string_append_len(buffer, img, cur - img);

		g_string_append_printf(buffer, " src='%s' ", get_image_src_from_id(priv, nid));
	}

	return g_string_free(buffer, FALSE);
}

static gboolean
process_js_script_queue(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	char *script;

	if (priv->is_loading)
		return FALSE; /* we will be called when loaded */
	if (!priv->js_queue || g_queue_is_empty(priv->js_queue))
		return FALSE; /* nothing to do! */

	script = g_queue_pop_head(priv->js_queue);
	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(webview), script);
	g_free(script);

	return TRUE; /* there may be more for now */
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
	g_idle_add((GSourceFunc)process_js_script_queue, webview);
}

static gboolean
webview_link_clicked(WebKitWebView *webview,
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
	} else
		webkit_web_policy_decision_use(policy_decision);

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
#if GTK_CHECK_VERSION(2,14,0)
	max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
#else
	max_val = adj->upper - adj->page_size;
#endif
	scroll_val = gtk_adjustment_get_value(adj) +
	             ((max_val - gtk_adjustment_get_value(adj)) / 3);

	if (g_timer_elapsed(priv->scroll_time, NULL) > MAX_SCROLL_TIME
	 || scroll_val >= max_val) {
		/* time's up. jump to the end and kill the timer */
		gtk_adjustment_set_value(adj, max_val);
		g_timer_destroy(priv->scroll_time);
		priv->scroll_time = NULL;
		g_source_remove(priv->scroll_src);
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
#if GTK_CHECK_VERSION(2,14,0)
		max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
#else
		max_val = adj->upper - adj->page_size;
#endif
		gtk_adjustment_set_value(adj, max_val);
	}

	priv->scroll_src = 0;
	return FALSE;
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

	while ((temp = g_queue_pop_head(priv->js_queue)))
		g_free(temp);
	g_queue_free(priv->js_queue);

	if (priv->images)
		g_hash_table_unref(priv->images);

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(webview));
}

static void
gtk_webview_class_init(GtkWebViewClass *klass, gpointer userdata)
{
	parent_class = g_type_class_ref(webkit_web_view_get_type());

	g_type_class_add_private(klass, sizeof(GtkWebViewPriv));

	G_OBJECT_CLASS(klass)->finalize = gtk_webview_finalize;
}

static void
gtk_webview_init(GtkWebView *webview, gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	priv->empty = TRUE;
	priv->js_queue = g_queue_new();

	g_signal_connect(webview, "navigation-policy-decision-requested",
			  G_CALLBACK(webview_link_clicked),
			  webview);

	g_signal_connect(webview, "load-started",
			  G_CALLBACK(webview_load_started),
			  webview);

	g_signal_connect(webview, "load-finished",
			  G_CALLBACK(webview_load_finished),
			  webview);
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

gboolean
gtk_webview_is_empty(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	return priv->empty;
}

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
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->js_queue, g_strdup(script));
	g_idle_add((GSourceFunc)process_js_script_queue, webview);
}

void
gtk_webview_load_html_string_with_imgstore(GtkWebView *webview, const char *html)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	char *html_imged;

	if (priv->images) {
		g_hash_table_unref(priv->images);
		priv->images = NULL;
	}

	html_imged = replace_img_id_with_src(priv, html);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), html_imged, NULL,
	                            NULL, "file:///");
	g_free(html_imged);
}

/* this is a "hack", my plan is to eventually handle this
 * correctly using a signals and a plugin: the plugin will have
 * the information as to what javascript function to call. It seems
 * wrong to hardcode that here.
 */
void
gtk_webview_append_html(GtkWebView *webview, const char *html)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	char *escaped = gtk_webview_quote_js_string(html);
	char *script = g_strdup_printf("document.write(%s)", escaped);
	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(webview), script);
	priv->empty = FALSE;
	gtk_webview_scroll_to_end(webview, TRUE);
	g_free(script);
	g_free(escaped);
}

void
gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	priv->vadj = vadj;
}

void
gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
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

void gtk_webview_page_up(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	GtkAdjustment *vadj = priv->vadj;
	gdouble scroll_val;

#if GTK_CHECK_VERSION(2,14,0)
	scroll_val = gtk_adjustment_get_value(vadj) - gtk_adjustment_get_page_size(vadj);
	scroll_val = MAX(scroll_val, gtk_adjustment_get_lower(vadj));
#else
	scroll_val = gtk_adjustment_get_value(vadj) - vadj->page_size;
	scroll_val = MAX(scroll_val, vadj->lower);
#endif

	gtk_adjustment_set_value(vadj, scroll_val);
}

void gtk_webview_page_down(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	GtkAdjustment *vadj = priv->vadj;
	gdouble scroll_val;
	gdouble page_size;

#if GTK_CHECK_VERSION(2,14,0)
	page_size = gtk_adjustment_get_page_size(vadj);
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, gtk_adjustment_get_upper(vadj) - page_size);
#else
	page_size = vadj->page_size;
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, vadj->upper - page_size);
#endif

	gtk_adjustment_set_value(vadj, scroll_val);
}

