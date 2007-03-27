/**
 * @file gntrequest.c GNT Request API
 * @ingroup gntui
 *
 * finch
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntfilesel.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>

#include "finch.h"
#include "gntrequest.h"
#include "util.c"

typedef struct
{
	void *user_data;
	GntWidget *dialog;
	GCallback *cbs;
} PurpleGntFileRequest;

static GntWidget *
setup_request_window(const char *title, const char *primary,
		const char *secondary, PurpleRequestType type)
{
	GntWidget *window;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	if (primary)
		gnt_box_add_widget(GNT_BOX(window),
				gnt_label_new_with_format(primary, GNT_TEXT_FLAG_BOLD));
	if (secondary)
		gnt_box_add_widget(GNT_BOX(window), gnt_label_new(secondary));

	g_signal_connect_swapped(G_OBJECT(window), "destroy", G_CALLBACK(purple_request_close),
			GINT_TO_POINTER(type));

	return window;
}

static GntWidget *
setup_button_box(gpointer userdata, gpointer cb, gpointer data, ...)
{
	GntWidget *box, *button;
	va_list list;
	const char *text;
	gpointer callback;

	box = gnt_hbox_new(FALSE);

	va_start(list, data);

	while ((text = va_arg(list, const char *)))
	{
		callback = va_arg(list, gpointer);
		button = gnt_button_new(text);
		gnt_box_add_widget(GNT_BOX(box), button);
		g_object_set_data(G_OBJECT(button), "activate-callback", callback);
		g_object_set_data(G_OBJECT(button), "activate-userdata", userdata);
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(cb), data);
	}

	va_end(list);
	return box;
}

static void
notify_input_cb(GntWidget *button, GntWidget *entry)
{
	PurpleRequestInputCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	const char *text = gnt_entry_get_text(GNT_ENTRY(entry));

	if (callback)
		callback(data, text);

	while (button->parent)
		button = button->parent;

	purple_request_close(PURPLE_REQUEST_INPUT, button);
}

static void *
finch_request_input(const char *title, const char *primary,
		const char *secondary, const char *default_value,
		gboolean multiline, gboolean masked, gchar *hint,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		void *user_data)
{
	GntWidget *window, *box, *entry;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_INPUT);

	entry = gnt_entry_new(default_value);
	if (masked)
		gnt_entry_set_masked(GNT_ENTRY(entry), TRUE);
	gnt_box_add_widget(GNT_BOX(window), entry);

	box = setup_button_box(user_data, notify_input_cb, entry,
			ok_text, ok_cb, cancel_text, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	gnt_widget_show(window);

	return window;
}

static void
finch_close_request(PurpleRequestType type, gpointer ui_handle)
{
	GntWidget *widget = GNT_WIDGET(ui_handle);
	while (widget->parent)
		widget = widget->parent;
	gnt_widget_destroy(widget);
}

static void
request_choice_cb(GntWidget *button, GntComboBox *combo)
{
	PurpleRequestChoiceCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int choice = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo))) - 1;

	if (callback)
		callback(data, choice);

	while (button->parent)
		button = button->parent;

	purple_request_close(PURPLE_REQUEST_INPUT, button);
}

static void *
finch_request_choice(const char *title, const char *primary,
		const char *secondary, unsigned int default_value,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		void *user_data, va_list choices)
{
	GntWidget *window, *combo, *box;
	const char *text;
	int val;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_CHOICE);

	combo = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(window), combo);
	while ((text = va_arg(choices, const char *)))
	{
		val = va_arg(choices, int);
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), GINT_TO_POINTER(val + 1), text);
	}
	gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), GINT_TO_POINTER(default_value + 1));

	box = setup_button_box(user_data, request_choice_cb, combo,
			ok_text, ok_cb, cancel_text, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	gnt_widget_show(window);
	
	return window;
}

static void
request_action_cb(GntWidget *button, GntWidget *window)
{
	PurpleRequestActionCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "activate-id"));

	if (callback)
		callback(data, id);

	purple_request_close(PURPLE_REQUEST_ACTION, window);
}

static void*
finch_request_action(const char *title, const char *primary,
		const char *secondary, unsigned int default_value,
		void *user_data, size_t actioncount,
		va_list actions)
{
	GntWidget *window, *box, *button;
	int i;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_ACTION);

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);
	for (i = 0; i < actioncount; i++)
	{
		const char *text = va_arg(actions, const char *);
		PurpleRequestActionCb callback = va_arg(actions, PurpleRequestActionCb);

		button = gnt_button_new(text);
		gnt_box_add_widget(GNT_BOX(box), button);

		g_object_set_data(G_OBJECT(button), "activate-callback", callback);
		g_object_set_data(G_OBJECT(button), "activate-userdata", user_data);
		g_object_set_data(G_OBJECT(button), "activate-id", GINT_TO_POINTER(i));
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(request_action_cb), window);
	}

	gnt_widget_show(window);

	return window;
}

static void
request_fields_cb(GntWidget *button, PurpleRequestFields *fields)
{
	PurpleRequestFieldsCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	GList *list;

	/* Update the data of the fields. GtkPurple does this differently. Instead of
	 * updating the fields at the end like here, it updates the appropriate field
	 * instantly whenever a change is made. That allows it to make sure the
	 * 'required' fields are entered before the user can hit OK. It's not the case
	 * here, althought it can be done. I am not honouring the 'required' fields
	 * for the moment. */
	for (list = purple_request_fields_get_groups(fields); list; list = list->next)
	{
		PurpleRequestFieldGroup *group = list->data;
		GList *fields = purple_request_field_group_get_fields(group);
		
		for (; fields ; fields = fields->next)
		{
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
			{
				GntWidget *check = field->ui_data;
				gboolean value = gnt_check_box_get_checked(GNT_CHECK_BOX(check));
				purple_request_field_bool_set_value(field, value);
			}
			else if (type == PURPLE_REQUEST_FIELD_STRING)
			{
				GntWidget *entry = field->ui_data;
				const char *text = gnt_entry_get_text(GNT_ENTRY(entry));
				purple_request_field_string_set_value(field, (text && *text) ? text : NULL);
			}
			else if (type == PURPLE_REQUEST_FIELD_INTEGER)
			{
				GntWidget *entry = field->ui_data;
				const char *text = gnt_entry_get_text(GNT_ENTRY(entry));
				int value = (text && *text) ? atoi(text) : 0;
				purple_request_field_int_set_value(field, value);
			}
			else if (type == PURPLE_REQUEST_FIELD_CHOICE)
			{
				GntWidget *combo = field->ui_data;
				int id;
				id = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo)));
				purple_request_field_choice_set_value(field, id);
			}
			else if (type == PURPLE_REQUEST_FIELD_LIST)
			{
				GList *list = NULL;
				if (purple_request_field_list_get_multi_select(field))
				{
					const GList *iter;
					GntWidget *tree = field->ui_data;

					iter = purple_request_field_list_get_items(field);
					for (; iter; iter = iter->next)
					{
						const char *text = iter->data;
						gpointer key = purple_request_field_list_get_data(field, text);
						if (gnt_tree_get_choice(GNT_TREE(tree), key))
							list = g_list_prepend(list, key);
					}
				}
				else
				{
					GntWidget *combo = field->ui_data;
					gpointer data = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo));
					list = g_list_append(list, data);
				}

				purple_request_field_list_set_selected(field, list);
				g_list_free(list);
			}
			else if (type == PURPLE_REQUEST_FIELD_ACCOUNT)
			{
				GntWidget *combo = field->ui_data;
				PurpleAccount *acc = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo));
				purple_request_field_account_set_value(field, acc);
			}
		}
	}

	if (callback)
		callback(data, fields);

	while (button->parent)
		button = button->parent;

	purple_request_close(PURPLE_REQUEST_FIELDS, button);
}

