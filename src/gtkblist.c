/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef GAIM_PLUGINS
#ifndef _WIN32
#include <dlfcn.h>
#endif
#endif /* GAIM_PLUGINS */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <gdk/gdkwin32.h>
#else
#include <unistd.h>
#include <gdk/gdkx.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "prpl.h"
#include "sound.h"
#include "gaim.h"
#include "gtkblist.h"
#include "gtkpounce.h"
#include "gtkft.h"
#include "gtkdebug.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

GSList *gaim_gtk_blist_sort_methods = NULL;
static struct gaim_gtk_blist_sort_method *current_sort_method = NULL;
static GtkTreeIter sort_method_none(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur);
static GtkTreeIter sort_method_alphabetical(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur);
static GtkTreeIter sort_method_status(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur);
static GtkTreeIter sort_method_log(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur);
static struct gaim_gtk_buddy_list *gtkblist = NULL;

/* part of the best damn Docklet code this side of Tahiti */
static gboolean gaim_gtk_blist_obscured = FALSE;

static void gaim_gtk_blist_selection_changed(GtkTreeSelection *selection, gpointer data);
static void gaim_gtk_blist_update(struct gaim_buddy_list *list, GaimBlistNode *node);
static char *gaim_get_tooltip_text(GaimBlistNode *node);
static char *item_factory_translate_func (const char *path, gpointer func_data);
static gboolean get_iter_from_node(GaimBlistNode *node, GtkTreeIter *iter);

char sort_method[64];

struct _gaim_gtk_blist_node {
	GtkTreeRowReference *row;
};

/***************************************************
 *              Callbacks                          *
 ***************************************************/

static gboolean gtk_blist_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	if (docklet_count)
		gaim_blist_set_visible(FALSE);
	else
		do_quit();

	/* we handle everything, event should not propogate further */
	return TRUE;
}

static gboolean gtk_blist_save_prefs_cb(gpointer data)
{
	save_prefs();

	/* only run once */
	return FALSE;
}

static gboolean gtk_blist_configure_cb(GtkWidget *w, GdkEventConfigure *event, gpointer data)
{
	/* unfortunately GdkEventConfigure ignores the window gravity, but  *
	 * the only way we have of setting the position doesn't. we have to *
	 * call get_position and get_size because they do pay attention to  *
	 * the gravity. this is inefficient and I agree it sucks, but it's  *
	 * more likely to work correctly.                        - Robot101 */
	gint x, y;

	/* check for visibility because when we aren't visible, this will   *
	 * give us bogus (0,0) coordinates.                      - xOr      */
	if (GTK_WIDGET_VISIBLE(w)) {
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);

		if (x != blist_pos.x ||
		    y != blist_pos.y ||
		    event->width != blist_pos.width ||
		    event->height != blist_pos.height) {
			blist_pos.x = x;
			blist_pos.y = y;
			blist_pos.width = event->width;
			blist_pos.height = event->height;

			if (!g_main_context_find_source_by_user_data(NULL, &gtk_blist_save_prefs_cb)) {
				g_timeout_add(5000, gtk_blist_save_prefs_cb, &gtk_blist_save_prefs_cb);
			}
		}
	}

	/* continue to handle event normally */
	return FALSE;
}

static gboolean gtk_blist_visibility_cb(GtkWidget *w, GdkEventVisibility *event, gpointer data)
{
	if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
		gaim_gtk_blist_obscured = TRUE;
	else
		gaim_gtk_blist_obscured = FALSE;

	/* continue to handle event normally */
	return FALSE;
}

static void gtk_blist_menu_info_cb(GtkWidget *w, struct buddy *b)
{
	serv_get_info(b->account->gc, b->name);
}

static void gtk_blist_menu_im_cb(GtkWidget *w, struct buddy *b)
{
	gaim_conversation_new(GAIM_CONV_IM, b->account, b->name);
}

static void gtk_blist_menu_join_cb(GtkWidget *w, struct chat *chat)
{
	serv_join_chat(chat->account->gc, chat->components);
}

static void gtk_blist_menu_alias_cb(GtkWidget *w, GaimBlistNode *node)
{
	if(GAIM_BLIST_NODE_IS_BUDDY(node))
		alias_dialog_bud((struct buddy*)node);
	else if(GAIM_BLIST_NODE_IS_CHAT(node))
		alias_dialog_chat((struct chat*)node);
}

static void gtk_blist_menu_bp_cb(GtkWidget *w, struct buddy *b)
{
	gaim_gtkpounce_dialog_show(b, NULL);
}

static void gtk_blist_menu_showlog_cb(GtkWidget *w, struct buddy *b)
{
	show_log(b->name);
}

static void gtk_blist_show_systemlog_cb()
{
	show_log(NULL);
}

static void gtk_blist_show_onlinehelp_cb()
{
	open_url(NULL, WEBSITE "documentation.php");
}

static void gtk_blist_button_im_cb(GtkWidget *w, GtkTreeView *tv)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);

	if(gtk_tree_selection_get_selected(sel, &model, &iter)){
		GaimBlistNode *node;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			gaim_conversation_new(GAIM_CONV_IM, ((struct buddy*)node)->account, ((struct buddy*)node)->name);
			return;
		}
	}
	show_im_dialog();
}

static void gtk_blist_button_info_cb(GtkWidget *w, GtkTreeView *tv)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);

	if(gtk_tree_selection_get_selected(sel, &model, &iter)){
		GaimBlistNode *node;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			serv_get_info(((struct buddy*)node)->account->gc, ((struct buddy*)node)->name);
			return;
		}
	}
	show_info_dialog();
}

static void gtk_blist_button_chat_cb(GtkWidget *w, GtkTreeView *tv)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);

	if(gtk_tree_selection_get_selected(sel, &model, &iter)){
		GaimBlistNode *node;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_CHAT(node)) {
			serv_join_chat(((struct chat *)node)->account->gc, ((struct chat*)node)->components);
			return;
		}
	}
	join_chat();
}

static void gtk_blist_button_away_cb(GtkWidget *w, gpointer data)
{
	gtk_menu_popup(GTK_MENU(awaymenu), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME);
}

static void gtk_blist_row_expanded_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
	GaimBlistNode *node;
	GValue val = {0,};

	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_group_set_setting((struct group *)node, "collapsed", NULL);
		gaim_blist_save();
	}
}

static void gtk_blist_row_collapsed_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
	GaimBlistNode *node;
	GValue val = {0,};

	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_group_set_setting((struct group *)node, "collapsed", "true");
		gaim_blist_save();
	}
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	GaimBlistNode *node;
	GtkTreeIter iter;
	GValue val = { 0, };

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);

	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		struct gaim_conversation *conv =
			gaim_conversation_new(GAIM_CONV_IM, ((struct buddy*)node)->account, ((struct buddy*)node)->name);
		if(conv) {
			gaim_window_raise(gaim_conversation_get_window(conv));
			gaim_window_switch_conversation(
				gaim_conversation_get_window(conv),
				gaim_conversation_get_index(conv));
		}
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		serv_join_chat(((struct chat *)node)->account->gc, ((struct chat*)node)->components);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);
	}
}

static void gaim_gtk_blist_add_chat_cb()
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	GaimBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CHAT(node))
			show_add_chat(NULL, (struct group*)node->parent);
		else if (GAIM_BLIST_NODE_IS_GROUP(node))
			show_add_chat(NULL, (struct group*)node);
	}
	else {
		show_add_chat(NULL, NULL);
	}
}

static void gaim_gtk_blist_add_buddy_cb()
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	GaimBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CHAT(node))
			show_add_buddy(NULL, NULL, ((struct group*)node->parent)->name, NULL);
		else if (GAIM_BLIST_NODE_IS_GROUP(node))
			show_add_buddy(NULL, NULL, ((struct group*)node)->name, NULL);
	}
	else {
		show_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void
gaim_gtk_blist_remove_cb (GtkWidget *w, GaimBlistNode *node) 
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		struct buddy *b = (struct buddy*)node;
		show_confirm_del(b->account->gc, b->name);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		struct chat *chat = (struct chat*)node;
		show_confirm_del_chat(chat);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		struct group *g = (struct group*)node;
		show_confirm_del_group(g);
	}
}

static void gaim_proto_menu_cb(GtkMenuItem *item, struct buddy *b)
{
	struct proto_buddy_menu *pbm = g_object_get_data(G_OBJECT(item), "gaimcallback");
	if (pbm->callback)
		pbm->callback(pbm->gc, b->name);
}

