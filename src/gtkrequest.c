/**
 * @file gtkrequest.c GTK+ Request API
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#include "gtkinternal.h"

#include "prefs.h"

#include "gtkrequest.h"
#include "gtkutils.h"
#include "stock.h"
#include "ui.h"

typedef struct
{
	GaimRequestType type;

	void *user_data;
	GtkWidget *dialog;

	GtkWidget *ok_button;

	size_t cb_count;
	GCallback *cbs;

	union
	{
		struct
		{
			GtkWidget *entry;

			gboolean multiline;

		} input;

		struct
		{
			GaimRequestFields *fields;

		} multifield;

	} u;

} GaimGtkRequestData;

static void
input_response_cb(GtkDialog *dialog, gint id, GaimGtkRequestData *data)
{
	const char *value;
	char *multiline_value = NULL;

	if (data->u.input.multiline) {
		GtkTextIter start_iter, end_iter;
		GtkTextBuffer *buffer =
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->u.input.entry));

		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(buffer, &end_iter);

		multiline_value = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
										 FALSE);
		value = multiline_value;
	}
	else
		value = gtk_entry_get_text(GTK_ENTRY(data->u.input.entry));

	if (id < data->cb_count && data->cbs[id] != NULL)
		((GaimRequestInputCb)data->cbs[id])(data->user_data, value);
	else if (data->cbs[1] != NULL)
		((GaimRequestInputCb)data->cbs[1])(data->user_data, value);

	if (data->u.input.multiline)
		g_free(multiline_value);

	gaim_request_close(GAIM_REQUEST_INPUT, data);
}

static void
action_response_cb(GtkDialog *dialog, gint id, GaimGtkRequestData *data)
{
	if (id < data->cb_count && data->cbs[id] != NULL)
		((GaimRequestActionCb)data->cbs[id])(data->user_data, id);

	gaim_request_close(GAIM_REQUEST_INPUT, data);
}

static gboolean
field_string_focus_out_cb(GtkWidget *entry, GdkEventFocus *event,
						  GaimRequestField *field)
{
	const char *value;

	if (gaim_request_field_string_is_multiline(field))
	{
		GtkTextBuffer *buffer;
		GtkTextIter start_iter, end_iter;

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));

		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(buffer, &end_iter);

		value = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);
	}
	else
		value = gtk_entry_get_text(GTK_ENTRY(entry));

	gaim_request_field_string_set_value(field,
			(*value == '\0' ? NULL : value));

	return FALSE;
}

static gboolean
field_int_focus_out_cb(GtkEntry *entry, GdkEventFocus *event,
					   GaimRequestField *field)
{
	gaim_request_field_int_set_value(field,
			atoi(gtk_entry_get_text(entry)));

	return FALSE;
}

static void
field_bool_cb(GtkToggleButton *button, GaimRequestField *field)
{
	gaim_request_field_bool_set_value(field,
			gtk_toggle_button_get_active(button));
}

static void
field_choice_menu_cb(GtkOptionMenu *menu, GaimRequestField *field)
{
	gaim_request_field_choice_set_value(field,
			gtk_option_menu_get_history(menu));
}

static void
field_choice_option_cb(GtkRadioButton *button, GaimRequestField *field)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		gaim_request_field_choice_set_value(field,
				g_slist_index(gtk_radio_button_get_group(button), button));
}

static void
field_account_cb(GObject *w, GaimAccount *account, GaimRequestField *field)
{
	gaim_request_field_account_set_value(field, account);
}

static void
multifield_ok_cb(GtkWidget *button, GaimGtkRequestData *data)
{
	if (data->cbs[0] != NULL)
		((GaimRequestFieldsCb)data->cbs[0])(data->user_data,
											data->u.multifield.fields);

	gaim_request_close(GAIM_REQUEST_FIELDS, data);
}

static void
multifield_cancel_cb(GtkWidget *button, GaimGtkRequestData *data)
{
	if (data->cbs[1] != NULL)
		((GaimRequestFieldsCb)data->cbs[1])(data->user_data,
											data->u.multifield.fields);

	gaim_request_close(GAIM_REQUEST_FIELDS, data);
}

#define STOCK_ITEMIZE(r, l) \
	if (!strcmp((r), text)) \
		return (l);

static const char *
text_to_stock(const char *text)
{
	STOCK_ITEMIZE(_("Yes"),     GTK_STOCK_YES);
	STOCK_ITEMIZE(_("No"),      GTK_STOCK_NO);
	STOCK_ITEMIZE(_("OK"),      GTK_STOCK_OK);
	STOCK_ITEMIZE(_("Cancel"),  GTK_STOCK_CANCEL);
	STOCK_ITEMIZE(_("Apply"),   GTK_STOCK_APPLY);
	STOCK_ITEMIZE(_("Close"),   GTK_STOCK_CLOSE);
	STOCK_ITEMIZE(_("Delete"),  GTK_STOCK_DELETE);
	STOCK_ITEMIZE(_("Add"),     GTK_STOCK_ADD);
	STOCK_ITEMIZE(_("Remove"),  GTK_STOCK_REMOVE);
	STOCK_ITEMIZE(_("Save"),    GTK_STOCK_SAVE);

	return text;
}

static void *
gaim_gtk_request_input(const char *title, const char *primary,
					   const char *secondary, const char *default_value,
					   gboolean multiline, gboolean masked,
					   const char *ok_text, GCallback ok_cb,
					   const char *cancel_text, GCallback cancel_cb,
					   void *user_data)
{
	GaimGtkRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *img;
	char *label_text;

	data            = g_new0(GaimGtkRequestData, 1);
	data->type      = GAIM_REQUEST_INPUT;
	data->user_data = user_data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;

	/* Create the dialog. */
	dialog = gtk_dialog_new_with_buttons(title ? title : GAIM_ALERT_TITLE,
					     NULL, 0,
					     text_to_stock(cancel_text), 1,
					     text_to_stock(ok_text),     0,
					     NULL);
	data->dialog = dialog;

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(input_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), 0);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	label_text = g_strdup_printf((primary ? "<span weight=\"bold\" size=\"larger\">"
								 "%s</span>%s%s" : "%s%s%s"),
								 (primary ? primary : ""),
								 ((primary && secondary) ? "\n\n" : ""),
								 (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	/* Entry field. */
	data->u.input.multiline = multiline;

	if (multiline) {
		GtkWidget *sw;

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
									   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
											GTK_SHADOW_IN);

		gtk_widget_set_size_request(sw, 320, 130);

		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

		entry = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(entry), TRUE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(entry), GTK_WRAP_WORD_CHAR);

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
			gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(entry));

		gtk_container_add(GTK_CONTAINER(sw), entry);

		if (default_value != NULL) {
			GtkTextBuffer *buffer;

			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
			gtk_text_buffer_set_text(buffer, default_value, -1);
		}
	}
	else {
		entry = gtk_entry_new();

		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

		gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

		if (default_value != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), default_value);

		if (masked)
			gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	}

	gaim_set_accessible_label (entry, label);
	data->u.input.entry = entry;

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

