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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <JavaScriptCore/JavaScript.h>

#include "util.h"
#include "gtkwebview.h"
#include "imgstore.h"

static WebKitWebViewClass *parent_class = NULL;

struct GtkWebViewPriv {
	GHashTable *images; /**< a map from id to temporary file for the image */
	gboolean    empty;  /**< whether anything has been appended **/

	/* JS execute queue */
	GQueue *js_queue;
	gboolean is_loading;
	GtkAdjustment *vadj;
	guint scroll_src;
	GTimer *scroll_time;
};

GtkWidget* gtk_webview_new (void)
{
	GtkWebView* ret = GTK_WEBVIEW (g_object_new(gtk_webview_get_type(), NULL));
	return GTK_WIDGET (ret);
}

static char*
get_image_filename_from_id (GtkWebView* view, int id)
{
	char *filename = NULL;
	FILE *file;
	PurpleStoredImage* img;

	if (!view->priv->images)
		view->priv->images = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	
	filename = (char*) g_hash_table_lookup (view->priv->images, GINT_TO_POINTER (id));
	if (filename) return filename;
			
	/* else get from img store */
	file = purple_mkstemp (&filename, TRUE);

	img = purple_imgstore_find_by_id (id);

	fwrite (purple_imgstore_get_data (img), purple_imgstore_get_size (img), 1, file);
	g_hash_table_insert (view->priv->images, GINT_TO_POINTER (id), filename);
	fclose (file);
	return filename;
}

static void
clear_single_image (gpointer key, gpointer value, gpointer userdata)
{
	g_unlink ((char*) value);
}

static void
clear_images (GtkWebView* view)
{
	if (!view->priv->images) return;
	g_hash_table_foreach (view->priv->images, clear_single_image, NULL);
	g_hash_table_unref (view->priv->images);
}

/*
 * Replace all <img id=""> tags with <img src="">. I hoped to never
 * write any HTML parsing code, but I'm forced to do this, until 
 * purple changes the way it works.
 */
static char*
replace_img_id_with_src (GtkWebView *view, const char* html)
{
	GString *buffer = g_string_sized_new (strlen (html));
	const char* cur = html;
	char *id;
	int nid;

	while (*cur) {
		const char* img = strstr (cur, "<img");
		if (!img) {
			g_string_append (buffer, cur);
			break;
		} else
			g_string_append_len (buffer, cur, img - cur);

		cur = strstr (img, "/>");
		if (!cur)
			cur = strstr (img, ">");

		if (!cur) { /* invalid html? */
			g_string_printf (buffer, "%s", html);
			break;
		}

		if (strstr (img, "src=") || !strstr (img, "id=")) {
			g_string_printf (buffer, "%s", html);
			break;
		}

		/*
		 * if this is valid HTML, then I can be sure that it
		 * has an id= and does not have an src=, since
		 * '=' cannot appear in parameters.
		 */

		id = strstr (img, "id=") + 3; 

		/* *id can't be \0, since a ">" appears after this */
		if (isdigit (*id)) 
			nid = atoi (id);
		else 
			nid = atoi (id+1);

		/* let's dump this, tag and then dump the src information */
		g_string_append_len (buffer, img, cur - img);

		g_string_append_printf (buffer, " src='file://%s' ", get_image_filename_from_id (view, nid));
	}

	return g_string_free (buffer, FALSE);
}

static void
gtk_webview_finalize (GObject *view)
{
	gpointer temp;
	
	while ((temp = g_queue_pop_head (GTK_WEBVIEW(view)->priv->js_queue)))
		g_free (temp);
	g_queue_free (GTK_WEBVIEW(view)->priv->js_queue);

	clear_images (GTK_WEBVIEW (view));
	g_free (GTK_WEBVIEW(view)->priv);
	G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT(view));
}

static void
gtk_webview_class_init (GtkWebViewClass *klass, gpointer userdata)
{
	parent_class = g_type_class_ref (webkit_web_view_get_type ());
	G_OBJECT_CLASS (klass)->finalize = gtk_webview_finalize;
}

static gboolean
webview_link_clicked (WebKitWebView *view,
		      WebKitWebFrame *frame,
		      WebKitNetworkRequest *request,
		      WebKitWebNavigationAction *navigation_action,
		      WebKitWebPolicyDecision *policy_decision)
{
	const gchar *uri;

	uri = webkit_network_request_get_uri (request);

	/* the gtk imhtml way was to create an idle cb, not sure
	 * why, so right now just using purple_notify_uri directly */
	purple_notify_uri (NULL, uri);
	return TRUE;
}

static gboolean
process_js_script_queue (GtkWebView *view)
{
	char *script;
	if (view->priv->is_loading) return FALSE; /* we will be called when loaded */
	if (!view->priv->js_queue || g_queue_is_empty (view->priv->js_queue))
		return FALSE; /* nothing to do! */

	script = g_queue_pop_head (view->priv->js_queue);
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW(view), script);
	g_free (script);

	return TRUE; /* there may be more for now */
}

