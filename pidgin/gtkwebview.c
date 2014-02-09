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
#include "glibcompat.h"
#include "pidgin.h"

#include <gdk/gdkkeysyms.h>

#include "gtkutils.h"
#include "gtkwebview.h"
#include "gtkwebviewtoolbar.h"

#include "gtkinternal.h"
#include "gtk3compat.h"

#define MAX_FONT_SIZE 7
#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33 /* milliseconds */
#define PIDGIN_WEBVIEW_MAX_PROCESS_TIME 100000 /* microseconds */

#define PIDGIN_WEBVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_WEBVIEW, PidginWebViewPriv))

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
	HTML_APPENDED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	WebKitWebInspector *inspector;
	WebKitDOMNode *node;
} PidginWebViewInspectData;

typedef struct {
	WebKitWebView *webview;
	gunichar ch;
} PidginWebViewInsertData;

typedef struct {
	const char *label;
	gunichar ch;
} GtkUnicodeMenuEntry;

typedef struct {
	char *name;
	int length;

	gboolean (*activate)(PidginWebView *webview, const char *uri);
	gboolean (*context_menu)(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu);
} PidginWebViewProtocol;

struct _PidginWebViewSmiley {
	gchar *smile;
	gchar *file;
	GdkPixbufAnimation *icon;
	gboolean hidden;
	GdkPixbufLoader *loader;
	GSList *anchors;
	PidginWebViewSmileyFlags flags;
	PidginWebView *webview;
	gpointer data;
	gsize datasize;
};

typedef struct _GtkSmileyTree GtkSmileyTree;
struct _GtkSmileyTree {
	GString *values;
	GtkSmileyTree **children;
	PidginWebViewSmiley *image;
};

typedef struct _PidginWebViewPriv {
	/* Processing queues */
	gboolean is_loading;
	GQueue *load_queue;
	guint loader;

	/* Scroll adjustments */
	GtkAdjustment *vadj;
	gboolean autoscroll;
	guint scroll_src;
	GTimer *scroll_time;

	/* Format options */
	PidginWebViewButtons format_functions;
	PidginWebViewToolbar *toolbar;
	struct {
		gboolean wbfo:1;	/* Whole buffer formatting only. */
		gboolean block_changed:1;
	} edit;

	/* Smileys */
	char *protocol_name;
	GHashTable *smiley_data;
	GtkSmileyTree *default_smilies;

	/* WebKit inspector */
	WebKitWebView *inspector_view;
	GtkWindow *inspector_win;
} PidginWebViewPriv;

/******************************************************************************
 * Globals
 *****************************************************************************/

static WebKitWebViewClass *parent_class = NULL;

/******************************************************************************
 * Smileys
 *****************************************************************************/

const char *
pidgin_webview_get_protocol_name(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_val_if_fail(webview != NULL, NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	return priv->protocol_name;
}

void
pidgin_webview_set_protocol_name(PidginWebView *webview, const char *protocol_name)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	priv->protocol_name = g_strdup(protocol_name);
}

static GtkSmileyTree *
gtk_smiley_tree_new(void)
{
	return g_new0(GtkSmileyTree, 1);
}

static void
gtk_smiley_tree_insert(GtkSmileyTree *tree, PidginWebViewSmiley *smiley)
{
	GtkSmileyTree *t = tree;
	const char *x = smiley->smile;

	if (!(*x))
		return;

	do {
		char *pos;
		gsize index;

		if (!t->values)
			t->values = g_string_new("");

		pos = strchr(t->values->str, *x);
		if (!pos) {
			t->values = g_string_append_c(t->values, *x);
			index = t->values->len - 1;
			t->children = g_realloc(t->children, t->values->len * sizeof(GtkSmileyTree *));
			t->children[index] = g_new0(GtkSmileyTree, 1);
		} else
			index = pos - t->values->str;

		t = t->children[index];

		x++;
	} while (*x);

	t->image = smiley;
}

static void
gtk_smiley_tree_destroy(GtkSmileyTree *tree)
{
	GSList *list = g_slist_prepend(NULL, tree);

	while (list) {
		GtkSmileyTree *t = list->data;
		gsize i;
		list = g_slist_delete_link(list, list);
		if (t && t->values) {
			for (i = 0; i < t->values->len; i++)
				list = g_slist_prepend(list, t->children[i]);
			g_string_free(t->values, TRUE);
			g_free(t->children);
		}

		g_free(t);
	}
}

static void
gtk_smiley_tree_remove(GtkSmileyTree *tree, PidginWebViewSmiley *smiley)
{
	GtkSmileyTree *t = tree;
	const gchar *x = smiley->smile;
	int len = 0;

	while (*x) {
		char *pos;

		if (!t->values)
			return;

		pos = strchr(t->values->str, *x);
		if (pos)
			t = t->children[pos - t->values->str];
		else
			return;

		x++; len++;
	}

	t->image = NULL;
}

#if 0
static int
gtk_smiley_tree_lookup(GtkSmileyTree *tree, const char *text)
{
	GtkSmileyTree *t = tree;
	const char *x = text;
	int len = 0;
	const char *amp;
	int alen;

	while (*x) {
		char *pos;

		if (!t->values)
			break;

		if (*x == '&' && (amp = purple_markup_unescape_entity(x, &alen))) {
			gboolean matched = TRUE;
			/* Make sure all chars of the unescaped value match */
			while (*(amp + 1)) {
				pos = strchr(t->values->str, *amp);
				if (pos)
					t = t->children[pos - t->values->str];
				else {
					matched = FALSE;
					break;
				}
				amp++;
			}
			if (!matched)
				break;

			pos = strchr(t->values->str, *amp);
		}
		else if (*x == '<') /* Because we're all WYSIWYG now, a '<' char should
		                     * only appear as the start of a tag.  Perhaps a
		                     * safer (but costlier) check would be to call
		                     * pidgin_webview_is_tag on it */
			break;
		else {
			alen = 1;
			pos = strchr(t->values->str, *x);
		}

		if (pos)
			t = t->children[pos - t->values->str];
		else
			break;

		x += alen;
		len += alen;
	}

	if (t->image)
		return len;

	return 0;
}
#endif

static void
pidgin_webview_disassociate_smiley_foreach(gpointer key, gpointer value,
                                        gpointer user_data)
{
	GtkSmileyTree *tree = (GtkSmileyTree *)value;
	PidginWebViewSmiley *smiley = (PidginWebViewSmiley *)user_data;
	gtk_smiley_tree_remove(tree, smiley);
}

static void
pidgin_webview_disconnect_smiley(PidginWebView *webview, PidginWebViewSmiley *smiley)
{
	smiley->webview = NULL;
	g_signal_handlers_disconnect_matched(webview, G_SIGNAL_MATCH_DATA, 0, 0,
	                                     NULL, NULL, smiley);
}

static void
pidgin_webview_disassociate_smiley(PidginWebViewSmiley *smiley)
{
	if (smiley->webview) {
		PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(smiley->webview);
		gtk_smiley_tree_remove(priv->default_smilies, smiley);
		g_hash_table_foreach(priv->smiley_data,
			pidgin_webview_disassociate_smiley_foreach, smiley);
		g_signal_handlers_disconnect_matched(smiley->webview,
		                                     G_SIGNAL_MATCH_DATA, 0, 0, NULL,
		                                     NULL, smiley);
		smiley->webview = NULL;
	}
}

