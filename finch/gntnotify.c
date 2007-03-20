/**
 * @file gntnotify.c GNT Notify API
 * @ingroup gntui
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntlabel.h>
#include <gnttree.h>

#include <util.h>

#include "gntnotify.h"
#include "finch.h"

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

	window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_fill(GNT_BOX(window), FALSE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	if (primary)
		gnt_box_add_widget(GNT_BOX(window),
				gnt_label_new_with_format(primary, pf));
	if (secondary)
		gnt_box_add_widget(GNT_BOX(window),
				gnt_label_new_with_format(secondary, sf));

	button = gnt_button_new(_("OK"));
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
	/* XXX: For now, simply strip the html and use _notify_message. For future use,
	 * there should be some way of parsing the makrups from GntTextView */
	char *unformat = purple_markup_strip_html(text);
	char *t = g_strdup_printf("%s%s%s",
			secondary ? secondary : "",
			secondary ? "\n" : "",
			unformat ? unformat : "");

	void *ret = finch_notify_message(PURPLE_NOTIFY_FORMATTED, title, primary, t);

	g_free(t);
	g_free(unformat);

	return ret;
}

static void
reset_email_dialog()
{
	emaildialog.window = NULL;
	emaildialog.tree = NULL;
}

static void
setup_email_dialog()
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
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Account"), _("From"), _("Subject"));
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

		setup_email_dialog();

		to = g_strdup_printf("%s (%s)", tos ? *tos : purple_account_get_username(account),
					purple_account_get_protocol_name(account));
		gnt_tree_add_row_after(GNT_TREE(emaildialog.tree), GINT_TO_POINTER(time(NULL)),
				gnt_tree_create_row(GNT_TREE(emaildialog.tree), to,
					froms ? *froms : "[Unknown sender]",
					*subjects),
				NULL, NULL);
		g_free(to);
		gnt_widget_show(emaildialog.window);
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

static void *
finch_notify_userinfo(PurpleConnection *gc, const char *who, PurpleNotifyUserInfo *user_info)
{
	/* Xeroxed from gtknotify.c */
	char *primary;
	char *info;
	void *ui_handle;
	
	primary = g_strdup_printf(_("Info for %s"), who);
	info = purple_notify_user_info_get_text_with_newline(user_info, "<BR>");
	ui_handle = finch_notify_formatted(_("Buddy Information"), primary, NULL, info);
	g_free(info);
	g_free(primary);
	return ui_handle;
}

static void
notify_button_activated(GntWidget *widget, PurpleNotifySearchButton *b)
{
	GList *list = NULL;
	PurpleAccount *account = g_object_get_data(G_OBJECT(widget), "notify-account");
	gpointer data = g_object_get_data(G_OBJECT(widget), "notify-data");

	list = gnt_tree_get_selection_text_list(GNT_TREE(widget));

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

static void *
finch_notify_searchresults(PurpleConnection *gc, const char *title,
		const char *primary, const char *secondary,
		PurpleNotifySearchResults *results, gpointer data)
{
	GntWidget *window, *tree, *box, *button;
	GList *iter;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_fill(GNT_BOX(window), FALSE);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(primary, GNT_TEXT_FLAG_BOLD));
	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(secondary, GNT_TEXT_FLAG_NORMAL));

	tree = gnt_tree_new_with_columns(g_list_length(results->columns));
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_box_add_widget(GNT_BOX(window), tree);

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
		g_signal_connect_swapped(G_OBJECT(button), "activate",
				G_CALLBACK(notify_button_activated), b);

		gnt_box_add_widget(GNT_BOX(box), button);
	}

	gnt_box_add_widget(GNT_BOX(window), box);

	finch_notify_sr_new_rows(gc, results, tree);

	gnt_widget_show(window);
	g_object_set_data(G_OBJECT(window), "notify-results", results);

	return tree;
}

static PurpleNotifyUiOps ops = 
{
	.notify_message = finch_notify_message,
	.close_notify = finch_close_notify,       /* The rest of the notify-uiops return a GntWidget.
                                              These widgets should be destroyed from here. */
	.notify_formatted = finch_notify_formatted,
	.notify_email = finch_notify_email,
	.notify_emails = finch_notify_emails,
	.notify_userinfo = finch_notify_userinfo,

	.notify_searchresults = finch_notify_searchresults,
	.notify_searchresults_new_rows = finch_notify_sr_new_rows,
	.notify_uri = NULL                     /* This is of low-priority to me */
};

PurpleNotifyUiOps *finch_notify_get_ui_ops()
{
	return &ops;
}

void finch_notify_init()
{
}

void finch_notify_uninit()
{
}


