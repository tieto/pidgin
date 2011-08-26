/**
 * @file gntroomlist.c GNT Room List API
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

#include "finch.h"
#include <internal.h>

#include "gntrequest.h"
#include "gntroomlist.h"

#include "gntbox.h"
#include "gntbutton.h"
#include "gntcombobox.h"
#include "gnttextview.h"
#include "gnttree.h"
#include "gntwindow.h"

#include "debug.h"

#define PREF_ROOT "/finch/roomlist"


/* Yes, just one roomlist at a time. Let's not get greedy. Aight? */
struct _FinchRoomlist
{
	GntWidget *window;

	GntWidget *accounts;
	GntWidget *tree;
	GntWidget *details;

	GntWidget *getlist;
	GntWidget *add;
	GntWidget *join;
	GntWidget *stop;
	GntWidget *close;

	PurpleAccount *account;
	PurpleRoomlist *roomlist;
} froomlist;

typedef struct _FinchRoomlist FinchRoomlist;

static void
unset_roomlist(gpointer null)
{
	froomlist.window = NULL;
	if (froomlist.roomlist) {
		purple_roomlist_unref(froomlist.roomlist);
		froomlist.roomlist = NULL;
	}
	froomlist.account = NULL;
	froomlist.tree = NULL;
}

static void
update_roomlist(PurpleRoomlist *list)
{
	if (froomlist.roomlist == list)
		return;

	if (froomlist.roomlist)
		purple_roomlist_unref(froomlist.roomlist);

	if ((froomlist.roomlist = list) != NULL)
		purple_roomlist_ref(list);
}

static void fl_stop(GntWidget *button, gpointer null)
{
	if (froomlist.roomlist &&
			purple_roomlist_get_in_progress(froomlist.roomlist))
		purple_roomlist_cancel_get_list(froomlist.roomlist);
}

static void fl_get_list(GntWidget *button, gpointer null)
{
	PurpleAccount *account = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(froomlist.accounts));
	PurpleConnection *gc = purple_account_get_connection(account);

	if (!gc)
		return;

	update_roomlist(NULL);
	froomlist.roomlist = purple_roomlist_get_list(gc);
	gnt_box_give_focus_to_child(GNT_BOX(froomlist.window), froomlist.tree);
}

static void fl_add_chat(GntWidget *button, gpointer null)
{
	char *name;
	PurpleRoomlistRoom *room = gnt_tree_get_selection_data(GNT_TREE(froomlist.tree));
	PurpleConnection *gc = purple_account_get_connection(froomlist.account);
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc == NULL || room == NULL)
		return;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

	if(prpl_info != NULL && prpl_info->roomlist_room_serialize)
		name = prpl_info->roomlist_room_serialize(room);
	else
		name = g_strdup(purple_roomlist_room_get_name(room));

	purple_blist_request_add_chat(froomlist.account, NULL, NULL, name);

	g_free(name);
}

static void fl_close(GntWidget *button, gpointer null)
{
	gnt_widget_destroy(froomlist.window);
}

static void
roomlist_activated(GntWidget *widget)
{
	PurpleRoomlistRoom *room = gnt_tree_get_selection_data(GNT_TREE(widget));
	if (!room)
		return;

	switch (purple_roomlist_room_get_type(room)) {
		case PURPLE_ROOMLIST_ROOMTYPE_ROOM:
			purple_roomlist_room_join(froomlist.roomlist, room);
			break;
		case PURPLE_ROOMLIST_ROOMTYPE_CATEGORY:
			if (!purple_roomlist_room_get_expanded_once(room)) {
				purple_roomlist_expand_category(froomlist.roomlist, room);
				purple_roomlist_room_set_expanded_once(room, TRUE);
			}
			break;
	}
	gnt_tree_set_expanded(GNT_TREE(widget), room, TRUE);
}