void
pidgin_webview_associate_smiley(PidginWebView *webview, const char *sml,
                             PidginWebViewSmiley *smiley)
{
	GtkSmileyTree *tree;
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);
	g_return_if_fail(PIDGIN_IS_WEBVIEW(webview));

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	if (sml == NULL)
		tree = priv->default_smilies;
	else if (!(tree = g_hash_table_lookup(priv->smiley_data, sml))) {
		tree = gtk_smiley_tree_new();
		g_hash_table_insert(priv->smiley_data, g_strdup(sml), tree);
	}

	/* need to disconnect old webview, if there is one */
	if (smiley->webview) {
		g_signal_handlers_disconnect_matched(smiley->webview,
		                                     G_SIGNAL_MATCH_DATA, 0, 0, NULL,
		                                     NULL, smiley);
	}

	smiley->webview = webview;

	gtk_smiley_tree_insert(tree, smiley);

	/* connect destroy signal for the webview */
	g_signal_connect(webview, "destroy",
	                 G_CALLBACK(pidgin_webview_disconnect_smiley), smiley);
}

#if 0
static gboolean
pidgin_webview_is_smiley(PidginWebViewPriv *priv, const char *sml, const char *text,
                      int *len)
{
	GtkSmileyTree *tree;

	if (!sml)
		sml = priv->protocol_name;

	if (!sml || !(tree = g_hash_table_lookup(priv->smiley_data, sml)))
		tree = priv->default_smilies;

	if (tree == NULL)
		return FALSE;

	*len = gtk_smiley_tree_lookup(tree, text);
	return (*len > 0);
}
#endif

static PidginWebViewSmiley *
pidgin_webview_smiley_get_from_tree(GtkSmileyTree *t, const char *text)
{
	const char *x = text;
	char *pos;

	if (t == NULL)
		return NULL;

	while (*x) {
		if (!t->values)
			return NULL;

		pos = strchr(t->values->str, *x);
		if (!pos)
			return NULL;

		t = t->children[pos - t->values->str];
		x++;
	}

	return t->image;
}

PidginWebViewSmiley *
pidgin_webview_smiley_find(PidginWebView *webview, const char *sml, const char *text)
{
	PidginWebViewPriv *priv;
	PidginWebViewSmiley *ret;

	g_return_val_if_fail(webview != NULL, NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	/* Look for custom smileys first */
	if (sml != NULL) {
		ret = pidgin_webview_smiley_get_from_tree(g_hash_table_lookup(priv->smiley_data, sml), text);
		if (ret != NULL)
			return ret;
	}

	/* Fall back to check for default smileys */
	return pidgin_webview_smiley_get_from_tree(priv->default_smilies, text);
}

#if 0
static GdkPixbufAnimation *
gtk_smiley_get_image(PidginWebViewSmiley *smiley)
{
	if (!smiley->icon) {
		if (smiley->file) {
			smiley->icon = gdk_pixbuf_animation_new_from_file(smiley->file, NULL);
		} else if (smiley->loader) {
			smiley->icon = gdk_pixbuf_loader_get_animation(smiley->loader);
			if (smiley->icon)
				g_object_ref(G_OBJECT(smiley->icon));
		}
	}

	return smiley->icon;
}
#endif

static void
gtk_custom_smiley_allocated(GdkPixbufLoader *loader, gpointer user_data)
{
	PidginWebViewSmiley *smiley;

	smiley = (PidginWebViewSmiley *)user_data;
	smiley->icon = gdk_pixbuf_loader_get_animation(loader);

	if (smiley->icon)
		g_object_ref(G_OBJECT(smiley->icon));
}

static void
gtk_custom_smiley_closed(GdkPixbufLoader *loader, gpointer user_data)
{
	PidginWebViewSmiley *smiley;
	GtkWidget *icon = NULL;
	GtkTextChildAnchor *anchor = NULL;
	GSList *current = NULL;

	smiley = (PidginWebViewSmiley *)user_data;
	if (!smiley->webview) {
		g_object_unref(G_OBJECT(loader));
		smiley->loader = NULL;
		return;
	}

	for (current = smiley->anchors; current; current = g_slist_next(current)) {
		anchor = GTK_TEXT_CHILD_ANCHOR(current->data);
		if (gtk_text_child_anchor_get_deleted(anchor))
			icon = NULL;
		else
			icon = gtk_image_new_from_animation(smiley->icon);

		if (icon) {
			GList *wids;
			gtk_widget_show(icon);

			wids = gtk_text_child_anchor_get_widgets(anchor);

#if 0
			g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_plaintext",
			                       purple_unescape_html(smiley->smile), g_free);
			g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_htmltext",
			                       g_strdup(smiley->smile), g_free);
#endif

			if (smiley->webview) {
				if (wids) {
					GList *children = gtk_container_get_children(GTK_CONTAINER(wids->data));
					g_list_foreach(children, (GFunc)gtk_widget_destroy, NULL);
					g_list_free(children);
					gtk_container_add(GTK_CONTAINER(wids->data), icon);
				} else
					gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(smiley->webview), icon, anchor);
			}
			g_list_free(wids);
		}
		g_object_unref(anchor);
	}

	g_slist_free(smiley->anchors);
	smiley->anchors = NULL;

	g_object_unref(G_OBJECT(loader));
	smiley->loader = NULL;
}

static void
gtk_custom_smiley_size_prepared(GdkPixbufLoader *loader, gint width, gint height, gpointer data)
{
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys")) {
		int custom_smileys_size = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size");
		if (width <= custom_smileys_size && height <= custom_smileys_size)
			return;

		if (width >= height) {
			height = height * custom_smileys_size / width;
			width = custom_smileys_size;
		} else {
			width = width * custom_smileys_size / height;
			height = custom_smileys_size;
		}
	}
	gdk_pixbuf_loader_set_size(loader, width, height);
}

PidginWebViewSmiley *
pidgin_webview_smiley_create(const char *file, const char *shortcut, gboolean hide,
                          PidginWebViewSmileyFlags flags)
{
	PidginWebViewSmiley *smiley = g_new0(PidginWebViewSmiley, 1);
	smiley->file = g_strdup(file);
	smiley->smile = g_strdup(shortcut);
	smiley->hidden = hide;
	smiley->flags = flags;
	smiley->webview = NULL;
	pidgin_webview_smiley_reload(smiley);
	return smiley;
}

void
pidgin_webview_smiley_reload(PidginWebViewSmiley *smiley)
{
	if (smiley->icon)
		g_object_unref(smiley->icon);
	if (smiley->loader)
		g_object_unref(smiley->loader);

	smiley->icon = NULL;
	smiley->loader = NULL;

	if (smiley->file) {
		/* We do not use the pixbuf loader for a smiley that can be loaded
		 * from a file. (e.g., local custom smileys)
		 */
		return;
	}

	smiley->loader = gdk_pixbuf_loader_new();

	g_signal_connect(smiley->loader, "area_prepared",
	                 G_CALLBACK(gtk_custom_smiley_allocated), smiley);
	g_signal_connect(smiley->loader, "closed",
	                 G_CALLBACK(gtk_custom_smiley_closed), smiley);
	g_signal_connect(smiley->loader, "size_prepared",
	                 G_CALLBACK(gtk_custom_smiley_size_prepared), smiley);
}

