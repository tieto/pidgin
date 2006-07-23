#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntlabel.h>

#include "gntrequest.h"

static GntWidget *
setup_request_window(const char *title, const char *primary,
		const char *secondary)
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

	return window;
}

static GntWidget *
setup_button_box(gpointer userdata, gpointer cb, gpointer data, ...)
{
	GntWidget *box, *button;
	va_list list;
	const char *text;
	gpointer callback;

	box = gnt_hbox_new(TRUE);

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
	GaimRequestInputCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	const char *text = gnt_entry_get_text(GNT_ENTRY(entry));

	if (callback)
		callback(data, text);

	while (button->parent)
		button = button->parent;

	gaim_request_close(GAIM_REQUEST_INPUT, button);
}

static void *
gg_request_input(const char *title, const char *primary,
		const char *secondary, const char *default_value,
		gboolean multiline, gboolean masked, gchar *hint,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		void *user_data)
{
	GntWidget *window, *box, *entry;

	window = setup_request_window(title, primary, secondary);

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
gg_close_request(GaimRequestType type, gpointer ui_handle)
{
	GntWidget *widget = GNT_WIDGET(ui_handle);
	while (widget->parent)
		widget = widget->parent;
	gnt_widget_destroy(widget);
}

static void
request_choice_cb(GntWidget *button, GntComboBox *combo)
{
	GaimRequestChoiceCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int choice = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo))) - 1;

	if (callback)
		callback(data, choice);

	while (button->parent)
		button = button->parent;

	gaim_request_close(GAIM_REQUEST_INPUT, button);
}

static void *
gg_request_choice(const char *title, const char *primary,
		const char *secondary, unsigned int default_value,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		void *user_data, va_list choices)
{
	GntWidget *window, *combo, *box;
	const char *text;
	int val;

	window = setup_request_window(title, primary, secondary);

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
	GaimRequestActionCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "activate-id"));

	callback(data, id);

	gaim_request_close(GAIM_REQUEST_ACTION, window);
}

static void*
gg_request_action(const char *title, const char *primary,
		const char *secondary, unsigned int default_value,
		void *user_data, size_t actioncount,
		va_list actions)
{
	GntWidget *window, *box, *button;
	int i;

	window = setup_request_window(title, primary, secondary);

	box = gnt_hbox_new(TRUE);
	gnt_box_add_widget(GNT_BOX(window), box);
	for (i = 0; i < actioncount; i++)
	{
		const char *text = va_arg(actions, const char *);
		GaimRequestActionCb callback = va_arg(actions, GaimRequestActionCb);

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

static GaimRequestUiOps uiops =
{
	.request_input = gg_request_input,
	.close_request = gg_close_request,
	.request_choice = gg_request_choice,
	.request_action = gg_request_action,
};

GaimRequestUiOps *gg_request_get_ui_ops()
{
	return &uiops;
}

void gg_request_init()
{
}

void gg_request_uninit()
{
}

