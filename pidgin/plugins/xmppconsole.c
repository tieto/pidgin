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
#include "gtkutils.h"

typedef struct {
	PurpleConnection *gc;
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *dropdown;
	GtkWidget *imhtml;
	GtkWidget *entry;
	GtkWidget *sw;
	int count;
	GList *accounts;
} XmppConsole;

XmppConsole *console = NULL;
static void *xmpp_console_handle = NULL;

#define BRACKET_COLOR "#940f8c"
#define TAG_COLOR "#8b1dab"
#define ATTR_NAME_COLOR "#a02961"
#define ATTR_VALUE_COLOR "#324aa4"
#define XMLNS_COLOR "#2cb12f"

static char *
xmlnode_to_pretty_str(xmlnode *node, int *len, int depth)
{
	GString *text = g_string_new("");
	xmlnode *c;
	char *node_name, *esc, *esc2, *tab = NULL;
	gboolean need_end = FALSE, pretty = TRUE;

	g_return_val_if_fail(node != NULL, NULL);

	if (pretty && depth) {
		tab = g_strnfill(depth, '\t');
		text = g_string_append(text, tab);
	}

	node_name = g_markup_escape_text(node->name, -1);
	g_string_append_printf(text,
	                       "<font color='" BRACKET_COLOR "'>&lt;</font>"
	                       "<font color='" TAG_COLOR "'><b>%s</b></font>",
	                       node_name);

	if (node->xmlns) {
		if ((!node->parent ||
		     !node->parent->xmlns ||
		     strcmp(node->xmlns, node->parent->xmlns)) &&
		    strcmp(node->xmlns, "jabber:client"))
		{
			char *xmlns = g_markup_escape_text(node->xmlns, -1);
			g_string_append_printf(text,
			                       " <font color='" ATTR_NAME_COLOR "'><b>xmlns</b></font>="
			                       "'<font color='" XMLNS_COLOR "'><b>%s</b></font>'",
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
			                       " <font color='" ATTR_NAME_COLOR "'><b>%s</b></font>="
			                       "'<font color='" ATTR_VALUE_COLOR "'>%s</font>'",
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
		                       "<font color='"BRACKET_COLOR"'>&gt;</font>%s",
		                       pretty ? "<br>" : "");

		for (c = node->child; c; c = c->next)
		{
			if (c->type == XMLNODE_TYPE_TAG) {
				int esc_len;
				esc = xmlnode_to_pretty_str(c, &esc_len, depth+1);
				text = g_string_append_len(text, esc, esc_len);
				g_free(esc);
			} else if (c->type == XMLNODE_TYPE_DATA && c->data_sz > 0) {
				esc = g_markup_escape_text(c->data, c->data_sz);
				text = g_string_append(text, esc);
				g_free(esc);
			}
		}

		if(tab && pretty)
			text = g_string_append(text, tab);
		g_string_append_printf(text,
		                       "<font color='" BRACKET_COLOR "'>&lt;</font>/"
		                       "<font color='" TAG_COLOR "'><b>%s</b></font>"
		                       "<font color='" BRACKET_COLOR "'>&gt;</font><br>",
		                       node_name);
	} else {
		g_string_append_printf(text,
		                       "/<font color='" BRACKET_COLOR "'>&gt;</font><br>");
	}

	g_free(node_name);

	g_free(tab);

	if(len)
		*len = text->len;

	return g_string_free(text, FALSE);
}

static void
xmlnode_received_cb(PurpleConnection *gc, xmlnode **packet, gpointer null)
{
	char *str, *formatted;

	if (!console || console->gc != gc)
		return;
	str = xmlnode_to_pretty_str(*packet, NULL, 0);
	formatted = g_strdup_printf("<body bgcolor='#ffcece'><pre>%s</pre></body>", str);
	gtk_imhtml_append_text(GTK_IMHTML(console->imhtml), formatted, 0);
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

	str = xmlnode_to_pretty_str(node, NULL, 0);
	formatted = g_strdup_printf("<body bgcolor='#dcecc4'><pre>%s</pre></body>", str);
	gtk_imhtml_append_text(GTK_IMHTML(console->imhtml), formatted, 0);
	g_free(formatted);
	g_free(str);
	xmlnode_free(node);
}

static void message_send_cb(GtkWidget *widget, gpointer p)
{
	GtkTextIter start, end;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;
	GtkTextBuffer *buffer;
	char *text;

	gc = console->gc;

	if (gc)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->entry));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);

	text = gtk_imhtml_get_text(GTK_IMHTML(console->entry), &start, &end);

	if (prpl_info && prpl_info->send_raw != NULL)
		prpl_info->send_raw(gc, text, strlen(text));

	g_free(text);
	gtk_imhtml_clear(GTK_IMHTML(console->entry));
}