static gboolean gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer null)
{
	GtkTreePath *path;
	GaimBlistNode *node;
	GValue val = { 0, };
	GtkTreeIter iter;
	GtkWidget *menu, *menuitem;
	GtkTreeSelection *sel;
	GList *list;
	GaimPlugin *prpl = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (event->button != 3)
		return FALSE;

	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	menu = gtk_menu_new();

	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_new_item_from_stock(menu, _("Add a _Buddy"), GTK_STOCK_ADD, G_CALLBACK(gaim_gtk_blist_add_buddy_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("Add a C_hat"), GTK_STOCK_ADD, G_CALLBACK(gaim_gtk_blist_add_chat_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Delete Group"), GTK_STOCK_REMOVE, G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Rename"), NULL, G_CALLBACK(show_rename_group), node, 0, 0, NULL);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		gaim_new_item_from_stock(menu, _("_Join"), GAIM_STOCK_CHAT, G_CALLBACK(gtk_blist_menu_join_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Alias"), GAIM_STOCK_EDIT, G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE, G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		/* Protocol specific options */
		prpl = gaim_find_prpl(((struct buddy*)node)->account->protocol);

		if (prpl != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl && prpl_info->get_info)
			gaim_new_item_from_stock(menu, _("_Get Info"), GAIM_STOCK_INFO, G_CALLBACK(gtk_blist_menu_info_cb), node, 0, 0, NULL);

		gaim_new_item_from_stock(menu, _("_IM"), GAIM_STOCK_IM, G_CALLBACK(gtk_blist_menu_im_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("Add Buddy _Pounce"), NULL, G_CALLBACK(gtk_blist_menu_bp_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("View _Log"), NULL, G_CALLBACK(gtk_blist_menu_showlog_cb), node, 0, 0, NULL);

		if (prpl && prpl_info->buddy_menu) {
			list = prpl_info->buddy_menu(((struct buddy*)node)->account->gc, ((struct buddy*)node)->name);
			while (list) {
				struct proto_buddy_menu *pbm = list->data;
				menuitem = gtk_menu_item_new_with_mnemonic(pbm->label);
				g_object_set_data(G_OBJECT(menuitem), "gaimcallback", pbm);
				g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(gaim_proto_menu_cb), node);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
				list = list->next;
			}
		}

		gaim_event_broadcast (event_draw_menu, menu, ((struct buddy *) node)->name);

		gaim_separator(menu);
		gaim_new_item_from_stock(menu, _("_Alias"), GAIM_STOCK_EDIT, G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE, G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);
	}
	
	gtk_widget_show_all(menu);
	
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);

#if (1)  /* This code only exists because GTK doesn't work.  If we return FALSE here, as would be normal
	  * the event propoagates down and somehow gets interpreted as the start of a drag event. */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);
	return TRUE;
#endif
}

static void gaim_gtk_blist_show_empty_groups_cb(gpointer data, guint action, GtkWidget *item)
{
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		blist_options &= ~OPT_BLIST_NO_MT_GRP;
	else
		blist_options |= OPT_BLIST_NO_MT_GRP;
	save_prefs();
	gaim_gtk_blist_refresh(gaim_get_blist());
}

static void gaim_gtk_blist_edit_mode_cb(gpointer callback_data, guint callback_action,
		GtkWidget *checkitem) {
	if(gtkblist->window->window) {
		GdkCursor *cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor(gtkblist->window->window, cursor);
		while (gtk_events_pending())
			gtk_main_iteration();
		gdk_cursor_unref(cursor);
	}

	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkitem)))
		blist_options |= OPT_BLIST_SHOW_OFFLINE;
	else
		blist_options &= ~OPT_BLIST_SHOW_OFFLINE;
	save_prefs();

	if(gtkblist->window->window) {
		GdkCursor *cursor = gdk_cursor_new(GDK_LEFT_PTR);
		gdk_window_set_cursor(gtkblist->window->window, cursor);
		gdk_cursor_unref(cursor);
	}

	gaim_gtk_blist_refresh(gaim_get_blist());
}

static void gaim_gtk_blist_drag_data_get_cb (GtkWidget *widget,
					     GdkDragContext *dc,
					     GtkSelectionData *data,
					     guint info,
					     guint time,
					     gpointer *null)
{
	if (data->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE)) {
		GtkTreeRowReference *ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		GtkTreePath *sourcerow = gtk_tree_row_reference_get_path(ref);
		GtkTreeIter iter;
		GaimBlistNode *node = NULL;
		GValue val = {0};
		if(!sourcerow)
			return;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, sourcerow);
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
		node = g_value_get_pointer(&val);
		gtk_selection_data_set (data,
					gdk_atom_intern ("GAIM_BLIST_NODE", FALSE),
					8, /* bits */
					(void*)&node,
					sizeof (node));

		gtk_tree_path_free(sourcerow);
	}

}

static void gaim_gtk_blist_drag_data_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
			  GtkSelectionData *sd, guint info, guint t)
{	
	if (sd->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE) && sd->data) {
		GaimBlistNode *n = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		memcpy(&n, sd->data, sizeof(n));
		if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
			/* if we're here, I think it means the drop is ok */
			GtkTreeIter iter;
			GaimBlistNode *node;
			GValue val = {0};
			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);

			if (GAIM_BLIST_NODE_IS_BUDDY(n)) {
				struct buddy *b = (struct buddy*)n;
				if (GAIM_BLIST_NODE_IS_BUDDY(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							gaim_blist_add_buddy(b, (struct group*)node->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_buddy(b, (struct group*)node->parent, node->prev);
							break;
					}
				} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
					gaim_blist_add_buddy(b, (struct group*)node, NULL);
				}
			} else if (GAIM_BLIST_NODE_IS_CHAT(n)) {
				struct chat *chat = (struct chat*)n;
				if (GAIM_BLIST_NODE_IS_BUDDY(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							gaim_blist_add_chat(chat, (struct group*)node->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_chat(chat, (struct group*)node->parent, node->prev);
							break;
					}
				} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
					gaim_blist_add_chat(chat, (struct group*)node, NULL);
				}
			} else if (GAIM_BLIST_NODE_IS_GROUP(n)) {
				struct group *g = (struct group*)n;
				if (GAIM_BLIST_NODE_IS_GROUP(node)) {
					switch (position) {
					case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					case GTK_TREE_VIEW_DROP_AFTER:
						gaim_blist_add_group(g, node);
						break;
					case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					case GTK_TREE_VIEW_DROP_BEFORE:
						gaim_blist_add_group(g, node->prev);
						break;
					}

				} else if(GAIM_BLIST_NODE_IS_BUDDY(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					gaim_blist_add_group(g, node->parent);
				}

			}

			gtk_tree_path_free(path);
			gaim_blist_save();
		}
	}
}

static void gaim_gtk_blist_paint_tip(GtkWidget *widget, GdkEventExpose *event, GaimBlistNode *node)
{
	GtkStyle *style;
	GdkPixbuf *pixbuf = gaim_gtk_blist_get_status_icon(node, GAIM_STATUS_ICON_LARGE);
	PangoLayout *layout;
	char *tooltiptext = gaim_get_tooltip_text(node);

	layout = gtk_widget_create_pango_layout (gtkblist->tipwindow, NULL);
	pango_layout_set_markup(layout, tooltiptext, strlen(tooltiptext));
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 300000);
	style = gtkblist->tipwindow->style;

	gtk_paint_flat_box (style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			    NULL, gtkblist->tipwindow, "tooltip", 0, 0, -1, -1);

#if GTK_CHECK_VERSION(2,2,0)
	gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, pixbuf,
			0, 0, 4, 4, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
#else
	gdk_pixbuf_render_to_drawable(pixbuf, GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, 0, 0, 4, 4, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
#endif

	gtk_paint_layout (style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, TRUE,
			  NULL, gtkblist->tipwindow, "tooltip", 38, 4, layout);

	g_object_unref (pixbuf);
	g_object_unref (layout);
	g_free(tooltiptext);
	return;
}

