/**
 * @file gtkdebug.c GTK+ Debug API
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
 */
#include "internal.h"
#include "pidgin.h"

#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "util.h"

#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"

#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

typedef struct
{
	GtkWidget *window;
	GtkWidget *text;
	GtkWidget *filter;
	GtkWidget *expression;
	GtkWidget *filterlevel;

	gboolean paused;

	gboolean invert;
	gboolean highlight;
	guint timer;
	GRegex *regex;
} DebugWindow;

#define EMPTY_HTML \
	"<html><head><style>" \
	"body{white-space:pre-wrap;}" \
	"div.l0{color:#000000;}"                    /* All debug levels. */ \
	"div.l1{color:#666666;}"                    /* Misc.             */ \
	"div.l2{color:#000000;}"                    /* Information.      */ \
	"div.l3{color:#660000;}"                    /* Warnings.         */ \
	"div.l4{color:#FF0000;}"                    /* Errors.           */ \
	"div.l5{color:#FF0000;font-weight:bold;}"   /* Fatal errors.     */ \
	/* Filter levels */ \
	"div#pause~div{display:none;}" \
	"body.l1 div.l0{display:none;}" \
	"body.l2 div.l0,body.l2 div.l1{display:none;}" \
	"body.l3 div.l0,body.l3 div.l1,body.l3 div.l2{display:none;}" \
	"body.l4 div.l0,body.l4 div.l1,body.l4 div.l2,body.l4 div.l3{display:none;}" \
	"body.l5 div.l0,body.l5 div.l1,body.l5 div.l2,body.l5 div.l3,body.l5 div.l4{display:none;}" \
	/* Regex */ \
	"div.hide{display:none;}" \
	"span.regex{background-color:#ffafaf;font-weight:bold;}" \
	"</style><script>" \
	"function append(level, time, cat, msg) {" \
		"var div = document.createElement('div');" \
		"div.className = 'l' + level;" \
		"div.appendChild(document.createTextNode('(' + time + ') '));" \
		"if (cat) {" \
			"var cat_n = document.createElement('b');" \
			"cat_n.appendChild(document.createTextNode(cat + ':'));" \
			"div.appendChild(cat_n);" \
			"div.appendChild(document.createTextNode(' '));" \
		"}" \
		"div.appendChild(document.createTextNode(msg));" \
		"document.body.appendChild(div);" \
		"alert('appended');" \
	"}" \
	"</script></head><body class=l%d></body></html>"

static DebugWindow *debug_win = NULL;
static guint debug_enabled_timer = 0;

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	purple_prefs_disconnect_by_handle(pidgin_debug_get_handle());

	if(debug_win->timer != 0) {
		const gchar *text;

		purple_timeout_remove(debug_win->timer);

		text = gtk_entry_get_text(GTK_ENTRY(debug_win->expression));
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/debug/regex", text);
	}
	if (debug_win->regex != NULL)
		g_regex_unref(debug_win->regex);

	/* If the "Save Log" dialog is open then close it */
	purple_request_close_with_handle(debug_win);

	g_free(debug_win);
	debug_win = NULL;

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, DebugWindow *win)
{
	if (gtk_widget_get_visible(w)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/height", event->height);
	}

	return FALSE;
}

static void
save_writefile_cb(void *user_data, const char *filename)
{
	DebugWindow *win = (DebugWindow *)user_data;
	FILE *fp;
	char *tmp;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(win, NULL, _("Unable to open file."), NULL);
		return;
	}

	tmp = gtk_webview_get_body_text(GTK_WEBVIEW(win->text));
	fprintf(fp, "Pidgin Debug Log : %s\n", purple_date_format_full(NULL));
	fprintf(fp, "%s", tmp);
	g_free(tmp);

	fclose(fp);
}

static void
save_cb(GtkWidget *w, DebugWindow *win)
{
	purple_request_file(win, _("Save Debug Log"), "purple-debug.log", TRUE,
					  G_CALLBACK(save_writefile_cb), NULL,
					  NULL, NULL, NULL,
					  win);
}

