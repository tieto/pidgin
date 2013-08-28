/**
 * @file gtkrequest.c GTK+ Request API
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

#include "debug.h"
#include "prefs.h"
#include "util.h"

#include "gtkwebview.h"
#include "gtkrequest.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkblist.h"

#include <gdk/gdkkeysyms.h>

#ifdef ENABLE_GCR
#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>
#if !GTK_CHECK_VERSION(3,0,0)
#include <gcr/gcr-simple-certificate.h>
#endif
#endif

#include "gtk3compat.h"

static GtkWidget * create_account_field(PurpleRequestField *field);

typedef struct
{
	PurpleRequestType type;

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
			gchar *hint;

		} input;

		struct
		{
			PurpleRequestFields *fields;

		} multifield;

		struct
		{
			gboolean savedialog;
			gchar *name;

		} file;

	} u;

} PidginRequestData;

static void
pidgin_widget_decorate_account(GtkWidget *cont, PurpleAccount *account)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	if (!account)
		return;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(G_OBJECT(pixbuf));

	gtk_widget_set_tooltip_text(image, purple_account_get_username(account));

	if (GTK_IS_DIALOG(cont)) {
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(cont))),
	                       image, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(cont))),
	                          image, 0);
	} else if (GTK_IS_HBOX(cont)) {
		gtk_misc_set_alignment(GTK_MISC(image), 0, 0);
		gtk_box_pack_end(GTK_BOX(cont), image, FALSE, TRUE, 0);
	}
	gtk_widget_show(image);
}

static void
generic_response_start(PidginRequestData *data)
{
	g_return_if_fail(data != NULL);

	/* Tell the user we're doing something. */
	pidgin_set_cursor(GTK_WIDGET(data->dialog), GDK_WATCH);
}

static void
input_response_cb(GtkDialog *dialog, gint id, PidginRequestData *data)
{
	const char *value;
	char *multiline_value = NULL;

	generic_response_start(data);

	if (data->u.input.multiline) {
		GtkTextIter start_iter, end_iter;
		GtkTextBuffer *buffer =
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->u.input.entry));

		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(buffer, &end_iter);

		if ((data->u.input.hint != NULL) && (!strcmp(data->u.input.hint, "html")))
			multiline_value = gtk_webview_get_body_html(GTK_WEBVIEW(data->u.input.entry));
		else
			multiline_value = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
										 FALSE);

		value = multiline_value;
	}
	else
		value = gtk_entry_get_text(GTK_ENTRY(data->u.input.entry));

	if (id >= 0 && (gsize)id < data->cb_count && data->cbs[id] != NULL)
		((PurpleRequestInputCb)data->cbs[id])(data->user_data, value);
	else if (data->cbs[1] != NULL)
		((PurpleRequestInputCb)data->cbs[1])(data->user_data, value);

	if (data->u.input.multiline)
		g_free(multiline_value);

	purple_request_close(PURPLE_REQUEST_INPUT, data);
}

static void
action_response_cb(GtkDialog *dialog, gint id, PidginRequestData *data)
{
	generic_response_start(data);

	if (id >= 0 && (gsize)id < data->cb_count && data->cbs[id] != NULL)
		((PurpleRequestActionCb)data->cbs[id])(data->user_data, id);

	purple_request_close(PURPLE_REQUEST_INPUT, data);
}


static void
choice_response_cb(GtkDialog *dialog, gint id, PidginRequestData *data)
{
	GtkWidget *radio = g_object_get_data(G_OBJECT(dialog), "radio");
	GSList *group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));

	generic_response_start(data);

	if (id >= 0 && (gsize)id < data->cb_count && data->cbs[id] != NULL)
		while (group) {
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group->data))) {
				((PurpleRequestChoiceCb)data->cbs[id])(data->user_data, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(group->data), "choice_id")));
				break;
			}
			group = group->next;
		}
	purple_request_close(PURPLE_REQUEST_INPUT, data);
}

static gboolean
field_string_focus_out_cb(GtkWidget *entry, GdkEventFocus *event,
						  PurpleRequestField *field)
{
	const char *value;

	if (purple_request_field_string_is_multiline(field))
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

	purple_request_field_string_set_value(field,
			(*value == '\0' ? NULL : value));

	return FALSE;
}

static void
field_bool_cb(GtkToggleButton *button, PurpleRequestField *field)
{
	purple_request_field_bool_set_value(field,
			gtk_toggle_button_get_active(button));
}

static void
field_choice_menu_cb(GtkComboBox *menu, PurpleRequestField *field)
{
	purple_request_field_choice_set_value(field,
			gtk_combo_box_get_active(menu));
}

static void
field_choice_option_cb(GtkRadioButton *button, PurpleRequestField *field)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		purple_request_field_choice_set_value(field,
				(g_slist_length(gtk_radio_button_get_group(button)) -
				 g_slist_index(gtk_radio_button_get_group(button), button)) - 1);
}

static void
field_account_cb(GObject *w, PurpleAccount *account, PurpleRequestField *field)
{
	purple_request_field_account_set_value(field, account);
}

static void
multifield_ok_cb(GtkWidget *button, PidginRequestData *data)
{
	generic_response_start(data);

	if (!gtk_widget_has_focus(button))
		gtk_widget_grab_focus(button);

	if (data->cbs[0] != NULL)
		((PurpleRequestFieldsCb)data->cbs[0])(data->user_data,
											data->u.multifield.fields);

	purple_request_close(PURPLE_REQUEST_FIELDS, data);
}

