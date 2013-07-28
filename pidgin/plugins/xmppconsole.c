/*
 * Purple - XMPP debugging tool
 *
 * Copyright (C) 2002-2003, Sean Egan
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
#include "gtkplugin.h"
#include "version.h"
#include "prpl.h"
#include "xmlnode.h"

#include "gtkimhtml.h"
#include "gtkwebview.h"
#include "gtkutils.h"

#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

typedef struct {
	PurpleConnection *gc;
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *dropdown;
	GtkWidget *webview;
	GtkWidget *entry;
	GtkWidget *sw;
	int count;
	GList *accounts;
} XmppConsole;

XmppConsole *console = NULL;
static void *xmpp_console_handle = NULL;

#define EMPTY_HTML \
"<html><head><style type='text/css'>" \
	"body { word-wrap: break-word; margin: 0; }" \
	"div.tab { padding-left: 1em; }" \
	"div.info { color: #777777; }" \
	"div.incoming { background-color: #ffcece; }" \
	"div.outgoing { background-color: #dcecc4; }" \
	"span.bracket { color: #940f8c; }" \
	"span.tag { color: #8b1dab; font-weight: bold; }" \
	"span.attr { color: #a02961; font-weight: bold; }" \
	"span.value { color: #324aa4; }" \
	"span.xmlns { color: #2cb12f; font-weight: bold;}" \
"</style></head></html>"

static char *
xmlnode_to_pretty_str(xmlnode *node, int *len)
{
	GString *text = g_string_new("");
	xmlnode *c;
	char *node_name, *esc, *esc2;
	gboolean need_end = FALSE, pretty = TRUE;

	g_return_val_if_fail(node != NULL, NULL);

	node_name = g_markup_escape_text(node->name, -1);
	g_string_append_printf(text,
	                       "<span class=bracket>&lt;</span>"
	                       "<span class=tag>%s</span>",
	                       node_name);

	if (node->xmlns) {
		if ((!node->parent ||
		     !node->parent->xmlns ||
		     strcmp(node->xmlns, node->parent->xmlns)) &&
		    strcmp(node->xmlns, "jabber:client"))
		{
			char *xmlns = g_markup_escape_text(node->xmlns, -1);
			g_string_append_printf(text,
			                       " <span class=attr>xmlns</span>="
			                       "'<span class=xmlns>%s</span>'",
			                       xmlns);
			g_free(xmlns);
		}
	}
	for (c = node->child; c; c = c->next)
	{
		if (c->type == XMLNODE_TYPE_ATTRIB) {
			esc = g_markup_escape_text(c->name, -1);
			esc2 = g_markup_escape_text(c->data, -1);
			g_string_append_printf(text,
			                       " <span class=attr>%s</span>="
			                       "'<span class=value>%s</span>'",
			                       esc, esc2);
			g_free(esc);
			g_free(esc2);
		} else if (c->type == XMLNODE_TYPE_TAG || c->type == XMLNODE_TYPE_DATA) {
			if (c->type == XMLNODE_TYPE_DATA)
				pretty = FALSE;
			need_end = TRUE;
		}
	}

	if (need_end) {
		g_string_append_printf(text,
		                       "<span class=bracket>&gt;</span>%s",
		                       pretty ? "<br>" : "");

		need_end = FALSE;
		for (c = node->child; c; c = c->next)
		{
			if (c->type == XMLNODE_TYPE_TAG) {
				int esc_len;
				esc = xmlnode_to_pretty_str(c, &esc_len);
				if (!need_end) {
					g_string_append(text, "<div class=tab>");
					need_end = TRUE;
				}
				text = g_string_append_len(text, esc, esc_len);
				g_free(esc);
			} else if (c->type == XMLNODE_TYPE_DATA && c->data_sz > 0) {
				esc = g_markup_escape_text(c->data, c->data_sz);
				text = g_string_append(text, esc);
				g_free(esc);
			}
		}

		if (need_end)
			g_string_append(text, "</div>");

		g_string_append_printf(text,
		                       "<span class=bracket>&lt;</span>/"
		                       "<span class=tag>%s</span>"
		                       "<span class=bracket>&gt;</span><br>",
		                       node_name);
	} else {
		g_string_append_printf(text,
		                       "/<span class=bracket>&gt;</span><br>");
	}

	g_free(node_name);

	if (len)
		*len = text->len;

	return g_string_free(text, FALSE);
}

static void
xmlnode_received_cb(PurpleConnection *gc, xmlnode **packet, gpointer null)
{
	char *str, *formatted;

	if (!console || console->gc != gc)
		return;
	str = xmlnode_to_pretty_str(*packet, NULL);
	formatted = g_strdup_printf("<div class=incoming>%s</div>", str);
	gtk_webview_append_html(GTK_WEBVIEW(console->webview), formatted);
	g_free(formatted);
	g_free(str);
}

static void
xmlnode_sent_cb(PurpleConnection *gc, char **packet, gpointer null)
{
	char *str;
	char *formatted;
	xmlnode *node;

	if (!console || console->gc != gc)
		return;
	node = xmlnode_from_str(*packet, -1);

	if (!node)
		return;

	str = xmlnode_to_pretty_str(node, NULL);
	formatted = g_strdup_printf("<div class=outgoing>%s</div>", str);
	gtk_webview_append_html(GTK_WEBVIEW(console->webview), formatted);
	g_free(formatted);
	g_free(str);
	xmlnode_free(node);
}

static gboolean
message_send_cb(GtkWidget *widget, GdkEventKey *event, gpointer p)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;
	gchar *text;

	if (event->keyval != GDK_KEY_KP_Enter && event->keyval != GDK_KEY_Return)
		return FALSE;

	gc = console->gc;

	if (gc)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_protocol_info(gc));

	text = gtk_webview_get_body_text(GTK_WEBVIEW(widget));

	if (prpl_info && prpl_info->send_raw != NULL)
		prpl_info->send_raw(gc, text, strlen(text));

	g_free(text);
	gtk_webview_load_html_string(GTK_WEBVIEW(console->entry), "");

	return TRUE;
}

static void
entry_changed_cb(GtkWidget *webview, void *data)
{
	char *xmlstr, *str;
#if 0
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;
#endif
	xmlnode *node;

#if 0
	/* TODO WebKit: Do entry auto-sizing... */
	wrapped_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(console->entry), &iter, &oneline);
	while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(console->entry), &iter))
		wrapped_lines++;

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 64 lines */
	lines = MIN(lines, 6);
	wrapped_lines = MIN(wrapped_lines, 6);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(console->entry));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(console->entry));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(console->entry));

	height = (oneline.height + pad_top + pad_bottom) * lines;
	height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_set_size_request(console->sw, -1, height + 6);