static void
roomlist_selection_changed(GntWidget *widget, gpointer old, gpointer current, gpointer null)
{
	GList *iter, *field;
	PurpleRoomlistRoom *room = current;
	GntTextView *tv = GNT_TEXT_VIEW(froomlist.details);
	gboolean first = TRUE;

	gnt_text_view_clear(tv);

	if (!room)
		return;

	for (iter = purple_roomlist_room_get_fields(room),
			field = purple_roomlist_get_fields(froomlist.roomlist);
			iter && field;
			iter = iter->next, field = field->next) {
		PurpleRoomlistField *f = field->data;
		char *label = NULL;

		if (purple_roomlist_field_get_hidden(f)) {
			continue;
		}

		if (!first)
			gnt_text_view_append_text_with_flags(tv, "\n", GNT_TEXT_FLAG_NORMAL);

		gnt_text_view_append_text_with_flags(tv,
				purple_roomlist_field_get_label(f), GNT_TEXT_FLAG_BOLD);
		gnt_text_view_append_text_with_flags(tv, ": ", GNT_TEXT_FLAG_BOLD);

		switch (purple_roomlist_field_get_type(f)) {
			case PURPLE_ROOMLIST_FIELD_BOOL:
				label = g_strdup(iter->data ? "True" : "False");
				break;
			case PURPLE_ROOMLIST_FIELD_INT:
				label = g_strdup_printf("%d", GPOINTER_TO_INT(iter->data));
				break;
			case PURPLE_ROOMLIST_FIELD_STRING:
				label = g_strdup(iter->data);
				break;
		}
		gnt_text_view_append_text_with_flags(tv, label, GNT_TEXT_FLAG_NORMAL);
		g_free(label);
		first = FALSE;
	}

	if (purple_roomlist_room_get_type(room) == PURPLE_ROOMLIST_ROOMTYPE_CATEGORY) {
		if (!first)
			gnt_text_view_append_text_with_flags(tv, "\n", GNT_TEXT_FLAG_NORMAL);
		gnt_text_view_append_text_with_flags(tv,
				_("Hit 'Enter' to find more rooms of this category."),
				GNT_TEXT_FLAG_NORMAL);
	}
}

static void
roomlist_account_changed(GntWidget *widget, gpointer old, gpointer current, gpointer null)
{
	if (froomlist.account == current) {
		return;
	}

	froomlist.account = current;
	if (froomlist.roomlist) {
		if (purple_roomlist_get_in_progress(froomlist.roomlist))
			purple_roomlist_cancel_get_list(froomlist.roomlist);
		update_roomlist(NULL);
	}

	gnt_tree_remove_all(GNT_TREE(froomlist.tree));
	gnt_widget_draw(froomlist.tree);
}

static void
reset_account_list(PurpleAccount *account)
{
	GList *list;
	GntComboBox *accounts = GNT_COMBO_BOX(froomlist.accounts);
	gnt_combo_box_remove_all(accounts);
	for (list = purple_connections_get_all(); list; list = list->next) {
		PurplePluginProtocolInfo *prpl_info = NULL;
		PurpleConnection *gc = list->data;

		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
		if (PURPLE_CONNECTION_IS_CONNECTED(gc) &&
		        prpl_info->roomlist_get_list != NULL) {
			PurpleAccount *account = purple_connection_get_account(gc);
			char *text = g_strdup_printf("%s (%s)",
					purple_account_get_username(account),
					purple_account_get_protocol_name(account));
			gnt_combo_box_add_data(accounts, account, text);
			g_free(text);
		}
	}
}

static void
size_changed_cb(GntWidget *widget, int oldw, int oldh)
{
	int w, h;
	gnt_widget_get_size(widget, &w, &h);
	purple_prefs_set_int(PREF_ROOT "/size/width", w);
	purple_prefs_set_int(PREF_ROOT "/size/height", h);
}

static void
setup_roomlist(PurpleAccount *account)
{
	GntWidget *window, *tree, *hbox, *accounts;
	int iter;
	struct {
		const char *label;
		GCallback callback;
		GntWidget **widget;
	} buttons[] = {
		{_("Stop"), G_CALLBACK(fl_stop), &froomlist.stop},
		{_("Get"), G_CALLBACK(fl_get_list), &froomlist.getlist},
		{_("Add"), G_CALLBACK(fl_add_chat), &froomlist.add},
		{_("Close"), G_CALLBACK(fl_close), &froomlist.close},
		{NULL, NULL, NULL}
	};

	if (froomlist.window)
		return;

	froomlist.window = window = gnt_window_new();
	g_object_set(G_OBJECT(window), "vertical", TRUE, NULL);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_title(GNT_BOX(window), _("Room List"));
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	froomlist.accounts = accounts = gnt_combo_box_new();
	reset_account_list(account);
	gnt_box_add_widget(GNT_BOX(window), accounts);
	g_signal_connect(G_OBJECT(accounts), "selection-changed",
			G_CALLBACK(roomlist_account_changed), NULL);
	froomlist.account = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(accounts));

	froomlist.tree = tree = gnt_tree_new_with_columns(2);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(roomlist_activated), NULL);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Name"), "");
	gnt_tree_set_show_separator(GNT_TREE(tree), FALSE);
	gnt_tree_set_col_width(GNT_TREE(tree), 1, 1);
	gnt_tree_set_column_resizable(GNT_TREE(tree), 1, FALSE);
	gnt_tree_set_search_column(GNT_TREE(tree), 0);

	gnt_box_add_widget(GNT_BOX(window), tree);

	froomlist.details = gnt_text_view_new();
	gnt_text_view_set_flag(GNT_TEXT_VIEW(froomlist.details), GNT_TEXT_VIEW_TOP_ALIGN);
	gnt_box_add_widget(GNT_BOX(window), froomlist.details);
	gnt_widget_set_size(froomlist.details, -1, 8);

	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	for (iter = 0; buttons[iter].label; iter++) {
		GntWidget *button = gnt_button_new(buttons[iter].label);
		gnt_box_add_widget(GNT_BOX(hbox), button);
		g_signal_connect(G_OBJECT(button), "activate", buttons[iter].callback, NULL);
		*buttons[iter].widget = button;
		gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(froomlist.details), button);
	}

	g_signal_connect(G_OBJECT(tree), "selection-changed", G_CALLBACK(roomlist_selection_changed), NULL);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(unset_roomlist), NULL);
}