static void
clear_cb(GtkWidget *w, DebugWindow *win)
{
	char *tmp;
	int level;

	level = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel");
	tmp = g_strdup_printf(EMPTY_HTML, level);
	gtk_webview_load_html_string(GTK_WEBVIEW(win->text), tmp);
	g_free(tmp);
}

static void
pause_cb(GtkWidget *w, DebugWindow *win)
{
	win->paused = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));

	if (win->paused) {
		gtk_webview_append_html(GTK_WEBVIEW(win->text), "<div id=pause></div>");
	} else {
		WebKitDOMDocument *dom;
		WebKitDOMElement *pause;

		dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(win->text));
		pause = webkit_dom_document_get_element_by_id(dom, "pause");
		if (pause) {
			WebKitDOMNode *parent;
			parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(pause));
			webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(pause), NULL);
		}
	}
}

/******************************************************************************
 * regex stuff
 *****************************************************************************/
static void
regex_clear_color(GtkWidget *w) {
	gtk_widget_modify_base(w, GTK_STATE_NORMAL, NULL);
}

static void
regex_change_color(GtkWidget *w, guint16 r, guint16 g, guint16 b) {
	GdkColor color;

	color.red = r;
	color.green = g;
	color.blue = b;

	gtk_widget_modify_base(w, GTK_STATE_NORMAL, &color);
}

static void
regex_toggle_div(WebKitDOMNode *div)
{
	WebKitDOMDOMTokenList *classes;

	if (!WEBKIT_DOM_IS_HTML_ELEMENT(div))
		return;

#if (WEBKIT_MAJOR_VERSION == 1 && \
     WEBKIT_MINOR_VERSION == 9 && \
     WEBKIT_MICRO_VERSION == 90)
	/* Workaround WebKit API bug. */
	classes = webkit_dom_element_get_class_list(WEBKIT_DOM_ELEMENT(div));
#else
	classes = webkit_dom_html_element_get_class_list(WEBKIT_DOM_HTML_ELEMENT(div));
#endif
	webkit_dom_dom_token_list_toggle(classes, "hide", NULL);
	g_object_unref(classes);
}

static void
regex_highlight_clear(WebKitDOMDocument *dom)
{
	WebKitDOMNodeList *nodes;
	gulong i;

	/* Remove highlighting SPANs */
	nodes = webkit_dom_document_get_elements_by_class_name(dom, "regex");
	i = webkit_dom_node_list_get_length(nodes);
	while (i--) {
		WebKitDOMNode *span, *parent;
		char *content;
		WebKitDOMText *text;
		GError *err = NULL;

		span = webkit_dom_node_list_item(nodes, i);
		parent = webkit_dom_node_get_parent_node(span);

		content = webkit_dom_node_get_text_content(span);
		text = webkit_dom_document_create_text_node(dom, content);
		g_free(content);

		webkit_dom_node_replace_child(parent, WEBKIT_DOM_NODE(text), span, &err);
	}

	g_object_unref(nodes);
}

