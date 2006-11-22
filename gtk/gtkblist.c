/*
 * @file gtkblist.c GTK+ BuddyList API
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
 *
 */
#include "internal.h"
#include "gtkgaim.h"

#include "account.h"
#include "connection.h"
#include "core.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "prefs.h"
#include "plugin.h"
#include "request.h"
#include "signals.h"
#include "gaimstock.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkcellrendererexpander.h"
#include "gtkconv.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkft.h"
#include "gtklog.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

typedef struct
{
	GaimAccount *account;

	GtkWidget *window;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *entry_for_alias;
	GtkWidget *account_box;

} GaimGtkAddBuddyData;

typedef struct
{
	GaimAccount *account;
	gchar *default_chat_name;

	GtkWidget *window;
	GtkWidget *account_menu;
	GtkWidget *alias_entry;
	GtkWidget *group_combo;
	GtkWidget *entries_box;
	GtkSizeGroup *sg;

	GList *entries;

} GaimGtkAddChatData;

typedef struct
{
	GaimAccount *account;

	GtkWidget *window;
	GtkWidget *account_menu;
	GtkWidget *entries_box;
	GtkSizeGroup *sg;

	GList *entries;
} GaimGtkJoinChatData;


static GtkWidget *accountmenu = NULL;

static guint visibility_manager_count = 0;
static gboolean gtk_blist_obscured = FALSE;
GHashTable* status_icon_hash_table = NULL;

static GList *gaim_gtk_blist_sort_methods = NULL;
static struct gaim_gtk_blist_sort_method *current_sort_method = NULL;
static void sort_method_none(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

/* The functions we use for sorting aren't available in gtk 2.0.x, and
 * segfault in 2.2.0.  2.2.1 is known to work, so I'll require that */
#if GTK_CHECK_VERSION(2,2,1)
static void sort_method_alphabetical(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_status(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_log(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
#endif
static GaimGtkBuddyList *gtkblist = NULL;

static gboolean gaim_gtk_blist_refresh_timer(GaimBuddyList *list);
static void gaim_gtk_blist_update_buddy(GaimBuddyList *list, GaimBlistNode *node, gboolean statusChange);
static void gaim_gtk_blist_selection_changed(GtkTreeSelection *selection, gpointer data);
static void gaim_gtk_blist_update(GaimBuddyList *list, GaimBlistNode *node);
static void gaim_gtk_blist_update_contact(GaimBuddyList *list, GaimBlistNode *node);
static char *gaim_get_tooltip_text(GaimBlistNode *node, gboolean full);
static const char *item_factory_translate_func (const char *path, gpointer func_data);
static gboolean get_iter_from_node(GaimBlistNode *node, GtkTreeIter *iter);
static void redo_buddy_list(GaimBuddyList *list, gboolean remove, gboolean rerender);
static void gaim_gtk_blist_collapse_contact_cb(GtkWidget *w, GaimBlistNode *node);

static void gaim_gtk_blist_tooltip_destroy(void);

struct _gaim_gtk_blist_node {
	GtkTreeRowReference *row;
	gboolean contact_expanded;
	gboolean recent_signonoff;
	gint recent_signonoff_timer;
	GString *status_icon_key;
};

static void gaim_gtk_blist_update_buddy_status_icon_key(struct _gaim_gtk_blist_node *gtkbuddynode,
	GaimBuddy *buddy, GaimStatusIconSize size);


static char dim_grey_string[8] = "";
static char *dim_grey()
{
	if (!gtkblist)
		return "dim grey";
	if (!dim_grey_string[0]) {
		GtkStyle *style = gtk_widget_get_style(gtkblist->treeview);
		snprintf(dim_grey_string, sizeof(dim_grey_string), "#%02x%02x%02x",
			 style->text_aa[GTK_STATE_NORMAL].red >> 8,
			 style->text_aa[GTK_STATE_NORMAL].green >> 8,
			 style->text_aa[GTK_STATE_NORMAL].blue >> 8);
	}
	return dim_grey_string;
}

/***************************************************
 *              Callbacks                          *
 ***************************************************/
static gboolean gtk_blist_visibility_cb(GtkWidget *w, GdkEventVisibility *event, gpointer data)
{
	if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
		gtk_blist_obscured = TRUE;
	else if (gtk_blist_obscured) {
			gtk_blist_obscured = FALSE;
			gaim_gtk_blist_refresh_timer(gaim_get_blist());
	}

	/* continue to handle event normally */
	return FALSE;
}

static gboolean gtk_blist_window_state_cb(GtkWidget *w, GdkEventWindowState *event, gpointer data)
{
	if(event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN) {
		if(event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN)
			gaim_prefs_set_bool("/gaim/gtk/blist/list_visible", FALSE);
		else {
			gaim_prefs_set_bool("/gaim/gtk/blist/list_visible", TRUE);
			gaim_gtk_blist_refresh_timer(gaim_get_blist());
		}
	}

	if(event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		if(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
			gaim_prefs_set_bool("/gaim/gtk/blist/list_maximized", TRUE);
		else
			gaim_prefs_set_bool("/gaim/gtk/blist/list_maximized", FALSE);
	}

	/* Refresh gtkblist if un-iconifying */
	if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED){
		if (!(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED))
			gaim_gtk_blist_refresh_timer(gaim_get_blist());
	}

	return FALSE;
}

static gboolean gtk_blist_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	if(visibility_manager_count)
		gaim_blist_set_visible(FALSE);
	else
		gaim_core_quit();

	/* we handle everything, event should not propogate further */
	return TRUE;
}

static gboolean gtk_blist_configure_cb(GtkWidget *w, GdkEventConfigure *event, gpointer data)
{
	/* unfortunately GdkEventConfigure ignores the window gravity, but  *
	 * the only way we have of setting the position doesn't. we have to *
	 * call get_position because it does pay attention to the gravity.  *
	 * this is inefficient and I agree it sucks, but it's more likely   *
	 * to work correctly.                                    - Robot101 */
	gint x, y;

	/* check for visibility because when we aren't visible, this will   *
	 * give us bogus (0,0) coordinates.                      - xOr      */
	if (GTK_WIDGET_VISIBLE(w))
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);
	else
		return FALSE; /* carry on normally */

#ifdef _WIN32
	/* Workaround for GTK+ bug # 169811 - "configure_event" is fired
	 * when the window is being maximized */
	if (gdk_window_get_state(w->window)
			& GDK_WINDOW_STATE_MAXIMIZED) {
		return FALSE;
	}
#endif

	/* don't save if nothing changed */
	if (x == gaim_prefs_get_int("/gaim/gtk/blist/x") &&
		y == gaim_prefs_get_int("/gaim/gtk/blist/y") &&
		event->width  == gaim_prefs_get_int("/gaim/gtk/blist/width") &&
		event->height == gaim_prefs_get_int("/gaim/gtk/blist/height")) {

		return FALSE; /* carry on normally */
	}

	/* don't save off-screen positioning */
	if (x + event->width < 0 ||
		y + event->height < 0 ||
		x > gdk_screen_width() ||
		y > gdk_screen_height()) {

		return FALSE; /* carry on normally */
	}

	/* ignore changes when maximized */
	if(gaim_prefs_get_bool("/gaim/gtk/blist/list_maximized"))
		return FALSE;

	/* store the position */
	gaim_prefs_set_int("/gaim/gtk/blist/x",      x);
	gaim_prefs_set_int("/gaim/gtk/blist/y",      y);
	gaim_prefs_set_int("/gaim/gtk/blist/width",  event->width);
	gaim_prefs_set_int("/gaim/gtk/blist/height", event->height);

	/* continue to handle event normally */
	return FALSE;
}

static void gtk_blist_menu_info_cb(GtkWidget *w, GaimBuddy *b)
{
	serv_get_info(b->account->gc, b->name);
}

static void gtk_blist_menu_im_cb(GtkWidget *w, GaimBuddy *b)
{
	gaim_gtkdialogs_im_with_user(b->account, b->name);
}

static void gtk_blist_menu_send_file_cb(GtkWidget *w, GaimBuddy *b)
{
	serv_send_file(b->account->gc, b->name, NULL);
}

static void gtk_blist_menu_autojoin_cb(GtkWidget *w, GaimChat *chat)
{
	gaim_blist_node_set_bool((GaimBlistNode*)chat, "gtk-autojoin",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
}

static void gtk_blist_join_chat(GaimChat *chat)
{
	GaimConversation *conv;

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT,
											   gaim_chat_get_name(chat),
											   chat->account);

	if (conv != NULL)
		gaim_gtkconv_present_conversation(conv);

	serv_join_chat(chat->account->gc, chat->components);
}

static void gtk_blist_menu_join_cb(GtkWidget *w, GaimChat *chat)
{
	gtk_blist_join_chat(chat);
}

static void gtk_blist_renderer_edited_cb(GtkCellRendererText *text_rend, char *arg1,
					 char *arg2, gpointer nada)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GValue val;
	GaimBlistNode *node;
	GaimGroup *dest;

	path = gtk_tree_path_new_from_string (arg1);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), TRUE);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", FALSE, NULL);

	switch (node->type)
	{
		case GAIM_BLIST_CONTACT_NODE:
			{
				GaimContact *contact = (GaimContact *)node;
				struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

				if (contact->alias || gtknode->contact_expanded)
					gaim_blist_alias_contact(contact, arg2);
				else
				{
					GaimBuddy *buddy = gaim_contact_get_priority_buddy(contact);
					gaim_blist_alias_buddy(buddy, arg2);
					serv_alias_buddy(buddy);
				}
			}
			break;

		case GAIM_BLIST_BUDDY_NODE:
			gaim_blist_alias_buddy((GaimBuddy*)node, arg2);
			serv_alias_buddy((GaimBuddy *)node);
			break;
		case GAIM_BLIST_GROUP_NODE:
			dest = gaim_find_group(arg2);
			if (dest != NULL && strcmp(arg2, ((GaimGroup*) node)->name)) {
				gaim_gtkdialogs_merge_groups((GaimGroup*) node, arg2);
			} else
				gaim_blist_rename_group((GaimGroup*)node, arg2);
			break;
		case GAIM_BLIST_CHAT_NODE:
			gaim_blist_alias_chat((GaimChat*)node, arg2);
			break;
		default:
			break;
	}
}

static void gtk_blist_menu_alias_cb(GtkWidget *w, GaimBlistNode *node)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	const char *text = NULL;
	char *esc;

	if (!(get_iter_from_node(node, &iter))) {
		/* This is either a bug, or the buddy is in a collapsed contact */
		node = node->parent;
		if (!get_iter_from_node(node, &iter))
			/* Now it's definitely a bug */
			return;
	}

	switch (node->type) {
	case GAIM_BLIST_BUDDY_NODE:
		text = gaim_buddy_get_alias((GaimBuddy *)node);
		break;
	case GAIM_BLIST_CONTACT_NODE:
		text = gaim_contact_get_alias((GaimContact *)node);
		break;
	case GAIM_BLIST_GROUP_NODE:
		text = ((GaimGroup *)node)->name;
		break;
	case GAIM_BLIST_CHAT_NODE:
		text = gaim_chat_get_name((GaimChat *)node);
		break;
	default:
		g_return_if_reached();
	}

	esc = g_markup_escape_text(text, -1);
	gtk_tree_store_set(gtkblist->treemodel, &iter, NAME_COLUMN, esc, -1);
	g_free(esc);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", TRUE, NULL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), FALSE);
	gtk_widget_grab_focus(gtkblist->treeview);
#if GTK_CHECK_VERSION(2,2,0)
	gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(gtkblist->treeview), path,
			gtkblist->text_column, gtkblist->text_rend, TRUE);
#else
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(gtkblist->treeview), path, gtkblist->text_column, TRUE);
#endif
	gtk_tree_path_free(path);
}

static void gtk_blist_menu_bp_cb(GtkWidget *w, GaimBuddy *b)
{
	gaim_gtk_pounce_editor_show(b->account, b->name, NULL);
}

static void gtk_blist_menu_showlog_cb(GtkWidget *w, GaimBlistNode *node)
{
	GaimLogType type;
	GaimAccount *account;
	char *name = NULL;

	gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *b = (GaimBuddy*) node;
		type = GAIM_LOG_IM;
		name = g_strdup(b->name);
		account = b->account;
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		GaimChat *c = (GaimChat*) node;
		GaimPluginProtocolInfo *prpl_info = NULL;
		type = GAIM_LOG_CHAT;
		account = c->account;
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gaim_find_prpl(gaim_account_get_protocol_id(account)));
		if (prpl_info && prpl_info->get_chat_name) {
			name = prpl_info->get_chat_name(c->components);
		}
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		gaim_gtk_log_show_contact((GaimContact *)node);
		gaim_gtk_clear_cursor(gtkblist->window);
		return;
	} else {
		gaim_gtk_clear_cursor(gtkblist->window);

		/* This callback should not have been registered for a node
		 * that doesn't match the type of one of the blocks above. */
		g_return_if_reached();
	}

	if (name && account) {
		gaim_gtk_log_show(type, name, account);
		g_free(name);

		gaim_gtk_clear_cursor(gtkblist->window);
	}
}

static void gtk_blist_show_systemlog_cb()
{
	gaim_gtk_syslog_show();
}

static void gtk_blist_show_onlinehelp_cb()
{
	gaim_notify_uri(NULL, GAIM_WEBSITE "documentation.php");
}

static void
do_join_chat(GaimGtkJoinChatData *data)
{
	if (data)
	{
		GHashTable *components =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		GList *tmp;
		GaimChat *chat;

		for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
		{
			if (g_object_get_data(tmp->data, "is_spin"))
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
							gtk_spin_button_get_value_as_int(tmp->data)));
			}
			else
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup(gtk_entry_get_text(tmp->data)));
			}
		}

		chat = gaim_chat_new(data->account, NULL, components);
		gtk_blist_join_chat(chat);
		gaim_blist_remove_chat(chat);
	}
}

static void
do_joinchat(GtkWidget *dialog, int id, GaimGtkJoinChatData *info)
{
	switch(id)
	{
		case GTK_RESPONSE_OK:
			do_join_chat(info);

		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_list_free(info->entries);
	g_free(info);
}

/*
 * Check the values of all the text entry boxes.  If any required input
 * strings are empty then don't allow the user to click on "OK."
 */
static void
joinchat_set_sensitive_if_input_cb(GtkWidget *entry, gpointer user_data)
{
	GaimGtkJoinChatData *data;
	GList *tmp;
	const char *text;
	gboolean required;
	gboolean sensitive = TRUE;

	data = user_data;

	for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
	{
		if (!g_object_get_data(tmp->data, "is_spin"))
		{
			required = GPOINTER_TO_INT(g_object_get_data(tmp->data, "required"));
			text = gtk_entry_get_text(tmp->data);
			if (required && (*text == '\0'))
				sensitive = FALSE;
		}
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->window), GTK_RESPONSE_OK, sensitive);
}

static void
gaim_gtk_blist_update_privacy_cb(GaimBuddy *buddy)
{
	gaim_gtk_blist_update_buddy(gaim_get_blist(), (GaimBlistNode*)(buddy), TRUE);
}

static void
rebuild_joinchat_entries(GaimGtkJoinChatData *data)
{
	GaimConnection *gc;
	GList *list = NULL, *tmp;
	GHashTable *defaults = NULL;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	g_return_if_fail(data->account != NULL);

	gc = gaim_account_get_connection(data->account);

	while ((tmp = gtk_container_get_children(GTK_CONTAINER(data->entries_box))))
		gtk_widget_destroy(tmp->data);

	g_list_free(data->entries);
	data->entries = NULL;

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
		list = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info(gc);

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
		defaults = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, NULL);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *label;
		GtkWidget *rowbox;
		GtkWidget *input;

		pce = tmp->data;

		rowbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(data->entries_box), rowbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(pce->label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(data->sg, label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		if (pce->is_int)
		{
			GtkObject *adjust;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max,
										1, 10, 10);
			input = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			gtk_widget_set_size_request(input, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), input, FALSE, FALSE, 0);
		}
		else
		{
			char *value;
			input = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);
			value = g_hash_table_lookup(defaults, pce->identifier);
			if (value != NULL)
				gtk_entry_set_text(GTK_ENTRY(input), value);
			if (pce->secret)
			{
				gtk_entry_set_visibility(GTK_ENTRY(input), FALSE);
				gtk_entry_set_invisible_char(GTK_ENTRY(input), GAIM_INVISIBLE_CHAR);
			}
			gtk_box_pack_end(GTK_BOX(rowbox), input, TRUE, TRUE, 0);
			g_signal_connect(G_OBJECT(input), "changed",
							 G_CALLBACK(joinchat_set_sensitive_if_input_cb), data);
		}

		/* Do the following for any type of input widget */
		if (focus)
		{
			gtk_widget_grab_focus(input);
			focus = FALSE;
		}
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), input);
		gaim_set_accessible_label(input, label);
		g_object_set_data(G_OBJECT(input), "identifier", (gpointer)pce->identifier);
		g_object_set_data(G_OBJECT(input), "is_spin", GINT_TO_POINTER(pce->is_int));
		g_object_set_data(G_OBJECT(input), "required", GINT_TO_POINTER(pce->required));
		data->entries = g_list_append(data->entries, input);

		g_free(pce);
	}

	g_list_free(list);
	g_hash_table_destroy(defaults);

	/* Set whether the "OK" button should be clickable initially */
	joinchat_set_sensitive_if_input_cb(NULL, data);

	gtk_widget_show_all(data->entries_box);
}

static void
joinchat_select_account_cb(GObject *w, GaimAccount *account,
                           GaimGtkJoinChatData *data)
{
    data->account = account;
    rebuild_joinchat_entries(data);
}

static gboolean
chat_account_filter_func(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimPluginProtocolInfo *prpl_info = NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	return (prpl_info->chat_info != NULL);
}

gboolean
gaim_gtk_blist_joinchat_is_showable()
{
	GList *c;
	GaimConnection *gc;

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (chat_account_filter_func(gaim_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

void
gaim_gtk_blist_joinchat_show(void)
{
	GtkWidget *hbox, *vbox;
	GtkWidget *rowbox;
	GtkWidget *label;
	GaimGtkBuddyList *gtkblist;
	GtkWidget *img = NULL;
	GaimGtkJoinChatData *data = NULL;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	data = g_new0(GaimGtkJoinChatData, 1);

	data->window = gtk_dialog_new_with_buttons(_("Join a Chat"),
		NULL, GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GAIM_STOCK_CHAT, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), GAIM_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width(
		GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BOX_SPACE);
	gtk_window_set_role(GTK_WINDOW(data->window), "join_chat");

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("Please enter the appropriate information "
							"about the chat you would like to join.\n"));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	rowbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, TRUE, TRUE, 0);

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new_with_mnemonic(_("_Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(data->sg, label);

	data->account_menu = gaim_gtk_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(joinchat_select_account_cb),
			chat_account_filter_func, data);
	gtk_box_pack_start(GTK_BOX(rowbox), data->account_menu, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label),
								  GTK_WIDGET(data->account_menu));
	gaim_set_accessible_label (data->account_menu, label);

	data->entries_box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), data->entries_box);
	gtk_container_set_border_width(GTK_CONTAINER(data->entries_box), 0);

	data->account =	gaim_gtk_account_option_menu_get_selected(data->account_menu);

	rebuild_joinchat_entries(data);

	g_signal_connect(G_OBJECT(data->window), "response",
					 G_CALLBACK(do_joinchat), data);

	g_object_unref(data->sg);

	gtk_widget_show_all(data->window);
}

static void gtk_blist_row_expanded_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
	GaimBlistNode *node;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_blist_node_set_bool(node, "collapsed", FALSE);
	}
}

static void gtk_blist_row_collapsed_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data) {
	GaimBlistNode *node;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_blist_node_set_bool(node, "collapsed", TRUE);
	} else if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		gaim_gtk_blist_collapse_contact_cb(NULL, node);
	}
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	GaimBlistNode *node;
	GtkTreeIter iter;
	GValue val;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);

	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy;

		if(GAIM_BLIST_NODE_IS_CONTACT(node))
			buddy = gaim_contact_get_priority_buddy((GaimContact*)node);
		else
			buddy = (GaimBuddy*)node;

		gaim_gtkdialogs_im_with_user(buddy->account, buddy->name);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		gtk_blist_join_chat((GaimChat *)node);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
/*		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);*/
	}
}

static void gaim_gtk_blist_add_chat_cb()
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	GaimBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node))
			gaim_blist_request_add_chat(NULL, (GaimGroup*)node->parent->parent, NULL, NULL);
		if (GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_CHAT(node))
			gaim_blist_request_add_chat(NULL, (GaimGroup*)node->parent, NULL, NULL);
		else if (GAIM_BLIST_NODE_IS_GROUP(node))
			gaim_blist_request_add_chat(NULL, (GaimGroup*)node, NULL, NULL);
	}
	else {
		gaim_blist_request_add_chat(NULL, NULL, NULL, NULL);
	}
}

static void gaim_gtk_blist_add_buddy_cb()
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	GaimBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
			gaim_blist_request_add_buddy(NULL, NULL, ((GaimGroup*)node->parent->parent)->name,
					NULL);
		} else if (GAIM_BLIST_NODE_IS_CONTACT(node)
				|| GAIM_BLIST_NODE_IS_CHAT(node)) {
			gaim_blist_request_add_buddy(NULL, NULL, ((GaimGroup*)node->parent)->name, NULL);
		} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
			gaim_blist_request_add_buddy(NULL, NULL, ((GaimGroup*)node)->name, NULL);
		}
	}
	else {
		gaim_blist_request_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void
gaim_gtk_blist_remove_cb (GtkWidget *w, GaimBlistNode *node)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		gaim_gtkdialogs_remove_buddy((GaimBuddy*)node);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		gaim_gtkdialogs_remove_chat((GaimChat*)node);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		gaim_gtkdialogs_remove_group((GaimGroup*)node);
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		gaim_gtkdialogs_remove_contact((GaimContact*)node);
	}
}

struct _expand {
	GtkTreeView *treeview;
	GtkTreePath *path;
	GaimBlistNode *node;
};

static gboolean
scroll_to_expanded_cell(gpointer data)
{
	struct _expand *ex = data;
	gtk_tree_view_scroll_to_cell(ex->treeview, ex->path, NULL, FALSE, 0, 0);
	gaim_gtk_blist_update_contact(NULL, ex->node);

	gtk_tree_path_free(ex->path);
	g_free(ex);

	return FALSE;
}

