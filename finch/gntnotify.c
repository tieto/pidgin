/**
 * @file gntnotify.c GNT Notify API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <internal.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntlabel.h>
#include <gnttree.h>
#include <gntutils.h>
#include <gntwindow.h>

#include "finch.h"

#include <util.h>

#include "gntnotify.h"
#include "debug.h"

static struct
{
	GntWidget *window;
	GntWidget *tree;
} emaildialog;

static void
notify_msg_window_destroy_cb(GntWidget *window, PurpleNotifyMsgType type)
{
	purple_notify_close(type, window);
}

static void *
finch_notify_message(PurpleNotifyMsgType type, const char *title,
		const char *primary, const char *secondary)
{
	GntWidget *window, *button;
	GntTextFormatFlags pf = 0, sf = 0;

	switch (type)
	{
		case PURPLE_NOTIFY_MSG_ERROR:
			sf |= GNT_TEXT_FLAG_BOLD;
		case PURPLE_NOTIFY_MSG_WARNING:
			pf |= GNT_TEXT_FLAG_UNDERLINE;
		case PURPLE_NOTIFY_MSG_INFO:
			pf |= GNT_TEXT_FLAG_BOLD;
			break;
	}

	window = gnt_window_box_new(FALSE, TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_fill(GNT_BOX(window), FALSE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(window), 0);

	if (primary)
		gnt_box_add_widget(GNT_BOX(window),
				gnt_label_new_with_format(primary, pf));

	button = gnt_button_new(_("OK"));

	if (secondary) {
		GntWidget *msg;
		/* XXX: This is broken.  type is PurpleNotifyMsgType, not
		 * PurpleNotifyType.  Also, the if() followed by the
		 * inner switch doesn't make much sense.
		 */
		if (type == PURPLE_NOTIFY_FORMATTED) {
			int width = -1, height = -1;
			char *plain = (char*)secondary;
			msg = gnt_text_view_new();
			gnt_text_view_set_flag(GNT_TEXT_VIEW(msg), GNT_TEXT_VIEW_TOP_ALIGN | GNT_TEXT_VIEW_NO_SCROLL);
			switch (type) {
				case PURPLE_NOTIFY_FORMATTED:
					plain = purple_markup_strip_html(secondary);
					if (gnt_util_parse_xhtml_to_textview(secondary, GNT_TEXT_VIEW(msg)))
						break;
					/* Fallthrough */
				default:
					gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(msg), plain, sf);
			}
			gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(msg), button);
			gnt_util_get_text_bound(plain, &width, &height);
			gnt_widget_set_size(msg, width + 3, height + 1);
			if (plain != secondary)
				g_free(plain);
		} else {
			msg = gnt_label_new_with_format(secondary, sf);
		}
		gnt_box_add_widget(GNT_BOX(window), msg);
		g_object_set_data(G_OBJECT(window), "info-widget", msg);
	}

	gnt_box_add_widget(GNT_BOX(window), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate",
			G_CALLBACK(gnt_widget_destroy), window);
	g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(notify_msg_window_destroy_cb), GINT_TO_POINTER(type));

	gnt_widget_show(window);
	return window;
}

/* handle is, in all/most occasions, a GntWidget * */
static void finch_close_notify(PurpleNotifyType type, void *handle)
{
	GntWidget *widget = handle;

	if (!widget)
		return;

	while (widget->parent)
		widget = widget->parent;

	if (type == PURPLE_NOTIFY_SEARCHRESULTS)
		purple_notify_searchresults_free(g_object_get_data(handle, "notify-results"));
#if 1
	/* This did not seem to be necessary */
	g_signal_handlers_disconnect_by_func(G_OBJECT(widget),
			G_CALLBACK(notify_msg_window_destroy_cb), GINT_TO_POINTER(type));
#endif
	gnt_widget_destroy(widget);
}