const char *
pidgin_webview_smiley_get_smile(const PidginWebViewSmiley *smiley)
{
	return smiley->smile;
}

const char *
pidgin_webview_smiley_get_file(const PidginWebViewSmiley *smiley)
{
	return smiley->file;
}

gboolean
pidgin_webview_smiley_get_hidden(const PidginWebViewSmiley *smiley)
{
	return smiley->hidden;
}

PidginWebViewSmileyFlags
pidgin_webview_smiley_get_flags(const PidginWebViewSmiley *smiley)
{
	return smiley->flags;
}

void
pidgin_webview_smiley_destroy(PidginWebViewSmiley *smiley)
{
	pidgin_webview_disassociate_smiley(smiley);
	g_free(smiley->smile);
	g_free(smiley->file);
	if (smiley->icon)
		g_object_unref(smiley->icon);
	if (smiley->loader)
		g_object_unref(smiley->loader);
	g_free(smiley->data);
	g_free(smiley);
}

void
pidgin_webview_remove_smileys(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	g_hash_table_destroy(priv->smiley_data);
	gtk_smiley_tree_destroy(priv->default_smilies);
	priv->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                                          (GDestroyNotify)gtk_smiley_tree_destroy);
	priv->default_smilies = gtk_smiley_tree_new();
}

void
pidgin_webview_insert_smiley(PidginWebView *webview, const char *sml,
                          const char *smiley)
{
	PidginWebViewPriv *priv;
	char *unescaped;
	PidginWebViewSmiley *webview_smiley;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	unescaped = purple_unescape_html(smiley);
	webview_smiley = pidgin_webview_smiley_find(webview, sml, unescaped);

	if (priv->format_functions & PIDGIN_WEBVIEW_SMILEY) {
		char *tmp;
		/* TODO Better smiley insertion... */
		tmp = g_strdup_printf("<img isEmoticon src='purple-smiley:%p' alt='%s'>",
		                      webview_smiley, smiley);
		pidgin_webview_append_html(webview, tmp);
		g_free(tmp);
	} else {
		pidgin_webview_append_html(webview, smiley);
	}

	g_free(unescaped);
}

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
	PurpleStoredImage *img = NULL;
	const char *filename;

	uri = webkit_network_request_get_uri(request);
	if (purple_str_has_prefix(uri, PURPLE_STORED_IMAGE_PROTOCOL)) {
		int id;

		uri += sizeof(PURPLE_STORED_IMAGE_PROTOCOL) - 1;
		id = strtoul(uri, NULL, 10);

		img = purple_imgstore_find_by_id(id);
		if (!img)
			return;
	} else if (purple_str_has_prefix(uri, PURPLE_STOCK_IMAGE_PROTOCOL)) {
		gchar *p_uri, *found;
		const gchar *domain, *stock_name;

		uri += sizeof(PURPLE_STOCK_IMAGE_PROTOCOL) - 1;

		p_uri = g_strdup(uri);
		found = strchr(p_uri, '/');
		if (!found) {
			purple_debug_warning("webview", "Invalid purple stock "
				"image uri: %s", uri);
			return;
		}

		found[0] = '\0';
		domain = p_uri;
		stock_name = found + 1;

		if (g_strcmp0(domain, "e2ee") == 0) {
			img = _pidgin_e2ee_stock_icon_get(stock_name);
			if (!img)
				return;
		} else {
			purple_debug_warning("webview", "Invalid purple stock "
				"image domain: %s", domain);
			return;
		}
	} else
		return;

	if (img != NULL) {
		filename = purple_imgstore_get_filename(img);
		if (filename && g_path_is_absolute(filename)) {
			gchar *tmp = g_strdup_printf("file://%s", filename);
			webkit_network_request_set_uri(request, tmp);
			g_free(tmp);
		} else {
			gchar *b64, *tmp;
			const gchar *type;

			b64 = purple_base64_encode(
				purple_imgstore_get_data(img),
				purple_imgstore_get_size(img));
			type = purple_imgstore_get_extension(img);
			tmp = g_strdup_printf("data:image/%s;base64,%s",
				type, b64);
			webkit_network_request_set_uri(request, tmp);
			g_free(b64);
			g_free(tmp);
		}
	}
}

static void
process_load_queue_element(PidginWebView *webview)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	int type;
	char *str;
	WebKitDOMDocument *doc;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *start, *end;
	WebKitDOMRange *range;
	gboolean require_scroll = FALSE;

	type = GPOINTER_TO_INT(g_queue_pop_head(priv->load_queue));
	str = g_queue_pop_head(priv->load_queue);

	switch (type) {
		case LOAD_HTML:
			doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
			body = webkit_dom_document_get_body(doc);
			start = webkit_dom_node_get_last_child(WEBKIT_DOM_NODE(body));

			if (priv->autoscroll) {
				require_scroll = (gtk_adjustment_get_value(priv->vadj)
				                 >= (gtk_adjustment_get_upper(priv->vadj) -
				                 1.5*gtk_adjustment_get_page_size(priv->vadj)));
			}

			webkit_dom_html_element_insert_adjacent_html(body, "beforeend",
			                                             str, NULL);

			range = webkit_dom_document_create_range(doc);
			if (start) {
				end = webkit_dom_node_get_last_child(WEBKIT_DOM_NODE(body));
				webkit_dom_range_set_start_after(range,
				                                 WEBKIT_DOM_NODE(start),
				                                 NULL);
				webkit_dom_range_set_end_after(range,
				                               WEBKIT_DOM_NODE(end),
				                               NULL);
			} else {
				webkit_dom_range_select_node_contents(range,
				                                      WEBKIT_DOM_NODE(body),
				                                      NULL);
			}

			if (require_scroll) {
				if (start)
					webkit_dom_element_scroll_into_view(WEBKIT_DOM_ELEMENT(start),
					                                    TRUE);
				else
					webkit_dom_element_scroll_into_view(WEBKIT_DOM_ELEMENT(body),
					                                    TRUE);
			}

			g_signal_emit(webview, signals[HTML_APPENDED], 0, range);

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
}

static gboolean
process_load_queue(PidginWebView *webview)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	gint64 start_time;

	if (priv->is_loading) {
		priv->loader = 0;
		return FALSE;
	}
	if (!priv->load_queue || g_queue_is_empty(priv->load_queue)) {
		priv->loader = 0;
		return FALSE;
	}

	start_time = g_get_monotonic_time();
	while (!g_queue_is_empty(priv->load_queue)) {
		process_load_queue_element(webview);
		if (g_get_monotonic_time() - start_time >
			PIDGIN_WEBVIEW_MAX_PROCESS_TIME)
			break;
	}

	if (g_queue_is_empty(priv->load_queue)) {
		priv->loader = 0;
		return FALSE;
	}
	return TRUE;
}

static void
webview_load_started(WebKitWebView *webview, WebKitWebFrame *frame,
                     gpointer userdata)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	/* is there a better way to test for is_loading? */
	priv->is_loading = TRUE;
}