static void *
gaim_gtk_request_choice(const char *title, const char *primary,
						const char *secondary, unsigned int default_value,
						const char *ok_text, GCallback ok_cb,
						const char *cancel_text, GCallback cancel_cb,
						void *user_data, size_t choice_count, va_list args)
{
	return NULL;
}

static void *
gaim_gtk_request_action(const char *title, const char *primary,
						const char *secondary, unsigned int default_action,
						void *user_data, size_t action_count, va_list actions)
{
	GaimGtkRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	void **buttons;
	char *label_text;
	int i;

	data            = g_new0(GaimGtkRequestData, 1);
	data->type      = GAIM_REQUEST_ACTION;
	data->user_data = user_data;

	data->cb_count = action_count;
	data->cbs = g_new0(GCallback, action_count);

	/* Reverse the buttons */
	buttons = g_new0(void *, action_count * 2);

	for (i = 0; i < action_count * 2; i += 2) {
		buttons[(action_count * 2) - i - 2] = va_arg(actions, char *);
		buttons[(action_count * 2) - i - 1] = va_arg(actions, GCallback);
	}

	/* Create the dialog. */
	data->dialog = dialog = gtk_dialog_new();

	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(dialog), title);

	for (i = 0; i < action_count; i++) {
		gtk_dialog_add_button(GTK_DIALOG(dialog),
							  text_to_stock(buttons[2 * i]), i);

		data->cbs[i] = buttons[2 * i + 1];
	}

	g_free(buttons);

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(action_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	label_text = g_strdup_printf((primary ? "<span weight=\"bold\" size=\"larger\">"
								 "%s</span>%s%s" : "%s%s%s"),
								 (primary ? primary : ""),
								 ((primary && secondary) ? "\n\n" : ""),
 								 (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	if (default_action != -1)
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_action);

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

static void
req_entry_field_changed_cb(GtkWidget *entry, GaimRequestField *field)
{
	GaimGtkRequestData *req_data;
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));

	gaim_request_field_string_set_value(field, (*text == '\0' ? NULL : text));

	req_data = (GaimGtkRequestData *)field->group->fields_list->ui_data;

	gtk_widget_set_sensitive(req_data->ok_button,
		gaim_request_fields_all_required_filled(field->group->fields_list));
}