static void
regex_highlight_text_nodes(WebKitDOMDocument *dom, WebKitDOMNode *div,
                           gint start_pos, gint end_pos)
{
	GSList *data = NULL;
	WebKitDOMNode *node;
	WebKitDOMRange *range;
	WebKitDOMElement *span;
	gint ind, end_ind;
	gint this_start, this_end;

	ind = 0;
	webkit_dom_node_normalize(div);
	node = div;

	/* First, find the container nodes and offsets to apply highlighting. */
	do {
		if (webkit_dom_node_get_node_type(node) == 3/*TEXT_NODE*/) {
			/* The GObject model does not correctly reflect the type, hence the
			   regular cast. */
			end_ind = ind + webkit_dom_character_data_get_length((WebKitDOMCharacterData*)node);

			if (start_pos <= ind)
				this_start = 0;
			else if (start_pos < end_ind)
				this_start = start_pos - ind;
			else
				this_start = -1;

			if (end_pos < end_ind)
				this_end = end_pos - ind;
			else
				this_end = end_ind - ind;

			if (this_start != -1 && this_start < this_end) {
				data = g_slist_prepend(data, GINT_TO_POINTER(this_end));
				data = g_slist_prepend(data, GINT_TO_POINTER(this_start));
				data = g_slist_prepend(data, node);
			}

			ind = end_ind;
		}

		if (webkit_dom_node_has_child_nodes(node)) {
			node = webkit_dom_node_get_first_child(node);
		} else {
			while (node != div) {
				WebKitDOMNode *next;

				next = webkit_dom_node_get_next_sibling(node);
				if (next) {
					node = next;
					break;
				} else {
					node = webkit_dom_node_get_parent_node(node);
				}
			}
		}
	} while (node != div);

	/* Second, apply highlighting to saved sections. Changing the DOM is
	   automatically reflected in all WebKit API, so we have to do this after
	   finding the offsets, or things could get complicated. */
	while (data) {
		node = WEBKIT_DOM_NODE(data->data);
		data = g_slist_delete_link(data, data);
		this_start = GPOINTER_TO_INT(data->data);
		data = g_slist_delete_link(data, data);
		this_end = GPOINTER_TO_INT(data->data);
		data = g_slist_delete_link(data, data);

		range = webkit_dom_document_create_range(dom);
		webkit_dom_range_set_start(range, node, this_start, NULL);
		webkit_dom_range_set_end(range, node, this_end, NULL);
		span = webkit_dom_document_create_element(dom, "span", NULL);
		webkit_dom_html_element_set_class_name(WEBKIT_DOM_HTML_ELEMENT(span),
		                                       "regex");
		webkit_dom_range_surround_contents(range, WEBKIT_DOM_NODE(span), NULL);
	}
}

static void
regex_match(DebugWindow *win, WebKitDOMDocument *dom, WebKitDOMNode *div)
{
	GMatchInfo *match_info;
	gchar *text;
	gchar *plaintext;

	text = webkit_dom_node_get_text_content(div);
	if (!text)
		return;

	/* I don't like having to do this, but we need it for highlighting.  Plus
	 * it makes the ^ and $ operators work :)
	 */
	plaintext = purple_markup_strip_html(text);

	/* We do a first pass to see if it matches at all.  If it does we work out
	 * the offsets to highlight.
	 */
	if (g_regex_match(win->regex, plaintext, 0, &match_info) != win->invert) {
		/* If we're not highlighting or the expression is inverted, we're
		 * done and move on.
		 */
		if (!win->highlight || win->invert) {
			g_free(plaintext);
			g_match_info_free(match_info);
			return;
		}

		do {
			gint m, count;
			gint start_pos, end_pos;

			if (!g_match_info_matches(match_info))
				break;

			count = g_match_info_get_match_count(match_info);
			if (count == 1)
				m = 0;
			else
				m = 1;

			for (; m < count; m++)
			{
				g_match_info_fetch_pos(match_info, m, &start_pos, &end_pos);

				if (end_pos == -1)
					break;

				regex_highlight_text_nodes(dom, div, start_pos, end_pos);
			}
		} while (g_match_info_next(match_info, NULL));

		g_match_info_free(match_info);
	} else {
		regex_toggle_div(div);
	}

	g_free(plaintext);
	g_free(text);
}

static void
regex_toggle_filter(DebugWindow *win, gboolean filter)
{
	WebKitDOMDocument *dom;
	WebKitDOMNodeList *list;
	gulong i;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(win->text));

	if (win->highlight)
		regex_highlight_clear(dom);

	/* Re-show debug lines that didn't match regex */
	list = webkit_dom_document_get_elements_by_class_name(dom, "hide");
	i = webkit_dom_node_list_get_length(list);

	while (i--) {
		WebKitDOMNode *div = webkit_dom_node_list_item(list, i);
		regex_toggle_div(div);
	}

	g_object_unref(list);

	if (filter) {
		list = webkit_dom_document_get_elements_by_tag_name(dom, "div");

		for (i = 0; i < webkit_dom_node_list_get_length(list); i++) {
			WebKitDOMNode *div = webkit_dom_node_list_item(list, i);
			regex_match(win, dom, div);
		}

		g_object_unref(list);
	}
}