static void entry_changed_cb(GtkTextBuffer *buffer, void *data)
{
	char *xmlstr, *str;
	GtkTextIter iter;
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;
	GtkTextIter start, end;
	xmlnode *node;

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

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
       	str = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (!str)
		return;
	xmlstr = g_strdup_printf("<xml>%s</xml>", str);
	node = xmlnode_from_str(xmlstr, -1);
	if (node) {
		gtk_imhtml_clear_formatting(GTK_IMHTML(console->entry));
	} else {
		gtk_imhtml_toggle_background(GTK_IMHTML(console->entry), "#ffcece");
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
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	const char *to;
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
	/* TODO: how to set no separator for GtkDialog in gtk+ 3.0... */
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
#if GTK_CHECK_VERSION(2,14,0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif

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

	to = gtk_entry_get_text(GTK_ENTRY(to_entry));

	stanza = g_strdup_printf("<iq %s%s%s id='console%x' type='%s'></iq>",
				 to && *to ? "to='" : "",
				 to && *to ? to : "",
				 to && *to ? "'" : "",
				 g_random_int(),
				 gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type_combo)));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->entry));
	gtk_text_buffer_set_text(buffer, stanza, -1);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, strstr(stanza, "</iq>") - stanza);
	gtk_text_buffer_place_cursor(buffer, &iter);
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
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	const char *to, *type, *status, *show, *priority;
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

  /* TODO: find a way to specify no separator for a dialog in gtk+ 3 */
  /*gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);*/
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
#if GTK_CHECK_VERSION(2,14,0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif

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

	to = gtk_entry_get_text(GTK_ENTRY(to_entry));
	type = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type_combo));
	if (!strcmp(type, "default"))
		type = "";
	show = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(show_combo));
	if (!strcmp(show, "default"))
		show = "";
	status = gtk_entry_get_text(GTK_ENTRY(status_entry));
	priority = gtk_entry_get_text(GTK_ENTRY(priority_entry));
	if (!strcmp(priority, "0"))
		priority = "";

	stanza = g_strdup_printf("<presence %s%s%s id='console%x' %s%s%s>"
	                         "%s%s%s%s%s%s%s%s%s"
	                         "</presence>",
	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),

	                         *type ? "type='" : "",
	                         *type ? type : "",
	                         *type ? "'" : "",

	                         *show ? "<show>" : "",
	                         *show ? show : "",
	                         *show ? "</show>" : "",

	                         *status ? "<status>" : "",
	                         *status ? status : "",
	                         *status ? "</status>" : "",

	                         *priority ? "<priority>" : "",
	                         *priority ? priority : "",
	                         *priority ? "</priority>" : "");

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->entry));
	gtk_text_buffer_set_text(buffer, stanza, -1);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, strstr(stanza, "</presence>") - stanza);
	gtk_text_buffer_place_cursor(buffer, &iter);
	g_free(stanza);

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
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	const char *to, *body, *thread, *subject;
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
  /* TODO: find a way to create a dialog without separtor in gtk+ 3 */
	/*gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);*/
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
#if GTK_CHECK_VERSION(2,14,0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif

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

	to = gtk_entry_get_text(GTK_ENTRY(to_entry));
	body = gtk_entry_get_text(GTK_ENTRY(body_entry));
	thread = gtk_entry_get_text(GTK_ENTRY(thread_entry));
	subject = gtk_entry_get_text(GTK_ENTRY(subject_entry));

	stanza = g_strdup_printf("<message %s%s%s id='console%x' type='%s'>"
	                         "%s%s%s%s%s%s%s%s%s"
	                         "</message>",

	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),
	                         gtk_combo_box_text_get_active_text(
                               GTK_COMBO_BOX_TEXT(type_combo)),

	                         *body ? "<body>" : "",
	                         *body ? body : "",
	                         *body ? "</body>" : "",

	                         *subject ? "<subject>" : "",
	                         *subject ? subject : "",
	                         *subject ? "</subject>" : "",

	                         *thread ? "<thread>" : "",
	                         *thread ? thread : "",
	                         *thread ? "</thread>" : "");

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->entry));
	gtk_text_buffer_set_text(buffer, stanza, -1);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, strstr(stanza, "</message>") - stanza);
	gtk_text_buffer_place_cursor(buffer, &iter);
	g_free(stanza);

	gtk_widget_destroy(dialog);
	g_object_unref(sg);
}

static void
signing_on_cb(PurpleConnection *gc)
{
	if (!console)
		return;

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(console->dropdown),
      purple_account_get_username(gc->account));
	console->accounts = g_list_append(console->accounts, gc);
	console->count++;

	if (console->count == 1)
		console->gc = gc;
	else
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
		console->gc = NULL;
		gtk_imhtml_append_text(GTK_IMHTML(console->imhtml),
				       _("<font color='#777777'>Logged out.</font>"), 0);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurplePlugin *jabber;

	jabber = purple_find_prpl("prpl-jabber");
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
	PurpleAccount *account;

	if (!console)
		return;

	account =
      purple_accounts_find(gtk_combo_box_text_get_active_text(
          GTK_COMBO_BOX_TEXT(console->dropdown)), "prpl-jabber");
	if (!account || !account->gc)
		return;

	console->gc = account->gc;
	gtk_imhtml_clear(GTK_IMHTML(console->imhtml));
}

static void
create_console(PurplePluginAction *action)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget *label;
	GtkTextBuffer *buffer;
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
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->dropdown),0);
	gtk_box_pack_start(GTK_BOX(console->hbox), console->dropdown, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(console->dropdown), "changed", G_CALLBACK(dropdown_changed_cb), NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	console->imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	if (console->count == 0)
		gtk_imhtml_append_text(GTK_IMHTML(console->imhtml),
				       _("<font color='#777777'>Not connected to XMPP</font>"), 0);
	gtk_container_add(GTK_CONTAINER(sw), console->imhtml);

	toolbar = gtk_toolbar_new();
	button = gtk_tool_button_new(NULL, "<iq/>");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(iq_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_tool_button_new(NULL, "<presence/>");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(presence_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_tool_button_new(NULL, "<message/>");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(message_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	console->entry = gtk_imhtml_new(NULL, NULL);
	gtk_imhtml_set_whole_buffer_formatting_only(GTK_IMHTML(console->entry), TRUE);
	g_signal_connect(G_OBJECT(console->entry),"message_send", G_CALLBACK(message_send_cb), console);

	gtk_box_pack_start(GTK_BOX(vbox), sw, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(sw), console->entry);
	gtk_imhtml_set_editable(GTK_IMHTML(console->entry), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(console->entry));
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(entry_changed_cb), NULL);
	console->sw = sw;
	entry_changed_cb(buffer, NULL);

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