static gboolean gaim_gtk_blist_tooltip_timeout(GtkWidget *tv)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GaimBlistNode *node;
	GValue val = {0};
	int scr_w,scr_h, w, h, x, y;
	PangoLayout *layout;
	char *tooltiptext = NULL;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), gtkblist->rect.x, gtkblist->rect.y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	if(!GAIM_BLIST_NODE_IS_BUDDY(node) && !GAIM_BLIST_NODE_IS_CHAT(node))
		return FALSE;

	tooltiptext = gaim_get_tooltip_text(node);
	gtkblist->tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
	gtkblist->tipwindow->parent = tv;
	gtk_widget_set_app_paintable(gtkblist->tipwindow, TRUE);
	gtk_window_set_resizable(GTK_WINDOW(gtkblist->tipwindow), FALSE);
	gtk_widget_set_name(gtkblist->tipwindow, "gtk-tooltips");
	g_signal_connect(G_OBJECT(gtkblist->tipwindow), "expose_event",
			G_CALLBACK(gaim_gtk_blist_paint_tip), node);
	gtk_widget_ensure_style (gtkblist->tipwindow);

	layout = gtk_widget_create_pango_layout (gtkblist->tipwindow, NULL);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 300000);
	pango_layout_set_markup(layout, tooltiptext, strlen(tooltiptext));
	scr_w = gdk_screen_width();
	scr_h = gdk_screen_height();
	pango_layout_get_size (layout, &w, &h);
	w = PANGO_PIXELS(w) + 8;
	h = PANGO_PIXELS(h) + 8;

	/* 38 is the size of a large status icon plus 4 pixels padding on each side.
	 *  I should #define this or something */
	w = w + 38;
	h = MAX(h, 38);

	gdk_window_get_pointer(NULL, &x, &y, NULL);
	if (GTK_WIDGET_NO_WINDOW(gtkblist->window))
		y+=gtkblist->window->allocation.y;

	x -= ((w >> 1) + 4);

	if ((x + w) > scr_w)
		x -= (x + w) - scr_w;
	else if (x < 0)
		x = 0;

	if ((y + h + 4) > scr_h)
		y = y - h;
	else
		y = y + 6;
	g_object_unref (layout);
	g_free(tooltiptext);
	gtk_widget_set_size_request(gtkblist->tipwindow, w, h);
	gtk_window_move(GTK_WINDOW(gtkblist->tipwindow), x, y);
	gtk_widget_show(gtkblist->tipwindow);

	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean gaim_gtk_blist_motion_cb (GtkWidget *tv, GdkEventMotion *event, gpointer null)
{
	GtkTreePath *path;
	if (gtkblist->timeout) {
		if ((event->y > gtkblist->rect.y) && ((event->y - gtkblist->rect.height) < gtkblist->rect.y))
			return FALSE;
		/* We've left the cell.  Remove the timeout and create a new one below */
		if (gtkblist->tipwindow) {
			gtk_widget_destroy(gtkblist->tipwindow);
			gtkblist->tipwindow = NULL;
		}
		
		g_source_remove(gtkblist->timeout);
	}
	
	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &gtkblist->rect);
	if (path)
		gtk_tree_path_free(path);
	gtkblist->timeout = g_timeout_add(500, (GSourceFunc)gaim_gtk_blist_tooltip_timeout, tv);
	return FALSE;
}

static void gaim_gtk_blist_leave_cb (GtkWidget *w, GdkEventCrossing *e, gpointer n)
{
	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}
	if (gtkblist->tipwindow) {
		gtk_widget_destroy(gtkblist->tipwindow);
		gtkblist->tipwindow = NULL;
	}
}

static void
toggle_debug(void)
{
	misc_options ^= OPT_MISC_DEBUG;

	if ((misc_options & OPT_MISC_DEBUG))
		gaim_gtk_debug_window_show();
	else
		gaim_gtk_debug_window_hide();

	save_prefs();
}


/***************************************************
 *            Crap                                 *
 ***************************************************/
static GtkItemFactoryEntry blist_menu[] =
{
	/* Buddies menu */
	{ N_("/_Buddies"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Buddies/New _Instant Message..."), "<CTL>I", show_im_dialog, 0, "<StockItem>", GAIM_STOCK_IM },
	{ N_("/Buddies/Join a _Chat..."), "<CTL>C", join_chat, 0, "<StockItem>", GAIM_STOCK_CHAT },
	{ N_("/Buddies/Get _User Info..."), "<CTL>J", show_info_dialog, 0, "<StockItem>", GAIM_STOCK_INFO },
	{ "/Buddies/sep1", NULL, NULL, 0, "<Separator>" },
	{ N_("/Buddies/Show _Offline Buddies"), NULL, gaim_gtk_blist_edit_mode_cb, 1, "<CheckItem>"},
	{ N_("/Buddies/Show _Empty Groups"), NULL, gaim_gtk_blist_show_empty_groups_cb, 1, "<CheckItem>"},
	{ N_("/Buddies/_Add a Buddy..."), "<CTL>B", gaim_gtk_blist_add_buddy_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add a C_hat..."), NULL, gaim_gtk_blist_add_chat_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add a _Group..."), NULL, show_add_group, 0, NULL},
	{ "/Buddies/sep2", NULL, NULL, 0, "<Separator>" },
	{ N_("/Buddies/_Signoff"), "<CTL>D", signoff_all, 0, "<StockItem>", GAIM_STOCK_SIGN_OFF },
	{ N_("/Buddies/_Quit"), "<CTL>Q", do_quit, 0, "<StockItem>", GTK_STOCK_QUIT },

	/* Tools */ 
	{ N_("/_Tools"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/_Away"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/Buddy _Pounce"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/P_rotocol Actions"), NULL, NULL, 0, "<Branch>" },
	{ "/Tools/sep1", NULL, NULL, 0, "<Separator>" },
	{ N_("/Tools/A_ccounts..."), "<CTL>A", account_editor, 0, "<StockItem>", GAIM_STOCK_ACCOUNTS },
	{ N_("/Tools/_File Transfers..."), NULL, gaim_show_xfer_dialog, 0, "<StockItem>", GAIM_STOCK_FILE_TRANSFER },
	{ N_("/Tools/Preferences..."), "<CTL>P", show_prefs, 0, "<StockItem>", GTK_STOCK_PREFERENCES },
	{ N_("/Tools/Pr_ivacy..."), NULL, show_privacy_options, 0, "<StockItem>", GAIM_STOCK_PRIVACY },
	{ "/Tools/sep2", NULL, NULL, 0, "<Separator>" },
	{ N_("/Tools/View System _Log..."), NULL, gtk_blist_show_systemlog_cb, 0, NULL },

	/* Help */
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Help/Online _Help"), "F1", gtk_blist_show_onlinehelp_cb, 0, "<StockItem>", GTK_STOCK_HELP },
	{ N_("/Help/_Debug Window..."), NULL, toggle_debug, 0, NULL },
	{ N_("/Help/_About..."), "<CTL>F1", show_about, 0,  "<StockItem>", GAIM_STOCK_ABOUT },
};

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static char *gaim_get_tooltip_text(GaimBlistNode *node)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	char *text = NULL;

	if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		struct chat *chat = (struct chat *)node;
		char *name = NULL;
		struct proto_chat_entry *pce;
		GList *parts, *tmp;
		GString *parts_text = g_string_new("");

		prpl = gaim_find_prpl(chat->account->protocol);
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		parts = prpl_info->chat_info(chat->account->gc);

		if(chat->alias) {
			name = g_markup_escape_text(chat->alias, -1);
		} else {
			pce = parts->data;
			name = g_markup_escape_text(g_hash_table_lookup(chat->components,
						pce->identifier), -1);
		}
		if(g_slist_length(connections) > 1) {
			char *account = g_markup_escape_text(chat->account->username, -1);
			g_string_append_printf(parts_text, _("\n<b>Account:</b> %s"),
					account);
			g_free(account);
		}
		for(tmp = parts; tmp; tmp = tmp->next) {
			char *label, *value;
			pce = tmp->data;

			label = g_markup_escape_text(pce->label, -1);

			value = g_markup_escape_text(g_hash_table_lookup(chat->components,
						pce->identifier), -1);

			g_string_append_printf(parts_text, "\n<b>%s</b> %s", label, value);
			g_free(label);
			g_free(value);
			g_free(pce);
		}
		g_list_free(parts);

		text = g_strdup_printf("<span size='larger' weight='bold'>%s</span>%s",
				name, parts_text->str);
		g_string_free(parts_text, TRUE);
		g_free(name);
	} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		struct buddy *b = (struct buddy *)node;
		char *statustext = NULL;
		char *aliastext = NULL, *nicktext = NULL;
		char *warning = NULL, *idletime = NULL;
		char *accounttext = NULL;

		prpl = gaim_find_prpl(b->account->protocol);
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl_info->tooltip_text) {
			const char *end;
			statustext = prpl_info->tooltip_text(b);

			if(statustext && !g_utf8_validate(statustext, -1, &end)) {
				char *new = g_strndup(statustext,
						g_utf8_pointer_to_offset(statustext, end));
				g_free(statustext);
				statustext = new;
			}
		}

		if (!statustext && !GAIM_BUDDY_IS_ONLINE(b))
			statustext = g_strdup(_("<b>Status:</b> Offline"));

		if (b->idle > 0)
			idletime = sec_to_text(time(NULL) - b->idle);

		if(b->alias && b->alias[0])
			aliastext = g_markup_escape_text(b->alias, -1);

		if(b->server_alias)
			nicktext = g_markup_escape_text(b->server_alias, -1);

		if (b->evil > 0)
			warning = g_strdup_printf(_("%d%%"), b->evil);

		if(g_slist_length(connections) > 1)
			accounttext = g_markup_escape_text(b->account->username, -1);

		text = g_strdup_printf("<span size='larger' weight='bold'>%s</span>"
				   "%s %s"     /* Account */
			       "%s %s"  /* Alias */
				   "%s %s"  /* Nickname */
			       "%s %s"     /* Idle */
			       "%s %s"     /* Warning */
			       "%s%s"     /* Status */
				   "%s",
			       b->name,
				   accounttext ? _("\n<b>Account:</b>") : "", accounttext ? accounttext : "",
			       aliastext ? _("\n<b>Alias:</b>") : "", aliastext ? aliastext : "",
			       nicktext ? _("\n<b>Nickname:</b>") : "", nicktext ? nicktext : "",
			       idletime ? _("\n<b>Idle:</b>") : "", idletime ? idletime : "",
			       b->evil ? _("\n<b>Warned:</b>") : "", b->evil ? warning : "",
			       statustext ? "\n" : "", statustext ? statustext : "",
				   !g_ascii_strcasecmp(b->name, "robflynn") ? _("\n<b>Description:</b> Spooky") : "");

		if(warning)
			g_free(warning);
		if(idletime)
			g_free(idletime);
		if(statustext)
			g_free(statustext);
		if(nicktext)
			g_free(nicktext);
		if(aliastext)
			g_free(aliastext);
		if(accounttext)
			g_free(accounttext);
	}

	return text;
}