static void
regex_pref_filter_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val), current;

	if (!win || !win->window)
		return;

	current = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter));
	if (active != current)
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter), active);
}

static void
regex_pref_expression_cb(const gchar *name, PurplePrefType type,
						 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	const gchar *exp = (const gchar *)val;

	gtk_entry_set_text(GTK_ENTRY(win->expression), exp);
}

static void
regex_pref_invert_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->invert = active;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_toggle_filter(win, TRUE);
}

static void
regex_pref_highlight_cb(const gchar *name, PurplePrefType type,
						gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->highlight = active;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_toggle_filter(win, TRUE);
}

static gboolean
regex_timer_cb(DebugWindow *win) {
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/debug/regex", text);

	win->timer = 0;

	return FALSE;
}

static void
regex_changed_cb(GtkWidget *w, DebugWindow *win) {
	const gchar *text;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter))) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter),
									 FALSE);
	}

	if (win->timer == 0)
		win->timer = purple_timeout_add_seconds(5, (GSourceFunc)regex_timer_cb, win);

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));

	if (text == NULL || *text == '\0') {
		regex_clear_color(win->expression);
		gtk_widget_set_sensitive(win->filter, FALSE);
		return;
	}

	if (win->regex)
		g_regex_unref(win->regex);
	win->regex = g_regex_new(text, G_REGEX_CASELESS, 0, NULL);
	if (win->regex == NULL) {
		/* failed to compile */
		regex_change_color(win->expression, 0xFFFF, 0xAFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, FALSE);
	} else {
		/* compiled successfully */
		regex_change_color(win->expression, 0xAFFF, 0xFFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, TRUE);
	}
}

static void
regex_key_release_cb(GtkWidget *w, GdkEventKey *e, DebugWindow *win) {
	if(e->keyval == GDK_KEY_Return &&
	   gtk_widget_is_sensitive(win->filter) &&
	   !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
	{
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter), TRUE);
	}
}

static void
regex_menu_cb(GtkWidget *item, const gchar *pref) {
	gboolean active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));

	purple_prefs_set_bool(pref, active);
}

static void
regex_popup_cb(GtkEntry *entry, GtkWidget *menu, DebugWindow *win) {
	pidgin_separator(menu);
	pidgin_new_check_item(menu, _("Invert"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/invert", win->invert);
	pidgin_new_check_item(menu, _("Highlight matches"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/highlight", win->highlight);
}

static void
regex_filter_toggled_cb(GtkToggleToolButton *button, DebugWindow *win)
{
	gboolean active;

	active = gtk_toggle_tool_button_get_active(button);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/filter", active);

	if (!GTK_IS_WEBVIEW(win->text))
		return;

	regex_toggle_filter(win, active);
}

static void
filter_level_pref_changed(const char *name, PurplePrefType type, gconstpointer value, gpointer data)
{
	DebugWindow *win = data;
	WebKitDOMDocument *dom;
	WebKitDOMHTMLElement *body;
	int level = GPOINTER_TO_INT(value);
	char *tmp;

	if (level != gtk_combo_box_get_active(GTK_COMBO_BOX(win->filterlevel)))
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel), level);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(win->text));
	body = webkit_dom_document_get_body(dom);
	tmp = g_strdup_printf("l%d", level);
	webkit_dom_html_element_set_class_name(body, tmp);
	g_free(tmp);
}

static void
filter_level_changed_cb(GtkWidget *combo, gpointer null)
{
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/filterlevel",
				gtk_combo_box_get_active(GTK_COMBO_BOX(combo)));
}

static void
toolbar_style_pref_changed_cb(const char *name, PurplePrefType type, gconstpointer value, gpointer data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(data), GPOINTER_TO_INT(value));
}

static void
toolbar_icon_pref_changed(GtkWidget *item, GtkWidget *toolbar)
{
	int style = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "user_data"));
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/style", style);
}