static void *
finch_request_fields(const char *title, const char *primary,
		const char *secondary, PurpleRequestFields *allfields,
		const char *ok, GCallback ok_cb,
		const char *cancel, GCallback cancel_cb,
		void *userdata)
{
	GntWidget *window, *box;
	GList *grlist;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_FIELDS);

	/* This is how it's going to work: the request-groups are going to be
	 * stacked vertically one after the other. A GntLine will be separating
	 * the groups. */
	box = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(box), 0);
	gnt_box_set_fill(GNT_BOX(box), TRUE);
	for (grlist = purple_request_fields_get_groups(allfields); grlist; grlist = grlist->next)
	{
		PurpleRequestFieldGroup *group = grlist->data;
		GList *fields = purple_request_field_group_get_fields(group);
		GntWidget *hbox;
		const char *title = purple_request_field_group_get_title(group);

		if (title)
			gnt_box_add_widget(GNT_BOX(box),
					gnt_label_new_with_format(title, GNT_TEXT_FLAG_BOLD));

		for (; fields ; fields = fields->next)
		{
			/* XXX: Break each of the fields into a separate function? */
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			const char *label = purple_request_field_get_label(field);
				
			hbox = gnt_hbox_new(TRUE);   /* hrm */
			gnt_box_add_widget(GNT_BOX(box), hbox);
			
			if (type != PURPLE_REQUEST_FIELD_BOOLEAN && label)
			{
				GntWidget *l = gnt_label_new(label);
				gnt_widget_set_size(l, 0, 1);
				gnt_box_add_widget(GNT_BOX(hbox), l);
			}

			if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
			{
				GntWidget *check = gnt_check_box_new(label);
				gnt_check_box_set_checked(GNT_CHECK_BOX(check),
						purple_request_field_bool_get_default_value(field));
				gnt_box_add_widget(GNT_BOX(hbox), check);
				field->ui_data = check;
			}
			else if (type == PURPLE_REQUEST_FIELD_STRING)
			{
				const char *hint = purple_request_field_get_type_hint(field);
				GntWidget *entry = gnt_entry_new(
							purple_request_field_string_get_default_value(field));
				gnt_entry_set_masked(GNT_ENTRY(entry),
						purple_request_field_string_is_masked(field));
				if (purple_str_has_prefix(hint, "screenname")) {
					PurpleBlistNode *node = purple_blist_get_root();
					gboolean offline = purple_str_has_suffix(hint, "all");
					for (; node; node = purple_blist_node_next(node, offline)) {
						if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
							continue;
						gnt_entry_add_suggest(GNT_ENTRY(entry), purple_buddy_get_name((PurpleBuddy*)node));
					}
					gnt_entry_set_always_suggest(GNT_ENTRY(entry), TRUE);
				} else if (hint && !strcmp(hint, "group")) {
					PurpleBlistNode *node;
					for (node = purple_blist_get_root(); node; node = node->next) {
						if (PURPLE_BLIST_NODE_IS_GROUP(node))
							gnt_entry_add_suggest(GNT_ENTRY(entry), ((PurpleGroup *)node)->name);
					}
				}
				gnt_box_add_widget(GNT_BOX(hbox), entry);
				field->ui_data = entry;
			}
			else if (type == PURPLE_REQUEST_FIELD_INTEGER)
			{
				char str[256];
				int val = purple_request_field_int_get_default_value(field);
				GntWidget *entry;
				
				snprintf(str, sizeof(str), "%d", val);
				entry = gnt_entry_new(str);
				gnt_entry_set_flag(GNT_ENTRY(entry), GNT_ENTRY_FLAG_INT);
				gnt_box_add_widget(GNT_BOX(hbox), entry);
				field->ui_data = entry;
			}
			else if (type == PURPLE_REQUEST_FIELD_CHOICE)
			{
				int id;
				const GList *list;
				GntWidget *combo = gnt_combo_box_new();
				gnt_box_add_widget(GNT_BOX(hbox), combo);
				field->ui_data = combo;

				list = purple_request_field_choice_get_labels(field);
				for (id = 1; list; list = list->next, id++)
				{
					gnt_combo_box_add_data(GNT_COMBO_BOX(combo),
							GINT_TO_POINTER(id), list->data);
				}
				gnt_combo_box_set_selected(GNT_COMBO_BOX(combo),
						GINT_TO_POINTER(purple_request_field_choice_get_default_value(field)));
			}
			else if (type == PURPLE_REQUEST_FIELD_LIST)
			{
				const GList *list;
				gboolean multi = purple_request_field_list_get_multi_select(field);
				if (multi)
				{
					GntWidget *tree = gnt_tree_new();
					gnt_box_add_widget(GNT_BOX(hbox), tree);
					field->ui_data = tree;

					list = purple_request_field_list_get_items(field);
					for (; list; list = list->next)
					{
						const char *text = list->data;
						gpointer key = purple_request_field_list_get_data(field, text);
						gnt_tree_add_choice(GNT_TREE(tree), key,
								gnt_tree_create_row(GNT_TREE(tree), text), NULL, NULL);
						if (purple_request_field_list_is_selected(field, text))
							gnt_tree_set_choice(GNT_TREE(tree), key, TRUE);
					}
				}
				else
				{
					GntWidget *combo = gnt_combo_box_new();
					gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
					gnt_box_add_widget(GNT_BOX(hbox), combo);
					field->ui_data = combo;

					list = purple_request_field_list_get_items(field);
					for (; list; list = list->next)
					{
						const char *text = list->data;
						gpointer key = purple_request_field_list_get_data(field, text);
						gnt_combo_box_add_data(GNT_COMBO_BOX(combo), key, text);
						if (purple_request_field_list_is_selected(field, text))
							gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), key);
					}
				}
			}
			else if (type == PURPLE_REQUEST_FIELD_ACCOUNT)
			{
				gboolean all;
				PurpleAccount *def;
				GList *list;
				GntWidget *combo = gnt_combo_box_new();
				gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
				gnt_box_add_widget(GNT_BOX(hbox), combo);
				field->ui_data = combo;

				all = purple_request_field_account_get_show_all(field);
				def = purple_request_field_account_get_default_value(field);

				if (all)
					list = purple_accounts_get_all();
				else
					list = purple_connections_get_all();

				for (; list; list = list->next)
				{
					PurpleAccount *account;
					char *text;

					if (all)
						account = list->data;
					else
						account = purple_connection_get_account(list->data);

					text = g_strdup_printf("%s (%s)",
							purple_account_get_username(account),
							purple_account_get_protocol_name(account));
					gnt_combo_box_add_data(GNT_COMBO_BOX(combo), account, text);
					g_free(text);
					if (account == def)
						gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), account);
				}
				gnt_widget_set_size(combo, 20, 3); /* ew */
			}
			else
			{
				gnt_box_add_widget(GNT_BOX(hbox),
						gnt_label_new_with_format(_("Not implemented yet."),
							GNT_TEXT_FLAG_BOLD));
			}
		}
		if (grlist->next)
			gnt_box_add_widget(GNT_BOX(box), gnt_hline_new());
	}
	gnt_box_add_widget(GNT_BOX(window), box);

	box = setup_button_box(userdata, request_fields_cb, allfields,
			ok, ok_cb, cancel, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	gnt_widget_show(window);
	
	return window;
}