GdkPixbuf *gaim_gtk_blist_get_status_icon(GaimBlistNode *node, GaimStatusIconSize size)
{
	GdkPixbuf *status = NULL;
	GdkPixbuf *scale = NULL;
	GdkPixbuf *emblem = NULL;
	gchar *filename = NULL;
	const char *protoname = NULL;

	char *se = NULL, *sw = NULL ,*nw = NULL ,*ne = NULL;

	int scalesize = 30;

	GaimPlugin *prpl = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if(GAIM_BLIST_NODE_IS_BUDDY(node))
		prpl = gaim_find_prpl(((struct buddy *)node)->account->protocol);
	else if(GAIM_BLIST_NODE_IS_CHAT(node))
		prpl = gaim_find_prpl(((struct chat *)node)->account->protocol);

	if (!prpl)
		return NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->list_icon) {
		if(GAIM_BLIST_NODE_IS_BUDDY(node))
			protoname = prpl_info->list_icon(((struct buddy*)node)->account,
					(struct buddy *)node);
		else if(GAIM_BLIST_NODE_IS_CHAT(node))
			protoname = prpl_info->list_icon(((struct chat*)node)->account, NULL);
	}

	if (GAIM_BLIST_NODE_IS_BUDDY(node) &&
			((struct buddy *)node)->present != GAIM_BUDDY_SIGNING_OFF &&
			prpl_info->list_emblems) {
		prpl_info->list_emblems((struct buddy*)node, &se, &sw, &nw, &ne);
	}

	if (size == GAIM_STATUS_ICON_SMALL) {
		scalesize = 15;
		sw = nw = ne = NULL; /* So that only the se icon will composite */
	}


	if (GAIM_BLIST_NODE_IS_BUDDY(node) &&
			((struct buddy*)node)->present == GAIM_BUDDY_SIGNING_ON) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "login.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node) &&
			((struct buddy *)node)->present == GAIM_BUDDY_SIGNING_OFF) {
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "logout.png", NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);

		/* "Hey, what's all this crap?" you ask.  Status icons will be themeable too, and
		   then it will look up protoname from the theme */
	} else {
		char *image = g_strdup_printf("%s.png", protoname);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		status = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(image);
		g_free(filename);

	}

	if (!status)
		return NULL;

	scale =  gdk_pixbuf_scale_simple(status, scalesize, scalesize, GDK_INTERP_BILINEAR);

	g_object_unref(G_OBJECT(status));

	/* Emblems */

	/* Each protocol can specify up to four "emblems" to composite over the base icon.  "away", "busy", "mobile user"
	 * are all examples of states represented by emblems.  I'm not even really sure I like this yet. */

	/* XXX Clean this crap up, yo. */
	if (se) {
		char *image = g_strdup_printf("%s.png", se);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			if (size == GAIM_STATUS_ICON_LARGE)
				gdk_pixbuf_composite (emblem,
						      scale, 15, 15,
						      15, 15,
						      15, 15,
						      1, 1,
						      GDK_INTERP_BILINEAR,
						      255);
			else
					gdk_pixbuf_composite (emblem,
						      scale, 5, 5,
						      10, 10,
						      5, 5,
						      .6, .6,
						      GDK_INTERP_BILINEAR,
						      255);
			g_object_unref(G_OBJECT(emblem));
		}
	}
	if (sw) {
		char *image = g_strdup_printf("%s.png", sw);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			gdk_pixbuf_composite (emblem,
					scale, 0, 15,
					15, 15,
					0, 15,
					1, 1,
					GDK_INTERP_BILINEAR,
					255);
			g_object_unref(G_OBJECT(emblem));
		}
	}
	if (nw) {
		char *image = g_strdup_printf("%s.png", nw);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			gdk_pixbuf_composite (emblem,
					      scale, 0, 0,
					      15, 15,
					      0, 0,
					      1, 1,
					      GDK_INTERP_BILINEAR,
					      255);
			g_object_unref(G_OBJECT(emblem));
		}
	}
	if (ne) {
		char *image = g_strdup_printf("%s.png", ne);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
		emblem = gdk_pixbuf_new_from_file(filename,NULL);
		g_free(filename);
		if (emblem) {
			gdk_pixbuf_composite (emblem,
					      scale, 15, 0,
					      15, 15,
					      15, 0,
					      1, 1,
					      GDK_INTERP_BILINEAR,
					      255);
			g_object_unref(G_OBJECT(emblem));
		}
	}


	/* Idle grey buddies affects the whole row.  This converts the status icon to greyscale. */
	if (GAIM_BLIST_NODE_IS_BUDDY(node) &&
			((struct buddy *)node)->present == GAIM_BUDDY_OFFLINE)
		gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node) &&
			((struct buddy *)node)->idle &&
			blist_options & OPT_BLIST_GREY_IDLERS)
		gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.25, FALSE);
	return scale;
}

static GdkPixbuf *gaim_gtk_blist_get_buddy_icon(struct buddy *b)
{
	/* This just opens a file from ~/.gaim/icons/screenname.  This needs to change to be more gooder. */
	char *file;
	GdkPixbuf *buf, *ret;

	if (!(blist_options & OPT_BLIST_SHOW_ICONS))
		return NULL;

	if ((file = gaim_buddy_get_setting(b, "buddy_icon")) == NULL)
		return NULL;

	buf = gdk_pixbuf_new_from_file(file, NULL);
	g_free(file);


	if (buf) {
		if (!GAIM_BUDDY_IS_ONLINE(b))
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);
		if (b->idle && blist_options & OPT_BLIST_GREY_IDLERS)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.25, FALSE);

		ret = gdk_pixbuf_scale_simple(buf,30,30, GDK_INTERP_BILINEAR);
		g_object_unref(G_OBJECT(buf));
		return ret;
	}
	return NULL;
}

static gchar *gaim_gtk_blist_get_name_markup(struct buddy *b, gboolean selected)
{
	char *name = gaim_get_buddy_alias(b);
	char *esc = g_markup_escape_text(name, strlen(name)), *text = NULL;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	/* XXX Clean up this crap */

	int ihrs, imin;
	char *idletime = NULL, *warning = NULL, *statustext = NULL;
	time_t t;

	prpl = gaim_find_prpl(b->account->protocol);

	if (prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (!(blist_options & OPT_BLIST_SHOW_ICONS)) {
		if ((b->idle && blist_options & OPT_BLIST_GREY_IDLERS && !selected) || !GAIM_BUDDY_IS_ONLINE(b)) {
			text =  g_strdup_printf("<span color='dim grey'>%s</span>",
						esc);
			g_free(esc);
			return text;
		} else {
			return esc;
		}
	}

	time(&t);
	ihrs = (t - b->idle) / 3600;
	imin = ((t - b->idle) / 60) % 60;

	if (prpl && prpl_info->status_text) {
		char *tmp = prpl_info->status_text(b);
		const char *end;

		if(tmp && !g_utf8_validate(tmp, -1, &end)) {
			char *new = g_strndup(tmp,
					g_utf8_pointer_to_offset(tmp, end));
			g_free(tmp);
			tmp = new;
		}

		if(tmp) {
			char buf[32];
			char *c = tmp;
			int length = 0, vis=0;
			gboolean inside = FALSE;
			g_strdelimit(tmp, "\n", ' ');

			while(*c && vis < 20) {
				if(*c == '&')
					inside = TRUE;
				else if(*c == ';')
					inside = FALSE;
				if(!inside)
					vis++;
				length++;
				c++; /* this is fun */
			}

			if(vis == 20)
				g_snprintf(buf, sizeof(buf), "%%.%ds...", length);
			else
				g_snprintf(buf, sizeof(buf), "%%s ");

			statustext = g_strdup_printf(buf, tmp);

			g_free(tmp);
		}
	}

	if (b->idle > 0) {
		if (ihrs)
			idletime = g_strdup_printf(_("Idle (%dh%02dm) "), ihrs, imin);
		else
			idletime = g_strdup_printf(_("Idle (%dm) "), imin);
	}

	if (b->evil > 0)
		warning = g_strdup_printf(_("Warned (%d%%) "), b->evil);

	if(!GAIM_BUDDY_IS_ONLINE(b) && !statustext)
		statustext = g_strdup("Offline ");

	if (b->idle && blist_options & OPT_BLIST_GREY_IDLERS && !selected) {
		text =  g_strdup_printf("<span color='dim grey'>%s</span>\n"
					"<span color='dim grey' size='smaller'>%s%s%s</span>",
					esc,
					statustext != NULL ? statustext : "",
					idletime != NULL ? idletime : "",
					warning != NULL ? warning : "");
	} else if (statustext == NULL && idletime == NULL && warning == NULL && GAIM_BUDDY_IS_ONLINE(b)) {
		text = g_strdup(esc);
	} else {
		text = g_strdup_printf("%s\n"
				       "<span %s size='smaller'>%s%s%s</span>", esc,
				       selected ? "" : "color='dim grey'",
				       statustext != NULL ? statustext :  "",
				       idletime != NULL ? idletime : "", 
				       warning != NULL ? warning : "");
	}
	if (idletime)
		g_free(idletime);
	if (warning)
		g_free(warning);
	if (statustext)
		g_free(statustext);
	if (esc)
		g_free(esc);

	return text;
}

static void gaim_gtk_blist_restore_position()
{
	/* if the window exists, is hidden, we're saving positions, and the position is sane... */
	if(gtkblist && gtkblist->window &&
	   !GTK_WIDGET_VISIBLE(gtkblist->window) &&
	   blist_pos.width != 0) {
		/* ...check position is on screen... */
		if (blist_pos.x >= gdk_screen_width())
			blist_pos.x = gdk_screen_width() - 100;
		else if (blist_pos.x < 0)
			blist_pos.x = 100;
		
		if (blist_pos.y >= gdk_screen_height())
			blist_pos.y = gdk_screen_height() - 100;
		else if (blist_pos.y < 0)
			blist_pos.y = 100;
		
		/* ...and move it back. */
		gtk_window_move(GTK_WINDOW(gtkblist->window), blist_pos.x, blist_pos.y);
		gtk_window_resize(GTK_WINDOW(gtkblist->window), blist_pos.width, blist_pos.height);
	}
}

static gboolean gaim_gtk_blist_refresh_timer(struct gaim_buddy_list *list)
{
	GaimBlistNode *group, *buddy;

	for(group = list->root; group; group = group->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(group))
			continue;
		for(buddy = group->child; buddy; buddy = buddy->next) {
			if(!GAIM_BLIST_NODE_IS_BUDDY(buddy))
				continue;
			if (((struct buddy *)buddy)->idle)
				gaim_gtk_blist_update(list, buddy);
		}
	}

	/* keep on going */
	return TRUE;
}