static gboolean
toolbar_context(GtkWidget *toolbar, GdkEventButton *event, gpointer null)
{
	GtkWidget *menu, *item;
	const char *text[3];
	GtkToolbarStyle value[3];
	int i;

	if (!(event->button == 3 && event->type == GDK_BUTTON_PRESS))
		return FALSE;

	text[0] = _("_Icon Only");          value[0] = GTK_TOOLBAR_ICONS;
	text[1] = _("_Text Only");          value[1] = GTK_TOOLBAR_TEXT;
	text[2] = _("_Both Icon & Text");   value[2] = GTK_TOOLBAR_BOTH_HORIZ;

	menu = gtk_menu_new();

	for (i = 0; i < 3; i++) {
		item = gtk_check_menu_item_new_with_mnemonic(text[i]);
		g_object_set_data(G_OBJECT(item), "user_data", GINT_TO_POINTER(value[i]));
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(toolbar_icon_pref_changed), toolbar);
		if (value[i] == purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/style"))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);
	return FALSE;
}

static void
debug_window_appended(DebugWindow *win)
{
	WebKitDOMDocument *dom;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *div;

	if (!gtk_toggle_tool_button_get_active(
		GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		return;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(win->text));
	body = webkit_dom_document_get_body(dom);
	div = webkit_dom_node_get_last_child(WEBKIT_DOM_NODE(body));

	if (webkit_dom_element_webkit_matches_selector(
		WEBKIT_DOM_ELEMENT(div), "body>div:not(#pause)", NULL))
		regex_match(win, dom, div);
}

static gboolean debug_window_alert_cb(WebKitWebView *webview,
	WebKitWebFrame *frame, gchar *message, gpointer _win)
{
	DebugWindow *win = _win;

	if (!win || !win->window)
		return FALSE;

	if (g_strcmp0(message, "appended") == 0) {
		debug_window_appended(win);
		return TRUE;
	}

	return FALSE;
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *frame;
	gint width, height;
	void *handle;
	GtkToolItem *item;

	win = g_new0(DebugWindow, 1);

	width  = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/width");
	height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/height");

	win->window = pidgin_create_window(_("Debug Window"), 0, "debug", TRUE);
	purple_debug_info("gtkdebug", "Setting dimensions to %d, %d\n",
					width, height);

	gtk_window_set_default_size(GTK_WINDOW(win->window), width, height);

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win->window), "configure_event",
	                 G_CALLBACK(configure_cb), win);

	handle = pidgin_debug_get_handle();

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win->window), vbox);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/toolbar")) {
		/* Setup our top button bar thingie. */
		toolbar = gtk_toolbar_new();
		gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), TRUE);
		g_signal_connect(G_OBJECT(toolbar), "button-press-event", G_CALLBACK(toolbar_context), win);

		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),
		                      purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/style"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/style",
	                                toolbar_style_pref_changed_cb, toolbar);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
		                          GTK_ICON_SIZE_SMALL_TOOLBAR);

		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

		/* Save */
		item = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
		gtk_tool_item_set_is_important(item, TRUE);
		gtk_tool_item_set_tooltip_text(item, _("Save"));
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(save_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* Clear button */
		item = gtk_tool_button_new_from_stock(GTK_STOCK_CLEAR);
		gtk_tool_item_set_is_important(item, TRUE);
		gtk_tool_item_set_tooltip_text(item, _("Clear"));
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(clear_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* Pause */
		item = gtk_toggle_tool_button_new_from_stock(PIDGIN_STOCK_PAUSE);
		gtk_tool_item_set_is_important(item, TRUE);
		gtk_tool_item_set_tooltip_text(item, _("Pause"));
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(pause_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* regex stuff */
		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* regex toggle button */
		item = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_FIND);
		gtk_tool_item_set_is_important(item, TRUE);
		win->filter = GTK_WIDGET(item);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(win->filter), _("Filter"));
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(win->filter), _("Filter"));
		g_signal_connect(G_OBJECT(win->filter), "clicked", G_CALLBACK(regex_filter_toggled_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(win->filter));

		/* we purposely disable the toggle button here in case
		 * /purple/gtk/debug/expression has an empty string.  If it does not have
		 * an empty string, the change signal will get called and make the
		 * toggle button sensitive.
		 */
		gtk_widget_set_sensitive(win->filter, FALSE);
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter),
									 purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/filter"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/filter",
									regex_pref_filter_cb, win);

		/* regex entry */
		win->expression = gtk_entry_new();
		item = gtk_tool_item_new();
		gtk_widget_set_tooltip_text(win->expression, _("Right click for more options."));
		gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(win->expression));
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* this needs to be before the text is set from the pref if we want it
		 * to colorize a stored expression.
		 */
		g_signal_connect(G_OBJECT(win->expression), "changed",
						 G_CALLBACK(regex_changed_cb), win);
		gtk_entry_set_text(GTK_ENTRY(win->expression),
						   purple_prefs_get_string(PIDGIN_PREFS_ROOT "/debug/regex"));
		g_signal_connect(G_OBJECT(win->expression), "populate-popup",
						 G_CALLBACK(regex_popup_cb), win);
		g_signal_connect(G_OBJECT(win->expression), "key-release-event",
						 G_CALLBACK(regex_key_release_cb), win);
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/regex",
									regex_pref_expression_cb, win);

		/* connect the rest of our pref callbacks */
		win->invert = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/invert");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/invert",
									regex_pref_invert_cb, win);

		win->highlight = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/highlight");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/highlight",
									regex_pref_highlight_cb, win);

		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		item = gtk_tool_item_new();
		gtk_container_add(GTK_CONTAINER(item), gtk_label_new(_("Level ")));
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		win->filterlevel = gtk_combo_box_text_new();
		item = gtk_tool_item_new();
		gtk_widget_set_tooltip_text(win->filterlevel, _("Select the debug filter level."));
		gtk_container_add(GTK_CONTAINER(item), win->filterlevel);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("All"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("Misc"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("Info"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("Warning"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("Error "));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(win->filterlevel), _("Fatal Error"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel),
					purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"));

		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/filterlevel",
						filter_level_pref_changed, win);
		g_signal_connect(G_OBJECT(win->filterlevel), "changed",
						 G_CALLBACK(filter_level_changed_cb), NULL);
	}

	/* Add the gtkwebview */
	frame = pidgin_create_webview(FALSE, &win->text, NULL, NULL);
	gtk_webview_set_format_functions(GTK_WEBVIEW(win->text),
	                                 GTK_WEBVIEW_ALL ^ GTK_WEBVIEW_SMILEY ^ GTK_WEBVIEW_IMAGE);
	gtk_webview_set_autoscroll(GTK_WEBVIEW(win->text), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	g_signal_connect(G_OBJECT(win->text), "script-alert",
		G_CALLBACK(debug_window_alert_cb), win);

	clear_cb(NULL, win);

	gtk_widget_show_all(win->window);

	return win;
}

static gboolean
debug_enabled_timeout_cb(gpointer data)
{
	debug_enabled_timer = 0;

	if (data)
		pidgin_debug_window_show();
	else
		pidgin_debug_window_hide();

	return FALSE;
}

static void
debug_enabled_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	debug_enabled_timer = g_timeout_add(0, debug_enabled_timeout_cb, GINT_TO_POINTER(GPOINTER_TO_INT(value)));
}