static void *finch_notify_formatted(const char *title, const char *primary,
		const char *secondary, const char *text)
{
	char *xhtml = NULL;
	char *t = g_strdup_printf("<span>%s%s%s</span>",
			secondary ? secondary : "",
			secondary ? "\n" : "",
			text ? text : "");
	void *ret;

	purple_markup_html_to_xhtml(t, &xhtml, NULL);
	ret = finch_notify_message(PURPLE_NOTIFY_FORMATTED, title, primary, xhtml);

	g_free(t);
	g_free(xhtml);

	return ret;
}

static void
reset_email_dialog(void)
{
	emaildialog.window = NULL;
	emaildialog.tree = NULL;
}

static void
setup_email_dialog(void)
{
	GntWidget *box, *tree, *button;
	if (emaildialog.window)
		return;

	emaildialog.window = box = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(box), TRUE);
	gnt_box_set_title(GNT_BOX(box), _("Emails"));
	gnt_box_set_fill(GNT_BOX(box), FALSE);
	gnt_box_set_alignment(GNT_BOX(box), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(box), 0);

	gnt_box_add_widget(GNT_BOX(box),
			gnt_label_new_with_format(_("You have mail!"), GNT_TEXT_FLAG_BOLD));

	emaildialog.tree = tree = gnt_tree_new_with_columns(3);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Account"), _("Sender"), _("Subject"));
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 15);
	gnt_tree_set_col_width(GNT_TREE(tree), 1, 25);
	gnt_tree_set_col_width(GNT_TREE(tree), 2, 25);

	gnt_box_add_widget(GNT_BOX(box), tree);

	button = gnt_button_new(_("Close"));
	gnt_box_add_widget(GNT_BOX(box), button);

	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), box);
	g_signal_connect(G_OBJECT(box), "destroy", G_CALLBACK(reset_email_dialog), NULL);
}

static void *
finch_notify_emails(PurpleConnection *gc, size_t count, gboolean detailed,
		const char **subjects, const char **froms, const char **tos,
		const char **urls)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GString *message = g_string_new(NULL);
	void *ret;
	static int key = 0;

	if (count == 0)
		return NULL;

	if (!detailed)
	{
		g_string_append_printf(message,
				ngettext("%s (%s) has %d new message.",
					     "%s (%s) has %d new messages.",
						 (int)count),
				tos ? *tos : purple_account_get_username(account),
				purple_account_get_protocol_name(account), (int)count);
	}
	else
	{
		char *to;
		gboolean newwin = (emaildialog.window == NULL);

		if (newwin)
			setup_email_dialog();

		to = g_strdup_printf("%s (%s)", tos ? *tos : purple_account_get_username(account),
					purple_account_get_protocol_name(account));
		gnt_tree_add_row_after(GNT_TREE(emaildialog.tree), GINT_TO_POINTER(++key),
				gnt_tree_create_row(GNT_TREE(emaildialog.tree), to,
					froms ? *froms : "[Unknown sender]",
					*subjects),
				NULL, NULL);
		g_free(to);
		if (newwin)
			gnt_widget_show(emaildialog.window);
		else
			gnt_window_present(emaildialog.window);
		return NULL;
	}

	ret = finch_notify_message(PURPLE_NOTIFY_EMAIL, _("New Mail"), _("You have mail!"), message->str);
	g_string_free(message, TRUE);
	return ret;
}

static void *
finch_notify_email(PurpleConnection *gc, const char *subject, const char *from,
		const char *to, const char *url)
{
	return finch_notify_emails(gc, 1, subject != NULL,
			subject ? &subject : NULL,
			from ? &from : NULL,
			to ? &to : NULL,
			url ? &url : NULL);
}

/** User information. **/
static GHashTable *userinfo;

static char *
userinfo_hash(PurpleAccount *account, const char *who)
{
	char key[256];
	g_snprintf(key, sizeof(key), "%s - %s", purple_account_get_username(account), purple_normalize(account, who));
	return g_utf8_strup(key, -1);
}

static void
remove_userinfo(GntWidget *widget, gpointer key)
{
	g_hash_table_remove(userinfo, key);
}

