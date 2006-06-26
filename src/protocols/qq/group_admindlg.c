/**
* The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

// START OF FILE
/*****************************************************************************/
#include "debug.h"		// gaim_debug
#include "blist.h"		// GAIM_BLIST_NODE_IS_BUDDY
#include "notify.h"		// gaim_notify_warning

#include "utils.h"		// gaim_name_to_uid
#include "group_admindlg.h"
#include "group_find.h"		// qq_group_find_by_internal_group_id
#include "group_join.h"		// auth_type
#include "group_opt.h"		// QQ_GROUP_TYPE_PERMANENT

enum {
	COLUMN_SELECTED = 0,
	COLUMN_UID,
	COLUMN_NICKNAME,
	NUM_COLUMNS
};

enum {
	PAGE_INFO = 0,
	PAGE_MEMBER,
};

typedef struct _qun_info_window {
	guint32 internal_group_id;
	GaimConnection *gc;
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *lbl_external_group_id;
	GtkWidget *lbl_admin_uid;
	GtkWidget *ent_group_name;
	GtkWidget *cmb_group_category;
	GtkWidget *txt_group_desc;
	GtkWidget *txt_group_notice;
	GtkWidget *rad_auth[3];
	GtkWidget *btn_mod;
	GtkWidget *btn_close;
	GtkWidget *tre_members;
} qun_info_window;

const gchar *qq_group_category[] = {
	"同学", "朋友", "同事", "其他",
};				// qq_group_category

const gchar *qq_group_auth_type_desc[] = {
	"无须认证", "需要认证", "不可添加",
};				// qq_group_auth_type_desc

/*****************************************************************************/
static void _qq_group_info_window_deleteevent(GtkWidget * widget, GdkEvent * event, gpointer data) {
	gtk_widget_destroy(widget);	// this will call _window_destroy
}				// _window_deleteevent

/*****************************************************************************/
static void _qq_group_info_window_close(GtkWidget * widget, gpointer data)
{
	// this will call _info_window_destroy if it is info-window
	gtk_widget_destroy(GTK_WIDGET(data));
}				// _window_close