static void
gaim_gtk_blist_expand_contact_cb(GtkWidget *w, GaimBlistNode *node)
{
	struct _gaim_gtk_blist_node *gtknode;
	GtkTreeIter iter, parent;
	GaimBlistNode *bnode;
	GtkTreePath *path;

	if(!GAIM_BLIST_NODE_IS_CONTACT(node))
		return;

	gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

	gtknode->contact_expanded = TRUE;

	for(bnode = node->child; bnode; bnode = bnode->next) {
		gaim_gtk_blist_update(NULL, bnode);
	}

	/* This ensures that the bottom buddy is visible, i.e. not scrolled off the alignment */
	if (get_iter_from_node(node, &parent)) {
		struct _expand *ex = g_new0(struct _expand, 1);

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtkblist->treemodel), &iter, &parent,
				  gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &parent) -1);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);

		/* Let the treeview draw so it knows where to scroll */
		ex->treeview = GTK_TREE_VIEW(gtkblist->treeview);
		ex->path = path;
		ex->node = node->child;
		g_idle_add(scroll_to_expanded_cell, ex);
	}
}

static void
gaim_gtk_blist_collapse_contact_cb(GtkWidget *w, GaimBlistNode *node)
{
	GaimBlistNode *bnode;
	struct _gaim_gtk_blist_node *gtknode;

	if(!GAIM_BLIST_NODE_IS_CONTACT(node))
		return;

	gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

	gtknode->contact_expanded = FALSE;

	for(bnode = node->child; bnode; bnode = bnode->next) {
		gaim_gtk_blist_update(NULL, bnode);
	}
}

void
gaim_gtk_append_blist_node_proto_menu(GtkWidget *menu, GaimConnection *gc,
                                      GaimBlistNode *node)
{
	GList *l, *ll;
	GaimPluginProtocolInfo *prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(!prpl_info || !prpl_info->blist_node_menu)
		return;

	for(l = ll = prpl_info->blist_node_menu(node); l; l = l->next) {
		GaimMenuAction *act = (GaimMenuAction *) l->data;
		gaim_gtk_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}

void
gaim_gtk_append_blist_node_extended_menu(GtkWidget *menu, GaimBlistNode *node)
{
	GList *l, *ll;

	for(l = ll = gaim_blist_node_get_extended_menu(node); l; l = l->next) {
		GaimMenuAction *act = (GaimMenuAction *) l->data;
		gaim_gtk_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}

void
gaim_gtk_blist_make_buddy_menu(GtkWidget *menu, GaimBuddy *buddy, gboolean sub) {
	GaimPluginProtocolInfo *prpl_info;
	GaimContact *contact;
	gboolean contact_expanded = FALSE;

	g_return_if_fail(menu);
	g_return_if_fail(buddy);

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(buddy->account->gc->prpl);

	contact = gaim_buddy_get_contact(buddy);
	if (contact) {
		contact_expanded = ((struct _gaim_gtk_blist_node *)(((GaimBlistNode*)contact)->ui_data))->contact_expanded;
	}

	if (prpl_info && prpl_info->get_info) {
		gaim_new_item_from_stock(menu, _("Get _Info"), GAIM_STOCK_INFO,
				G_CALLBACK(gtk_blist_menu_info_cb), buddy, 0, 0, NULL);
	}
	gaim_new_item_from_stock(menu, _("I_M"), GAIM_STOCK_IM,
			G_CALLBACK(gtk_blist_menu_im_cb), buddy, 0, 0, NULL);
	if (prpl_info && prpl_info->send_file) {
		if (!prpl_info->can_receive_file ||
			prpl_info->can_receive_file(buddy->account->gc, buddy->name))
		{
			gaim_new_item_from_stock(menu, _("_Send File"),
									 GAIM_STOCK_FILE_TRANSFER,
									 G_CALLBACK(gtk_blist_menu_send_file_cb),
									 buddy, 0, 0, NULL);
		}
	}

	gaim_new_item_from_stock(menu, _("Add Buddy _Pounce"), GAIM_STOCK_POUNCE,
			G_CALLBACK(gtk_blist_menu_bp_cb), buddy, 0, 0, NULL);

	if(((GaimBlistNode*)buddy)->parent->child->next && !sub && !contact_expanded) {
		gaim_new_item_from_stock(menu, _("View _Log"), GAIM_STOCK_LOG,
				G_CALLBACK(gtk_blist_menu_showlog_cb),
				contact, 0, 0, NULL);
	} else if (!sub) {
		gaim_new_item_from_stock(menu, _("View _Log"), GAIM_STOCK_LOG,
				G_CALLBACK(gtk_blist_menu_showlog_cb), buddy, 0, 0, NULL);
	}

	gaim_gtk_append_blist_node_proto_menu(menu, buddy->account->gc,
										  (GaimBlistNode *)buddy);
	gaim_gtk_append_blist_node_extended_menu(menu, (GaimBlistNode *)buddy);

	if (((GaimBlistNode*)buddy)->parent->child->next && !sub && !contact_expanded) {
		gaim_separator(menu);

		gaim_new_item_from_stock(menu, _("Alias..."), GAIM_STOCK_ALIAS,
				G_CALLBACK(gtk_blist_menu_alias_cb),
				contact, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("Remove"), GTK_STOCK_REMOVE,
				G_CALLBACK(gaim_gtk_blist_remove_cb),
				contact, 0, 0, NULL);
	} else if (!sub || contact_expanded) {
		gaim_separator(menu);

		gaim_new_item_from_stock(menu, _("_Alias..."), GAIM_STOCK_ALIAS,
				G_CALLBACK(gtk_blist_menu_alias_cb), buddy, 0, 0, NULL);
		gaim_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				G_CALLBACK(gaim_gtk_blist_remove_cb), buddy,
				0, 0, NULL);
	}
}

static gboolean
gtk_blist_key_press_cb(GtkWidget *tv, GdkEventKey *event, gpointer data) {
	GaimBlistNode *node;
	GValue val;
	GtkTreeIter iter;
	GtkTreeSelection *sel;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
			NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(event->state & GDK_CONTROL_MASK &&
			(event->keyval == 'o' || event->keyval == 'O')) {
		GaimBuddy *buddy;

		if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
			buddy = gaim_contact_get_priority_buddy((GaimContact*)node);
		} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
			buddy = (GaimBuddy*)node;
		} else {
			return FALSE;
		}
		if(buddy)
			serv_get_info(buddy->account->gc, buddy->name);
	}

	return FALSE;
}


static GtkWidget *
create_group_menu (GaimBlistNode *node, GaimGroup *g)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new();
	gaim_new_item_from_stock(menu, _("Add a _Buddy"), GTK_STOCK_ADD,
				 G_CALLBACK(gaim_gtk_blist_add_buddy_cb), node, 0, 0, NULL);
	item = gaim_new_item_from_stock(menu, _("Add a C_hat"), GTK_STOCK_ADD,
				 G_CALLBACK(gaim_gtk_blist_add_chat_cb), node, 0, 0, NULL);
	gtk_widget_set_sensitive(item, gaim_gtk_blist_joinchat_is_showable());
	gaim_new_item_from_stock(menu, _("_Delete Group"), GTK_STOCK_REMOVE,
				 G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("_Rename"), NULL,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);

	gaim_gtk_append_blist_node_extended_menu(menu, node);

	return menu;
}


static GtkWidget *
create_chat_menu(GaimBlistNode *node, GaimChat *c) {
	GtkWidget *menu;
	gboolean autojoin;

	menu = gtk_menu_new();
	autojoin = (gaim_blist_node_get_bool(node, "gtk-autojoin") ||
			(gaim_blist_node_get_string(node, "gtk-autojoin") != NULL));

	gaim_new_item_from_stock(menu, _("_Join"), GAIM_STOCK_CHAT,
			G_CALLBACK(gtk_blist_menu_join_cb), node, 0, 0, NULL);
	gaim_new_check_item(menu, _("Auto-Join"),
			G_CALLBACK(gtk_blist_menu_autojoin_cb), node, autojoin);
	gaim_new_item_from_stock(menu, _("View _Log"), GAIM_STOCK_LOG,
			G_CALLBACK(gtk_blist_menu_showlog_cb), node, 0, 0, NULL);

	gaim_gtk_append_blist_node_proto_menu(menu, c->account->gc, node);
	gaim_gtk_append_blist_node_extended_menu(menu, node);

	gaim_separator(menu);

	gaim_new_item_from_stock(menu, _("_Alias..."), GAIM_STOCK_ALIAS,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				 G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);

	return menu;
}

static GtkWidget *
create_contact_menu (GaimBlistNode *node)
{
	GtkWidget *menu;

	menu = gtk_menu_new();

	gaim_new_item_from_stock(menu, _("View _Log"), GAIM_STOCK_LOG,
				 G_CALLBACK(gtk_blist_menu_showlog_cb),
				 node, 0, 0, NULL);

	gaim_separator(menu);

	gaim_new_item_from_stock(menu, _("_Alias..."), GAIM_STOCK_ALIAS,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				 G_CALLBACK(gaim_gtk_blist_remove_cb), node, 0, 0, NULL);

	gaim_separator(menu);

	gaim_new_item_from_stock(menu, _("_Collapse"), GTK_STOCK_ZOOM_OUT,
				 G_CALLBACK(gaim_gtk_blist_collapse_contact_cb),
				 node, 0, 0, NULL);

	gaim_gtk_append_blist_node_extended_menu(menu, node);

	return menu;
}

static GtkWidget *
create_buddy_menu(GaimBlistNode *node, GaimBuddy *b) {
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean show_offline = gaim_prefs_get_bool("/gaim/gtk/blist/show_offline_buddies");

	menu = gtk_menu_new();
	gaim_gtk_blist_make_buddy_menu(menu, b, FALSE);

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		gaim_separator(menu);

		if(gtknode->contact_expanded) {
			gaim_new_item_from_stock(menu, _("_Collapse"),
						 GTK_STOCK_ZOOM_OUT,
						 G_CALLBACK(gaim_gtk_blist_collapse_contact_cb),
						 node, 0, 0, NULL);
		} else {
			gaim_new_item_from_stock(menu, _("_Expand"),
						 GTK_STOCK_ZOOM_IN,
						 G_CALLBACK(gaim_gtk_blist_expand_contact_cb), node,
						 0, 0, NULL);
		}
		if(node->child->next) {
			GaimBlistNode *bnode;

			for(bnode = node->child; bnode; bnode = bnode->next) {
				GaimBuddy *buddy = (GaimBuddy*)bnode;
				GdkPixbuf *buf;
				GtkWidget *submenu;
				GtkWidget *image;

				if(buddy == b)
					continue;
				if(!buddy->account->gc)
					continue;
				if(!show_offline && !GAIM_BUDDY_IS_ONLINE(buddy))
					continue;

				menuitem = gtk_image_menu_item_new_with_label(buddy->name);
				buf = gaim_gtk_blist_get_status_icon(bnode,
										GAIM_STATUS_ICON_SMALL);
				image = gtk_image_new_from_pixbuf(buf);
				g_object_unref(G_OBJECT(buf));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
											  image);
				gtk_widget_show(image);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
				gtk_widget_show(menuitem);

				submenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
				gtk_widget_show(submenu);

				gaim_gtk_blist_make_buddy_menu(submenu, buddy, TRUE);
			}
		}
	}
	return menu;
}

static gboolean
gaim_gtk_blist_show_context_menu(GaimBlistNode *node,
								 GtkMenuPositionFunc func,
								 GtkWidget *tv,
								 guint button,
								 guint32 time)
{
	struct _gaim_gtk_blist_node *gtknode;
	GtkWidget *menu = NULL;
	gboolean handled = FALSE;

	gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

	/* Create a menu based on the thing we right-clicked on */
	if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		GaimGroup *g = (GaimGroup *)node;

		menu = create_group_menu(node, g);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		GaimChat *c = (GaimChat *)node;

		menu = create_chat_menu(node, c);
	} else if ((GAIM_BLIST_NODE_IS_CONTACT(node)) && (gtknode->contact_expanded)) {
		menu = create_contact_menu(node);
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *b;

		if (GAIM_BLIST_NODE_IS_CONTACT(node))
			b = gaim_contact_get_priority_buddy((GaimContact*)node);
		else
			b = (GaimBuddy *)node;

		menu = create_buddy_menu(node, b);
	}

#ifdef _WIN32
	/* Unhook the tooltip-timeout since we don't want a tooltip
	 * to appear and obscure the context menu we are about to show
	   This is a workaround for GTK+ bug 107320. */
	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}
#endif

	/* Now display the menu */
	if (menu != NULL) {
		gtk_widget_show_all(menu);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, func, tv, button, time);
		handled = TRUE;
	}

	return handled;
}

static gboolean gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer user_data)
{
	GtkTreePath *path;
	GaimBlistNode *node;
	GValue val;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	GaimPlugin *prpl = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;
	struct _gaim_gtk_blist_node *gtknode;
	gboolean handled = FALSE;

	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;

	/* Right click draws a context menu */
	if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS)) {
		handled = gaim_gtk_blist_show_context_menu(node, NULL, tv, 3, event->time);

	/* CTRL+middle click expands or collapse a contact */
	} else if ((event->button == 2) && (event->type == GDK_BUTTON_PRESS) &&
			   (event->state & GDK_CONTROL_MASK) && (GAIM_BLIST_NODE_IS_CONTACT(node))) {
		if (gtknode->contact_expanded)
			gaim_gtk_blist_collapse_contact_cb(NULL, node);
		else
			gaim_gtk_blist_expand_contact_cb(NULL, node);
		handled = TRUE;

	/* Double middle click gets info */
	} else if ((event->button == 2) && (event->type == GDK_2BUTTON_PRESS) &&
			   ((GAIM_BLIST_NODE_IS_CONTACT(node)) || (GAIM_BLIST_NODE_IS_BUDDY(node)))) {
		GaimBuddy *b;
		if(GAIM_BLIST_NODE_IS_CONTACT(node))
			b = gaim_contact_get_priority_buddy((GaimContact*)node);
		else
			b = (GaimBuddy *)node;

		prpl = gaim_find_prpl(gaim_account_get_protocol_id(b->account));
		if (prpl != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl && prpl_info->get_info)
			serv_get_info(b->account->gc, b->name);
		handled = TRUE;
	}

#if (1)
	/*
	 * This code only exists because GTK+ doesn't work.  If we return
	 * FALSE here, as would be normal the event propoagates down and
	 * somehow gets interpreted as the start of a drag event.
	 *
	 * Um, isn't it _normal_ to return TRUE here?  Since the event
	 * was handled?  --Mark
	 */
	if(handled) {
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
		return TRUE;
	}
#endif
	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean
gaim_gtk_blist_popup_menu_cb(GtkWidget *tv, void *user_data)
{
	GaimBlistNode *node;
	GValue val;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	gboolean handled = FALSE;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel),
							 &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	/* Shift+F10 draws a context menu */
	handled = gaim_gtk_blist_show_context_menu(node, gaim_gtk_treeview_popup_menu_position_func, tv, 0, GDK_CURRENT_TIME);

	return handled;
}

static void gaim_gtk_blist_buddy_details_cb(gpointer data, guint action, GtkWidget *item)
{
	gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

	gaim_prefs_set_bool("/gaim/gtk/blist/show_buddy_icons",
			    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	gaim_gtk_clear_cursor(gtkblist->window);
}

static void gaim_gtk_blist_show_idle_time_cb(gpointer data, guint action, GtkWidget *item)
{
	gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

	gaim_prefs_set_bool("/gaim/gtk/blist/show_idle_time",
			    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	gaim_gtk_clear_cursor(gtkblist->window);
}

static void gaim_gtk_blist_show_empty_groups_cb(gpointer data, guint action, GtkWidget *item)
{
	gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

	gaim_prefs_set_bool("/gaim/gtk/blist/show_empty_groups",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	gaim_gtk_clear_cursor(gtkblist->window);
}

static void gaim_gtk_blist_edit_mode_cb(gpointer callback_data, guint callback_action,
		GtkWidget *checkitem)
{
	gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);

	gaim_prefs_set_bool("/gaim/gtk/blist/show_offline_buddies",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkitem)));

	gaim_gtk_clear_cursor(gtkblist->window);
}

static void gaim_gtk_blist_mute_sounds_cb(gpointer data, guint action, GtkWidget *item)
{
	gaim_prefs_set_bool("/gaim/gtk/sound/mute", GTK_CHECK_MENU_ITEM(item)->active);
}

static void
gaim_gtk_blist_mute_pref_cb(const char *name, GaimPrefType type,
							gconstpointer value, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(gtkblist->ift,
						N_("/Tools/Mute Sounds"))),	(gboolean)GPOINTER_TO_INT(value));
}

static void
gaim_gtk_blist_sound_method_pref_cb(const char *name, GaimPrefType type,
									gconstpointer value, gpointer data)
{
	gboolean sensitive = TRUE;

	if(!strcmp(value, "none"))
		sensitive = FALSE;

	gtk_widget_set_sensitive(gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Mute Sounds")), sensitive);
}

static void
add_buddies_from_vcard(const char *prpl_id, GaimGroup *group, GList *list,
					   const char *alias)
{
	GList *l;
	GaimAccount *account = NULL;
	GaimConnection *gc;

	if (list == NULL)
		return;

	for (l = gaim_connections_get_all(); l != NULL; l = l->next)
	{
		gc = (GaimConnection *)l->data;
		account = gaim_connection_get_account(gc);

		if (!strcmp(gaim_account_get_protocol_id(account), prpl_id))
			break;

		account = NULL;
	}

	if (account != NULL)
	{
		for (l = list; l != NULL; l = l->next)
		{
			gaim_blist_request_add_buddy(account, l->data,
										 (group ? group->name : NULL),
										 alias);
		}
	}

	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static gboolean
parse_vcard(const char *vcard, GaimGroup *group)
{
	char *temp_vcard;
	char *s, *c;
	char *alias    = NULL;
	GList *aims    = NULL;
	GList *icqs    = NULL;
	GList *yahoos  = NULL;
	GList *msns    = NULL;
	GList *jabbers = NULL;

	s = temp_vcard = g_strdup(vcard);

	while (*s != '\0' && strncmp(s, "END:vCard", strlen("END:vCard")))
	{
		char *field, *value;

		field = s;

		/* Grab the field */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ':')
			s++;

		if (*s == '\r') s++;
		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		if ((c = strchr(field, ';')) != NULL)
			*c = '\0';

		/* Proceed to the end of the line */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		/* We only want to worry about a few fields here. */
		if (!strcmp(field, "FN"))
			alias = g_strdup(value);
		else if (!strcmp(field, "X-AIM") || !strcmp(field, "X-ICQ") ||
				 !strcmp(field, "X-YAHOO") || !strcmp(field, "X-MSN") ||
				 !strcmp(field, "X-JABBER"))
		{
			char **values = g_strsplit(value, ":", 0);
			char **im;

			for (im = values; *im != NULL; im++)
			{
				if (!strcmp(field, "X-AIM"))
					aims = g_list_append(aims, g_strdup(*im));
				else if (!strcmp(field, "X-ICQ"))
					icqs = g_list_append(icqs, g_strdup(*im));
				else if (!strcmp(field, "X-YAHOO"))
					yahoos = g_list_append(yahoos, g_strdup(*im));
				else if (!strcmp(field, "X-MSN"))
					msns = g_list_append(msns, g_strdup(*im));
				else if (!strcmp(field, "X-JABBER"))
					jabbers = g_list_append(jabbers, g_strdup(*im));
			}

			g_strfreev(values);
		}
	}

	g_free(temp_vcard);

	if (aims == NULL && icqs == NULL && yahoos == NULL &&
		msns == NULL && jabbers == NULL)
	{
		g_free(alias);

		return FALSE;
	}

	add_buddies_from_vcard("prpl-oscar",  group, aims,    alias);
	add_buddies_from_vcard("prpl-oscar",  group, icqs,    alias);
	add_buddies_from_vcard("prpl-yahoo",  group, yahoos,  alias);
	add_buddies_from_vcard("prpl-msn",    group, msns,    alias);
	add_buddies_from_vcard("prpl-jabber", group, jabbers, alias);

	g_free(alias);

	return TRUE;
}

#ifdef _WIN32
static void gaim_gtk_blist_drag_begin(GtkWidget *widget,
		GdkDragContext *drag_context, gpointer user_data)
{
	gaim_gtk_blist_tooltip_destroy();


	/* Unhook the tooltip-timeout since we don't want a tooltip
	 * to appear and obscure the dragging operation.
	 * This is a workaround for GTK+ bug 107320. */
	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}
}
#endif

static void gaim_gtk_blist_drag_data_get_cb(GtkWidget *widget,
											GdkDragContext *dc,
											GtkSelectionData *data,
											guint info,
											guint time,
											gpointer null)
{

	if (data->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE))
	{
		GtkTreeRowReference *ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		GtkTreePath *sourcerow = gtk_tree_row_reference_get_path(ref);
		GtkTreeIter iter;
		GaimBlistNode *node = NULL;
		GValue val;
		if(!sourcerow)
			return;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, sourcerow);
		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
		node = g_value_get_pointer(&val);
		gtk_selection_data_set (data,
					gdk_atom_intern ("GAIM_BLIST_NODE", FALSE),
					8, /* bits */
					(void*)&node,
					sizeof (node));

		gtk_tree_path_free(sourcerow);
	}
	else if (data->target == gdk_atom_intern("application/x-im-contact", FALSE))
	{
		GtkTreeRowReference *ref;
		GtkTreePath *sourcerow;
		GtkTreeIter iter;
		GaimBlistNode *node = NULL;
		GaimBuddy *buddy;
		GaimConnection *gc;
		GValue val;
		GString *str;
		const char *protocol;

		ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		sourcerow = gtk_tree_row_reference_get_path(ref);

		if (!sourcerow)
			return;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
								sourcerow);
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
								 NODE_COLUMN, &val);

		node = g_value_get_pointer(&val);

		if (GAIM_BLIST_NODE_IS_CONTACT(node))
		{
			buddy = gaim_contact_get_priority_buddy((GaimContact *)node);
		}
		else if (!GAIM_BLIST_NODE_IS_BUDDY(node))
		{
			gtk_tree_path_free(sourcerow);
			return;
		}
		else
		{
			buddy = (GaimBuddy *)node;
		}

		gc = gaim_account_get_connection(buddy->account);

		if (gc == NULL)
		{
			gtk_tree_path_free(sourcerow);
			return;
		}

		protocol =
			GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->list_icon(buddy->account,
														   buddy);

		str = g_string_new(NULL);
		g_string_printf(str,
			"MIME-Version: 1.0\r\n"
			"Content-Type: application/x-im-contact\r\n"
			"X-IM-Protocol: %s\r\n"
			"X-IM-Username: %s\r\n",
			protocol,
			buddy->name);

		if (buddy->alias != NULL)
		{
			g_string_append_printf(str,
				"X-IM-Alias: %s\r\n",
				buddy->alias);
		}

		g_string_append(str, "\r\n");

		gtk_selection_data_set(data,
					gdk_atom_intern("application/x-im-contact", FALSE),
					8, /* bits */
					(const guchar *)str->str,
					strlen(str->str) + 1);

		g_string_free(str, TRUE);
		gtk_tree_path_free(sourcerow);
	}
}