static void
multifield_cancel_cb(GtkWidget *button, PidginRequestData *data)
{
	generic_response_start(data);

	if (data->cbs[1] != NULL)
		((PurpleRequestFieldsCb)data->cbs[1])(data->user_data,
											data->u.multifield.fields);

	purple_request_close(PURPLE_REQUEST_FIELDS, data);
}

static gboolean
destroy_multifield_cb(GtkWidget *dialog, GdkEvent *event,
					  PidginRequestData *data)
{
	multifield_cancel_cb(NULL, data);
	return FALSE;
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
	STOCK_ITEMIZE(_("Alias"),   PIDGIN_STOCK_ALIAS);

	return text;
}

static void *
pidgin_request_input(const char *title, const char *primary,
					   const char *secondary, const char *default_value,
					   gboolean multiline, gboolean masked, gchar *hint,
					   const char *ok_text, GCallback ok_cb,
					   const char *cancel_text, GCallback cancel_cb,
					   PurpleAccount *account, const char *who, PurpleConversation *conv,
					   void *user_data)
{
	PidginRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *img;
	char *label_text;
	char *primary_esc, *secondary_esc;

	data            = g_new0(PidginRequestData, 1);
	data->type      = PURPLE_REQUEST_INPUT;
	data->user_data = user_data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;

	/* Create the dialog. */
	dialog = gtk_dialog_new_with_buttons(title ? title : PIDGIN_ALERT_TITLE,
					     NULL, 0,
					     text_to_stock(cancel_text), 1,
					     text_to_stock(ok_text),     0,
					     NULL);
	data->dialog = dialog;

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(input_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), PIDGIN_HIG_BORDER/2);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                               PIDGIN_HIG_BORDER / 2);
	if (!multiline)
		gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), 0);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                    PIDGIN_HIG_BORDER);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                  hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
					gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	pidgin_widget_decorate_account(hbox, account);

	/* Descriptive label */
	primary_esc = (primary != NULL) ? g_markup_escape_text(primary, -1) : NULL;
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	label_text = g_strdup_printf((primary ? "<span weight=\"bold\" size=\"larger\">"
								 "%s</span>%s%s" : "%s%s%s"),
								 (primary ? primary_esc : ""),
								 ((primary && secondary) ? "\n\n" : ""),
								 (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	g_free(label_text);

	/* Entry field. */
	data->u.input.multiline = multiline;
	data->u.input.hint = g_strdup(hint);

	gtk_widget_show_all(hbox);

	if ((data->u.input.hint != NULL) && (!strcmp(data->u.input.hint, "html"))) {
		GtkWidget *frame;

		/* webview */
		frame = pidgin_create_webview(TRUE, &entry, NULL);
		gtk_widget_set_size_request(entry, 320, 130);
		gtk_widget_set_name(entry, "pidgin_request_webview");
		if (default_value != NULL)
			gtk_webview_append_html(GTK_WEBVIEW(entry), default_value);
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show(frame);
	}
	else {
		if (multiline) {
			/* GtkTextView */
			entry = gtk_text_view_new();
			gtk_text_view_set_editable(GTK_TEXT_VIEW(entry), TRUE);

			if (default_value != NULL) {
				GtkTextBuffer *buffer;

				buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
				gtk_text_buffer_set_text(buffer, default_value, -1);
			}

			gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(entry), GTK_WRAP_WORD_CHAR);

			if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck"))
				pidgin_setup_gtkspell(GTK_TEXT_VIEW(entry));

			gtk_box_pack_start(GTK_BOX(vbox), 
				pidgin_make_scrollable(entry, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, 320, 130),
				TRUE, TRUE, 0);
		}
		else {
			entry = gtk_entry_new();

			gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

			gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

			if (default_value != NULL)
				gtk_entry_set_text(GTK_ENTRY(entry), default_value);

			if (masked)
			{
				gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
			}
		}
		gtk_widget_show_all(vbox);
	}

	pidgin_set_accessible_label (entry, label);
	data->u.input.entry = entry;

	pidgin_auto_parent_window(dialog);

	/* Show everything. */
	gtk_widget_show(dialog);

	return data;
}

static void *
pidgin_request_choice(const char *title, const char *primary,
			const char *secondary, int default_value,
			const char *ok_text, GCallback ok_cb,
			const char *cancel_text, GCallback cancel_cb,
			PurpleAccount *account, const char *who, PurpleConversation *conv,
			void *user_data, va_list args)
{
	PidginRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	GtkWidget *radio = NULL;
	char *label_text;
	char *radio_text;
	char *primary_esc, *secondary_esc;

	data            = g_new0(PidginRequestData, 1);
	data->type      = PURPLE_REQUEST_ACTION;
	data->user_data = user_data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);
	data->cbs[0] = cancel_cb;
	data->cbs[1] = ok_cb;

	/* Create the dialog. */
	data->dialog = dialog = gtk_dialog_new();

	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(dialog), title);
#ifdef _WIN32
		gtk_window_set_title(GTK_WINDOW(dialog), PIDGIN_ALERT_TITLE);