/*****************************************************************************/
static void _qq_group_info_window_destroy(GtkWidget * widget, gpointer data)
{
	GaimConnection *gc;
	GList *list;
	qq_data *qd;
	qun_info_window *info_window;

	gc = (GaimConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Group info is destoryed\n");

	qd = (qq_data *) gc->proto_data;
	list = qd->qun_info_window;

	while (list) {
		info_window = (qun_info_window *) (list->data);
		if (info_window->window != widget)
			list = list->next;
		else {
			qd->qun_info_window = g_list_remove(qd->qun_info_window, info_window);
			g_free(info_window);
			break;
		}		// if info_window
	}			// while
}				// _window_destroy

/*****************************************************************************/
void qq_qun_info_window_free(qq_data * qd)
{
	gint i;
	qun_info_window *info_window;

	i = 0;
	while (qd->qun_info_window) {
		info_window = (qun_info_window *) qd->qun_info_window->data;
		qd->qun_info_window = g_list_remove(qd->qun_info_window, info_window);
		if (info_window->window)
			gtk_widget_destroy(info_window->window);
		g_free(info_window);
		i++;
	}			// while

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d Qun info windows are freed\n", i);
}				// qq_qun_info_window_free

/*****************************************************************************/
static void _qq_group_info_window_modify(GtkWidget * widget, gpointer data)
{
	GaimConnection *gc;
	qun_info_window *info_window;

	g_return_if_fail(data != NULL);
	info_window = (qun_info_window *) data;

	gc = info_window->gc;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	//henry: This function contains some codes only supported by gtk-2.4 or later
//#if !GTK_CHECK_VERSION(2,4,0)
//        gaim_notify_info(gc, _("QQ Qun Operation"),
//                        _("This version of GTK-2 does not support this function"), NULL);
//        return;
//#else
	gint page, group_category, i = 0;
	qq_group *group;
	qq_data *qd;
	GtkTextIter start, end;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value = { 0, };
	guint32 *new_members;
	guint32 uid;
	gboolean selected;

	qd = (qq_data *) gc->proto_data;

	// we assume the modification can succeed
	// maybe it needs some tweak here
	group = qq_group_find_by_internal_group_id(gc, info_window->internal_group_id);
	g_return_if_fail(group != NULL);

	new_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(info_window->notebook));
	switch (page) {
	case PAGE_INFO:
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Gonna change Qun detailed information\n");
		// get the group_category
#if GTK_CHECK_VERSION(2,4,0)
		group_category = gtk_combo_box_get_active(GTK_COMBO_BOX(info_window->cmb_group_category));
#else
		group_category = gtk_option_menu_get_history(GTK_OPTION_MENU(info_window->cmb_group_category));
#endif

		if (group_category >= 0)
			group->group_category = group_category;
		else {
			g_free(group);
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Invalid group_category: %d\n", group_category);
			return;
		}		// if group_category
		// get auth_type
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->rad_auth[0])))
			group->auth_type = QQ_GROUP_AUTH_TYPE_NO_AUTH;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->rad_auth[1])))
			group->auth_type = QQ_GROUP_AUTH_TYPE_NEED_AUTH;
		else
			group->auth_type = QQ_GROUP_AUTH_TYPE_NO_ADD;
		// MUST use g_strdup, otherwise core dump after info_window is closed
		group->group_name_utf8 = g_strdup(gtk_entry_get_text(GTK_ENTRY(info_window->ent_group_name)));
		gtk_text_buffer_get_bounds(gtk_text_view_get_buffer
					   (GTK_TEXT_VIEW(info_window->txt_group_desc)), &start, &end);
		group->group_desc_utf8 =
		    g_strdup(gtk_text_buffer_get_text
			     (gtk_text_view_get_buffer
			      (GTK_TEXT_VIEW(info_window->txt_group_desc)), &start, &end, FALSE));
		gtk_text_buffer_get_bounds(gtk_text_view_get_buffer
					   (GTK_TEXT_VIEW(info_window->txt_group_notice)), &start, &end);
		group->notice_utf8 =
		    g_strdup(gtk_text_buffer_get_text
			     (gtk_text_view_get_buffer
			      (GTK_TEXT_VIEW(info_window->txt_group_notice)), &start, &end, FALSE));
		// finally we can modify it with new information
		qq_group_modify_info(gc, group);
		break;
	case PAGE_MEMBER:
		if (info_window->tre_members == NULL) {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Member list is not ready, cannot modify!\n");
		} else {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Gonna change Qun member list\n");
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(info_window->tre_members));
			if (gtk_tree_model_get_iter_first(model, &iter)) {
				gtk_tree_model_get_value(model, &iter, COLUMN_UID, &value);
				uid = g_value_get_uint(&value);
				g_value_unset(&value);
				gtk_tree_model_get_value(model, &iter, COLUMN_SELECTED, &value);
				selected = g_value_get_boolean(&value);
				g_value_unset(&value);
				if (!selected)
					new_members[i++] = uid;
				while (gtk_tree_model_iter_next(model, &iter)) {
					gtk_tree_model_get_value(model, &iter, COLUMN_UID, &value);
					uid = g_value_get_uint(&value);
					g_value_unset(&value);
					gtk_tree_model_get_value(model, &iter, COLUMN_SELECTED, &value);
					selected = g_value_get_boolean(&value);
					g_value_unset(&value);
					if (!selected)
						new_members[i++] = uid;
				}	// while
				new_members[i] = 0xffffffff;	// this labels the end
			} else
				new_members[0] = 0xffffffff;
			qq_group_modify_members(gc, group, new_members);
		}		// if info_window->tre_members
		break;
	default:
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Invalid page number: %d\n", page);
	}			// switch

	_qq_group_info_window_close(NULL, info_window->window);

//#endif /* GTK_CHECK_VERSION */
}				// _qq_group_info_window_modify

/*****************************************************************************/
static void _qq_group_member_list_deleted_toggled(GtkCellRendererToggle * cell, gchar * path_str, gpointer data) {
	qun_info_window *info_window;
	GaimConnection *gc;
	qq_group *group;

	info_window = (qun_info_window *) data;
	g_return_if_fail(info_window != NULL);

	gc = info_window->gc;
	g_return_if_fail(gc != NULL);

	group = qq_group_find_by_internal_group_id(gc, info_window->internal_group_id);
	g_return_if_fail(group != NULL);

	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(info_window->tre_members));
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	gboolean selected;
	guint32 uid;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COLUMN_SELECTED, &selected, -1);
	gtk_tree_model_get(model, &iter, COLUMN_UID, &uid, -1);

	if (uid != group->creator_uid) {	// do not allow delete admin
		selected ^= 1;
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_SELECTED, selected, -1);
		gtk_tree_path_free(path);
	} else
		gaim_notify_error(gc, NULL, _("Qun creator cannot be removed"), NULL);
}				// _qq_group_member_list_deleted_toggled