static void gaim_gtk_blist_hide_node(struct gaim_buddy_list *list, GaimBlistNode *node)
{
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GtkTreeIter iter;

	if (!gtknode || !gtknode->row || !gtkblist)
		return;

	if(gtkblist->selected_node == node)
		gtkblist->selected_node = NULL;

	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CHAT(node)) {
			gaim_gtk_blist_update(list, node->parent);
		}
	}
	gtk_tree_row_reference_free(gtknode->row);
	gtknode->row = NULL;
}


/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/
static void gaim_gtk_blist_new_list(struct gaim_buddy_list *blist)
{
	blist->ui_data = g_new0(struct gaim_gtk_buddy_list, 1);
}

static void gaim_gtk_blist_new_node(GaimBlistNode *node)
{
	node->ui_data = g_new0(struct _gaim_gtk_blist_node, 1);
}

void gaim_gtk_blist_update_columns()
{
	if(!gtkblist)
		return;

	if (blist_options & OPT_BLIST_SHOW_ICONS) {
		gtk_tree_view_column_set_visible(gtkblist->buddy_icon_column, TRUE);
		gtk_tree_view_column_set_visible(gtkblist->idle_column, FALSE);
		gtk_tree_view_column_set_visible(gtkblist->warning_column, FALSE);
	} else {
		gtk_tree_view_column_set_visible(gtkblist->idle_column, blist_options & OPT_BLIST_SHOW_IDLETIME);
		gtk_tree_view_column_set_visible(gtkblist->warning_column, blist_options & OPT_BLIST_SHOW_WARN);
		gtk_tree_view_column_set_visible(gtkblist->buddy_icon_column, FALSE);
	}
}

enum {DRAG_BUDDY, DRAG_ROW};

static char *
item_factory_translate_func (const char *path, gpointer func_data)
{
	return _(path);
}

void gaim_gtk_blist_setup_sort_methods()
{
	gaim_gtk_blist_sort_method_reg("None", sort_method_none);
	gaim_gtk_blist_sort_method_reg("Alphabetical", sort_method_alphabetical);
	gaim_gtk_blist_sort_method_reg("By status", sort_method_status);
	gaim_gtk_blist_sort_method_reg("By log size", sort_method_log);
	gaim_gtk_blist_sort_method_set(sort_method[0] ? sort_method : "None");
}		


static void gaim_gtk_blist_show(struct gaim_buddy_list *list)
{
	GtkItemFactory *ift;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *column;
	GtkWidget *sw;
	GtkWidget *button;
	GtkSizeGroup *sg;
	GtkAccelGroup *accel_group;
	GtkTreeSelection *selection;
	GtkTargetEntry gte[] = {{"GAIM_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY}};

	if (gtkblist && gtkblist->window) {
		gtk_widget_show(gtkblist->window);
		return;
	}

	gtkblist = GAIM_GTK_BLIST(list);

	gtkblist->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(gtkblist->window), "buddy_list");
	gtk_window_set_title(GTK_WINDOW(gtkblist->window), _("Buddy List"));

	GTK_WINDOW(gtkblist->window)->allow_shrink = TRUE;

	gtkblist->vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtkblist->window), gtkblist->vbox);

	g_signal_connect(G_OBJECT(gtkblist->window), "delete_event", G_CALLBACK(gtk_blist_delete_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "configure_event", G_CALLBACK(gtk_blist_configure_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "visibility_notify_event", G_CALLBACK(gtk_blist_visibility_cb), NULL);
	gtk_widget_add_events(gtkblist->window, GDK_VISIBILITY_NOTIFY_MASK);

	/******************************* Menu bar *************************************/
	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW (gtkblist->window), accel_group);
	g_object_unref(accel_group);
	ift = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<GaimMain>", accel_group);
	gtk_item_factory_set_translate_func (ift,
					     item_factory_translate_func,
					     NULL, NULL);
	gtk_item_factory_create_items(ift, sizeof(blist_menu) / sizeof(*blist_menu),
				      blist_menu, NULL);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtk_item_factory_get_widget(ift, "<GaimMain>"), FALSE, FALSE, 0);

	awaymenu = gtk_item_factory_get_widget(ift, N_("/Tools/Away"));
	do_away_menu();

	gtkblist->bpmenu = gtk_item_factory_get_widget(ift, N_("/Tools/Buddy Pounce"));
	gaim_gtkpounce_menu_build(gtkblist->bpmenu);

	protomenu = gtk_item_factory_get_widget(ift, N_("/Tools/Protocol Actions"));
	do_proto_menu();

	/****************************** GtkTreeView **********************************/
	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtkblist->treemodel = gtk_tree_store_new(BLIST_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_POINTER);

	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));
	gtk_widget_set_size_request(gtkblist->treeview, -1, 200);

	/* Set up selection stuff */

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(gaim_gtk_blist_selection_changed), NULL);


	/* Set up dnd */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(gtkblist->treeview), GDK_BUTTON1_MASK, gte,
					       2, GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(gtkblist->treeview), gte, 2,
					     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-received", G_CALLBACK(gaim_gtk_blist_drag_data_rcv_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-get", G_CALLBACK(gaim_gtk_blist_drag_data_get_cb), NULL);

	/* Tooltips */
	g_signal_connect(G_OBJECT(gtkblist->treeview), "motion-notify-event", G_CALLBACK(gaim_gtk_blist_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "leave-notify-event", G_CALLBACK(gaim_gtk_blist_leave_cb), NULL);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);

	column = gtk_tree_view_column_new ();

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start (column, rend, FALSE);
	gtk_tree_view_column_set_attributes (column, rend, 
					     "pixbuf", STATUS_ICON_COLUMN,
					     "visible", STATUS_ICON_VISIBLE_COLUMN,
					     NULL);
	g_object_set(rend, "xalign", 0.0, "ypad", 0, NULL);

	rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start (column, rend, TRUE);
	gtk_tree_view_column_set_attributes (column, rend, 
					     "markup", NAME_COLUMN,
					     NULL);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	rend = gtk_cell_renderer_text_new();
	gtkblist->warning_column = gtk_tree_view_column_new_with_attributes("Warning", rend, "markup", WARNING_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->warning_column);
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);

	rend = gtk_cell_renderer_text_new();
	gtkblist->idle_column = gtk_tree_view_column_new_with_attributes("Idle", rend, "markup", IDLE_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->idle_column);
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtkblist->buddy_icon_column = gtk_tree_view_column_new_with_attributes("Buddy Icon", rend, "pixbuf", BUDDY_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->buddy_icon_column);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated", G_CALLBACK(gtk_blist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-expanded", G_CALLBACK(gtk_blist_row_expanded_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-collapsed", G_CALLBACK(gtk_blist_row_collapsed_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(gtkblist->treeview), NAME_COLUMN);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sw, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(sw), gtkblist->treeview);
	gaim_gtk_blist_update_columns();

	/* set the Show Offline Buddies option. must be done
	 * after the treeview or faceprint gets mad. -Robot101
	 */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (ift, N_("/Buddies/Show Offline Buddies"))),
			blist_options & OPT_BLIST_SHOW_OFFLINE);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (ift, N_("/Buddies/Show Empty Groups"))),
			!(blist_options & OPT_BLIST_NO_MT_GRP));

	/* OK... let's show this bad boy. */
	gaim_gtk_blist_refresh(list);
	gaim_gtk_blist_restore_position();
	gtk_widget_show_all(gtkblist->window);

	/**************************** Button Box **************************************/
	/* add this afterwards so it doesn't force up the width of the window         */

	gtkblist->tooltips = gtk_tooltips_new();

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
	gtkblist->bbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->bbox, FALSE, FALSE, 0);
	gtk_widget_show(gtkblist->bbox);

	button = gaim_pixbuf_button_from_stock(_("IM"), GAIM_STOCK_IM, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_im_cb),
			 gtkblist->treeview);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtkblist->tooltips), button, _("Send a message to the selected buddy"), NULL);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(_("Get Info"), GAIM_STOCK_INFO, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_info_cb),
			 gtkblist->treeview);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtkblist->tooltips), button, _("Get information on the selected buddy"), NULL);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(_("Chat"), GAIM_STOCK_CHAT, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_chat_cb), gtkblist->treeview);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtkblist->tooltips), button, _("Join a chat room"), NULL);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(_("Away"), GAIM_STOCK_ICON_AWAY, GAIM_BUTTON_VERTICAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gtk_blist_button_away_cb), NULL);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtkblist->tooltips), button, _("Set an away message"), NULL);
	gtk_widget_show(button);

	/* this will show the right image/label widgets for us */
	gaim_gtk_blist_update_toolbar();

	/* start the refresh timer */
	if (blist_options & (OPT_BLIST_SHOW_IDLETIME | OPT_BLIST_SHOW_ICONS))
		gtkblist->refresh_timer = g_timeout_add(30000, (GSourceFunc)gaim_gtk_blist_refresh_timer, list);
}