static void
setup_entry_field(GtkWidget *entry, GaimRequestField *field)
{
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	if (gaim_request_field_is_required(field))
	{
		g_signal_connect(G_OBJECT(entry), "changed",
						 G_CALLBACK(req_entry_field_changed_cb), field);
	}
}

static GtkWidget *
create_string_field(GaimRequestField *field)
{
	const char *value;
	GtkWidget *widget;

	value = gaim_request_field_string_get_default_value(field);

	if (gaim_request_field_string_is_multiline(field))
	{
		GtkWidget *textview;

		widget = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(widget),
											GTK_SHADOW_IN);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
									   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

		textview = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(textview),
								   TRUE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview),
									GTK_WRAP_WORD_CHAR);

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
			gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(textview));

		gtk_container_add(GTK_CONTAINER(widget), textview);
		gtk_widget_show(textview);

		gtk_widget_set_size_request(widget, -1, 75);

		if (value != NULL)
		{
			GtkTextBuffer *buffer;

			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

			gtk_text_buffer_set_text(buffer, value, -1);
		}

		gtk_text_view_set_editable(GTK_TEXT_VIEW(textview),
			gaim_request_field_string_is_editable(field));

		g_signal_connect(G_OBJECT(textview), "focus-out-event",
						 G_CALLBACK(field_string_focus_out_cb), field);
	}
	else
	{
		widget = gtk_entry_new();

		setup_entry_field(widget, field);

		if (value != NULL)
			gtk_entry_set_text(GTK_ENTRY(widget), value);

		gtk_entry_set_visibility(GTK_ENTRY(widget),
			!gaim_request_field_string_is_masked(field));

		gtk_editable_set_editable(GTK_EDITABLE(widget),
			gaim_request_field_string_is_editable(field));

		g_signal_connect(G_OBJECT(widget), "focus-out-event",
						 G_CALLBACK(field_string_focus_out_cb), field);
	}

	return widget;
}

static GtkWidget *
create_int_field(GaimRequestField *field)
{
	int value;
	GtkWidget *widget;

	widget = gtk_entry_new();

	setup_entry_field(widget, field);

	value = gaim_request_field_int_get_default_value(field);

	if (value != 0)
	{
		char buf[32];

		g_snprintf(buf, sizeof(buf), "%d", value);

		gtk_entry_set_text(GTK_ENTRY(widget), buf);
	}

	g_signal_connect(G_OBJECT(widget), "focus-out-event",
					 G_CALLBACK(field_int_focus_out_cb), field);

	return widget;
}

static GtkWidget *
create_bool_field(GaimRequestField *field)
{
	GtkWidget *widget;

	widget = gtk_check_button_new_with_label(
		gaim_request_field_get_label(field));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		gaim_request_field_bool_get_default_value(field));

	g_signal_connect(G_OBJECT(widget), "toggled",
					 G_CALLBACK(field_bool_cb), field);

	return widget;
}

static GtkWidget *
create_choice_field(GaimRequestField *field)
{
	GtkWidget *widget;
	GList *labels;
	GList *l;

	labels = gaim_request_field_choice_get_labels(field);

	if (g_list_length(labels) > 5)
	{
		GtkWidget *menu;
		GtkWidget *item;

		widget = gtk_option_menu_new();

		menu = gtk_menu_new();

		gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);

		for (l = labels; l != NULL; l = l->next)
		{
			const char *text = l->data;

			item = gtk_menu_item_new_with_label(text);

			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		}

		g_signal_connect(G_OBJECT(widget), "changed",
						 G_CALLBACK(field_choice_menu_cb), field);
	}
	else
	{
		GtkWidget *box;
		GtkWidget *first_radio = NULL;
		GtkWidget *radio;

		if (g_list_length(labels) == 2)
			box = gtk_hbox_new(FALSE, 6);
		else
			box = gtk_vbox_new(FALSE, 0);

		widget = box;

		for (l = labels; l != NULL; l = l->next)
		{
			const char *text = l->data;

			radio = gtk_radio_button_new_with_label_from_widget(
				GTK_RADIO_BUTTON(first_radio), text);

			if (first_radio == NULL)
				first_radio = radio;

			gtk_box_pack_start(GTK_BOX(box), radio, TRUE, TRUE, 0);
			gtk_widget_show(radio);

			g_signal_connect(G_OBJECT(radio), "toggled",
							 G_CALLBACK(field_choice_option_cb), field);
		}
	}

	return widget;
}