#endif

	str = gtk_webview_get_body_text(GTK_WEBVIEW(webview));
	if (!str)
		return;
	xmlstr = g_strdup_printf("<xml>%s</xml>", str);
	node = xmlnode_from_str(xmlstr, -1);
	if (node) {
		gtk_webview_clear_formatting(GTK_WEBVIEW(console->entry));
	} else {
		gtk_webview_toggle_backcolor(GTK_WEBVIEW(console->entry), "#ffcece");
	}
	g_free(str);
	g_free(xmlstr);
	if (node)
		xmlnode_free(node);
}

static void iq_clicked_cb(GtkWidget *w, gpointer nul)
{
	GtkWidget *vbox, *hbox, *to_entry, *label, *type_combo;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	char *to;
	int result;
	char *stanza;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("<iq/>",
							GTK_WINDOW(console->window),
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
#if !GTK_CHECK_VERSION(2,22,0)
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
#endif
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("To:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	to_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (to_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), to_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	type_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "get");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "set");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "result");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "error");
	gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
	gtk_box_pack_start(GTK_BOX(hbox), type_combo, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return;
	}

	to = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(to_entry)), -1);

	stanza = g_strdup_printf("&lt;iq %s%s%s id='console%x' type='%s'&gt;"
	                         "<a id=caret></a>"
	                         "&lt;/iq&gt;",
				 to && *to ? "to='" : "",
				 to && *to ? to : "",
				 to && *to ? "'" : "",
				 g_random_int(),
				 gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type_combo)));

	gtk_webview_load_html_string_with_selection(GTK_WEBVIEW(console->entry), stanza);
	gtk_widget_grab_focus(console->entry);
	g_free(to);
	g_free(stanza);

	gtk_widget_destroy(dialog);
	g_object_unref(sg);
}