static void
webview_load_finished(WebKitWebView *webview, WebKitWebFrame *frame,
                      gpointer userdata)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	priv->is_loading = FALSE;
	if (priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

static void
webview_inspector_inspect_element(GtkWidget *item, PidginWebViewInspectData *data)
{
	webkit_web_inspector_inspect_node(data->inspector, data->node);
}

static void
webview_inspector_destroy(GtkWindow *window, PidginWebViewPriv *priv)
{
	g_return_if_fail(priv->inspector_win == window);

	priv->inspector_win = NULL;
	priv->inspector_view = NULL;
}

static WebKitWebView *
webview_inspector_create(WebKitWebInspector *inspector,
	WebKitWebView *webview, gpointer _unused)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	if (priv->inspector_view != NULL)
		return priv->inspector_view;

	priv->inspector_win = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_title(priv->inspector_win, _("WebKit inspector"));
	gtk_window_set_default_size(priv->inspector_win, 600, 400);

	priv->inspector_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(priv->inspector_win),
		GTK_WIDGET(priv->inspector_view));

	g_signal_connect(priv->inspector_win, "destroy",
		G_CALLBACK(webview_inspector_destroy), priv);

	return priv->inspector_view;
}

static gboolean
webview_inspector_show(WebKitWebInspector *inspector, GtkWidget *webview)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);

	gtk_widget_show_all(GTK_WIDGET(priv->inspector_win));

	return TRUE;
}

static PidginWebViewProtocol *
webview_find_protocol(const char *url, gboolean reverse)
{
	PidginWebViewClass *klass;
	GList *iter;
	PidginWebViewProtocol *proto = NULL;
	gssize length = reverse ? (gssize)strlen(url) : -1;

	klass = g_type_class_ref(PIDGIN_TYPE_WEBVIEW);
	for (iter = klass->protocols; iter; iter = iter->next) {
		proto = iter->data;
		if (g_ascii_strncasecmp(url, proto->name, reverse ? MIN(length, proto->length) : proto->length) == 0) {
			g_type_class_unref(klass);
			return proto;
		}
	}

	g_type_class_unref(klass);
	return NULL;
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
		PidginWebViewProtocol *proto = webview_find_protocol(uri, FALSE);
		if (proto) {
			/* XXX: Do something with the return value? */
			proto->activate(PIDGIN_WEBVIEW(webview), uri);
		}
		webkit_web_policy_decision_ignore(policy_decision);
	} else if (reason == WEBKIT_WEB_NAVIGATION_REASON_OTHER)
		webkit_web_policy_decision_use(policy_decision);
	else
		webkit_web_policy_decision_ignore(policy_decision);

	return TRUE;
}

static GtkWidget *
get_input_methods_menu(WebKitWebView *webview)
{
	GtkSettings *settings;
	gboolean show = TRUE;
	GtkWidget *item;
	GtkWidget *menu;
	GtkIMContext *im;

	settings = webview ? gtk_widget_get_settings(GTK_WIDGET(webview)) : gtk_settings_get_default();

	if (settings)
		g_object_get(settings, "gtk-show-input-method-menu", &show, NULL);
	if (!show)
		return NULL;

	item = gtk_image_menu_item_new_with_mnemonic(_("Input _Methods"));

	g_object_get(webview, "im-context", &im, NULL);
	menu = gtk_menu_new();
	gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(im),
	                                     GTK_MENU_SHELL(menu));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);

	return item;
}

/* Values taken from gtktextutil.c */
static const GtkUnicodeMenuEntry bidi_menu_entries[] = {
	{ N_("LRM _Left-to-right mark"), 0x200E },
	{ N_("RLM _Right-to-left mark"), 0x200F },
	{ N_("LRE Left-to-right _embedding"), 0x202A },
	{ N_("RLE Right-to-left e_mbedding"), 0x202B },
	{ N_("LRO Left-to-right _override"), 0x202D },
	{ N_("RLO Right-to-left o_verride"), 0x202E },
	{ N_("PDF _Pop directional formatting"), 0x202C },
	{ N_("ZWS _Zero width space"), 0x200B },
	{ N_("ZWJ Zero width _joiner"), 0x200D },
	{ N_("ZWNJ Zero width _non-joiner"), 0x200C }
};

static void
insert_control_character_cb(GtkMenuItem *item, PidginWebViewInsertData *data)
{
	WebKitWebView *webview = data->webview;
	gunichar ch = data->ch;
	PidginWebViewPriv *priv;
	WebKitDOMDocument *dom;
	char buf[6];

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(PIDGIN_WEBVIEW(webview));
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	g_unichar_to_utf8(ch, buf);
	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHTML", FALSE, buf);
	priv->edit.block_changed = FALSE;
}

static GtkWidget *
get_unicode_menu(WebKitWebView *webview)
{
	GtkSettings *settings;
	gboolean show = TRUE;
	GtkWidget *menuitem;
	GtkWidget *menu;
	gsize i;

	settings = webview ? gtk_widget_get_settings(GTK_WIDGET(webview)) : gtk_settings_get_default();

	if (settings)
		g_object_get(settings, "gtk-show-unicode-menu", &show, NULL);
	if (!show)
		return NULL;

	menuitem = gtk_image_menu_item_new_with_mnemonic(_("_Insert Unicode Control Character"));

	menu = gtk_menu_new();
	for (i = 0; i < G_N_ELEMENTS(bidi_menu_entries); i++) {
		PidginWebViewInsertData *data;
		GtkWidget *item;

		data = g_new0(PidginWebViewInsertData, 1);
		data->webview = webview;
		data->ch = bidi_menu_entries[i].ch;

		item = gtk_menu_item_new_with_mnemonic(_(bidi_menu_entries[i].label));
		g_signal_connect_data(item, "activate",
		                      G_CALLBACK(insert_control_character_cb), data,
		                      (GClosureNotify)g_free, 0);
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);

	return menuitem;
}