static void
file_cancel_cb(GntWidget *wid, gpointer fq)
{
	PurpleGntFileRequest *data = fq;
	if (data->cbs[1] != NULL)
		((PurpleRequestFileCb)data->cbs[1])(data->user_data, NULL);

	purple_request_close(PURPLE_REQUEST_FILE, data->dialog);
}

static void
file_ok_cb(GntWidget *wid, gpointer fq)
{
	PurpleGntFileRequest *data = fq;
	char *file = gnt_file_sel_get_selected_file(GNT_FILE_SEL(data->dialog));
	if (data->cbs[0] != NULL)
		((PurpleRequestFileCb)data->cbs[0])(data->user_data, file);
	g_free(file);

	purple_request_close(PURPLE_REQUEST_FILE, data->dialog);
}

static void
file_request_destroy(PurpleGntFileRequest *data)
{
	g_free(data->cbs);
	g_free(data);
}

static void *
finch_request_file(const char *title, const char *filename,
				gboolean savedialog,
				GCallback ok_cb, GCallback cancel_cb,
				void *user_data)
{
	GntWidget *window = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(window);
	PurpleGntFileRequest *data = g_new0(PurpleGntFileRequest, 1);

	data->user_data = user_data;
	data->cbs = g_new0(GCallback, 2);
	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;
	data->dialog = window;
	gnt_box_set_title(GNT_BOX(window), title ? title : (savedialog ? _("Save File...") : _("Open File...")));
	gnt_file_sel_set_current_location(sel, purple_home_dir());  /* XXX: */
	if (savedialog)
		gnt_file_sel_set_suggested_filename(sel, filename);
	g_signal_connect(G_OBJECT(sel->cancel), "activate",
			G_CALLBACK(file_cancel_cb), data);
	g_signal_connect(G_OBJECT(sel->select), "activate",
			G_CALLBACK(file_ok_cb), data);
	g_signal_connect_swapped(G_OBJECT(window), "destroy",
			G_CALLBACK(file_request_destroy), data);

	gnt_widget_show(window);

	return window;
}

static PurpleRequestUiOps uiops =
{
	.request_input = finch_request_input,
	.close_request = finch_close_request,
	.request_choice = finch_request_choice,
	.request_action = finch_request_action,
	.request_fields = finch_request_fields,
	.request_file = finch_request_file,
	.request_folder = NULL                        /* No plans for this */
};

PurpleRequestUiOps *finch_request_get_ui_ops()
{
	return &uiops;
}

void finch_request_init()
{
}

void finch_request_uninit()
{
}