#endif

	gtk_dialog_add_button(GTK_DIALOG(dialog),
			      text_to_stock(cancel_text), 0);

	gtk_dialog_add_button(GTK_DIALOG(dialog),
			      text_to_stock(ok_text), 1);

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(choice_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), PIDGIN_HIG_BORDER/2);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                               PIDGIN_HIG_BORDER / 2);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                    PIDGIN_HIG_BORDER);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                  hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
				       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	pidgin_widget_decorate_account(hbox, account);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	primary_esc = (primary != NULL) ? g_markup_escape_text(primary, -1) : NULL;
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	label_text = g_strdup_printf((primary ? "<span weight=\"bold\" size=\"larger\">"
				      "%s</span>%s%s" : "%s%s%s"),
				     (primary ? primary_esc : ""),
				     ((primary && secondary) ? "\n\n" : ""),
				     (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);
	while ((radio_text = va_arg(args, char*))) {
		       int resp = va_arg(args, int);
		       radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), radio_text);
		       gtk_box_pack_start(GTK_BOX(vbox2), radio, FALSE, FALSE, 0);
		       g_object_set_data(G_OBJECT(radio), "choice_id", GINT_TO_POINTER(resp));
		       if (resp == default_value)
			       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
	}

	g_object_set_data(G_OBJECT(dialog), "radio", radio);

	/* Show everything. */
	pidgin_auto_parent_window(dialog);

	gtk_widget_show_all(dialog);

	return data;
}

static void *
pidgin_request_action_with_icon(const char *title, const char *primary,
						const char *secondary, int default_action,
					    PurpleAccount *account, const char *who,
						PurpleConversation *conv, gconstpointer icon_data,
						gsize icon_size,
						void *user_data, size_t action_count, va_list actions)
{
	PidginRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img = NULL;
	void **buttons;
	char *label_text;
	char *primary_esc, *secondary_esc;
	gsize i;

	data            = g_new0(PidginRequestData, 1);
	data->type      = PURPLE_REQUEST_ACTION;
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

	gtk_window_set_deletable(GTK_WINDOW(data->dialog), FALSE);

	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(dialog), title);
#ifdef _WIN32
	else
		gtk_window_set_title(GTK_WINDOW(dialog), PIDGIN_ALERT_TITLE);
#endif

	for (i = 0; i < action_count; i++) {
		gtk_dialog_add_button(GTK_DIALOG(dialog),
							  text_to_stock(buttons[2 * i]), i);

		data->cbs[i] = buttons[2 * i + 1];
	}

	g_free(buttons);

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(action_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), PIDGIN_HIG_BORDER/2);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                               PIDGIN_HIG_BORDER / 2);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                    PIDGIN_HIG_BORDER);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                  hbox);

	/* Dialog icon. */
	if (icon_data) {
		GdkPixbuf *pixbuf = pidgin_pixbuf_from_data(icon_data, icon_size);
		if (pixbuf) {
			/* scale the image if it is too large */
			int width = gdk_pixbuf_get_width(pixbuf);
			int height = gdk_pixbuf_get_height(pixbuf);
			if (width > 128 || height > 128) {
				int scaled_width = width > height ? 128 : (128 * width) / height;
				int scaled_height = height > width ? 128 : (128 * height) / width;
				GdkPixbuf *scaled =
						gdk_pixbuf_scale_simple(pixbuf, scaled_width, scaled_height,
						    GDK_INTERP_BILINEAR);

				purple_debug_info("pidgin",
				    "dialog icon was too large, scaled it down\n");
				if (scaled) {
					g_object_unref(pixbuf);
					pixbuf = scaled;
				}
			}
			img = gtk_image_new_from_pixbuf(pixbuf);
			g_object_unref(pixbuf);
		} else {
			purple_debug_info("pidgin", "failed to parse dialog icon\n");
		}
	}

	if (!img) {
		img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
				       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	}
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	pidgin_widget_decorate_account(hbox, account);

	/* Descriptive label */
	primary_esc = (primary != NULL) ? g_markup_escape_text(primary, -1) : NULL;
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	label_text = g_strdup_printf((primary ? "<span weight=\"bold\" size=\"larger\">"
								 "%s</span>%s%s" : "%s%s%s"),
								 (primary ? primary_esc : ""),
								 ((primary && secondary) ? "\n\n" : ""),
								 (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);


	if (default_action == PURPLE_DEFAULT_ACTION_NONE) {
		gtk_widget_set_can_default(img, TRUE);
		gtk_widget_set_can_focus(img, TRUE);
		gtk_widget_grab_focus(img);
		gtk_widget_grab_default(img);
	} else
		/*
		 * Need to invert the default_action number because the
		 * buttons are added to the dialog in reverse order.
		 */
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), action_count - 1 - default_action);

	/* Show everything. */
	pidgin_auto_parent_window(dialog);

	gtk_widget_show_all(dialog);

	return data;
}

static void *
pidgin_request_action(const char *title, const char *primary,
						const char *secondary, int default_action,
					    PurpleAccount *account, const char *who, PurpleConversation *conv,
						void *user_data, size_t action_count, va_list actions)
{
	return pidgin_request_action_with_icon(title, primary, secondary,
		default_action, account, who, conv, NULL, 0, user_data, action_count,
		actions);
}

static void
req_entry_field_changed_cb(GtkWidget *entry, PurpleRequestField *field)
{
	if (purple_request_field_get_type(field) == PURPLE_REQUEST_FIELD_INTEGER) {
		int value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(entry));
		purple_request_field_int_set_value(field, value);
		return;
	}

	if (purple_request_field_string_is_multiline(field))
	{
		char *text;
		GtkTextIter start_iter, end_iter;

		gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(entry), &start_iter);
		gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(entry), &end_iter);

		text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(entry), &start_iter, &end_iter, FALSE);
		purple_request_field_string_set_value(field, (!text || !*text) ? NULL : text);
		g_free(text);
	}
	else
	{
		const char *text = NULL;
		text = gtk_entry_get_text(GTK_ENTRY(entry));
		purple_request_field_string_set_value(field, (*text == '\0') ? NULL : text);
	}
}