/*****************************************************************************/
static void _qq_group_member_list_drag_data_rcv_cb
    (GtkWidget * widget, GdkDragContext * dc, guint x, guint y,
     GtkSelectionData * sd, guint info, guint t, gpointer data) {

	GaimConnection *gc;
	GaimAccount *account;
	GaimBlistNode *n = NULL;
	GaimContact *c = NULL;
	GaimBuddy *b = NULL;
	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	GValue value = { 0, };
	guint32 uid, input_uid;

	treeview = widget;
	gc = (GaimConnection *) data;
	g_return_if_fail(gc != NULL);
	account = gaim_connection_get_account(gc);

	if (sd->target != gdk_atom_intern("GAIM_BLIST_NODE", FALSE) || sd->data == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Invalid drag data received, discard...\n");
		return;
	}			// if (sd->target

	memcpy(&n, sd->data, sizeof(n));

	// we expect GAIM_BLIST_CONTACT_NODE and GAIM_BLIST_BUDDY_NODE
	if (GAIM_BLIST_NODE_IS_CONTACT(n)) {
		c = (GaimContact *) n;
		b = c->priority;	// we get the first buddy only
	} else if (GAIM_BLIST_NODE_IS_BUDDY(n))
		b = (GaimBuddy *) n;

	if (b == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "No valid GaimBuddy is passed from DnD\n");
		return;
	}			// if b == NULL

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "We get a GaimBuddy: %s\n", b->name);
	input_uid = gaim_name_to_uid(b->name);
	g_return_if_fail(input_uid > 0);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	// we need to check if the user id in the member list is unique
	// possibly a tree transverse is necessary to achieve this
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		gtk_tree_model_get_value(model, &iter, COLUMN_UID, &value);
		uid = g_value_get_uint(&value);
		g_value_unset(&value);
		while (uid != input_uid && gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_model_get_value(model, &iter, COLUMN_UID, &value);
			uid = g_value_get_uint(&value);
			g_value_unset(&value);
		}		// while
	} else
		uid = 0;	// if gtk_tree_model_get_iter_first

	if (uid == input_uid) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Qun already has this buddy %s\n", b->name);
		return;
	} else {		// we add it to list
		store = GTK_LIST_STORE(model);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_SELECTED, FALSE, COLUMN_UID, input_uid, COLUMN_NICKNAME, b->alias, -1);
		// re-sort the list
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COLUMN_UID, GTK_SORT_ASCENDING);
	}			// if uid

}				// _qq_group_member_list_drag_data_rcv_cb

/*****************************************************************************/
static GtkWidget *_create_page_info(GaimConnection * gc, qq_group * group, gboolean do_manage, qun_info_window * info_window) {
	GtkWidget *vbox, *hbox;
	GtkWidget *frame_info, *frame_auth;
	GtkWidget *tbl_info;
	GtkWidget *label, *entry, *combo, *text, *scrolled_window;
	gint i;

	g_return_val_if_fail(gc != NULL && group != NULL, NULL);

	vbox = gtk_vbox_new(FALSE, 5);

	frame_info = gtk_frame_new(NULL);
	gtk_box_pack_start(GTK_BOX(vbox), frame_info, TRUE, TRUE, 0);

	tbl_info = gtk_table_new(6, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(tbl_info), 4);
	gtk_table_set_col_spacing(GTK_TABLE(tbl_info), 1, 10);
	gtk_container_add(GTK_CONTAINER(frame_info), tbl_info);

	label = gtk_label_new(_("Group ID: "));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	label = gtk_label_new(g_strdup_printf("%d", group->external_group_id));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	info_window->lbl_external_group_id = label;

	label = gtk_label_new(_("Group Name"));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 2, 3, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	entry = gtk_entry_new();
	gtk_widget_set_size_request(entry, 100, -1);
	if (group->group_name_utf8 != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), group->group_name_utf8);
	gtk_table_attach(GTK_TABLE(tbl_info), entry, 3, 4, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	info_window->ent_group_name = entry;

	label = gtk_label_new(_("Admin: "));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	label = gtk_label_new(g_strdup_printf("%d", group->creator_uid));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	info_window->lbl_admin_uid = label;

	label = gtk_label_new(_("Category"));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 2, 3, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

	//henry: these codes are supported only in GTK-2.4 or later
#if GTK_CHECK_VERSION(2, 4, 0)
	combo = gtk_combo_box_new_text();
	for (i = 0; i < 4; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), qq_group_category[i]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), group->group_category);