static void redo_buddy_list(struct gaim_buddy_list *list, gboolean remove)
{
	GaimBlistNode *group, *buddy;
	
	for(group = list->root; group; group = group->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(group))
			continue;
		gaim_gtk_blist_update(list, group);
		for(buddy = group->child; buddy; buddy = buddy->next) {
			if (remove)
				gaim_gtk_blist_hide_node(list, buddy);
			gaim_gtk_blist_update(list, buddy);
		}
	}
}

void gaim_gtk_blist_refresh(struct gaim_buddy_list *list)
{
	redo_buddy_list(list, FALSE);
}

void
gaim_gtk_blist_update_refresh_timeout()
{
	struct gaim_buddy_list *blist;
	struct gaim_gtk_buddy_list *gtkblist;

	blist = gaim_get_blist();
	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (blist_options & (OPT_BLIST_SHOW_IDLETIME | OPT_BLIST_SHOW_ICONS)) {
		gtkblist->refresh_timer = g_timeout_add(30000, (GSourceFunc)gaim_gtk_blist_refresh_timer, blist);
	} else {
		g_source_remove(gtkblist->refresh_timer);
		gtkblist->refresh_timer = 0;
	}
}

static gboolean get_iter_from_node(GaimBlistNode *node, GtkTreeIter *iter) {
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GtkTreePath *path;

	if (!gtknode) {
		gaim_debug(GAIM_DEBUG_ERROR, "gtkblist", "buddy %s has no ui_data\n", ((struct buddy *)node)->name);
		return FALSE;
	}

	if (!gtkblist) {
		gaim_debug(GAIM_DEBUG_ERROR, "gtkblist", "get_iter_from_node was called, but we don't seem to have a blist\n");
		return FALSE;
	}

	if (!gtknode->row)
		return FALSE;

	if ((path = gtk_tree_row_reference_get_path(gtknode->row)) == NULL)
		return FALSE;
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}
	gtk_tree_path_free(path);
	return TRUE;
}

/*
 * These state assignments suck. I'm sorry. They're for historical reasons.
 * Roll on new prefs. -Robot101
 * 
 * NO_BUTTON_TEXT && SHOW_BUTTON_XPM - image
 * !NO_BUTTON_TEXT && !SHOW_BUTTON_XPM - text
 * !NO_BUTTON_TEXT && SHOW_BUTTON_XPM - text & images
 * NO_BUTTON_TEXT && !SHOW_BUTTON_XPM - none
 */

static void gaim_gtk_blist_update_toolbar_icons (GtkWidget *widget, gpointer data) {
	if (GTK_IS_IMAGE(widget)) {
		if (blist_options & OPT_BLIST_SHOW_BUTTON_XPM)
			gtk_widget_show(widget);
		else
			gtk_widget_hide(widget);
	} else if (GTK_IS_LABEL(widget)) {
		if (blist_options & OPT_BLIST_NO_BUTTON_TEXT)
			gtk_widget_hide(widget);
		else
			gtk_widget_show(widget);
	} else if (GTK_IS_CONTAINER(widget)) {
		gtk_container_foreach(GTK_CONTAINER(widget), gaim_gtk_blist_update_toolbar_icons, NULL);
	}
}

void gaim_gtk_blist_update_toolbar() {
	if (!gtkblist)
		return;

	if (blist_options & OPT_BLIST_NO_BUTTON_TEXT && !(blist_options & OPT_BLIST_SHOW_BUTTON_XPM))
		gtk_widget_hide(gtkblist->bbox);
	else {
		gtk_container_foreach(GTK_CONTAINER(gtkblist->bbox), gaim_gtk_blist_update_toolbar_icons, NULL);
		gtk_widget_show(gtkblist->bbox);
	}
}

static void gaim_gtk_blist_remove(struct gaim_buddy_list *list, GaimBlistNode *node)
{
	gaim_gtk_blist_hide_node(list, node);

	/* There's something I don't understand here */
	/* g_free(node->ui_data);
	node->ui_data = NULL; */
}

static gboolean do_selection_changed(GaimBlistNode *new_selection)
{
	GaimBlistNode *old_selection = NULL;

	/* test for gtkblist because crazy timeout means we can be called after the blist is gone */
	if (gtkblist && new_selection != gtkblist->selected_node) {
		old_selection = gtkblist->selected_node;
		gtkblist->selected_node = new_selection;
		if(new_selection)
			gaim_gtk_blist_update(NULL, new_selection);
		if(old_selection)
			gaim_gtk_blist_update(NULL, old_selection);
	}

	return FALSE;
}

static void gaim_gtk_blist_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	GaimBlistNode *new_selection = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_get_selected(selection, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &new_selection, -1);
	}

	/* we set this up as a timeout, otherwise the blist flickers */
	g_timeout_add(0, (GSourceFunc)do_selection_changed, new_selection);
}

static void make_a_group(GaimBlistNode *node, GtkTreeIter *iter) {
	GaimBlistNode *sibling;
	GtkTreeIter siblingiter;
	GtkTreePath *newpath;
	struct group *group = (struct group *)node;
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	char *esc = g_markup_escape_text(group->name, -1);
	char *mark;

	if(blist_options & OPT_BLIST_SHOW_GRPNUM)
		mark = g_strdup_printf("<span weight='bold'>%s</span> (%d/%d)", esc, gaim_blist_get_group_online_count(group), gaim_blist_get_group_size(group, FALSE));
	else
		mark = g_strdup_printf("<span weight='bold'>%s</span>", esc);

	g_free(esc);

	sibling = node->prev;
	while (sibling && !get_iter_from_node(sibling, &siblingiter)) {
		sibling = sibling->prev;
	}

	gtk_tree_store_insert_after(gtkblist->treemodel, iter, NULL,
			sibling ? &siblingiter : NULL);
	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), iter);
	gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
	gtk_tree_path_free(newpath);

	gtk_tree_store_set(gtkblist->treemodel, iter,
			STATUS_ICON_COLUMN, NULL,
			STATUS_ICON_VISIBLE_COLUMN, FALSE,
			NAME_COLUMN, mark,
			NODE_COLUMN, node,
			-1);
	g_free(mark);
}