static void
req_field_changed_cb(GtkWidget *widget, PurpleRequestField *field)
{
	PurpleRequestFieldGroup *group;
	PurpleRequestFields *fields;
	PidginRequestData *req_data;

	group = purple_request_field_get_group(field);
	fields = purple_request_field_group_get_fields_list(group);
	req_data = purple_request_fields_get_ui_data(fields);

	gtk_widget_set_sensitive(req_data->ok_button,
		purple_request_fields_all_required_filled(fields) &&
		purple_request_fields_all_valid(fields));
}

static void
setup_entry_field(GtkWidget *entry, PurpleRequestField *field)
{
	const char *type_hint;

	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	g_signal_connect(G_OBJECT(entry), "changed",
		G_CALLBACK(req_entry_field_changed_cb), field);
	g_signal_connect(G_OBJECT(entry), "changed",
		G_CALLBACK(req_field_changed_cb), field);

	if ((type_hint = purple_request_field_get_type_hint(field)) != NULL)
	{
		if (purple_str_has_prefix(type_hint, "screenname"))
		{
			GtkWidget *optmenu = NULL;
			PurpleRequestFieldGroup *group = purple_request_field_get_group(field);
			GList *fields = purple_request_field_group_get_fields(group);

			/* Ensure the account option menu is created (if the widget hasn't
			 * been initialized already) for username auto-completion. */
			while (fields)
			{
				PurpleRequestField *fld = fields->data;
				fields = fields->next;

				if (purple_request_field_get_type(fld) == PURPLE_REQUEST_FIELD_ACCOUNT &&
						purple_request_field_is_visible(fld))
				{
					const char *type_hint = purple_request_field_get_type_hint(fld);
					if (type_hint != NULL && strcmp(type_hint, "account") == 0)
					{
						optmenu = GTK_WIDGET(purple_request_field_get_ui_data(fld));
						if (optmenu == NULL) {
							optmenu = GTK_WIDGET(create_account_field(fld));
							purple_request_field_set_ui_data(fld, optmenu);
						}
						break;
					}
				}
			}
			pidgin_setup_screenname_autocomplete(entry, optmenu, pidgin_screenname_autocomplete_default_filter, GINT_TO_POINTER(!strcmp(type_hint, "screenname-all")));
		}
	}
}

static GtkWidget *
create_string_field(PurpleRequestField *field)
{
	const char *value;
	GtkWidget *widget;

	value = purple_request_field_string_get_default_value(field);

	if (purple_request_field_string_is_multiline(field))
	{
		GtkWidget *textview;

		textview = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(textview),
								   TRUE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview),
									GTK_WRAP_WORD_CHAR);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck"))
			pidgin_setup_gtkspell(GTK_TEXT_VIEW(textview));

		gtk_widget_show(textview);

		if (value != NULL)
		{
			GtkTextBuffer *buffer;

			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

			gtk_text_buffer_set_text(buffer, value, -1);
		}

		gtk_widget_set_tooltip_text(textview, purple_request_field_get_tooltip(field));

		gtk_text_view_set_editable(GTK_TEXT_VIEW(textview),
			purple_request_field_string_is_editable(field));

		g_signal_connect(G_OBJECT(textview), "focus-out-event",
						 G_CALLBACK(field_string_focus_out_cb), field);

	    if (purple_request_field_is_required(field))
	    {
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
			g_signal_connect(G_OBJECT(buffer), "changed",
							 G_CALLBACK(req_entry_field_changed_cb), field);
	    }

		widget = pidgin_make_scrollable(textview, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, 75);
	}
	else
	{
		widget = gtk_entry_new();

		setup_entry_field(widget, field);

		if (value != NULL)
			gtk_entry_set_text(GTK_ENTRY(widget), value);

		gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

		if (purple_request_field_string_is_masked(field))
		{
			gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
		}

		gtk_editable_set_editable(GTK_EDITABLE(widget),
			purple_request_field_string_is_editable(field));

		g_signal_connect(G_OBJECT(widget), "focus-out-event",
						 G_CALLBACK(field_string_focus_out_cb), field);
	}

	return widget;
}

static GtkWidget *
create_int_field(PurpleRequestField *field)
{
	int value;
	GtkWidget *widget;

	widget = gtk_spin_button_new_with_range(
		purple_request_field_int_get_lower_bound(field),
		purple_request_field_int_get_upper_bound(field), 1);

	setup_entry_field(widget, field);

	value = purple_request_field_int_get_default_value(field);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), value);

	gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

	return widget;
}

static GtkWidget *
create_bool_field(PurpleRequestField *field)
{
	GtkWidget *widget;

	widget = gtk_check_button_new_with_label(
		purple_request_field_get_label(field));

	gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		purple_request_field_bool_get_default_value(field));

	g_signal_connect(G_OBJECT(widget), "toggled",
					 G_CALLBACK(field_bool_cb), field);
	g_signal_connect(widget, "toggled",
		G_CALLBACK(req_field_changed_cb), field);

	return widget;
}