static void gaim_gtk_blist_drag_data_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
			  GtkSelectionData *sd, guint info, guint t)
{
	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	if (sd->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE) && sd->data) {
		GaimBlistNode *n = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		memcpy(&n, sd->data, sizeof(n));
		if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
			/* if we're here, I think it means the drop is ok */
			GtkTreeIter iter;
			GaimBlistNode *node;
			GValue val;
			struct _gaim_gtk_blist_node *gtknode;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);
			gtknode = node->ui_data;

			if (GAIM_BLIST_NODE_IS_CONTACT(n)) {
				GaimContact *c = (GaimContact*)n;
				if (GAIM_BLIST_NODE_IS_CONTACT(node) && gtknode->contact_expanded) {
					gaim_blist_merge_contact(c, node);
				} else if (GAIM_BLIST_NODE_IS_CONTACT(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							gaim_blist_add_contact(c, (GaimGroup*)node->parent,
									node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_contact(c, (GaimGroup*)node->parent,
									node->prev);
							break;
					}
				} else if(GAIM_BLIST_NODE_IS_GROUP(node)) {
					gaim_blist_add_contact(c, (GaimGroup*)node, NULL);
				} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
					gaim_blist_merge_contact(c, node);
				}
			} else if (GAIM_BLIST_NODE_IS_BUDDY(n)) {
				GaimBuddy *b = (GaimBuddy*)n;
				if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							gaim_blist_add_buddy(b, (GaimContact*)node->parent,
									(GaimGroup*)node->parent->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_buddy(b, (GaimContact*)node->parent,
									(GaimGroup*)node->parent->parent,
									node->prev);
							break;
					}
				} else if(GAIM_BLIST_NODE_IS_CHAT(node)) {
					gaim_blist_add_buddy(b, NULL, (GaimGroup*)node->parent,
							NULL);
				} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
					gaim_blist_add_buddy(b, NULL, (GaimGroup*)node, NULL);
				} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
					if(gtknode->contact_expanded) {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
								gaim_blist_add_buddy(b, (GaimContact*)node,
										(GaimGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_BEFORE:
								gaim_blist_add_buddy(b, NULL,
										(GaimGroup*)node->parent, node->prev);
								break;
						}
					} else {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
								gaim_blist_add_buddy(b, NULL,
										(GaimGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							case GTK_TREE_VIEW_DROP_BEFORE:
								gaim_blist_add_buddy(b, NULL,
										(GaimGroup*)node->parent, node->prev);
								break;
						}
					}
				}
			} else if (GAIM_BLIST_NODE_IS_CHAT(n)) {
				GaimChat *chat = (GaimChat *)n;
				if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_chat(chat,
									(GaimGroup*)node->parent->parent,
									node->parent);
							break;
					}
				} else if(GAIM_BLIST_NODE_IS_CONTACT(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							gaim_blist_add_chat(chat, (GaimGroup*)node->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							gaim_blist_add_chat(chat, (GaimGroup*)node->parent, node->prev);
							break;
					}
				} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
					gaim_blist_add_chat(chat, (GaimGroup*)node, NULL);
				}
			} else if (GAIM_BLIST_NODE_IS_GROUP(n)) {
				GaimGroup *g = (GaimGroup*)n;
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
				} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
					gaim_blist_add_group(g, node->parent->parent);
				} else if(GAIM_BLIST_NODE_IS_CONTACT(node) ||
						GAIM_BLIST_NODE_IS_CHAT(node)) {
					gaim_blist_add_group(g, node->parent);
				}
			}

			gtk_tree_path_free(path);
			gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
		}
	}
	else if (sd->target == gdk_atom_intern("application/x-im-contact",
										   FALSE) && sd->data)
	{
		GaimGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		GaimAccount *account;
		char *protocol = NULL;
		char *username = NULL;
		char *alias = NULL;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			GaimBlistNode *node;
			GValue val;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
									  &iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);

			if (GAIM_BLIST_NODE_IS_BUDDY(node))
			{
				group = (GaimGroup *)node->parent->parent;
			}
			else if (GAIM_BLIST_NODE_IS_CHAT(node) ||
					 GAIM_BLIST_NODE_IS_CONTACT(node))
			{
				group = (GaimGroup *)node->parent;
			}
			else if (GAIM_BLIST_NODE_IS_GROUP(node))
			{
				group = (GaimGroup *)node;
			}
		}

		if (gaim_gtk_parse_x_im_contact((const char *)sd->data, FALSE, &account,
										&protocol, &username, &alias))
		{
			if (account == NULL)
			{
				gaim_notify_error(NULL, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL);
			}
			else
			{
				gaim_blist_request_add_buddy(account, username,
											 (group ? group->name : NULL),
											 alias);
			}
		}

		g_free(username);
		g_free(protocol);
		g_free(alias);

		if (path != NULL)
			gtk_tree_path_free(path);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else if (sd->target == gdk_atom_intern("text/x-vcard", FALSE) && sd->data)
	{
		gboolean result;
		GaimGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			GaimBlistNode *node;
			GValue val;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
									  &iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);

			if (GAIM_BLIST_NODE_IS_BUDDY(node))
			{
				group = (GaimGroup *)node->parent->parent;
			}
			else if (GAIM_BLIST_NODE_IS_CHAT(node) ||
					 GAIM_BLIST_NODE_IS_CONTACT(node))
			{
				group = (GaimGroup *)node->parent;
			}
			else if (GAIM_BLIST_NODE_IS_GROUP(node))
			{
				group = (GaimGroup *)node;
			}
		}

		result = parse_vcard((const gchar *)sd->data, group);

		gtk_drag_finish(dc, result, (dc->action == GDK_ACTION_MOVE), t);
	} else if (sd->target == gdk_atom_intern("text/uri-list", FALSE) && sd->data) {
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
			{
				GtkTreeIter iter;
				GaimBlistNode *node;
				GValue val;

				gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
							&iter, path);
				val.g_type = 0;
				gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
							  &iter, NODE_COLUMN, &val);
				node = g_value_get_pointer(&val);

				if (GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CONTACT(node)) {
					GaimBuddy *b = GAIM_BLIST_NODE_IS_BUDDY(node) ? (GaimBuddy*)node : gaim_contact_get_priority_buddy((GaimContact*)node);
					gaim_dnd_file_manage(sd, b->account, b->name);
					gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
				} else {
					gtk_drag_finish(dc, FALSE, FALSE, t);
				}
			}
	}
}

static GdkPixbuf *gaim_gtk_blist_get_buddy_icon(GaimBlistNode *node,
		gboolean scaled, gboolean greyed, gboolean custom)
{
	GdkPixbuf *buf, *ret = NULL;
	GdkPixbufLoader *loader;
	GaimBuddyIcon *icon;
	const guchar *data = NULL;
	gsize len;
	GaimBuddy *buddy = (GaimBuddy *)node;

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		buddy = gaim_contact_get_priority_buddy((GaimContact*)node);
	} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		buddy = (GaimBuddy*)node;
	} else {
		return NULL;
	}

#if 0
	if (!gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
		return NULL;
#endif

	if (custom) {
		const char *file = gaim_blist_node_get_string((GaimBlistNode*)gaim_buddy_get_contact(buddy),
							"custom_buddy_icon");
		if (file && *file) {
			char *contents;
			GError *err  = NULL;
			if (!g_file_get_contents(file, &contents, &len, &err)) {
				gaim_debug_info("custom -icon", "Could not open custom-icon %s for %s\n",
							file, gaim_buddy_get_name(buddy), err->message);
				g_error_free(err);
			} else
				data = (const guchar*)contents;
		}
	}

	if (data == NULL) {
		if (!(icon = gaim_buddy_get_icon(buddy)))
			if (!(icon = gaim_buddy_icons_find(buddy->account, buddy->name))) /* Not sure I like this...*/
				return NULL;
		data = gaim_buddy_icon_get_data(icon, &len);
		custom = FALSE;  /* We are not using the custom icon */
	}

	loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(loader, data, len, NULL);
	gdk_pixbuf_loader_close(loader, NULL);
	buf = gdk_pixbuf_loader_get_pixbuf(loader);
	if (buf)
		g_object_ref(G_OBJECT(buf));
	g_object_unref(G_OBJECT(loader));

	if (custom)
		g_free((void*)data);
	if (buf) {
		GaimAccount *account = gaim_buddy_get_account(buddy);
		GaimPluginProtocolInfo *prpl_info = NULL;
		int orig_width, orig_height;
		int scale_width, scale_height;

		if(account && account->gc)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

		if (greyed) {
			GaimPresence *presence = gaim_buddy_get_presence(buddy);
			if (!GAIM_BUDDY_IS_ONLINE(buddy))
				gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);
			if (gaim_presence_is_idle(presence))
				gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.25, FALSE);
		}

		/* i'd use the gaim_gtk_buddy_icon_get_scale_size() thing,
		 * but it won't tell me the original size, which I need for scaling
		 * purposes */
		scale_width = orig_width = gdk_pixbuf_get_width(buf);
		scale_height = orig_height = gdk_pixbuf_get_height(buf);

		gaim_buddy_icon_get_scale_size(prpl_info ? &prpl_info->icon_spec : NULL, &scale_width, &scale_height);

		if (scaled) {
			if(scale_height > scale_width) {
				scale_width = 30.0 * (double)scale_width / (double)scale_height;
				scale_height = 30;
			} else {
				scale_height = 30.0 * (double)scale_height / (double)scale_width;
				scale_width = 30;
			}

			ret = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 30, 30);
			gdk_pixbuf_fill(ret, 0x00000000);
			gdk_pixbuf_scale(buf, ret, (30-scale_width)/2, (30-scale_height)/2, scale_width, scale_height, (30-scale_width)/2, (30-scale_height)/2, (double)scale_width/(double)orig_width, (double)scale_height/(double)orig_height, GDK_INTERP_BILINEAR);
		} else {
			ret = gdk_pixbuf_scale_simple(buf,scale_width,scale_height, GDK_INTERP_BILINEAR);
		}
		g_object_unref(G_OBJECT(buf));
	}

	return ret;
}

struct tooltip_data {
	PangoLayout *layout;
	GdkPixbuf *status_icon;
	GdkPixbuf *avatar;
	int avatar_width;
	int width;
	int height;
};

static struct tooltip_data * create_tip_for_node(GaimBlistNode *node, gboolean full)
{
	char *tooltip_text = NULL;
	struct tooltip_data *td = g_new0(struct tooltip_data, 1);

	td->status_icon = gaim_gtk_blist_get_status_icon(node, GAIM_STATUS_ICON_LARGE);
	td->avatar = gaim_gtk_blist_get_buddy_icon(node, !full, FALSE, FALSE);
	tooltip_text = gaim_get_tooltip_text(node, full);
	td->layout = gtk_widget_create_pango_layout(gtkblist->tipwindow, NULL);
	pango_layout_set_markup(td->layout, tooltip_text, -1);
	pango_layout_set_wrap(td->layout, PANGO_WRAP_WORD);
	pango_layout_set_width(td->layout, 300000);

	pango_layout_get_size (td->layout, &td->width, &td->height);
	td->width = PANGO_PIXELS(td->width) + 38 + 8;
	td->height = MAX(PANGO_PIXELS(td->height + 4) + 8, 38);

	if(td->avatar) {
		td->avatar_width = gdk_pixbuf_get_width(td->avatar);
		td->width += td->avatar_width + 8;
		td->height = MAX(td->height, gdk_pixbuf_get_height(td->avatar) + 8);
	}

	g_free(tooltip_text);
	return td;
}

static void gaim_gtk_blist_paint_tip(GtkWidget *widget, GdkEventExpose *event, GaimBlistNode *node)
{
	GtkStyle *style;
	int current_height, max_width;
	GList *l;

	if(gtkblist->tooltipdata == NULL)
		return;

	style = gtkblist->tipwindow->style;
	gtk_paint_flat_box(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			NULL, gtkblist->tipwindow, "tooltip", 0, 0, -1, -1);

	max_width = 0;
	for(l = gtkblist->tooltipdata; l; l = l->next)
	{
		struct tooltip_data *td = l->data;
		max_width = MAX(max_width, td->width);
	}

	current_height = 4;
	for(l = gtkblist->tooltipdata; l; l = l->next)
	{
		struct tooltip_data *td = l->data;

#if GTK_CHECK_VERSION(2,2,0)
		gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, td->status_icon,
			0, 0, 4, current_height, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
		if(td->avatar)
			gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL,
					td->avatar, 0, 0, max_width - (td->avatar_width + 4), current_height, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
#else
		gdk_pixbuf_render_to_drawable(td->status_icon, GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, 0, 0, 4, current_height, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
		if(td->avatar)
			gdk_pixbuf_render_to_drawable(td->avatar,
					GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, 0, 0,
					max_width - (td->avatar_width + 4),
					current_height, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
#endif

		gtk_paint_layout (style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, FALSE,
				NULL, gtkblist->tipwindow, "tooltip", 38 + 4, current_height, td->layout);

		current_height += td->height;

		if(l->next)
			gtk_paint_hline(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, NULL, NULL, NULL, 4, max_width - 4, current_height-6);

	}
}

static void gaim_gtk_blist_tooltip_destroy()
{
	while(gtkblist->tooltipdata) {
		struct tooltip_data *td = gtkblist->tooltipdata->data;

		if(td->avatar)
			g_object_unref(td->avatar);
		if(td->status_icon)
			g_object_unref(td->status_icon);
		g_object_unref(td->layout);
		g_free(td);
		gtkblist->tooltipdata = g_list_delete_link(gtkblist->tooltipdata, gtkblist->tooltipdata);
	}

	if (gtkblist->tipwindow == NULL)
		return;

	gtk_widget_destroy(gtkblist->tipwindow);
	gtkblist->tipwindow = NULL;
}

static gboolean gaim_gtk_blist_expand_timeout(GtkWidget *tv)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GaimBlistNode *node;
	GValue val;
	struct _gaim_gtk_blist_node *gtknode;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), gtkblist->tip_rect.x, gtkblist->tip_rect.y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(!GAIM_BLIST_NODE_IS_CONTACT(node)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtknode = node->ui_data;

	if (!gtknode->contact_expanded) {
		GtkTreeIter i;

		gaim_gtk_blist_expand_contact_cb(NULL, node);

		gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &gtkblist->contact_rect);
		gdk_drawable_get_size(GDK_DRAWABLE(tv->window), &(gtkblist->contact_rect.width), NULL);
		gtkblist->mouseover_contact = node;
		gtk_tree_path_down (path);
		while (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &i, path)) {
			GdkRectangle rect;
			gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &rect);
			gtkblist->contact_rect.height += rect.height;
			gtk_tree_path_next(path);
		}
	}
	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean buddy_is_displayable(GaimBuddy *buddy)
{
	struct _gaim_gtk_blist_node *gtknode;

	if(!buddy)
		return FALSE;

	gtknode = ((GaimBlistNode*)buddy)->ui_data;

	return (gaim_account_is_connected(buddy->account) &&
			(gaim_presence_is_online(buddy->presence) ||
			 (gtknode && gtknode->recent_signonoff) ||
			 gaim_prefs_get_bool("/gaim/gtk/blist/show_offline_buddies") ||
			 gaim_blist_node_get_bool((GaimBlistNode*)buddy, "show_offline")));
}

static gboolean gaim_gtk_blist_tooltip_timeout(GtkWidget *tv)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GaimBlistNode *node;
	GValue val;
	int scr_w, scr_h, w, h, x, y;
#if GTK_CHECK_VERSION(2,2,0)
	int mon_num;
	GdkScreen *screen = NULL;
#endif
	gboolean tooltip_top = FALSE;
	struct _gaim_gtk_blist_node *gtknode;
	GdkRectangle mon_size;

	/*
	 * Attempt to free the previous tooltip.  I have a feeling
	 * this is never needed... but just in case.
	 */
	gaim_gtk_blist_tooltip_destroy();

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), gtkblist->tip_rect.x, gtkblist->tip_rect.y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	gtk_tree_path_free(path);

	gtkblist->tipwindow = gtk_window_new(GTK_WINDOW_POPUP);

	if(GAIM_BLIST_NODE_IS_CHAT(node) || GAIM_BLIST_NODE_IS_BUDDY(node)) {
		struct tooltip_data *td = create_tip_for_node(node, TRUE);
		gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);
		w = td->width;
		h = td->height;
	} else if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimBlistNode *child;
		GaimBuddy *b = gaim_contact_get_priority_buddy((GaimContact *)node);
		w = h = 0;
		for(child = node->child; child; child = child->next)
		{
			if(GAIM_BLIST_NODE_IS_BUDDY(child) && buddy_is_displayable((GaimBuddy*)child)) {
				struct tooltip_data *td = create_tip_for_node(child, (b == (GaimBuddy*)child));
				if (b == (GaimBuddy *)child) {
					gtkblist->tooltipdata = g_list_prepend(gtkblist->tooltipdata, td);
				} else {
					gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);
				}
				w = MAX(w, td->width);
				h += td->height;
			}
		}
	} else {
		gtk_widget_destroy(gtkblist->tipwindow);
		gtkblist->tipwindow = NULL;
		return FALSE;
	}

	if (gtkblist->tooltipdata == NULL) {
		gtk_widget_destroy(gtkblist->tipwindow);
		gtkblist->tipwindow = NULL;
		return FALSE;
	}

	gtknode = node->ui_data;

	gtk_widget_set_app_paintable(gtkblist->tipwindow, TRUE);
	gtk_window_set_resizable(GTK_WINDOW(gtkblist->tipwindow), FALSE);
	gtk_widget_set_name(gtkblist->tipwindow, "gtk-tooltips");
	g_signal_connect(G_OBJECT(gtkblist->tipwindow), "expose_event",
			G_CALLBACK(gaim_gtk_blist_paint_tip), NULL);
	gtk_widget_ensure_style (gtkblist->tipwindow);


#if GTK_CHECK_VERSION(2,2,0)
	gdk_display_get_pointer(gdk_display_get_default(), &screen, &x, &y, NULL);
	mon_num = gdk_screen_get_monitor_at_point(screen, x, y);
	gdk_screen_get_monitor_geometry(screen, mon_num, &mon_size);

	scr_w = mon_size.width + mon_size.x;
	scr_h = mon_size.height + mon_size.y;
#else
	scr_w = gdk_screen_width();
	scr_h = gdk_screen_height();
	gdk_window_get_pointer(NULL, &x, &y, NULL);
	mon_size.x = 0;
	mon_size.y = 0;
#endif

#if GTK_CHECK_VERSION(2,2,0)
	if (w > mon_size.width)
	  w = mon_size.width - 10;

	if (h > mon_size.height)
	  h = mon_size.height - 10;
#endif

	if (GTK_WIDGET_NO_WINDOW(gtkblist->window))
		y+=gtkblist->window->allocation.y;

	x -= ((w >> 1) + 4);

	if ((y + h + 4) > scr_h || tooltip_top)
		y = y - h - 5;
	else
		y = y + 6;

	if (y < mon_size.y)
		y = mon_size.y;

	if (y != mon_size.y) {
		if ((x + w) > scr_w)
			x -= (x + w + 5) - scr_w;
		else if (x < mon_size.x)
			x = mon_size.x;
	} else {
		x -= (w / 2 + 10);
		if (x < mon_size.x)
			x = mon_size.x;
	}

	gtk_widget_set_size_request(gtkblist->tipwindow, w, h);
	gtk_window_move(GTK_WINDOW(gtkblist->tipwindow), x, y);
	gtk_widget_show(gtkblist->tipwindow);

	return FALSE;
}