static void
pidgin_glib_log_handler(const gchar *domain, GLogLevelFlags flags,
					  const gchar *msg, gpointer user_data)
{
	PurpleDebugLevel level;
	char *new_msg = NULL;
	char *new_domain = NULL;

	if ((flags & G_LOG_LEVEL_ERROR) == G_LOG_LEVEL_ERROR)
		level = PURPLE_DEBUG_ERROR;
	else if ((flags & G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL)
		level = PURPLE_DEBUG_FATAL;
	else if ((flags & G_LOG_LEVEL_WARNING) == G_LOG_LEVEL_WARNING)
		level = PURPLE_DEBUG_WARNING;
	else if ((flags & G_LOG_LEVEL_MESSAGE) == G_LOG_LEVEL_MESSAGE)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_INFO) == G_LOG_LEVEL_INFO)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_DEBUG) == G_LOG_LEVEL_DEBUG)
		level = PURPLE_DEBUG_MISC;
	else
	{
		purple_debug_warning("gtkdebug",
				   "Unknown glib logging level in %d\n", flags);

		level = PURPLE_DEBUG_MISC; /* This will never happen. */
	}

	if (msg != NULL)
		new_msg = purple_utf8_try_convert(msg);

	if (domain != NULL)
		new_domain = purple_utf8_try_convert(domain);

	if (new_msg != NULL)
	{
		purple_debug(level, (new_domain != NULL ? new_domain : "g_log"),
				   "%s\n", new_msg);

		g_free(new_msg);
	}

	g_free(new_domain);
}