static GtkWidget *
create_choice_field(PurpleRequestField *field)
{
	GtkWidget *widget;
	GList *labels = purple_request_field_choice_get_labels(field);
	int num_labels = g_list_length(labels);
	GList *l;

	if (num_labels > 5)
	{
		widget = gtk_combo_box_text_new();

		for (l = labels; l != NULL; l = l->next)
		{
			const char *text = l->data;
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), text);
		}

		gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
						purple_request_field_choice_get_default_value(field));

		gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

		g_signal_connect(G_OBJECT(widget), "changed",
						 G_CALLBACK(field_choice_menu_cb), field);
	}
	else
	{
		GtkWidget *box;
		GtkWidget *first_radio = NULL;
		GtkWidget *radio;
		gint i;

		if (num_labels == 2)
			box = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		else
			box = gtk_vbox_new(FALSE, 0);

		widget = box;

		gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

		for (l = labels, i = 0; l != NULL; l = l->next, i++)
		{
			const char *text = l->data;

			radio = gtk_radio_button_new_with_label_from_widget(
				GTK_RADIO_BUTTON(first_radio), text);

			if (first_radio == NULL)
				first_radio = radio;

			if (i == purple_request_field_choice_get_default_value(field))
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);

			gtk_box_pack_start(GTK_BOX(box), radio, TRUE, TRUE, 0);
			gtk_widget_show(radio);

			g_signal_connect(G_OBJECT(radio), "toggled",
							 G_CALLBACK(field_choice_option_cb), field);
		}
	}

	return widget;
}

static GtkWidget *
create_image_field(PurpleRequestField *field)
{
	GtkWidget *widget;
	GdkPixbuf *buf, *scale;

	buf = pidgin_pixbuf_from_data(
			(const guchar *)purple_request_field_image_get_buffer(field),
			purple_request_field_image_get_size(field));

	scale = gdk_pixbuf_scale_simple(buf,
			purple_request_field_image_get_scale_x(field) * gdk_pixbuf_get_width(buf),
			purple_request_field_image_get_scale_y(field) * gdk_pixbuf_get_height(buf),
			GDK_INTERP_BILINEAR);
	widget = gtk_image_new_from_pixbuf(scale);
	g_object_unref(G_OBJECT(buf));
	g_object_unref(G_OBJECT(scale));

	gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));

	return widget;
}

static GtkWidget *
create_account_field(PurpleRequestField *field)
{
	GtkWidget *widget;

	widget = pidgin_account_option_menu_new(
		purple_request_field_account_get_default_value(field),
		purple_request_field_account_get_show_all(field),
		G_CALLBACK(field_account_cb),
		purple_request_field_account_get_filter(field),
		field);

	gtk_widget_set_tooltip_text(widget, purple_request_field_get_tooltip(field));
	g_signal_connect(widget, "changed",
		G_CALLBACK(req_field_changed_cb), field);

	return widget;
}

static void
select_field_list_item(GtkTreeModel *model, GtkTreePath *path,
					   GtkTreeIter *iter, gpointer data)
{
	PurpleRequestField *field = (PurpleRequestField *)data;
	char *text;

	gtk_tree_model_get(model, iter, 1, &text, -1);

	purple_request_field_list_add_selected(field, text);
	g_free(text);
}

static void
list_field_select_changed_cb(GtkTreeSelection *sel, PurpleRequestField *field)
{
	purple_request_field_list_clear_selected(field);

	gtk_tree_selection_selected_foreach(sel, select_field_list_item, field);
}

static GtkWidget *
create_list_field(PurpleRequestField *field)
{
	GtkWidget *treeview;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GList *l;
	GList *icons = NULL;

	icons = purple_request_field_list_get_icons(field);


	/* Create the list store */
	if (icons)
		store = gtk_list_store_new(3, G_TYPE_POINTER, G_TYPE_STRING, GDK_TYPE_PIXBUF);
	else
		store = gtk_list_store_new(2, G_TYPE_POINTER, G_TYPE_STRING);

	/* Create the tree view */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	if (purple_request_field_list_get_multi_select(field))
		gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", 1);

	if (icons)
	{
		renderer = gtk_cell_renderer_pixbuf_new();
		gtk_tree_view_column_pack_start(column, renderer, TRUE);
		gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", 2);

		gtk_widget_set_size_request(treeview, 200, 400);
	}

	for (l = purple_request_field_list_get_items(field); l != NULL; l = l->next)
	{
		const char *text = (const char *)l->data;

		gtk_list_store_append(store, &iter);

		if (icons)
		{
			const char *icon_path = (const char *)icons->data;
			GdkPixbuf* pixbuf = NULL;

			if (icon_path)
				pixbuf = pidgin_pixbuf_new_from_file(icon_path);

			gtk_list_store_set(store, &iter,
						   0, purple_request_field_list_get_data(field, text),
						   1, text,
						   2, pixbuf,
						   -1);
			icons = icons->next;
		}
		else
			gtk_list_store_set(store, &iter,
						   0, purple_request_field_list_get_data(field, text),
						   1, text,
						   -1);

		if (purple_request_field_list_is_selected(field, text))
			gtk_tree_selection_select_iter(sel, &iter);
	}

	/*
	 * We only want to catch changes made by the user, so it's important
	 * that we wait until after the list is created to connect this
	 * handler.  If we connect the handler before the loop above and
	 * there are multiple items selected, then selecting the first iter
	 * in the tree causes list_field_select_changed_cb to be triggered
	 * which clears out the rest of the list of selected items.
	 */
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(list_field_select_changed_cb), field);

	gtk_widget_show(treeview);

	return pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1);
}