static GtkWidget *
create_account_field(GaimRequestField *field)
{
	GtkWidget *widget;

	widget = gaim_gtk_account_option_menu_new(
		gaim_request_field_account_get_default_value(field),
		gaim_request_field_account_get_show_all(field),
		G_CALLBACK(field_account_cb), NULL, field);

	return widget;
}

static void
select_field_list_item(GtkTreeModel *model, GtkTreePath *path,
					   GtkTreeIter *iter, gpointer data)
{
	GaimRequestField *field = (GaimRequestField *)data;
	const char *text;

	gtk_tree_model_get(model, iter, 1, &text, -1);

	gaim_request_field_list_add_selected(field, text);
}

static void
list_field_select_changed_cb(GtkTreeSelection *sel, GaimRequestField *field)
{
	gaim_request_field_list_clear_selected(field);

	gtk_tree_selection_selected_foreach(sel, select_field_list_item, field);
}

static GtkWidget *
create_list_field(GaimRequestField *field)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	const GList *l;

	/* Create the scrolled window */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list store */
	store = gtk_list_store_new(2, G_TYPE_POINTER, G_TYPE_STRING);

	/* Create the tree view */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	if (gaim_request_field_list_get_multi_select(field))
		gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(list_field_select_changed_cb), field);

	column = gtk_tree_view_column_new();
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", 1);

	for (l = gaim_request_field_list_get_items(field); l != NULL; l = l->next)
	{
		const char *text = (const char *)l->data;

		gtk_list_store_append(store, &iter);

		gtk_list_store_set(store, &iter,
						   0, gaim_request_field_list_get_data(field, text),
						   1, text,
						   -1);

		if (gaim_request_field_list_is_selected(field, text))
			gtk_tree_selection_select_iter(sel, &iter);
	}

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	return sw;
}