static char *
purple_notify_user_info_get_xhtml(PurpleNotifyUserInfo *user_info)
{
	GList *l;
	GString *text;

	text = g_string_new("<span>");

	for (l = purple_notify_user_info_get_entries(user_info)->head; l != NULL;
			l = l->next) {
		PurpleNotifyUserInfoEntry *user_info_entry = l->data;
		PurpleNotifyUserInfoEntryType type = purple_notify_user_info_entry_get_type(user_info_entry);
		const char *label = purple_notify_user_info_entry_get_label(user_info_entry);
		const char *value = purple_notify_user_info_entry_get_value(user_info_entry);

		/* Handle the label/value pair itself */
		if (type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER)
			g_string_append(text, "<u>");
		if (label)
			g_string_append_printf(text, "<b>%s</b>", label);
		g_string_append(text, "<span>");
		if (label && value)
			g_string_append(text, ": ");
		if (value) {
			char *strip = purple_markup_strip_html(value);
			g_string_append(text, strip);
			g_free(strip);
		}
		g_string_append(text, "</span>");
		if (type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER)
			g_string_append(text, "</u>");
		else if (type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK)
			g_string_append(text, "<HR/>");
		g_string_append(text, "<BR/>");
	}
	g_string_append(text, "</span>");

	return g_string_free(text, FALSE);
}

static void *
finch_notify_userinfo(PurpleConnection *gc, const char *who, PurpleNotifyUserInfo *user_info)
{
	char *primary;
	char *info;
	void *ui_handle;
	char *key = userinfo_hash(purple_connection_get_account(gc), who);

	info = purple_notify_user_info_get_xhtml(user_info);

	ui_handle = g_hash_table_lookup(userinfo, key);
	if (ui_handle != NULL) {
		GntTextView *msg = GNT_TEXT_VIEW(g_object_get_data(G_OBJECT(ui_handle), "info-widget"));
		char *strip = purple_markup_strip_html(info);
		int tvw, tvh, width, height, ntvw, ntvh;

		while (GNT_WIDGET(ui_handle)->parent)
			ui_handle = GNT_WIDGET(ui_handle)->parent;
		gnt_widget_get_size(GNT_WIDGET(ui_handle), &width, &height);
		gnt_widget_get_size(GNT_WIDGET(msg), &tvw, &tvh);

		gnt_text_view_clear(msg);
		if (!gnt_util_parse_xhtml_to_textview(info, msg))
			gnt_text_view_append_text_with_flags(msg, strip, GNT_TEXT_FLAG_NORMAL);
		gnt_text_view_scroll(msg, 0);
		gnt_util_get_text_bound(strip, &ntvw, &ntvh);
		ntvw += 3;
		ntvh++;

		gnt_screen_resize_widget(GNT_WIDGET(ui_handle), width + MAX(0, ntvw - tvw), height + MAX(0, ntvh - tvh));
		g_free(strip);
		g_free(key);
	} else {
		primary = g_strdup_printf(_("Info for %s"), who);
		ui_handle = finch_notify_formatted(_("Buddy Information"), primary, NULL, info);
		g_hash_table_insert(userinfo, key, ui_handle);
		g_free(primary);
		g_signal_connect(G_OBJECT(ui_handle), "destroy", G_CALLBACK(remove_userinfo), key);
	}

	g_free(info);
	return ui_handle;
}