static GtkWidget *
create_certificate_field(PurpleRequestField *field)
{
	PurpleCertificate *cert;
#ifdef ENABLE_GCR
	GcrCertificateBasicsWidget *cert_widget;
	GByteArray *der;
	GcrCertificate *gcrt;
#else
	GtkWidget *cert_label;
	char *str;
	char *escaped;
#endif

	cert = purple_request_field_certificate_get_value(field);

#ifdef ENABLE_GCR
	der = purple_certificate_get_der_data(cert);
	g_return_val_if_fail(der, NULL);

	gcrt = gcr_simple_certificate_new(der->data, der->len);
	g_return_val_if_fail(gcrt, NULL);

	cert_widget = gcr_certificate_basics_widget_new(gcrt);

	g_byte_array_free(der, TRUE);
	g_object_unref(G_OBJECT(gcrt));

	return GTK_WIDGET(cert_widget);
#else
	str = purple_certificate_get_display_string(cert);
	escaped = g_markup_escape_text(str, -1);

	cert_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(cert_label), escaped);
	gtk_label_set_line_wrap(GTK_LABEL(cert_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(cert_label), 0, 0);

	g_free(str);
	g_free(escaped);

	return cert_label;
#endif
}

static void *
pidgin_request_fields(const char *title, const char *primary,
						const char *secondary, PurpleRequestFields *fields,
						const char *ok_text, GCallback ok_cb,
						const char *cancel_text, GCallback cancel_cb,
					    PurpleAccount *account, const char *who, PurpleConversation *conv,
						void *user_data)
{
	PidginRequestData *data;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *button;
	GtkWidget *img;
	GtkSizeGroup *sg;
	GList *gl, *fl;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	char *label_text;
	char *primary_esc, *secondary_esc;
	int total_fields = 0;

	data            = g_new0(PidginRequestData, 1);
	data->type      = PURPLE_REQUEST_FIELDS;
	data->user_data = user_data;
	data->u.multifield.fields = fields;

	purple_request_fields_set_ui_data(fields, data);

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;


#ifdef _WIN32
	data->dialog = win = pidgin_create_dialog(PIDGIN_ALERT_TITLE, PIDGIN_HIG_BORDER, "multifield", TRUE) ;
#else /* !_WIN32 */
	data->dialog = win = pidgin_create_dialog(title, PIDGIN_HIG_BORDER, "multifield", TRUE) ;
#endif /* _WIN32 */

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(destroy_multifield_cb), data);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(pidgin_dialog_get_vbox(GTK_DIALOG(win))), hbox);
	gtk_widget_show(hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
					gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_widget_show(img);

	/* Cancel button */
	button = pidgin_dialog_add_button(GTK_DIALOG(win), text_to_stock(cancel_text), G_CALLBACK(multifield_cancel_cb), data);
	gtk_widget_set_can_default(button, TRUE);

	/* OK button */
	button = pidgin_dialog_add_button(GTK_DIALOG(win), text_to_stock(ok_text), G_CALLBACK(multifield_ok_cb), data);
	data->ok_button = button;
	gtk_widget_set_can_default(button, TRUE);
	gtk_window_set_default(GTK_WINDOW(win), button);

	pidgin_widget_decorate_account(hbox, account);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if(primary) {
		primary_esc = g_markup_escape_text(primary, -1);
		label_text = g_strdup_printf(
				"<span weight=\"bold\" size=\"larger\">%s</span>", primary_esc);
		g_free(primary_esc);
		label = gtk_label_new(NULL);

		gtk_label_set_markup(GTK_LABEL(label), label_text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		g_free(label_text);
	}

	for (gl = purple_request_fields_get_groups(fields); gl != NULL;
			gl = gl->next)
		total_fields += g_list_length(purple_request_field_group_get_fields(gl->data));

	if(total_fields > 9) {
		GtkWidget *hbox_for_spacing, *vbox_for_spacing;

		hbox_for_spacing = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(vbox), 
			pidgin_make_scrollable(hbox_for_spacing, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC, GTK_SHADOW_NONE, -1, 200), 
			TRUE, TRUE, 0);
		gtk_widget_show(hbox_for_spacing);

		vbox_for_spacing = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(hbox_for_spacing),
				vbox_for_spacing, TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);
		gtk_widget_show(vbox_for_spacing);

		vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(vbox_for_spacing),
				vbox2, TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);
		gtk_widget_show(vbox2);
	} else {
		vbox2 = vbox;
	}

	if (secondary) {
		secondary_esc = g_markup_escape_text(secondary, -1);
		label = gtk_label_new(NULL);

		gtk_label_set_markup(GTK_LABEL(label), secondary_esc);
		g_free(secondary_esc);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
		gtk_widget_show(label);
	}

	for (gl = purple_request_fields_get_groups(fields);
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
		field_list = purple_request_field_group_get_fields(group);

		if (purple_request_field_group_get_title(group) != NULL)
		{
			frame = pidgin_make_frame(vbox2,
				purple_request_field_group_get_title(group));
		}
		else
			frame = vbox2;

		field_count = g_list_length(field_list);
/*
		if (field_count > 9)
		{
			rows = field_count / 2;
			cols++;
		}
		else
		*/
			rows = field_count;

		col_num = 0;

		for (fl = field_list; fl != NULL; fl = fl->next)
		{
			PurpleRequestFieldType type;

			field = (PurpleRequestField *)fl->data;

			type = purple_request_field_get_type(field);

			if (type == PURPLE_REQUEST_FIELD_LABEL)
			{
				if (col_num > 0)
					rows++;

				rows++;
			}
			else if ((type == PURPLE_REQUEST_FIELD_LIST) ||
				 (type == PURPLE_REQUEST_FIELD_STRING &&
				  purple_request_field_string_is_multiline(field)))
			{
				if (col_num > 0)
					rows++;

				rows += 2;
			}

			col_num++;

			if (col_num >= cols)
				col_num = 0;
		}

		table = gtk_table_new(rows, 2 * cols, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);
		gtk_table_set_col_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);

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
				PurpleRequestFieldType type;
				GtkWidget *widget = NULL;
				const char *field_label;

				label = NULL;
				field = fl->data;

				if (!purple_request_field_is_visible(field)) {
					col_num--;
					continue;
				}

				type = purple_request_field_get_type(field);
				field_label = purple_request_field_get_label(field);

				if (type != PURPLE_REQUEST_FIELD_BOOLEAN && field_label)
				{
					char *text = NULL;

					if (field_label[strlen(field_label) - 1] != ':')
						text = g_strdup_printf("%s:", field_label);

					label = gtk_label_new(NULL);
					gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), text ? text : field_label);
					g_free(text);

					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

					gtk_size_group_add_widget(sg, label);

					if (type == PURPLE_REQUEST_FIELD_LABEL ||
					    type == PURPLE_REQUEST_FIELD_LIST ||
						(type == PURPLE_REQUEST_FIELD_STRING &&
						 purple_request_field_string_is_multiline(field)))
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

				widget = GTK_WIDGET(purple_request_field_get_ui_data(field));
				if (widget == NULL)
				{
					if (type == PURPLE_REQUEST_FIELD_STRING)
						widget = create_string_field(field);
					else if (type == PURPLE_REQUEST_FIELD_INTEGER)
						widget = create_int_field(field);
					else if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
						widget = create_bool_field(field);
					else if (type == PURPLE_REQUEST_FIELD_CHOICE)
						widget = create_choice_field(field);
					else if (type == PURPLE_REQUEST_FIELD_LIST)
						widget = create_list_field(field);
					else if (type == PURPLE_REQUEST_FIELD_IMAGE)
						widget = create_image_field(field);
					else if (type == PURPLE_REQUEST_FIELD_ACCOUNT)
						widget = create_account_field(field);
					else if (type == PURPLE_REQUEST_FIELD_CERTIFICATE)
						widget = create_certificate_field(field);
					else
						continue;
				}

				if (label)
					gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);

				if (type == PURPLE_REQUEST_FIELD_STRING &&
					purple_request_field_string_is_multiline(field))
				{
					gtk_table_attach(GTK_TABLE(table), widget,
									 0, 2 * cols,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}
				else if (type == PURPLE_REQUEST_FIELD_LIST)
				{
									gtk_table_attach(GTK_TABLE(table), widget,
									0, 2 * cols,
									row_num, row_num + 1,
									GTK_FILL | GTK_EXPAND,
									GTK_FILL | GTK_EXPAND,
									5, 0);
				}
				else if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
				{
					gtk_table_attach(GTK_TABLE(table), widget,
									 col_offset, col_offset + 1,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}
				else
				{
					gtk_table_attach(GTK_TABLE(table), widget,
							 		 1, 2 * cols,
									 row_num, row_num + 1,
									 GTK_FILL | GTK_EXPAND,
									 GTK_FILL | GTK_EXPAND,
									 5, 0);
				}

				gtk_widget_show(widget);

				purple_request_field_set_ui_data(field, widget);
			}
		}
	}

	g_object_unref(sg);

	if (!purple_request_fields_all_required_filled(fields))
		gtk_widget_set_sensitive(data->ok_button, FALSE);

	if (!purple_request_fields_all_valid(fields))
		gtk_widget_set_sensitive(data->ok_button, FALSE);

	pidgin_auto_parent_window(win);

	gtk_widget_show(win);

	return data;
}