static void gaim_gtk_blist_update(struct gaim_buddy_list *list, GaimBlistNode *node)
{
	GtkTreeIter iter;
	GtkTreePath *expand = NULL, *newpath = NULL;
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	gboolean new_entry = FALSE;

	if (!gtkblist || !gtknode)
		return;

	if (!get_iter_from_node(node, &iter)) {
		new_entry = TRUE;
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			if (((struct buddy*)node)->present != GAIM_BUDDY_OFFLINE || ((blist_options & OPT_BLIST_SHOW_OFFLINE) && ((struct buddy*)node)->account->gc)) {
				GtkTreeIter groupiter;
				GaimBlistNode *oldersibling;
				GtkTreeIter oldersiblingiter;
				char *collapsed = gaim_group_get_setting((struct group *)node->parent, "collapsed");

				if(node->parent &&
						!get_iter_from_node(node->parent, &groupiter)) {
					/* This buddy's group has not yet been added.
					 * We do that here */
					make_a_group(node->parent, &groupiter);
				}
				if(!collapsed)
					expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter);
				else
					g_free(collapsed);
				
				iter = current_sort_method->func(node, list, groupiter, NULL);
			
				if (blist_options & OPT_BLIST_POPUP) {
					gtk_widget_show(gtkblist->window);
					gtk_window_deiconify(GTK_WINDOW(gtkblist->window));
					gdk_window_raise(gtkblist->window->window);
				}

			}
		} else if (GAIM_BLIST_NODE_IS_CHAT(node) &&
				((struct chat *)node)->account->gc) {
			GtkTreeIter groupiter;
			GaimBlistNode *oldersibling;
			GtkTreeIter oldersiblingiter;
			char *collapsed = gaim_group_get_setting((struct group *)node->parent, "collapsed");

			if(node->parent &&
					!get_iter_from_node(node->parent, &groupiter)) {
				/* This buddy's group has not yet been added.
				 * We do that here */
				make_a_group(node->parent, &groupiter);
			}
			if(!collapsed)
				expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter);
			else
				g_free(collapsed);

			oldersibling = node->prev;
			while (oldersibling && !get_iter_from_node(oldersibling, &oldersiblingiter)) {
				oldersibling = oldersibling->prev;
			}

			gtk_tree_store_insert_after(gtkblist->treemodel, &iter, &groupiter, oldersibling ? &oldersiblingiter : NULL);
			newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
			gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
			gtk_tree_path_free(newpath);

		} else if (GAIM_BLIST_NODE_IS_GROUP(node) &&
					((blist_options & OPT_BLIST_SHOW_OFFLINE) ||
					!(blist_options & OPT_BLIST_NO_MT_GRP))) {
			make_a_group(node, &iter);
			expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		}
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		if((blist_options & OPT_BLIST_NO_MT_GRP) && !(blist_options & OPT_BLIST_SHOW_OFFLINE) && !gaim_blist_get_group_online_count((struct group *)node)) {
			gtk_tree_store_remove(gtkblist->treemodel, &iter);
		} else {
			struct group *group = (struct group *)node;
			char *esc = g_markup_escape_text(group->name, -1);
			char *mark;

			if(blist_options & OPT_BLIST_SHOW_GRPNUM)
				mark = g_strdup_printf("<span weight='bold'>%s</span> (%d/%d)", esc, gaim_blist_get_group_online_count(group), gaim_blist_get_group_size(group, FALSE));
			else
				mark = g_strdup_printf("<span weight='bold'>%s</span>", esc);

			g_free(esc);
			gtk_tree_store_set(gtkblist->treemodel, &iter,
					NAME_COLUMN, mark,
					-1);
			g_free(mark);
		}
	}

	if (GAIM_BLIST_NODE_IS_CHAT(node) && ((struct chat*)node)->account->gc) {
		GdkPixbuf *status;
		struct chat *chat = (struct chat *)node;
		char *name;

		status = gaim_gtk_blist_get_status_icon(node,
							blist_options & OPT_BLIST_SHOW_ICONS ? GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL);
		if(chat->alias) {
			name = g_markup_escape_text(chat->alias, -1);
		} else {
			struct proto_chat_entry *pce;
			GList *parts, *tmp;

			parts = GAIM_PLUGIN_PROTOCOL_INFO(chat->account->gc->prpl)->chat_info(chat->account->gc);
			pce = parts->data;
			name = g_markup_escape_text(g_hash_table_lookup(chat->components,
						pce->identifier), -1);
			for(tmp = parts; tmp; tmp = tmp->next)
				g_free(tmp->data);
			g_list_free(parts);
		}


		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_COLUMN, status,
				   STATUS_ICON_VISIBLE_COLUMN, TRUE,
				   NAME_COLUMN, name,
				   NODE_COLUMN, node,
				   -1);

		g_free(name);
		if (status != NULL)
			g_object_unref(status);
	} else if(GAIM_BLIST_NODE_IS_CHAT(node) && !((struct chat *)node)->account->gc) {
		gaim_gtk_blist_hide_node(list, node);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node) && (((struct buddy*)node)->present != GAIM_BUDDY_OFFLINE || ((blist_options & OPT_BLIST_SHOW_OFFLINE) && ((struct buddy*)node)->account->gc))) {
		GdkPixbuf *status, *avatar;
		GtkTreeIter groupiter;
		char *mark;
		char *warning = NULL, *idle = NULL;

		gboolean selected = (gtkblist->selected_node == node);

		status = gaim_gtk_blist_get_status_icon(node,
							blist_options & OPT_BLIST_SHOW_ICONS ? GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL);
		avatar = gaim_gtk_blist_get_buddy_icon((struct buddy*)node);
		mark   = gaim_gtk_blist_get_name_markup((struct buddy*)node, selected);

		if (((struct buddy*)node)->idle > 0) {
			time_t t;
			int ihrs, imin;
			time(&t);
			ihrs = (t - ((struct buddy *)node)->idle) / 3600;
			imin = ((t - ((struct buddy*)node)->idle) / 60) % 60;
			if(ihrs > 0)
				idle = g_strdup_printf("(%d:%02d)", ihrs, imin);
			else
				idle = g_strdup_printf("(%d)", imin);
		}

		if (((struct buddy*)node)->evil > 0)
			warning = g_strdup_printf("%d%%", ((struct buddy*)node)->evil);


		if((blist_options & OPT_BLIST_GREY_IDLERS)
				&& ((struct buddy *)node)->idle) {
			if(warning && !selected) {
				char *w2 = g_strdup_printf("<span color='dim grey'>%s</span>",
						warning);
				g_free(warning);
				warning = w2;
			}

			if(idle && !selected) {
				char *i2 = g_strdup_printf("<span color='dim grey'>%s</span>",
						idle);
				g_free(idle);
				idle = i2;
			}
		}
		if (!selected) {
			get_iter_from_node(node->parent, &groupiter);
			iter = current_sort_method->func(node, list, groupiter, &iter);
		}

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_COLUMN, status,
				   STATUS_ICON_VISIBLE_COLUMN, TRUE,
				   NAME_COLUMN, mark,
				   WARNING_COLUMN, warning,
				   IDLE_COLUMN, idle,
				   BUDDY_ICON_COLUMN, avatar,
				   NODE_COLUMN, node,
				   -1);

		if (blist_options & OPT_BLIST_POPUP &&
				((struct buddy *)node)->present == GAIM_BUDDY_SIGNING_OFF) {
			gtk_widget_show(gtkblist->window);
			gtk_window_deiconify(GTK_WINDOW(gtkblist->window));
			gdk_window_raise(gtkblist->window->window);
		}

		g_free(mark);
		if (idle)
			g_free(idle);
		if (warning)
			g_free(warning);

		if (status != NULL)
			g_object_unref(status);

		if (avatar != NULL)
			g_object_unref(avatar);

	} else if (GAIM_BLIST_NODE_IS_BUDDY(node) && !new_entry) {
		gaim_gtk_blist_hide_node(list, node);
	}

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gtkblist->treeview));


	if(expand) {
		gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), expand, TRUE);
		gtk_tree_path_free(expand);
	}

	if(GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CHAT(node))
		gaim_gtk_blist_update(list, node->parent);
}

static void gaim_gtk_blist_destroy(struct gaim_buddy_list *list)
{
	if (!gtkblist)
		return;

	gtk_widget_destroy(gtkblist->window);
	gtk_object_sink(GTK_OBJECT(gtkblist->tooltips));

	if (gtkblist->refresh_timer)
		g_source_remove(gtkblist->refresh_timer);
	if (gtkblist->timeout)
		g_source_remove(gtkblist->timeout);

	gtkblist->refresh_timer = 0;
	gtkblist->timeout = 0;
	gtkblist->window = gtkblist->vbox = gtkblist->treeview = NULL;
	gtkblist->treemodel = NULL;
	gtkblist->idle_column = NULL;
	gtkblist->warning_column = gtkblist->buddy_icon_column = NULL;
	gtkblist->bbox = gtkblist->tipwindow = NULL;
	protomenu = NULL;
	awaymenu = NULL;
	gtkblist = NULL;
}

static void gaim_gtk_blist_set_visible(struct gaim_buddy_list *list, gboolean show)
{
	if (!(gtkblist && gtkblist->window))
		return;

	if (show) {
		gaim_gtk_blist_restore_position();
		gtk_window_present(GTK_WINDOW(gtkblist->window));
	} else {
		if (!connections || docklet_count) {
#ifdef _WIN32
			wgaim_systray_minimize(gtkblist->window);
#endif
			gtk_widget_hide(gtkblist->window);
		} else {
			gtk_window_iconify(GTK_WINDOW(gtkblist->window));
		}
	}
}