static void
do_popup_menu(WebKitWebView *webview, int button, int time, int context,
              WebKitDOMNode *node, const char *uri)
{
	GtkWidget *menu;
	GtkWidget *cut, *copy, *paste, *delete, *select;

	menu = gtk_menu_new();
	g_signal_connect(menu, "selection-done",
	                 G_CALLBACK(gtk_widget_destroy), NULL);

	if ((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION)) {
		PidginWebViewProtocol *proto = NULL;
		GList *children;

		while (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(node)) {
			node = webkit_dom_node_get_parent_node(node);
		}

		if (uri && node)
			proto = webview_find_protocol(uri, FALSE);

		if (proto && proto->context_menu) {
			proto->context_menu(PIDGIN_WEBVIEW(webview),
			                    WEBKIT_DOM_HTML_ANCHOR_ELEMENT(node), menu);
		}

		children = gtk_container_get_children(GTK_CONTAINER(menu));
		if (!children) {
			GtkWidget *item = gtk_menu_item_new_with_label(_("No actions available"));
			gtk_widget_show(item);
			gtk_widget_set_sensitive(item, FALSE);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		} else {
			g_list_free(children);
		}
		gtk_widget_show_all(menu);

	} else {
		/* Using connect_swapped means we don't need any wrapper functions */
		cut = pidgin_new_item_from_stock(menu, _("Cu_t"), GTK_STOCK_CUT,
		                                 NULL, NULL, 0, 0, NULL);
		g_signal_connect_swapped(G_OBJECT(cut), "activate",
		                         G_CALLBACK(webkit_web_view_cut_clipboard),
		                         webview);

		copy = pidgin_new_item_from_stock(menu, _("_Copy"), GTK_STOCK_COPY,
		                                  NULL, NULL, 0, 0, NULL);
		g_signal_connect_swapped(G_OBJECT(copy), "activate",
		                         G_CALLBACK(webkit_web_view_copy_clipboard),
		                         webview);

		paste = pidgin_new_item_from_stock(menu, _("_Paste"), GTK_STOCK_PASTE,
		                                   NULL, NULL, 0, 0, NULL);
		g_signal_connect_swapped(G_OBJECT(paste), "activate",
		                         G_CALLBACK(webkit_web_view_paste_clipboard),
		                         webview);

		delete = pidgin_new_item_from_stock(menu, _("_Delete"), GTK_STOCK_DELETE,
		                                    NULL, NULL, 0, 0, NULL);
		g_signal_connect_swapped(G_OBJECT(delete), "activate",
		                         G_CALLBACK(webkit_web_view_delete_selection),
		                         webview);

		pidgin_separator(menu);

		select = pidgin_new_item_from_stock(menu, _("Select _All"),
		                                    GTK_STOCK_SELECT_ALL,
		                                    NULL, NULL, 0, 0, NULL);
		g_signal_connect_swapped(G_OBJECT(select), "activate",
		                         G_CALLBACK(webkit_web_view_select_all),
		                         webview);

		gtk_widget_set_sensitive(cut,
			webkit_web_view_can_cut_clipboard(webview));
		gtk_widget_set_sensitive(copy,
			webkit_web_view_can_copy_clipboard(webview));
		gtk_widget_set_sensitive(paste,
			webkit_web_view_can_paste_clipboard(webview));
		gtk_widget_set_sensitive(delete,
			webkit_web_view_can_cut_clipboard(webview));
	}

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT
		"/webview/inspector_enabled"))
	{
		WebKitWebSettings *settings;
		GtkWidget *inspect;
		PidginWebViewInspectData *data;

		settings = webkit_web_view_get_settings(webview);
		g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);

		data = g_new0(PidginWebViewInspectData, 1);
		data->inspector = webkit_web_view_get_inspector(webview);
		data->node = node;

		pidgin_separator(menu);

		inspect = pidgin_new_item_from_stock(menu, _("Inspect _Element"), NULL,
		                                     NULL, NULL, 0, 0, NULL);
		g_signal_connect_data(G_OBJECT(inspect), "activate",
		                      G_CALLBACK(webview_inspector_inspect_element),
		                      data, (GClosureNotify)g_free, 0);
	}

	if (webkit_web_view_get_editable(webview)) {
		GtkWidget *im = get_input_methods_menu(webview);
		GtkWidget *unicode = get_unicode_menu(webview);

		if (im || unicode)
			pidgin_separator(menu);

		if (im) {
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), im);
			gtk_widget_show(im);
		}

		if (unicode) {
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), unicode);
			gtk_widget_show(unicode);
		}
	}

	g_signal_emit_by_name(G_OBJECT(webview), "populate-popup", menu);

	gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(webview), NULL);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, time);
}

static gboolean
webview_popup_menu(WebKitWebView *webview)
{
	WebKitDOMDocument *doc;
	WebKitDOMElement *active;
	WebKitDOMElement *link;
	int context;
	char *uri;

	context = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT;
	uri = NULL;

	doc = webkit_web_view_get_dom_document(webview);
	active = webkit_dom_html_document_get_active_element(WEBKIT_DOM_HTML_DOCUMENT(doc));

	link = active;
	while (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(link))
		link = webkit_dom_node_get_parent_element(WEBKIT_DOM_NODE(link));
	if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(link)) {
		context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK;
		uri = webkit_dom_html_anchor_element_get_href(WEBKIT_DOM_HTML_ANCHOR_ELEMENT(link));
	}

	do_popup_menu(webview, 0, gtk_get_current_event_time(),
	              context, WEBKIT_DOM_NODE(active), uri);

	g_free(uri);

	return TRUE;
}

static gboolean
webview_button_pressed(WebKitWebView *webview, GdkEventButton *event)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		WebKitHitTestResult *hit;
		int context;
		WebKitDOMNode *node;
		char *uri;

		hit = webkit_web_view_get_hit_test_result(webview, event);
		g_object_get(G_OBJECT(hit),
		             "context", &context,
		             "inner-node", &node,
		             "link-uri", &uri,
		             NULL);

		do_popup_menu(webview, event->button, event->time, context,
		              node, uri);

		g_free(uri);
		g_object_unref(hit);

		return TRUE;
	}

	return FALSE;
}

/*
 * Smoothly scroll a WebView.
 *
 * @return TRUE if the window needs to be scrolled further, FALSE if we're at the bottom.
 */
static gboolean
smooth_scroll_cb(gpointer data)
{
	PidginWebViewPriv *priv = data;
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
	PidginWebViewPriv *priv = data;
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
emit_format_signal(PidginWebView *webview, PidginWebViewButtons buttons)
{
	g_object_ref(webview);
	g_signal_emit(webview, signals[TOGGLE_FORMAT], 0, buttons);
	g_object_unref(webview);
}

static void
do_formatting(PidginWebView *webview, const char *name, const char *value)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
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
	webkit_dom_document_exec_command(dom, (gchar *)name, FALSE, (gchar *)value);
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
webview_font_shrink(PidginWebView *webview)
{
	gint fontsize;
	char *tmp;

	fontsize = pidgin_webview_get_current_fontsize(webview);
	fontsize = MAX(fontsize - 1, 1);

	tmp = g_strdup_printf("%d", fontsize);
	do_formatting(webview, "fontSize", tmp);
	g_free(tmp);
}

static void
webview_font_grow(PidginWebView *webview)
{
	gint fontsize;
	char *tmp;

	fontsize = pidgin_webview_get_current_fontsize(webview);
	fontsize = MIN(fontsize + 1, MAX_FONT_SIZE);

	tmp = g_strdup_printf("%d", fontsize);
	do_formatting(webview, "fontSize", tmp);
	g_free(tmp);
}

static void
webview_clear_formatting(PidginWebView *webview)
{
	if (!webkit_web_view_get_editable(WEBKIT_WEB_VIEW(webview)))
		return;

	do_formatting(webview, "removeFormat", "");
	do_formatting(webview, "unlink", "");
	do_formatting(webview, "backColor", "inherit");
}

static void
webview_toggle_format(PidginWebView *webview, PidginWebViewButtons buttons)
{
	/* since this function is the handler for the formatting keystrokes,
	   we need to check here that the formatting attempted is permitted */
	buttons &= pidgin_webview_get_format_functions(webview);

	switch (buttons) {
	case PIDGIN_WEBVIEW_BOLD:
		do_formatting(webview, "bold", "");
		break;
	case PIDGIN_WEBVIEW_ITALIC:
		do_formatting(webview, "italic", "");
		break;
	case PIDGIN_WEBVIEW_UNDERLINE:
		do_formatting(webview, "underline", "");
		break;
	case PIDGIN_WEBVIEW_STRIKE:
		do_formatting(webview, "strikethrough", "");
		break;
	case PIDGIN_WEBVIEW_SHRINK:
		webview_font_shrink(webview);
		break;
	case PIDGIN_WEBVIEW_GROW:
		webview_font_grow(webview);
		break;
	default:
		break;
	}
}

static void
editable_input_cb(PidginWebView *webview, gpointer data)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	if (!priv->edit.block_changed && gtk_widget_is_sensitive(GTK_WIDGET(webview)))
		g_signal_emit(webview, signals[CHANGED], 0);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

GtkWidget *
pidgin_webview_new(gboolean editable)
{
	GtkWidget *result;
	WebKitWebView *webview;
	WebKitWebSettings *settings;

	result = g_object_new(pidgin_webview_get_type(), NULL);
	webview = WEBKIT_WEB_VIEW(result);
	settings = webkit_web_view_get_settings(webview);

	g_object_set(G_OBJECT(settings), "default-encoding", "utf-8", NULL);
#ifdef _WIN32
	/* XXX: win32 WebKitGTK replaces backslash with yen sign for
	 * "sans-serif" font. We should figure out, how to disable this
	 * behavior, but for now I will just apply this simple hack (using other
	 * font family).
	 */
	g_object_set(G_OBJECT(settings), "default-font-family", "Verdana", NULL);
#endif
	webkit_web_view_set_settings(webview, settings);

	if (editable) {
		webkit_web_view_set_editable(WEBKIT_WEB_VIEW(webview), editable);

		g_signal_connect(G_OBJECT(webview), "user-changed-contents",
		                 G_CALLBACK(editable_input_cb), NULL);
	}

	return result;
}

static void
pidgin_webview_finalize(GObject *webview)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	gpointer temp;

	if (priv->inspector_win != NULL)
		gtk_widget_destroy(GTK_WIDGET(priv->inspector_win));

	if (priv->loader)
		g_source_remove(priv->loader);

	while (!g_queue_is_empty(priv->load_queue)) {
		temp = g_queue_pop_head(priv->load_queue);
		temp = g_queue_pop_head(priv->load_queue);
		g_free(temp);
	}
	g_queue_free(priv->load_queue);

	g_hash_table_destroy(priv->smiley_data);
	gtk_smiley_tree_destroy(priv->default_smilies);
	g_free(priv->protocol_name);

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(webview));
}