static void
fl_show_with_account(PurpleAccount *account)
{
	setup_roomlist(account);
	g_signal_handlers_disconnect_matched(G_OBJECT(froomlist.window), G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, G_CALLBACK(size_changed_cb), NULL);
	gnt_widget_show(froomlist.window);
	gnt_screen_resize_widget(froomlist.window,
			purple_prefs_get_int(PREF_ROOT "/size/width"),
			purple_prefs_get_int(PREF_ROOT "/size/height"));
	g_signal_connect(G_OBJECT(froomlist.window), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	gnt_window_present(froomlist.window);
}

static void
fl_create(PurpleRoomlist *list)
{
	purple_roomlist_set_ui_data(list, &froomlist);
	setup_roomlist(NULL);
	update_roomlist(list);
}

static void
fl_set_fields(PurpleRoomlist *list, GList *fields)
{
}

static void
fl_add_room(PurpleRoomlist *roomlist, PurpleRoomlistRoom *room)
{
	gboolean category;
	if (froomlist.roomlist != roomlist)
		return;

	category = (purple_roomlist_room_get_type(room) == PURPLE_ROOMLIST_ROOMTYPE_CATEGORY);
	gnt_tree_remove(GNT_TREE(froomlist.tree), room);
	gnt_tree_add_row_after(GNT_TREE(froomlist.tree), room,
			gnt_tree_create_row(GNT_TREE(froomlist.tree),
				purple_roomlist_room_get_name(room),
				category ? "<" : ""),
			purple_roomlist_room_get_parent(room), NULL);
	gnt_tree_set_expanded(GNT_TREE(froomlist.tree), room, !category);
}

static void
fl_destroy(PurpleRoomlist *list)
{
	if (!froomlist.window)
		return;

	if (froomlist.roomlist == list) {
		froomlist.roomlist = NULL;
		gnt_tree_remove_all(GNT_TREE(froomlist.tree));
		gnt_widget_draw(froomlist.tree);
	}
}

static PurpleRoomlistUiOps ui_ops =
{
	fl_show_with_account, /* void (*show_with_account)(PurpleAccount *account); **< Force the ui to pop up a dialog and get the list */
	fl_create, /* void (*create)(PurpleRoomlist *list); **< A new list was created. */
	fl_set_fields, /* void (*set_fields)(PurpleRoomlist *list, GList *fields); **< Sets the columns. */
	fl_add_room, /* void (*add_room)(PurpleRoomlist *list, PurpleRoomlistRoom *room); **< Add a room to the list. */
	NULL, /* void (*in_progress)(PurpleRoomlist *list, gboolean flag); **< Are we fetching stuff still? */
	fl_destroy, /* void (*destroy)(PurpleRoomlist *list); **< We're destroying list. */

	NULL, /* void (*_purple_reserved1)(void); */
	NULL, /* void (*_purple_reserved2)(void); */
	NULL, /* void (*_purple_reserved3)(void); */
	NULL /* void (*_purple_reserved4)(void); */
};

PurpleRoomlistUiOps *finch_roomlist_get_ui_ops(void)
{
	return &ui_ops;
}

void finch_roomlist_show_all(void)
{
	purple_roomlist_show_with_account(NULL);
}

void finch_roomlist_init(void)
{
	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_none(PREF_ROOT "/size");
	purple_prefs_add_int(PREF_ROOT "/size/width", 60);
	purple_prefs_add_int(PREF_ROOT "/size/height", 15);
}

void finch_roomlist_uninit(void)
{
}