static gboolean gaim_gtk_blist_drag_motion_cb(GtkWidget *tv, GdkDragContext *drag_context,
					      gint x, gint y, guint time, gpointer user_data)
{
	GtkTreePath *path;
	int delay;

	/*
	 * When dragging a buddy into a contact, this is the delay before
	 * the contact auto-expands.
	 */
	delay = 900;

	if (gtkblist->drag_timeout) {
		if ((y > gtkblist->tip_rect.y) && ((y - gtkblist->tip_rect.height) < gtkblist->tip_rect.y))
			return FALSE;
		/* We've left the cell.  Remove the timeout and create a new one below */
		g_source_remove(gtkblist->drag_timeout);
	}

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), x, y, &path, NULL, NULL, NULL);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &gtkblist->tip_rect);

	if (path)
		gtk_tree_path_free(path);
	gtkblist->drag_timeout = g_timeout_add(delay, (GSourceFunc)gaim_gtk_blist_expand_timeout, tv);

	if (gtkblist->mouseover_contact) {
		if ((y < gtkblist->contact_rect.y) || ((y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			gaim_gtk_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

static gboolean gaim_gtk_blist_motion_cb (GtkWidget *tv, GdkEventMotion *event, gpointer null)
{
	GtkTreePath *path;
	int delay;

	delay = gaim_prefs_get_int("/gaim/gtk/blist/tooltip_delay");

	if (delay == 0)
		return FALSE;

	if (gtkblist->timeout) {
		if ((event->y > gtkblist->tip_rect.y) && ((event->y - gtkblist->tip_rect.height) < gtkblist->tip_rect.y))
			return FALSE;
		/* We've left the cell.  Remove the timeout and create a new one below */
		gaim_gtk_blist_tooltip_destroy();
		g_source_remove(gtkblist->timeout);
	}

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &gtkblist->tip_rect);

	if (path)
		gtk_tree_path_free(path);
	gtkblist->timeout = g_timeout_add(delay, (GSourceFunc)gaim_gtk_blist_tooltip_timeout, tv);

	if (gtkblist->mouseover_contact) {
		if ((event->y < gtkblist->contact_rect.y) || ((event->y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			gaim_gtk_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

static void gaim_gtk_blist_leave_cb (GtkWidget *w, GdkEventCrossing *e, gpointer n)
{

	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}

	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	gaim_gtk_blist_tooltip_destroy();

	if (gtkblist->mouseover_contact &&
		!((e->x > gtkblist->contact_rect.x) && (e->x < (gtkblist->contact_rect.x + gtkblist->contact_rect.width)) &&
		 (e->y > gtkblist->contact_rect.y) && (e->y < (gtkblist->contact_rect.y + gtkblist->contact_rect.height)))) {
			gaim_gtk_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
		gtkblist->mouseover_contact = NULL;
	}
}

static void
toggle_debug(void)
{
	gaim_prefs_set_bool("/gaim/gtk/debug/enabled",
			!gaim_prefs_get_bool("/gaim/gtk/debug/enabled"));
}


/***************************************************
 *            Crap                                 *
 ***************************************************/
static GtkItemFactoryEntry blist_menu[] =
{
	/* Buddies menu */
	{ N_("/_Buddies"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Buddies/New Instant _Message..."), "<CTL>M", gaim_gtkdialogs_im, 0, "<StockItem>", GAIM_STOCK_IM },
	{ N_("/Buddies/Join a _Chat..."), "<CTL>C", gaim_gtk_blist_joinchat_show, 0, "<StockItem>", GAIM_STOCK_CHAT },
	{ N_("/Buddies/Get User _Info..."), "<CTL>I", gaim_gtkdialogs_info, 0, "<StockItem>", GAIM_STOCK_INFO },
	{ N_("/Buddies/View User _Log..."), "<CTL>L", gaim_gtkdialogs_log, 0, "<StockItem>", GAIM_STOCK_LOG },
	{ "/Buddies/sep1", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/Show _Offline Buddies"), NULL, gaim_gtk_blist_edit_mode_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show _Empty Groups"), NULL, gaim_gtk_blist_show_empty_groups_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show Buddy _Details"), NULL, gaim_gtk_blist_buddy_details_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show Idle _Times"), NULL, gaim_gtk_blist_show_idle_time_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/_Sort Buddies"), NULL, NULL, 0, "<Branch>", NULL },
	{ "/Buddies/sep2", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/_Add Buddy..."), "<CTL>B", gaim_gtk_blist_add_buddy_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add C_hat..."), NULL, gaim_gtk_blist_add_chat_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add _Group..."), NULL, gaim_blist_request_add_group, 0, "<StockItem>", GTK_STOCK_ADD },
	{ "/Buddies/sep3", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/_Quit"), "<CTL>Q", gaim_core_quit, 0, "<StockItem>", GTK_STOCK_QUIT },

	/* Accounts menu */
	{ N_("/_Accounts"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Accounts/Add\\/Edit"), "<CTL>A", gaim_gtk_accounts_window_show, 0, "<StockItem>", GAIM_STOCK_ACCOUNTS },

	/* Tools */
	{ N_("/_Tools"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Tools/Buddy _Pounces"), NULL, gaim_gtk_pounces_manager_show, 0, "<StockItem>", GAIM_STOCK_POUNCE },
	{ N_("/Tools/Plu_gins"), "<CTL>U", gaim_gtk_plugin_dialog_show, 0, "<StockItem>", GAIM_STOCK_PLUGIN },
	{ N_("/Tools/Pr_eferences"), "<CTL>P", gaim_gtk_prefs_show, 0, "<StockItem>", GTK_STOCK_PREFERENCES },
	{ N_("/Tools/Pr_ivacy"), NULL, gaim_gtk_privacy_dialog_show, 0, "<StockItem>", GTK_STOCK_DIALOG_ERROR },
	{ "/Tools/sep2", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Tools/_File Transfers"), "<CTL>T", gaim_gtkxfer_dialog_show, 0, "<StockItem>", GAIM_STOCK_FILE_TRANSFER },
	{ N_("/Tools/R_oom List"), NULL, gaim_gtk_roomlist_dialog_show, 0, "<StockItem>", GTK_STOCK_INDEX },
	{ N_("/Tools/System _Log"), NULL, gtk_blist_show_systemlog_cb, 0, "<StockItem>", GAIM_STOCK_LOG },
	{ "/Tools/sep3", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Tools/Mute _Sounds"), "<CTL>S", gaim_gtk_blist_mute_sounds_cb, 0, "<CheckItem>", NULL },

	/* Help */
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Help/Online _Help"), "F1", gtk_blist_show_onlinehelp_cb, 0, "<StockItem>", GTK_STOCK_HELP },
	{ N_("/Help/_Debug Window"), NULL, toggle_debug, 0, "<StockItem>", GAIM_STOCK_DEBUG },
	{ N_("/Help/_About"), NULL, gaim_gtkdialogs_about, 0,  "<StockItem>", GAIM_STOCK_ABOUT },
};

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static char *gaim_get_tooltip_text(GaimBlistNode *node, gboolean full)
{
	GString *str = g_string_new("");
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	char *tmp;

	if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		GaimChat *chat;
		GList *cur;
		struct proto_chat_entry *pce;
		char *name, *value;

		chat = (GaimChat *)node;
		prpl = gaim_find_prpl(gaim_account_get_protocol_id(chat->account));
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		tmp = g_markup_escape_text(gaim_chat_get_name(chat), -1);
		g_string_append_printf(str, "<span size='larger' weight='bold'>%s</span>", tmp);
		g_free(tmp);

		if (g_list_length(gaim_connections_get_all()) > 1)
		{
			tmp = g_markup_escape_text(chat->account->username, -1);
			g_string_append_printf(str, _("\n<b>Account:</b> %s"), tmp);
			g_free(tmp);
		}

		if (prpl_info->chat_info != NULL)
			cur = prpl_info->chat_info(chat->account->gc);
		else
			cur = NULL;

		while (cur != NULL)
		{
			pce = cur->data;

			if (!pce->secret && (!pce->required &&
				g_hash_table_lookup(chat->components, pce->identifier) == NULL))
			{
				tmp = gaim_text_strip_mnemonic(pce->label);
				name = g_markup_escape_text(tmp, -1);
				g_free(tmp);
				value = g_markup_escape_text(g_hash_table_lookup(
										chat->components, pce->identifier), -1);
				g_string_append_printf(str, "\n<b>%s</b> %s",
							name ? name : "",
							value ? value : "");
				g_free(name);
				g_free(value);
			}

			g_free(pce);
			cur = g_list_remove(cur, pce);
		}
	}
	else if (GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		/* NOTE: THIS FUNCTION IS NO LONGER CALLED FOR CONTACTS
		 * See create_tip_for_node(). */

		GaimContact *c;
		GaimBuddy *b;
		GaimPresence *presence;
		char *tmp;
		time_t idle_secs, signon;

		if (GAIM_BLIST_NODE_IS_CONTACT(node))
		{
			c = (GaimContact *)node;
			b = gaim_contact_get_priority_buddy(c);
		}
		else
		{
			b = (GaimBuddy *)node;
			c = gaim_buddy_get_contact(b);
		}

		prpl = gaim_find_prpl(gaim_account_get_protocol_id(b->account));
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		presence = gaim_buddy_get_presence(b);

		/* Buddy Name */
		tmp = g_markup_escape_text(gaim_buddy_get_name(b), -1);
		g_string_append_printf(str, "<span size='larger' weight='bold'>%s</span>", tmp);
		g_free(tmp);

		/* Account */
		if (full && g_list_length(gaim_connections_get_all()) > 1)
		{
			tmp = g_markup_escape_text(gaim_account_get_username(
									   gaim_buddy_get_account(b)), -1);
			g_string_append_printf(str, _("\n<b>Account:</b> %s"), tmp);
			g_free(tmp);
		}

		/* Alias */
		/* If there's not a contact alias, the node is being displayed with
		 * this alias, so there's no point in showing it in the tooltip. */
		if (full && b->alias != NULL && b->alias[0] != '\0' &&
		    (c->alias != NULL && c->alias[0] != '\0') &&
		    strcmp(c->alias, b->alias) != 0)
		{
			tmp = g_markup_escape_text(b->alias, -1);
			g_string_append_printf(str, _("\n<b>Buddy Alias:</b> %s"), tmp);
			g_free(tmp);
		}

		/* Nickname/Server Alias */
		/* I'd like to only show this if there's a contact or buddy
		 * alias, but many people on MSN set long nicknames, which
		 * get ellipsized, so the only way to see the whole thing is
		 * to look at the tooltip. */
		if (full && b->server_alias != NULL && b->server_alias[0] != '\0')
		{
			tmp = g_markup_escape_text(b->server_alias, -1);
			g_string_append_printf(str, _("\n<b>Nickname:</b> %s"), tmp);
			g_free(tmp);
		}

		/* Logged In */
		signon = gaim_presence_get_login_time(presence);
		if (full && GAIM_BUDDY_IS_ONLINE(b) && signon > 0)
		{
			tmp = gaim_str_seconds_to_string(time(NULL) - signon);
			g_string_append_printf(str, _("\n<b>Logged In:</b> %s"), tmp);
			g_free(tmp);
		}

		/* Idle */
		if (gaim_presence_is_idle(presence))
		{
			idle_secs = gaim_presence_get_idle_time(presence);
			if (idle_secs > 0)
			{
				tmp = gaim_str_seconds_to_string(time(NULL) - idle_secs);
				g_string_append_printf(str, _("\n<b>Idle:</b> %s"), tmp);
				g_free(tmp);
			}
		}

		/* Last Seen */
		if (full && !GAIM_BUDDY_IS_ONLINE(b))
		{
			struct _gaim_gtk_blist_node *gtknode = ((GaimBlistNode *)c)->ui_data;
			GaimBlistNode *bnode;
			int lastseen = 0;

			if (!gtknode->contact_expanded || GAIM_BLIST_NODE_IS_CONTACT(node))
			{
				/* We're either looking at a buddy for a collapsed contact or
				 * an expanded contact itself so we show the most recent
				 * (largest) last_seen time for any of the buddies under
				 * the contact. */
				for (bnode = ((GaimBlistNode *)c)->child ; bnode != NULL ; bnode = bnode->next)
				{
					int value = gaim_blist_node_get_int(bnode, "last_seen");
					if (value > lastseen)
						lastseen = value;
				}
			}
			else
			{
				/* We're dealing with a buddy under an expanded contact,
				 * so we show the last_seen time for the buddy. */
				lastseen = gaim_blist_node_get_int(&b->node, "last_seen");
			}

			if (lastseen > 0)
			{
				tmp = gaim_str_seconds_to_string(time(NULL) - lastseen);
				g_string_append_printf(str, _("\n<b>Last Seen:</b> %s ago"), tmp);
				g_free(tmp);
			}
		}


		/* Offline? */
		/* FIXME: Why is this status special-cased by the core? -- rlaager */
		if (!GAIM_BUDDY_IS_ONLINE(b)) {
			g_string_append_printf(str, _("\n<b>Status:</b> Offline"));
		}

		if (prpl_info && prpl_info->tooltip_text)
		{
			/* Additional text from the PRPL */
			prpl_info->tooltip_text(b, str, full);
		}

		/* These are Easter Eggs.  Patches to remove them will be rejected. */
		if (!g_ascii_strcasecmp(b->name, "robflynn"))
			g_string_append(str, _("\n<b>Description:</b> Spooky"));
		if (!g_ascii_strcasecmp(b->name, "seanegn"))
			g_string_append(str, _("\n<b>Status:</b> Awesome"));
		if (!g_ascii_strcasecmp(b->name, "chipx86"))
			g_string_append(str, _("\n<b>Status:</b> Rockin'"));
	}

	gaim_signal_emit(gaim_gtk_blist_get_handle(),
			 "drawing-tooltip", node, str, full);

	return g_string_free(str, FALSE);
}

struct _emblem_data {
	const char *filename;
	int x;
	int y;
};

static void g_string_destroy(GString *destroyable)
{
	g_string_free(destroyable, TRUE);
}

static void
gaim_gtk_blist_update_buddy_status_icon_key(struct _gaim_gtk_blist_node *gtkbuddynode, GaimBuddy *buddy, GaimStatusIconSize size)
{
	GString *key = g_string_sized_new(16);

	if(gtkbuddynode && gtkbuddynode->recent_signonoff) {
		if(GAIM_BUDDY_IS_ONLINE(buddy))
			g_string_printf(key, "login");
		else
			g_string_printf(key, "logout");
	} else {
		int i;
		const char *protoname = NULL;
		GaimAccount *account = buddy->account;
		GaimPlugin *prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
		GaimPluginProtocolInfo *prpl_info;
		GaimConversation *conv;
		struct _emblem_data emblems[4] = {{NULL, 15, 15}, {NULL, 0, 15},
			{NULL, 0, 0}, {NULL, 15, 0}};

		if(!prpl)
			return;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if(prpl_info && prpl_info->list_icon) {
			protoname = prpl_info->list_icon(account, buddy);
		}
		if(prpl_info && prpl_info->list_emblems) {
			prpl_info->list_emblems(buddy, &emblems[0].filename,
					&emblems[1].filename, &emblems[2].filename,
					&emblems[3].filename);
		}

		g_string_assign(key, protoname);

		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM,
			gaim_buddy_get_name(buddy), account);

		if(conv != NULL) {
			GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);
			if(gaim_gtkconv_is_hidden(gtkconv)) {
				/* add pending emblem */
				if(size == GAIM_STATUS_ICON_SMALL) {
					emblems[0].filename = "pending";
				}
				else {
					emblems[3].filename = emblems[2].filename;
					emblems[2].filename = "pending";
				}
			}
		}

		if(size == GAIM_STATUS_ICON_SMALL) {
			/* So that only the se icon will composite */
			emblems[1].filename = emblems[2].filename = emblems[3].filename = NULL;
		}

		for(i = 0; i < 4; i++) {
			if(emblems[i].filename)
				g_string_append_printf(key, "/%s", emblems[i].filename);
		}
	}

	if (!GAIM_BUDDY_IS_ONLINE(buddy))
		g_string_append(key, "/off");
	else if (gaim_presence_is_idle(gaim_buddy_get_presence(buddy)))
		g_string_append(key, "/idle");

	if (!gaim_privacy_check(buddy->account, gaim_buddy_get_name(buddy)))
		g_string_append(key, "/priv");

	if (gtkbuddynode->status_icon_key)
		g_string_free(gtkbuddynode->status_icon_key, TRUE);
	gtkbuddynode->status_icon_key = key;

}

GdkPixbuf *
gaim_gtk_blist_get_status_icon(GaimBlistNode *node, GaimStatusIconSize size)
{
	GdkPixbuf *scale, *status = NULL;
	int i, scalesize = 30;
	char *filename;
	GString *key = g_string_sized_new(16);
	const char *protoname = NULL;
	struct _gaim_gtk_blist_node *gtknode = node->ui_data;
	struct _gaim_gtk_blist_node *gtkbuddynode = NULL;
	struct _emblem_data emblems[4] = {{NULL, 15, 15}, {NULL, 0, 15},
		{NULL, 0, 0}, {NULL, 15, 0}};
	GaimPresence *presence = NULL;
	GaimBuddy *buddy = NULL;
	GaimChat *chat = NULL;

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = gaim_contact_get_priority_buddy((GaimContact*)node);
			gtkbuddynode = ((GaimBlistNode*)buddy)->ui_data;
		}
	} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		buddy = (GaimBuddy*)node;
		gtkbuddynode = node->ui_data;
	} else if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		chat = (GaimChat*)node;
	} else {
		return NULL;
	}

	if (!status_icon_hash_table) {
		status_icon_hash_table = g_hash_table_new_full((GHashFunc)g_string_hash,
								(GEqualFunc)g_string_equal,
								(GDestroyNotify)g_string_destroy,
								(GDestroyNotify)gdk_pixbuf_unref);

	} else if (buddy && gtkbuddynode->status_icon_key && gtkbuddynode->status_icon_key->str) {
		key = g_string_new(gtkbuddynode->status_icon_key->str);

		/* Respect the size request given */
		if (size == GAIM_STATUS_ICON_SMALL) {
			g_string_append(key, "/tiny");
		}

		scale = g_hash_table_lookup(status_icon_hash_table, key);
		if (scale) {
			gdk_pixbuf_ref(scale);
			g_string_free(key, TRUE);
			return scale;
		}
	}

	if(buddy || chat) {
		GaimAccount *account;
		GaimPlugin *prpl;
		GaimPluginProtocolInfo *prpl_info;

		if(buddy)
			account = buddy->account;
		else
			account = chat->account;

		prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
		if(!prpl)
			return NULL;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if(prpl_info && prpl_info->list_icon) {
			protoname = prpl_info->list_icon(account, buddy);
		}
		if(prpl_info && prpl_info->list_emblems && buddy) {
			if(gtknode && !gtknode->recent_signonoff)
				prpl_info->list_emblems(buddy, &emblems[0].filename,
						&emblems[1].filename, &emblems[2].filename,
						&emblems[3].filename);
		}
	}

/* Begin Generating Lookup Key */
	if (buddy) {
		gaim_gtk_blist_update_buddy_status_icon_key(gtkbuddynode, buddy, size);
		g_string_assign(key, gtkbuddynode->status_icon_key->str);
	}
	/* There are only two options for chat or gaimdude - big or small */
	else if (chat) {
		GaimAccount *account;
		GaimPlugin *prpl;
		GaimPluginProtocolInfo *prpl_info;

		account = chat->account;

		prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
		if(!prpl)
			return NULL;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if(prpl_info && prpl_info->list_icon) {
			protoname = prpl_info->list_icon(account, NULL);
		}
		g_string_append_printf(key, "%s-chat", protoname);
	}
	else
		g_string_append(key, "gaimdude");

	/* If the icon is small, we do not store this into the status_icon_key
	 * in the gtkbuddynode.  This way we can respect the size value on cache
	 * lookup. Otherwise, different sized icons could not be stored easily.
	 */
	if (size == GAIM_STATUS_ICON_SMALL) {
		g_string_append(key, "/tiny");
	}

/* End Generating Lookup Key */

/* If we already know this icon, just return it */
	scale = g_hash_table_lookup(status_icon_hash_table, key);
	if (scale) {
		gdk_pixbuf_ref(scale);
		g_string_free(key, TRUE);
		return scale;
	}

/* Create a new composite icon */

	if(buddy) {
		GaimAccount *account;
		GaimPlugin *prpl;
		GaimPluginProtocolInfo *prpl_info;
		GaimConversation *conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM,
			gaim_buddy_get_name(buddy),
			gaim_buddy_get_account(buddy));

		account = buddy->account;

		prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
		if(!prpl)
			return NULL;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		if(prpl_info && prpl_info->list_icon) {
			protoname = prpl_info->list_icon(account, buddy);
		}
		if(prpl_info && prpl_info->list_emblems) {
			if(gtknode && !gtknode->recent_signonoff)
				prpl_info->list_emblems(buddy, &emblems[0].filename,
						&emblems[1].filename, &emblems[2].filename,
						&emblems[3].filename);
		}

		if(conv != NULL) {
			GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);
			if(gtkconv != NULL && gaim_gtkconv_is_hidden(gtkconv)) {
				/* add pending emblem */
				if(size == GAIM_STATUS_ICON_SMALL) {
					emblems[0].filename="pending";
				}
				else {
					emblems[3].filename=emblems[2].filename;
					emblems[2].filename="pending";
				}
			}
		}
	}

	if(size == GAIM_STATUS_ICON_SMALL) {
		scalesize = 15;
		/* So that only the se icon will composite */
		emblems[1].filename = emblems[2].filename = emblems[3].filename = NULL;
	}

	if(buddy && GAIM_BUDDY_IS_ONLINE(buddy) &&  gtkbuddynode && gtkbuddynode->recent_signonoff) {
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "login.png", NULL);
	} else if(buddy && !GAIM_BUDDY_IS_ONLINE(buddy) && gtkbuddynode && gtkbuddynode->recent_signonoff) {
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "logout.png", NULL);
	} else if(buddy || chat) {
		char *image = g_strdup_printf("%s.png", protoname);
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
		g_free(image);
	} else {
		/* gaim dude */
		filename = g_build_filename(DATADIR, "pixmaps", "gaim.png", NULL);
	}

	status = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	if(!status) {
		g_string_free(key, TRUE);
		return NULL;
	}

	scale = gdk_pixbuf_scale_simple(status, scalesize, scalesize,
			GDK_INTERP_BILINEAR);
	g_object_unref(status);

	for(i=0; i<4; i++) {
		if(emblems[i].filename) {
			GdkPixbuf *emblem;
			char *image = g_strdup_printf("%s.png", emblems[i].filename);
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", image, NULL);
			g_free(image);
			emblem = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);
			if(emblem) {
				if(i == 0 && size == GAIM_STATUS_ICON_SMALL) {
					double scale_factor = 0.6;
					if(gdk_pixbuf_get_width(emblem) > 20)
						scale_factor = 9.0 / gdk_pixbuf_get_width(emblem);

					gdk_pixbuf_composite(emblem,
							scale, 5, 5,
							10, 10,
							5, 5,
							scale_factor, scale_factor,
							GDK_INTERP_BILINEAR,
							255);
				} else {
					double scale_factor = 1.0;
					if(gdk_pixbuf_get_width(emblem) > 20)
						scale_factor = 15.0 / gdk_pixbuf_get_width(emblem);

					gdk_pixbuf_composite(emblem,
							scale, emblems[i].x, emblems[i].y,
							15, 15,
							emblems[i].x, emblems[i].y,
							scale_factor, scale_factor,
							GDK_INTERP_BILINEAR,
							255);
				}
				g_object_unref(emblem);
			}
		}
	}

	if(buddy) {
		presence = gaim_buddy_get_presence(buddy);
		if (!GAIM_BUDDY_IS_ONLINE(buddy))
			gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);
		else if (gaim_presence_is_idle(presence))
		{
			gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.25, FALSE);
		}

		if (!gaim_privacy_check(buddy->account, gaim_buddy_get_name(buddy)))
		{
			GdkPixbuf *emblem;
			char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status", "default", "blocked.png", NULL);

			emblem = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (emblem)
			{
				gdk_pixbuf_composite(emblem, scale,
						0, 0, scalesize, scalesize,
						0, 0,
						(double)scalesize / gdk_pixbuf_get_width(emblem),
						(double)scalesize / gdk_pixbuf_get_height(emblem),
						GDK_INTERP_BILINEAR,
						224);
				g_object_unref(emblem);
			}
		}
	}

	/* Insert the new icon into the status icon hash table */
	g_hash_table_insert (status_icon_hash_table, key, scale);
	gdk_pixbuf_ref(scale);

	return scale;
}

static gchar *gaim_gtk_blist_get_name_markup(GaimBuddy *b, gboolean selected)
{
	const char *name;
	char *esc, *text = NULL;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimContact *contact;
	GaimPresence *presence;
	struct _gaim_gtk_blist_node *gtkcontactnode = NULL;
	char *idletime = NULL, *statustext = NULL;
	time_t t;
	/* XXX Good luck cleaning up this crap */

	contact = (GaimContact*)((GaimBlistNode*)b)->parent;
	if(contact)
		gtkcontactnode = ((GaimBlistNode*)contact)->ui_data;

	if(gtkcontactnode && !gtkcontactnode->contact_expanded && contact->alias)
		name = contact->alias;
	else
		name = gaim_buddy_get_alias(b);
	esc = g_markup_escape_text(name, strlen(name));

	presence = gaim_buddy_get_presence(b);

	if (!gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
	{
		if (!selected && gaim_presence_is_idle(presence))
		{
			text = g_strdup_printf("<span color='%s'>%s</span>",
					       dim_grey(), esc);
			g_free(esc);
			return text;
		}
		else
			return esc;
	}

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(b->account));

	if (prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && prpl_info->status_text && b->account->gc) {
		char *tmp = prpl_info->status_text(b);
		const char *end;

		if(tmp && !g_utf8_validate(tmp, -1, &end)) {
			char *new = g_strndup(tmp,
					g_utf8_pointer_to_offset(tmp, end));
			g_free(tmp);
			tmp = new;
		}

#if !GTK_CHECK_VERSION(2,6,0)
		if(tmp) {
			char buf[32];
			char *c = tmp;
			int length = 0, vis=0;
			gboolean inside = FALSE;
			g_strdelimit(tmp, "\n", ' ');
			gaim_str_strip_char(tmp, '\r');

			while(*c && vis < 20) {
				if(*c == '&')
					inside = TRUE;
				else if(*c == ';')
					inside = FALSE;
				if(!inside)
					vis++;
				c = g_utf8_next_char(c); /* this is fun */
			}

			length = c - tmp;

			if(vis == 20)
				g_snprintf(buf, sizeof(buf), "%%.%ds...", length);
			else
				g_snprintf(buf, sizeof(buf), "%%s ");

			statustext = g_strdup_printf(buf, tmp);

			g_free(tmp);
		}
#else
		if(tmp) {
			g_strdelimit(tmp, "\n", ' ');
			gaim_str_strip_char(tmp, '\r');
		}
		statustext = tmp;
#endif
	}

	if(!gaim_presence_is_online(presence) && !statustext)
		statustext = g_strdup(_("Offline"));
	else if (!statustext)
		text = g_strdup(esc);

	if (gaim_presence_is_idle(presence)) {
		if (gaim_prefs_get_bool("/gaim/gtk/blist/show_idle_time")) {
			time_t idle_secs = gaim_presence_get_idle_time(presence);

			if (idle_secs > 0) {
				int ihrs, imin;

				time(&t);
				ihrs = (t - idle_secs) / 3600;
				imin = ((t - idle_secs) / 60) % 60;

				if (ihrs)
					idletime = g_strdup_printf(_("Idle %dh %02dm"), ihrs, imin);
				else
					idletime = g_strdup_printf(_("Idle %dm"), imin);
			}
			else
				idletime = g_strdup(_("Idle"));

			if (!selected)
				text = g_strdup_printf("<span color='%s'>%s</span>\n"
				"<span color='%s' size='smaller'>%s%s%s</span>",
				dim_grey(), esc, dim_grey(),
				idletime != NULL ? idletime : "",
				(idletime != NULL && statustext != NULL) ? " - " : "",
				statustext != NULL ? statustext : "");
		}
		else if (!selected && !statustext) /* We handle selected text later */
			text = g_strdup_printf("<span color='%s'>%s</span>", dim_grey(), esc);
		else if (!selected && !text)
			text = g_strdup_printf("<span color='%s'>%s</span>\n"
				"<span color='%s' size='smaller'>%s</span>",
				dim_grey(), esc, dim_grey(),
				statustext != NULL ? statustext : "");
	}

	/* Not idle and not selected */
	else if (!selected && !text)
	{
		text = g_strdup_printf("%s\n"
			"<span color='%s' size='smaller'>%s</span>",
			esc, dim_grey(),
			statustext != NULL ? statustext :  "");
	}

	/* It is selected. */
	if ((selected && !text) || (selected && idletime))
		text = g_strdup_printf("%s\n"
			"<span size='smaller'>%s%s%s</span>",
			esc,
			idletime != NULL ? idletime : "",
			(idletime != NULL && statustext != NULL) ? " - " : "",
			statustext != NULL ? statustext :  "");

	g_free(idletime);
	g_free(statustext);
	g_free(esc);

	return text;
}

static void gaim_gtk_blist_restore_position()
{
	int blist_x, blist_y, blist_width, blist_height;

	blist_width = gaim_prefs_get_int("/gaim/gtk/blist/width");

	/* if the window exists, is hidden, we're saving positions, and the
	 * position is sane... */
	if (gtkblist && gtkblist->window &&
		!GTK_WIDGET_VISIBLE(gtkblist->window) && blist_width != 0) {

		blist_x      = gaim_prefs_get_int("/gaim/gtk/blist/x");
		blist_y      = gaim_prefs_get_int("/gaim/gtk/blist/y");
		blist_height = gaim_prefs_get_int("/gaim/gtk/blist/height");

		/* ...check position is on screen... */
		if (blist_x >= gdk_screen_width())
			blist_x = gdk_screen_width() - 100;
		else if (blist_x + blist_width < 0)
			blist_x = 100;

		if (blist_y >= gdk_screen_height())
			blist_y = gdk_screen_height() - 100;
		else if (blist_y + blist_height < 0)
			blist_y = 100;

		/* ...and move it back. */
		gtk_window_move(GTK_WINDOW(gtkblist->window), blist_x, blist_y);
		gtk_window_resize(GTK_WINDOW(gtkblist->window), blist_width, blist_height);
		if (gaim_prefs_get_bool("/gaim/gtk/blist/list_maximized"))
			gtk_window_maximize(GTK_WINDOW(gtkblist->window));
	}
}

static gboolean gaim_gtk_blist_refresh_timer(GaimBuddyList *list)
{
	GaimBlistNode *gnode, *cnode;

	if (gtk_blist_obscured || !GTK_WIDGET_VISIBLE(gtkblist->window))
		return TRUE;

	for(gnode = list->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				GaimBuddy *buddy;

				buddy = gaim_contact_get_priority_buddy((GaimContact*)cnode);

				if (buddy &&
						gaim_presence_is_idle(gaim_buddy_get_presence(buddy)))
					gaim_gtk_blist_update_contact(list, (GaimBlistNode*)buddy);
			}
		}
	}

	/* keep on going */
	return TRUE;
}