static void *
gaim_gtk_request_fields(const char *title, const char *primary,
						const char *secondary, GaimRequestFields *fields,
						const char *ok_text, GCallback ok_cb,
						const char *cancel_text, GCallback cancel_cb,
						void *user_data)
{
	GaimGtkRequestData *data;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *button;
	GtkWidget *img;
	GtkSizeGroup *sg;
	GList *gl, *fl;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	char *label_text;

	data            = g_new0(GaimGtkRequestData, 1);
	data->type      = GAIM_REQUEST_FIELDS;
	data->user_data = user_data;
	data->u.multifield.fields = fields;

	fields->ui_data = data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;

	data->dialog = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(win), title);

	gtk_window_set_role(GTK_WINDOW(win), "multifield");
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), hbox);
	gtk_widget_show(hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_widget_show(img);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if (primary != NULL || secondary != NULL)
	{
		label_text = g_strdup_printf(
			(primary
			 ? "<span weight=\"bold\" size=\"larger\">"
			   "%s</span>%s%s"
			 : "%s%s%s"),
			(primary ? primary : ""),
			((primary && secondary) ? "\n\n" : ""),
			(secondary ? secondary : ""));

		label = gtk_label_new(NULL);

		gtk_label_set_markup(GTK_LABEL(label), label_text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_free(label_text);
	}

	for (gl = gaim_request_fields_get_groups(fields);
		 gl != NULL;
		 gl = gl->next)
	{
		GList *field_list;
		size_t field_count = 0;
		size_t cols = 1;
		size_t rows;
		size_t col_num;
		size_t row_num = 0;

		group      = gl->data;
		field_list = gaim_request_field_group_get_fields(group);

		if (gaim_request_field_group_get_title(group) != NULL)
		{
			frame = gaim_gtk_make_frame(vbox,
				gaim_request_field_group_get_title(group));
		}
		else
			frame = vbox;

		field_count = g_list_length(field_list);

		if (field_count > 9)
		{
			rows = field_count / 2;
			cols++;
		}
		else
			rows = field_count;

		col_num = 0;

		for (fl = field_list; fl != NULL; fl = fl->next)
		{
			GaimRequestFieldType type;

			field = (GaimRequestField *)fl->data;

			type = gaim_request_field_get_type(field);

			if (type == GAIM_REQUEST_FIELD_LABEL)
			{
				if (col_num > 0)
					rows++;

				rows++;
			}
			else if (type == GAIM_REQUEST_FIELD_STRING &&
					 gaim_request_field_string_is_multiline(field))
			{
				if (col_num > 0)
					rows++;

				rows += 2;
			}

			col_num++;

			if(col_num >= cols)
				col_num = 0;
		}

		table = gtk_table_new(rows, 2 * cols, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 6);
		gtk_table_set_col_spacings(GTK_TABLE(table), 6);

		gtk_container_add(GTK_CONTAINER(frame), table);
		gtk_widget_show(table);

		for (row_num = 0, fl = field_list;
			 row_num < rows && fl != NULL;
			 row_num++)
		{
			for (col_num = 0;
				 col_num < cols && fl != NULL;
				 col_num++, fl = fl->next)
			{
				size_t col_offset = col_num * 2;
				GaimRequestFieldType type;
				GtkWidget *widget = NULL;

				field = fl->data;

				if (!gaim_request_field_is_visible(field)) {
					col_num--;
					continue;
				}

				type = gaim_request_field_get_type(field);

				if (type != GAIM_REQUEST_FIELD_BOOLEAN)
				{
					char *text;

					text = g_strdup_printf("%s:",
						gaim_request_field_get_label(field));

					label = gtk_label_new(NULL);
					gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), text);
					g_free(text);

					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

					gtk_size_group_add_widget(sg, label);

					if (type == GAIM_REQUEST_FIELD_LABEL ||
						(type == GAIM_REQUEST_FIELD_STRING &&
						 gaim_request_field_string_is_multiline(field)))
					{
						if(col_num > 0)
							row_num++;

						gtk_table_attach_defaults(GTK_TABLE(table), label,
												  0, 2 * cols,
												  row_num, row_num + 1);

						row_num++;
						col_num=cols;
					}
					else
					{
						gtk_table_attach_defaults(GTK_TABLE(table), label,
												  col_offset, col_offset + 1,
												  row_num, row_num + 1);
					}

					gtk_widget_show(label);
				}

				if (type == GAIM_REQUEST_FIELD_STRING)
					widget = create_string_field(field);
				else if (type == GAIM_REQUEST_FIELD_INTEGER)
					widget = create_int_field(field);
				else if (type == GAIM_REQUEST_FIELD_BOOLEAN)
					widget = create_bool_field(field);
				else if (type == GAIM_REQUEST_FIELD_CHOICE)
					widget = create_choice_field(field);
				else if (type == GAIM_REQUEST_FIELD_LIST)
					widget = create_list_field(field);
				else if (type == GAIM_REQUEST_FIELD_ACCOUNT)
					widget = create_account_field(field);
				else
					continue;

				if (type == GAIM_REQUEST_FIELD_STRING &&
					gaim_request_field_string_is_multiline(field))
				{
					gtk_table_attach(GTK_TABLE(table), widget,
									 0, 2 * cols,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}
				else if (type != GAIM_REQUEST_FIELD_BOOLEAN)
				{
					gtk_table_attach(GTK_TABLE(table), widget,
									 col_offset + 1, col_offset + 2,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}
				else
				{
					gtk_table_attach(GTK_TABLE(table), widget,
									 col_offset, col_offset + 1,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}

				gtk_widget_show(widget);

				field->ui_data = widget;
			}
		}
	}

	g_object_unref(sg);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Cancel button */
	button = gtk_button_new_from_stock(text_to_stock(cancel_text));
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(multifield_cancel_cb), data);

	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

	/* OK button */
	button = gtk_button_new_from_stock(text_to_stock(ok_text));
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	data->ok_button = button;

	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_window_set_default(GTK_WINDOW(win), button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(multifield_ok_cb), data);

	if (!gaim_request_fields_all_required_filled(fields))
		gtk_widget_set_sensitive(button, FALSE);

	gtk_widget_show(win);

	return data;
}

static void
gaim_gtk_close_request(GaimRequestType type, void *ui_handle)
{
	GaimGtkRequestData *data = (GaimGtkRequestData *)ui_handle;

	if (data->cbs != NULL)
		g_free(data->cbs);

	gtk_widget_destroy(data->dialog);

	if (type == GAIM_REQUEST_FIELDS)
		gaim_request_fields_destroy(data->u.multifield.fields);

	g_free(data);
}

static GaimRequestUiOps ops =
{
	gaim_gtk_request_input,
	gaim_gtk_request_choice,
	gaim_gtk_request_action,
	gaim_gtk_request_fields,
	gaim_gtk_close_request
};

GaimRequestUiOps *
gaim_gtk_request_get_ui_ops(void)
{
	return &ops;
}