static void
pidgin_webview_class_init(PidginWebViewClass *klass, gpointer userdata)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	parent_class = g_type_class_ref(webkit_web_view_get_type());
	gobject_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(PidginWebViewPriv));

	/* Signals */

	signals[BUTTONS_UPDATE] = g_signal_new("allowed-formats-updated",
	                                       G_TYPE_FROM_CLASS(gobject_class),
	                                       G_SIGNAL_RUN_FIRST,
	                                       G_STRUCT_OFFSET(PidginWebViewClass, buttons_update),
	                                       NULL, 0, g_cclosure_marshal_VOID__INT,
	                                       G_TYPE_NONE, 1, G_TYPE_INT);
	signals[TOGGLE_FORMAT] = g_signal_new("format-toggled",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
	                                      G_STRUCT_OFFSET(PidginWebViewClass, toggle_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__INT,
	                                      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[CLEAR_FORMAT] = g_signal_new("format-cleared",
	                                     G_TYPE_FROM_CLASS(gobject_class),
	                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
	                                     G_STRUCT_OFFSET(PidginWebViewClass, clear_format),
	                                     NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                     G_TYPE_NONE, 0);
	signals[UPDATE_FORMAT] = g_signal_new("format-updated",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_FIRST,
	                                      G_STRUCT_OFFSET(PidginWebViewClass, update_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                      G_TYPE_NONE, 0);
	signals[CHANGED] = g_signal_new("changed",
	                                G_TYPE_FROM_CLASS(gobject_class),
	                                G_SIGNAL_RUN_FIRST,
	                                G_STRUCT_OFFSET(PidginWebViewClass, changed),
	                                NULL, NULL, g_cclosure_marshal_VOID__VOID,
	                                G_TYPE_NONE, 0);
	signals[HTML_APPENDED] = g_signal_new("html-appended",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_FIRST,
	                                      G_STRUCT_OFFSET(PidginWebViewClass, html_appended),
	                                      NULL, NULL,
	                                      g_cclosure_marshal_VOID__OBJECT,
	                                      G_TYPE_NONE, 1, WEBKIT_TYPE_DOM_RANGE,
	                                      NULL);

	/* Class Methods */

	klass->toggle_format = webview_toggle_format;
	klass->clear_format = webview_clear_formatting;

	gobject_class->finalize = pidgin_webview_finalize;

	/* Key Bindings */

	binding_set = gtk_binding_set_by_class(parent_class);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_b, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_BOLD);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_i, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_ITALIC);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_u, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_UNDERLINE);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_plus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_equal, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_minus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             PIDGIN_WEBVIEW_SHRINK);

	binding_set = gtk_binding_set_by_class(klass);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_r, GDK_CONTROL_MASK,
	                             "format-cleared", 0);

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/webview");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", FALSE);
}

static void
pidgin_webview_init(PidginWebView *webview, gpointer userdata)
{
	PidginWebViewPriv *priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	WebKitWebInspector *inspector;

	priv->load_queue = g_queue_new();

	priv->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                                          (GDestroyNotify)gtk_smiley_tree_destroy);
	priv->default_smilies = gtk_smiley_tree_new();

	g_signal_connect(G_OBJECT(webview), "button-press-event",
	                 G_CALLBACK(webview_button_pressed), NULL);

	g_signal_connect(G_OBJECT(webview), "popup-menu",
	                 G_CALLBACK(webview_popup_menu), NULL);

	g_signal_connect(G_OBJECT(webview), "navigation-policy-decision-requested",
	                 G_CALLBACK(webview_navigation_decision), NULL);

	g_signal_connect(G_OBJECT(webview), "load-started",
	                 G_CALLBACK(webview_load_started), NULL);

	g_signal_connect(G_OBJECT(webview), "load-finished",
	                 G_CALLBACK(webview_load_finished), NULL);

	g_signal_connect(G_OBJECT(webview), "resource-request-starting",
	                 G_CALLBACK(webview_resource_loading), NULL);

	inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(webview));
	g_signal_connect(G_OBJECT(inspector), "inspect-web-view",
		G_CALLBACK(webview_inspector_create), NULL);
	g_signal_connect(G_OBJECT(inspector), "show-window",
		G_CALLBACK(webview_inspector_show), webview);
}

GType
pidgin_webview_get_type(void)
{
	static GType mview_type = 0;
	if (G_UNLIKELY(mview_type == 0)) {
		static const GTypeInfo mview_info = {
			sizeof(PidginWebViewClass),
			NULL,
			NULL,
			(GClassInitFunc)pidgin_webview_class_init,
			NULL,
			NULL,
			sizeof(PidginWebView),
			0,
			(GInstanceInitFunc)pidgin_webview_init,
			NULL
		};
		mview_type = g_type_register_static(webkit_web_view_get_type(),
				"PidginWebView", &mview_info, 0);
	}
	return mview_type;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

char *
pidgin_webview_quote_js_string(const char *text)
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
pidgin_webview_safe_execute_script(PidginWebView *webview, const char *script)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->load_queue, GINT_TO_POINTER(LOAD_JS));
	g_queue_push_tail(priv->load_queue, g_strdup(script));
	if (!priv->is_loading && priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

void
pidgin_webview_load_html_string(PidginWebView *webview, const char *html)
{
	g_return_if_fail(webview != NULL);

	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), html, NULL, NULL,
	                            "file:///");
}