static void gaim_gtk_blist_hide_node(GaimBuddyList *list, GaimBlistNode *node, gboolean update)
{
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GtkTreeIter iter;

	if (!gtknode || !gtknode->row || !gtkblist)
		return;

	if(gtkblist->selected_node == node)
		gtkblist->selected_node = NULL;
	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(update && (GAIM_BLIST_NODE_IS_CONTACT(node) ||
			GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CHAT(node))) {
			gaim_gtk_blist_update(list, node->parent);
		}
	}
	gtk_tree_row_reference_free(gtknode->row);
	gtknode->row = NULL;
}

static const char *require_connection[] =
{
	N_("/Buddies/New Instant Message..."),
	N_("/Buddies/Join a Chat..."),
	N_("/Buddies/Get User Info..."),
	N_("/Buddies/Add Buddy..."),
	N_("/Buddies/Add Chat..."),
	N_("/Buddies/Add Group..."),
};

static const int require_connection_size = sizeof(require_connection)
											/ sizeof(*require_connection);

/**
 * Rebuild dynamic menus and make menu items sensitive/insensitive
 * where appropriate.
 */
static void
update_menu_bar(GaimGtkBuddyList *gtkblist)
{
	GtkWidget *widget;
	gboolean sensitive;
	int i;

	g_return_if_fail(gtkblist != NULL);

	gaim_gtk_blist_update_accounts_menu();

	sensitive = (gaim_connections_get_all() != NULL);

	for (i = 0; i < require_connection_size; i++)
	{
		widget = gtk_item_factory_get_widget(gtkblist->ift, require_connection[i]);
		gtk_widget_set_sensitive(widget, sensitive);
	}

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Join a Chat..."));
	gtk_widget_set_sensitive(widget, gaim_gtk_blist_joinchat_is_showable());

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Add Chat..."));
	gtk_widget_set_sensitive(widget, gaim_gtk_blist_joinchat_is_showable());

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Buddy Pounces"));
	gtk_widget_set_sensitive(widget, (gaim_accounts_get_all() != NULL));

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Privacy"));
	gtk_widget_set_sensitive(widget, (gaim_connections_get_all() != NULL));

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Room List"));
	gtk_widget_set_sensitive(widget, gaim_gtk_roomlist_is_showable());
}

static void
sign_on_off_cb(GaimConnection *gc, GaimBuddyList *blist)
{
	GaimGtkBuddyList *gtkblist = GAIM_GTK_BLIST(blist);

	update_menu_bar(gtkblist);
}

static void
plugin_changed_cb(GaimPlugin *p, gpointer *data)
{
	gaim_gtk_blist_update_plugin_actions();
}

static void
unseen_conv_menu()
{
	static GtkWidget *menu = NULL;
	GList *convs = NULL;

	if (menu)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	convs = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_IM, GAIM_UNSEEN_TEXT, TRUE, 0);
	if (!convs) {
		/* no conversations added, don't show the menu */
		gtk_widget_destroy(menu);
		return;
	}
	gaim_gtk_conversations_fill_menu(menu, convs);
	g_list_free(convs);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3,
			gtk_get_current_event_time());
}

static gboolean
menutray_press_cb(GtkWidget *widget, GdkEventButton *event)
{
	GList *convs;

	switch (event->button) {
		case 1:
			convs = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_IM,
															GAIM_UNSEEN_TEXT, TRUE, 1);
			if (convs) {
				gaim_gtkconv_present_conversation((GaimConversation*)convs->data);
				g_list_free(convs);
			}
			break;
		case 3:
			unseen_conv_menu();
			break;
	}
	return TRUE;
}

static void
conversation_updated_cb(GaimConversation *conv, GaimConvUpdateType type,
                        GaimGtkBuddyList *gtkblist)
{
	GList *convs = NULL;
	GList *l = NULL;

	if (type != GAIM_CONV_UPDATE_UNSEEN)
		return;

	if(conv->account != NULL && conv->name != NULL) {
		GaimBuddy *buddy = gaim_find_buddy(conv->account, conv->name);
		if(buddy != NULL)
			gaim_gtk_blist_update_buddy(NULL, (GaimBlistNode *)buddy, TRUE);
	}

	if (gtkblist->menutrayicon) {
		gtk_widget_destroy(gtkblist->menutrayicon);
		gtkblist->menutrayicon = NULL;
	}

	convs = gaim_gtk_conversations_find_unseen_list(GAIM_CONV_TYPE_IM, GAIM_UNSEEN_TEXT, TRUE, 0);
	if (convs) {
		GtkWidget *img = NULL;
		GString *tooltip_text = NULL;

		tooltip_text = g_string_new("");
		l = convs;
		while (l != NULL) {
			if (GAIM_IS_GTK_CONVERSATION(l->data)) {
				GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION((GaimConversation *)l->data);

				g_string_append_printf(tooltip_text,
						ngettext("%d unread message from %s\n", "%d unread messages from %s\n", gtkconv->unseen_count),
						gtkconv->unseen_count,
						gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
			}
			l = l->next;
		}
		if(tooltip_text->len > 0) {
			/* get rid of the last newline */
			g_string_truncate(tooltip_text, tooltip_text->len -1);
			img = gtk_image_new_from_stock(GAIM_STOCK_PENDING, GTK_ICON_SIZE_MENU);

			gtkblist->menutrayicon = gtk_event_box_new();
			gtk_container_add(GTK_CONTAINER(gtkblist->menutrayicon), img);
			gtk_widget_show(img);
			gtk_widget_show(gtkblist->menutrayicon);
			g_signal_connect(G_OBJECT(gtkblist->menutrayicon), "button-press-event", G_CALLBACK(menutray_press_cb), NULL);

			gaim_gtk_menu_tray_append(GAIM_GTK_MENU_TRAY(gtkblist->menutray), gtkblist->menutrayicon, tooltip_text->str);
		}
		g_string_free(tooltip_text, TRUE);
		g_list_free(convs);
	}
}

static void
conversation_deleting_cb(GaimConversation *conv, GaimGtkBuddyList *gtkblist)
{
	conversation_updated_cb(conv, GAIM_CONV_UPDATE_UNSEEN, gtkblist);
}

/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/

static void gaim_gtk_blist_new_list(GaimBuddyList *blist)
{
	GaimGtkBuddyList *gtkblist;

	gtkblist = g_new0(GaimGtkBuddyList, 1);
	gtkblist->connection_errors = g_hash_table_new_full(g_direct_hash,
												g_direct_equal, NULL, g_free);
	blist->ui_data = gtkblist;
}

static void gaim_gtk_blist_new_node(GaimBlistNode *node)
{
	node->ui_data = g_new0(struct _gaim_gtk_blist_node, 1);
}

gboolean gaim_gtk_blist_node_is_contact_expanded(GaimBlistNode *node)
{
	if GAIM_BLIST_NODE_IS_BUDDY(node)
		node = node->parent;

	g_return_val_if_fail(GAIM_BLIST_NODE_IS_CONTACT(node), FALSE);

	return ((struct _gaim_gtk_blist_node *)node->ui_data)->contact_expanded;
}

enum {
	DRAG_BUDDY,
	DRAG_ROW,
	DRAG_VCARD,
	DRAG_TEXT,
	DRAG_URI,
	NUM_TARGETS
};

static const char *
item_factory_translate_func (const char *path, gpointer func_data)
{
	return _((char *)path);
}

void gaim_gtk_blist_setup_sort_methods()
{
	gaim_gtk_blist_sort_method_reg("none", _("Manually"), sort_method_none);
#if GTK_CHECK_VERSION(2,2,1)
	gaim_gtk_blist_sort_method_reg("alphabetical", _("Alphabetically"), sort_method_alphabetical);
	gaim_gtk_blist_sort_method_reg("status", _("By status"), sort_method_status);
	gaim_gtk_blist_sort_method_reg("log_size", _("By log size"), sort_method_log);
#endif
	gaim_gtk_blist_sort_method_set(gaim_prefs_get_string("/gaim/gtk/blist/sort_type"));
}

static void _prefs_change_redo_list()
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GaimBlistNode *node = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	}

	redo_buddy_list(gaim_get_blist(), FALSE, FALSE);
#if GTK_CHECK_VERSION(2,6,0)
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gtkblist->treeview));
#endif

	if (node)
	{
		struct _gaim_gtk_blist_node *gtknode;
		GtkTreePath *path;

		gtknode = node->ui_data;
		if (gtknode && gtknode->row)
		{
			path = gtk_tree_row_reference_get_path(gtknode->row);
			gtk_tree_selection_select_path(sel, path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(gtkblist->treeview), path, NULL, FALSE, 0, 0);
			gtk_tree_path_free(path);
		}
	}
}

static void _prefs_change_sort_method(const char *pref_name, GaimPrefType type,
									  gconstpointer val, gpointer data)
{
	if(!strcmp(pref_name, "/gaim/gtk/blist/sort_type"))
		gaim_gtk_blist_sort_method_set(val);
}

/*
 * "This is so dead sexy."
 * "Two thumbs up."
 * "Best movie of the year."
 *
 * This is the function that handles CTRL+F searching in the buddy list.
 * It finds the top-most buddy/group/chat/whatever containing the
 * entered string.
 *
 * It's somewhat ineffecient, because we strip all the HTML from the
 * "name" column of the buddy list (because the GtkTreeModel does not
 * contain the screen name in a non-markedup format).  But the alternative
 * is to add an extra column to the GtkTreeModel.  And this function is
 * used rarely, so it shouldn't matter TOO much.
 */
static gboolean
_search_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	gchar *enteredstring;
	gchar *tmp;
	gchar *withmarkup;
	gchar *nomarkup;
	gchar *normalized;
	gboolean result;
	size_t i;
	size_t len;
	PangoLogAttr *log_attrs;
	gchar *word;

	if (strcasecmp(key, "Global Thermonuclear War") == 0)
	{
		gaim_notify_info(NULL, "WOPR",
				"Wouldn't you prefer a nice game of chess?", NULL);
		return FALSE;
	}

	gtk_tree_model_get(model, iter, column, &withmarkup, -1);

	tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
	enteredstring = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	nomarkup = gaim_markup_strip_html(withmarkup);
	tmp = g_utf8_normalize(nomarkup, -1, G_NORMALIZE_DEFAULT);
	g_free(nomarkup);
	normalized = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	if (gaim_str_has_prefix(normalized, enteredstring))
	{
		g_free(withmarkup);
		g_free(enteredstring);
		g_free(normalized);
		return FALSE;
	}


	/* Use Pango to separate by words. */
	len = g_utf8_strlen(normalized, -1);
	log_attrs = g_new(PangoLogAttr, len + 1);

	pango_get_log_attrs(normalized, strlen(normalized), -1, NULL, log_attrs, len + 1);

	word = normalized;
	result = TRUE;
	for (i = 0; i < (len - 1) ; i++)
	{
		if (log_attrs[i].is_word_start &&
		    gaim_str_has_prefix(word, enteredstring))
		{
			result = FALSE;
			break;
		}
		word = g_utf8_next_char(word);
	}
	g_free(log_attrs);

/* The non-Pango version. */
#if 0
	word = normalized;
	result = TRUE;
	while (word[0] != '\0')
	{
		gunichar c = g_utf8_get_char(word);
		if (!g_unichar_isalnum(c))
		{
			word = g_utf8_find_next_char(word, NULL);
			if (gaim_str_has_prefix(word, enteredstring))
			{
				result = FALSE;
				break;
			}
		}
		else
			word = g_utf8_find_next_char(word, NULL);
	}
#endif

	g_free(withmarkup);
	g_free(enteredstring);
	g_free(normalized);

	return result;
}

static void account_modified(GaimAccount *account, GaimGtkBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	if (gaim_accounts_get_all_active())
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 1);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 0);

	update_menu_bar(gtkblist);
}

static void
account_status_changed(GaimAccount *account, GaimStatus *old,
					   GaimStatus *new, GaimGtkBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	update_menu_bar(gtkblist);
}

static gboolean
gtk_blist_window_key_press_cb(GtkWidget *w, GdkEventKey *event, GaimGtkBuddyList *gtkblist)
{
	GtkWidget *imhtml;

	if (!gtkblist)
		return FALSE;

	imhtml = gtk_window_get_focus(GTK_WINDOW(gtkblist->window));

	if (GTK_IS_IMHTML(imhtml) && gtk_bindings_activate(GTK_OBJECT(imhtml), event->keyval, event->state))
		return TRUE;
	return FALSE;
}

/***********************************/
/* Connection error handling stuff */
/***********************************/

static void
ce_modify_account_cb(GaimAccount *account)
{
	gaim_gtk_account_dialog_show(GAIM_GTK_MODIFY_ACCOUNT_DIALOG, account);
}

static void
ce_enable_account_cb(GaimAccount *account)
{
	gaim_account_set_enabled(account, gaim_core_get_ui(), TRUE);
}

static void
connection_error_button_clicked_cb(GtkButton *widget, gpointer user_data)
{
	GaimAccount *account;
	char *primary;
	const char *text;
	gboolean enabled;

	account = user_data;
	primary = g_strdup_printf(_("%s disconnected"),
							  gaim_account_get_username(account));
	text = g_hash_table_lookup(gtkblist->connection_errors, account);

	enabled = gaim_account_get_enabled(account, gaim_core_get_ui());
	gaim_request_action(account, _("Connection Error"), primary, text, 2,
						account, 3,
						_("OK"), NULL,
						_("Modify Account"), GAIM_CALLBACK(ce_modify_account_cb),
						enabled ? _("Connect") : _("Re-enable Account"),
						enabled ? GAIM_CALLBACK(gaim_account_connect) :
									GAIM_CALLBACK(ce_enable_account_cb));
	g_free(primary);
	gtk_widget_destroy(GTK_WIDGET(widget));
	g_hash_table_remove(gtkblist->connection_errors, account);
}

/* Add some buttons that show connection errors */
static void
create_connection_error_buttons(gpointer key, gpointer value,
                                gpointer user_data)
{
	GaimAccount *account;
	GaimStatusType *status_type;
	gchar *escaped, *text;
	GtkWidget *button, *label, *image, *hbox;
	GdkPixbuf *pixbuf;

	account = key;
	escaped = g_markup_escape_text((const gchar *)value, -1);
	text = g_strdup_printf(_("<span color=\"red\">%s disconnected: %s</span>"),
	                       gaim_account_get_username(account),
	                       escaped);
	g_free(escaped);

	hbox = gtk_hbox_new(FALSE, 0);

	/* Create the icon */
	if ((status_type = gaim_account_get_status_type_with_primitive(account,
							GAIM_STATUS_OFFLINE))) {
		pixbuf = gaim_gtk_create_prpl_icon_with_status(account, status_type, 0.5);
		if (pixbuf != NULL) {
			image = gtk_image_new_from_pixbuf(pixbuf);
			g_object_unref(pixbuf);

			gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE,
			                   GAIM_HIG_BOX_SPACE);
		}
	}

	/* Create the text */
	label = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
#if GTK_CHECK_VERSION(2,6,0)
	g_object_set(label, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE,
	                   GAIM_HIG_BOX_SPACE);

	/* Create the actual button and put the icon and text on it */
	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), hbox);
	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(connection_error_button_clicked_cb),
	                 account);
	gtk_widget_show_all(button);
	gtk_box_pack_end(GTK_BOX(gtkblist->error_buttons), button,
	                 FALSE, FALSE, 0);
}

void
gaim_gtk_blist_update_account_error_state(GaimAccount *account, const char *text)
{
	GList *l;

	if (text == NULL)
		g_hash_table_remove(gtkblist->connection_errors, account);
	else
		g_hash_table_insert(gtkblist->connection_errors, account, g_strdup(text));

	/* Remove the old error buttons */
	for (l = gtk_container_get_children(GTK_CONTAINER(gtkblist->error_buttons));
			l != NULL;
			l = l->next)
	{
		gtk_widget_destroy(GTK_WIDGET(l->data));
	}

	/* Add new error buttons */
	g_hash_table_foreach(gtkblist->connection_errors,
			create_connection_error_buttons, NULL);
}


/******************************************/
/* End of connection error handling stuff */
/******************************************/

#if 0
static GtkWidget *
kiosk_page()
{
	GtkWidget *ret = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *bbox;
	GtkWidget *button;

	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ret), label, TRUE, TRUE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Username:</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(ret), entry, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Password:</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(ret), entry, FALSE, FALSE, 0);

	label = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);

	bbox = gtk_hbutton_box_new();
	button = gtk_button_new_with_mnemonic(_("_Login"));
	gtk_box_pack_start(GTK_BOX(ret), bbox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(bbox), button);

	
	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ret), label, TRUE, TRUE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(ret), GAIM_HIG_BORDER);

	gtk_widget_show_all(ret);
	return ret;
}
#endif