static void
file_yes_no_cb(PidginRequestData *data, gint id)
{
	/* Only call the callback if yes was selected, otherwise the request
	 * (eg. file transfer) will be cancelled, then when a new filename is chosen
	 * things go BOOM */
	if (id == 1) {
		if (data->cbs[1] != NULL)
			((PurpleRequestFileCb)data->cbs[1])(data->user_data, data->u.file.name);
		purple_request_close(data->type, data);
	} else {
		pidgin_clear_cursor(GTK_WIDGET(data->dialog));
	}
}

static void
file_ok_check_if_exists_cb(GtkWidget *widget, gint response, PidginRequestData *data)
{
	gchar *current_folder;

	generic_response_start(data);

	if (response != GTK_RESPONSE_ACCEPT) {
		if (data->cbs[0] != NULL)
			((PurpleRequestFileCb)data->cbs[0])(data->user_data, NULL);
		purple_request_close(data->type, data);
		return;
	}

	data->u.file.name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(data->dialog));
	current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(data->dialog));
	if (current_folder != NULL) {
		if (data->u.file.savedialog) {
			purple_prefs_set_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder", current_folder);
		} else {
			purple_prefs_set_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder", current_folder);
		}
		g_free(current_folder);
	}
	if ((data->u.file.savedialog == TRUE) &&
		(g_file_test(data->u.file.name, G_FILE_TEST_EXISTS))) {
		purple_request_action(data, NULL, _("That file already exists"),
							_("Would you like to overwrite it?"), 0,
							NULL, NULL, NULL,
							data, 2,
							_("Overwrite"), G_CALLBACK(file_yes_no_cb),
							_("Choose New Name"), G_CALLBACK(file_yes_no_cb));
	} else
		file_yes_no_cb(data, 1);
}