static void presence_clicked_cb(GtkWidget *w, gpointer nul)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *to_entry;
	GtkWidget *status_entry;
	GtkWidget *priority_entry;
	GtkWidget *label;
	GtkWidget *show_combo;
	GtkWidget *type_combo;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	char *to, *status, *priority;
	const char *type, *show;
	int result;
	char *stanza;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("<presence/>",
							GTK_WINDOW(console->window),
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
#if !GTK_CHECK_VERSION(2,22,0)
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
#endif
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("To:");
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	to_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (to_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), to_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	type_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "default");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "unavailable");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "subscribe");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "unsubscribe");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "subscribed");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "unsubscribed");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "probe");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "error");
	gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
	gtk_box_pack_start(GTK_BOX(hbox), type_combo, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("Show:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	show_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(show_combo), "default");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(show_combo), "away");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(show_combo), "dnd");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(show_combo), "xa");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(show_combo), "chat");

	gtk_combo_box_set_active(GTK_COMBO_BOX(show_combo), 0);
	gtk_box_pack_start(GTK_BOX(hbox), show_combo, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Status:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	status_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (status_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), status_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Priority:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	priority_entry = gtk_spin_button_new_with_range(-128, 127, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(priority_entry), 0);
	gtk_box_pack_start(GTK_BOX(hbox), priority_entry, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return;
	}

	to = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(to_entry)), -1);
	type = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type_combo));
	if (!strcmp(type, "default"))
		type = "";
	show = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(show_combo));
	if (!strcmp(show, "default"))
		show = "";
	status = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(status_entry)), -1);
	priority = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(priority_entry)), -1);
	if (!strcmp(priority, "0"))
		*priority = '\0';

	stanza = g_strdup_printf("&lt;presence %s%s%s id='console%x' %s%s%s&gt;"
	                         "%s%s%s%s%s%s%s%s%s"
	                         "<a id=caret></a>"
	                         "&lt;/presence&gt;",
	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),

	                         *type ? "type='" : "",
	                         *type ? type : "",
	                         *type ? "'" : "",

	                         *show ? "&lt;show&gt;" : "",
	                         *show ? show : "",
	                         *show ? "&lt;/show&gt;" : "",

	                         *status ? "&lt;status&gt;" : "",
	                         *status ? status : "",
	                         *status ? "&lt;/status&gt;" : "",

	                         *priority ? "&lt;priority&gt;" : "",
	                         *priority ? priority : "",
	                         *priority ? "&lt;/priority&gt;" : "");

	gtk_webview_load_html_string_with_selection(GTK_WEBVIEW(console->entry), stanza);
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);
	g_free(to);
	g_free(status);
	g_free(priority);

	gtk_widget_destroy(dialog);
	g_object_unref(sg);
}

static void message_clicked_cb(GtkWidget *w, gpointer nul)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *to_entry;
	GtkWidget *body_entry;
	GtkWidget *thread_entry;
	GtkWidget *subject_entry;
	GtkWidget *label;
	GtkWidget *type_combo;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	char *to, *body, *thread, *subject;
	char *stanza;
	int result;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("<message/>",
							GTK_WINDOW(console->window),
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
#if !GTK_CHECK_VERSION(2,22,0)
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
#endif
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("To:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	to_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (to_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), to_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	type_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "chat");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "headline");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "groupchat");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "normal");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "error");
	gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
	gtk_box_pack_start(GTK_BOX(hbox), type_combo, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Body:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	body_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (body_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), body_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Subject:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	subject_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (subject_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), subject_entry, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Thread:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(sg, label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	thread_entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (thread_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), thread_entry, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return;
	}

	to = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(to_entry)), -1);
	body = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(body_entry)), -1);
	thread = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(thread_entry)), -1);
	subject = g_markup_escape_text(gtk_entry_get_text(GTK_ENTRY(subject_entry)), -1);

	stanza = g_strdup_printf("&lt;message %s%s%s id='console%x' type='%s'&gt;"
	                         "%s%s%s%s%s%s%s%s%s"
	                         "<a id=caret></a>"
	                         "&lt;/message&gt;",

	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),
	                         gtk_combo_box_text_get_active_text(
                               GTK_COMBO_BOX_TEXT(type_combo)),

	                         *body ? "&lt;body&gt;" : "",
	                         *body ? body : "",
	                         *body ? "&lt;/body&gt;" : "",

	                         *subject ? "&lt;subject&gt;" : "",
	                         *subject ? subject : "",
	                         *subject ? "&lt;/subject&gt;" : "",

	                         *thread ? "&lt;thread&gt;" : "",
	                         *thread ? thread : "",
	                         *thread ? "&lt;/thread&gt;" : "");

	gtk_webview_load_html_string_with_selection(GTK_WEBVIEW(console->entry), stanza);
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);
	g_free(to);
	g_free(body);
	g_free(thread);
	g_free(subject);

	gtk_widget_destroy(dialog);
	g_object_unref(sg);
}