static void gaim_gtk_blist_show(GaimBuddyList *list)
{
	void *handle;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *column;
	GtkWidget *menu;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *label;
	char *pretty;
	GtkAccelGroup *accel_group;
	GtkTreeSelection *selection;
	GtkTargetEntry dte[] = {{"GAIM_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD },
				{"text/uri-list", 0, DRAG_URI},
				{"text/plain", 0, DRAG_TEXT}};
	GtkTargetEntry ste[] = {{"GAIM_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD }};
	if (gtkblist && gtkblist->window) {
		gaim_blist_set_visible(gaim_prefs_get_bool("/gaim/gtk/blist/list_visible"));
		return;
	}

	gtkblist = GAIM_GTK_BLIST(list);

	gtkblist->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(gtkblist->window), "buddy_list");
	gtk_window_set_title(GTK_WINDOW(gtkblist->window), _("Buddy List"));
	GTK_WINDOW(gtkblist->window)->allow_shrink = TRUE;

	gtkblist->main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(gtkblist->main_vbox);
	gtk_container_add(GTK_CONTAINER(gtkblist->window), gtkblist->main_vbox);

	g_signal_connect(G_OBJECT(gtkblist->window), "delete_event", G_CALLBACK(gtk_blist_delete_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "configure_event", G_CALLBACK(gtk_blist_configure_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "visibility_notify_event", G_CALLBACK(gtk_blist_visibility_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "window_state_event", G_CALLBACK(gtk_blist_window_state_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "key_press_event", G_CALLBACK(gtk_blist_window_key_press_cb), gtkblist);
	gtk_widget_add_events(gtkblist->window, GDK_VISIBILITY_NOTIFY_MASK);

	/******************************* Menu bar *************************************/
	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW (gtkblist->window), accel_group);
	g_object_unref(accel_group);
	gtkblist->ift = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<GaimMain>", accel_group);
	gtk_item_factory_set_translate_func(gtkblist->ift,
										(GtkTranslateFunc)item_factory_translate_func,
										NULL, NULL);
	gtk_item_factory_create_items(gtkblist->ift, sizeof(blist_menu) / sizeof(*blist_menu),
								  blist_menu, NULL);
	gaim_gtk_load_accels();
	g_signal_connect(G_OBJECT(accel_group), "accel-changed",
														G_CALLBACK(gaim_gtk_save_accels_cb), NULL);
	menu = gtk_item_factory_get_widget(gtkblist->ift, "<GaimMain>");
	gtkblist->menutray = gaim_gtk_menu_tray_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtkblist->menutray);
	gtk_widget_show(gtkblist->menutray);
	gtk_widget_show(menu);
	gtk_box_pack_start(GTK_BOX(gtkblist->main_vbox), menu, FALSE, FALSE, 0);

	accountmenu = gtk_item_factory_get_widget(gtkblist->ift, N_("/Accounts"));


	/****************************** Notebook *************************************/
	gtkblist->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkblist->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(gtkblist->notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(gtkblist->main_vbox), gtkblist->notebook, TRUE, TRUE, 0);

#if 0
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook), kiosk_page(), NULL);
#endif

	/* Translators: Please maintain the use of -> and <- to refer to menu heirarchy */
	pretty = gaim_gtk_make_pretty_arrows(_("<span weight='bold' size='larger'>Welcome to Gaim!</span>\n\n"
					       
					       "You have no accounts enabled. Enable your IM accounts from the "
					       "<b>Accounts</b> window at <b>Accounts->Add/Edit</b>. Once you "
					       "enable accounts, you'll be able to sign on, set your status, "
					       "and talk to your friends."));
	label = gtk_label_new(NULL);
	gtk_widget_set_size_request(label, gaim_prefs_get_int("/gaim/gtk/blist/width") - 12, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.2);
	gtk_label_set_markup(GTK_LABEL(label), pretty);
	g_free(pretty);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook),label, NULL);
	gtkblist->vbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook), gtkblist->vbox, NULL);
	gtk_widget_show_all(gtkblist->notebook);


	/****************************** GtkTreeView **********************************/
	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_widget_show(sw);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtkblist->treemodel = gtk_tree_store_new(BLIST_COLUMNS,
						 GDK_TYPE_PIXBUF, /* Status icon */
						 G_TYPE_BOOLEAN,  /* Status icon visible */
						 G_TYPE_STRING,   /* Name */ 
						 G_TYPE_STRING,   /* Idle */
						 G_TYPE_BOOLEAN,  /* Idle visible */
						 GDK_TYPE_PIXBUF, /* Buddy icon */
						 G_TYPE_BOOLEAN,  /* Buddy icon visible */
						 G_TYPE_POINTER,  /* Node */
						 GDK_TYPE_COLOR,  /* bgcolor */
						 G_TYPE_BOOLEAN,  /* Group expander */
						 G_TYPE_BOOLEAN,  /* Contact expander */
						 G_TYPE_BOOLEAN); /* Contact expander visible */

	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));

	gtk_widget_show(gtkblist->treeview);
	gtk_widget_set_name(gtkblist->treeview, "gaim_gtkblist_treeview");
/*	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(gtkblist->treeview), TRUE); */

	/* Set up selection stuff */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(gaim_gtk_blist_selection_changed), NULL);

	/* Set up dnd */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(gtkblist->treeview),
										   GDK_BUTTON1_MASK, ste, 3,
										   GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(gtkblist->treeview),
										 dte, 5,
										 GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-received", G_CALLBACK(gaim_gtk_blist_drag_data_rcv_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-get", G_CALLBACK(gaim_gtk_blist_drag_data_get_cb), NULL);
#ifdef _WIN32
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-begin", G_CALLBACK(gaim_gtk_blist_drag_begin), NULL);
#endif

	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-motion", G_CALLBACK(gaim_gtk_blist_drag_motion_cb), NULL);

	/* Tooltips */
	g_signal_connect(G_OBJECT(gtkblist->treeview), "motion-notify-event", G_CALLBACK(gaim_gtk_blist_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "leave-notify-event", G_CALLBACK(gaim_gtk_blist_leave_cb), NULL);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);
	
	column = gtk_tree_view_column_new();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	gtkblist->text_column = column = gtk_tree_view_column_new ();
	rend = gaim_gtk_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "expander-visible", GROUP_EXPANDER_COLUMN,
#if GTK_CHECK_VERSION(2,6,0)
					    "sensitive", GROUP_EXPANDER_COLUMN,
					    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
					    NULL);

	rend = gaim_gtk_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "expander-visible", CONTACT_EXPANDER_COLUMN,
#if GTK_CHECK_VERSION(2,6,0)
					    "sensitive", CONTACT_EXPANDER_COLUMN,
					    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
					    "visible", CONTACT_EXPANDER_VISIBLE_COLUMN,
					    NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "pixbuf", STATUS_ICON_COLUMN,
					    "visible", STATUS_ICON_VISIBLE_COLUMN,
#if GTK_CHECK_VERSION(2,6,0)
					    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
					    NULL);
	g_object_set(rend, "xalign", 0.0, "ypad", 0, NULL);

	gtkblist->text_rend = rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start (column, rend, TRUE);
	gtk_tree_view_column_set_attributes(column, rend,
#if GTK_CHECK_VERSION(2,6,0)
					    	    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
										"markup", NAME_COLUMN,
										NULL);
	g_signal_connect(G_OBJECT(rend), "edited", G_CALLBACK(gtk_blist_renderer_edited_cb), NULL);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);
#if GTK_CHECK_VERSION(2,6,0)
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	rend = gtk_cell_renderer_text_new();
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend, 
					    "markup", IDLE_COLUMN, 
					    "visible", IDLE_VISIBLE_COLUMN,
#if GTK_CHECK_VERSION(2,6,0)
					    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
					    NULL);
	
	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend, "pixbuf", BUDDY_ICON_COLUMN, 
#if GTK_CHECK_VERSION(2,6,0)
					    "cell-background-gdk", BGCOLOR_COLUMN,
#endif
					    "visible", BUDDY_ICON_VISIBLE_COLUMN,
					    NULL);
	

	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated", G_CALLBACK(gtk_blist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-expanded", G_CALLBACK(gtk_blist_row_expanded_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-collapsed", G_CALLBACK(gtk_blist_row_collapsed_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "key-press-event", G_CALLBACK(gtk_blist_key_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "popup-menu", G_CALLBACK(gaim_gtk_blist_popup_menu_cb), NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(gtkblist->treeview), NAME_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(gtkblist->treeview), _search_func, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sw, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(sw), gtkblist->treeview);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sep, FALSE, FALSE, 0);
	
	/* Create an empty vbox used for showing connection errors */
	gtkblist->error_buttons = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->error_buttons, FALSE, FALSE, 0);

	/* Add the statusbox */
	gtkblist->statusbox = gtk_gaim_status_box_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->statusbox, FALSE, TRUE, 0);
	gtk_widget_set_name(gtkblist->statusbox, "gaim_gtkblist_statusbox");
	gtk_container_set_border_width(GTK_CONTAINER(gtkblist->statusbox), 3);
	gtk_widget_show(gtkblist->statusbox);

	/* set the Show Offline Buddies option. must be done
	 * after the treeview or faceprint gets mad. -Robot101
	 */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show Offline Buddies"))),
			gaim_prefs_get_bool("/gaim/gtk/blist/show_offline_buddies"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show Empty Groups"))),
			gaim_prefs_get_bool("/gaim/gtk/blist/show_empty_groups"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Tools/Mute Sounds"))),
			gaim_prefs_get_bool("/gaim/gtk/sound/mute"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show Buddy Details"))),
			gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show Idle Times"))),
			gaim_prefs_get_bool("/gaim/gtk/blist/show_idle_time"));

	if(!strcmp(gaim_prefs_get_string("/gaim/gtk/sound/method"), "none"))
		gtk_widget_set_sensitive(gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Mute Sounds")), FALSE);

	/* Update some dynamic things */
	update_menu_bar(gtkblist);
	gaim_gtk_blist_update_plugin_actions();
	gaim_gtk_blist_update_sort_methods();

	/* OK... let's show this bad boy. */
	gaim_gtk_blist_refresh(list);
	gaim_gtk_blist_restore_position();
	gtk_widget_show_all(GTK_WIDGET(gtkblist->vbox));
	gtk_widget_realize(GTK_WIDGET(gtkblist->window));
	gaim_blist_set_visible(gaim_prefs_get_bool("/gaim/gtk/blist/list_visible"));

	/* start the refresh timer */
	gtkblist->refresh_timer = g_timeout_add(30000, (GSourceFunc)gaim_gtk_blist_refresh_timer, list);

	handle = gaim_gtk_blist_get_handle();

	/* things that affect how buddies are displayed */
	gaim_prefs_connect_callback(handle, "/gaim/gtk/blist/show_buddy_icons",
			_prefs_change_redo_list, NULL);
	gaim_prefs_connect_callback(handle, "/gaim/gtk/blist/show_idle_time",
			_prefs_change_redo_list, NULL);
	gaim_prefs_connect_callback(handle, "/gaim/gtk/blist/show_empty_groups",
			_prefs_change_redo_list, NULL);
	gaim_prefs_connect_callback(handle, "/gaim/gtk/blist/show_offline_buddies",
			_prefs_change_redo_list, NULL);

	/* sorting */
	gaim_prefs_connect_callback(handle, "/gaim/gtk/blist/sort_type",
			_prefs_change_sort_method, NULL);

	/* menus */
	gaim_prefs_connect_callback(handle, "/gaim/gtk/sound/mute",
			gaim_gtk_blist_mute_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/gaim/gtk/sound/method",
			gaim_gtk_blist_sound_method_pref_cb, NULL);

	/* Setup some gaim signal handlers. */
	gaim_signal_connect(gaim_accounts_get_handle(), "account-enabled",
			gtkblist, GAIM_CALLBACK(account_modified), gtkblist);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-disabled",
			gtkblist, GAIM_CALLBACK(account_modified), gtkblist);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
			gtkblist, GAIM_CALLBACK(account_modified), gtkblist);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-status-changed",
			gtkblist, GAIM_CALLBACK(account_status_changed), gtkblist);

	gaim_signal_connect(gaim_gtk_account_get_handle(), "account-modified",
			gtkblist, GAIM_CALLBACK(account_modified), gtkblist);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						gtkblist, GAIM_CALLBACK(sign_on_off_cb), list);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						gtkblist, GAIM_CALLBACK(sign_on_off_cb), list);

	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-load",
			gtkblist, GAIM_CALLBACK(plugin_changed_cb), NULL);
	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-unload",
			gtkblist, GAIM_CALLBACK(plugin_changed_cb), NULL);

	gaim_signal_connect(gaim_conversations_get_handle(), "conversation-updated",
						gtkblist, GAIM_CALLBACK(conversation_updated_cb),
						gtkblist);
	gaim_signal_connect(gaim_conversations_get_handle(), "deleting-conversation",
						gtkblist, GAIM_CALLBACK(conversation_deleting_cb),
						gtkblist);

	/* emit our created signal */
	gaim_signal_emit(handle, "gtkblist-created", list);
}

static void redo_buddy_list(GaimBuddyList *list, gboolean remove, gboolean rerender)
{
	GaimBlistNode *node = list->root;

	while (node)
	{
		/* This is only needed when we're reverting to a non-GTK+ sorted
		 * status.  We shouldn't need to remove otherwise.
		 */
		if (remove && !GAIM_BLIST_NODE_IS_GROUP(node))
			gaim_gtk_blist_hide_node(list, node, FALSE);

		if (GAIM_BLIST_NODE_IS_BUDDY(node))
			gaim_gtk_blist_update_buddy(list, node, rerender);
		else if (GAIM_BLIST_NODE_IS_CHAT(node))
			gaim_gtk_blist_update(list, node);
		else if (GAIM_BLIST_NODE_IS_GROUP(node))
			gaim_gtk_blist_update(list, node);
		node = gaim_blist_node_next(node, FALSE);
	}

	/* There is no hash table if there is nothing in the buddy list to update */
	if (status_icon_hash_table) {
		g_hash_table_destroy(status_icon_hash_table);
		status_icon_hash_table = NULL;
	}

}

void gaim_gtk_blist_refresh(GaimBuddyList *list)
{
	redo_buddy_list(list, FALSE, TRUE);
}

void
gaim_gtk_blist_update_refresh_timeout()
{
	GaimBuddyList *blist;
	GaimGtkBuddyList *gtkblist;

	blist = gaim_get_blist();
	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	gtkblist->refresh_timer = g_timeout_add(30000,(GSourceFunc)gaim_gtk_blist_refresh_timer, blist);
}

static gboolean get_iter_from_node(GaimBlistNode *node, GtkTreeIter *iter) {
	struct _gaim_gtk_blist_node *gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	GtkTreePath *path;

	if (!gtknode) {
		return FALSE;
	}

	if (!gtkblist) {
		gaim_debug_error("gtkblist", "get_iter_from_node was called, but we don't seem to have a blist\n");
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

static void gaim_gtk_blist_remove(GaimBuddyList *list, GaimBlistNode *node)
{
	struct _gaim_gtk_blist_node *gtknode = node->ui_data;

	gaim_request_close_with_handle(node);

	gaim_gtk_blist_hide_node(list, node, TRUE);

	if(node->parent)
		gaim_gtk_blist_update(list, node->parent);

	/* There's something I don't understand here - Ethan */
	/* Ethan said that back in 2003, but this g_free has been left commented
	 * out ever since. I can't find any reason at all why this is bad and
	 * valgrind found several reasons why it's good. If this causes problems
	 * comment it out again. Stu */
	/* Of course it still causes problems - this breaks dragging buddies into
	 * contacts, the dragged buddy mysteriously 'disappears'. Stu. */
	/* I think it's fixed now. Stu. */

	if(gtknode) {
		if(gtknode->recent_signonoff_timer > 0)
			gaim_timeout_remove(gtknode->recent_signonoff_timer);

		g_free(node->ui_data);
		node->ui_data = NULL;
	}
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

static gboolean insert_node(GaimBuddyList *list, GaimBlistNode *node, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter, cur, *curptr = NULL;
	struct _gaim_gtk_blist_node *gtknode = node->ui_data;
	GtkTreePath *newpath;

	if(!iter)
		return FALSE;

	if(node->parent && !get_iter_from_node(node->parent, &parent_iter))
		return FALSE;

	if(get_iter_from_node(node, &cur))
		curptr = &cur;

	if(GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_CHAT(node)) {
		current_sort_method->func(node, list, parent_iter, curptr, iter);
	} else {
		sort_method_none(node, list, parent_iter, curptr, iter);
	}

	if(gtknode != NULL) {
		gtk_tree_row_reference_free(gtknode->row);
	} else {
		gaim_gtk_blist_new_node(node);
		gtknode = (struct _gaim_gtk_blist_node *)node->ui_data;
	}

	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel),
			iter);
	gtknode->row =
		gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel),
				newpath);

	gtk_tree_path_free(newpath);

	gtk_tree_store_set(gtkblist->treemodel, iter,
			NODE_COLUMN, node,
			-1);

	if(node->parent) {
		GtkTreePath *expand = NULL;
		struct _gaim_gtk_blist_node *gtkparentnode = node->parent->ui_data;

		if(GAIM_BLIST_NODE_IS_GROUP(node->parent)) {
			if(!gaim_blist_node_get_bool(node->parent, "collapsed"))
				expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		} else if(GAIM_BLIST_NODE_IS_CONTACT(node->parent) &&
				gtkparentnode->contact_expanded) {
			expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		}
		if(expand) {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), expand, FALSE);
			gtk_tree_path_free(expand);
		}
	}

	return TRUE;
}

/*This version of gaim_gtk_blist_update_group can take the original buddy
or a group, but has much better algorithmic performance with a pre-known buddy*/
static void gaim_gtk_blist_update_group(GaimBuddyList *list, GaimBlistNode *node)
{
	GaimGroup *group;
	int count;
	gboolean show = FALSE;
	GaimBlistNode* gnode;
	gboolean selected;

	g_return_if_fail(node != NULL);

	if (GAIM_BLIST_NODE_IS_GROUP(node))
		gnode = node;
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		gnode = node->parent->parent;
	else if (GAIM_BLIST_NODE_IS_CONTACT(node) || GAIM_BLIST_NODE_IS_CHAT(node))
		gnode = node->parent;
	else
		return;

	selected = gtkblist ? (gtkblist->selected_node == gnode) : FALSE;
	group = (GaimGroup*)gnode;

	if(gaim_prefs_get_bool("/gaim/gtk/blist/show_offline_buddies"))
		count = gaim_blist_get_group_size(group, FALSE);
	else
		count = gaim_blist_get_group_online_count(group);

	if (count > 0 || gaim_prefs_get_bool("/gaim/gtk/blist/show_empty_groups"))
		show = TRUE;
	else if (GAIM_BLIST_NODE_IS_BUDDY(node)){ /* Or chat? */
		if (buddy_is_displayable((GaimBuddy*)node))
			show = TRUE;}

	if (show) {
		char group_count[12] = "";
		char *mark, *esc;
		GtkTreeIter iter;
		GtkTreePath *path;
		gboolean expanded;
		GdkColor bgcolor;
		GdkColor textcolor;
	
		if(!insert_node(list, gnode, &iter))
			return;

		bgcolor = gtkblist->treeview->style->bg[GTK_STATE_ACTIVE];
		textcolor = gtkblist->treeview->style->fg[GTK_STATE_ACTIVE];

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(gtkblist->treeview), path);
		gtk_tree_path_free(path);

		if (!expanded) {
			g_snprintf(group_count, sizeof(group_count), " (%d/%d)",
			           gaim_blist_get_group_online_count(group),
			           gaim_blist_get_group_size(group, FALSE));
		}

		esc = g_markup_escape_text(group->name, -1);
		if (selected)
			mark = g_strdup_printf("<span weight='bold'>%s</span>%s", esc, group_count);
		else
			mark = g_strdup_printf("<span color='#%02x%02x%02x' weight='bold'>%s</span>%s",
					       textcolor.red>>8, textcolor.green>>8, textcolor.blue>>8,
					       esc, group_count);

		g_free(esc);
		
		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_VISIBLE_COLUMN, FALSE,
				   STATUS_ICON_COLUMN, NULL,
				   NAME_COLUMN, mark,
				   NODE_COLUMN, gnode,
				   BGCOLOR_COLUMN, &bgcolor,
				   GROUP_EXPANDER_COLUMN, TRUE,
				   CONTACT_EXPANDER_VISIBLE_COLUMN, FALSE,
				   BUDDY_ICON_VISIBLE_COLUMN, FALSE,
				   IDLE_VISIBLE_COLUMN, FALSE,
				   -1);
		g_free(mark);
	} else {
		gaim_gtk_blist_hide_node(list, gnode, TRUE);
	}
}