#else
	GtkWidget *menu;
	GtkWidget *item;

	combo = gtk_option_menu_new();
	menu = gtk_menu_new();
	for (i = 0; i < 4; i++) {
		item = gtk_menu_item_new_with_label(qq_group_category[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(combo), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(combo), group->group_category);
#endif				/* GTK_CHECK_VERSION */

	gtk_table_attach(GTK_TABLE(tbl_info), combo, 3, 4, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	info_window->cmb_group_category = combo;

	label = gtk_label_new(_("Description"));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_widget_set_size_request(text, -1, 50);
	if (group->group_desc_utf8 != NULL)
		gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), group->group_desc_utf8, -1);
	info_window->txt_group_desc = text;

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), text);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 2);
	gtk_table_attach(GTK_TABLE(tbl_info), scrolled_window, 0, 4, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new(_("Group Notice"));
	gtk_table_attach(GTK_TABLE(tbl_info), label, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
	text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_widget_set_size_request(text, -1, 50);
	if (group->notice_utf8 != NULL)
		gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), group->notice_utf8, -1);
	info_window->txt_group_notice = text;

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), text);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 2);
	gtk_table_attach(GTK_TABLE(tbl_info), scrolled_window, 0, 4, 5, 6, GTK_FILL, GTK_FILL, 0, 0);

	frame_auth = gtk_frame_new(_("Authentication"));
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame_auth), hbox);
	info_window->rad_auth[0] = gtk_radio_button_new_with_label(NULL, qq_group_auth_type_desc[0]);
	info_window->rad_auth[1] =
	    gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON
							(info_window->rad_auth[0]), qq_group_auth_type_desc[1]);
	info_window->rad_auth[2] =
	    gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON
							(info_window->rad_auth[0]), qq_group_auth_type_desc[2]);
	for (i = 0; i < 3; i++)
		gtk_box_pack_start(GTK_BOX(hbox), info_window->rad_auth[i], FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(info_window->rad_auth[group->auth_type - 1]), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), frame_auth, FALSE, FALSE, 0);

	if (!do_manage) {
		gtk_widget_set_sensitive(frame_info, FALSE);
		gtk_widget_set_sensitive(frame_auth, FALSE);
	}			// if ! do_manage

	return vbox;
}				// _create_info_page

/*****************************************************************************/
static GtkWidget *_create_page_members
    (GaimConnection * gc, qq_group * group, gboolean do_manage, qun_info_window * info_window) {
	GtkWidget *vbox, *sw, *treeview;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GList *list;
	qq_buddy *q_bud;
	GtkTargetEntry gte = { "GAIM_BLIST_NODE", GTK_TARGET_SAME_APP, 0 };

	g_return_val_if_fail(gc != NULL && group != NULL, NULL);

	vbox = gtk_vbox_new(FALSE, 0);

	if (group->members == NULL) {	// if NULL, not ready
		sw = gtk_label_new(_
				   ("OpenQ is collecting member information.\nPlease close this window and open again"));
		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
		return vbox;
	}			// if group->members

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_UINT, G_TYPE_STRING);

	list = group->members;
	while (list != NULL) {
		q_bud = (qq_buddy *) list->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_SELECTED, FALSE,
				   COLUMN_UID, q_bud->uid, COLUMN_NICKNAME, q_bud->nickname, -1);
		list = list->next;
	}			// for

	model = GTK_TREE_MODEL(store);
	treeview = gtk_tree_view_new_with_model(model);
	info_window->tre_members = treeview;

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COLUMN_UID);
	g_object_unref(model);

	// set up drag & drop ONLY for managable Qun
	if (do_manage) {
		gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(treeview), &gte, 1, GDK_ACTION_COPY);
		g_signal_connect(G_OBJECT(treeview), "drag-data-received",
				 G_CALLBACK(_qq_group_member_list_drag_data_rcv_cb), gc);
	}			// if do manage

	gtk_container_add(GTK_CONTAINER(sw), treeview);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	renderer = gtk_cell_renderer_toggle_new();

	// it seems this signal has to be handled
	// otherwise, the checkbox in the column does not reponse to user action
	if (do_manage)
		g_signal_connect(renderer, "toggled", G_CALLBACK(_qq_group_member_list_deleted_toggled), info_window);

	column = gtk_tree_view_column_new_with_attributes(_("Del"), renderer, "active", COLUMN_SELECTED, NULL);

	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 30);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("UID"), renderer, "text", COLUMN_UID, NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_UID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	// default sort by UID
	gtk_tree_view_column_set_sort_order(column, GTK_SORT_ASCENDING);
	gtk_tree_view_column_set_sort_indicator(column, TRUE);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Nickname"), renderer, "text", COLUMN_NICKNAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	return vbox;
}				// _create_page_members