#ifdef _WIN32
static void
pidgin_glib_dummy_print_handler(const gchar *string)
{
}
#endif

void
pidgin_debug_init(void)
{
	/* Debug window preferences. */
	/*
	 * NOTE: This must be set before prefs are loaded, and the callbacks
	 *       set after they are loaded, since prefs sets the enabled
	 *       preference here and that loads the window, which calls the
	 *       configure event, which overrides the width and height! :P
	 */

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/debug");

	/* Controls printing to the debug window */
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/enabled", FALSE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/filterlevel", PURPLE_DEBUG_ALL);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/style", GTK_TOOLBAR_BOTH_HORIZ);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/toolbar", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/width",  450);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/height", 250);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/debug/regex", "");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/filter", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/invert", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/case_insensitive", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/highlight", FALSE);

	purple_prefs_connect_callback(NULL, PIDGIN_PREFS_ROOT "/debug/enabled",
								debug_enabled_cb, NULL);

#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  pidgin_glib_log_handler, NULL)

	/* Register the glib/gtk log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("Gdk");
	REGISTER_G_LOG_HANDLER("Gtk");
	REGISTER_G_LOG_HANDLER("GdkPixbuf");
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");
	REGISTER_G_LOG_HANDLER("Json");
#ifdef USE_GSTREAMER
	REGISTER_G_LOG_HANDLER("GStreamer");
#endif

#ifdef _WIN32
	if (!purple_debug_is_enabled())
		g_set_print_handler(pidgin_glib_dummy_print_handler);
#endif
}

void
pidgin_debug_uninit(void)
{
	purple_debug_set_ui_ops(NULL);

	if (debug_enabled_timer != 0)
		g_source_remove(debug_enabled_timer);
}

void
pidgin_debug_window_show(void)
{
	if (debug_win == NULL)
		debug_win = debug_window_new();

	gtk_widget_show(debug_win->window);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", TRUE);
}

void
pidgin_debug_window_hide(void)
{
	if (debug_win != NULL) {
		gtk_widget_destroy(debug_win->window);
		debug_window_destroy(NULL, NULL, NULL);
	}
}

static void
pidgin_debug_print(PurpleDebugLevel level, const char *category,
	const char *arg_s)
{
	gchar *esc_s;
	const char *mdate;
	time_t mtime;
	gchar *js;

	if (debug_win == NULL)
		return;
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"))
		return;

	mtime = time(NULL);
	mdate = purple_utf8_strftime("%H:%M:%S", localtime(&mtime));

	esc_s = purple_escape_js(arg_s);

	js = g_strdup_printf("append(%d, '%s', '%s', %s);",
		level, mdate, category ? category : "", esc_s);
	g_free(esc_s);

	gtk_webview_safe_execute_script(GTK_WEBVIEW(debug_win->text), js);
	g_free(js);
}

static gboolean
pidgin_debug_is_enabled(PurpleDebugLevel level, const char *category)
{
	return (debug_win != NULL &&
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"));
}

static PurpleDebugUiOps ops =
{
	pidgin_debug_print,
	pidgin_debug_is_enabled,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleDebugUiOps *
pidgin_debug_get_ui_ops(void)
{
	return &ops;
}

void *
pidgin_debug_get_handle() {
	static int handle;

	return &handle;
}