static void buddy_node(GaimBuddy *buddy, GtkTreeIter *iter, GaimBlistNode *node)
{
	GaimPresence *presence;
	GdkPixbuf *status, *avatar;
	char *mark;
	char *idle = NULL;
	gboolean expanded = ((struct _gaim_gtk_blist_node *)(node->parent->ui_data))->contact_expanded;
	gboolean selected = (gtkblist->selected_node == node);
	gboolean biglist = gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons");
	presence = gaim_buddy_get_presence(buddy);

	status = gaim_gtk_blist_get_status_icon((GaimBlistNode*)buddy,
						biglist ? GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL);

	avatar = gaim_gtk_blist_get_buddy_icon((GaimBlistNode *)buddy, TRUE, TRUE, TRUE);
	mark = gaim_gtk_blist_get_name_markup(buddy, selected);

	if (gaim_prefs_get_bool("/gaim/gtk/blist/show_idle_time") &&
		gaim_presence_is_idle(presence) &&
		!gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
	{
		time_t idle_secs = gaim_presence_get_idle_time(presence);

		if (idle_secs > 0)
		{
			time_t t;
			int ihrs, imin;
			time(&t);
			ihrs = (t - idle_secs) / 3600;
			imin = ((t - idle_secs) / 60) % 60;
			idle = g_strdup_printf("%d:%02d", ihrs, imin);
		}
	}

	if (gaim_presence_is_idle(presence))
	{
		if (idle && !selected) {
			char *i2 = g_strdup_printf("<span color='%s'>%s</span>",
						   dim_grey(), idle);
			g_free(idle);
			idle = i2;
		}
	}

	gtk_tree_store_set(gtkblist->treemodel, iter,
			   STATUS_ICON_COLUMN, status,
			   STATUS_ICON_VISIBLE_COLUMN, TRUE,
			   NAME_COLUMN, mark,
			   IDLE_COLUMN, idle,
			   IDLE_VISIBLE_COLUMN, !biglist && idle,
			   BUDDY_ICON_COLUMN, avatar,
			   BUDDY_ICON_VISIBLE_COLUMN, biglist,
			   BGCOLOR_COLUMN, NULL,
			   CONTACT_EXPANDER_COLUMN, NULL,
			   CONTACT_EXPANDER_VISIBLE_COLUMN, expanded,
			-1);

	g_free(mark);
	g_free(idle);
	if(status)
		g_object_unref(status);
	if(avatar)
		g_object_unref(avatar);
}

/* This is a variation on the original gtk_blist_update_contact. Here we
	can know in advance which buddy has changed so we can just update that */
static void gaim_gtk_blist_update_contact(GaimBuddyList *list, GaimBlistNode *node)
{
	GaimBlistNode *cnode;
	GaimContact *contact;
	GaimBuddy *buddy;
	struct _gaim_gtk_blist_node *gtknode;

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		cnode = node->parent;
	else
		cnode = node;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT(cnode));

	/* First things first, update the group */
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		gaim_gtk_blist_update_group(list, node);
	else
		gaim_gtk_blist_update_group(list, cnode->parent);

	contact = (GaimContact*)cnode;
	buddy = gaim_contact_get_priority_buddy(contact);

	if (buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if(!insert_node(list, cnode, &iter))
			return;

		gtknode = (struct _gaim_gtk_blist_node *)cnode->ui_data;

		if(gtknode->contact_expanded) {
			GdkPixbuf *status;
			char *mark;

			status = gaim_gtk_blist_get_status_icon(cnode,
					(gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons") ?
					 GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL));

			mark = g_markup_escape_text(gaim_contact_get_alias(contact), -1);
			gtk_tree_store_set(gtkblist->treemodel, &iter,
					   STATUS_ICON_COLUMN, status,
					   STATUS_ICON_VISIBLE_COLUMN, TRUE,
					   NAME_COLUMN, mark,
					   IDLE_COLUMN, NULL,
					   IDLE_VISIBLE_COLUMN, FALSE,
					   BGCOLOR_COLUMN, NULL,
					   BUDDY_ICON_COLUMN, NULL,
					   CONTACT_EXPANDER_COLUMN, TRUE,
					   CONTACT_EXPANDER_VISIBLE_COLUMN, TRUE,
					-1);
			g_free(mark);
			if(status)
				g_object_unref(status);
		} else {
			buddy_node(buddy, &iter, cnode);
		}
	} else {
		gaim_gtk_blist_hide_node(list, cnode, TRUE);
	}
}



static void gaim_gtk_blist_update_buddy(GaimBuddyList *list, GaimBlistNode *node, gboolean statusChange)
{
	GaimBuddy *buddy;
	struct _gaim_gtk_blist_node *gtkparentnode;
	struct _gaim_gtk_blist_node *gtknode = node->ui_data;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	if (node->parent == NULL)
		return;

	buddy = (GaimBuddy*)node;

	if (statusChange)
		gaim_gtk_blist_update_buddy_status_icon_key(gtknode, buddy,
			(gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons")
				 ? GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL));

	/* First things first, update the contact */
	gaim_gtk_blist_update_contact(list, node);

	gtkparentnode = (struct _gaim_gtk_blist_node *)node->parent->ui_data;

	if (gtkparentnode->contact_expanded && buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if (!insert_node(list, node, &iter))
			return;

		buddy_node(buddy, &iter, node);

	} else {
		gaim_gtk_blist_hide_node(list, node, TRUE);
	}

}

static void gaim_gtk_blist_update_chat(GaimBuddyList *list, GaimBlistNode *node)
{
	GaimChat *chat;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	/* First things first, update the group */
	gaim_gtk_blist_update_group(list, node->parent);

	chat = (GaimChat*)node;

	if(gaim_account_is_connected(chat->account)) {
		GtkTreeIter iter;
		GdkPixbuf *status;
		char *mark;

		if(!insert_node(list, node, &iter))
			return;

		status = gaim_gtk_blist_get_status_icon(node,
				(gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons") ?
				 GAIM_STATUS_ICON_LARGE : GAIM_STATUS_ICON_SMALL));

		mark = g_markup_escape_text(gaim_chat_get_name(chat), -1);

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				STATUS_ICON_COLUMN, status,
				STATUS_ICON_VISIBLE_COLUMN, TRUE,
				NAME_COLUMN, mark,
				-1);

		g_free(mark);
		if(status)
			g_object_unref(status);
	} else {
		gaim_gtk_blist_hide_node(list, node, TRUE);
	}
}

static void gaim_gtk_blist_update(GaimBuddyList *list, GaimBlistNode *node)
{
	if(!gtkblist || !node)
		return;

	if (node->ui_data == NULL)
		gaim_gtk_blist_new_node(node);

	switch(node->type) {
		case GAIM_BLIST_GROUP_NODE:
			gaim_gtk_blist_update_group(list, node);
			break;
		case GAIM_BLIST_CONTACT_NODE:
			gaim_gtk_blist_update_contact(list, node);
			break;
		case GAIM_BLIST_BUDDY_NODE:
			gaim_gtk_blist_update_buddy(list, node, TRUE);
			break;
		case GAIM_BLIST_CHAT_NODE:
			gaim_gtk_blist_update_chat(list, node);
			break;
		case GAIM_BLIST_OTHER_NODE:
			return;
	}

#if !GTK_CHECK_VERSION(2,6,0)
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gtkblist->treeview));
#endif
}


static void gaim_gtk_blist_destroy(GaimBuddyList *list)
{
	if (!gtkblist)
		return;

	gaim_signal_disconnect(gaim_connections_get_handle(), "signed-on",
						   gtkblist, GAIM_CALLBACK(sign_on_off_cb));
	gaim_signal_disconnect(gaim_connections_get_handle(), "signed-off",
						   gtkblist, GAIM_CALLBACK(sign_on_off_cb));

	gtk_widget_destroy(gtkblist->window);

	gaim_gtk_blist_tooltip_destroy();

	if (gtkblist->refresh_timer)
		g_source_remove(gtkblist->refresh_timer);
	if (gtkblist->timeout)
		g_source_remove(gtkblist->timeout);
	if (gtkblist->drag_timeout)
		g_source_remove(gtkblist->drag_timeout);

	g_hash_table_destroy(gtkblist->connection_errors);
	gtkblist->refresh_timer = 0;
	gtkblist->timeout = 0;
	gtkblist->drag_timeout = 0;
	gtkblist->window = gtkblist->vbox = gtkblist->treeview = NULL;
	gtkblist->treemodel = NULL;
	g_object_unref(G_OBJECT(gtkblist->ift));
	g_free(gtkblist);
	accountmenu = NULL;
	gtkblist = NULL;

	gaim_prefs_disconnect_by_handle(gaim_gtk_blist_get_handle());
}

static void gaim_gtk_blist_set_visible(GaimBuddyList *list, gboolean show)
{
	if (!(gtkblist && gtkblist->window))
		return;

	if (show) {
		if(!GAIM_WINDOW_ICONIFIED(gtkblist->window) && !GTK_WIDGET_VISIBLE(gtkblist->window))
			gaim_signal_emit(gaim_gtk_blist_get_handle(), "gtkblist-unhiding", gtkblist);
		gaim_gtk_blist_restore_position();
		gtk_window_present(GTK_WINDOW(gtkblist->window));
	} else {
		if(visibility_manager_count) {
			gaim_signal_emit(gaim_gtk_blist_get_handle(), "gtkblist-hiding", gtkblist);
			gtk_widget_hide(gtkblist->window);
		} else {
			if (!GTK_WIDGET_VISIBLE(gtkblist->window))
				gtk_widget_show(gtkblist->window);
			gtk_window_iconify(GTK_WINDOW(gtkblist->window));
		}
	}
}

static GList *
groups_tree(void)
{
	GList *tmp = NULL;
	char *tmp2;
	GaimGroup *g;
	GaimBlistNode *gnode;

	if (gaim_get_blist()->root == NULL)
	{
		tmp2 = g_strdup(_("Buddies"));
		tmp  = g_list_append(tmp, tmp2);
	}
	else
	{
		for (gnode = gaim_get_blist()->root;
			 gnode != NULL;
			 gnode = gnode->next)
		{
			if (GAIM_BLIST_NODE_IS_GROUP(gnode))
			{
				g    = (GaimGroup *)gnode;
				tmp2 = g->name;
				tmp  = g_list_append(tmp, tmp2);
			}
		}
	}

	return tmp;
}

static void
add_buddy_select_account_cb(GObject *w, GaimAccount *account,
							GaimGtkAddBuddyData *data)
{
	/* Save our account */
	data->account = account;
}

static void
destroy_add_buddy_dialog_cb(GtkWidget *win, GaimGtkAddBuddyData *data)
{
	g_free(data);
}

static void
add_buddy_cb(GtkWidget *w, int resp, GaimGtkAddBuddyData *data)
{
	const char *grp, *who, *whoalias;
	GaimGroup *g;
	GaimBuddy *b;
	GaimConversation *c;
	GaimBuddyIcon *icon;

	if (resp == GTK_RESPONSE_OK)
	{
		who = gtk_entry_get_text(GTK_ENTRY(data->entry));
		grp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(data->combo)->entry));
		whoalias = gtk_entry_get_text(GTK_ENTRY(data->entry_for_alias));
		if (*whoalias == '\0')
			whoalias = NULL;

		if ((g = gaim_find_group(grp)) == NULL)
		{
			g = gaim_group_new(grp);
			gaim_blist_add_group(g, NULL);
		}

		b = gaim_buddy_new(data->account, who, whoalias);
		gaim_blist_add_buddy(b, NULL, g, NULL);
		gaim_account_add_buddy(data->account, b);

		/*
		 * XXX
		 * It really seems like it would be better if the call to
		 * gaim_account_add_buddy() and gaim_conversation_update() were done in
		 * blist.c, possibly in the gaim_blist_add_buddy() function.  Maybe
		 * gaim_account_add_buddy() should be renamed to
		 * gaim_blist_add_new_buddy() or something, and have it call
		 * gaim_blist_add_buddy() after it creates it.  --Mark
		 *
		 * No that's not good.  blist.c should only deal with adding nodes to the
		 * local list.  We need a new, non-gtk file that calls both
		 * gaim_account_add_buddy and gaim_blist_add_buddy().
		 * Or something.  --Mark
		 */

		c = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, who, data->account);
		if (c != NULL) {
			icon = gaim_conv_im_get_icon(GAIM_CONV_IM(c));
			if (icon != NULL)
				gaim_buddy_icon_update(icon);
		}
	}

	gtk_widget_destroy(data->window);
}

static void
gaim_gtk_blist_request_add_buddy(GaimAccount *account, const char *username,
								 const char *group, const char *alias)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *img;
	GaimGtkBuddyList *gtkblist;
	GaimGtkAddBuddyData *data = g_new0(GaimGtkAddBuddyData, 1);

	data->account =
		(account != NULL
		 ? account
		 : gaim_connection_get_account(gaim_connections_get_all()->data));

	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	data->window = gtk_dialog_new_with_buttons(_("Add Buddy"),
			NULL, GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_ADD, GTK_RESPONSE_OK,
			NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), GAIM_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BOX_SPACE);
	gtk_window_set_role(GTK_WINDOW(data->window), "add_buddy");
	gtk_window_set_type_hint(GTK_WINDOW(data->window),
							 GDK_WINDOW_TYPE_HINT_DIALOG);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(
		_("Please enter the screen name of the person you would like "
		  "to add to your buddy list. You may optionally enter an alias, "
		  "or nickname,  for the buddy. The alias will be displayed in "
		  "place of the screen name whenever possible.\n"));

	gtk_widget_set_size_request(GTK_WIDGET(label), 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	g_signal_connect(G_OBJECT(data->window), "destroy",
					 G_CALLBACK(destroy_add_buddy_dialog_cb), data);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 0);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	label = gtk_label_new(_("Screen name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

	data->entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), data->entry, 1, 2, 0, 1);
	gtk_widget_grab_focus(data->entry);

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry), username);
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(data->window),
										  GTK_RESPONSE_OK, FALSE);

	gtk_entry_set_activates_default (GTK_ENTRY(data->entry), TRUE);
	gaim_set_accessible_label (data->entry, label);

	g_signal_connect(G_OBJECT(data->entry), "changed",
					 G_CALLBACK(gaim_gtk_set_sensitive_if_input),
					 data->window);

	label = gtk_label_new(_("Alias:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

	data->entry_for_alias = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table),
							  data->entry_for_alias, 1, 2, 1, 2);

	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry_for_alias), alias);

	if (username != NULL)
		gtk_widget_grab_focus(GTK_WIDGET(data->entry_for_alias));

	gtk_entry_set_activates_default (GTK_ENTRY(data->entry_for_alias), TRUE);
	gaim_set_accessible_label (data->entry_for_alias, label);

	label = gtk_label_new(_("Group:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

	data->combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(data->combo), groups_tree());
	gtk_table_attach_defaults(GTK_TABLE(table), data->combo, 1, 2, 2, 3);
	gaim_set_accessible_label (data->combo, label);

	/* Set up stuff for the account box */
	label = gtk_label_new(_("Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

	data->account_box = gaim_gtk_account_option_menu_new(account, FALSE,
			G_CALLBACK(add_buddy_select_account_cb), NULL, data);

	gtk_table_attach_defaults(GTK_TABLE(table), data->account_box, 1, 2, 3, 4);
	gaim_set_accessible_label (data->account_box, label);
	/* End of account box */

	g_signal_connect(G_OBJECT(data->window), "response",
					 G_CALLBACK(add_buddy_cb), data);

	gtk_widget_show_all(data->window);

	if (group != NULL)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(data->combo)->entry), group);
}

static void
add_chat_cb(GtkWidget *w, GaimGtkAddChatData *data)
{
	GHashTable *components;
	GList *tmp;
	GaimChat *chat;
	GaimGroup *group;
	const char *group_name;
	const char *value;

	components = g_hash_table_new_full(g_str_hash, g_str_equal,
									   g_free, g_free);

	for (tmp = data->entries; tmp; tmp = tmp->next)
	{
		if (g_object_get_data(tmp->data, "is_spin"))
		{
			g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
						gtk_spin_button_get_value_as_int(tmp->data)));
		}
		else
		{
			value = gtk_entry_get_text(tmp->data);
			if (*value != '\0')
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup(value));
		}
	}

	chat = gaim_chat_new(data->account,
							   gtk_entry_get_text(GTK_ENTRY(data->alias_entry)),
							   components);

	group_name = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(data->group_combo)->entry));

	if ((group = gaim_find_group(group_name)) == NULL)
	{
		group = gaim_group_new(group_name);
		gaim_blist_add_group(group, NULL);
	}

	if (chat != NULL)
	{
		gaim_blist_add_chat(chat, group, NULL);
	}

	gtk_widget_destroy(data->window);
	g_free(data->default_chat_name);
	g_list_free(data->entries);
	g_free(data);
}

static void
add_chat_resp_cb(GtkWidget *w, int resp, GaimGtkAddChatData *data)
{
	if (resp == GTK_RESPONSE_OK)
	{
		add_chat_cb(NULL, data);
	}
	else
	{
		gtk_widget_destroy(data->window);
		g_free(data->default_chat_name);
		g_list_free(data->entries);
		g_free(data);
	}
}

/*
 * Check the values of all the text entry boxes.  If any required input
 * strings are empty then don't allow the user to click on "OK."
 */
static void
addchat_set_sensitive_if_input_cb(GtkWidget *entry, gpointer user_data)
{
	GaimGtkAddChatData *data;
	GList *tmp;
	const char *text;
	gboolean required;
	gboolean sensitive = TRUE;

	data = user_data;

	for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
	{
		if (!g_object_get_data(tmp->data, "is_spin"))
		{
			required = GPOINTER_TO_INT(g_object_get_data(tmp->data, "required"));
			text = gtk_entry_get_text(tmp->data);
			if (required && (*text == '\0'))
				sensitive = FALSE;
		}
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->window), GTK_RESPONSE_OK, sensitive);
}

static void
rebuild_addchat_entries(GaimGtkAddChatData *data)
{
	GaimConnection *gc;
	GList *list = NULL, *tmp;
	GHashTable *defaults = NULL;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	g_return_if_fail(data->account != NULL);

	gc = gaim_account_get_connection(data->account);

	while ((tmp = gtk_container_get_children(GTK_CONTAINER(data->entries_box))))
		gtk_widget_destroy(tmp->data);

	g_list_free(data->entries);

	data->entries = NULL;

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
		list = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info(gc);

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
		defaults = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, data->default_chat_name);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *label;
		GtkWidget *rowbox;
		GtkWidget *input;

		pce = tmp->data;

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(data->entries_box), rowbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(pce->label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(data->sg, label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		if (pce->is_int)
		{
			GtkObject *adjust;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max,
										1, 10, 10);
			input = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			gtk_widget_set_size_request(input, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), input, FALSE, FALSE, 0);
		}
		else
		{
			char *value;
			input = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);
			value = g_hash_table_lookup(defaults, pce->identifier);
			if (value != NULL)
				gtk_entry_set_text(GTK_ENTRY(input), value);
			if (pce->secret)
			{
				gtk_entry_set_visibility(GTK_ENTRY(input), FALSE);
				gtk_entry_set_invisible_char(GTK_ENTRY(input), GAIM_INVISIBLE_CHAR);
			}
			gtk_box_pack_end(GTK_BOX(rowbox), input, TRUE, TRUE, 0);
			g_signal_connect(G_OBJECT(input), "changed",
							 G_CALLBACK(addchat_set_sensitive_if_input_cb), data);
		}

		/* Do the following for any type of input widget */
		if (focus)
		{
			gtk_widget_grab_focus(input);
			focus = FALSE;
		}
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), input);
		gaim_set_accessible_label(input, label);
		g_object_set_data(G_OBJECT(input), "identifier", (gpointer)pce->identifier);
		g_object_set_data(G_OBJECT(input), "is_spin", GINT_TO_POINTER(pce->is_int));
		g_object_set_data(G_OBJECT(input), "required", GINT_TO_POINTER(pce->required));
		data->entries = g_list_append(data->entries, input);

		g_free(pce);
	}

	g_list_free(list);
	g_hash_table_destroy(defaults);

	/* Set whether the "OK" button should be clickable initially */
	addchat_set_sensitive_if_input_cb(NULL, data);

	gtk_widget_show_all(data->entries_box);
}

static void
addchat_select_account_cb(GObject *w, GaimAccount *account,
						   GaimGtkAddChatData *data)
{
	if (strcmp(gaim_account_get_protocol_id(data->account),
		gaim_account_get_protocol_id(account)) == 0)
	{
		data->account = account;
	}
	else
	{
		data->account = account;
		rebuild_addchat_entries(data);
	}
}

static void
gaim_gtk_blist_request_add_chat(GaimAccount *account, GaimGroup *group,
								const char *alias, const char *name)
{
	GaimGtkAddChatData *data;
	GaimGtkBuddyList *gtkblist;
	GList *l;
	GaimConnection *gc;
	GtkWidget *label;
	GtkWidget *rowbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *img;

	if (account != NULL) {
		gc = gaim_account_get_connection(account);

		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat == NULL) {
			gaim_notify_error(gc, NULL, _("This protocol does not support chat rooms."), NULL);
			return;
		}
	} else {
		/* Find an account with chat capabilities */
		for (l = gaim_connections_get_all(); l != NULL; l = l->next) {
			gc = (GaimConnection *)l->data;

			if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat != NULL) {
				account = gaim_connection_get_account(gc);
				break;
			}
		}

		if (account == NULL) {
			gaim_notify_error(NULL, NULL,
							  _("You are not currently signed on with any "
								"protocols that have the ability to chat."), NULL);
			return;
		}
	}

	data = g_new0(GaimGtkAddChatData, 1);
	data->account = account;
	data->default_chat_name = g_strdup(name);

	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	data->window = gtk_dialog_new_with_buttons(_("Add Chat"),
		NULL, GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_ADD, GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), GAIM_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), GAIM_HIG_BOX_SPACE);
	gtk_window_set_role(GTK_WINDOW(data->window), "add_chat");
	gtk_window_set_type_hint(GTK_WINDOW(data->window),
							 GDK_WINDOW_TYPE_HINT_DIALOG);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(
		_("Please enter an alias, and the appropriate information "
		  "about the chat you would like to add to your buddy list.\n"));
	gtk_widget_set_size_request(label, 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(data->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

	data->account_menu = gaim_gtk_account_option_menu_new(account, FALSE,
			G_CALLBACK(addchat_select_account_cb),
			chat_account_filter_func, data);
	gtk_box_pack_start(GTK_BOX(rowbox), data->account_menu, TRUE, TRUE, 0);
	gaim_set_accessible_label (data->account_menu, label);

	data->entries_box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(data->entries_box), 0);
	gtk_box_pack_start(GTK_BOX(vbox), data->entries_box, TRUE, TRUE, 0);

	rebuild_addchat_entries(data);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Alias:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(data->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

	data->alias_entry = gtk_entry_new();
	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->alias_entry), alias);
	gtk_box_pack_end(GTK_BOX(rowbox), data->alias_entry, TRUE, TRUE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(data->alias_entry), TRUE);
	gaim_set_accessible_label (data->alias_entry, label);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), rowbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Group:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(data->sg, label);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

	data->group_combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(data->group_combo), groups_tree());
	gtk_box_pack_end(GTK_BOX(rowbox), data->group_combo, TRUE, TRUE, 0);

	if (group)
	{
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(data->group_combo)->entry),
						   group->name);
	}
	gaim_set_accessible_label (data->group_combo, label);

	g_signal_connect(G_OBJECT(data->window), "response",
					 G_CALLBACK(add_chat_resp_cb), data);

	gtk_widget_show_all(data->window);
}

static void
add_group_cb(GaimConnection *gc, const char *group_name)
{
	GaimGroup *group;

	if ((group_name == NULL) || (*group_name == '\0'))
		return;

	group = gaim_group_new(group_name);
	gaim_blist_add_group(group, NULL);
}

static void
gaim_gtk_blist_request_add_group(void)
{
	gaim_request_input(NULL, _("Add Group"), NULL,
					   _("Please enter the name of the group to be added."),
					   NULL, FALSE, FALSE, NULL,
					   _("Add"), G_CALLBACK(add_group_cb),
					   _("Cancel"), NULL, NULL);
}

void
gaim_gtk_blist_toggle_visibility()
{
	if (gtkblist && gtkblist->window) {
		if (GTK_WIDGET_VISIBLE(gtkblist->window)) {
			gaim_blist_set_visible(GAIM_WINDOW_ICONIFIED(gtkblist->window) || gtk_blist_obscured);
		} else {
			gaim_blist_set_visible(TRUE);
		}
	}
}