static void *
pidgin_request_file(const char *title, const char *filename,
					  gboolean savedialog,
					  GCallback ok_cb, GCallback cancel_cb,
					  PurpleAccount *account, const char *who, PurpleConversation *conv,
					  void *user_data)
{
	PidginRequestData *data;
	GtkWidget *filesel;
#ifdef _WIN32
	const gchar *current_folder;
	gboolean folder_set = FALSE;
#endif

	data = g_new0(PidginRequestData, 1);
	data->type = PURPLE_REQUEST_FILE;
	data->user_data = user_data;
	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);
	data->cbs[0] = cancel_cb;
	data->cbs[1] = ok_cb;
	data->u.file.savedialog = savedialog;

	filesel = gtk_file_chooser_dialog_new(
						title ? title : (savedialog ? _("Save File...")
													: _("Open File...")),
						NULL,
						savedialog ? GTK_FILE_CHOOSER_ACTION_SAVE
								   : GTK_FILE_CHOOSER_ACTION_OPEN,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						savedialog ? GTK_STOCK_SAVE
								   : GTK_STOCK_OPEN,
						GTK_RESPONSE_ACCEPT,
						NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(filesel), GTK_RESPONSE_ACCEPT);

	if ((filename != NULL) && (*filename != '\0')) {
		if (savedialog)
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filesel), filename);
		else if (g_file_test(filename, G_FILE_TEST_EXISTS))
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel), filename);
	}

#ifdef _WIN32

	if (savedialog) {
		current_folder = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder");
	} else {
		current_folder = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder");
	}

	if ((filename == NULL || *filename == '\0' || !g_file_test(filename, G_FILE_TEST_EXISTS)) &&
				(current_folder != NULL) && (*current_folder != '\0')) {
		folder_set = gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filesel), current_folder);
	}

	if (!folder_set && (filename == NULL || *filename == '\0' || !g_file_test(filename, G_FILE_TEST_EXISTS))) {
		char *my_documents = wpurple_get_special_folder(CSIDL_PERSONAL);

		if (my_documents != NULL) {
			gtk_file_chooser_set_current_folder(
					GTK_FILE_CHOOSER(filesel), my_documents);

			g_free(my_documents);
		}
	}

#endif
	g_signal_connect(G_OBJECT(GTK_FILE_CHOOSER(filesel)), "response",
					 G_CALLBACK(file_ok_check_if_exists_cb), data);

	pidgin_auto_parent_window(filesel);

	data->dialog = filesel;
	gtk_widget_show(filesel);

	return (void *)data;
}

static void *
pidgin_request_folder(const char *title, const char *dirname,
					  GCallback ok_cb, GCallback cancel_cb,
					  PurpleAccount *account, const char *who, PurpleConversation *conv,
					  void *user_data)
{
	PidginRequestData *data;
	GtkWidget *dirsel;

	data = g_new0(PidginRequestData, 1);
	data->type = PURPLE_REQUEST_FOLDER;
	data->user_data = user_data;
	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);
	data->cbs[0] = cancel_cb;
	data->cbs[1] = ok_cb;
	data->u.file.savedialog = FALSE;

	dirsel = gtk_file_chooser_dialog_new(
						title ? title : _("Select Folder..."),
						NULL,
						GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dirsel), GTK_RESPONSE_ACCEPT);

	if ((dirname != NULL) && (*dirname != '\0'))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dirsel), dirname);

	g_signal_connect(G_OBJECT(GTK_FILE_CHOOSER(dirsel)), "response",
						G_CALLBACK(file_ok_check_if_exists_cb), data);

	data->dialog = dirsel;
	pidgin_auto_parent_window(dirsel);

	gtk_widget_show(dirsel);

	return (void *)data;
}

#ifdef _WIN32

/* Not needed (yet) for non-win32, but should work everywhere. */
static void
pidgin_window_detach_children(GtkWindow* parent)
{
	GList *it;

	g_return_if_fail(parent != NULL);

	it = gtk_window_list_toplevels();
	for (it = g_list_first(it); it != NULL; it = g_list_next(it)) {
		GtkWindow *win = GTK_WINDOW(it->data);
		if (gtk_window_get_transient_for(win) == parent)
			gtk_window_set_transient_for(win, NULL);
	}
}

#endif

static void
pidgin_close_request(PurpleRequestType type, void *ui_handle)
{
	PidginRequestData *data = (PidginRequestData *)ui_handle;

	g_free(data->cbs);

#ifdef _WIN32
	/* Win32 gtk ignores gtk_window_set_destroy_with_parent(..., FALSE). */
	pidgin_window_detach_children(GTK_WINDOW(data->dialog));
#endif

	gtk_widget_destroy(data->dialog);

	if (type == PURPLE_REQUEST_FIELDS)
		purple_request_fields_destroy(data->u.multifield.fields);
	else if (type == PURPLE_REQUEST_FILE)
		g_free(data->u.file.name);

	g_free(data);
}

static PurpleRequestUiOps ops =
{
	pidgin_request_input,
	pidgin_request_choice,
	pidgin_request_action,
	pidgin_request_fields,
	pidgin_request_file,
	pidgin_close_request,
	pidgin_request_folder,
	pidgin_request_action_with_icon,
	NULL,
	NULL,
	NULL
};

PurpleRequestUiOps *
pidgin_request_get_ui_ops(void)
{
	return &ops;
}