static void
signing_on_cb(PurpleConnection *gc)
{
	PurpleAccount *account;

	if (!console)
		return;

	account = purple_connection_get_account(gc);
	if (strcmp(purple_account_get_protocol_id(account), "prpl-jabber"))
		return;

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(console->dropdown),
		purple_account_get_username(account));
	console->accounts = g_list_append(console->accounts, gc);
	console->count++;

	if (console->count == 1) {
		console->gc = gc;
		gtk_webview_load_html_string(GTK_WEBVIEW(console->webview), EMPTY_HTML);
		gtk_combo_box_set_active(GTK_COMBO_BOX(console->dropdown), 0);
	} else
		gtk_widget_show_all(console->hbox);
}

static void
signed_off_cb(PurpleConnection *gc)
{
	int i = 0;
	GList *l;

	if (!console)
		return;

	l = console->accounts;
	while (l) {
		PurpleConnection *g = l->data;
		if (gc == g)
			break;
		i++;
		l = l->next;
	}

	if (l == NULL)
		return;

	gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(console->dropdown), i);
	console->accounts = g_list_remove(console->accounts, gc);
	console->count--;

	if (gc == console->gc) {
		char *tmp = g_strdup_printf("<div class=info>%s</div>",
		                            _("Logged out."));
		gtk_webview_append_html(GTK_WEBVIEW(console->webview), tmp);
		g_free(tmp);
		console->gc = NULL;
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurplePlugin *jabber;

	jabber = purple_find_protocol_info("prpl-jabber");
	if (!jabber)
		return FALSE;

	xmpp_console_handle = plugin;
	purple_signal_connect(jabber, "jabber-receiving-xmlnode", xmpp_console_handle,
			    PURPLE_CALLBACK(xmlnode_received_cb), NULL);
	purple_signal_connect(jabber, "jabber-sending-text", xmpp_console_handle,
			    PURPLE_CALLBACK(xmlnode_sent_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-on",
			    plugin, PURPLE_CALLBACK(signing_on_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off",
			    plugin, PURPLE_CALLBACK(signed_off_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	if (console)
		gtk_widget_destroy(console->window);
	return TRUE;
}

static void
console_destroy(GtkWidget *window, gpointer nul)
{
	g_list_free(console->accounts);
	g_free(console);
	console = NULL;
}

static void
dropdown_changed_cb(GtkComboBox *widget, gpointer nul)
{
	if (!console)
		return;

	console->gc = g_list_nth_data(console->accounts, gtk_combo_box_get_active(GTK_COMBO_BOX(console->dropdown)));
	gtk_webview_load_html_string(GTK_WEBVIEW(console->webview), EMPTY_HTML);
}

static void
create_console(PurplePluginAction *action)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
	GtkWidget *label;
	GtkWidget *toolbar;
	GList *connections;
	GtkToolItem *button;

	if (console) {
		gtk_window_present(GTK_WINDOW(console->window));
		return;
	}

	console = g_new0(XmppConsole, 1);

	console->window = pidgin_create_window(_("XMPP Console"), PIDGIN_HIG_BORDER, NULL, TRUE);
	g_signal_connect(G_OBJECT(console->window), "destroy", G_CALLBACK(console_destroy), NULL);
	gtk_window_set_default_size(GTK_WINDOW(console->window), 580, 400);
	gtk_container_add(GTK_CONTAINER(console->window), vbox);

	console->hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), console->hbox, FALSE, FALSE, 0);
	label = gtk_label_new(_("Account: "));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(console->hbox), label, FALSE, FALSE, 0);
	console->dropdown = gtk_combo_box_text_new();
	for (connections = purple_connections_get_all(); connections; connections = connections->next) {
		PurpleConnection *gc = connections->data;
		if (!strcmp(purple_account_get_protocol_id(purple_connection_get_account(gc)), "prpl-jabber")) {
			console->count++;
			console->accounts = g_list_append(console->accounts, gc);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(console->dropdown),
						  purple_account_get_username(purple_connection_get_account(gc)));
			if (!console->gc)
				console->gc = gc;
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->dropdown), 0);
	gtk_box_pack_start(GTK_BOX(console->hbox), console->dropdown, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(console->dropdown), "changed", G_CALLBACK(dropdown_changed_cb), NULL);

	console->webview = gtk_webview_new();
	gtk_webview_load_html_string(GTK_WEBVIEW(console->webview), EMPTY_HTML);
	if (console->count == 0) {
		char *tmp = g_strdup_printf("<div class=info>%s</div>",
		                            _("Not connected to XMPP"));
		gtk_webview_append_html(GTK_WEBVIEW(console->webview), tmp);
		g_free(tmp);
	}
	gtk_box_pack_start(GTK_BOX(vbox), 
		pidgin_make_scrollable(console->webview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_ETCHED_IN, -1, -1),
		TRUE, TRUE, 0);

	toolbar = gtk_toolbar_new();
	button = gtk_tool_button_new(NULL, "<iq/>");
	gtk_tool_item_set_is_important(button, TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(iq_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_tool_button_new(NULL, "<presence/>");
	gtk_tool_item_set_is_important(button, TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(presence_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_tool_button_new(NULL, "<message/>");
	gtk_tool_item_set_is_important(button, TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(message_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	console->entry = gtk_webview_new();
	gtk_webview_set_whole_buffer_formatting_only(GTK_WEBVIEW(console->entry), TRUE);
	g_signal_connect(G_OBJECT(console->entry),"key-press-event", G_CALLBACK(message_send_cb), console);

	console->sw = pidgin_make_scrollable(console->entry, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_ETCHED_IN, -1, -1);
	gtk_box_pack_start(GTK_BOX(vbox), console->sw, FALSE, FALSE, 0);
	gtk_webview_set_editable(GTK_WEBVIEW(console->entry), TRUE);
	g_signal_connect(G_OBJECT(console->entry), "changed", G_CALLBACK(entry_changed_cb), NULL);

	entry_changed_cb(console->entry, NULL);

	gtk_widget_show_all(console->window);
	if (console->count < 2)
		gtk_widget_hide(console->hbox);
}

static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("XMPP Console"), create_console);
	l = g_list_append(l, act);

	return l;
}


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                       /**< type           */
	PIDGIN_PLUGIN_TYPE,                           /**< ui_requirement */
	0,                                            /**< flags          */
	NULL,                                         /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                      /**< priority       */

	"gtk-xmpp",                                   /**< id             */
	N_("XMPP Console"),                           /**< name           */
	DISPLAY_VERSION,                              /**< version        */
	                                              /**  summary        */
	N_("Send and receive raw XMPP stanzas."),
	                                              /**  description    */
	N_("This plugin is useful for debugging XMPP servers or clients."),
	"Sean Egan <seanegan@gmail.com>",             /**< author         */
	PURPLE_WEBSITE,                               /**< homepage       */

	plugin_load,                                  /**< load           */
	plugin_unload,                                /**< unload         */
	NULL,                                         /**< destroy        */

	NULL,                                         /**< ui_info        */
	NULL,                                         /**< extra_info     */
	NULL,
	actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(xmppconsole, init_plugin, info)