void
gaim_gtk_blist_visibility_manager_add()
{
	visibility_manager_count++;
	gaim_debug_info("gtkblist", "added visibility manager: %d\n", visibility_manager_count);
}

void
gaim_gtk_blist_visibility_manager_remove()
{
	if (visibility_manager_count)
		visibility_manager_count--;
	if (!visibility_manager_count)
		gaim_blist_set_visible(TRUE);
	gaim_debug_info("gtkblist", "removed visibility manager: %d\n", visibility_manager_count);
}


static GaimBlistUiOps blist_ui_ops =
{
	gaim_gtk_blist_new_list,
	gaim_gtk_blist_new_node,
	gaim_gtk_blist_show,
	gaim_gtk_blist_update,
	gaim_gtk_blist_remove,
	gaim_gtk_blist_destroy,
	gaim_gtk_blist_set_visible,
	gaim_gtk_blist_request_add_buddy,
	gaim_gtk_blist_request_add_chat,
	gaim_gtk_blist_request_add_group
};


GaimBlistUiOps *
gaim_gtk_blist_get_ui_ops(void)
{
	return &blist_ui_ops;
}

GaimGtkBuddyList *gaim_gtk_blist_get_default_gtk_blist()
{
        return gtkblist;
}

static void account_signon_cb(GaimConnection *gc, gpointer z)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBlistNode *gnode, *cnode;
	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next)
	{
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next)
		{
			GaimChat *chat;

			if(!GAIM_BLIST_NODE_IS_CHAT(cnode))
				continue;

			chat = (GaimChat *)cnode;

			if(chat->account != account)
				continue;

			if(gaim_blist_node_get_bool((GaimBlistNode*)chat, "gtk-autojoin") ||
					(gaim_blist_node_get_string((GaimBlistNode*)chat,
					 "gtk-autojoin") != NULL))
				serv_join_chat(gc, chat->components);
		}
	}
}

void *
gaim_gtk_blist_get_handle() {
	static int handle;

	return &handle;
}

static gboolean buddy_signonoff_timeout_cb(GaimBuddy *buddy)
{
	struct _gaim_gtk_blist_node *gtknode = ((GaimBlistNode*)buddy)->ui_data;

	gtknode->recent_signonoff = FALSE;
	gtknode->recent_signonoff_timer = 0;

	gaim_gtk_blist_update(NULL, (GaimBlistNode*)buddy);

	return FALSE;
}

static void buddy_signonoff_cb(GaimBuddy *buddy)
{
	struct _gaim_gtk_blist_node *gtknode;

	if(!((GaimBlistNode*)buddy)->ui_data) {
		gaim_gtk_blist_new_node((GaimBlistNode*)buddy);
	}

	gtknode = ((GaimBlistNode*)buddy)->ui_data;

	gtknode->recent_signonoff = TRUE;

	if(gtknode->recent_signonoff_timer > 0)
		gaim_timeout_remove(gtknode->recent_signonoff_timer);
	gtknode->recent_signonoff_timer = gaim_timeout_add(10000,
			(GSourceFunc)buddy_signonoff_timeout_cb, buddy);
}

void gaim_gtk_blist_init(void)
{
	void *gtk_blist_handle = gaim_gtk_blist_get_handle();

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						gtk_blist_handle, GAIM_CALLBACK(account_signon_cb),
						NULL);

	/* Initialize prefs */
	gaim_prefs_add_none("/gaim/gtk/blist");
	gaim_prefs_add_bool("/gaim/gtk/blist/show_buddy_icons", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/blist/show_empty_groups", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/blist/show_idle_time", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/blist/show_offline_buddies", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/blist/list_visible", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/blist/list_maximized", FALSE);
	gaim_prefs_add_string("/gaim/gtk/blist/sort_type", "alphabetical");
	gaim_prefs_add_int("/gaim/gtk/blist/x", 0);
	gaim_prefs_add_int("/gaim/gtk/blist/y", 0);
	gaim_prefs_add_int("/gaim/gtk/blist/width", 250); /* Golden ratio, baby */
	gaim_prefs_add_int("/gaim/gtk/blist/height", 405); /* Golden ratio, baby */
	gaim_prefs_add_int("/gaim/gtk/blist/tooltip_delay", 500);

	/* Register our signals */
	gaim_signal_register(gtk_blist_handle, "gtkblist-hiding",
	                     gaim_marshal_VOID__POINTER, NULL, 1,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST));

	gaim_signal_register(gtk_blist_handle, "gtkblist-unhiding",
	                     gaim_marshal_VOID__POINTER, NULL, 1,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST));

	gaim_signal_register(gtk_blist_handle, "gtkblist-created",
	                     gaim_marshal_VOID__POINTER, NULL, 1,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST));

	gaim_signal_register(gtk_blist_handle, "drawing-tooltip",
	                     gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST_NODE),
	                     gaim_value_new_outgoing(GAIM_TYPE_BOXED, "GString *"),
	                     gaim_value_new(GAIM_TYPE_BOOLEAN));


	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on", gtk_blist_handle, GAIM_CALLBACK(buddy_signonoff_cb), NULL);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-off", gtk_blist_handle, GAIM_CALLBACK(buddy_signonoff_cb), NULL);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-privacy-changed", gtk_blist_handle, GAIM_CALLBACK(gaim_gtk_blist_update_privacy_cb), NULL);
}

void
gaim_gtk_blist_uninit(void) {
	gaim_signals_unregister_by_instance(gaim_gtk_blist_get_handle());
}

/*********************************************************************
 * Buddy List sorting functions                                      *
 *********************************************************************/

GList *gaim_gtk_blist_get_sort_methods()
{
	return gaim_gtk_blist_sort_methods;
}

void gaim_gtk_blist_sort_method_reg(const char *id, const char *name, gaim_gtk_blist_sort_function func)
{
	struct gaim_gtk_blist_sort_method *method = g_new0(struct gaim_gtk_blist_sort_method, 1);
	method->id = g_strdup(id);
	method->name = g_strdup(name);
	method->func = func;
	gaim_gtk_blist_sort_methods = g_list_append(gaim_gtk_blist_sort_methods, method);
	gaim_gtk_blist_update_sort_methods();
}

void gaim_gtk_blist_sort_method_unreg(const char *id){
	GList *l = gaim_gtk_blist_sort_methods;

	while(l) {
		struct gaim_gtk_blist_sort_method *method = l->data;
		if(!strcmp(method->id, id)) {
			gaim_gtk_blist_sort_methods = g_list_delete_link(gaim_gtk_blist_sort_methods, l);
			g_free(method->id);
			g_free(method->name);
			g_free(method);
			break;
		}
	}
	gaim_gtk_blist_update_sort_methods();
}

void gaim_gtk_blist_sort_method_set(const char *id){
	GList *l = gaim_gtk_blist_sort_methods;

	if(!id)
		id = "none";

	while (l && strcmp(((struct gaim_gtk_blist_sort_method*)l->data)->id, id))
		l = l->next;

	if (l) {
		current_sort_method = l->data;
	} else if (!current_sort_method) {
		gaim_gtk_blist_sort_method_set("none");
		return;
	}
	if (!strcmp(id, "none")) {
		redo_buddy_list(gaim_get_blist(), TRUE, FALSE);
	} else {
		redo_buddy_list(gaim_get_blist(), FALSE, FALSE);
	}
}

/******************************************
 ** Sort Methods
 ******************************************/

static void sort_method_none(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter parent_iter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GaimBlistNode *sibling = node->prev;
	GtkTreeIter sibling_iter;

	if (cur != NULL) {
		*iter = *cur;
		return;
	}

	while (sibling && !get_iter_from_node(sibling, &sibling_iter)) {
		sibling = sibling->prev;
	}

	gtk_tree_store_insert_after(gtkblist->treemodel, iter,
			node->parent ? &parent_iter : NULL,
			sibling ? &sibling_iter : NULL);
}

#if GTK_CHECK_VERSION(2,2,1)

static void sort_method_alphabetical(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	const char *my_name;

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		my_name = gaim_contact_get_alias((GaimContact*)node);
	} else if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		my_name = gaim_chat_get_name((GaimChat*)node);
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		GValue val;
		GaimBlistNode *n;
		const char *this_name;
		int cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);

		if(GAIM_BLIST_NODE_IS_CONTACT(n)) {
			this_name = gaim_contact_get_alias((GaimContact*)n);
		} else if(GAIM_BLIST_NODE_IS_CHAT(n)) {
			this_name = gaim_chat_get_name((GaimChat*)n);
		} else {
			this_name = NULL;
		}

		cmp = gaim_utf8_strcasecmp(my_name, this_name);

		if(this_name && (cmp < 0 || (cmp == 0 && node < n))) {
			if(cur) {
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			} else {
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
						&groupiter, &more_z);
				return;
			}
		}
		g_value_unset(&val);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	if(cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

static void sort_method_status(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	GaimBuddy *my_buddy, *this_buddy;

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		my_buddy = gaim_contact_get_priority_buddy((GaimContact*)node);
	} else if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		if (cur != NULL) {
			*iter = *cur;
			return;
		}

		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		GValue val;
		GaimBlistNode *n;
		gint name_cmp;
		gint presence_cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);

		if(GAIM_BLIST_NODE_IS_CONTACT(n)) {
			this_buddy = gaim_contact_get_priority_buddy((GaimContact*)n);
		} else {
			this_buddy = NULL;
		}

		name_cmp = gaim_utf8_strcasecmp(
			gaim_contact_get_alias(gaim_buddy_get_contact(my_buddy)),
			(this_buddy
			 ? gaim_contact_get_alias(gaim_buddy_get_contact(this_buddy))
			 : NULL));

		presence_cmp = gaim_presence_compare(
			gaim_buddy_get_presence(my_buddy),
			this_buddy ? gaim_buddy_get_presence(this_buddy) : NULL);

		if (this_buddy == NULL ||
			(presence_cmp < 0 ||
			 (presence_cmp == 0 &&
			  (name_cmp < 0 || (name_cmp == 0 && node < n)))))
		{
			if (cur != NULL)
			{
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			}
			else
			{
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
											 &groupiter, &more_z);
				return;
			}
		}

		g_value_unset(&val);
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gtkblist->treemodel),
									&more_z));

	if (cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

static void sort_method_log(GaimBlistNode *node, GaimBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	int log_size = 0, this_log_size = 0;
	const char *buddy_name, *this_buddy_name;

	if(cur && (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter) == 1)) {
		*iter = *cur;
		return;
	}

	if(GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimBlistNode *n;
		for (n = node->child; n; n = n->next)
			log_size += gaim_log_get_total_size(GAIM_LOG_IM, ((GaimBuddy*)(n))->name, ((GaimBuddy*)(n))->account);
		buddy_name = gaim_contact_get_alias((GaimContact*)node);
	} else if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		/* we don't have a reliable way of getting the log filename
		 * from the chat info in the blist, yet */
		if (cur != NULL) {
			*iter = *cur;
			return;
		}

		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		GValue val;
		GaimBlistNode *n;
		GaimBlistNode *n2;
		int cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);
		this_log_size = 0;

		if(GAIM_BLIST_NODE_IS_CONTACT(n)) {
			for (n2 = n->child; n2; n2 = n2->next)
				this_log_size += gaim_log_get_total_size(GAIM_LOG_IM, ((GaimBuddy*)(n2))->name, ((GaimBuddy*)(n2))->account);
			this_buddy_name = gaim_contact_get_alias((GaimContact*)n);
		} else {
			this_buddy_name = NULL;
		}

		cmp = gaim_utf8_strcasecmp(buddy_name, this_buddy_name);

		if (!GAIM_BLIST_NODE_IS_CONTACT(n) || log_size > this_log_size ||
				((log_size == this_log_size) &&
				 (cmp < 0 || (cmp == 0 && node < n)))) {
			if (cur != NULL) {
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			} else {
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
						&groupiter, &more_z);
				return;
			}
		}
		g_value_unset(&val);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	if (cur != NULL) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

#endif

static void
plugin_act(GtkObject *obj, GaimPluginAction *pam)
{
	if (pam && pam->callback)
		pam->callback(pam);
}

static void
build_plugin_actions(GtkWidget *menu, GaimPlugin *plugin)
{
	GtkWidget *menuitem;
	GaimPluginAction *action = NULL;
	GList *actions, *l;

	actions = GAIM_PLUGIN_ACTIONS(plugin, NULL);

	for (l = actions; l != NULL; l = l->next)
	{
		if (l->data)
		{
			action = (GaimPluginAction *) l->data;
			action->plugin = plugin;
			action->context = NULL;

			menuitem = gtk_menu_item_new_with_label(action->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(plugin_act), action);
			g_object_set_data_full(G_OBJECT(menuitem), "plugin_action",
								   action,
								   (GDestroyNotify)gaim_plugin_action_free);
			gtk_widget_show(menuitem);
		}
		else
			gaim_separator(menu);
	}

	g_list_free(actions);
}

static void
modify_account_cb(GtkWidget *widget, gpointer data)
{
	gaim_gtk_account_dialog_show(GAIM_GTK_MODIFY_ACCOUNT_DIALOG, data);
}

static void
enable_account_cb(GtkCheckMenuItem *widget, gpointer data)
{
	GaimAccount *account = data;
	const GaimSavedStatus *saved_status;

	saved_status = gaim_savedstatus_get_current();
	gaim_savedstatus_activate_for_account(saved_status, account);

	gaim_account_set_enabled(account, GAIM_GTK_UI, TRUE);
}

static void
disable_account_cb(GtkCheckMenuItem *widget, gpointer data)
{
	GaimAccount *account = data;

	gaim_account_set_enabled(account, GAIM_GTK_UI, FALSE);
}

void
gaim_gtk_blist_update_accounts_menu(void)
{
	GtkWidget *menuitem = NULL, *submenu = NULL;
	GtkAccelGroup *accel_group = NULL;
	GList *l = NULL, *accounts = NULL;
	gboolean disabled_accounts = FALSE;

	if (accountmenu == NULL)
		return;

	/* Clear the old Accounts menu */
	for (l = gtk_container_get_children(GTK_CONTAINER(accountmenu)); l; l = l->next) {
		menuitem = l->data;

		if (menuitem != gtk_item_factory_get_widget(gtkblist->ift, N_("/Accounts/Add\\/Edit")))
			gtk_widget_destroy(menuitem);
	}

	for (accounts = gaim_accounts_get_all(); accounts; accounts = accounts->next) {
		char *buf = NULL;
		char *accel_path_buf = NULL;
		GtkWidget *image = NULL;
		GaimConnection *gc = NULL;
		GaimAccount *account = NULL;
		GaimStatus *status = NULL;
		GdkPixbuf *pixbuf = NULL;

		account = accounts->data;
		accel_group = gtk_menu_get_accel_group(GTK_MENU(accountmenu));

		if(gaim_account_get_enabled(account, GAIM_GTK_UI)) {
			buf = g_strconcat(gaim_account_get_username(account), " (",
					gaim_account_get_protocol_name(account), ")", NULL);
			menuitem = gtk_image_menu_item_new_with_label(buf);
			accel_path_buf = g_strconcat(N_("<GaimMain>/Accounts/"), buf, NULL);
			g_free(buf);
			status = gaim_account_get_active_status(account);
			pixbuf = gaim_gtk_create_prpl_icon_with_status(account, gaim_status_get_type(status), 0.5);
			if (pixbuf != NULL)
			{
				if (!gaim_account_is_connected(account))
					gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf,
							0.0, FALSE);
				image = gtk_image_new_from_pixbuf(pixbuf);
				g_object_unref(G_OBJECT(pixbuf));
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
			}
			gtk_menu_shell_append(GTK_MENU_SHELL(accountmenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
			gtk_menu_set_accel_path(GTK_MENU(submenu), accel_path_buf);
			g_free(accel_path_buf);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);


			menuitem = gtk_menu_item_new_with_mnemonic(_("_Edit Account"));
			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(modify_account_cb), account);
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
			gtk_widget_show(menuitem);

			gaim_separator(submenu);

			gc = gaim_account_get_connection(account);
			if (gc && GAIM_CONNECTION_IS_CONNECTED(gc)) {
				GaimPlugin *plugin = NULL;

				plugin = gc->prpl;
				if (GAIM_PLUGIN_HAS_ACTIONS(plugin)) {
					GList *l, *ll = NULL;
					GaimPluginAction *action = NULL;

					for (l = ll = GAIM_PLUGIN_ACTIONS(plugin, gc); l; l = l->next) {
						if (l->data) {
							action = (GaimPluginAction *)l->data;
							action->plugin = plugin;
							action->context = gc;

							menuitem = gtk_menu_item_new_with_label(action->label);
							gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
							g_signal_connect(G_OBJECT(menuitem), "activate",
									G_CALLBACK(plugin_act), action);
							g_object_set_data_full(G_OBJECT(menuitem), "plugin_action", action, (GDestroyNotify)gaim_plugin_action_free);
							gtk_widget_show(menuitem);
						} else
							gaim_separator(submenu);
					}
				} else {
					menuitem = gtk_menu_item_new_with_label(_("No actions available"));
					gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
					gtk_widget_set_sensitive(menuitem, FALSE);
					gtk_widget_show(menuitem);
				}
			} else {
				menuitem = gtk_menu_item_new_with_label(_("No actions available"));
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
				gtk_widget_set_sensitive(menuitem, FALSE);
				gtk_widget_show(menuitem);
			}

			gaim_separator(submenu);

			menuitem = gtk_menu_item_new_with_mnemonic(_("_Disable"));
			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(disable_account_cb), account);
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
			gtk_widget_show(menuitem);
		} else {
			disabled_accounts = TRUE;
		}
	}

	if(disabled_accounts) {
		gaim_separator(accountmenu);
		menuitem = gtk_menu_item_new_with_label(_("Enable Account"));
		gtk_menu_shell_append(GTK_MENU_SHELL(accountmenu), menuitem);
		gtk_widget_show(menuitem);

		submenu = gtk_menu_new();
		gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
		gtk_menu_set_accel_path(GTK_MENU(submenu), N_("<GaimMain>/Accounts/Enable Account"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		gtk_widget_show(submenu);

		for (accounts = gaim_accounts_get_all(); accounts; accounts = accounts->next) {
			char *buf = NULL;
			GtkWidget *image = NULL;
			GaimAccount *account = NULL;
			GdkPixbuf *pixbuf = NULL;

			account = accounts->data;

			if(!gaim_account_get_enabled(account, GAIM_GTK_UI)) {

				disabled_accounts = TRUE;

				buf = g_strconcat(gaim_account_get_username(account), " (",
						gaim_account_get_protocol_name(account), ")", NULL);
				menuitem = gtk_image_menu_item_new_with_label(buf);
				g_free(buf);
				pixbuf = gaim_gtk_create_prpl_icon(account, 0.5);
				if (pixbuf != NULL)
				{
					if (!gaim_account_is_connected(account))
						gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);
					image = gtk_image_new_from_pixbuf(pixbuf);
					g_object_unref(G_OBJECT(pixbuf));
					gtk_widget_show(image);
					gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
				}
				g_signal_connect(G_OBJECT(menuitem), "activate",
						G_CALLBACK(enable_account_cb), account);
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
				gtk_widget_show(menuitem);
			}
		}
	}
}

static GList *plugin_submenus = NULL;

void
gaim_gtk_blist_update_plugin_actions(void)
{
	GtkWidget *menuitem, *submenu;
	GaimPlugin *plugin = NULL;
	GList *l;
	GtkAccelGroup *accel_group;

	GtkWidget *pluginmenu = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools"));

	g_return_if_fail(pluginmenu != NULL);

	/* Remove old plugin action submenus from the Tools menu */
	for (l = plugin_submenus; l; l = l->next)
		gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(plugin_submenus);
	plugin_submenus = NULL;

	accel_group = gtk_menu_get_accel_group(GTK_MENU(pluginmenu));

	/* Add a submenu for each plugin with custom actions */
	for (l = gaim_plugins_get_loaded(); l; l = l->next) {
		char *path;

		plugin = (GaimPlugin *) l->data;

		if (GAIM_IS_PROTOCOL_PLUGIN(plugin))
			continue;

		if (!GAIM_PLUGIN_HAS_ACTIONS(plugin))
			continue;

		menuitem = gtk_image_menu_item_new_with_label(_(plugin->info->name));
		gtk_menu_shell_append(GTK_MENU_SHELL(pluginmenu), menuitem);
		gtk_widget_show(menuitem);

		plugin_submenus = g_list_append(plugin_submenus, menuitem);

		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		gtk_widget_show(submenu);

		gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
		path = g_strdup_printf("%s/Tools/%s", gtkblist->ift->path, plugin->info->name);
		gtk_menu_set_accel_path(GTK_MENU(submenu), path);
		g_free(path);

		build_plugin_actions(submenu, plugin);
	}
}

static void
sortmethod_act(GtkCheckMenuItem *checkmenuitem, char *id)
{
	if (gtk_check_menu_item_get_active(checkmenuitem))
	{
		gaim_gtk_set_cursor(gtkblist->window, GDK_WATCH);
		/* This is redundant. I think. */
		/* gaim_gtk_blist_sort_method_set(id); */
		gaim_prefs_set_string("/gaim/gtk/blist/sort_type", id);

		gaim_gtk_clear_cursor(gtkblist->window);
	}
}

void
gaim_gtk_blist_update_sort_methods(void)
{
	GtkWidget *menuitem = NULL, *activeitem = NULL;
	GaimGtkBlistSortMethod *method = NULL;
	GList *l;
	GSList *sl = NULL;
	GtkWidget *sortmenu;
	const char *m = gaim_prefs_get_string("/gaim/gtk/blist/sort_type");

	if (gtkblist == NULL)
		return;

	sortmenu = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Sort Buddies"));

	if (sortmenu == NULL)
		return;

	/* Clear the old menu */
	for (l = gtk_container_get_children(GTK_CONTAINER(sortmenu)); l; l = l->next) {
		menuitem = l->data;
		gtk_widget_destroy(GTK_WIDGET(menuitem));
	}

	for (l = gaim_gtk_blist_sort_methods; l; l = l->next) {
		method = (GaimGtkBlistSortMethod *) l->data;
		menuitem = gtk_radio_menu_item_new_with_label(sl, _(method->name));
		if (!strcmp(m, method->id))
			activeitem = menuitem;
		sl = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
		gtk_menu_shell_append(GTK_MENU_SHELL(sortmenu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "toggled",
				 G_CALLBACK(sortmethod_act), method->id);
		gtk_widget_show(menuitem);
	}
	if (activeitem)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(activeitem), TRUE);
}