static void
notify_button_activated(GntWidget *widget, PurpleNotifySearchButton *b)
{
	GList *list = NULL;
	PurpleAccount *account = g_object_get_data(G_OBJECT(widget), "notify-account");
	gpointer data = g_object_get_data(G_OBJECT(widget), "notify-data");

	list = gnt_tree_get_selection_text_list(GNT_TREE(g_object_get_data(G_OBJECT(widget), "notify-tree")));

	b->callback(purple_account_get_connection(account), list, data);
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static void
finch_notify_sr_new_rows(PurpleConnection *gc,
		PurpleNotifySearchResults *results, void *data)
{
	GntTree *tree = GNT_TREE(data);
	GList *o;

	/* XXX: Do I need to empty the tree here? */

	for (o = results->rows; o; o = o->next)
	{
		gnt_tree_add_row_after(GNT_TREE(tree), o->data,
				gnt_tree_create_row_from_list(GNT_TREE(tree), o->data),
				NULL, NULL);
	}
}

static void
notify_sr_destroy_cb(GntWidget *window, void *data)
{
	purple_notify_close(PURPLE_NOTIFY_SEARCHRESULTS, window);
}

static void *
finch_notify_searchresults(PurpleConnection *gc, const char *title,
		const char *primary, const char *secondary,
		PurpleNotifySearchResults *results, gpointer data)
{
	GntWidget *window, *tree, *box, *button;
	GList *iter;
	int columns, i;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_fill(GNT_BOX(window), TRUE);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	if (primary)
		gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(primary, GNT_TEXT_FLAG_BOLD));
	if (secondary)
		gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(secondary, GNT_TEXT_FLAG_NORMAL));

	columns = g_list_length(results->columns);
	tree = gnt_tree_new_with_columns(columns);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_box_add_widget(GNT_BOX(window), tree);

	i = 0;
	for (iter = results->columns; iter; iter = iter->next)
	{
		PurpleNotifySearchColumn *column = iter->data;
		gnt_tree_set_column_title(GNT_TREE(tree), i, purple_notify_searchresult_column_get_title(column));

		if (!purple_notify_searchresult_column_is_visible(column))
			gnt_tree_set_column_visible(GNT_TREE(tree), i, FALSE);
		i++;
	}

	box = gnt_hbox_new(TRUE);

	for (iter = results->buttons; iter; iter = iter->next)
	{
		PurpleNotifySearchButton *b = iter->data;
		const char *text;

		switch (b->type)
		{
			case PURPLE_NOTIFY_BUTTON_LABELED:
				text = b->label;
				break;
			case PURPLE_NOTIFY_BUTTON_CONTINUE:
				text = _("Continue");
				break;
			case PURPLE_NOTIFY_BUTTON_ADD:
				text = _("Add");
				break;
			case PURPLE_NOTIFY_BUTTON_INFO:
				text = _("Info");
				break;
			case PURPLE_NOTIFY_BUTTON_IM:
				text = _("IM");
				break;
			case PURPLE_NOTIFY_BUTTON_JOIN:
				text = _("Join");
				break;
			case PURPLE_NOTIFY_BUTTON_INVITE:
				text = _("Invite");
				break;
			default:
				text = _("(none)");
		}

		button = gnt_button_new(text);
		g_object_set_data(G_OBJECT(button), "notify-account", purple_connection_get_account(gc));
		g_object_set_data(G_OBJECT(button), "notify-data", data);
		g_object_set_data(G_OBJECT(button), "notify-tree", tree);
		g_signal_connect(G_OBJECT(button), "activate",
				G_CALLBACK(notify_button_activated), b);

		gnt_box_add_widget(GNT_BOX(box), button);
	}

	gnt_box_add_widget(GNT_BOX(window), box);
	g_signal_connect(G_OBJECT(tree), "destroy",
			G_CALLBACK(notify_sr_destroy_cb), NULL);

	finch_notify_sr_new_rows(gc, results, tree);

	gnt_widget_show(window);
	g_object_set_data(G_OBJECT(window), "notify-results", results);

	return tree;
}

static void *
finch_notify_uri(const char *url)
{
	return finch_notify_message(PURPLE_NOTIFY_URI, _("URI"), url, NULL);
}

static PurpleNotifyUiOps ops =
{
	finch_notify_message,
	finch_notify_email,
	finch_notify_emails,
	finch_notify_formatted,
	finch_notify_searchresults,
	finch_notify_sr_new_rows,
	finch_notify_userinfo,
	finch_notify_uri,
	finch_close_notify,       /* The rest of the notify-uiops return a GntWidget.
                                     These widgets should be destroyed from here. */
	NULL,
	NULL,
	NULL,
	NULL

};

PurpleNotifyUiOps *finch_notify_get_ui_ops()
{
	return &ops;
}

void finch_notify_init()
{
	userinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void finch_notify_uninit()
{
	g_hash_table_destroy(userinfo);
}