void
pidgin_webview_load_html_string_with_selection(PidginWebView *webview, const char *html)
{
	g_return_if_fail(webview != NULL);

	pidgin_webview_load_html_string(webview, html);
	pidgin_webview_safe_execute_script(webview,
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
pidgin_webview_append_html(PidginWebView *webview, const char *html)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->load_queue, GINT_TO_POINTER(LOAD_HTML));
	g_queue_push_tail(priv->load_queue, g_strdup(html));
	if (!priv->is_loading && priv->loader == 0)
		priv->loader = g_idle_add((GSourceFunc)process_load_queue, webview);
}

void
pidgin_webview_set_vadjustment(PidginWebView *webview, GtkAdjustment *vadj)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	priv->vadj = vadj;
}

void
pidgin_webview_scroll_to_end(PidginWebView *webview, gboolean smooth)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
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
pidgin_webview_set_autoscroll(PidginWebView *webview, gboolean scroll)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	priv->autoscroll = scroll;
}

gboolean
pidgin_webview_get_autoscroll(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_val_if_fail(webview != NULL, FALSE);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	return priv->autoscroll;
}

void
pidgin_webview_page_up(PidginWebView *webview)
{
	PidginWebViewPriv *priv;
	GtkAdjustment *vadj;
	gdouble scroll_val;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	vadj = priv->vadj;
	scroll_val = gtk_adjustment_get_value(vadj) - gtk_adjustment_get_page_size(vadj);
	scroll_val = MAX(scroll_val, gtk_adjustment_get_lower(vadj));

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
pidgin_webview_page_down(PidginWebView *webview)
{
	PidginWebViewPriv *priv;
	GtkAdjustment *vadj;
	gdouble scroll_val;
	gdouble page_size;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	vadj = priv->vadj;
	page_size = gtk_adjustment_get_page_size(vadj);
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, gtk_adjustment_get_upper(vadj) - page_size);

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
pidgin_webview_setup_entry(PidginWebView *webview, PurpleConnectionFlags flags)
{
	PidginWebViewButtons buttons;

	g_return_if_fail(webview != NULL);

	if (flags & PURPLE_CONNECTION_FLAG_HTML) {
		gboolean bold, italic, underline, strike;

		buttons = PIDGIN_WEBVIEW_ALL;

		if (flags & PURPLE_CONNECTION_FLAG_NO_BGCOLOR)
			buttons &= ~PIDGIN_WEBVIEW_BACKCOLOR;
		if (flags & PURPLE_CONNECTION_FLAG_NO_FONTSIZE)
		{
			buttons &= ~PIDGIN_WEBVIEW_GROW;
			buttons &= ~PIDGIN_WEBVIEW_SHRINK;
		}
		if (flags & PURPLE_CONNECTION_FLAG_NO_URLDESC)
			buttons &= ~PIDGIN_WEBVIEW_LINKDESC;

		pidgin_webview_get_current_format(webview, &bold, &italic, &underline, &strike);

		pidgin_webview_set_format_functions(webview, PIDGIN_WEBVIEW_ALL);
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold") != bold)
			pidgin_webview_toggle_bold(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic") != italic)
			pidgin_webview_toggle_italic(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline") != underline)
			pidgin_webview_toggle_underline(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike") != strike)
			pidgin_webview_toggle_strike(webview);

		pidgin_webview_toggle_fontface(webview,
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/font_face"));

		if (!(flags & PURPLE_CONNECTION_FLAG_NO_FONTSIZE))
		{
			int size = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/font_size");

			/* 3 is the default. */
			if (size != 3)
				pidgin_webview_font_set_size(webview, size);
		}

		pidgin_webview_toggle_forecolor(webview,
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"));

		if (!(flags & PURPLE_CONNECTION_FLAG_NO_BGCOLOR)) {
			pidgin_webview_toggle_backcolor(webview,
				purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"));
		} else {
			pidgin_webview_toggle_backcolor(webview, "");
		}		

		if (flags & PURPLE_CONNECTION_FLAG_FORMATTING_WBFO)
			pidgin_webview_set_whole_buffer_formatting_only(webview, TRUE);
		else
			pidgin_webview_set_whole_buffer_formatting_only(webview, FALSE);
	} else {
		buttons = PIDGIN_WEBVIEW_SMILEY | PIDGIN_WEBVIEW_IMAGE;
		webview_clear_formatting(webview);
	}

	if (flags & PURPLE_CONNECTION_FLAG_NO_IMAGES)
		buttons &= ~PIDGIN_WEBVIEW_IMAGE;

	if (flags & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY)
		buttons |= PIDGIN_WEBVIEW_CUSTOM_SMILEY;
	else
		buttons &= ~PIDGIN_WEBVIEW_CUSTOM_SMILEY;

	pidgin_webview_set_format_functions(webview, buttons);
}

void
pidgin_webview_set_spellcheck(PidginWebView *webview, gboolean enable)
{
	WebKitWebSettings *settings;

	g_return_if_fail(webview != NULL);
	
	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
	g_object_set(G_OBJECT(settings), "enable-spell-checking", enable, NULL);
	webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webview), settings);
}

void
pidgin_webview_set_whole_buffer_formatting_only(PidginWebView *webview, gboolean wbfo)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	priv->edit.wbfo = wbfo;
}

void
pidgin_webview_set_format_functions(PidginWebView *webview, PidginWebViewButtons buttons)
{
	PidginWebViewPriv *priv;
	GObject *object;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	object = g_object_ref(G_OBJECT(webview));
	priv->format_functions = buttons;
	g_signal_emit(object, signals[BUTTONS_UPDATE], 0, buttons);
	g_object_unref(object);
}

void
pidgin_webview_activate_anchor(WebKitDOMHTMLAnchorElement *link)
{
	WebKitDOMDocument *doc;
	WebKitDOMEvent *event;

	doc = webkit_dom_node_get_owner_document(WEBKIT_DOM_NODE(link));
	event = webkit_dom_document_create_event(doc, "MouseEvent", NULL);
	webkit_dom_event_init_event(event, "click", TRUE, TRUE);
	webkit_dom_node_dispatch_event(WEBKIT_DOM_NODE(link), event, NULL);
}

gboolean
pidgin_webview_class_register_protocol(const char *name,
	gboolean (*activate)(PidginWebView *webview, const char *uri),
	gboolean (*context_menu)(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu))
{
	PidginWebViewClass *klass;
	PidginWebViewProtocol *proto;

	g_return_val_if_fail(name, FALSE);

	klass = g_type_class_ref(PIDGIN_TYPE_WEBVIEW);
	g_return_val_if_fail(klass, FALSE);

	if ((proto = webview_find_protocol(name, TRUE))) {
		if (activate) {
			return FALSE;
		}
		klass->protocols = g_list_remove(klass->protocols, proto);
		g_free(proto->name);
		g_free(proto);
		return TRUE;
	} else if (!activate) {
		return FALSE;
	}

	proto = g_new0(PidginWebViewProtocol, 1);
	proto->name = g_strdup(name);
	proto->length = strlen(name);
	proto->activate = activate;
	proto->context_menu = context_menu;
	klass->protocols = g_list_prepend(klass->protocols, proto);

	return TRUE;
}

gchar *
pidgin_webview_get_head_html(PidginWebView *webview)
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
pidgin_webview_get_body_html(PidginWebView *webview)
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
pidgin_webview_get_body_text(PidginWebView *webview)
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
pidgin_webview_get_selected_text(PidginWebView *webview)
{
	WebKitDOMDocument *dom;
	WebKitDOMDOMWindow *win;
	WebKitDOMDOMSelection *sel;
	WebKitDOMRange *range = NULL;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	win = webkit_dom_document_get_default_view(dom);
	sel = webkit_dom_dom_window_get_selection(win);
	if (webkit_dom_dom_selection_get_range_count(sel))
		range = webkit_dom_dom_selection_get_range_at(sel, 0, NULL);

	if (range)
		return webkit_dom_range_get_text(range);
	else
		return NULL;
}

void
pidgin_webview_get_caret(PidginWebView *webview, WebKitDOMNode **container_ret,
		glong *pos_ret)
{
	WebKitDOMDocument *dom;
	WebKitDOMDOMWindow *win;
	WebKitDOMDOMSelection *sel;
	WebKitDOMRange *range = NULL;
	WebKitDOMNode *start_container, *end_container;
	glong start, end;

	g_return_if_fail(webview && container_ret && pos_ret);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	win = webkit_dom_document_get_default_view(dom);
	sel = webkit_dom_dom_window_get_selection(win);
	if (webkit_dom_dom_selection_get_range_count(sel))
		range = webkit_dom_dom_selection_get_range_at(sel, 0, NULL);

	if (range) {
		start_container = webkit_dom_range_get_start_container(range, NULL);
		start = webkit_dom_range_get_start_offset(range, NULL);
		end_container = webkit_dom_range_get_end_container(range, NULL);
		end = webkit_dom_range_get_end_offset(range, NULL);

		if (start == end &&
				webkit_dom_node_is_same_node(start_container, end_container)) {

			*container_ret = start_container;
			*pos_ret = start;
			return;
		}
	}

	*container_ret = NULL;
	*pos_ret = -1;
}

void
pidgin_webview_set_caret(PidginWebView *webview, WebKitDOMNode *container, glong pos)
{
	WebKitDOMDocument *dom;
	WebKitDOMDOMWindow *win;
	WebKitDOMDOMSelection *sel;

	g_return_if_fail(webview && container && pos >= 0);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	win = webkit_dom_document_get_default_view(dom);
	sel = webkit_dom_dom_window_get_selection(win);

	webkit_dom_dom_selection_set_position(sel, container, pos, NULL);
}

PidginWebViewButtons
pidgin_webview_get_format_functions(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_val_if_fail(webview != NULL, 0);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	return priv->format_functions;
}

void
pidgin_webview_get_current_format(PidginWebView *webview, gboolean *bold,
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
pidgin_webview_get_current_fontface(PidginWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "fontName");
}

char *
pidgin_webview_get_current_forecolor(PidginWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "foreColor");
}

char *
pidgin_webview_get_current_backcolor(PidginWebView *webview)
{
	WebKitDOMDocument *dom;

	g_return_val_if_fail(webview != NULL, NULL);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "backColor");
}

gint
pidgin_webview_get_current_fontsize(PidginWebView *webview)
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

void
pidgin_webview_clear_formatting(PidginWebView *webview)
{
	GObject *object;

	g_return_if_fail(webview != NULL);

	object = g_object_ref(G_OBJECT(webview));
	g_signal_emit(object, signals[CLEAR_FORMAT], 0);
	g_object_unref(object);
}

void
pidgin_webview_toggle_bold(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_BOLD);
}

void
pidgin_webview_toggle_italic(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_ITALIC);
}