void gaim_gtk_blist_docklet_toggle() {
	/* Useful for the docklet plugin and also for the win32 tray icon*/
	/* This is called when one of those is clicked--it will show/hide the 
	   buddy list/login window--depending on which is active */
	if (connections) {
		if (gtkblist && gtkblist->window) {
			if (GTK_WIDGET_VISIBLE(gtkblist->window)) {
				gaim_blist_set_visible(GAIM_WINDOW_ICONIFIED(gtkblist->window) || gaim_gtk_blist_obscured);
			} else {
#if _WIN32
				wgaim_systray_maximize(gtkblist->window);
#endif
				gaim_blist_set_visible(TRUE);
			}
		} else {
			/* we're logging in or something... do nothing */
			/* or should I make the blist? */
			gaim_debug(GAIM_DEBUG_WARNING, "blist",
					   "docklet_toggle called with connections "
					   "but no blist!\n");
		}
	} else if (mainwindow) {
		if (GTK_WIDGET_VISIBLE(mainwindow)) {
			if (GAIM_WINDOW_ICONIFIED(mainwindow)) {
				gtk_window_present(GTK_WINDOW(mainwindow));
			} else {
#if _WIN32
				wgaim_systray_minimize(mainwindow);
#endif
				gtk_widget_hide(mainwindow);
			}
		} else {
#if _WIN32
			wgaim_systray_maximize(mainwindow);
#endif
			show_login();
		}
	} else {
		show_login();
	}
}

void gaim_gtk_blist_docklet_add()
{
	docklet_count++;
}

void gaim_gtk_blist_docklet_remove()
{
	docklet_count--;
	if (!docklet_count) {
		if (connections)
			gaim_blist_set_visible(TRUE);
		else if (mainwindow)
			gtk_window_present(GTK_WINDOW(mainwindow));
		else
			show_login();
	}
}

static struct gaim_blist_ui_ops blist_ui_ops =
{
	gaim_gtk_blist_new_list,
	gaim_gtk_blist_new_node,
	gaim_gtk_blist_show,
	gaim_gtk_blist_update,
	gaim_gtk_blist_remove,
	gaim_gtk_blist_destroy,
	gaim_gtk_blist_set_visible
};


struct gaim_blist_ui_ops *gaim_get_gtk_blist_ui_ops()
{
	return &blist_ui_ops;
}



/*********************************************************************
 * Public utility functions                                          *
 *********************************************************************/

GdkPixbuf *
create_prpl_icon(struct gaim_account *account)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	GdkPixbuf *status = NULL;
	char *filename = NULL;
	const char *protoname = NULL;
	char buf[256];

	prpl = gaim_find_prpl(account->protocol);

	if (prpl != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl_info->list_icon != NULL)
			protoname = prpl_info->list_icon(account, NULL);
	}

	if (protoname == NULL)
		return NULL;

	/*
	 * Status icons will be themeable too, and then it will look up
	 * protoname from the theme
	 */
	g_snprintf(buf, sizeof(buf), "%s.png", protoname);

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
								"default", buf, NULL);
	status = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	return status;
}


/*********************************************************************
 * Buddy List sorting functions                                          *
 *********************************************************************/

void gaim_gtk_blist_sort_method_reg(const char *name, gaim_gtk_blist_sort_function func)
{
	struct gaim_gtk_blist_sort_method *method = g_new0(struct gaim_gtk_blist_sort_method, 1);
	method->name = g_strdup(name);
	method->func = func;
	gaim_gtk_blist_sort_methods = g_slist_append(gaim_gtk_blist_sort_methods, method);
}

void gaim_gtk_blist_sort_method_unreg(const char *name){

}

void gaim_gtk_blist_sort_method_set(const char *name){
	GSList *l = gaim_gtk_blist_sort_methods;
	while (l && gaim_utf8_strcasecmp(((struct gaim_gtk_blist_sort_method*)l->data)->name, name))
		l = l->next;
	
	if (l) {
		current_sort_method = l->data;
		strcpy(sort_method, ((struct gaim_gtk_blist_sort_method*)l->data)->name);
	} else if (!current_sort_method) {
		gaim_gtk_blist_sort_method_set("None");
		return;
	}
	save_prefs();
	redo_buddy_list(gaim_get_blist(), TRUE);

}

/******************************************
 ** Sort Methods
 ******************************************/

/* A sort method takes a core buddy list node, the buddy list it belongs in, the GtkTreeIter of its group and
 * the nodes own iter if it has one.  It returns the iter the buddy list should use to represent this buddy, be
 * it a new iter, or an existing one.  If it is a new iter, and cur is defined, the buddy list will probably want
 * to remove cur from the buddy list. */
static GtkTreeIter sort_method_none(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur)
{
	GtkTreePath *newpath;
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GaimBlistNode *oldersibling = node->prev;
	GtkTreeIter iter, oldersiblingiter;
	
	if (cur)
		return *cur;
	
	while (oldersibling && !get_iter_from_node(oldersibling, &oldersiblingiter)) {
		oldersibling = oldersibling->prev;
	}
	
	gtk_tree_store_insert_after(gtkblist->treemodel, &iter, &groupiter, oldersibling ? &oldersiblingiter : NULL);
	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
	gtk_tree_path_free(newpath);
	return iter;
}

static GtkTreeIter sort_method_alphabetical(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur)
{
	GtkTreeIter more_z, iter;
	GaimBlistNode *n;
	GtkTreePath *newpath;
	GValue val = {0,};
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

	if (cur) 
		return *cur;
	

	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, &iter, &groupiter, 0);
		newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
		gtk_tree_path_free(newpath);
		return iter;
	}

	do {
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);
		
		if (GAIM_BLIST_NODE_IS_BUDDY(n) && gaim_utf8_strcasecmp(gaim_get_buddy_alias((struct buddy*)node), 
									gaim_get_buddy_alias((struct buddy*)n)) < 0) {
			gtk_tree_store_insert_before(gtkblist->treemodel, &iter, &groupiter, &more_z);
			newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
			gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
			gtk_tree_path_free(newpath);
			return iter;
		}
		g_value_unset(&val);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));
													  
	gtk_tree_store_append(gtkblist->treemodel, &iter, &groupiter);
	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
	gtk_tree_path_free(newpath);
	return iter;
}

static GtkTreeIter sort_method_status(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur)
{
	GtkTreeIter more_z, iter;
	GaimBlistNode *n;
	GtkTreePath *newpath, *expand;
	GValue val = {0,};
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	char *collapsed = gaim_group_get_setting((struct group *)node->parent, "collapsed");
	if(!collapsed)
		expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter);
	else
		g_free(collapsed);
	
	
	if (cur)
		gaim_gtk_blist_hide_node(blist, node);
	
	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, &iter, &groupiter, 0);
		newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
		gtk_tree_path_free(newpath);
		if(expand) {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), expand, TRUE);
			gtk_tree_path_free(expand);
		}
		return iter;
	}

	do {
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);
		
		if (n && GAIM_BLIST_NODE_IS_BUDDY(n)) {
			struct buddy *new = (struct buddy*)node, *it = (struct buddy*)n;
			if (it->idle > new->idle)
				{
					printf("Inserting %s before %s\n", new->name, it->name);
					gtk_tree_store_insert_before(gtkblist->treemodel, &iter, &groupiter, &more_z);
						newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
						gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
						gtk_tree_path_free(newpath);
						return iter;
				}
			g_value_unset(&val);
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	gtk_tree_store_append(gtkblist->treemodel, &iter, &groupiter);
	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
	gtk_tree_path_free(newpath);
	return iter;
}

static GtkTreeIter sort_method_log(GaimBlistNode *node, struct gaim_buddy_list *blist, GtkTreeIter groupiter, GtkTreeIter *cur)
{
	GtkTreeIter more_z, iter;
	GaimBlistNode *n;
	GtkTreePath *newpath;
	GValue val = {0,};
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	char *logname = g_strdup_printf("%s.log", normalize(((struct buddy*)node)->name));
	char *filename = g_build_filename(gaim_user_dir(), "logs", logname, NULL);
	struct stat st, st2;
	
	if (cur)
		return *cur;

	if (stat(filename, &st))
		st.st_size = 0;
	g_free(filename);
	g_free(logname);	

	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, &iter, &groupiter, 0);
		newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
		gtk_tree_path_free(newpath);
		return iter;
	}

	do {
		
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);
		
		logname = g_strdup_printf("%s.log", normalize(((struct buddy*)n)->name));
		filename = g_build_filename(gaim_user_dir(), "logs", logname, NULL);
		if (stat(filename, &st2))
			st2.st_size = 0;
		g_free(filename);
		g_free(logname);
		if (GAIM_BLIST_NODE_IS_BUDDY(n) && st.st_size > st2.st_size) {
			gtk_tree_store_insert_before(gtkblist->treemodel, &iter, &groupiter, &more_z);
			newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
			gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
			gtk_tree_path_free(newpath);
			return iter;
		}
		g_value_unset(&val);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));
													  
	gtk_tree_store_append(gtkblist->treemodel, &iter, &groupiter);
	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	gtknode->row = gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel), newpath);
	gtk_tree_path_free(newpath);
	return iter;
}