static void
webview_load_started (WebKitWebView *view,
		      WebKitWebFrame *frame,
		      gpointer userdata)
{
	/* is there a better way to test for is_loading? */
	GTK_WEBVIEW(view)->priv->is_loading = true;
}

static void
webview_load_finished (WebKitWebView *view,
		       WebKitWebFrame *frame,
		       gpointer userdata)
{
	GTK_WEBVIEW(view)->priv->is_loading = false;
	g_idle_add ((GSourceFunc) process_js_script_queue, view);
}

void
gtk_webview_safe_execute_script (GtkWebView *view, const char* script)
{
	g_queue_push_tail (view->priv->js_queue, g_strdup (script));
	g_idle_add ((GSourceFunc)process_js_script_queue, view);
}

static void
gtk_webview_init (GtkWebView *view, gpointer userdata)
{
	view->priv = g_new0 (struct GtkWebViewPriv, 1);
	g_signal_connect (view, "navigation-policy-decision-requested",
			  G_CALLBACK (webview_link_clicked),
			  view);

	g_signal_connect (view, "load-started",
			  G_CALLBACK (webview_load_started),
			  view);

	g_signal_connect (view, "load-finished",
			  G_CALLBACK (webview_load_finished),
			  view);

	view->priv->empty = TRUE;
	view->priv->js_queue = g_queue_new ();
}


void
gtk_webview_load_html_string_with_imgstore (GtkWebView* view, const char* html)
{
	char* html_imged;
	
	clear_images (view);
	html_imged = replace_img_id_with_src (view, html);
	printf ("%s\n", html_imged);
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (view), html_imged, "file:///");
	g_free (html_imged);
}

char *gtk_webview_quote_js_string(const char *text)
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
		cur ++;
	}
	g_string_append_c (str, '"');
	return g_string_free (str, FALSE);
}

void gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj)
{
	webview->priv->vadj = vadj;
}

/* this is a "hack", my plan is to eventually handle this 
 * correctly using a signals and a plugin: the plugin will have
 * the information as to what javascript function to call. It seems
 * wrong to hardcode that here.
 */
void
gtk_webview_append_html (GtkWebView* view, const char* html)
{
	char* escaped = gtk_webview_quote_js_string (html);
	char* script = g_strdup_printf ("document.write(%s)", escaped);
	printf ("script: %s\n", script);
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view), script);
	view->priv->empty = FALSE;
	gtk_webview_scroll_to_end(view, TRUE);
	g_free (script);
	g_free (escaped);
}

gboolean gtk_webview_is_empty (GtkWebView *view)
{
	return view->priv->empty;
}

#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33 /* milliseconds */

/*
 * Smoothly scroll a WebView.
 *
 * @return TRUE if the window needs to be scrolled further, FALSE if we're at the bottom.
 */
static gboolean smooth_scroll_cb(gpointer data)
{
	struct GtkWebViewPriv *priv = data;
	GtkAdjustment *adj = priv->vadj;
	gdouble max_val = adj->upper - adj->page_size;
	gdouble scroll_val = gtk_adjustment_get_value(adj) + ((max_val - gtk_adjustment_get_value(adj)) / 3);

	g_return_val_if_fail(priv->scroll_time != NULL, FALSE);

	if (g_timer_elapsed(priv->scroll_time, NULL) > MAX_SCROLL_TIME || scroll_val >= max_val) {
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

static gboolean scroll_idle_cb(gpointer data)
{
	struct GtkWebViewPriv *priv = data;
	GtkAdjustment *adj = priv->vadj;
	if(adj) {
		gtk_adjustment_set_value(adj, adj->upper - adj->page_size);
	}
	priv->scroll_src = 0;
	return FALSE;
}

void gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth)
{
	struct GtkWebViewPriv *priv = webview->priv;
	if (priv->scroll_time)
		g_timer_destroy(priv->scroll_time);
	if (priv->scroll_src)
		g_source_remove(priv->scroll_src);
	if(smooth) {
		priv->scroll_time = g_timer_new();
		priv->scroll_src = g_timeout_add_full(G_PRIORITY_LOW, SCROLL_DELAY, smooth_scroll_cb, priv, NULL);
	} else {
		priv->scroll_time = NULL;
		priv->scroll_src = g_idle_add_full(G_PRIORITY_LOW, scroll_idle_cb, priv, NULL);
	}
}

GType gtk_webview_get_type (void)
{
	static GType mview_type = 0;
	if (G_UNLIKELY (mview_type == 0)) {
		static const GTypeInfo mview_info = {
			sizeof (GtkWebViewClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_webview_class_init,
			NULL,
			NULL,
			sizeof (GtkWebView),
			0,
			(GInstanceInitFunc) gtk_webview_init,
			NULL
		};
		mview_type = g_type_register_static(webkit_web_view_get_type (),
				"GtkWebView", &mview_info, 0);
	}
	return mview_type;
}