void
pidgin_webview_toggle_underline(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_UNDERLINE);
}

void
pidgin_webview_toggle_strike(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_STRIKE);
}

gboolean
pidgin_webview_toggle_forecolor(PidginWebView *webview, const char *color)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "foreColor", color);
	emit_format_signal(webview, PIDGIN_WEBVIEW_FORECOLOR);

	return FALSE;
}

gboolean
pidgin_webview_toggle_backcolor(PidginWebView *webview, const char *color)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "backColor", color);
	emit_format_signal(webview, PIDGIN_WEBVIEW_BACKCOLOR);

	return FALSE;
}

gboolean
pidgin_webview_toggle_fontface(PidginWebView *webview, const char *face)
{
	g_return_val_if_fail(webview != NULL, FALSE);

	do_formatting(webview, "fontName", face);
	emit_format_signal(webview, PIDGIN_WEBVIEW_FACE);

	return FALSE;
}

void
pidgin_webview_font_set_size(PidginWebView *webview, gint size)
{
	char *tmp;

	g_return_if_fail(webview != NULL);

	tmp = g_strdup_printf("%d", size);
	do_formatting(webview, "fontSize", tmp);
	emit_format_signal(webview, PIDGIN_WEBVIEW_SHRINK|PIDGIN_WEBVIEW_GROW);
	g_free(tmp);
}

void
pidgin_webview_font_shrink(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_SHRINK);
}

void
pidgin_webview_font_grow(PidginWebView *webview)
{
	g_return_if_fail(webview != NULL);
	emit_format_signal(webview, PIDGIN_WEBVIEW_GROW);
}

void
pidgin_webview_insert_hr(PidginWebView *webview)
{
	PidginWebViewPriv *priv;
	WebKitDOMDocument *dom;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHorizontalRule", FALSE, "");
	priv->edit.block_changed = FALSE;
}

void
pidgin_webview_insert_link(PidginWebView *webview, const char *url, const char *desc)
{
	PidginWebViewPriv *priv;
	WebKitDOMDocument *dom;
	char *link;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	link = g_strdup_printf("<a href='%s'>%s</a>", url, desc ? desc : url);

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHTML", FALSE, link);
	priv->edit.block_changed = FALSE;
	g_free(link);
}

void
pidgin_webview_insert_image(PidginWebView *webview, int id)
{
	PidginWebViewPriv *priv;
	WebKitDOMDocument *dom;
	char *img;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	img = g_strdup_printf("<img src='" PURPLE_STORED_IMAGE_PROTOCOL "%d'/>",
	                      id);

	priv->edit.block_changed = TRUE;
	webkit_dom_document_exec_command(dom, "insertHTML", FALSE, img);
	priv->edit.block_changed = FALSE;
	g_free(img);
}

void
pidgin_webview_set_toolbar(PidginWebView *webview, GtkWidget *toolbar)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	priv->toolbar = PIDGIN_WEBVIEWTOOLBAR(toolbar);
}

void
pidgin_webview_show_toolbar(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	g_return_if_fail(priv->toolbar != NULL);

	gtk_widget_show(GTK_WIDGET(priv->toolbar));
}

void
pidgin_webview_hide_toolbar(PidginWebView *webview)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	g_return_if_fail(priv->toolbar != NULL);

	gtk_widget_hide(GTK_WIDGET(priv->toolbar));
}

void
pidgin_webview_activate_toolbar(PidginWebView *webview, PidginWebViewAction action)
{
	PidginWebViewPriv *priv;

	g_return_if_fail(webview != NULL);

	priv = PIDGIN_WEBVIEW_GET_PRIVATE(webview);
	g_return_if_fail(priv->toolbar != NULL);

	pidgin_webviewtoolbar_activate(priv->toolbar, action);
}