/*****************************************************************************/
void qq_group_detail_window_show(GaimConnection * gc, qq_group * group)
{
	GtkWidget *vbox, *notebook;
	GtkWidget *label, *bbox;
	GList *list;
	qq_data *qd;
	qun_info_window *info_window = NULL;
	gboolean do_manage, do_show, do_exist;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && group != NULL);
	qd = (qq_data *) gc->proto_data;

	do_manage = group->my_status == QQ_GROUP_MEMBER_STATUS_IS_ADMIN;
	do_show = do_manage || (group->my_status == QQ_GROUP_MEMBER_STATUS_IS_MEMBER);

	if (!do_show) {
		gaim_notify_error(gc, _("QQ Qun Operation"),
				  _("You can not view Qun details"),
				  _("Only Qun admin or Qun member can view details"));
		return;
	}			// if ! do_show

	list = qd->qun_info_window;
	do_exist = FALSE;
	while (list != NULL) {
		info_window = (qun_info_window *) list->data;
		if (info_window->internal_group_id == group->internal_group_id) {
			break;
			do_exist = TRUE;
		} else
			list = list->next;
	}			// while list

	if (!do_exist) {
		info_window = g_new0(qun_info_window, 1);
		info_window->gc = gc;
		info_window->internal_group_id = group->internal_group_id;
		g_list_append(qd->qun_info_window, info_window);

		info_window->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		g_signal_connect(GTK_WINDOW(info_window->window),
				 "delete_event", G_CALLBACK(_qq_group_info_window_deleteevent), NULL);
		g_signal_connect(G_OBJECT(info_window->window), "destroy",
				 G_CALLBACK(_qq_group_info_window_destroy), gc);

		gtk_window_set_title(GTK_WINDOW(info_window->window), _("Manage Qun"));
		gtk_window_set_resizable(GTK_WINDOW(info_window->window), FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(info_window->window), 5);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(info_window->window), vbox);

		notebook = gtk_notebook_new();
		info_window->notebook = notebook;
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
		gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

		label = gtk_label_new(_("Qun Information"));
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
					 _create_page_info(gc, group, do_manage, info_window), label);
		label = gtk_label_new(_("Members"));
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
					 _create_page_members(gc, group, do_manage, info_window), label);

		bbox = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
		gtk_box_set_spacing(GTK_BOX(bbox), 10);

		info_window->btn_mod = gtk_button_new_with_label(_("Modify"));
		gtk_container_add(GTK_CONTAINER(bbox), info_window->btn_mod);
		g_signal_connect(G_OBJECT(info_window->btn_mod), "clicked",
				 G_CALLBACK(_qq_group_info_window_modify), info_window);

		info_window->btn_close = gtk_button_new_with_label(_("Close"));
		gtk_container_add(GTK_CONTAINER(bbox), info_window->btn_close);
		g_signal_connect(G_OBJECT(info_window->btn_close),
				 "clicked", G_CALLBACK(_qq_group_info_window_close), info_window->window);

		if (!do_manage)
			gtk_widget_set_sensitive(info_window->btn_mod, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);
		gtk_widget_show_all(info_window->window);

	} else			// we already have this
		gtk_widget_grab_focus(info_window->window);

}				// qq_group_manage_window_show

/*****************************************************************************/
// END OF FILE
