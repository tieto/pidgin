/*
 * @file gtkblist.c GTK+ BuddyList API
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
 *
 */
#include "internal.h"
#include "pidgin.h"

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
#include "pidginstock.h"
#include "theme-loader.h"
#include "theme-manager.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkcellrendererexpander.h"
#include "gtkcertmgr.h"
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
#include "gtkscrollbook.h"
#include "gtksmiley.h"
#include "gtkblist-theme.h"
#include "gtkblist-theme-loader.h"
#include "gtkutils.h"
#include "pidgin/minidialog.h"
#include "pidgin/pidgintooltip.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

typedef struct
{
	PurpleAccount *account;
	GtkWidget *window;
	GtkBox *vbox;
	GtkWidget *account_menu;
	GtkSizeGroup *sg;
} PidginBlistRequestData;

typedef struct
{
	PidginBlistRequestData rq_data;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *entry_for_alias;

} PidginAddBuddyData;

typedef struct
{
	PidginBlistRequestData rq_data;
	gchar *default_chat_name;
	GList *entries;
} PidginChatData;

typedef struct
{
	PidginChatData chat_data;

	GtkWidget *alias_entry;
	GtkWidget *group_combo;
	GtkWidget *autojoin;
	GtkWidget *persistent;
} PidginAddChatData;

typedef struct
{
	/** Used to hold error minidialogs.  Gets packed
	 *  inside PidginBuddyList.error_buttons
	 */
	PidginScrollBook *error_scrollbook;

	/** Pointer to the mini-dialog about having signed on elsewhere, if one
	 *  is showing; @c NULL otherwise.
	 */
	PidginMiniDialog *signed_on_elsewhere;

	PidginBlistTheme *current_theme;
} PidginBuddyListPrivate;

#define PIDGIN_BUDDY_LIST_GET_PRIVATE(list) \
	((PidginBuddyListPrivate *)((list)->priv))

static GtkWidget *accountmenu = NULL;

static guint visibility_manager_count = 0;
static GdkVisibilityState gtk_blist_visibility = GDK_VISIBILITY_UNOBSCURED;
static gboolean gtk_blist_focused = FALSE;
static gboolean editing_blist = FALSE;

static GList *pidgin_blist_sort_methods = NULL;
static struct pidgin_blist_sort_method *current_sort_method = NULL;
static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_log_activity(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static PidginBuddyList *gtkblist = NULL;

static GList *groups_tree(void);
static gboolean pidgin_blist_refresh_timer(PurpleBuddyList *list);
static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change);
static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data);
static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_group(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node);
static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full);
static const char *item_factory_translate_func (const char *path, gpointer func_data);
static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter);
static gboolean buddy_is_displayable(PurpleBuddy *buddy);
static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender);
static void pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node);
static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded);
static void pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node);
static void set_urgent(void);

typedef enum {
	PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE            =  1 << 0,  /* Whether there's pending message in a conversation */
	PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK	 =  1 << 1,  /* Whether there's a pending message in a chat that mentions our nick */
} PidginBlistNodeFlags;

typedef struct _pidgin_blist_node {
	GtkTreeRowReference *row;
	gboolean contact_expanded;
	gboolean recent_signonoff;
	gint recent_signonoff_timer;
	struct {
		PurpleConversation *conv;
		time_t last_message;          /* timestamp for last displayed message */
		PidginBlistNodeFlags flags;
	} conv;
} PidginBlistNode;

/***************************************************
 *              Callbacks                          *
 ***************************************************/
static gboolean gtk_blist_visibility_cb(GtkWidget *w, GdkEventVisibility *event, gpointer data)
{
	GdkVisibilityState old_state = gtk_blist_visibility;
	gtk_blist_visibility = event->state;

	if (gtk_blist_visibility == GDK_VISIBILITY_FULLY_OBSCURED &&
		old_state != GDK_VISIBILITY_FULLY_OBSCURED) {

		/* no longer fully obscured */
		pidgin_blist_refresh_timer(purple_get_blist());
	}

	/* continue to handle event normally */
	return FALSE;
}

static gboolean gtk_blist_window_state_cb(GtkWidget *w, GdkEventWindowState *event, gpointer data)
{
	if(event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN) {
		if(event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN)
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", FALSE);
		else {
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", TRUE);
			pidgin_blist_refresh_timer(purple_get_blist());
		}
	}

	if(event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		if(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", TRUE);
		else
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", FALSE);
	}

	/* Refresh gtkblist if un-iconifying */
	if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED){
		if (!(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED))
			pidgin_blist_refresh_timer(purple_get_blist());
	}

	return FALSE;
}

static gboolean gtk_blist_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	if(visibility_manager_count)
		purple_blist_set_visible(FALSE);
	else
		purple_core_quit();

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
	if (x == purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/x") &&
		y == purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/y") &&
		event->width  == purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width") &&
		event->height == purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/height")) {

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
	if(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized"))
		return FALSE;

	/* store the position */
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/x",      x);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/y",      y);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/width",  event->width);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/height", event->height);

	/* continue to handle event normally */
	return FALSE;
}

static void gtk_blist_menu_info_cb(GtkWidget *w, PurpleBuddy *b)
{
	PurpleAccount *account = purple_buddy_get_account(b);

	pidgin_retrieve_user_info(purple_account_get_connection(account),
	                          purple_buddy_get_name(b));
}

static void gtk_blist_menu_im_cb(GtkWidget *w, PurpleBuddy *b)
{
	pidgin_dialogs_im_with_user(purple_buddy_get_account(b),
	                            purple_buddy_get_name(b));
}

#ifdef USE_VV
static void gtk_blist_menu_audio_call_cb(GtkWidget *w, PurpleBuddy *b)
{
	purple_prpl_initiate_media(purple_buddy_get_account(b),
		purple_buddy_get_name(b), PURPLE_MEDIA_AUDIO);
}

static void gtk_blist_menu_video_call_cb(GtkWidget *w, PurpleBuddy *b)
{
	/* if the buddy supports both audio and video, start a combined call,
	 otherwise start a pure video session */
	if (purple_prpl_get_media_caps(purple_buddy_get_account(b),
			purple_buddy_get_name(b)) &
			PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
		purple_prpl_initiate_media(purple_buddy_get_account(b),
			purple_buddy_get_name(b), PURPLE_MEDIA_AUDIO | PURPLE_MEDIA_VIDEO);
	} else {
		purple_prpl_initiate_media(purple_buddy_get_account(b),
			purple_buddy_get_name(b), PURPLE_MEDIA_VIDEO);
	}
}

#endif

static void gtk_blist_menu_send_file_cb(GtkWidget *w, PurpleBuddy *b)
{
	PurpleAccount *account = purple_buddy_get_account(b);

	serv_send_file(purple_account_get_connection(account),
	               purple_buddy_get_name(b), NULL);
}

static void gtk_blist_menu_move_to_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleGroup *group = g_object_get_data(G_OBJECT(w), "groupnode");
	purple_blist_add_contact((PurpleContact *)node, group, NULL);

}

static void gtk_blist_menu_autojoin_cb(GtkWidget *w, PurpleChat *chat)
{
	purple_blist_node_set_bool((PurpleBlistNode*)chat, "gtk-autojoin",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
}

static void gtk_blist_menu_persistent_cb(GtkWidget *w, PurpleChat *chat)
{
	purple_blist_node_set_bool((PurpleBlistNode*)chat, "gtk-persistent",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
}

static PurpleConversation *
find_conversation_with_buddy(PurpleBuddy *buddy)
{
	PidginBlistNode *ui = purple_blist_node_get_ui_data(PURPLE_BLIST_NODE(buddy));
	if (ui)
		return ui->conv.conv;
	return purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
									     purple_buddy_get_name(buddy),
									     purple_buddy_get_account(buddy));
}

static void gtk_blist_join_chat(PurpleChat *chat)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	PurplePluginProtocolInfo *prpl_info;
	GHashTable *components;
	const char *name;
	char *chat_name;

	account = purple_chat_get_account(chat);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(account)));

	components = purple_chat_get_components(chat);

	if (prpl_info && prpl_info->get_chat_name)
		chat_name = prpl_info->get_chat_name(components);
	else
		chat_name = NULL;

	if (chat_name)
		name = chat_name;
	else
		name = purple_chat_get_name(chat);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, name,
	                                             account);

	if (conv != NULL) {
		pidgin_conv_attach_to_conversation(conv);
		purple_conversation_present(conv);
	}

	serv_join_chat(purple_account_get_connection(account), components);
	g_free(chat_name);
}

static void gtk_blist_menu_join_cb(GtkWidget *w, PurpleChat *chat)
{
	gtk_blist_join_chat(chat);
}

static void gtk_blist_renderer_editing_cancelled_cb(GtkCellRenderer *renderer, PurpleBuddyList *list)
{
	editing_blist = FALSE;
	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
	pidgin_blist_refresh(list);
}

static void gtk_blist_renderer_editing_started_cb(GtkCellRenderer *renderer,
		GtkCellEditable *editable,
		gchar *path_str,
		gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GValue val;
	PurpleBlistNode *node;
	const char *text = NULL;

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	switch (purple_blist_node_get_type(node)) {
	case PURPLE_BLIST_CONTACT_NODE:
		text = purple_contact_get_alias(PURPLE_CONTACT(node));
		break;
	case PURPLE_BLIST_BUDDY_NODE:
		text = purple_buddy_get_alias(PURPLE_BUDDY(node));
		break;
	case PURPLE_BLIST_GROUP_NODE:
		text = purple_group_get_name(PURPLE_GROUP(node));
		break;
	case PURPLE_BLIST_CHAT_NODE:
		text = purple_chat_get_name(PURPLE_CHAT(node));
		break;
	default:
		g_return_if_reached();
	}

	if (GTK_IS_ENTRY (editable)) {
		GtkEntry *entry = GTK_ENTRY (editable);
		gtk_entry_set_text(entry, text);
	}
	editing_blist = TRUE;
}

static void
gtk_blist_do_personize(GList *merges)
{
	PurpleBlistNode *contact = NULL;
	int max = 0;
	GList *tmp;

	/* First, we find the contact to merge the rest of the buddies into.
 	 * This will be the contact with the most buddies in it; ties are broken
 	 * by which contact is higher in the list
 	 */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		PurpleBlistNode *b;
		PurpleBlistNodeType type;
		int i = 0;

		type = purple_blist_node_get_type(node);

		if (type == PURPLE_BLIST_BUDDY_NODE) {
			node = purple_blist_node_get_parent(node);
			type = purple_blist_node_get_type(node);
		}

		if (type != PURPLE_BLIST_CONTACT_NODE)
			continue;

		for (b = purple_blist_node_get_first_child(node);
		     b;
		     b = purple_blist_node_get_sibling_next(b))
		{
			i++;
		}

		if (i > max) {
			contact = node;
			max = i;
		}
	}

	if (contact == NULL)
		return;

	/* Merge all those buddies into this contact */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		if (purple_blist_node_get_type(node) == PURPLE_BLIST_BUDDY_NODE)
			node = purple_blist_node_get_parent(node);

		if (node == contact)
			continue;

		purple_blist_merge_contact((PurpleContact *)node, contact);
	}

	/* And show the expanded contact, so the people know what's going on */
	pidgin_blist_expand_contact_cb(NULL, contact);
	g_list_free(merges);
}

static void
gtk_blist_auto_personize(PurpleBlistNode *group, const char *alias)
{
	PurpleBlistNode *contact;
	PurpleBlistNode *buddy;
	GList *merges = NULL;
	int i = 0;
	char *a = g_utf8_casefold(alias, -1);

	for (contact = purple_blist_node_get_first_child(group);
	     contact != NULL;
	     contact = purple_blist_node_get_sibling_next(contact)) {
		char *node_alias;
		if (purple_blist_node_get_type(contact) != PURPLE_BLIST_CONTACT_NODE)
			continue;

		node_alias = g_utf8_casefold(purple_contact_get_alias((PurpleContact *)contact), -1);
		if (node_alias && !g_utf8_collate(node_alias, a)) {
			merges = g_list_append(merges, contact);
			i++;
			g_free(node_alias);
			continue;
		}
		g_free(node_alias);

		for (buddy = purple_blist_node_get_first_child(contact);
		     buddy;
		     buddy = purple_blist_node_get_sibling_next(buddy))
		{
			if (purple_blist_node_get_type(buddy) != PURPLE_BLIST_BUDDY_NODE)
				continue;

			node_alias = g_utf8_casefold(purple_buddy_get_alias(PURPLE_BUDDY(buddy)), -1);
			if (node_alias && !g_utf8_collate(node_alias, a)) {
				merges = g_list_append(merges, buddy);
				i++;
				g_free(node_alias);
				break;
			}
			g_free(node_alias);
		}
	}
	g_free(a);

	if (i > 1)
	{
		char *msg = g_strdup_printf(ngettext("You have %d contact named %s. Would you like to merge them?", "You currently have %d contacts named %s. Would you like to merge them?", i), i, alias);
		purple_request_action(NULL, NULL, msg, _("Merging these contacts will cause them to share a single entry on the buddy list and use a single conversation window. "
							 "You can separate them again by choosing 'Expand' from the contact's context menu"), 0, NULL, NULL, NULL,
				      merges, 2, _("_Yes"), PURPLE_CALLBACK(gtk_blist_do_personize), _("_No"), PURPLE_CALLBACK(g_list_free));
		g_free(msg);
	} else
		g_list_free(merges);
}

static void gtk_blist_renderer_edited_cb(GtkCellRendererText *text_rend, char *arg1,
					 char *arg2, PurpleBuddyList *list)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GValue val;
	PurpleBlistNode *node;
	PurpleGroup *dest;

	editing_blist = FALSE;
	path = gtk_tree_path_new_from_string (arg1);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), TRUE);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", FALSE, NULL);

	switch (purple_blist_node_get_type(node))
	{
		case PURPLE_BLIST_CONTACT_NODE:
			{
				PurpleContact *contact = PURPLE_CONTACT(node);
				struct _pidgin_blist_node *gtknode =
					(struct _pidgin_blist_node *)purple_blist_node_get_ui_data(node);

				/*
				 * XXX Using purple_contact_get_alias here breaks because we
				 * specifically want to check the contact alias only (i.e. not
				 * the priority buddy, which purple_contact_get_alias does).
				 * Adding yet another get_alias is evil, so figure this out
				 * later :-P
				 */
				if (contact->alias || gtknode->contact_expanded) {
					purple_blist_alias_contact(contact, arg2);
					gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
				} else {
					PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);
					purple_blist_alias_buddy(buddy, arg2);
					serv_alias_buddy(buddy);
					gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
				}
			}
			break;

		case PURPLE_BLIST_BUDDY_NODE:
			{
				PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));

				purple_blist_alias_buddy(PURPLE_BUDDY(node), arg2);
				serv_alias_buddy(PURPLE_BUDDY(node));
				gtk_blist_auto_personize(PURPLE_BLIST_NODE(group), arg2);
			}
			break;
		case PURPLE_BLIST_GROUP_NODE:
			dest = purple_find_group(arg2);
			if (dest != NULL && purple_utf8_strcasecmp(arg2, purple_group_get_name(PURPLE_GROUP(node)))) {
				pidgin_dialogs_merge_groups(PURPLE_GROUP(node), arg2);
			} else {
				purple_blist_rename_group(PURPLE_GROUP(node), arg2);
			}
			break;
		case PURPLE_BLIST_CHAT_NODE:
			purple_blist_alias_chat(PURPLE_CHAT(node), arg2);
			break;
		default:
			break;
	}
	pidgin_blist_refresh(list);
}

static void
chat_components_edit_ok(PurpleChat *chat, PurpleRequestFields *allfields)
{
	GList *groups, *fields;

	for (groups = purple_request_fields_get_groups(allfields); groups; groups = groups->next) {
		fields = purple_request_field_group_get_fields(groups->data);
		for (; fields; fields = fields->next) {
			PurpleRequestField *field = fields->data;
			const char *id;
			char *val;

			id = purple_request_field_get_id(field);
			if (purple_request_field_get_type(field) == PURPLE_REQUEST_FIELD_INTEGER)
				val = g_strdup_printf("%d", purple_request_field_int_get_value(field));
			else
				val = g_strdup(purple_request_field_string_get_value(field));

			if (!val) {
				g_hash_table_remove(purple_chat_get_components(chat), id);
			} else {
				g_hash_table_replace(purple_chat_get_components(chat), g_strdup(id), val);  /* val should not be free'd */
			}
		}
	}
}

static void chat_components_edit(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;
	GList *parts, *iter;
	struct proto_chat_entry *pce;
	PurpleConnection *gc;
	PurpleChat *chat = (PurpleChat*)node;

	purple_request_fields_add_group(fields, group);

	gc = purple_account_get_connection(purple_chat_get_account(chat));
	parts = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc))->chat_info(gc);

	for (iter = parts; iter; iter = iter->next) {
		pce = iter->data;
		if (pce->is_int) {
			int val;
			const char *str = g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier);
			if (!str || sscanf(str, "%d", &val) != 1)
				val = pce->min;
			field = purple_request_field_int_new(pce->identifier, pce->label, val);
		} else {
			field = purple_request_field_string_new(pce->identifier, pce->label,
					g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier), FALSE);
			if (pce->secret)
				purple_request_field_string_set_masked(field, TRUE);
		}

		if (pce->required)
			purple_request_field_set_required(field, TRUE);

		purple_request_field_group_add_field(group, field);
		g_free(pce);
	}

	g_list_free(parts);

	purple_request_fields(NULL, _("Edit Chat"), NULL, _("Please update the necessary fields."),
			fields, _("Save"), G_CALLBACK(chat_components_edit_ok), _("Cancel"), NULL,
			NULL, NULL, NULL,
			chat);
}

static void gtk_blist_menu_alias_cb(GtkWidget *w, PurpleBlistNode *node)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!(get_iter_from_node(node, &iter))) {
		/* This is either a bug, or the buddy is in a collapsed contact */
		node = purple_blist_node_get_parent(node);
		if (!get_iter_from_node(node, &iter))
			/* Now it's definitely a bug */
			return;
	}

	pidgin_blist_tooltip_destroy();

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", TRUE, NULL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), FALSE);
	gtk_widget_grab_focus(gtkblist->treeview);
	gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(gtkblist->treeview), path,
			gtkblist->text_column, gtkblist->text_rend, TRUE);
	gtk_tree_path_free(path);
}

static void gtk_blist_menu_bp_cb(GtkWidget *w, PurpleBuddy *b)
{
	pidgin_pounce_editor_show(purple_buddy_get_account(b),
	                          purple_buddy_get_name(b), NULL);
}

static void gtk_blist_menu_showlog_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleLogType type;
	PurpleAccount *account;
	char *name = NULL;

	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *b = (PurpleBuddy*) node;
		type = PURPLE_LOG_IM;
		name = g_strdup(purple_buddy_get_name(b));
		account = purple_buddy_get_account(b);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		PurpleChat *c = PURPLE_CHAT(node);
		PurplePluginProtocolInfo *prpl_info = NULL;
		type = PURPLE_LOG_CHAT;
		account = purple_chat_get_account(c);
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(account)));
		if (prpl_info && prpl_info->get_chat_name) {
			name = prpl_info->get_chat_name(purple_chat_get_components(c));
		}
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		pidgin_log_show_contact(PURPLE_CONTACT(node));
		pidgin_clear_cursor(gtkblist->window);
		return;
	} else {
		pidgin_clear_cursor(gtkblist->window);

		/* This callback should not have been registered for a node
		 * that doesn't match the type of one of the blocks above. */
		g_return_if_reached();
	}

	if (name && account) {
		pidgin_log_show(type, name, account);
		pidgin_clear_cursor(gtkblist->window);
	}

	g_free(name);
}

static void gtk_blist_menu_showoffline_cb(GtkWidget *w, PurpleBlistNode *node)
{
	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		purple_blist_node_set_bool(node, "show_offline",
		                           !purple_blist_node_get_bool(node, "show_offline"));
		pidgin_blist_update(purple_get_blist(), node);
	}
	else if (PURPLE_BLIST_NODE_IS_CONTACT(node))
	{
		PurpleBlistNode *bnode;
		gboolean setting = !purple_blist_node_get_bool(node, "show_offline");

		purple_blist_node_set_bool(node, "show_offline", setting);
		for (bnode = purple_blist_node_get_first_child(node);
		     bnode != NULL;
		     bnode = purple_blist_node_get_sibling_next(bnode))
		{
			purple_blist_node_set_bool(bnode, "show_offline", setting);
			pidgin_blist_update(purple_get_blist(), bnode);
		}
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleBlistNode *cnode, *bnode;
		gboolean setting = !purple_blist_node_get_bool(node, "show_offline");

		purple_blist_node_set_bool(node, "show_offline", setting);
		for (cnode = purple_blist_node_get_first_child(node);
		     cnode != NULL;
		     cnode = purple_blist_node_get_sibling_next(cnode))
		{
			purple_blist_node_set_bool(cnode, "show_offline", setting);
			for (bnode = purple_blist_node_get_first_child(cnode);
			     bnode != NULL;
			     bnode = purple_blist_node_get_sibling_next(bnode))
			{
				purple_blist_node_set_bool(bnode, "show_offline", setting);
				pidgin_blist_update(purple_get_blist(), bnode);
			}
		}
	}
}

static void gtk_blist_show_systemlog_cb(void)
{
	pidgin_syslog_show();
}

static void gtk_blist_show_onlinehelp_cb(void)
{
	purple_notify_uri(NULL, PURPLE_WEBSITE "documentation");
}

static void
do_join_chat(PidginChatData *data)
{
	if (data)
	{
		GHashTable *components =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		GList *tmp;
		PurpleChat *chat;

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

		chat = purple_chat_new(data->rq_data.account, NULL, components);
		gtk_blist_join_chat(chat);
		purple_blist_remove_chat(chat);
	}
}

static void
do_joinchat(GtkWidget *dialog, int id, PidginChatData *info)
{
	switch(id)
	{
		case GTK_RESPONSE_OK:
			do_join_chat(info);
			break;

		case 1:
			pidgin_roomlist_dialog_show_with_account(info->rq_data.account);
			return;

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
set_sensitive_if_input_cb(GtkWidget *entry, gpointer user_data)
{
	PurplePluginProtocolInfo *prpl_info;
	PurpleConnection *gc;
	PidginChatData *data;
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

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), GTK_RESPONSE_OK, sensitive);

	gc = purple_account_get_connection(data->rq_data.account);
	prpl_info = (gc != NULL) ? PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl) : NULL;
	sensitive = (prpl_info != NULL && prpl_info->roomlist_get_list != NULL);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), 1, sensitive);
}

static void
pidgin_blist_update_privacy_cb(PurpleBuddy *buddy)
{
	struct _pidgin_blist_node *ui_data = purple_blist_node_get_ui_data(PURPLE_BLIST_NODE(buddy));
	if (ui_data == NULL || ui_data->row == NULL)
		return;
	pidgin_blist_update_buddy(purple_get_blist(), PURPLE_BLIST_NODE(buddy), TRUE);
}

static gboolean
chat_account_filter_func(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePluginProtocolInfo *prpl_info = NULL;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	return (prpl_info->chat_info != NULL);
}

gboolean
pidgin_blist_joinchat_is_showable()
{
	GList *c;
	PurpleConnection *gc;

	for (c = purple_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (chat_account_filter_func(purple_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

static GtkWidget *
make_blist_request_dialog(PidginBlistRequestData *data, PurpleAccount *account,
	const char *title, const char *window_role, const char *label_text, 
	GCallback callback_func, PurpleFilterAccountFunc filter_func,
	GCallback response_cb)
{
	GtkWidget *label;
	GtkWidget *img;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWindow *blist_window;
	PidginBuddyList *gtkblist;

	data->account = account;

	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
		gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));

	gtkblist = PIDGIN_BLIST(purple_get_blist());
	blist_window = gtkblist ? GTK_WINDOW(gtkblist->window) : NULL;

	data->window = gtk_dialog_new_with_buttons(title,
		blist_window, GTK_DIALOG_NO_SEPARATOR,
		NULL);

	gtk_window_set_transient_for(GTK_WINDOW(data->window), blist_window);
	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(data->window)->vbox), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_role(GTK_WINDOW(data->window), window_role);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(label_text);

	gtk_widget_set_size_request(label, 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	data->account_menu = pidgin_account_option_menu_new(account, FALSE,
			callback_func, filter_func, data);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("A_ccount"), data->sg, data->account_menu, TRUE, NULL);

	data->vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
	gtk_container_set_border_width(GTK_CONTAINER(data->vbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(data->vbox), FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(data->window), "response", response_cb, data);

	g_object_unref(data->sg);

	return vbox;
}

static void
rebuild_chat_entries(PidginChatData *data, const char *default_chat_name)
{
	PurpleConnection *gc;
	GList *list = NULL, *tmp;
	GHashTable *defaults = NULL;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	g_return_if_fail(data->rq_data.account != NULL);

	gc = purple_account_get_connection(data->rq_data.account);

	gtk_container_foreach(GTK_CONTAINER(data->rq_data.vbox), (GtkCallback)gtk_widget_destroy, NULL);

	g_list_free(data->entries);
	data->entries = NULL;

	if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
		list = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info(gc);

	if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
		defaults = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, default_chat_name);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *input;

		pce = tmp->data;

		if (pce->is_int)
		{
			GtkObject *adjust;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max,
										1, 10, 10);
			input = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			gtk_widget_set_size_request(input, 50, -1);
			pidgin_add_widget_to_vbox(GTK_BOX(data->rq_data.vbox), pce->label, data->rq_data.sg, input, FALSE, NULL);
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
#if !GTK_CHECK_VERSION(2,16,0)
				if (gtk_entry_get_invisible_char(GTK_ENTRY(input)) == '*')
					gtk_entry_set_invisible_char(GTK_ENTRY(input), PIDGIN_INVISIBLE_CHAR);
#endif /* Less than GTK+ 2.16 */
			}
			pidgin_add_widget_to_vbox(data->rq_data.vbox, pce->label, data->rq_data.sg, input, TRUE, NULL);
			g_signal_connect(G_OBJECT(input), "changed",
							 G_CALLBACK(set_sensitive_if_input_cb), data);
		}

		/* Do the following for any type of input widget */
		if (focus)
		{
			gtk_widget_grab_focus(input);
			focus = FALSE;
		}
		g_object_set_data(G_OBJECT(input), "identifier", (gpointer)pce->identifier);
		g_object_set_data(G_OBJECT(input), "is_spin", GINT_TO_POINTER(pce->is_int));
		g_object_set_data(G_OBJECT(input), "required", GINT_TO_POINTER(pce->required));
		data->entries = g_list_append(data->entries, input);

		g_free(pce);
	}

	g_list_free(list);
	g_hash_table_destroy(defaults);

	/* Set whether the "OK" button should be clickable initially */
	set_sensitive_if_input_cb(NULL, data);

	gtk_widget_show_all(GTK_WIDGET(data->rq_data.vbox));
}

static void
chat_select_account_cb(GObject *w, PurpleAccount *account,
                       PidginChatData *data)
{
	if (strcmp(purple_account_get_protocol_id(data->rq_data.account),
	           purple_account_get_protocol_id(account)) == 0)
	{
		data->rq_data.account = account;
	}
	else
	{
		data->rq_data.account = account;
		rebuild_chat_entries(data, data->default_chat_name);
	}
}

void
pidgin_blist_joinchat_show(void)
{
	PidginChatData *data = NULL;

	data = g_new0(PidginChatData, 1);

	make_blist_request_dialog((PidginBlistRequestData *)data, NULL,
		_("Join a Chat"), "join_chat",
		_("Please enter the appropriate information about the chat "
			"you would like to join.\n"),
		G_CALLBACK(chat_select_account_cb),
		chat_account_filter_func, (GCallback)do_joinchat);
	gtk_dialog_add_buttons(GTK_DIALOG(data->rq_data.window),
		_("Room _List"), 1,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		PIDGIN_STOCK_CHAT, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->rq_data.window),
		GTK_RESPONSE_OK);
	data->default_chat_name = NULL;
	data->rq_data.account = pidgin_account_option_menu_get_selected(data->rq_data.account_menu);

	rebuild_chat_entries(data, NULL);

	gtk_widget_show_all(data->rq_data.window);
}

static void gtk_blist_row_expanded_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PurpleBlistNode *node;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		char *title;

		title = pidgin_get_group_title(node, TRUE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", FALSE);
		pidgin_blist_tooltip_destroy();
	}
}

static void gtk_blist_row_collapsed_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PurpleBlistNode *node;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &val);

	node = g_value_get_pointer(&val);

	if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		char *title;
		struct _pidgin_blist_node *gtknode;
		PurpleBlistNode *cnode;

		title = pidgin_get_group_title(node, FALSE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", TRUE);

		for(cnode = purple_blist_node_get_first_child(node); cnode; cnode = purple_blist_node_get_sibling_next(cnode)) {
			if (PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
				gtknode = purple_blist_node_get_ui_data(cnode);
				if (!gtknode->contact_expanded)
					continue;
				gtknode->contact_expanded = FALSE;
				pidgin_blist_update_contact(NULL, cnode);
			}
		}
		pidgin_blist_tooltip_destroy();
	} else if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		pidgin_blist_collapse_contact_cb(NULL, node);
	}
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GValue val;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);

	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *buddy;

		if(PURPLE_BLIST_NODE_IS_CONTACT(node))
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			buddy = (PurpleBuddy*)node;

		pidgin_dialogs_im_with_user(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		gtk_blist_join_chat((PurpleChat *)node);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
/*		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);*/
	}
}

static void pidgin_blist_add_chat_cb(void)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	PurpleBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_BLIST_NODE_IS_BUDDY(node))
			purple_blist_request_add_chat(NULL, purple_buddy_get_group(PURPLE_BUDDY(node)), NULL, NULL);
		if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_CHAT(node))
			purple_blist_request_add_chat(NULL, purple_contact_get_group(PURPLE_CONTACT(node)), NULL, NULL);
		else if (PURPLE_BLIST_NODE_IS_GROUP(node))
			purple_blist_request_add_chat(NULL, (PurpleGroup*)node, NULL, NULL);
	}
	else {
		purple_blist_request_add_chat(NULL, NULL, NULL, NULL);
	}
}

static void pidgin_blist_add_buddy_cb(void)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	PurpleBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
			PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_CHAT(node)) {
			PurpleGroup *group = purple_contact_get_group(PURPLE_CONTACT(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(PURPLE_GROUP(node)), NULL);
		}
	}
	else {
		purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void
pidgin_blist_remove_cb (GtkWidget *w, PurpleBlistNode *node)
{
	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		pidgin_dialogs_remove_buddy((PurpleBuddy*)node);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		pidgin_dialogs_remove_chat((PurpleChat*)node);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		pidgin_dialogs_remove_group((PurpleGroup*)node);
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		pidgin_dialogs_remove_contact((PurpleContact*)node);
	}
}

struct _expand {
	GtkTreeView *treeview;
	GtkTreePath *path;
	PurpleBlistNode *node;
};

static gboolean
scroll_to_expanded_cell(gpointer data)
{
	struct _expand *ex = data;
	gtk_tree_view_scroll_to_cell(ex->treeview, ex->path, NULL, FALSE, 0, 0);
	pidgin_blist_update_contact(NULL, ex->node);

	gtk_tree_path_free(ex->path);
	g_free(ex);

	return FALSE;
}

static void
pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	struct _pidgin_blist_node *gtknode;
	GtkTreeIter iter, parent;
	PurpleBlistNode *bnode;
	GtkTreePath *path;

	if(!PURPLE_BLIST_NODE_IS_CONTACT(node))
		return;

	gtknode = purple_blist_node_get_ui_data(node);

	gtknode->contact_expanded = TRUE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
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
		ex->node = purple_blist_node_get_first_child(node);
		g_idle_add(scroll_to_expanded_cell, ex);
	}
}

static void
pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleBlistNode *bnode;
	struct _pidgin_blist_node *gtknode;

	if(!PURPLE_BLIST_NODE_IS_CONTACT(node))
		return;

	gtknode = purple_blist_node_get_ui_data(node);

	gtknode->contact_expanded = FALSE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
	}
}

static void
toggle_privacy(GtkWidget *widget, PurpleBlistNode *node)
{
	PurpleBuddy *buddy;
	PurpleAccount *account;
	gboolean permitted;
	const char *name;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;

	buddy = (PurpleBuddy *)node;
	account = purple_buddy_get_account(buddy);
	name = purple_buddy_get_name(buddy);

	permitted = purple_privacy_check(account, name);

	/* XXX: Perhaps ask whether to restore the previous lists where appropirate? */

	if (permitted)
		purple_privacy_deny(account, name, FALSE, FALSE);
	else
		purple_privacy_allow(account, name, FALSE, FALSE);

	pidgin_blist_update(purple_get_blist(), node);
}

void pidgin_append_blist_node_privacy_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	PurpleBuddy *buddy = (PurpleBuddy *)node;
	PurpleAccount *account;
	gboolean permitted;

	account = purple_buddy_get_account(buddy);
	permitted = purple_privacy_check(account, purple_buddy_get_name(buddy));

	pidgin_new_item_from_stock(menu, permitted ? _("_Block") : _("Un_block"),
						permitted ? PIDGIN_STOCK_TOOLBAR_BLOCK : PIDGIN_STOCK_TOOLBAR_UNBLOCK, G_CALLBACK(toggle_privacy),
						node, 0 ,0, NULL);
}

void
pidgin_append_blist_node_proto_menu(GtkWidget *menu, PurpleConnection *gc,
                                      PurpleBlistNode *node)
{
	GList *l, *ll;
	PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(!prpl_info || !prpl_info->blist_node_menu)
		return;

	for(l = ll = prpl_info->blist_node_menu(node); l; l = l->next) {
		PurpleMenuAction *act = (PurpleMenuAction *) l->data;
		pidgin_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}

void
pidgin_append_blist_node_extended_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	GList *l, *ll;

	for(l = ll = purple_blist_node_get_extended_menu(node); l; l = l->next) {
		PurpleMenuAction *act = (PurpleMenuAction *) l->data;
		pidgin_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}



static void
pidgin_append_blist_node_move_to_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	GtkWidget *submenu;
	GtkWidget *menuitem;
	PurpleBlistNode *group;

	menuitem = gtk_menu_item_new_with_label(_("Move to"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

	for (group = purple_blist_get_root(); group; group = purple_blist_node_get_sibling_next(group)) {
		if (!PURPLE_BLIST_NODE_IS_GROUP(group))
			continue;
		if (group == purple_blist_node_get_parent(node))
			continue;
		menuitem = pidgin_new_item_from_stock(submenu, purple_group_get_name((PurpleGroup *)group), NULL,
						      G_CALLBACK(gtk_blist_menu_move_to_cb), node, 0, 0, NULL);
		g_object_set_data(G_OBJECT(menuitem), "groupnode", group);
	}
	gtk_widget_show_all(submenu);
}

void
pidgin_blist_make_buddy_menu(GtkWidget *menu, PurpleBuddy *buddy, gboolean sub) {
	PurpleAccount *account = NULL;
	PurpleConnection *pc = NULL;
	PurplePluginProtocolInfo *prpl_info;
	PurpleContact *contact;
	PurpleBlistNode *node;
	gboolean contact_expanded = FALSE;

	g_return_if_fail(menu);
	g_return_if_fail(buddy);

	account = purple_buddy_get_account(buddy);
	pc = purple_account_get_connection(account);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(pc));

	node = PURPLE_BLIST_NODE(buddy);

	contact = purple_buddy_get_contact(buddy);
	if (contact) {
		PidginBlistNode *node = purple_blist_node_get_ui_data(PURPLE_BLIST_NODE(contact));
		contact_expanded = node->contact_expanded;
	}

	if (prpl_info && prpl_info->get_info) {
		pidgin_new_item_from_stock(menu, _("Get _Info"), PIDGIN_STOCK_TOOLBAR_USER_INFO,
				G_CALLBACK(gtk_blist_menu_info_cb), buddy, 0, 0, NULL);
	}
	pidgin_new_item_from_stock(menu, _("I_M"), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW,
			G_CALLBACK(gtk_blist_menu_im_cb), buddy, 0, 0, NULL);
	
#ifdef USE_VV
	if (prpl_info && prpl_info->get_media_caps) {
		PurpleAccount *account = purple_buddy_get_account(buddy);
		const gchar *who = purple_buddy_get_name(buddy);
		PurpleMediaCaps caps = purple_prpl_get_media_caps(account, who);
		if (caps & PURPLE_MEDIA_CAPS_AUDIO) {
			pidgin_new_item_from_stock(menu, _("_Audio Call"),
				PIDGIN_STOCK_TOOLBAR_AUDIO_CALL,
				G_CALLBACK(gtk_blist_menu_audio_call_cb), buddy, 0, 0, NULL);
		}
		if (caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
			pidgin_new_item_from_stock(menu, _("Audio/_Video Call"),
				PIDGIN_STOCK_TOOLBAR_VIDEO_CALL,
				G_CALLBACK(gtk_blist_menu_video_call_cb), buddy, 0, 0, NULL);
		} else if (caps & PURPLE_MEDIA_CAPS_VIDEO) {
			pidgin_new_item_from_stock(menu, _("_Video Call"),
				PIDGIN_STOCK_TOOLBAR_VIDEO_CALL,
				G_CALLBACK(gtk_blist_menu_video_call_cb), buddy, 0, 0, NULL);
		}
	}
	
#endif
	
	if (prpl_info && prpl_info->send_file) {
		if (!prpl_info->can_receive_file ||
			prpl_info->can_receive_file(buddy->account->gc, buddy->name))
		{
			pidgin_new_item_from_stock(menu, _("_Send File..."),
									 PIDGIN_STOCK_TOOLBAR_SEND_FILE,
									 G_CALLBACK(gtk_blist_menu_send_file_cb),
									 buddy, 0, 0, NULL);
		}
	}

	pidgin_new_item_from_stock(menu, _("Add Buddy _Pounce..."), NULL,
			G_CALLBACK(gtk_blist_menu_bp_cb), buddy, 0, 0, NULL);

	if (node->parent && node->parent->child->next &&
	      !sub && !contact_expanded) {
		pidgin_new_item_from_stock(menu, _("View _Log"), NULL,
				G_CALLBACK(gtk_blist_menu_showlog_cb),
				contact, 0, 0, NULL);
	} else if (!sub) {
		pidgin_new_item_from_stock(menu, _("View _Log"), NULL,
				G_CALLBACK(gtk_blist_menu_showlog_cb), buddy, 0, 0, NULL);
	}

	if (!PURPLE_BLIST_NODE_HAS_FLAG(node, PURPLE_BLIST_NODE_FLAG_NO_SAVE)) {
		gboolean show_offline = purple_blist_node_get_bool(node, "show_offline");
		pidgin_new_item_from_stock(menu, show_offline ? _("Hide When Offline") : _("Show When Offline"),
				NULL, G_CALLBACK(gtk_blist_menu_showoffline_cb), node, 0, 0, NULL);
	}

	pidgin_append_blist_node_proto_menu(menu, buddy->account->gc, node);
	pidgin_append_blist_node_extended_menu(menu, node);

	if (!contact_expanded && contact != NULL)
		pidgin_append_blist_node_move_to_menu(menu, (PurpleBlistNode *)contact);

	if (node->parent && node->parent->child->next &&
              !sub && !contact_expanded) {
		pidgin_separator(menu);
		pidgin_append_blist_node_privacy_menu(menu, node);
		pidgin_new_item_from_stock(menu, _("_Alias..."), PIDGIN_STOCK_ALIAS,
				G_CALLBACK(gtk_blist_menu_alias_cb),
				contact, 0, 0, NULL);
		pidgin_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				G_CALLBACK(pidgin_blist_remove_cb),
				contact, 0, 0, NULL);
	} else if (!sub || contact_expanded) {
		pidgin_separator(menu);
		pidgin_append_blist_node_privacy_menu(menu, node);
		pidgin_new_item_from_stock(menu, _("_Alias..."), PIDGIN_STOCK_ALIAS,
				G_CALLBACK(gtk_blist_menu_alias_cb), buddy, 0, 0, NULL);
		pidgin_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				G_CALLBACK(pidgin_blist_remove_cb), buddy,
				0, 0, NULL);
	}
}

static gboolean
gtk_blist_key_press_cb(GtkWidget *tv, GdkEventKey *event, gpointer data)
{
	PurpleBlistNode *node;
	GValue val;
	GtkTreeIter iter, parent;
	GtkTreeSelection *sel;
	GtkTreePath *path;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
			NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(event->state & GDK_CONTROL_MASK &&
			(event->keyval == 'o' || event->keyval == 'O')) {
		PurpleBuddy *buddy;

		if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		} else if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
			buddy = (PurpleBuddy*)node;
		} else {
			return FALSE;
		}
		if(buddy)
			pidgin_retrieve_user_info(buddy->account->gc, buddy->name);
	} else {
		switch (event->keyval) {
			case GDK_F2:
				gtk_blist_menu_alias_cb(tv, node);
				break;

			case GDK_Left:
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(tv), path)) {
					/* Collapse the Group */
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(tv), path);
					gtk_tree_path_free(path);
					return TRUE;
				} else {
					/* Select the Parent */
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path)) {
						if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(gtkblist->treemodel), &parent, &iter)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent);
							gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv), path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;

			case GDK_Right:
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
				if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(tv), path)) {
					/* Expand the Group */
					if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
						pidgin_blist_expand_contact_cb(NULL, node);
						gtk_tree_path_free(path);
						return TRUE;
					} else if (!PURPLE_BLIST_NODE_IS_BUDDY(node)) {
						gtk_tree_view_expand_row(GTK_TREE_VIEW(tv), path, FALSE);
						gtk_tree_path_free(path);
						return TRUE;
					}
				} else {
					/* Select the First Child */
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &parent, path)) {
						if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtkblist->treemodel), &iter, &parent, 0)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
							gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv), path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;
		}
	}

	return FALSE;
}

static void
set_node_custom_icon_cb(const gchar *filename, gpointer data)
{
	if (filename) {
		PurpleBlistNode *node = (PurpleBlistNode*)data;

		purple_buddy_icons_node_set_custom_icon_from_file(node,
		                                                  filename);
	}
}

static void
set_node_custom_icon(GtkWidget *w, PurpleBlistNode *node)
{
	/* This doesn't keep track of the returned dialog (so that successive
	 * calls could be made to re-display that dialog). Do we want that? */
	GtkWidget *win = pidgin_buddy_icon_chooser_new(NULL, set_node_custom_icon_cb, node);
	gtk_widget_show_all(win);
}

static void
remove_node_custom_icon(GtkWidget *w, PurpleBlistNode *node)
{
	purple_buddy_icons_node_set_custom_icon(node, NULL, 0);
}

static void
add_buddy_icon_menu_items(GtkWidget *menu, PurpleBlistNode *node)
{
	GtkWidget *item;

	pidgin_new_item_from_stock(menu, _("Set Custom Icon"), NULL,
	                           G_CALLBACK(set_node_custom_icon), node, 0,
	                           0, NULL);

	item = pidgin_new_item_from_stock(menu, _("Remove Custom Icon"), NULL,
	                           G_CALLBACK(remove_node_custom_icon), node,
	                           0, 0, NULL);
	if (!purple_buddy_icons_node_has_custom_icon(node))
		gtk_widget_set_sensitive(item, FALSE);
}

static GtkWidget *
create_group_menu (PurpleBlistNode *node, PurpleGroup *g)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new();
	item = pidgin_new_item_from_stock(menu, _("Add _Buddy..."), GTK_STOCK_ADD,
				 G_CALLBACK(pidgin_blist_add_buddy_cb), node, 0, 0, NULL);
	gtk_widget_set_sensitive(item, purple_connections_get_all() != NULL);
	item = pidgin_new_item_from_stock(menu, _("Add C_hat..."), GTK_STOCK_ADD,
				 G_CALLBACK(pidgin_blist_add_chat_cb), node, 0, 0, NULL);
	gtk_widget_set_sensitive(item, pidgin_blist_joinchat_is_showable());
	pidgin_new_item_from_stock(menu, _("_Delete Group"), GTK_STOCK_REMOVE,
				 G_CALLBACK(pidgin_blist_remove_cb), node, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("_Rename"), NULL,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
	if (!(purple_blist_node_get_flags(node) & PURPLE_BLIST_NODE_FLAG_NO_SAVE)) {
		gboolean show_offline = purple_blist_node_get_bool(node, "show_offline");
		pidgin_new_item_from_stock(menu, show_offline ? _("Hide When Offline") : _("Show When Offline"),
				NULL, G_CALLBACK(gtk_blist_menu_showoffline_cb), node, 0, 0, NULL);
	}

	add_buddy_icon_menu_items(menu, node);

	pidgin_append_blist_node_extended_menu(menu, node);

	return menu;
}

static GtkWidget *
create_chat_menu(PurpleBlistNode *node, PurpleChat *c)
{
	GtkWidget *menu;
	gboolean autojoin, persistent;

	menu = gtk_menu_new();
	autojoin = (purple_blist_node_get_bool(node, "gtk-autojoin") ||
			(purple_blist_node_get_string(node, "gtk-autojoin") != NULL));
	persistent = purple_blist_node_get_bool(node, "gtk-persistent");

	pidgin_new_item_from_stock(menu, _("_Join"), PIDGIN_STOCK_CHAT,
			G_CALLBACK(gtk_blist_menu_join_cb), node, 0, 0, NULL);
	pidgin_new_check_item(menu, _("Auto-Join"),
			G_CALLBACK(gtk_blist_menu_autojoin_cb), node, autojoin);
	pidgin_new_check_item(menu, _("Persistent"),
			G_CALLBACK(gtk_blist_menu_persistent_cb), node, persistent);
	pidgin_new_item_from_stock(menu, _("View _Log"), NULL,
			G_CALLBACK(gtk_blist_menu_showlog_cb), node, 0, 0, NULL);

	pidgin_append_blist_node_proto_menu(menu, c->account->gc, node);
	pidgin_append_blist_node_extended_menu(menu, node);

	pidgin_separator(menu);

	pidgin_new_item_from_stock(menu, _("_Edit Settings..."), NULL,
				 G_CALLBACK(chat_components_edit), node, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("_Alias..."), PIDGIN_STOCK_ALIAS,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				 G_CALLBACK(pidgin_blist_remove_cb), node, 0, 0, NULL);

	add_buddy_icon_menu_items(menu, node);

	return menu;
}

static GtkWidget *
create_contact_menu (PurpleBlistNode *node)
{
	GtkWidget *menu;

	menu = gtk_menu_new();

	pidgin_new_item_from_stock(menu, _("View _Log"), NULL,
				 G_CALLBACK(gtk_blist_menu_showlog_cb),
				 node, 0, 0, NULL);

	pidgin_separator(menu);

	pidgin_new_item_from_stock(menu, _("_Alias..."), PIDGIN_STOCK_ALIAS,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("_Remove"), GTK_STOCK_REMOVE,
				 G_CALLBACK(pidgin_blist_remove_cb), node, 0, 0, NULL);

	add_buddy_icon_menu_items(menu, node);

	pidgin_separator(menu);

	pidgin_new_item_from_stock(menu, _("_Collapse"), GTK_STOCK_ZOOM_OUT,
				 G_CALLBACK(pidgin_blist_collapse_contact_cb),
				 node, 0, 0, NULL);

	pidgin_append_blist_node_extended_menu(menu, node);
	return menu;
}

static GtkWidget *
create_buddy_menu(PurpleBlistNode *node, PurpleBuddy *b)
{
	struct _pidgin_blist_node *gtknode = (struct _pidgin_blist_node *)node->ui_data;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean show_offline = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies");

	menu = gtk_menu_new();
	pidgin_blist_make_buddy_menu(menu, b, FALSE);

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		pidgin_separator(menu);

		add_buddy_icon_menu_items(menu, node);

		if(gtknode->contact_expanded) {
			pidgin_new_item_from_stock(menu, _("_Collapse"),
						 GTK_STOCK_ZOOM_OUT,
						 G_CALLBACK(pidgin_blist_collapse_contact_cb),
						 node, 0, 0, NULL);
		} else {
			pidgin_new_item_from_stock(menu, _("_Expand"),
						 GTK_STOCK_ZOOM_IN,
						 G_CALLBACK(pidgin_blist_expand_contact_cb), node,
						 0, 0, NULL);
		}
		if(node->child->next) {
			PurpleBlistNode *bnode;

			for(bnode = node->child; bnode; bnode = bnode->next) {
				PurpleBuddy *buddy = (PurpleBuddy*)bnode;
				GdkPixbuf *buf;
				GtkWidget *submenu;
				GtkWidget *image;

				if(buddy == b)
					continue;
				if(!buddy->account->gc)
					continue;
				if(!show_offline && !PURPLE_BUDDY_IS_ONLINE(buddy))
					continue;

				menuitem = gtk_image_menu_item_new_with_label(buddy->name);
				buf = pidgin_create_prpl_icon(buddy->account,PIDGIN_PRPL_ICON_SMALL);
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

				pidgin_blist_make_buddy_menu(submenu, buddy, TRUE);
			}
		}
	}
	return menu;
}

static gboolean
pidgin_blist_show_context_menu(PurpleBlistNode *node,
								 GtkMenuPositionFunc func,
								 GtkWidget *tv,
								 guint button,
								 guint32 time)
{
	struct _pidgin_blist_node *gtknode;
	GtkWidget *menu = NULL;
	gboolean handled = FALSE;

	gtknode = (struct _pidgin_blist_node *)node->ui_data;

	/* Create a menu based on the thing we right-clicked on */
	if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *g = (PurpleGroup *)node;

		menu = create_group_menu(node, g);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		PurpleChat *c = (PurpleChat *)node;

		menu = create_chat_menu(node, c);
	} else if ((PURPLE_BLIST_NODE_IS_CONTACT(node)) && (gtknode->contact_expanded)) {
		menu = create_contact_menu(node);
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *b;

		if (PURPLE_BLIST_NODE_IS_CONTACT(node))
			b = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			b = (PurpleBuddy *)node;

		menu = create_buddy_menu(node, b);
	}

#ifdef _WIN32
	pidgin_blist_tooltip_destroy();

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

static gboolean
gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer user_data)
{
	GtkTreePath *path;
	PurpleBlistNode *node;
	GValue val;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	struct _pidgin_blist_node *gtknode;
	gboolean handled = FALSE;

	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	gtknode = (struct _pidgin_blist_node *)node->ui_data;

	/* Right click draws a context menu */
	if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS)) {
		handled = pidgin_blist_show_context_menu(node, NULL, tv, 3, event->time);

	/* CTRL+middle click expands or collapse a contact */
	} else if ((event->button == 2) && (event->type == GDK_BUTTON_PRESS) &&
			   (event->state & GDK_CONTROL_MASK) && (PURPLE_BLIST_NODE_IS_CONTACT(node))) {
		if (gtknode->contact_expanded)
			pidgin_blist_collapse_contact_cb(NULL, node);
		else
			pidgin_blist_expand_contact_cb(NULL, node);
		handled = TRUE;

	/* Double middle click gets info */
	} else if ((event->button == 2) && (event->type == GDK_2BUTTON_PRESS) &&
			   ((PURPLE_BLIST_NODE_IS_CONTACT(node)) || (PURPLE_BLIST_NODE_IS_BUDDY(node)))) {
		PurpleBuddy *b;
		if(PURPLE_BLIST_NODE_IS_CONTACT(node))
			b = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			b = (PurpleBuddy *)node;

		prpl = purple_find_prpl(purple_account_get_protocol_id(b->account));
		if (prpl != NULL)
			prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl && prpl_info->get_info)
			pidgin_retrieve_user_info(b->account->gc, b->name);
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
pidgin_blist_popup_menu_cb(GtkWidget *tv, void *user_data)
{
	PurpleBlistNode *node;
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
	handled = pidgin_blist_show_context_menu(node, pidgin_treeview_popup_menu_position_func, tv, 0, GDK_CURRENT_TIME);

	return handled;
}

static void pidgin_blist_buddy_details_cb(gpointer data, guint action, GtkWidget *item)
{
	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons",
			    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	pidgin_clear_cursor(gtkblist->window);
}

static void pidgin_blist_show_idle_time_cb(gpointer data, guint action, GtkWidget *item)
{
	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time",
			    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	pidgin_clear_cursor(gtkblist->window);
}

static void pidgin_blist_show_protocol_icons_cb(gpointer data, guint action, GtkWidget *item)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons",
			      gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));
}

static void pidgin_blist_show_empty_groups_cb(gpointer data, guint action, GtkWidget *item)
{
	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)));

	pidgin_clear_cursor(gtkblist->window);
}

static void pidgin_blist_edit_mode_cb(gpointer callback_data, guint callback_action,
		GtkWidget *checkitem)
{
	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkitem)));

	pidgin_clear_cursor(gtkblist->window);
}

static void pidgin_blist_mute_sounds_cb(gpointer data, guint action, GtkWidget *item)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", GTK_CHECK_MENU_ITEM(item)->active);
}

static void
pidgin_blist_mute_pref_cb(const char *name, PurplePrefType type,
							gconstpointer value, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(gtkblist->ift,
						N_("/Tools/Mute Sounds"))),	(gboolean)GPOINTER_TO_INT(value));
}

static void
pidgin_blist_sound_method_pref_cb(const char *name, PurplePrefType type,
									gconstpointer value, gpointer data)
{
	gboolean sensitive = TRUE;

	if(!strcmp(value, "none"))
		sensitive = FALSE;

	gtk_widget_set_sensitive(gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Mute Sounds")), sensitive);
}

static void
add_buddies_from_vcard(const char *prpl_id, PurpleGroup *group, GList *list,
					   const char *alias)
{
	GList *l;
	PurpleAccount *account = NULL;
	PurpleConnection *gc;

	if (list == NULL)
		return;

	for (l = purple_connections_get_all(); l != NULL; l = l->next)
	{
		gc = (PurpleConnection *)l->data;
		account = purple_connection_get_account(gc);

		if (!strcmp(purple_account_get_protocol_id(account), prpl_id))
			break;

		account = NULL;
	}

	if (account != NULL)
	{
		for (l = list; l != NULL; l = l->next)
		{
			purple_blist_request_add_buddy(account, l->data,
										 (group ? group->name : NULL),
										 alias);
		}
	}

	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static gboolean
parse_vcard(const char *vcard, PurpleGroup *group)
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
static void pidgin_blist_drag_begin(GtkWidget *widget,
		GdkDragContext *drag_context, gpointer user_data)
{
	pidgin_blist_tooltip_destroy();


	/* Unhook the tooltip-timeout since we don't want a tooltip
	 * to appear and obscure the dragging operation.
	 * This is a workaround for GTK+ bug 107320. */
	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}
}
#endif

static void pidgin_blist_drag_data_get_cb(GtkWidget *widget,
											GdkDragContext *dc,
											GtkSelectionData *data,
											guint info,
											guint time,
											gpointer null)
{

	if (data->target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE))
	{
		GtkTreeRowReference *ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		GtkTreePath *sourcerow = gtk_tree_row_reference_get_path(ref);
		GtkTreeIter iter;
		PurpleBlistNode *node = NULL;
		GValue val;
		if(!sourcerow)
			return;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, sourcerow);
		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
		node = g_value_get_pointer(&val);
		gtk_selection_data_set (data,
					gdk_atom_intern ("PURPLE_BLIST_NODE", FALSE),
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
		PurpleBlistNode *node = NULL;
		PurpleBuddy *buddy;
		PurpleConnection *gc;
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

		if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		{
			buddy = purple_contact_get_priority_buddy((PurpleContact *)node);
		}
		else if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
		{
			gtk_tree_path_free(sourcerow);
			return;
		}
		else
		{
			buddy = (PurpleBuddy *)node;
		}

		gc = purple_account_get_connection(buddy->account);

		if (gc == NULL)
		{
			gtk_tree_path_free(sourcerow);
			return;
		}

		protocol =
			PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->list_icon(buddy->account,
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

static void pidgin_blist_drag_data_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
			  GtkSelectionData *sd, guint info, guint t)
{
	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	if (sd->target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE) && sd->data) {
		PurpleBlistNode *n = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		memcpy(&n, sd->data, sizeof(n));
		if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
			/* if we're here, I think it means the drop is ok */
			GtkTreeIter iter;
			PurpleBlistNode *node;
			GValue val;
			struct _pidgin_blist_node *gtknode;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);
			gtknode = node->ui_data;

			if (PURPLE_BLIST_NODE_IS_CONTACT(n)) {
				PurpleContact *c = (PurpleContact*)n;
				if (PURPLE_BLIST_NODE_IS_CONTACT(node) && gtknode->contact_expanded) {
					purple_blist_merge_contact(c, node);
				} else if (PURPLE_BLIST_NODE_IS_CONTACT(node) ||
						PURPLE_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_contact(c, (PurpleGroup*)node->parent,
									node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_contact(c, (PurpleGroup*)node->parent,
									node->prev);
							break;
					}
				} else if(PURPLE_BLIST_NODE_IS_GROUP(node)) {
					purple_blist_add_contact(c, (PurpleGroup*)node, NULL);
				} else if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
					purple_blist_merge_contact(c, node);
				}
			} else if (PURPLE_BLIST_NODE_IS_BUDDY(n)) {
				PurpleBuddy *b = (PurpleBuddy*)n;
				if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_buddy(b, (PurpleContact*)node->parent,
									(PurpleGroup*)node->parent->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_buddy(b, (PurpleContact*)node->parent,
									(PurpleGroup*)node->parent->parent,
									node->prev);
							break;
					}
				} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
					purple_blist_add_buddy(b, NULL, (PurpleGroup*)node->parent,
							NULL);
				} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
					purple_blist_add_buddy(b, NULL, (PurpleGroup*)node, NULL);
				} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
					if(gtknode->contact_expanded) {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
								purple_blist_add_buddy(b, (PurpleContact*)node,
										(PurpleGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_BEFORE:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, node->prev);
								break;
						}
					} else {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							case GTK_TREE_VIEW_DROP_BEFORE:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, node->prev);
								break;
						}
					}
				}
			} else if (PURPLE_BLIST_NODE_IS_CHAT(n)) {
				PurpleChat *chat = (PurpleChat *)n;
				if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_chat(chat,
									(PurpleGroup*)node->parent->parent,
									node->parent);
							break;
					}
				} else if(PURPLE_BLIST_NODE_IS_CONTACT(node) ||
						PURPLE_BLIST_NODE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_chat(chat, (PurpleGroup*)node->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_chat(chat, (PurpleGroup*)node->parent, node->prev);
							break;
					}
				} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
					purple_blist_add_chat(chat, (PurpleGroup*)node, NULL);
				}
			} else if (PURPLE_BLIST_NODE_IS_GROUP(n)) {
				PurpleGroup *g = (PurpleGroup*)n;
				if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
					switch (position) {
					case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					case GTK_TREE_VIEW_DROP_AFTER:
						purple_blist_add_group(g, node);
						break;
					case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					case GTK_TREE_VIEW_DROP_BEFORE:
						purple_blist_add_group(g, node->prev);
						break;
					}
				} else if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
					purple_blist_add_group(g, node->parent->parent);
				} else if(PURPLE_BLIST_NODE_IS_CONTACT(node) ||
						PURPLE_BLIST_NODE_IS_CHAT(node)) {
					purple_blist_add_group(g, node->parent);
				}
			}

			gtk_tree_path_free(path);
			gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
		}
	}
	else if (sd->target == gdk_atom_intern("application/x-im-contact",
										   FALSE) && sd->data)
	{
		PurpleGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		PurpleAccount *account;
		char *protocol = NULL;
		char *username = NULL;
		char *alias = NULL;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			PurpleBlistNode *node;
			GValue val;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
									  &iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);

			if (PURPLE_BLIST_NODE_IS_BUDDY(node))
			{
				group = (PurpleGroup *)node->parent->parent;
			}
			else if (PURPLE_BLIST_NODE_IS_CHAT(node) ||
					 PURPLE_BLIST_NODE_IS_CONTACT(node))
			{
				group = (PurpleGroup *)node->parent;
			}
			else if (PURPLE_BLIST_NODE_IS_GROUP(node))
			{
				group = (PurpleGroup *)node;
			}
		}

		if (pidgin_parse_x_im_contact((const char *)sd->data, FALSE, &account,
										&protocol, &username, &alias))
		{
			if (account == NULL)
			{
				purple_notify_error(NULL, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL);
			}
			else
			{
				purple_blist_request_add_buddy(account, username,
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
		PurpleGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			PurpleBlistNode *node;
			GValue val;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
									  &iter, NODE_COLUMN, &val);
			node = g_value_get_pointer(&val);

			if (PURPLE_BLIST_NODE_IS_BUDDY(node))
			{
				group = (PurpleGroup *)node->parent->parent;
			}
			else if (PURPLE_BLIST_NODE_IS_CHAT(node) ||
					 PURPLE_BLIST_NODE_IS_CONTACT(node))
			{
				group = (PurpleGroup *)node->parent;
			}
			else if (PURPLE_BLIST_NODE_IS_GROUP(node))
			{
				group = (PurpleGroup *)node;
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
				PurpleBlistNode *node;
				GValue val;

				gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
							&iter, path);
				val.g_type = 0;
				gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel),
							  &iter, NODE_COLUMN, &val);
				node = g_value_get_pointer(&val);

				if (PURPLE_BLIST_NODE_IS_BUDDY(node) || PURPLE_BLIST_NODE_IS_CONTACT(node)) {
					PurpleBuddy *b = PURPLE_BLIST_NODE_IS_BUDDY(node) ? PURPLE_BUDDY(node) : purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
					pidgin_dnd_file_manage(sd, b->account, b->name);
					gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
				} else {
					gtk_drag_finish(dc, FALSE, FALSE, t);
				}
			}
	}
}

/* Altered from do_colorshift in gnome-panel */
static void
do_alphashift(GdkPixbuf *pixbuf, int shift)
{
	gint i, j;
	gint width, height, padding;
	guchar *pixels;
	int val;

	if (!gdk_pixbuf_get_has_alpha(pixbuf))
	  return;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	padding = gdk_pixbuf_get_rowstride(pixbuf) - width * 4;
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			pixels++;
			pixels++;
			pixels++;
			val = *pixels - shift;
			*(pixels++) = CLAMP(val, 0, 255);
		}
		pixels += padding;
	}
}


static GdkPixbuf *pidgin_blist_get_buddy_icon(PurpleBlistNode *node,
                                              gboolean scaled, gboolean greyed)
{
	gsize len;
	GdkPixbufLoader *loader;
	PurpleBuddy *buddy = NULL;
	PurpleGroup *group = NULL;
	const guchar *data = NULL;
	GdkPixbuf *buf, *ret = NULL;
	PurpleBuddyIcon *icon = NULL;
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurpleStoredImage *custom_img;
	PurplePluginProtocolInfo *prpl_info = NULL;
	gint orig_width, orig_height, scale_width, scale_height;

	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		contact = (PurpleContact*)node;
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		contact = purple_buddy_get_contact(buddy);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		group = (PurpleGroup*)node;
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		/* We don't need to do anything here. We just need to not fall
		 * into the else block and return. */
	} else {
		return NULL;
	}

	if (buddy) {
		account = purple_buddy_get_account(buddy);
	}

	if(account && account->gc) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(account->gc->prpl);
	}

#if 0
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons"))
		return NULL;
#endif

	/* If we have a contact then this is either a contact or a buddy and
	 * we want to fetch the custom icon for the contact. If we don't have
	 * a contact then this is a group or some other type of node and we
	 * want to use that directly. */
	if (contact) {
		custom_img = purple_buddy_icons_node_find_custom_icon((PurpleBlistNode*)contact);
	} else {
		custom_img = purple_buddy_icons_node_find_custom_icon(node);
	}

	if (custom_img) {
		data = purple_imgstore_get_data(custom_img);
		len = purple_imgstore_get_size(custom_img);
	}

	if (data == NULL) {
		if (buddy) {
			/* Not sure I like this...*/
			if (!(icon = purple_buddy_icons_find(buddy->account, buddy->name)))
				return NULL;
			data = purple_buddy_icon_get_data(icon, &len);
		}

		if(data == NULL)
			return NULL;
	}

	loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(loader, data, len, NULL);
	gdk_pixbuf_loader_close(loader, NULL);

	purple_imgstore_unref(custom_img);
	purple_buddy_icon_unref(icon);

	buf = gdk_pixbuf_loader_get_pixbuf(loader);
	if (buf)
		g_object_ref(G_OBJECT(buf));
	g_object_unref(G_OBJECT(loader));

	if (!buf) {
		return NULL;
	}

	if (greyed) {
		gboolean offline = FALSE, idle = FALSE;

		if (buddy) {
			PurplePresence *presence = purple_buddy_get_presence(buddy);
			if (!PURPLE_BUDDY_IS_ONLINE(buddy))
				offline = TRUE;
			if (purple_presence_is_idle(presence))
				idle = TRUE;
		} else if (group) {
			if (purple_blist_get_group_online_count(group) == 0)
				offline = TRUE;
		}

		if (offline)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);

		if (idle)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.25, FALSE);
	}

	/* I'd use the pidgin_buddy_icon_get_scale_size() thing, but it won't
	 * tell me the original size, which I need for scaling purposes. */
	scale_width = orig_width = gdk_pixbuf_get_width(buf);
	scale_height = orig_height = gdk_pixbuf_get_height(buf);

	if (prpl_info && prpl_info->icon_spec.scale_rules & PURPLE_ICON_SCALE_DISPLAY)
		purple_buddy_icon_get_scale_size(&prpl_info->icon_spec, &scale_width, &scale_height);

	if (scaled || scale_height > 200 || scale_width > 200) {
		GdkPixbuf *tmpbuf;
		float scale_size = scaled ? 32.0 : 200.0;
		if(scale_height > scale_width) {
			scale_width = scale_size * (double)scale_width / (double)scale_height;
			scale_height = scale_size;
		} else {
			scale_height = scale_size * (double)scale_height / (double)scale_width;
			scale_width = scale_size;
		}
		/* Scale & round before making square, so rectangular (but
		 * non-square) images get rounded corners too. */
		tmpbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_width, scale_height);
		gdk_pixbuf_fill(tmpbuf, 0x00000000);
		gdk_pixbuf_scale(buf, tmpbuf, 0, 0, scale_width, scale_height, 0, 0, (double)scale_width/(double)orig_width, (double)scale_height/(double)orig_height, GDK_INTERP_BILINEAR);
		if (pidgin_gdk_pixbuf_is_opaque(tmpbuf))
			pidgin_gdk_pixbuf_make_round(tmpbuf);
		ret = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_size, scale_size);
		gdk_pixbuf_fill(ret, 0x00000000);
		gdk_pixbuf_copy_area(tmpbuf, 0, 0, scale_width, scale_height, ret, (scale_size-scale_width)/2, (scale_size-scale_height)/2);
		g_object_unref(G_OBJECT(tmpbuf));
	} else {
		ret = gdk_pixbuf_scale_simple(buf,scale_width,scale_height, GDK_INTERP_BILINEAR);
	}
	g_object_unref(G_OBJECT(buf));

	return ret;
}

/* # - Status Icon
 * P - Protocol Icon
 * A - Buddy Icon
 * [ - SMALL_SPACE
 * = - LARGE_SPACE
 *                   +--- STATUS_SIZE                +--- td->avatar_width
 *                   |         +-- td->name_width    |
 *                +----+   +-------+            +---------+
 *                |    |   |       |            |         |
 *                +-------------------------------------------+
 *                |       [          =        [               |--- TOOLTIP_BORDER
 *name_height --+-| ######[BuddyName = PP     [   AAAAAAAAAAA |--+
 *              | | ######[          = PP     [   AAAAAAAAAAA |  |
 * STATUS SIZE -| | ######[[[[[[[[[[[[[[[[[[[[[   AAAAAAAAAAA |  |
 *           +--+-| ######[Account: So-and-so [   AAAAAAAAAAA |  |-- td->avatar_height
 *           |    |       [Idle: 4h 15m       [   AAAAAAAAAAA |  |
 *  height --+    |       [Foo: Bar, Baz      [   AAAAAAAAAAA |  |
 *           |    |       [Status: Awesome    [   AAAAAAAAAAA |--+
 *           +----|       [Stop: Hammer Time  [               |
 *                |       [                   [               |--- TOOLTIP_BORDER
 *                +-------------------------------------------+
 *                 |       |                |                |
 *                 |       +----------------+                |
 *                 |               |                         |
 *                 |               +-- td->width             |
 *                 |                                         |
 *                 +---- TOOLTIP_BORDER                      +---- TOOLTIP_BORDER
 *
 *
 */
#define STATUS_SIZE 16
#define TOOLTIP_BORDER 12
#define SMALL_SPACE 6
#define LARGE_SPACE 12
#define PRPL_SIZE 16
struct tooltip_data {
	PangoLayout *layout;
	PangoLayout *name_layout;
	GdkPixbuf *prpl_icon;
	GdkPixbuf *status_icon;
	GdkPixbuf *avatar;
	gboolean avatar_is_prpl_icon;
	int avatar_width;
	int avatar_height;
	int name_height;
	int name_width;
	int width;
	int height;
	int padding;
};

static PangoLayout * create_pango_layout(const char *markup, int *width, int *height)
{
	PangoLayout *layout;
	int w, h;

	layout = gtk_widget_create_pango_layout(gtkblist->tipwindow, NULL);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 300000);

	pango_layout_get_size (layout, &w, &h);
	if (width)
		*width = PANGO_PIXELS(w);
	if (height)
		*height = PANGO_PIXELS(h);
	return layout;
}

static struct tooltip_data * create_tip_for_account(PurpleAccount *account)
{
	struct tooltip_data *td = g_new0(struct tooltip_data, 1);
	td->status_icon = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
		/* Yes, status_icon, not prpl_icon */
	if (purple_account_is_disconnected(account))
		gdk_pixbuf_saturate_and_pixelate(td->status_icon, td->status_icon, 0.0, FALSE);
	td->layout = create_pango_layout(purple_account_get_username(account), &td->width, &td->height);
	td->padding = SMALL_SPACE;
	return td;
}

static struct tooltip_data * create_tip_for_node(PurpleBlistNode *node, gboolean full)
{
	struct tooltip_data *td = g_new0(struct tooltip_data, 1);
	PurpleAccount *account = NULL;
	char *tmp = NULL, *node_name = NULL, *tooltip_text = NULL;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		account = ((PurpleBuddy*)(node))->account;
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		account = ((PurpleChat*)(node))->account;
	}

	td->padding = TOOLTIP_BORDER;
	td->status_icon = pidgin_blist_get_status_icon(node, PIDGIN_STATUS_ICON_LARGE);
	td->avatar = pidgin_blist_get_buddy_icon(node, !full, FALSE);
	if (account != NULL) {
		td->prpl_icon = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
	}
	tooltip_text = pidgin_get_tooltip_text(node, full);
	if (tooltip_text && *tooltip_text) {
		td->layout = create_pango_layout(tooltip_text, &td->width, &td->height);
	}

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		tmp = g_markup_escape_text(purple_buddy_get_name((PurpleBuddy*)node), -1);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		tmp = g_markup_escape_text(purple_chat_get_name((PurpleChat*)node), -1);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		tmp = g_markup_escape_text(purple_group_get_name((PurpleGroup*)node), -1);
	} else {
		/* I don't believe this can happen currently, I think
		 * everything that calls this function checks for one of the
		 * above node types first. */
		tmp = g_strdup(_("Unknown node type"));
	}
	node_name = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>",
								tmp ? tmp : "");
	g_free(tmp);

	td->name_layout = create_pango_layout(node_name, &td->name_width, &td->name_height);
	td->name_width += SMALL_SPACE + PRPL_SIZE;
	td->name_height = MAX(td->name_height, PRPL_SIZE + SMALL_SPACE);
#if 0  /* PRPL Icon as avatar */
	if(!td->avatar && full) {
		td->avatar = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_LARGE);
		td->avatar_is_prpl_icon = TRUE;
	}
#endif

	if (td->avatar) {
		td->avatar_width = gdk_pixbuf_get_width(td->avatar);
		td->avatar_height = gdk_pixbuf_get_height(td->avatar);
	}

	g_free(node_name);
	g_free(tooltip_text);
	return td;
}

static gboolean
pidgin_blist_paint_tip(GtkWidget *widget, gpointer null)
{
	GtkStyle *style;
	int current_height, max_width;
	int max_text_width;
	int max_avatar_width;
	GList *l;
	int prpl_col = 0;
	GtkTextDirection dir = gtk_widget_get_direction(widget);
	int status_size = 0;

	if(gtkblist->tooltipdata == NULL)
		return FALSE;

	style = gtkblist->tipwindow->style;

	max_text_width = 0;
	max_avatar_width = 0;

	for(l = gtkblist->tooltipdata; l; l = l->next)
	{
		struct tooltip_data *td = l->data;

		max_text_width = MAX(max_text_width,
				MAX(td->width, td->name_width));
		max_avatar_width = MAX(max_avatar_width, td->avatar_width);
		if (td->status_icon)
			status_size = STATUS_SIZE;
	}

	max_width = TOOLTIP_BORDER + status_size + SMALL_SPACE + max_text_width + SMALL_SPACE + max_avatar_width + TOOLTIP_BORDER;
	if (dir == GTK_TEXT_DIR_RTL)
		prpl_col = TOOLTIP_BORDER + max_avatar_width + SMALL_SPACE;
	else
		prpl_col = TOOLTIP_BORDER + status_size + SMALL_SPACE + max_text_width - PRPL_SIZE;

	current_height = 12;
	for(l = gtkblist->tooltipdata; l; l = l->next)
	{
		struct tooltip_data *td = l->data;

		if (td->avatar && pidgin_gdk_pixbuf_is_opaque(td->avatar))
		{
			if (dir == GTK_TEXT_DIR_RTL)
				gtk_paint_flat_box(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
						NULL, gtkblist->tipwindow, "tooltip",
						TOOLTIP_BORDER -1, current_height -1, td->avatar_width +2, td->avatar_height + 2);
			else
				gtk_paint_flat_box(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
						NULL, gtkblist->tipwindow, "tooltip",
						max_width - (td->avatar_width+ TOOLTIP_BORDER)-1,
						current_height-1,td->avatar_width+2, td->avatar_height+2);
		}

		if (td->status_icon) {
			if (dir == GTK_TEXT_DIR_RTL)
				gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, td->status_icon,
				                0, 0, max_width - TOOLTIP_BORDER - status_size, current_height, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			else
				gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, td->status_icon,
				                0, 0, TOOLTIP_BORDER, current_height, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
		}

		if(td->avatar) {
			if (dir == GTK_TEXT_DIR_RTL)
				gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL,
						td->avatar, 0, 0, TOOLTIP_BORDER, current_height, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			else
				gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL,
						td->avatar, 0, 0, max_width - (td->avatar_width + TOOLTIP_BORDER),
						current_height, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
		}

		if (!td->avatar_is_prpl_icon && td->prpl_icon)
			gdk_draw_pixbuf(GDK_DRAWABLE(gtkblist->tipwindow->window), NULL, td->prpl_icon,
					0, 0,
					prpl_col,
					current_height + ((td->name_height / 2) - (PRPL_SIZE / 2)),
					-1 , -1, GDK_RGB_DITHER_NONE, 0, 0);

		if (td->name_layout) {
			if (dir == GTK_TEXT_DIR_RTL) {
				gtk_paint_layout(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, FALSE,
						NULL, gtkblist->tipwindow, "tooltip",
						max_width  -(TOOLTIP_BORDER + status_size + SMALL_SPACE) - PANGO_PIXELS(300000),
						current_height, td->name_layout);
			} else {
				gtk_paint_layout (style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, FALSE,
						NULL, gtkblist->tipwindow, "tooltip",
						TOOLTIP_BORDER + status_size + SMALL_SPACE, current_height, td->name_layout);
			}
		}

		if (td->layout) {
			if (dir != GTK_TEXT_DIR_RTL) {
				gtk_paint_layout (style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, FALSE,
						NULL, gtkblist->tipwindow, "tooltip",
						TOOLTIP_BORDER + status_size + SMALL_SPACE, current_height + td->name_height, td->layout);
			} else {
				gtk_paint_layout(style, gtkblist->tipwindow->window, GTK_STATE_NORMAL, FALSE,
						NULL, gtkblist->tipwindow, "tooltip",
						max_width - (TOOLTIP_BORDER + status_size + SMALL_SPACE) - PANGO_PIXELS(300000),
						current_height + td->name_height,
						td->layout);
			}
		}

		current_height += MAX(td->name_height + td->height, td->avatar_height) + td->padding;
	}
	return FALSE;
}

static void
pidgin_blist_destroy_tooltip_data(void)
{
	while(gtkblist->tooltipdata) {
		struct tooltip_data *td = gtkblist->tooltipdata->data;

		if(td->avatar)
			g_object_unref(td->avatar);
		if(td->status_icon)
			g_object_unref(td->status_icon);
		if(td->prpl_icon)
			g_object_unref(td->prpl_icon);
		if (td->layout)
			g_object_unref(td->layout);
		if (td->name_layout)
			g_object_unref(td->name_layout);
		g_free(td);
		gtkblist->tooltipdata = g_list_delete_link(gtkblist->tooltipdata, gtkblist->tooltipdata);
	}
}

void pidgin_blist_tooltip_destroy()
{
	pidgin_blist_destroy_tooltip_data();
	pidgin_tooltip_destroy();
}

static void
pidgin_blist_align_tooltip(struct tooltip_data *td, GtkWidget *widget)
{
	GtkTextDirection dir = gtk_widget_get_direction(widget);

	if (dir == GTK_TEXT_DIR_RTL)
	{
		char* layout_name = purple_markup_strip_html(pango_layout_get_text(td->name_layout));
		PangoDirection dir = pango_find_base_dir(layout_name, -1);
		if (dir == PANGO_DIRECTION_RTL || dir == PANGO_DIRECTION_NEUTRAL)
			pango_layout_set_alignment(td->name_layout, PANGO_ALIGN_RIGHT);
		g_free(layout_name);
		pango_layout_set_alignment(td->layout, PANGO_ALIGN_RIGHT);
	}
}

static gboolean
pidgin_blist_create_tooltip_for_node(GtkWidget *widget, gpointer data, int *w, int *h)
{
	PurpleBlistNode *node = data;
	int width, height;
	GList *list;
	int max_text_width = 0;
	int max_avatar_width = 0;
	int status_size = 0;

	if (gtkblist->tooltipdata) {
		gtkblist->tipwindow = NULL;
		pidgin_blist_destroy_tooltip_data();
	}

	gtkblist->tipwindow = widget;
	if (PURPLE_BLIST_NODE_IS_CHAT(node) ||
	   PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		struct tooltip_data *td = create_tip_for_node(node, TRUE);
		pidgin_blist_align_tooltip(td, gtkblist->tipwindow);
		gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *group = (PurpleGroup*)node;
		GSList *accounts;
		struct tooltip_data *td = create_tip_for_node(node, TRUE);
		pidgin_blist_align_tooltip(td, gtkblist->tipwindow);
		gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);

		/* Accounts with buddies in group */
		accounts = purple_group_get_accounts(group);
		for (; accounts != NULL;
		     accounts = g_slist_delete_link(accounts, accounts)) {
			PurpleAccount *account = accounts->data;
			td = create_tip_for_account(account);
			gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);
		}
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBlistNode *child;
		PurpleBuddy *b = purple_contact_get_priority_buddy((PurpleContact *)node);
		width = height = 0;

		for(child = node->child; child; child = child->next)
		{
			if(PURPLE_BLIST_NODE_IS_BUDDY(child) && buddy_is_displayable((PurpleBuddy*)child)) {
				struct tooltip_data *td = create_tip_for_node(child, (b == (PurpleBuddy*)child));
				pidgin_blist_align_tooltip(td, gtkblist->tipwindow);
				if (b == (PurpleBuddy *)child) {
					gtkblist->tooltipdata = g_list_prepend(gtkblist->tooltipdata, td);
				} else {
					gtkblist->tooltipdata = g_list_append(gtkblist->tooltipdata, td);
				}
			}
		}
	} else {
		return FALSE;
	}

	height = width = 0;
	for (list = gtkblist->tooltipdata; list; list = list->next) {
		struct tooltip_data *td = list->data;
		max_text_width = MAX(max_text_width, MAX(td->width, td->name_width));
		max_avatar_width = MAX(max_avatar_width, td->avatar_width);
		height += MAX(MAX(STATUS_SIZE, td->avatar_height), td->height + td->name_height) + td->padding;
		if (td->status_icon)
			status_size = MAX(status_size, STATUS_SIZE);
	}
	height += TOOLTIP_BORDER;
	width = TOOLTIP_BORDER + status_size + SMALL_SPACE + max_text_width + SMALL_SPACE + max_avatar_width + TOOLTIP_BORDER;

	if (w)
		*w = width;
	if (h)
		*h = height;

	return TRUE;
}

static gboolean pidgin_blist_expand_timeout(GtkWidget *tv)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	PurpleBlistNode *node;
	GValue val;
	struct _pidgin_blist_node *gtknode;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), gtkblist->tip_rect.x, gtkblist->tip_rect.y + (gtkblist->tip_rect.height/2),
		&path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);

	if(!PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtknode = node->ui_data;

	if (!gtknode->contact_expanded) {
		GtkTreeIter i;

		pidgin_blist_expand_contact_cb(NULL, node);

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

static gboolean buddy_is_displayable(PurpleBuddy *buddy)
{
	struct _pidgin_blist_node *gtknode;

	if(!buddy)
		return FALSE;

	gtknode = ((PurpleBlistNode*)buddy)->ui_data;

	return (purple_account_is_connected(buddy->account) &&
			(purple_presence_is_online(buddy->presence) ||
			 (gtknode && gtknode->recent_signonoff) ||
			 purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies") ||
			 purple_blist_node_get_bool((PurpleBlistNode*)buddy, "show_offline")));
}

void pidgin_blist_draw_tooltip(PurpleBlistNode *node, GtkWidget *widget)
{
	pidgin_tooltip_show(widget, node, pidgin_blist_create_tooltip_for_node, pidgin_blist_paint_tip);
}

static gboolean pidgin_blist_drag_motion_cb(GtkWidget *tv, GdkDragContext *drag_context,
					      gint x, gint y, guint time, gpointer user_data)
{
	GtkTreePath *path;
	int delay;
	GdkRectangle rect;

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
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &rect);

	if (path)
		gtk_tree_path_free(path);

	/* Only autoexpand when in the middle of the cell to avoid annoying un-intended expands */
	if (y < rect.y + (rect.height / 3) ||
	    y > rect.y + (2 * (rect.height /3)))
		return FALSE;

	rect.height = rect.height / 3;
	rect.y += rect.height;

	gtkblist->tip_rect = rect;

	gtkblist->drag_timeout = g_timeout_add(delay, (GSourceFunc)pidgin_blist_expand_timeout, tv);

	if (gtkblist->mouseover_contact) {
		if ((y < gtkblist->contact_rect.y) || ((y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

static gboolean
pidgin_blist_create_tooltip(GtkWidget *widget, GtkTreePath *path,
		gpointer null, int *w, int *h)
{
	GtkTreeIter iter;
	PurpleBlistNode *node;
	GValue val;
	gboolean editable = FALSE;

	/* If we're editing a cell (e.g. alias editing), don't show the tooltip */
	g_object_get(G_OBJECT(gtkblist->text_rend), "editable", &editable, NULL);
	if (editable)
		return FALSE;

	if (gtkblist->tooltipdata) {
		gtkblist->tipwindow = NULL;
		pidgin_blist_destroy_tooltip_data();
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &val);
	node = g_value_get_pointer(&val);
	return pidgin_blist_create_tooltip_for_node(widget, node, w, h);
}

static gboolean pidgin_blist_motion_cb (GtkWidget *tv, GdkEventMotion *event, gpointer null)
{
	if (gtkblist->mouseover_contact) {
		if ((event->y < gtkblist->contact_rect.y) || ((event->y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

static gboolean pidgin_blist_leave_cb (GtkWidget *w, GdkEventCrossing *e, gpointer n)
{
	if (gtkblist->timeout) {
		g_source_remove(gtkblist->timeout);
		gtkblist->timeout = 0;
	}

	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	if (gtkblist->mouseover_contact &&
		!((e->x > gtkblist->contact_rect.x) && (e->x < (gtkblist->contact_rect.x + gtkblist->contact_rect.width)) &&
		 (e->y > gtkblist->contact_rect.y) && (e->y < (gtkblist->contact_rect.y + gtkblist->contact_rect.height)))) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
		gtkblist->mouseover_contact = NULL;
	}
	return FALSE;
}

static void
toggle_debug(void)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled",
			!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"));
}

static char *get_mood_icon_path(const char *mood)
{
	char *path;

	if (!strcmp(mood, "busy")) {
		path = g_build_filename(DATADIR, "pixmaps", "pidgin",
		                        "status", "16", "busy.png", NULL);
	} else if (!strcmp(mood, "hiptop")) {
		path = g_build_filename(DATADIR, "pixmaps", "pidgin",
		                        "emblems", "16", "hiptop.png", NULL);
	} else {
		char *filename = g_strdup_printf("%s.png", mood);
		path = g_build_filename(DATADIR, "pixmaps", "pidgin",
		                        "emotes", "small", filename, NULL);
		g_free(filename);
	}
	return path;
}

static void
update_status_with_mood(PurpleAccount *account, const gchar *mood,
    const gchar *text)
{
	if (mood && *mood) {
		if (text) {
			purple_account_set_status(account, "mood", TRUE,
			                          PURPLE_MOOD_NAME, mood,
				    				  PURPLE_MOOD_COMMENT, text,
			                          NULL);
		} else {
			purple_account_set_status(account, "mood", TRUE,
			                          PURPLE_MOOD_NAME, mood,
			                          NULL);
		}
	} else {
		purple_account_set_status(account, "mood", FALSE, NULL);
	}
}

static void
edit_mood_cb(PurpleConnection *gc, PurpleRequestFields *fields)
{
	PurpleRequestField *mood_field;
	GList *l;

	mood_field = purple_request_fields_get_field(fields, "mood");
	l = purple_request_field_list_get_selected(mood_field);

	if (l) {
		const char *mood = purple_request_field_list_get_data(mood_field, l->data);

		if (gc) {
			const char *text;
			PurpleAccount *account = purple_connection_get_account(gc);

			if (gc->flags & PURPLE_CONNECTION_SUPPORT_MOOD_MESSAGES) {
				PurpleRequestField *text_field;
				text_field = purple_request_fields_get_field(fields, "text");
				text = purple_request_field_string_get_value(text_field);
			} else {
				text = NULL;
			}

			update_status_with_mood(account, mood, text);
		} else {
			GList *accounts = purple_accounts_get_all_active();

			for (; accounts ; accounts = g_list_delete_link(accounts, accounts)) {
				PurpleAccount *account = (PurpleAccount *) accounts->data;
				PurpleConnection *gc = purple_account_get_connection(account);

				if (gc->flags & PURPLE_CONNECTION_SUPPORT_MOODS) {
					update_status_with_mood(account, mood, NULL);
				}
			}
		}
	}
}
	
static void
global_moods_for_each(gpointer key, gpointer value, gpointer user_data)
{
	GList **out_moods = (GList **) user_data;
	PurpleMood *mood = (PurpleMood *) value;
	
	*out_moods = g_list_append(*out_moods, mood);
}

static PurpleMood *
get_global_moods(void)
{
	GHashTable *global_moods =
		g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	GHashTable *mood_counts =
		g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	GList *accounts = purple_accounts_get_all_active();
	PurpleMood *result = NULL;
	GList *out_moods = NULL;
	int i = 0;
	int num_accounts = 0;
	
	for (; accounts ; accounts = g_list_delete_link(accounts, accounts)) {
		PurpleAccount *account = (PurpleAccount *) accounts->data;
		PurpleConnection *gc = purple_account_get_connection(account);

		if (gc->flags & PURPLE_CONNECTION_SUPPORT_MOODS) {
			PurplePluginProtocolInfo *prpl_info =
				PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
			PurpleMood *mood = NULL;

			/* PURPLE_CONNECTION_SUPPORT_MOODS would not be set if the prpl doesn't
			 * have get_moods, so using PURPLE_PROTOCOL_PLUGIN_HAS_FUNC isn't necessary
			 * here */
			for (mood = prpl_info->get_moods(account) ;
			    mood->mood != NULL ; mood++) {
				int mood_count =
						GPOINTER_TO_INT(g_hash_table_lookup(mood_counts, mood->mood));

				if (!g_hash_table_lookup(global_moods, mood->mood)) {
					g_hash_table_insert(global_moods, (gpointer)mood->mood, mood);
				}
				g_hash_table_insert(mood_counts, (gpointer)mood->mood,
				    GINT_TO_POINTER(mood_count + 1));
			}

			num_accounts++;
		}
	}

	g_hash_table_foreach(global_moods, global_moods_for_each, &out_moods);
	result = g_new0(PurpleMood, g_hash_table_size(global_moods) + 1);

	while (out_moods) {
		PurpleMood *mood = (PurpleMood *) out_moods->data;
		int in_num_accounts = 
			GPOINTER_TO_INT(g_hash_table_lookup(mood_counts, mood->mood));

		if (in_num_accounts == num_accounts) {
			/* mood is present in all accounts supporting moods */
			result[i].mood = mood->mood;
			result[i].description = mood->description;
			i++;
		}
		out_moods = g_list_delete_link(out_moods, out_moods);
	}

	g_hash_table_destroy(global_moods);
	g_hash_table_destroy(mood_counts);

	return result;
}

/* get current set mood for all mood-supporting accounts, or NULL if not set
 or not set to the same on all */
static const gchar *
get_global_mood_status(void)
{
	GList *accounts = purple_accounts_get_all_active();
	const gchar *found_mood = NULL;
	
	for (; accounts ; accounts = g_list_delete_link(accounts, accounts)) {
		PurpleAccount *account = (PurpleAccount *) accounts->data;

		if (purple_account_get_connection(account)->flags &
		    PURPLE_CONNECTION_SUPPORT_MOODS) {
			PurplePresence *presence = purple_account_get_presence(account);
			PurpleStatus *status = purple_presence_get_status(presence, "mood");
			const gchar *curr_mood = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);

			if (found_mood != NULL && !purple_strequal(curr_mood, found_mood)) {
				/* found a different mood */
				found_mood = NULL;
				break;
			} else {
				found_mood = curr_mood;
			}
		}
	}

	return found_mood;
}

static void
set_mood_cb(GtkWidget *widget, PurpleAccount *account)
{
	const char *current_mood;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *g;
	PurpleRequestField *f;
	PurpleConnection *gc = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleMood *mood;
	PurpleMood *global_moods = get_global_moods();
	
	if (account) {
		PurplePresence *presence = purple_account_get_presence(account);
		PurpleStatus *status = purple_presence_get_status(presence, "mood");
		gc = purple_account_get_connection(account);
		g_return_if_fail(gc->prpl != NULL);
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
		current_mood = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);
	} else {
		current_mood = get_global_mood_status();
	}

	fields = purple_request_fields_new();
	g = purple_request_field_group_new(NULL);
	f = purple_request_field_list_new("mood", _("Please select your mood from the list"));

	purple_request_field_list_add(f, _("None"), "");
	if (current_mood == NULL)
		purple_request_field_list_add_selected(f, _("None"));

	/* TODO: rlaager wants this sorted. */
	/* The connection is checked for PURPLE_CONNECTION_SUPPORT_MOODS flag before
	 * this function is called for a non-null account. So using
	 * PURPLE_PROTOCOL_PLUGIN_HAS_FUNC isn't necessary here */
	for (mood = account ? prpl_info->get_moods(account) : global_moods;
	     mood->mood != NULL ; mood++) {
		char *path;

		if (mood->mood == NULL || mood->description == NULL)
			continue;

		path = get_mood_icon_path(mood->mood);
		purple_request_field_list_add_icon(f, _(mood->description),
				path, (gpointer)mood->mood);
		g_free(path);

		if (current_mood && !strcmp(current_mood, mood->mood))
			purple_request_field_list_add_selected(f, _(mood->description));
	}
	purple_request_field_group_add_field(g, f);

	purple_request_fields_add_group(fields, g);

	/* if the connection allows setting a mood message */
	if (gc && (gc->flags & PURPLE_CONNECTION_SUPPORT_MOOD_MESSAGES)) {
		g = purple_request_field_group_new(NULL);
		f = purple_request_field_string_new("text",
		    _("Message (optional)"), NULL, FALSE);
		purple_request_field_group_add_field(g, f);
		purple_request_fields_add_group(fields, g);
	}

	purple_request_fields(gc, _("Edit User Mood"), _("Edit User Mood"),
                              NULL, fields,
                              _("OK"), G_CALLBACK(edit_mood_cb),
                              _("Cancel"), NULL,
                              gc ? purple_connection_get_account(gc) : NULL,
                              NULL, NULL, gc);

	g_free(global_moods);
}

static void
set_mood_show(void)
{
	set_mood_cb(NULL, NULL);
}

/***************************************************
 *            Crap                                 *
 ***************************************************/
static GtkItemFactoryEntry blist_menu[] =
{
	/* Buddies menu */
	{ N_("/_Buddies"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Buddies/New Instant _Message..."), "<CTL>M", pidgin_dialogs_im, 0, "<StockItem>", PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW },
	{ N_("/Buddies/Join a _Chat..."), "<CTL>C", pidgin_blist_joinchat_show, 0, "<StockItem>", PIDGIN_STOCK_CHAT },
	{ N_("/Buddies/Get User _Info..."), "<CTL>I", pidgin_dialogs_info, 0, "<StockItem>", PIDGIN_STOCK_TOOLBAR_USER_INFO },
	{ N_("/Buddies/View User _Log..."), "<CTL>L", pidgin_dialogs_log, 0, "<Item>", NULL },
	{ "/Buddies/sep1", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/Sh_ow"), NULL, NULL, 0, "<Branch>", NULL},
	{ N_("/Buddies/Show/_Offline Buddies"), NULL, pidgin_blist_edit_mode_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show/_Empty Groups"), NULL, pidgin_blist_show_empty_groups_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show/Buddy _Details"), NULL, pidgin_blist_buddy_details_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show/Idle _Times"), NULL, pidgin_blist_show_idle_time_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/Show/_Protocol Icons"), NULL, pidgin_blist_show_protocol_icons_cb, 1, "<CheckItem>", NULL },
	{ N_("/Buddies/_Sort Buddies"), NULL, NULL, 0, "<Branch>", NULL },
	{ "/Buddies/sep2", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/_Add Buddy..."), "<CTL>B", pidgin_blist_add_buddy_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add C_hat..."), NULL, pidgin_blist_add_chat_cb, 0, "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Buddies/Add _Group..."), NULL, purple_blist_request_add_group, 0, "<StockItem>", GTK_STOCK_ADD },
	{ "/Buddies/sep3", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Buddies/_Quit"), "<CTL>Q", purple_core_quit, 0, "<StockItem>", GTK_STOCK_QUIT },

	/* Accounts menu */
	{ N_("/_Accounts"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Accounts/Manage Accounts"), "<CTL>A", pidgin_accounts_window_show, 0, "<Item>", NULL },

	/* Tools */
	{ N_("/_Tools"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Tools/Buddy _Pounces"), NULL, pidgin_pounces_manager_show, 1, "<Item>", NULL },
	{ N_("/Tools/_Certificates"), NULL, pidgin_certmgr_show, 0, "<Item>", NULL },
	{ N_("/Tools/Custom Smile_ys"), "<CTL>Y", pidgin_smiley_manager_show, 0, "<StockItem>", PIDGIN_STOCK_TOOLBAR_SMILEY },
	{ N_("/Tools/Plu_gins"), "<CTL>U", pidgin_plugin_dialog_show, 2, "<StockItem>", PIDGIN_STOCK_TOOLBAR_PLUGINS },
	{ N_("/Tools/Pr_eferences"), "<CTL>P", pidgin_prefs_show, 0, "<StockItem>", GTK_STOCK_PREFERENCES },
	{ N_("/Tools/Pr_ivacy"), NULL, pidgin_privacy_dialog_show, 0, "<Item>", NULL },
	{ N_("/Tools/Set _Mood"), "<CTL>M", set_mood_show, 0, "<Item>", NULL },
	{ "/Tools/sep2", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Tools/_File Transfers"), "<CTL>T", pidgin_xfer_dialog_show, 0, "<StockItem>", PIDGIN_STOCK_TOOLBAR_TRANSFER },
	{ N_("/Tools/R_oom List"), NULL, pidgin_roomlist_dialog_show, 0, "<Item>", NULL },
	{ N_("/Tools/System _Log"), NULL, gtk_blist_show_systemlog_cb, 3, "<Item>", NULL },
	{ "/Tools/sep3", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Tools/Mute _Sounds"), NULL, pidgin_blist_mute_sounds_cb, 0, "<CheckItem>", NULL },
	/* Help */
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Help/Online _Help"), "F1", gtk_blist_show_onlinehelp_cb, 0, "<StockItem>", GTK_STOCK_HELP },
	{ "/Help/sep1", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Help/_Build Information"), NULL, pidgin_dialogs_buildinfo, 0, "<Item>", NULL },
	{ N_("/Help/_Debug Window"), NULL, toggle_debug, 0, "<Item>", NULL },
	{ N_("/Help/De_veloper Information"), NULL, pidgin_dialogs_developers, 0, "<Item>", NULL },
	{ N_("/Help/_Translator Information"), NULL, pidgin_dialogs_translators, 0, "<Item>", NULL },
	{ "/Help/sep2", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Help/_About"), NULL, pidgin_dialogs_about, 4,  "<StockItem>", GTK_STOCK_ABOUT },
};

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full)
{
	GString *str = g_string_new("");
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info = NULL;
	char *tmp;

	if (PURPLE_BLIST_NODE_IS_CHAT(node))
	{
		PurpleChat *chat;
		GList *connections;
		GList *cur;
		struct proto_chat_entry *pce;
		char *name, *value;
		PurpleConversation *conv;
		PidginBlistNode *bnode = node->ui_data;

		chat = (PurpleChat *)node;
		prpl = purple_find_prpl(purple_account_get_protocol_id(chat->account));
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		connections = purple_connections_get_all();
		if (connections && connections->next)
		{
			tmp = g_markup_escape_text(chat->account->username, -1);
			g_string_append_printf(str, _("<b>Account:</b> %s"), tmp);
			g_free(tmp);
		}

		if (bnode && bnode->conv.conv) {
			conv = bnode->conv.conv;
		} else {
			char *chat_name;
			if (prpl_info && prpl_info->get_chat_name)
				chat_name = prpl_info->get_chat_name(chat->components);
			else
				chat_name = g_strdup(purple_chat_get_name(chat));

			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, chat_name,
					chat->account);
			g_free(chat_name);
		}

		if (conv && !purple_conv_chat_has_left(PURPLE_CONV_CHAT(conv))) {
			g_string_append_printf(str, _("\n<b>Occupants:</b> %d"),
					g_list_length(purple_conv_chat_get_users(PURPLE_CONV_CHAT(conv))));

			if (prpl_info && (prpl_info->options & OPT_PROTO_CHAT_TOPIC)) {
				const char *chattopic = purple_conv_chat_get_topic(PURPLE_CONV_CHAT(conv));
				char *topic = chattopic ? g_markup_escape_text(chattopic, -1) : NULL;
				g_string_append_printf(str, _("\n<b>Topic:</b> %s"), topic ? topic : _("(no topic set)"));
				g_free(topic);
			}
		}

		if (prpl_info && prpl_info->chat_info != NULL)
			cur = prpl_info->chat_info(chat->account->gc);
		else
			cur = NULL;

		while (cur != NULL)
		{
			pce = cur->data;

			if (!pce->secret && (!pce->required &&
				g_hash_table_lookup(chat->components, pce->identifier) == NULL))
			{
				tmp = purple_text_strip_mnemonic(pce->label);
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
			cur = g_list_delete_link(cur, cur);
		}
	}
	else if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		/* NOTE: THIS FUNCTION IS NO LONGER CALLED FOR CONTACTS.
		 * It is only called by create_tip_for_node(), and create_tip_for_node() is never called for a contact.
		 */
		PurpleContact *c;
		PurpleBuddy *b;
		PurplePresence *presence;
		PurpleNotifyUserInfo *user_info;
		GList *connections;
		char *tmp;
		time_t idle_secs, signon;

		if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		{
			c = (PurpleContact *)node;
			b = purple_contact_get_priority_buddy(c);
		}
		else
		{
			b = (PurpleBuddy *)node;
			c = purple_buddy_get_contact(b);
		}

		prpl = purple_find_prpl(purple_account_get_protocol_id(b->account));
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		presence = purple_buddy_get_presence(b);
		user_info = purple_notify_user_info_new();

		/* Account */
		connections = purple_connections_get_all();
		if (full && connections && connections->next)
		{
			tmp = g_markup_escape_text(purple_account_get_username(
									   purple_buddy_get_account(b)), -1);
			purple_notify_user_info_add_pair(user_info, _("Account"), tmp);
			g_free(tmp);
		}

		/* Alias */
		/* If there's not a contact alias, the node is being displayed with
		 * this alias, so there's no point in showing it in the tooltip. */
		if (full && c && b->alias != NULL && b->alias[0] != '\0' &&
		    (c->alias != NULL && c->alias[0] != '\0') &&
		    strcmp(c->alias, b->alias) != 0)
		{
			tmp = g_markup_escape_text(b->alias, -1);
			purple_notify_user_info_add_pair(user_info, _("Buddy Alias"), tmp);
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
			purple_notify_user_info_add_pair(user_info, _("Nickname"), tmp);
			g_free(tmp);
		}

		/* Logged In */
		signon = purple_presence_get_login_time(presence);
		if (full && PURPLE_BUDDY_IS_ONLINE(b) && signon > 0)
		{
			if (signon > time(NULL)) {
				/*
				 * They signed on in the future?!  Our local clock
				 * must be wrong, show the actual date instead of
				 * "4 days", etc.
				 */
				tmp = g_strdup(purple_date_format_long(localtime(&signon)));
			} else
				tmp = purple_str_seconds_to_string(time(NULL) - signon);
			purple_notify_user_info_add_pair(user_info, _("Logged In"), tmp);
			g_free(tmp);
		}

		/* Idle */
		if (purple_presence_is_idle(presence))
		{
			idle_secs = purple_presence_get_idle_time(presence);
			if (idle_secs > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - idle_secs);
				purple_notify_user_info_add_pair(user_info, _("Idle"), tmp);
				g_free(tmp);
			}
		}

		/* Last Seen */
		if (full && c && !PURPLE_BUDDY_IS_ONLINE(b))
		{
			struct _pidgin_blist_node *gtknode = ((PurpleBlistNode *)c)->ui_data;
			PurpleBlistNode *bnode;
			int lastseen = 0;

			if (gtknode && (!gtknode->contact_expanded || PURPLE_BLIST_NODE_IS_CONTACT(node)))
			{
				/* We're either looking at a buddy for a collapsed contact or
				 * an expanded contact itself so we show the most recent
				 * (largest) last_seen time for any of the buddies under
				 * the contact. */
				for (bnode = ((PurpleBlistNode *)c)->child ; bnode != NULL ; bnode = bnode->next)
				{
					int value = purple_blist_node_get_int(bnode, "last_seen");
					if (value > lastseen)
						lastseen = value;
				}
			}
			else
			{
				/* We're dealing with a buddy under an expanded contact,
				 * so we show the last_seen time for the buddy. */
				lastseen = purple_blist_node_get_int(&b->node, "last_seen");
			}

			if (lastseen > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - lastseen);
				purple_notify_user_info_add_pair(user_info, _("Last Seen"), tmp);
				g_free(tmp);
			}
		}


		/* Offline? */
		/* FIXME: Why is this status special-cased by the core? --rlaager
		 * FIXME: Alternatively, why not have the core do all of them? --rlaager */
		if (!PURPLE_BUDDY_IS_ONLINE(b)) {
			purple_notify_user_info_add_pair(user_info, _("Status"), _("Offline"));
		}

		if (purple_account_is_connected(b->account) &&
				prpl_info && prpl_info->tooltip_text)
		{
			/* Additional text from the PRPL */
			prpl_info->tooltip_text(b, user_info, full);
		}

		/* These are Easter Eggs.  Patches to remove them will be rejected. */
		if (!g_ascii_strcasecmp(b->name, "robflynn"))
			purple_notify_user_info_add_pair(user_info, _("Description"), _("Spooky"));
		if (!g_ascii_strcasecmp(b->name, "seanegn"))
			purple_notify_user_info_add_pair(user_info, _("Status"), _("Awesome"));
		if (!g_ascii_strcasecmp(b->name, "chipx86"))
			purple_notify_user_info_add_pair(user_info, _("Status"), _("Rockin'"));

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);

		purple_notify_user_info_destroy(user_info);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		gint count;
		PurpleGroup *group = (PurpleGroup*)node;
		PurpleNotifyUserInfo *user_info;

		user_info = purple_notify_user_info_new();

		count = purple_blist_get_group_online_count(group);

		if (count != 0) {
			/* Online buddies in group */
			tmp = g_strdup_printf("%d", count);
			purple_notify_user_info_add_pair(user_info,
			                                 _("Online Buddies"),
			                                 tmp);
			g_free(tmp);
		}
		count = 0;

		count = purple_blist_get_group_size(group, FALSE);
		if (count != 0) {
			/* Total buddies (from online accounts) in group */
			tmp = g_strdup_printf("%d", count);
			purple_notify_user_info_add_pair(user_info,
			                                 _("Total Buddies"),
			                                 tmp);
			g_free(tmp);
		}
		count = 0;

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);

		purple_notify_user_info_destroy(user_info);
	}

	purple_signal_emit(pidgin_blist_get_handle(), "drawing-tooltip",
	                   node, str, full);

	return g_string_free(str, FALSE);
}

static GHashTable *cached_emblems;

static void _cleanup_cached_emblem(gpointer data, GObject *obj) {
	g_hash_table_remove(cached_emblems, data);
}

static GdkPixbuf * _pidgin_blist_get_cached_emblem(gchar *path) {
	GdkPixbuf *pb = g_hash_table_lookup(cached_emblems, path);

	if (pb != NULL) {
		/* The caller gets a reference */
		g_object_ref(pb);
		g_free(path);
	} else {
		pb = gdk_pixbuf_new_from_file(path, NULL);
		if (pb != NULL) {
			/* We don't want to own a ref to the pixbuf, but we need to keep clean up. */
			/* I'm not sure if it would be better to just keep our ref and not let the emblem ever be destroyed */
			g_object_weak_ref(G_OBJECT(pb), _cleanup_cached_emblem, path);
			g_hash_table_insert(cached_emblems, path, pb);
		} else
			g_free(path);
	}

	return pb;
}

GdkPixbuf *
pidgin_blist_get_emblem(PurpleBlistNode *node)
{
	PurpleBuddy *buddy = NULL;
	struct _pidgin_blist_node *gtknode = node->ui_data;
	struct _pidgin_blist_node *gtkbuddynode = NULL;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;
	const char *name = NULL;
	char *filename, *path;
	PurplePresence *p = NULL;
	PurpleStatus *tune;

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
			gtkbuddynode = ((PurpleBlistNode*)buddy)->ui_data;
		}
	} else if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		gtkbuddynode = node->ui_data;
		p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			/* This emblem comes from the small emoticon set now,
			 * to reduce duplication. */
			path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emotes",
						"small", "mobile.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}

		if (((struct _pidgin_blist_node*)(node->parent->ui_data))->contact_expanded) {
			if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"))
				return NULL;
			return pidgin_create_prpl_icon(((PurpleBuddy*)node)->account, PIDGIN_PRPL_ICON_SMALL);
		}
	} else {
		return NULL;
	}

	g_return_val_if_fail(buddy != NULL, NULL);

	if (!purple_privacy_check(buddy->account, purple_buddy_get_name(buddy))) {
		path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emblems", "16", "blocked.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	/* If we came through the contact code flow above, we didn't need
	 * to get the presence until now. */
	if (p == NULL)
		p = purple_buddy_get_presence(buddy);

	if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emotes", "small", "mobile.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	tune = purple_presence_get_status(p, "tune");
	if (tune && purple_status_is_active(tune)) {
		/* Only in MSN.
		 * TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "game") != NULL) {
			path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emblems", "16", "game.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* Only in MSN.
		 * TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "office") != NULL) {
			path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emblems", "16", "office.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* Regular old "tune" is the only one in all protocols. */
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emotes", "small", "music.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	prpl = purple_find_prpl(purple_account_get_protocol_id(buddy->account));
	if (!prpl)
		return NULL;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info && prpl_info->list_emblem)
		name = prpl_info->list_emblem(buddy);

	if (name == NULL) {
		PurpleStatus *status;

		if (!purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOOD))
			return NULL;

		status = purple_presence_get_status(p, "mood");
		name = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);
		
		if (!(name && *name))
			return NULL;

		path = get_mood_icon_path(name);
	} else {
		filename = g_strdup_printf("%s.png", name);
		path = g_build_filename(DATADIR, "pixmaps", "pidgin", "emblems", "16", filename, NULL);
		g_free(filename);
	}

	/* _pidgin_blist_get_cached_emblem() assumes ownership of path */
	return _pidgin_blist_get_cached_emblem(path);
}


GdkPixbuf *
pidgin_blist_get_status_icon(PurpleBlistNode *node, PidginStatusIconSize size)
{
	GdkPixbuf *ret;
	const char *protoname = NULL;
	const char *icon = NULL;
	struct _pidgin_blist_node *gtknode = node->ui_data;
	struct _pidgin_blist_node *gtkbuddynode = NULL;
	PurpleBuddy *buddy = NULL;
	PurpleChat *chat = NULL;
	GtkIconSize icon_size = gtk_icon_size_from_name((size == PIDGIN_STATUS_ICON_LARGE) ? PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL :
											 PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC);

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
			if (buddy != NULL)
				gtkbuddynode = ((PurpleBlistNode*)buddy)->ui_data;
		}
	} else if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		gtkbuddynode = node->ui_data;
	} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
		chat = (PurpleChat*)node;
	} else {
		return NULL;
	}

	if(buddy || chat) {
		PurpleAccount *account;
		PurplePlugin *prpl;
		PurplePluginProtocolInfo *prpl_info;

		if(buddy)
			account = buddy->account;
		else
			account = chat->account;

		prpl = purple_find_prpl(purple_account_get_protocol_id(account));
		if(!prpl)
			return NULL;

		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if(prpl_info && prpl_info->list_icon) {
			protoname = prpl_info->list_icon(account, buddy);
		}
	}

	if(buddy) {
	  	PurpleConversation *conv = find_conversation_with_buddy(buddy);
		PurplePresence *p;
		gboolean trans;

		if(conv != NULL) {
			PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
			if (gtkconv == NULL && size == PIDGIN_STATUS_ICON_SMALL) {
				PidginBlistNode *ui = buddy->node.ui_data;
				if (ui == NULL || (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE))
					return gtk_widget_render_icon (GTK_WIDGET(gtkblist->treeview),
							PIDGIN_STOCK_STATUS_MESSAGE, icon_size, "GtkTreeView");
			}
		}

		p = purple_buddy_get_presence(buddy);
		trans = purple_presence_is_idle(p);

		if (PURPLE_BUDDY_IS_ONLINE(buddy) && gtkbuddynode && gtkbuddynode->recent_signonoff)
			icon = PIDGIN_STOCK_STATUS_LOGIN;
		else if (gtkbuddynode && gtkbuddynode->recent_signonoff)
			icon = PIDGIN_STOCK_STATUS_LOGOUT;
		else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE))
			if (trans)
				icon = PIDGIN_STOCK_STATUS_BUSY_I;
			else
				icon = PIDGIN_STOCK_STATUS_BUSY;
		else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY))
			if (trans)
				icon = PIDGIN_STOCK_STATUS_AWAY_I;
		 	else
				icon = PIDGIN_STOCK_STATUS_AWAY;
		else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY))
			if (trans)
				icon = PIDGIN_STOCK_STATUS_XA_I;
			else
				icon = PIDGIN_STOCK_STATUS_XA;
		else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE))
			icon = PIDGIN_STOCK_STATUS_OFFLINE;
		else if (trans)
			icon = PIDGIN_STOCK_STATUS_AVAILABLE_I;
		else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_INVISIBLE))
			icon = PIDGIN_STOCK_STATUS_INVISIBLE;
		else
			icon = PIDGIN_STOCK_STATUS_AVAILABLE;
	} else if (chat) {
		icon = PIDGIN_STOCK_STATUS_CHAT;
	} else {
		icon = PIDGIN_STOCK_STATUS_PERSON;
	}

	ret = gtk_widget_render_icon (GTK_WIDGET(gtkblist->treeview), icon,
			icon_size, "GtkTreeView");
	return ret;
}

static const char *
theme_font_get_color_default(PidginThemeFont *font, const char *def)
{
	const char *ret;
	if (!font || !(ret = pidgin_theme_font_get_color_describe(font)))
		ret = def;
	return ret;
}

static const char *
theme_font_get_face_default(PidginThemeFont *font, const char *def)
{
	const char *ret;
	if (!font || !(ret = pidgin_theme_font_get_font_face(font)))
		ret = def;
	return ret;
}

gchar *
pidgin_blist_get_name_markup(PurpleBuddy *b, gboolean selected, gboolean aliased)
{
	const char *name, *name_color, *name_font, *status_color, *status_font;
	char *text = NULL;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleContact *contact;
	PurplePresence *presence;
	struct _pidgin_blist_node *gtkcontactnode = NULL;
	char *idletime = NULL, *statustext = NULL, *nametext = NULL;
	PurpleConversation *conv = find_conversation_with_buddy(b);
	gboolean hidden_conv = FALSE;
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	PidginThemeFont *statusfont = NULL, *namefont = NULL;
	PidginBlistTheme *theme;

	if (conv != NULL) {
		PidginBlistNode *ui = b->node.ui_data;
		if (ui) {
			if (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE)
				hidden_conv = TRUE;
		} else {
			if (PIDGIN_CONVERSATION(conv) == NULL)
				hidden_conv = TRUE;
		}
	}

	/* XXX Good luck cleaning up this crap */
	contact = PURPLE_CONTACT(PURPLE_BLIST_NODE(b)->parent);
	if(contact)
		gtkcontactnode = purple_blist_node_get_ui_data(PURPLE_BLIST_NODE(contact));

	/* Name */
	if (gtkcontactnode && !gtkcontactnode->contact_expanded && contact->alias)
		name = contact->alias;
	else
		name = purple_buddy_get_alias(b);

	/* Raise a contact pre-draw signal here.  THe callback will return an
	 * escaped version of the name. */
	nametext = purple_signal_emit_return_1(pidgin_blist_get_handle(), "drawing-buddy", b);

	if(!nametext)
		nametext = g_markup_escape_text(name, strlen(name));

	presence = purple_buddy_get_presence(b);

	/* Name is all that is needed */
	if (!aliased || biglist) {

		/* Status Info */
		prpl = purple_find_prpl(purple_account_get_protocol_id(b->account));

		if (prpl != NULL)
			prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl_info && prpl_info->status_text && b->account->gc) {
			char *tmp = prpl_info->status_text(b);
			const char *end;

			if(tmp && !g_utf8_validate(tmp, -1, &end)) {
				char *new = g_strndup(tmp,
						g_utf8_pointer_to_offset(tmp, end));
				g_free(tmp);
				tmp = new;
			}
			if(tmp) {
				g_strdelimit(tmp, "\n", ' ');
				purple_str_strip_char(tmp, '\r');
			}
			statustext = tmp;
		}

		if(!purple_presence_is_online(presence) && !statustext)
				statustext = g_strdup(_("Offline"));

		/* Idle Text */
		if (purple_presence_is_idle(presence) && purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time")) {
			time_t idle_secs = purple_presence_get_idle_time(presence);

			if (idle_secs > 0) {
				int iday, ihrs, imin;
				time_t t;

				time(&t);
				iday = (t - idle_secs) / (24 * 60 * 60);
				ihrs = ((t - idle_secs) / 60 / 60) % 24;
				imin = ((t - idle_secs) / 60) % 60;

				if (iday)
					idletime = g_strdup_printf(_("Idle %dd %dh %02dm"), iday, ihrs, imin);
				else if (ihrs)
					idletime = g_strdup_printf(_("Idle %dh %02dm"), ihrs, imin);
				else
					idletime = g_strdup_printf(_("Idle %dm"), imin);

			} else
				idletime = g_strdup(_("Idle"));
		}
	}

	/* choose the colors of the text */
	theme = pidgin_blist_get_theme();
	name_color = NULL;

	if (theme) {
		if (purple_presence_is_idle(presence)) {
			namefont = statusfont = pidgin_blist_theme_get_idle_text_info(theme);
			name_color = "dim grey";
		} else if (!purple_presence_is_online(presence)) {
			namefont = pidgin_blist_theme_get_offline_text_info(theme);
			name_color = "dim grey";
			statusfont = pidgin_blist_theme_get_status_text_info(theme);
		} else if (purple_presence_is_available(presence)) {
			namefont = pidgin_blist_theme_get_online_text_info(theme);
			statusfont = pidgin_blist_theme_get_status_text_info(theme);
		} else {
			namefont = pidgin_blist_theme_get_away_text_info(theme);
			statusfont = pidgin_blist_theme_get_status_text_info(theme);
		}
	} else {
		if (!selected
				&& (purple_presence_is_idle(presence)
							|| !purple_presence_is_online(presence)))
		{
			name_color = "dim grey";
		}
	}

	name_color = theme_font_get_color_default(namefont, name_color);
	name_font = theme_font_get_face_default(namefont, "");

	status_color = theme_font_get_color_default(statusfont, "dim grey");
	status_font = theme_font_get_face_default(statusfont, "");

	if (aliased && selected) {
		if (theme) {
			name_color = "black";
			status_color = "black";
		} else {
			name_color = NULL;
			status_color = NULL;
		}
	}

	if (hidden_conv) {
		char *tmp = nametext;
		nametext = g_strdup_printf("<b>%s</b>", tmp);
		g_free(tmp);
	}

	/* Put it all together */
	if ((!aliased || biglist) && (statustext || idletime)) {
		/* using <span size='smaller'> breaks the status, so it must be seperated into <small><span>*/
		if (name_color) {
			text = g_strdup_printf("<span font_desc='%s' foreground='%s'>%s</span>\n"
				 		"<small><span font_desc='%s' foreground='%s'>%s%s%s</span></small>",
						name_font, name_color, nametext, status_font, status_color,
						idletime != NULL ? idletime : "",
				    		(idletime != NULL && statustext != NULL) ? " - " : "",
				    		statustext != NULL ? statustext : "");
		} else if (status_color) {
			text = g_strdup_printf("<span font_desc='%s'>%s</span>\n"
				 		"<small><span font_desc='%s' foreground='%s'>%s%s%s</span></small>",
						name_font, nametext, status_font, status_color,
						idletime != NULL ? idletime : "",
				    		(idletime != NULL && statustext != NULL) ? " - " : "",
				    		statustext != NULL ? statustext : "");
		} else {
			text = g_strdup_printf("<span font_desc='%s'>%s</span>\n"
				 		"<small><span font_desc='%s'>%s%s%s</span></small>",
						name_font, nametext, status_font,
						idletime != NULL ? idletime : "",
				    		(idletime != NULL && statustext != NULL) ? " - " : "",
				    		statustext != NULL ? statustext : "");
		}
	} else {
		if (name_color) {
			text = g_strdup_printf("<span font_desc='%s' color='%s'>%s</span>", 
				name_font, name_color, nametext);
		} else {
			text = g_strdup_printf("<span font_desc='%s'>%s</span>", name_font,
				nametext);
		}
	}
	g_free(nametext);
	g_free(statustext);
	g_free(idletime);

	if (hidden_conv) {
		char *tmp = text;
		text = g_strdup_printf("<b>%s</b>", tmp);
		g_free(tmp);
	}

	return text;
}

static void pidgin_blist_restore_position(void)
{
	int blist_x, blist_y, blist_width, blist_height;

	blist_width = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width");

	/* if the window exists, is hidden, we're saving positions, and the
	 * position is sane... */
	if (gtkblist && gtkblist->window &&
		!GTK_WIDGET_VISIBLE(gtkblist->window) && blist_width != 0) {

		blist_x      = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/x");
		blist_y      = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/y");
		blist_height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/height");

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
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized"))
			gtk_window_maximize(GTK_WINDOW(gtkblist->window));
	}
}

static gboolean pidgin_blist_refresh_timer(PurpleBuddyList *list)
{
	PurpleBlistNode *gnode, *cnode;

	if (gtk_blist_visibility == GDK_VISIBILITY_FULLY_OBSCURED
			|| !GTK_WIDGET_VISIBLE(gtkblist->window))
		return TRUE;

	for(gnode = list->root; gnode; gnode = gnode->next) {
		if(!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
				PurpleBuddy *buddy;

				buddy = purple_contact_get_priority_buddy((PurpleContact*)cnode);

				if (buddy &&
						purple_presence_is_idle(purple_buddy_get_presence(buddy)))
					pidgin_blist_update_contact(list, (PurpleBlistNode*)buddy);
			}
		}
	}

	/* keep on going */
	return TRUE;
}

static void pidgin_blist_hide_node(PurpleBuddyList *list, PurpleBlistNode *node, gboolean update)
{
	struct _pidgin_blist_node *gtknode = (struct _pidgin_blist_node *)node->ui_data;
	GtkTreeIter iter;

	if (!gtknode || !gtknode->row || !gtkblist)
		return;

	if(gtkblist->selected_node == node)
		gtkblist->selected_node = NULL;
	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(update && (PURPLE_BLIST_NODE_IS_CONTACT(node) ||
			PURPLE_BLIST_NODE_IS_BUDDY(node) || PURPLE_BLIST_NODE_IS_CHAT(node))) {
			pidgin_blist_update(list, node->parent);
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
update_menu_bar(PidginBuddyList *gtkblist)
{
	GtkWidget *widget;
	gboolean sensitive;
	int i;

	g_return_if_fail(gtkblist != NULL);

	pidgin_blist_update_accounts_menu();

	sensitive = (purple_connections_get_all() != NULL);

	for (i = 0; i < require_connection_size; i++)
	{
		widget = gtk_item_factory_get_widget(gtkblist->ift, require_connection[i]);
		gtk_widget_set_sensitive(widget, sensitive);
	}

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Join a Chat..."));
	gtk_widget_set_sensitive(widget, pidgin_blist_joinchat_is_showable());

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Add Chat..."));
	gtk_widget_set_sensitive(widget, pidgin_blist_joinchat_is_showable());

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Privacy"));
	gtk_widget_set_sensitive(widget, sensitive);

	widget = gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Room List"));
	gtk_widget_set_sensitive(widget, pidgin_roomlist_is_showable());
}

static void
sign_on_off_cb(PurpleConnection *gc, PurpleBuddyList *blist)
{
	PidginBuddyList *gtkblist = PIDGIN_BLIST(blist);

	update_menu_bar(gtkblist);
}

static void
plugin_changed_cb(PurplePlugin *p, gpointer *data)
{
	pidgin_blist_update_plugin_actions();
}

static void
unseen_conv_menu(void)
{
	static GtkWidget *menu = NULL;
	GList *convs = NULL;
	GList *chats, *ims;

	if (menu) {
		gtk_widget_destroy(menu);
		menu = NULL;
	}

	ims = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_IM,
				PIDGIN_UNSEEN_TEXT, FALSE, 0);

	chats = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_CHAT,
				PIDGIN_UNSEEN_NICK, FALSE, 0);

	if(ims && chats)
		convs = g_list_concat(ims, chats);
	else if(ims && !chats)
		convs = ims;
	else if(!ims && chats)
		convs = chats;

	if (!convs)
		/* no conversations added, don't show the menu */
		return;

	menu = gtk_menu_new();

	pidgin_conversations_fill_menu(menu, convs);
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
			convs = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_IM,
							PIDGIN_UNSEEN_TEXT, FALSE, 1);

			if(!convs)
				convs = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_CHAT,
								PIDGIN_UNSEEN_NICK, FALSE, 1);
			if (convs) {
				pidgin_conv_present_conversation((PurpleConversation*)convs->data);
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
conversation_updated_cb(PurpleConversation *conv, PurpleConvUpdateType type,
                        PidginBuddyList *gtkblist)
{
	GList *convs = NULL;
	GList *ims, *chats;
	GList *l = NULL;

	if (type != PURPLE_CONV_UPDATE_UNSEEN)
		return;

	if(conv->account != NULL && conv->name != NULL) {
		PurpleBuddy *buddy = purple_find_buddy(conv->account, conv->name);
		if(buddy != NULL)
			pidgin_blist_update_buddy(NULL, (PurpleBlistNode *)buddy, TRUE);
	}

	if (gtkblist->menutrayicon) {
		gtk_widget_destroy(gtkblist->menutrayicon);
		gtkblist->menutrayicon = NULL;
	}

	ims = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_IM,
				PIDGIN_UNSEEN_TEXT, FALSE, 0);

	chats = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_CHAT,
				PIDGIN_UNSEEN_NICK, FALSE, 0);

	if(ims && chats)
		convs = g_list_concat(ims, chats);
	else if(ims && !chats)
		convs = ims;
	else if(!ims && chats)
		convs = chats;

	if (convs) {
		GtkWidget *img = NULL;
		GString *tooltip_text = NULL;

		tooltip_text = g_string_new("");
		l = convs;
		while (l != NULL) {
			int count = 0;
			PidginConversation *gtkconv = PIDGIN_CONVERSATION((PurpleConversation *)l->data);

			if(gtkconv)
				count = gtkconv->unseen_count;
			else if(purple_conversation_get_data(l->data, "unseen-count"))
				count = GPOINTER_TO_INT(purple_conversation_get_data(l->data, "unseen-count"));

			g_string_append_printf(tooltip_text,
					ngettext("%d unread message from %s\n", "%d unread messages from %s\n", count),
					count, purple_conversation_get_title(l->data));
			l = l->next;
		}
		if(tooltip_text->len > 0) {
			/* get rid of the last newline */
			g_string_truncate(tooltip_text, tooltip_text->len -1);
			img = gtk_image_new_from_stock(PIDGIN_STOCK_TOOLBAR_PENDING,
							gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));

			gtkblist->menutrayicon = gtk_event_box_new();
			gtk_container_add(GTK_CONTAINER(gtkblist->menutrayicon), img);
			gtk_widget_show(img);
			gtk_widget_show(gtkblist->menutrayicon);
			g_signal_connect(G_OBJECT(gtkblist->menutrayicon), "button-press-event", G_CALLBACK(menutray_press_cb), NULL);

			pidgin_menu_tray_append(PIDGIN_MENU_TRAY(gtkblist->menutray), gtkblist->menutrayicon, tooltip_text->str);
		}
		g_string_free(tooltip_text, TRUE);
		g_list_free(convs);
	}
}

static void
conversation_deleting_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	conversation_updated_cb(conv, PURPLE_CONV_UPDATE_UNSEEN, gtkblist);
}

static void
conversation_deleted_update_ui_cb(PurpleConversation *conv, struct _pidgin_blist_node *ui)
{
	if (ui->conv.conv != conv)
		return;
	ui->conv.conv = NULL;
	ui->conv.flags = 0;
	ui->conv.last_message = 0;
}

static void
written_msg_update_ui_cb(PurpleAccount *account, const char *who, const char *message,
		PurpleConversation *conv, PurpleMessageFlags flag, PurpleBlistNode *node)
{
	PidginBlistNode *ui = node->ui_data;
	if (ui->conv.conv != conv || !pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv)) ||
			!(flag & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)))
		return;
	ui->conv.flags |= PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE;
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT
			&& (flag & PURPLE_MESSAGE_NICK))
		ui->conv.flags |= PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK;

	ui->conv.last_message = time(NULL);    /* XXX: for lack of better data */
	pidgin_blist_update(purple_get_blist(), node);
}

static void
displayed_msg_update_ui_cb(PidginConversation *gtkconv, PurpleBlistNode *node)
{
	PidginBlistNode *ui = node->ui_data;
	if (ui->conv.conv != gtkconv->active_conv)
		return;
	ui->conv.flags &= ~(PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE |
	                    PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK);
	pidgin_blist_update(purple_get_blist(), node);
}

static void
conversation_created_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	switch (conv->type) {
		case PURPLE_CONV_TYPE_IM:
			{
				GSList *buddies = purple_find_buddies(conv->account, conv->name);
				while (buddies) {
					PurpleBlistNode *buddy = buddies->data;
					struct _pidgin_blist_node *ui = buddy->ui_data;
					buddies = g_slist_delete_link(buddies, buddies);
					if (!ui)
						continue;
					ui->conv.conv = conv;
					ui->conv.flags = 0;
					ui->conv.last_message = 0;
					purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
							ui, PURPLE_CALLBACK(conversation_deleted_update_ui_cb), ui);
					purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg",
							ui, PURPLE_CALLBACK(written_msg_update_ui_cb), buddy);
					purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed",
							ui, PURPLE_CALLBACK(displayed_msg_update_ui_cb), buddy);
				}
			}
			break;
		case PURPLE_CONV_TYPE_CHAT:
			{
				PurpleChat *chat = purple_blist_find_chat(conv->account, conv->name);
				struct _pidgin_blist_node *ui;
				if (!chat)
					break;
				ui = chat->node.ui_data;
				if (!ui)
					break;
				ui->conv.conv = conv;
				ui->conv.flags = 0;
				ui->conv.last_message = 0;
				purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
						ui, PURPLE_CALLBACK(conversation_deleted_update_ui_cb), ui);
				purple_signal_connect(purple_conversations_get_handle(), "wrote-chat-msg",
						ui, PURPLE_CALLBACK(written_msg_update_ui_cb), chat);
				purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed",
						ui, PURPLE_CALLBACK(displayed_msg_update_ui_cb), chat);
			}
			break;
		default:
			break;
	}
}

/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/

static void pidgin_blist_new_list(PurpleBuddyList *blist)
{
	PidginBuddyList *gtkblist;

	gtkblist = g_new0(PidginBuddyList, 1);
	gtkblist->connection_errors = g_hash_table_new_full(g_direct_hash,
												g_direct_equal, NULL, g_free);
	gtkblist->priv = g_new0(PidginBuddyListPrivate, 1);

	blist->ui_data = gtkblist;
}

static void pidgin_blist_new_node(PurpleBlistNode *node)
{
	node->ui_data = g_new0(struct _pidgin_blist_node, 1);
}

gboolean pidgin_blist_node_is_contact_expanded(PurpleBlistNode *node)
{
	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		node = node->parent;
		if (node == NULL)
			return FALSE;
	}

	g_return_val_if_fail(PURPLE_BLIST_NODE_IS_CONTACT(node), FALSE);

	return ((struct _pidgin_blist_node *)node->ui_data)->contact_expanded;
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

void pidgin_blist_setup_sort_methods()
{
	const char *id;

	pidgin_blist_sort_method_reg("none", _("Manually"), sort_method_none);
	pidgin_blist_sort_method_reg("alphabetical", _("Alphabetically"), sort_method_alphabetical);
	pidgin_blist_sort_method_reg("status", _("By status"), sort_method_status);
	pidgin_blist_sort_method_reg("log_size", _("By recent log activity"), sort_method_log_activity);

	id = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/sort_type");
	if (id == NULL) {
		purple_debug_warning("gtkblist", "Sort method was NULL, resetting to alphabetical\n");
		id = "alphabetical";
	}
	pidgin_blist_sort_method_set(id);
}

static void _prefs_change_redo_list(const char *name, PurplePrefType type,
                                    gconstpointer val, gpointer data)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	PurpleBlistNode *node = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	}

	redo_buddy_list(purple_get_blist(), FALSE, FALSE);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gtkblist->treeview));

	if (node)
	{
		struct _pidgin_blist_node *gtknode;
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

static void _prefs_change_sort_method(const char *pref_name, PurplePrefType type,
									  gconstpointer val, gpointer data)
{
	if(!strcmp(pref_name, PIDGIN_PREFS_ROOT "/blist/sort_type"))
		pidgin_blist_sort_method_set(val);
}

static gboolean pidgin_blist_select_notebook_page_cb(gpointer user_data)
{
	PidginBuddyList *gtkblist = (PidginBuddyList *)user_data;
	int errors = 0;
	GList *list = NULL;
	PidginBuddyListPrivate *priv;

	priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);

	/* this is far too ugly thanks to me not wanting to fix #3989 properly right now */
	if (priv->error_scrollbook != NULL) {
		errors = gtk_notebook_get_n_pages(GTK_NOTEBOOK(priv->error_scrollbook->notebook));
	}
	if ((list = purple_accounts_get_all_active()) != NULL || errors) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 1);
		g_list_free(list);
	} else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 0);

	return FALSE;
}

static void pidgin_blist_select_notebook_page(PidginBuddyList *gtkblist)
{
	purple_timeout_add(0, pidgin_blist_select_notebook_page_cb, gtkblist);
}

static void account_modified(PurpleAccount *account, PidginBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	pidgin_blist_select_notebook_page(gtkblist);
	update_menu_bar(gtkblist);
}

static void
account_actions_changed(PurpleAccount *account, gpointer data)
{
	pidgin_blist_update_accounts_menu();
}

static void
account_status_changed(PurpleAccount *account, PurpleStatus *old,
					   PurpleStatus *new, PidginBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	account_modified(account, gtkblist);
}

static gboolean
gtk_blist_window_key_press_cb(GtkWidget *w, GdkEventKey *event, PidginBuddyList *gtkblist)
{
	GtkWidget *widget;

	if (!gtkblist)
		return FALSE;

	/* clear any tooltips */
	pidgin_blist_tooltip_destroy();

	widget = gtk_window_get_focus(GTK_WINDOW(gtkblist->window));

	if (GTK_IS_IMHTML(widget) || GTK_IS_ENTRY(widget)) {
		if (gtk_bindings_activate(GTK_OBJECT(widget), event->keyval, event->state))
			return TRUE;
	}
	return FALSE;
}

static gboolean
headline_box_enter_cb(GtkWidget *widget, GdkEventCrossing *event, PidginBuddyList *gtkblist)
{
	gdk_window_set_cursor(widget->window, gtkblist->hand_cursor);
	return FALSE;
}

static gboolean
headline_box_leave_cb(GtkWidget *widget, GdkEventCrossing *event, PidginBuddyList *gtkblist)
{
	gdk_window_set_cursor(widget->window, gtkblist->arrow_cursor);
	return FALSE;
}

static void
reset_headline(PidginBuddyList *gtkblist)
{
	gtkblist->headline_callback = NULL;
	gtkblist->headline_data = NULL;
	gtkblist->headline_destroy = NULL;
	pidgin_set_urgent(GTK_WINDOW(gtkblist->window), FALSE);
}

static gboolean
headline_click_callback(gpointer unused)
{
	if (gtkblist->headline_callback)
		((GSourceFunc) gtkblist->headline_callback)(gtkblist->headline_data);
	reset_headline(gtkblist);
	return FALSE;
}

static gboolean
headline_close_press_cb(GtkButton *button, PidginBuddyList *gtkblist)
{
	gtk_widget_hide(gtkblist->headline_hbox);
	return FALSE;
}

static gboolean
headline_box_press_cb(GtkWidget *widget, GdkEventButton *event, PidginBuddyList *gtkblist)
{
	gtk_widget_hide(gtkblist->headline_hbox);
	if (gtkblist->headline_callback)
		g_idle_add(headline_click_callback, NULL);
	else {
		if (gtkblist->headline_destroy)
			gtkblist->headline_destroy(gtkblist->headline_data);
		reset_headline(gtkblist);
	}
	return TRUE;
}

/***********************************/
/* Connection error handling stuff */
/***********************************/

#define OBJECT_DATA_KEY_ACCOUNT "account"
#define DO_NOT_CLEAR_ERROR "do-not-clear-error"

static gboolean
find_account_widget(GObject *widget,
                    PurpleAccount *account)
{
	if (g_object_get_data(widget, OBJECT_DATA_KEY_ACCOUNT) == account)
		return 0; /* found */
	else
		return 1;
}

static void
pack_prpl_icon_start(GtkWidget *box,
                     PurpleAccount *account)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
	if (pixbuf != NULL) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(pixbuf);

		gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
	}
}

static void
add_error_dialog(PidginBuddyList *gtkblist,
                 GtkWidget *dialog)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	gtk_container_add(GTK_CONTAINER(priv->error_scrollbook), dialog);
}

static GtkWidget *
find_child_widget_by_account(GtkContainer *container,
                             PurpleAccount *account)
{
	GList *l = NULL;
	GList *children = NULL;
	GtkWidget *ret = NULL;
	/* XXX: Workaround for the currently incomplete implementation of PidginScrollBook */
	if (PIDGIN_IS_SCROLL_BOOK(container))
		container = GTK_CONTAINER(PIDGIN_SCROLL_BOOK(container)->notebook);
	children = gtk_container_get_children(container);
	l = g_list_find_custom(children, account, (GCompareFunc) find_account_widget);
	if (l)
		ret = GTK_WIDGET(l->data);
	g_list_free(children);
	return ret;
}

static void
remove_child_widget_by_account(GtkContainer *container,
                               PurpleAccount *account)
{
	GtkWidget *widget = find_child_widget_by_account(container, account);
	if(widget) {
		/* Since we are destroying the widget in response to a change in
		 * error, we should not clear the error.
		 */
		g_object_set_data(G_OBJECT(widget), DO_NOT_CLEAR_ERROR,
			GINT_TO_POINTER(TRUE));
		gtk_widget_destroy(widget);
	}
}

/* Generic error buttons */

static void
generic_error_modify_cb(PurpleAccount *account)
{
	purple_account_clear_current_error(account);
	pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);
}

static void
generic_error_enable_cb(PurpleAccount *account)
{
	purple_account_clear_current_error(account);
	purple_account_set_enabled(account, purple_core_get_ui(), TRUE);
}

static void
generic_error_destroy_cb(GtkObject *dialog,
                         PurpleAccount *account)
{
	g_hash_table_remove(gtkblist->connection_errors, account);
	/* If the error dialog is being destroyed in response to the
	 * account-error-changed signal, we don't want to clear the current
	 * error.
	 */
	if (g_object_get_data(G_OBJECT(dialog), DO_NOT_CLEAR_ERROR) == NULL)
		purple_account_clear_current_error(account);
}

#define SSL_FAQ_URI "http://d.pidgin.im/wiki/FAQssl"

static void
ssl_faq_clicked_cb(PidginMiniDialog *mini_dialog,
                   GtkButton *button,
                   gpointer ignored)
{
	purple_notify_uri(NULL, SSL_FAQ_URI);
}

static void
add_generic_error_dialog(PurpleAccount *account,
                         const PurpleConnectionErrorInfo *err)
{
	GtkWidget *mini_dialog;
	const char *username = purple_account_get_username(account);
	gboolean enabled =
		purple_account_get_enabled(account, purple_core_get_ui());
	char *primary;

	if (enabled)
		primary = g_strdup_printf(_("%s disconnected"), username);
	else
		primary = g_strdup_printf(_("%s disabled"), username);

	mini_dialog = pidgin_make_mini_dialog(NULL, PIDGIN_STOCK_DIALOG_ERROR,
		primary, err->description, account,
		(enabled ? _("Reconnect") : _("Re-enable")),
		(enabled ? PURPLE_CALLBACK(purple_account_connect)
		         : PURPLE_CALLBACK(generic_error_enable_cb)),
		_("Modify Account"), PURPLE_CALLBACK(generic_error_modify_cb),
		NULL);

	g_free(primary);

	g_object_set_data(G_OBJECT(mini_dialog), OBJECT_DATA_KEY_ACCOUNT,
		account);

	 if(err->type == PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT)
		pidgin_mini_dialog_add_button(PIDGIN_MINI_DIALOG(mini_dialog),
				_("SSL FAQs"), ssl_faq_clicked_cb, NULL);

	g_signal_connect_after(mini_dialog, "destroy",
		(GCallback)generic_error_destroy_cb,
		account);

	add_error_dialog(gtkblist, mini_dialog);
}

static void
remove_generic_error_dialog(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	remove_child_widget_by_account(
		GTK_CONTAINER(priv->error_scrollbook), account);
}


static void
update_generic_error_message(PurpleAccount *account,
                             const char *description)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	GtkWidget *mini_dialog = find_child_widget_by_account(
		GTK_CONTAINER(priv->error_scrollbook), account);
	pidgin_mini_dialog_set_description(PIDGIN_MINI_DIALOG(mini_dialog),
		description);
}


/* Notifications about accounts which were disconnected with
 * PURPLE_CONNECTION_ERROR_NAME_IN_USE
 */

typedef void (*AccountFunction)(PurpleAccount *);

static void
elsewhere_foreach_account(PidginMiniDialog *mini_dialog,
                          AccountFunction f)
{
	PurpleAccount *account;
	GList *labels = gtk_container_get_children(
		GTK_CONTAINER(mini_dialog->contents));
	GList *l;

	for (l = labels; l; l = l->next) {
		account = g_object_get_data(G_OBJECT(l->data), OBJECT_DATA_KEY_ACCOUNT);
		if (account)
			f(account);
		else
			purple_debug_warning("gtkblist", "mini_dialog's child "
				"didn't have an account stored in it!");
	}
	g_list_free(labels);
}

static void
enable_account(PurpleAccount *account)
{
	purple_account_set_enabled(account, purple_core_get_ui(), TRUE);
}

static void
reconnect_elsewhere_accounts(PidginMiniDialog *mini_dialog,
                             GtkButton *button,
                             gpointer unused)
{
	elsewhere_foreach_account(mini_dialog, enable_account);
}

static void
clear_elsewhere_errors(PidginMiniDialog *mini_dialog,
                       gpointer unused)
{
	elsewhere_foreach_account(mini_dialog, purple_account_clear_current_error);
}

static void
ensure_signed_on_elsewhere_minidialog(PidginBuddyList *gtkblist)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	PidginMiniDialog *mini_dialog;

	if(priv->signed_on_elsewhere)
		return;

	mini_dialog = priv->signed_on_elsewhere =
		pidgin_mini_dialog_new(_("Welcome back!"), NULL, PIDGIN_STOCK_DISCONNECT);

	pidgin_mini_dialog_add_button(mini_dialog, _("Re-enable"),
		reconnect_elsewhere_accounts, NULL);

	/* Make dismissing the dialog clear the errors.  The "destroy" signal
	 * does not appear to fire at quit, which is fortunate!
	 */
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		(GCallback) clear_elsewhere_errors, NULL);

	add_error_dialog(gtkblist, GTK_WIDGET(mini_dialog));

	/* Set priv->signed_on_elsewhere to NULL when the dialog is destroyed */
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		(GCallback) gtk_widget_destroyed, &(priv->signed_on_elsewhere));
}

static void
update_signed_on_elsewhere_minidialog_title(void)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	PidginMiniDialog *mini_dialog = priv->signed_on_elsewhere;
	guint accounts;
	char *title;

	if (mini_dialog == NULL)
		return;

	accounts = pidgin_mini_dialog_get_num_children(mini_dialog);
	if (accounts == 0) {
		gtk_widget_destroy(GTK_WIDGET(mini_dialog));
		return;
	}

	title = g_strdup_printf(
		ngettext("%d account was disabled because you signed on from another location:",
			 "%d accounts were disabled because you signed on from another location:",
			 accounts),
		accounts);
	pidgin_mini_dialog_set_description(mini_dialog, title);
	g_free(title);
}

static GtkWidget *
create_account_label(PurpleAccount *account)
{
	GtkWidget *hbox, *label;
	const char *username = purple_account_get_username(account);
	char *markup;

	hbox = gtk_hbox_new(FALSE, 6);
	g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_KEY_ACCOUNT, account);

	pack_prpl_icon_start(hbox, account);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<span size=\"smaller\">%s</span>", username);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	g_object_set(G_OBJECT(label), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#if GTK_CHECK_VERSION(2,12,0)
	{ /* avoid unused variable warnings on pre-2.12 Gtk */
		char *description =
			purple_account_get_current_error(account)->description;
		if (description != NULL && *description != '\0')
			gtk_widget_set_tooltip_text(label, description);
	}
#endif
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	return hbox;
}

static void
add_to_signed_on_elsewhere(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	PidginMiniDialog *mini_dialog;
	GtkWidget *account_label;

	ensure_signed_on_elsewhere_minidialog(gtkblist);
	mini_dialog = priv->signed_on_elsewhere;

	if(find_child_widget_by_account(GTK_CONTAINER(mini_dialog->contents), account))
		return;

	account_label = create_account_label(account);
	gtk_box_pack_start(mini_dialog->contents, account_label, FALSE, FALSE, 0);
	gtk_widget_show_all(account_label);

	update_signed_on_elsewhere_minidialog_title();
}

static void
remove_from_signed_on_elsewhere(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	PidginMiniDialog *mini_dialog = priv->signed_on_elsewhere;
	if(mini_dialog == NULL)
		return;

	remove_child_widget_by_account(GTK_CONTAINER(mini_dialog->contents), account);

	update_signed_on_elsewhere_minidialog_title();
}


static void
update_signed_on_elsewhere_tooltip(PurpleAccount *account,
                                   const char *description)
{
#if GTK_CHECK_VERSION(2,12,0)
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	GtkContainer *c = GTK_CONTAINER(priv->signed_on_elsewhere->contents);
	GtkWidget *label = find_child_widget_by_account(c, account);
	gtk_widget_set_tooltip_text(label, description);
#endif
}


/* Call appropriate error notification code based on error types */
static void
update_account_error_state(PurpleAccount *account,
                           const PurpleConnectionErrorInfo *old,
                           const PurpleConnectionErrorInfo *new,
                           PidginBuddyList *gtkblist)
{
	gboolean descriptions_differ;
	const char *desc;

	if (old == NULL && new == NULL)
		return;

	/* For backwards compatibility: */
	if (new)
		pidgin_blist_update_account_error_state(account, new->description);
	else
		pidgin_blist_update_account_error_state(account, NULL);

	if (new != NULL)
		pidgin_blist_select_notebook_page(gtkblist);

	if (old != NULL && new == NULL) {
		if(old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE)
			remove_from_signed_on_elsewhere(account);
		else
			remove_generic_error_dialog(account);
		return;
	}

	if (old == NULL && new != NULL) {
		if(new->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE)
			add_to_signed_on_elsewhere(account);
		else
			add_generic_error_dialog(account, new);
		return;
	}

	/* else, new and old are both non-NULL */

	descriptions_differ = strcmp(old->description, new->description);
	desc = new->description;

	switch (new->type) {
	case PURPLE_CONNECTION_ERROR_NAME_IN_USE:
		if (old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE
		    && descriptions_differ) {
			update_signed_on_elsewhere_tooltip(account, desc);
		} else {
			remove_generic_error_dialog(account);
			add_to_signed_on_elsewhere(account);
		}
		break;
	default:
		if (old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE) {
			remove_from_signed_on_elsewhere(account);
			add_generic_error_dialog(account, new);
		} else if (descriptions_differ) {
			update_generic_error_message(account, desc);
		}
		break;
	}
}

/* In case accounts are loaded before the blist (which they currently are),
 * let's call update_account_error_state ourselves on every account's current
 * state when the blist starts.
 */
static void
show_initial_account_errors(PidginBuddyList *gtkblist)
{
	GList *l = purple_accounts_get_all();
	PurpleAccount *account;
	const PurpleConnectionErrorInfo *err;

	for (; l; l = l->next)
	{
		account = l->data;
		err = purple_account_get_current_error(account);

		update_account_error_state(account, NULL, err, gtkblist);
	}
}

void
pidgin_blist_update_account_error_state(PurpleAccount *account, const char *text)
{
	/* connection_errors isn't actually used anywhere; it's just kept in
	 * sync with reality in case a plugin uses it.
	 */
	if (text == NULL)
		g_hash_table_remove(gtkblist->connection_errors, account);
	else
		g_hash_table_insert(gtkblist->connection_errors, account, g_strdup(text));
}

static gboolean
paint_headline_hbox  (GtkWidget      *widget,
		      GdkEventExpose *event,
		      gpointer user_data)
{
	gtk_paint_flat_box (widget->style,
		      widget->window,
		      GTK_STATE_NORMAL,
		      GTK_SHADOW_OUT,
		      NULL,
		      widget,
		      "tooltip",
		      widget->allocation.x + 1,
		      widget->allocation.y + 1,
		      widget->allocation.width - 2,
		      widget->allocation.height - 2);
	return FALSE;
}

/* This assumes there are not things like groupless buddies or multi-leveled groups.
 * I'm sure other things in this code assumes that also.
 */
static void
treeview_style_set (GtkWidget *widget,
		    GtkStyle *prev_style,
		    gpointer data)
{
	PurpleBuddyList *list = data;
	PurpleBlistNode *node = list->root;
	while (node) {
		pidgin_blist_update_group(list, node);
		node = node->next;
	}
}

static void
headline_style_set (GtkWidget *widget,
		    GtkStyle  *prev_style)
{
	GtkTooltips *tooltips;
	GtkStyle *style;

	if (gtkblist->changing_style)
		return;

	tooltips = gtk_tooltips_new ();
	g_object_ref_sink (tooltips);

	gtk_tooltips_force_window (tooltips);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_name (tooltips->tip_window, "gtk-tooltips");
#endif
	gtk_widget_ensure_style (tooltips->tip_window);
	style = gtk_widget_get_style (tooltips->tip_window);

	gtkblist->changing_style = TRUE;
	gtk_widget_set_style (gtkblist->headline_hbox, style);
	gtkblist->changing_style = FALSE;

	g_object_unref (tooltips);
}

/******************************************/
/* End of connection error handling stuff */
/******************************************/

static int
blist_focus_cb(GtkWidget *widget, GdkEventFocus *event, PidginBuddyList *gtkblist)
{
	if(event->in) {
		gtk_blist_focused = TRUE;
		pidgin_set_urgent(GTK_WINDOW(gtkblist->window), FALSE);
	} else {
		gtk_blist_focused = FALSE;
	}
	return 0;
}

#if 0
static GtkWidget *
kiosk_page()
{
	GtkWidget *ret = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
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

	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	gtk_widget_show_all(ret);
	return ret;
}
#endif

/* builds the blist layout according to to the current theme */
static void
pidgin_blist_build_layout(PurpleBuddyList *list)
{
	GtkTreeViewColumn *column;
	PidginBlistLayout *layout;
	PidginBlistTheme *theme;
	GtkCellRenderer *rend;
	gint i, status_icon = 0, text = 1, emblem = 2, protocol_icon = 3, buddy_icon = 4;


	column = gtkblist->text_column;

	if ((theme = pidgin_blist_get_theme()) != NULL && (layout = pidgin_blist_theme_get_layout(theme)) != NULL) {
		status_icon = layout->status_icon ;
		text = layout->text;
		emblem = layout->emblem;
		protocol_icon = layout->protocol_icon;
		buddy_icon = layout->buddy_icon;
	}

	gtk_tree_view_column_clear(column);

	/* group */
	rend = pidgin_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "visible", GROUP_EXPANDER_VISIBLE_COLUMN,
					    "expander-visible", GROUP_EXPANDER_COLUMN,
					    "sensitive", GROUP_EXPANDER_COLUMN,
					    "cell-background-gdk", BGCOLOR_COLUMN,
					    NULL);

	/* contact */
	rend = pidgin_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "visible", CONTACT_EXPANDER_VISIBLE_COLUMN,
					    "expander-visible", CONTACT_EXPANDER_COLUMN,
					    "sensitive", CONTACT_EXPANDER_COLUMN,
					    "cell-background-gdk", BGCOLOR_COLUMN,
					    NULL);

	for (i = 0; i < 5; i++) {

		if (status_icon == i) {
			/* status icons */
			rend = gtk_cell_renderer_pixbuf_new();
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend,
							    "pixbuf", STATUS_ICON_COLUMN,
							    "visible", STATUS_ICON_VISIBLE_COLUMN,
							    "cell-background-gdk", BGCOLOR_COLUMN,
							    NULL);
			g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0, NULL);

		} else if (text == i) {
			/* name */
			gtkblist->text_rend = rend = gtk_cell_renderer_text_new();
			gtk_tree_view_column_pack_start(column, rend, TRUE);
			gtk_tree_view_column_set_attributes(column, rend,
							    "cell-background-gdk", BGCOLOR_COLUMN,
							    "markup", NAME_COLUMN,
							    NULL);
			g_signal_connect(G_OBJECT(rend), "editing-started", G_CALLBACK(gtk_blist_renderer_editing_started_cb), NULL);
			g_signal_connect(G_OBJECT(rend), "editing-canceled", G_CALLBACK(gtk_blist_renderer_editing_cancelled_cb), list);
			g_signal_connect(G_OBJECT(rend), "edited", G_CALLBACK(gtk_blist_renderer_edited_cb), list);
			g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);
			g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

			/* idle */
			rend = gtk_cell_renderer_text_new();
			g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend,
							    "markup", IDLE_COLUMN,
							    "visible", IDLE_VISIBLE_COLUMN,
							    "cell-background-gdk", BGCOLOR_COLUMN,
							    NULL);
		} else if (emblem == i) {
			/* emblem */
			rend = gtk_cell_renderer_pixbuf_new();
			g_object_set(rend, "xalign", 1.0, "yalign", 0.5, "ypad", 0, "xpad", 3, NULL);
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend, "pixbuf", EMBLEM_COLUMN,
									  "cell-background-gdk", BGCOLOR_COLUMN,
									  "visible", EMBLEM_VISIBLE_COLUMN, NULL);

		} else if (protocol_icon == i) {
			/* protocol icon */
			rend = gtk_cell_renderer_pixbuf_new();
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend,
							   "pixbuf", PROTOCOL_ICON_COLUMN,
							   "visible", PROTOCOL_ICON_VISIBLE_COLUMN,
							   "cell-background-gdk", BGCOLOR_COLUMN,
							  NULL);
			g_object_set(rend, "xalign", 0.0, "xpad", 3, "ypad", 0, NULL);

		} else if (buddy_icon == i) {
			/* buddy icon */
			rend = gtk_cell_renderer_pixbuf_new();
			g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend, "pixbuf", BUDDY_ICON_COLUMN,
							    "cell-background-gdk", BGCOLOR_COLUMN,
							    "visible", BUDDY_ICON_VISIBLE_COLUMN,
							    NULL);
		}

	}/* end for loop */

}

static gboolean
pidgin_blist_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data)
{
	PurpleBlistNode *node = NULL;
	gboolean res = TRUE;
	const char *compare = NULL;

	if (!pidgin_tree_view_search_equal_func(model, column, key, iter, data))
		return FALSE;

	/* If the search string does not match the displayed label, then look
	 * at the alternate labels for the nodes and search in them. Currently,
	 * alternate labels that make sense are usernames/email addresses for
	 * buddies (but only for the ones who don't have a local alias).
	 */

	gtk_tree_model_get(model, iter, NODE_COLUMN, &node, -1);
	if (!node)
		return TRUE;

	compare = NULL;
	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBuddy *b = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		if (!purple_buddy_get_local_buddy_alias(b))
			compare = purple_buddy_get_name(b);
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		if (!purple_buddy_get_local_buddy_alias(PURPLE_BUDDY(node)))
			compare = purple_buddy_get_name(PURPLE_BUDDY(node));
	}

	if (compare) {
		char *tmp, *enteredstring;
		tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
		enteredstring = g_utf8_casefold(tmp, -1);
		g_free(tmp);

		if (purple_str_has_prefix(compare, enteredstring))
			res = FALSE;
		g_free(enteredstring);
	}

	return res;
}

static void pidgin_blist_show(PurpleBuddyList *list)
{
	PidginBuddyListPrivate *priv;
	void *handle;
	GtkTreeViewColumn *column;
	GtkWidget *menu;
	GtkWidget *ebox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *label;
	GtkWidget *close;
	char *pretty, *tmp;
	const char *theme_name;
	GtkAccelGroup *accel_group;
	GtkTreeSelection *selection;
	GtkTargetEntry dte[] = {{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD },
				{"text/uri-list", 0, DRAG_URI},
				{"text/plain", 0, DRAG_TEXT}};
	GtkTargetEntry ste[] = {{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD }};
	if (gtkblist && gtkblist->window) {
		purple_blist_set_visible(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));
		return;
	}

	gtkblist = PIDGIN_BLIST(list);
	priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);

	if (priv->current_theme)
		g_object_unref(priv->current_theme);

	theme_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/theme");
	if (theme_name && *theme_name)
		priv->current_theme = g_object_ref(PIDGIN_BLIST_THEME(purple_theme_manager_find_theme(theme_name, "blist")));
	else
		priv->current_theme = NULL;

	gtkblist->empty_avatar = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill(gtkblist->empty_avatar, 0x00000000);

	gtkblist->window = pidgin_create_window(_("Buddy List"), 0, "buddy_list", TRUE);
	g_signal_connect(G_OBJECT(gtkblist->window), "focus-in-event",
			 G_CALLBACK(blist_focus_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->window), "focus-out-event",
			 G_CALLBACK(blist_focus_cb), gtkblist);
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
	gtkblist->ift = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<PurpleMain>", accel_group);
	gtk_item_factory_set_translate_func(gtkblist->ift,
										(GtkTranslateFunc)item_factory_translate_func,
										NULL, NULL);
	gtk_item_factory_create_items(gtkblist->ift, sizeof(blist_menu) / sizeof(*blist_menu),
								  blist_menu, NULL);
	pidgin_load_accels();
	g_signal_connect(G_OBJECT(accel_group), "accel-changed", G_CALLBACK(pidgin_save_accels_cb), NULL);

	menu = gtk_item_factory_get_widget(gtkblist->ift, "<PurpleMain>");
	gtkblist->menutray = pidgin_menu_tray_new();
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
	tmp = g_strdup_printf(_("<span weight='bold' size='larger'>Welcome to %s!</span>\n\n"

					       "You have no accounts enabled. Enable your IM accounts from the "
					       "<b>Accounts</b> window at <b>Accounts->Manage Accounts</b>. Once you "
					       "enable accounts, you'll be able to sign on, set your status, "
					       "and talk to your friends."), PIDGIN_NAME);
	pretty = pidgin_make_pretty_arrows(tmp);
	g_free(tmp);
	label = gtk_label_new(NULL);
	gtk_widget_set_size_request(label, purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width") - 12, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.2);
	gtk_label_set_markup(GTK_LABEL(label), pretty);
	g_free(pretty);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook),label, NULL);
	gtkblist->vbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook), gtkblist->vbox, NULL);
	gtk_widget_show_all(gtkblist->notebook);
	pidgin_blist_select_notebook_page(gtkblist);

	ebox = gtk_event_box_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), ebox, FALSE, FALSE, 0);
	gtkblist->headline_hbox = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(gtkblist->headline_hbox), 6);
	gtk_container_add(GTK_CONTAINER(ebox), gtkblist->headline_hbox);
	gtkblist->headline_image = gtk_image_new_from_pixbuf(NULL);
	gtk_misc_set_alignment(GTK_MISC(gtkblist->headline_image), 0.0, 0);
	gtkblist->headline_label = gtk_label_new(NULL);
	gtk_widget_set_size_request(gtkblist->headline_label,
				    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width")-25,-1);
	gtk_label_set_line_wrap(GTK_LABEL(gtkblist->headline_label), TRUE);
	gtk_box_pack_start(GTK_BOX(gtkblist->headline_hbox), gtkblist->headline_image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtkblist->headline_hbox), gtkblist->headline_label, TRUE, TRUE, 0);
	g_signal_connect(gtkblist->headline_label,   /* connecting on headline_hbox doesn't work, because
	                                                the signal is not emitted when theme is changed */
			"style-set",
			 G_CALLBACK(headline_style_set),
			 NULL);
	g_signal_connect (gtkblist->headline_hbox,
			  "expose_event",
			  G_CALLBACK (paint_headline_hbox),
			  NULL);
	gtk_widget_set_name(gtkblist->headline_hbox, "gtk-tooltips");

	gtkblist->headline_close = gtk_widget_render_icon(ebox, GTK_STOCK_CLOSE,
		gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC), NULL);
	gtkblist->hand_cursor = gdk_cursor_new (GDK_HAND2);
	gtkblist->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	/* Close button. */
	close = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	close = pidgin_create_small_button(close);
	gtk_box_pack_start(GTK_BOX(gtkblist->headline_hbox), close, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(close, _("Close"));
#endif
	g_signal_connect(close, "clicked", G_CALLBACK(headline_close_press_cb), gtkblist);

	g_signal_connect(G_OBJECT(ebox), "enter-notify-event", G_CALLBACK(headline_box_enter_cb), gtkblist);
	g_signal_connect(G_OBJECT(ebox), "leave-notify-event", G_CALLBACK(headline_box_leave_cb), gtkblist);
	g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(headline_box_press_cb), gtkblist);

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
						 G_TYPE_BOOLEAN,  /* Group expander visible */
						 G_TYPE_BOOLEAN,  /* Contact expander */
						 G_TYPE_BOOLEAN,  /* Contact expander visible */
						 GDK_TYPE_PIXBUF, /* Emblem */
						 G_TYPE_BOOLEAN,  /* Emblem visible */
						 GDK_TYPE_PIXBUF, /* Protocol icon */
						 G_TYPE_BOOLEAN   /* Protocol visible */
						);

	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));

	gtk_widget_show(gtkblist->treeview);
	gtk_widget_set_name(gtkblist->treeview, "pidgin_blist_treeview");

	g_signal_connect(gtkblist->treeview,
			 "style-set",
			 G_CALLBACK(treeview_style_set), list);
	/* Set up selection stuff */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(pidgin_blist_selection_changed), NULL);

	/* Set up dnd */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(gtkblist->treeview),
										   GDK_BUTTON1_MASK, ste, 3,
										   GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(gtkblist->treeview),
										 dte, 5,
										 GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-received", G_CALLBACK(pidgin_blist_drag_data_rcv_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-get", G_CALLBACK(pidgin_blist_drag_data_get_cb), NULL);
#ifdef _WIN32
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-begin", G_CALLBACK(pidgin_blist_drag_begin), NULL);
#endif
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-motion", G_CALLBACK(pidgin_blist_drag_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "motion-notify-event", G_CALLBACK(pidgin_blist_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "leave-notify-event", G_CALLBACK(pidgin_blist_leave_cb), NULL);

	/* Tooltips */
	pidgin_tooltip_setup_for_treeview(gtkblist->treeview, NULL,
			pidgin_blist_create_tooltip,
			pidgin_blist_paint_tip);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);

	/* expander columns */
	column = gtk_tree_view_column_new();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	/* everything else column */
	gtkblist->text_column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->text_column);
	pidgin_blist_build_layout(list);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated", G_CALLBACK(gtk_blist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-expanded", G_CALLBACK(gtk_blist_row_expanded_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-collapsed", G_CALLBACK(gtk_blist_row_collapsed_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "key-press-event", G_CALLBACK(gtk_blist_key_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "popup-menu", G_CALLBACK(pidgin_blist_popup_menu_cb), NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(gtkblist->treeview), NAME_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(gtkblist->treeview),
			pidgin_blist_search_equal_func, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sw, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(sw), gtkblist->treeview);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sep, FALSE, FALSE, 0);

	gtkblist->scrollbook = pidgin_scroll_book_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->scrollbook, FALSE, FALSE, 0);

	/* Create an vbox which holds the scrollbook which is actually used to
	 * display connection errors.  The vbox needs to still exist for
	 * backwards compatibility.
	 */
	gtkblist->error_buttons = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->error_buttons, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(gtkblist->error_buttons), 0);

	priv->error_scrollbook = PIDGIN_SCROLL_BOOK(pidgin_scroll_book_new());
	gtk_box_pack_start(GTK_BOX(gtkblist->error_buttons),
		GTK_WIDGET(priv->error_scrollbook), FALSE, FALSE, 0);


	/* Add the statusbox */
	gtkblist->statusbox = pidgin_status_box_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->statusbox, FALSE, TRUE, 0);
	gtk_widget_set_name(gtkblist->statusbox, "pidgin_blist_statusbox");
	gtk_container_set_border_width(GTK_CONTAINER(gtkblist->statusbox), 3);
	gtk_widget_show(gtkblist->statusbox);

	/* set the Show Offline Buddies option. must be done
	 * after the treeview or faceprint gets mad. -Robot101
	 */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show/Offline Buddies"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show/Empty Groups"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Tools/Mute Sounds"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show/Buddy Details"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show/Idle Times"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item (gtkblist->ift, N_("/Buddies/Show/Protocol Icons"))),
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"));

	if(!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), "none"))
		gtk_widget_set_sensitive(gtk_item_factory_get_widget(gtkblist->ift, N_("/Tools/Mute Sounds")), FALSE);

	/* Update some dynamic things */
	update_menu_bar(gtkblist);
	pidgin_blist_update_plugin_actions();
	pidgin_blist_update_sort_methods();

	/* OK... let's show this bad boy. */
	pidgin_blist_refresh(list);
	pidgin_blist_restore_position();
	gtk_widget_show_all(GTK_WIDGET(gtkblist->vbox));
	gtk_widget_realize(GTK_WIDGET(gtkblist->window));
	purple_blist_set_visible(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));

	/* start the refresh timer */
	gtkblist->refresh_timer = purple_timeout_add_seconds(30, (GSourceFunc)pidgin_blist_refresh_timer, list);

	handle = pidgin_blist_get_handle();

	/* things that affect how buddies are displayed */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_buddy_icons",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_idle_time",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_empty_groups",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_offline_buddies",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_protocol_icons",
			_prefs_change_redo_list, NULL);

	/* sorting */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/sort_type",
			_prefs_change_sort_method, NULL);

	/* menus */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/sound/mute",
			pidgin_blist_mute_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/sound/method",
			pidgin_blist_sound_method_pref_cb, NULL);

	/* Setup some purple signal handlers. */

	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-enabled", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-disabled", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-removed", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-status-changed", gtkblist,
	                      PURPLE_CALLBACK(account_status_changed),
	                      gtkblist);
	purple_signal_connect(handle, "account-error-changed", gtkblist,
	                      PURPLE_CALLBACK(update_account_error_state),
	                      gtkblist);
	purple_signal_connect(handle, "account-actions-changed", gtkblist,
	                      PURPLE_CALLBACK(account_actions_changed), NULL);

	handle = pidgin_account_get_handle();
	purple_signal_connect(handle, "account-modified", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);

	handle = purple_connections_get_handle();
	purple_signal_connect(handle, "signed-on", gtkblist,
	                      PURPLE_CALLBACK(sign_on_off_cb), list);
	purple_signal_connect(handle, "signed-off", gtkblist,
	                      PURPLE_CALLBACK(sign_on_off_cb), list);

	handle = purple_plugins_get_handle();
	purple_signal_connect(handle, "plugin-load", gtkblist,
	                      PURPLE_CALLBACK(plugin_changed_cb), NULL);
	purple_signal_connect(handle, "plugin-unload", gtkblist,
	                      PURPLE_CALLBACK(plugin_changed_cb), NULL);

	handle = purple_conversations_get_handle();
	purple_signal_connect(handle, "conversation-updated", gtkblist,
	                      PURPLE_CALLBACK(conversation_updated_cb),
	                      gtkblist);
	purple_signal_connect(handle, "deleting-conversation", gtkblist,
	                      PURPLE_CALLBACK(conversation_deleting_cb),
	                      gtkblist);
	purple_signal_connect(handle, "conversation-created", gtkblist,
	                      PURPLE_CALLBACK(conversation_created_cb),
	                      gtkblist);

	gtk_widget_hide(gtkblist->headline_hbox);

	show_initial_account_errors(gtkblist);

	/* emit our created signal */
	handle = pidgin_blist_get_handle();
	purple_signal_emit(handle, "gtkblist-created", list);
}

static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender)
{
	PurpleBlistNode *node;

	gtkblist = PIDGIN_BLIST(list);
	if(!gtkblist || !gtkblist->treeview)
		return;

	node = list->root;

	while (node)
	{
		/* This is only needed when we're reverting to a non-GTK+ sorted
		 * status.  We shouldn't need to remove otherwise.
		 */
		if (remove && !PURPLE_BLIST_NODE_IS_GROUP(node))
			pidgin_blist_hide_node(list, node, FALSE);

		if (PURPLE_BLIST_NODE_IS_BUDDY(node))
			pidgin_blist_update_buddy(list, node, rerender);
		else if (PURPLE_BLIST_NODE_IS_CHAT(node))
			pidgin_blist_update(list, node);
		else if (PURPLE_BLIST_NODE_IS_GROUP(node))
			pidgin_blist_update(list, node);
		node = purple_blist_node_next(node, FALSE);
	}

}

void pidgin_blist_refresh(PurpleBuddyList *list)
{
	redo_buddy_list(list, FALSE, TRUE);
}

void
pidgin_blist_update_refresh_timeout()
{
	PurpleBuddyList *blist;
	PidginBuddyList *gtkblist;

	blist = purple_get_blist();
	gtkblist = PIDGIN_BLIST(purple_get_blist());

	gtkblist->refresh_timer = purple_timeout_add_seconds(30,(GSourceFunc)pidgin_blist_refresh_timer, blist);
}

static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter) {
	struct _pidgin_blist_node *gtknode = (struct _pidgin_blist_node *)node->ui_data;
	GtkTreePath *path;

	if (!gtknode) {
		return FALSE;
	}

	if (!gtkblist) {
		purple_debug_error("gtkblist", "get_iter_from_node was called, but we don't seem to have a blist\n");
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

static void pidgin_blist_remove(PurpleBuddyList *list, PurpleBlistNode *node)
{
	struct _pidgin_blist_node *gtknode = node->ui_data;

	purple_request_close_with_handle(node);

	pidgin_blist_hide_node(list, node, TRUE);

	if(node->parent)
		pidgin_blist_update(list, node->parent);

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
			purple_timeout_remove(gtknode->recent_signonoff_timer);

		purple_signals_disconnect_by_handle(node->ui_data);
		g_free(node->ui_data);
		node->ui_data = NULL;
	}
}

static gboolean do_selection_changed(PurpleBlistNode *new_selection)
{
	PurpleBlistNode *old_selection = NULL;

	/* test for gtkblist because crazy timeout means we can be called after the blist is gone */
	if (gtkblist && new_selection != gtkblist->selected_node) {
		old_selection = gtkblist->selected_node;
		gtkblist->selected_node = new_selection;
		if(new_selection)
			pidgin_blist_update(NULL, new_selection);
		if(old_selection)
			pidgin_blist_update(NULL, old_selection);
	}

	return FALSE;
}

static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	PurpleBlistNode *new_selection = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_get_selected(selection, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &new_selection, -1);
	}

	/* we set this up as a timeout, otherwise the blist flickers ...
	 * but we don't do it for groups, because it causes total bizarness -
	 * the previously selected buddy node might rendered at half height.
	 */
	if ((new_selection != NULL) && PURPLE_BLIST_NODE_IS_GROUP(new_selection)) {
		do_selection_changed(new_selection);
	} else {
		g_timeout_add(0, (GSourceFunc)do_selection_changed, new_selection);
	}
}

static gboolean insert_node(PurpleBuddyList *list, PurpleBlistNode *node, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter, cur, *curptr = NULL;
	struct _pidgin_blist_node *gtknode = node->ui_data;
	GtkTreePath *newpath;

	if(!iter)
		return FALSE;

	if(node->parent && !get_iter_from_node(node->parent, &parent_iter))
		return FALSE;

	if(get_iter_from_node(node, &cur))
		curptr = &cur;

	if(PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_CHAT(node)) {
		current_sort_method->func(node, list, parent_iter, curptr, iter);
	} else {
		sort_method_none(node, list, parent_iter, curptr, iter);
	}

	if(gtknode != NULL) {
		gtk_tree_row_reference_free(gtknode->row);
	} else {
		pidgin_blist_new_node(node);
		gtknode = (struct _pidgin_blist_node *)node->ui_data;
	}

	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel),
			iter);
	gtknode->row =
		gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel),
				newpath);

	gtk_tree_path_free(newpath);

	if (!editing_blist)
		gtk_tree_store_set(gtkblist->treemodel, iter,
				NODE_COLUMN, node,
				-1);

	if(node->parent) {
		GtkTreePath *expand = NULL;
		struct _pidgin_blist_node *gtkparentnode = node->parent->ui_data;

		if(PURPLE_BLIST_NODE_IS_GROUP(node->parent)) {
			if(!purple_blist_node_get_bool(node->parent, "collapsed"))
				expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		} else if(PURPLE_BLIST_NODE_IS_CONTACT(node->parent) &&
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

static gboolean pidgin_blist_group_has_show_offline_buddy(PurpleGroup *group)
{
	PurpleBlistNode *gnode, *cnode, *bnode;

	gnode = (PurpleBlistNode *)group;
	for(cnode = gnode->child; cnode; cnode = cnode->next) {
		if(PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				PurpleBuddy *buddy = (PurpleBuddy *)bnode;
				if (purple_account_is_connected(buddy->account) &&
					purple_blist_node_get_bool(bnode, "show_offline"))
					return TRUE;
			}
		}
	}
	return FALSE;
}

/* This version of pidgin_blist_update_group can take the original buddy or a
 * group, but has much better algorithmic performance with a pre-known buddy.
 */
static void pidgin_blist_update_group(PurpleBuddyList *list,
                                      PurpleBlistNode *node)
{
	gint count;
	PurpleGroup *group;
	PurpleBlistNode* gnode;
	gboolean show = FALSE, show_offline = FALSE;

	g_return_if_fail(node != NULL);

	if (editing_blist)
		return;

	if (PURPLE_BLIST_NODE_IS_GROUP(node))
		gnode = node;
	else if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		gnode = node->parent->parent;
	else if (PURPLE_BLIST_NODE_IS_CONTACT(node) || PURPLE_BLIST_NODE_IS_CHAT(node))
		gnode = node->parent;
	else
		return;

	group = (PurpleGroup*)gnode;

	show_offline = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies");

	if(show_offline)
		count = purple_blist_get_group_size(group, FALSE);
	else
		count = purple_blist_get_group_online_count(group);

	if (count > 0 || purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups"))
		show = TRUE;
	else if (PURPLE_BLIST_NODE_IS_BUDDY(node) && buddy_is_displayable((PurpleBuddy*)node)) { /* Or chat? */
		show = TRUE;
	} else if (!show_offline) {
		show = pidgin_blist_group_has_show_offline_buddy(group);
	}

	if (show) {
		gchar *title;
		gboolean biglist;
		GtkTreeIter iter;
		GtkTreePath *path;
		gboolean expanded;
		GdkColor *bgcolor = NULL;
		GdkPixbuf *avatar = NULL;
		PidginBlistTheme *theme = NULL;

		if(!insert_node(list, gnode, &iter))
			return;

		if ((theme = pidgin_blist_get_theme()) == NULL)
			bgcolor = NULL;
		else if (purple_blist_node_get_bool(gnode, "collapsed") || count <= 0)
			bgcolor = pidgin_blist_theme_get_collapsed_background_color(theme);
		else
			bgcolor = pidgin_blist_theme_get_expanded_background_color(theme);

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(gtkblist->treeview), path);
		gtk_tree_path_free(path);

		title = pidgin_get_group_title(gnode, expanded);
		biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");

		if (biglist) {
			avatar = pidgin_blist_get_buddy_icon(gnode, TRUE, TRUE);
		}

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_VISIBLE_COLUMN, FALSE,
				   STATUS_ICON_COLUMN, NULL,
				   NAME_COLUMN, title,
				   NODE_COLUMN, gnode,
				   BGCOLOR_COLUMN, bgcolor,
				   GROUP_EXPANDER_COLUMN, TRUE,
				   GROUP_EXPANDER_VISIBLE_COLUMN, TRUE,
				   CONTACT_EXPANDER_VISIBLE_COLUMN, FALSE,
				   BUDDY_ICON_COLUMN, avatar,
				   BUDDY_ICON_VISIBLE_COLUMN, biglist,
				   IDLE_VISIBLE_COLUMN, FALSE,
				   EMBLEM_VISIBLE_COLUMN, FALSE,
				   -1);
		g_free(title);
	} else {
		pidgin_blist_hide_node(list, gnode, TRUE);
	}
}

static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded)
{
	PurpleGroup *group;
	gboolean selected;
	char group_count[12] = "";
	char *mark, *esc;
	PurpleBlistNode *selected_node = NULL;
	GtkTreeIter iter;
	PidginThemeFont *pair;
	gchar const *text_color, *text_font;
	PidginBlistTheme *theme;

	group = (PurpleGroup*)gnode;

	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview)), NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &selected_node, -1);
	}
	selected = (gnode == selected_node);

	if (!expanded) {
		g_snprintf(group_count, sizeof(group_count), "%d/%d",
		           purple_blist_get_group_online_count(group),
		           purple_blist_get_group_size(group, FALSE));
	}

	theme = pidgin_blist_get_theme();
	if (theme == NULL)
		pair = NULL;
	else if (expanded)
		pair = pidgin_blist_theme_get_expanded_text_info(theme);
	else
		pair = pidgin_blist_theme_get_collapsed_text_info(theme);


	text_color = selected ? NULL : theme_font_get_color_default(pair, NULL);
	text_font = theme_font_get_face_default(pair, "");

	esc = g_markup_escape_text(group->name, -1);
	if (text_color) {
		mark = g_strdup_printf("<span foreground='%s' font_desc='%s'><b>%s</b>%s%s%s</span>",
		                       text_color, text_font,
		                       esc ? esc : "",
		                       !expanded ? " <span weight='light'>(</span>" : "",
		                       group_count,
		                       !expanded ? "<span weight='light'>)</span>" : "");
	} else {
		mark = g_strdup_printf("<span font_desc='%s'><b>%s</b>%s%s%s</span>",
		                       text_font, esc ? esc : "",
		                       !expanded ? " <span weight='light'>(</span>" : "",
		                       group_count,
		                       !expanded ? "<span weight='light'>)</span>" : "");
	}

	g_free(esc);
	return mark;
}

static void buddy_node(PurpleBuddy *buddy, GtkTreeIter *iter, PurpleBlistNode *node)
{
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	GdkPixbuf *status, *avatar, *emblem, *prpl_icon;
	GdkColor *color = NULL;
	char *mark;
	char *idle = NULL;
	gboolean expanded = ((struct _pidgin_blist_node *)(node->parent->ui_data))->contact_expanded;
	gboolean selected = (gtkblist->selected_node == node);
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	PidginBlistTheme *theme;

	if (editing_blist)
		return;

	status = pidgin_blist_get_status_icon((PurpleBlistNode*)buddy,
						biglist ? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);

	/* Speed it up if we don't want buddy icons. */
	if(biglist)
		avatar = pidgin_blist_get_buddy_icon((PurpleBlistNode *)buddy, TRUE, TRUE);
	else
		avatar = NULL;

	if (!avatar) {
		g_object_ref(G_OBJECT(gtkblist->empty_avatar));
		avatar = gtkblist->empty_avatar;
	} else if ((!PURPLE_BUDDY_IS_ONLINE(buddy) || purple_presence_is_idle(presence))) {
		do_alphashift(avatar, 77);
	}

	emblem = pidgin_blist_get_emblem((PurpleBlistNode*) buddy);
	mark = pidgin_blist_get_name_markup(buddy, selected, TRUE);

	theme = pidgin_blist_get_theme();

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time") &&
		purple_presence_is_idle(presence) && !biglist)
	{
		time_t idle_secs = purple_presence_get_idle_time(presence);

		if (idle_secs > 0)
		{
			PidginThemeFont *pair = NULL;
			const gchar *textcolor;
			time_t t;
			int ihrs, imin;
			time(&t);

			ihrs = (t - idle_secs) / 3600;
			imin = ((t - idle_secs) / 60) % 60;

			if (selected)
				textcolor = NULL;
			else if (theme != NULL && (pair = pidgin_blist_theme_get_idle_text_info(theme)) != NULL)
				textcolor = pidgin_theme_font_get_color_describe(pair);
			else
				/* If no theme them default to making idle buddy names grey */
				textcolor = "dim grey";

			if (textcolor) {
				idle = g_strdup_printf("<span color='%s' font_desc='%s'>%d:%02d</span>",
					textcolor, theme_font_get_face_default(pair, ""),
					ihrs, imin);
			} else {
				idle = g_strdup_printf("<span font_desc='%s'>%d:%02d</span>",
					theme_font_get_face_default(pair, ""), 
					ihrs, imin);
			}
		}
	}

	prpl_icon = pidgin_create_prpl_icon(buddy->account, PIDGIN_PRPL_ICON_SMALL);

	if (theme != NULL)
		color = pidgin_blist_theme_get_contact_color(theme);

	gtk_tree_store_set(gtkblist->treemodel, iter,
			   STATUS_ICON_COLUMN, status,
			   STATUS_ICON_VISIBLE_COLUMN, TRUE,
			   NAME_COLUMN, mark,
			   IDLE_COLUMN, idle,
			   IDLE_VISIBLE_COLUMN, !biglist && idle,
			   BUDDY_ICON_COLUMN, avatar,
			   BUDDY_ICON_VISIBLE_COLUMN, biglist,
			   EMBLEM_COLUMN, emblem,
			   EMBLEM_VISIBLE_COLUMN, (emblem != NULL),
			   PROTOCOL_ICON_COLUMN, prpl_icon,
			   PROTOCOL_ICON_VISIBLE_COLUMN, purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"),
			   BGCOLOR_COLUMN, color,
			   CONTACT_EXPANDER_COLUMN, NULL,
			   CONTACT_EXPANDER_VISIBLE_COLUMN, expanded,
			   GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
			-1);

	g_free(mark);
	g_free(idle);
	if(emblem)
		g_object_unref(emblem);
	if(status)
		g_object_unref(status);
	if(avatar)
		g_object_unref(avatar);
	if(prpl_icon)
		g_object_unref(prpl_icon);
}

/* This is a variation on the original gtk_blist_update_contact. Here we
	can know in advance which buddy has changed so we can just update that */
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleBlistNode *cnode;
	PurpleContact *contact;
	PurpleBuddy *buddy;
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	struct _pidgin_blist_node *gtknode;

	if (editing_blist)
		return;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		cnode = node->parent;
	else
		cnode = node;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_CONTACT(cnode));

	/* First things first, update the group */
	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		pidgin_blist_update_group(list, node);
	else
		pidgin_blist_update_group(list, cnode->parent);

	contact = (PurpleContact*)cnode;
	buddy = purple_contact_get_priority_buddy(contact);

	if (buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if(!insert_node(list, cnode, &iter))
			return;

		gtknode = (struct _pidgin_blist_node *)cnode->ui_data;

		if(gtknode->contact_expanded) {
			GdkPixbuf *status;
			gchar *mark, *tmp;
			const gchar *fg_color, *font;
			GdkColor *color = NULL;
			PidginBlistTheme *theme;
			PidginThemeFont *pair;
			gboolean selected = (gtkblist->selected_node == cnode);

			mark = g_markup_escape_text(purple_contact_get_alias(contact), -1);

			theme = pidgin_blist_get_theme();
			if (theme == NULL)
				pair = NULL;
			else {
				pair = pidgin_blist_theme_get_contact_text_info(theme);
				color = pidgin_blist_theme_get_contact_color(theme);
			}

			font = theme_font_get_face_default(pair, "");
			fg_color = selected ? NULL : theme_font_get_color_default(pair, NULL);

			if (fg_color) {
				tmp = g_strdup_printf("<span font_desc='%s' color='%s'>%s</span>",
						font, fg_color, mark);
			} else {
				tmp = g_strdup_printf("<span font_desc='%s'>%s</span>", font, 
					mark);
			}
			g_free(mark);
			mark = tmp;

			status = pidgin_blist_get_status_icon(cnode,
					 biglist? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);

			gtk_tree_store_set(gtkblist->treemodel, &iter,
					   STATUS_ICON_COLUMN, status,
					   STATUS_ICON_VISIBLE_COLUMN, TRUE,
					   NAME_COLUMN, mark,
					   IDLE_COLUMN, NULL,
					   IDLE_VISIBLE_COLUMN, FALSE,
					   BGCOLOR_COLUMN, color,
					   BUDDY_ICON_COLUMN, NULL,
					   CONTACT_EXPANDER_COLUMN, TRUE,
					   CONTACT_EXPANDER_VISIBLE_COLUMN, TRUE,
				  	   GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
					-1);
			g_free(mark);
			if(status)
				g_object_unref(status);
		} else {
			buddy_node(buddy, &iter, cnode);
		}
	} else {
		pidgin_blist_hide_node(list, cnode, TRUE);
	}
}



static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change)
{
	PurpleBuddy *buddy;
	struct _pidgin_blist_node *gtkparentnode;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	if (node->parent == NULL)
		return;

	buddy = (PurpleBuddy*)node;

	/* First things first, update the contact */
	pidgin_blist_update_contact(list, node);

	gtkparentnode = (struct _pidgin_blist_node *)node->parent->ui_data;

	if (gtkparentnode->contact_expanded && buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if (!insert_node(list, node, &iter))
			return;

		buddy_node(buddy, &iter, node);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}

}

static void pidgin_blist_update_chat(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleChat *chat;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_CHAT(node));

	if (editing_blist)
		return;

	/* First things first, update the group */
	pidgin_blist_update_group(list, node->parent);

	chat = (PurpleChat*)node;

	if(purple_account_is_connected(chat->account)) {
		GtkTreeIter iter;
		GdkPixbuf *status, *avatar, *emblem, *prpl_icon;
		const gchar *color, *font;
		gchar *mark, *tmp;
		gboolean showicons = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
		gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
		PidginBlistNode *ui;
		PurpleConversation *conv;
		gboolean hidden = FALSE;
		GdkColor *bgcolor = NULL;
		PidginThemeFont *pair;
		PidginBlistTheme *theme;
		gboolean selected = (gtkblist->selected_node == node);
		gboolean nick_said = FALSE;

		if (!insert_node(list, node, &iter))
			return;

		ui = node->ui_data;
		conv = ui->conv.conv;
		if (conv && pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv))) {
			hidden = (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE);
			nick_said = (ui->conv.flags & PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK);
		}

		status = pidgin_blist_get_status_icon(node,
				 biglist ? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);
		emblem = pidgin_blist_get_emblem(node);

		/* Speed it up if we don't want buddy icons. */
		if(showicons)
			avatar = pidgin_blist_get_buddy_icon(node, TRUE, FALSE);
		else
			avatar = NULL;

		mark = g_markup_escape_text(purple_chat_get_name(chat), -1);

		theme = pidgin_blist_get_theme();

		if (theme == NULL)
			pair = NULL;
		else if (nick_said)
			pair = pidgin_blist_theme_get_unread_message_nick_said_text_info(theme);
		else if (hidden)
			pair = pidgin_blist_theme_get_unread_message_text_info(theme);
		else pair = pidgin_blist_theme_get_online_text_info(theme);


		font = theme_font_get_face_default(pair, "");
		if (selected || !(color = theme_font_get_color_default(pair, NULL)))
			/* nick_said color is the same as gtkconv:tab-label-attention */
			color = (nick_said ? "#006aff" : NULL);

		if (color) {
			tmp = g_strdup_printf("<span font_desc='%s' color='%s' weight='%s'>%s</span>",
				  	  font, color, hidden ? "bold" : "normal", mark);
		} else {
			tmp = g_strdup_printf("<span font_desc='%s' weight='%s'>%s</span>",
				  	  font, hidden ? "bold" : "normal", mark);
		}
		g_free(mark);
		mark = tmp;

		prpl_icon = pidgin_create_prpl_icon(chat->account, PIDGIN_PRPL_ICON_SMALL);

		if (theme != NULL)
			bgcolor = pidgin_blist_theme_get_contact_color(theme);

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				STATUS_ICON_COLUMN, status,
				STATUS_ICON_VISIBLE_COLUMN, TRUE,
				BUDDY_ICON_COLUMN, avatar ? avatar : gtkblist->empty_avatar,
				BUDDY_ICON_VISIBLE_COLUMN, showicons,
				EMBLEM_COLUMN, emblem,
				EMBLEM_VISIBLE_COLUMN, emblem != NULL,
				PROTOCOL_ICON_COLUMN, prpl_icon,
				PROTOCOL_ICON_VISIBLE_COLUMN, purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"),
				NAME_COLUMN, mark,
				BGCOLOR_COLUMN, bgcolor,
				GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
				-1);

		g_free(mark);
		if(emblem)
			g_object_unref(emblem);
		if(status)
			g_object_unref(status);
		if(avatar)
			g_object_unref(avatar);
		if(prpl_icon)
			g_object_unref(prpl_icon);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}
}

static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node)
{
	if (list)
		gtkblist = PIDGIN_BLIST(list);
	if(!gtkblist || !gtkblist->treeview || !node)
		return;

	if (node->ui_data == NULL)
		pidgin_blist_new_node(node);

	switch(node->type) {
		case PURPLE_BLIST_GROUP_NODE:
			pidgin_blist_update_group(list, node);
			break;
		case PURPLE_BLIST_CONTACT_NODE:
			pidgin_blist_update_contact(list, node);
			break;
		case PURPLE_BLIST_BUDDY_NODE:
			pidgin_blist_update_buddy(list, node, TRUE);
			break;
		case PURPLE_BLIST_CHAT_NODE:
			pidgin_blist_update_chat(list, node);
			break;
		case PURPLE_BLIST_OTHER_NODE:
			return;
	}

}

static void pidgin_blist_destroy(PurpleBuddyList *list)
{
	PidginBuddyListPrivate *priv;

	if (!list || !list->ui_data)
		return;

	g_return_if_fail(list->ui_data == gtkblist);

	purple_signals_disconnect_by_handle(gtkblist);

	if (gtkblist->headline_close)
		gdk_pixbuf_unref(gtkblist->headline_close);

	gtk_widget_destroy(gtkblist->window);

	pidgin_blist_tooltip_destroy();

	if (gtkblist->refresh_timer)
		purple_timeout_remove(gtkblist->refresh_timer);
	if (gtkblist->timeout)
		g_source_remove(gtkblist->timeout);
	if (gtkblist->drag_timeout)
		g_source_remove(gtkblist->drag_timeout);

	g_hash_table_destroy(gtkblist->connection_errors);
	gtkblist->refresh_timer = 0;
	gtkblist->timeout = 0;
	gtkblist->drag_timeout = 0;
	gtkblist->window = gtkblist->vbox = gtkblist->treeview = NULL;
	g_object_unref(G_OBJECT(gtkblist->treemodel));
	gtkblist->treemodel = NULL;
	g_object_unref(G_OBJECT(gtkblist->ift));
	g_object_unref(G_OBJECT(gtkblist->empty_avatar));

	gdk_cursor_unref(gtkblist->hand_cursor);
	gdk_cursor_unref(gtkblist->arrow_cursor);
	gtkblist->hand_cursor = NULL;
	gtkblist->arrow_cursor = NULL;

	priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	if (priv->current_theme)
		g_object_unref(priv->current_theme);
	g_free(priv);

	g_free(gtkblist);
	accountmenu = NULL;
	gtkblist = NULL;
	purple_prefs_disconnect_by_handle(pidgin_blist_get_handle());
}

static void pidgin_blist_set_visible(PurpleBuddyList *list, gboolean show)
{
	if (!(gtkblist && gtkblist->window))
		return;

	if (show) {
		if(!PIDGIN_WINDOW_ICONIFIED(gtkblist->window) && !GTK_WIDGET_VISIBLE(gtkblist->window))
			purple_signal_emit(pidgin_blist_get_handle(), "gtkblist-unhiding", gtkblist);
		pidgin_blist_restore_position();
		gtk_window_present(GTK_WINDOW(gtkblist->window));
	} else {
		if(visibility_manager_count) {
			purple_signal_emit(pidgin_blist_get_handle(), "gtkblist-hiding", gtkblist);
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
	static GList *list = NULL;
	char *tmp2;
	PurpleGroup *g;
	PurpleBlistNode *gnode;

	g_list_free(list);
	list = NULL;

	if (purple_get_blist()->root == NULL)
	{
		list  = g_list_append(list, (gpointer)_("Buddies"));
	}
	else
	{
		for (gnode = purple_get_blist()->root;
			 gnode != NULL;
			 gnode = gnode->next)
		{
			if (PURPLE_BLIST_NODE_IS_GROUP(gnode))
			{
				g    = (PurpleGroup *)gnode;
				tmp2 = g->name;
				list  = g_list_append(list, tmp2);
			}
		}
	}

	return list;
}

static void
add_buddy_select_account_cb(GObject *w, PurpleAccount *account,
							PidginAddBuddyData *data)
{
	/* Save our account */
	data->rq_data.account = account;
}

static void
destroy_add_buddy_dialog_cb(GtkWidget *win, PidginAddBuddyData *data)
{
	g_free(data);
}

static void
add_buddy_cb(GtkWidget *w, int resp, PidginAddBuddyData *data)
{
	const char *grp, *who, *whoalias;
	PurpleAccount *account;
	PurpleGroup *g;
	PurpleBuddy *b;
	PurpleConversation *c;
	PurpleBuddyIcon *icon;

	if (resp == GTK_RESPONSE_OK)
	{
		who = gtk_entry_get_text(GTK_ENTRY(data->entry));
		grp = pidgin_text_combo_box_entry_get_text(data->combo);
		whoalias = gtk_entry_get_text(GTK_ENTRY(data->entry_for_alias));
		if (*whoalias == '\0')
			whoalias = NULL;

		account = data->rq_data.account;

		g = NULL;
		if ((grp != NULL) && (*grp != '\0'))
		{
			if ((g = purple_find_group(grp)) == NULL)
			{
				g = purple_group_new(grp);
				purple_blist_add_group(g, NULL);
			}

			b = purple_find_buddy_in_group(account, who, g);
		}
		else if ((b = purple_find_buddy(account, who)) != NULL)
		{
			g = purple_buddy_get_group(b);
		}

		if (b == NULL)
		{
			b = purple_buddy_new(account, who, whoalias);
			purple_blist_add_buddy(b, NULL, g, NULL);
		}

		purple_account_add_buddy(account, b);

		/* Offer to merge people with the same alias. */
		if (whoalias != NULL && g != NULL)
			gtk_blist_auto_personize((PurpleBlistNode *)g, whoalias);

		/*
		 * XXX
		 * It really seems like it would be better if the call to
		 * purple_account_add_buddy() and purple_conversation_update() were done in
		 * blist.c, possibly in the purple_blist_add_buddy() function.  Maybe
		 * purple_account_add_buddy() should be renamed to
		 * purple_blist_add_new_buddy() or something, and have it call
		 * purple_blist_add_buddy() after it creates it.  --Mark
		 *
		 * No that's not good.  blist.c should only deal with adding nodes to the
		 * local list.  We need a new, non-gtk file that calls both
		 * purple_account_add_buddy and purple_blist_add_buddy().
		 * Or something.  --Mark
		 */

		c = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, data->rq_data.account);
		if (c != NULL) {
			icon = purple_conv_im_get_icon(PURPLE_CONV_IM(c));
			if (icon != NULL)
				purple_buddy_icon_update(icon);
		}
	}

	gtk_widget_destroy(data->rq_data.window);
}

static void
pidgin_blist_request_add_buddy(PurpleAccount *account, const char *username,
								 const char *group, const char *alias)
{
	PidginAddBuddyData *data = g_new0(PidginAddBuddyData, 1);

	make_blist_request_dialog((PidginBlistRequestData *)data,
		(account != NULL
			? account : purple_connection_get_account(purple_connections_get_all()->data)),
		_("Add Buddy"), "add_buddy",
		_("Add a buddy.\n"),
		G_CALLBACK(add_buddy_select_account_cb), NULL,
		G_CALLBACK(add_buddy_cb));
	gtk_dialog_add_buttons(GTK_DIALOG(data->rq_data.window),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_ADD, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->rq_data.window),
			GTK_RESPONSE_OK);

	g_signal_connect(G_OBJECT(data->rq_data.window), "destroy",
	                 G_CALLBACK(destroy_add_buddy_dialog_cb), data);

	data->entry = gtk_entry_new();

	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("Buddy's _username:"),
		data->rq_data.sg, data->entry, TRUE, NULL);
	gtk_widget_grab_focus(data->entry);

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry), username);
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window),
		                                  GTK_RESPONSE_OK, FALSE);

	gtk_entry_set_activates_default (GTK_ENTRY(data->entry), TRUE);

	g_signal_connect(G_OBJECT(data->entry), "changed",
	                 G_CALLBACK(pidgin_set_sensitive_if_input),
	                 data->rq_data.window);

	data->entry_for_alias = gtk_entry_new();
	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("(Optional) A_lias:"),
	                          data->rq_data.sg, data->entry_for_alias, TRUE,
	                          NULL);

	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry_for_alias), alias);

	if (username != NULL)
		gtk_widget_grab_focus(GTK_WIDGET(data->entry_for_alias));

	data->combo = pidgin_text_combo_box_entry_new(group, groups_tree());
	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("Add buddy to _group:"),
	                          data->rq_data.sg, data->combo, TRUE, NULL);

	gtk_widget_show_all(data->rq_data.window);
}

static void
add_chat_cb(GtkWidget *w, PidginAddChatData *data)
{
	GList *tmp;
	PurpleChat *chat;
	GHashTable *components;

	components = g_hash_table_new_full(g_str_hash, g_str_equal,
									   g_free, g_free);

	for (tmp = data->chat_data.entries; tmp; tmp = tmp->next)
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
			const char *value = gtk_entry_get_text(tmp->data);

			if (*value != '\0')
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup(value));
		}
	}

	chat = purple_chat_new(data->chat_data.rq_data.account,
	                       gtk_entry_get_text(GTK_ENTRY(data->alias_entry)),
	                       components);

	if (chat != NULL) {
		PurpleGroup *group;
		const char *group_name;

		group_name = pidgin_text_combo_box_entry_get_text(data->group_combo);

		group = NULL;
		if ((group_name != NULL) && (*group_name != '\0') &&
		    ((group = purple_find_group(group_name)) == NULL))
		{
			group = purple_group_new(group_name);
			purple_blist_add_group(group, NULL);
		}

		purple_blist_add_chat(chat, group, NULL);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->autojoin)))
			purple_blist_node_set_bool((PurpleBlistNode*)chat, "gtk-autojoin", TRUE);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->persistent)))
			purple_blist_node_set_bool((PurpleBlistNode*)chat, "gtk-persistent", TRUE);
	}

	gtk_widget_destroy(data->chat_data.rq_data.window);
	g_free(data->chat_data.default_chat_name);
	g_list_free(data->chat_data.entries);
	g_free(data);
}

static void
add_chat_resp_cb(GtkWidget *w, int resp, PidginAddChatData *data)
{
	if (resp == GTK_RESPONSE_OK)
	{
		add_chat_cb(NULL, data);
	}
	else if (resp == 1)
	{
		pidgin_roomlist_dialog_show_with_account(data->chat_data.rq_data.account);
	}
	else
	{
		gtk_widget_destroy(data->chat_data.rq_data.window);
		g_free(data->chat_data.default_chat_name);
		g_list_free(data->chat_data.entries);
		g_free(data);
	}
}

static void
pidgin_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
								const char *alias, const char *name)
{
	PidginAddChatData *data;
	GList *l;
	PurpleConnection *gc;
	GtkBox *vbox;

	if (account != NULL) {
		gc = purple_account_get_connection(account);

		if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat == NULL) {
			purple_notify_error(gc, NULL, _("This protocol does not support chat rooms."), NULL);
			return;
		}
	} else {
		/* Find an account with chat capabilities */
		for (l = purple_connections_get_all(); l != NULL; l = l->next) {
			gc = (PurpleConnection *)l->data;

			if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat != NULL) {
				account = purple_connection_get_account(gc);
				break;
			}
		}

		if (account == NULL) {
			purple_notify_error(NULL, NULL,
							  _("You are not currently signed on with any "
								"protocols that have the ability to chat."), NULL);
			return;
		}
	}

	data = g_new0(PidginAddChatData, 1);
	vbox = GTK_BOX(make_blist_request_dialog((PidginBlistRequestData *)data, account,
			_("Add Chat"), "add_chat",
			_("Please enter an alias, and the appropriate information "
			  "about the chat you would like to add to your buddy list.\n"),
			G_CALLBACK(chat_select_account_cb), chat_account_filter_func,
			G_CALLBACK(add_chat_resp_cb)));
	gtk_dialog_add_buttons(GTK_DIALOG(data->chat_data.rq_data.window),
		_("Room List"), 1,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_ADD, GTK_RESPONSE_OK,
		NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->chat_data.rq_data.window),
			GTK_RESPONSE_OK);

	data->chat_data.default_chat_name = g_strdup(name);

	rebuild_chat_entries((PidginChatData *)data, name);

	data->alias_entry = gtk_entry_new();
	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->alias_entry), alias);
	gtk_entry_set_activates_default(GTK_ENTRY(data->alias_entry), TRUE);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("A_lias:"),
	                          data->chat_data.rq_data.sg, data->alias_entry,
	                          TRUE, NULL);
	if (name != NULL)
		gtk_widget_grab_focus(data->alias_entry);

	data->group_combo = pidgin_text_combo_box_entry_new(group ? group->name : NULL, groups_tree());
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Group:"),
	                          data->chat_data.rq_data.sg, data->group_combo,
	                          TRUE, NULL);

	data->autojoin = gtk_check_button_new_with_mnemonic(_("Auto_join when account connects."));
	data->persistent = gtk_check_button_new_with_mnemonic(_("_Remain in chat after window is closed."));
	gtk_box_pack_start(GTK_BOX(vbox), data->autojoin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), data->persistent, FALSE, FALSE, 0);

	gtk_widget_show_all(data->chat_data.rq_data.window);
}

static void
add_group_cb(PurpleConnection *gc, const char *group_name)
{
	PurpleGroup *group;

	if ((group_name == NULL) || (*group_name == '\0'))
		return;

	group = purple_group_new(group_name);
	purple_blist_add_group(group, NULL);
}

static void
pidgin_blist_request_add_group(void)
{
	purple_request_input(NULL, _("Add Group"), NULL,
					   _("Please enter the name of the group to be added."),
					   NULL, FALSE, FALSE, NULL,
					   _("Add"), G_CALLBACK(add_group_cb),
					   _("Cancel"), NULL,
					   NULL, NULL, NULL,
					   NULL);
}

void
pidgin_blist_toggle_visibility()
{
	if (gtkblist && gtkblist->window) {
		if (GTK_WIDGET_VISIBLE(gtkblist->window)) {
			/* make the buddy list visible if it is iconified or if it is
			 * obscured and not currently focused (the focus part ensures
			 * that we do something reasonable if the buddy list is obscured
			 * by a window set to always be on top), otherwise hide the
			 * buddy list
			 */
			purple_blist_set_visible(PIDGIN_WINDOW_ICONIFIED(gtkblist->window) ||
					((gtk_blist_visibility != GDK_VISIBILITY_UNOBSCURED) &&
					!gtk_blist_focused));
		} else {
			purple_blist_set_visible(TRUE);
		}
	}
}

void
pidgin_blist_visibility_manager_add()
{
	visibility_manager_count++;
	purple_debug_info("gtkblist", "added visibility manager: %d\n", visibility_manager_count);
}

void
pidgin_blist_visibility_manager_remove()
{
	if (visibility_manager_count)
		visibility_manager_count--;
	if (!visibility_manager_count)
		purple_blist_set_visible(TRUE);
	purple_debug_info("gtkblist", "removed visibility manager: %d\n", visibility_manager_count);
}

void pidgin_blist_add_alert(GtkWidget *widget)
{
	gtk_container_add(GTK_CONTAINER(gtkblist->scrollbook), widget);
	set_urgent();
}

void
pidgin_blist_set_headline(const char *text, GdkPixbuf *pixbuf, GCallback callback,
			gpointer user_data, GDestroyNotify destroy)
{
	/* Destroy any existing headline first */
	if (gtkblist->headline_destroy)
		gtkblist->headline_destroy(gtkblist->headline_data);

	gtk_label_set_markup(GTK_LABEL(gtkblist->headline_label), text);
	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkblist->headline_image), pixbuf);

	gtkblist->headline_callback = callback;
	gtkblist->headline_data = user_data;
	gtkblist->headline_destroy = destroy;
	if (text != NULL || pixbuf != NULL) {
		set_urgent();
		gtk_widget_show_all(gtkblist->headline_hbox);
	} else {
		gtk_widget_hide(gtkblist->headline_hbox);
	}
}


static void
set_urgent(void)
{
	if (gtkblist->window && !GTK_WIDGET_HAS_FOCUS(gtkblist->window))
		pidgin_set_urgent(GTK_WINDOW(gtkblist->window), TRUE);
}

static PurpleBlistUiOps blist_ui_ops =
{
	pidgin_blist_new_list,
	pidgin_blist_new_node,
	pidgin_blist_show,
	pidgin_blist_update,
	pidgin_blist_remove,
	pidgin_blist_destroy,
	pidgin_blist_set_visible,
	pidgin_blist_request_add_buddy,
	pidgin_blist_request_add_chat,
	pidgin_blist_request_add_group,
	NULL,
	NULL,
	NULL,
	NULL
};


PurpleBlistUiOps *
pidgin_blist_get_ui_ops(void)
{
	return &blist_ui_ops;
}

PidginBuddyList *pidgin_blist_get_default_gtk_blist()
{
	return gtkblist;
}

static void account_signon_cb(PurpleConnection *gc, gpointer z)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBlistNode *gnode, *cnode;
	for(gnode = purple_get_blist()->root; gnode; gnode = gnode->next)
	{
		if(!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next)
		{
			PurpleChat *chat;

			if(!PURPLE_BLIST_NODE_IS_CHAT(cnode))
				continue;

			chat = (PurpleChat *)cnode;

			if(chat->account != account)
				continue;

			if(purple_blist_node_get_bool((PurpleBlistNode*)chat, "gtk-autojoin") ||
					(purple_blist_node_get_string((PurpleBlistNode*)chat,
					 "gtk-autojoin") != NULL))
				serv_join_chat(gc, chat->components);
		}
	}
}

void *
pidgin_blist_get_handle() {
	static int handle;

	return &handle;
}

static gboolean buddy_signonoff_timeout_cb(PurpleBuddy *buddy)
{
	struct _pidgin_blist_node *gtknode = ((PurpleBlistNode*)buddy)->ui_data;

	gtknode->recent_signonoff = FALSE;
	gtknode->recent_signonoff_timer = 0;

	pidgin_blist_update(NULL, (PurpleBlistNode*)buddy);

	return FALSE;
}

static void buddy_signonoff_cb(PurpleBuddy *buddy)
{
	struct _pidgin_blist_node *gtknode;

	if(!((PurpleBlistNode*)buddy)->ui_data) {
		pidgin_blist_new_node((PurpleBlistNode*)buddy);
	}

	gtknode = ((PurpleBlistNode*)buddy)->ui_data;

	gtknode->recent_signonoff = TRUE;

	if(gtknode->recent_signonoff_timer > 0)
		purple_timeout_remove(gtknode->recent_signonoff_timer);
	gtknode->recent_signonoff_timer = purple_timeout_add_seconds(10,
			(GSourceFunc)buddy_signonoff_timeout_cb, buddy);
}

void
pidgin_blist_set_theme(PidginBlistTheme *theme)
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);
	PurpleBuddyList *list = purple_get_blist();

	if (theme != NULL)
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/blist/theme",
				purple_theme_get_name(PURPLE_THEME(theme)));
	else
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/blist/theme", "");

	if (priv->current_theme)
		g_object_unref(priv->current_theme);

	priv->current_theme = theme ? g_object_ref(theme) : NULL;

	pidgin_blist_build_layout(list);

	pidgin_blist_refresh(list);
}


PidginBlistTheme *
pidgin_blist_get_theme()
{
	PidginBuddyListPrivate *priv = PIDGIN_BUDDY_LIST_GET_PRIVATE(gtkblist);

	return priv->current_theme;
}

void pidgin_blist_init(void)
{
	void *gtk_blist_handle = pidgin_blist_get_handle();

	cached_emblems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						gtk_blist_handle, PURPLE_CALLBACK(account_signon_cb),
						NULL);

	/* Initialize prefs */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/blist");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", FALSE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/sort_type", "alphabetical");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/x", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/y", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/width", 250); /* Golden ratio, baby */
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/height", 405); /* Golden ratio, baby */
#if !GTK_CHECK_VERSION(2,14,0)
	/* This pref is used in pidgintooltip.c. */
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/tooltip_delay", 500);
#endif
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/theme", "");

	purple_theme_manager_register_type(g_object_new(PIDGIN_TYPE_BLIST_THEME_LOADER, "type", "blist", NULL));

	/* Register our signals */
	purple_signal_register(gtk_blist_handle, "gtkblist-hiding",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST));

	purple_signal_register(gtk_blist_handle, "gtkblist-unhiding",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST));

	purple_signal_register(gtk_blist_handle, "gtkblist-created",
	                     purple_marshal_VOID__POINTER, NULL, 1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST));

	purple_signal_register(gtk_blist_handle, "drawing-tooltip",
	                     purple_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST_NODE),
	                     purple_value_new_outgoing(PURPLE_TYPE_BOXED, "GString *"),
	                     purple_value_new(PURPLE_TYPE_BOOLEAN));

	purple_signal_register(gtk_blist_handle, "drawing-buddy",
						purple_marshal_POINTER__POINTER,
						purple_value_new(PURPLE_TYPE_STRING), 1,
						purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on",
			gtk_blist_handle, PURPLE_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off",
			gtk_blist_handle, PURPLE_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-privacy-changed",
			gtk_blist_handle, PURPLE_CALLBACK(pidgin_blist_update_privacy_cb), NULL);

}

void
pidgin_blist_uninit(void) {
	g_hash_table_destroy(cached_emblems);

	purple_signals_unregister_by_instance(pidgin_blist_get_handle());
	purple_signals_disconnect_by_handle(pidgin_blist_get_handle());
}

/*********************************************************************
 * Buddy List sorting functions                                      *
 *********************************************************************/

GList *pidgin_blist_get_sort_methods()
{
	return pidgin_blist_sort_methods;
}

void pidgin_blist_sort_method_reg(const char *id, const char *name, pidgin_blist_sort_function func)
{
	struct pidgin_blist_sort_method *method;

	g_return_if_fail(id != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(func != NULL);

	method = g_new0(struct pidgin_blist_sort_method, 1);
	method->id = g_strdup(id);
	method->name = g_strdup(name);
	method->func = func;
	pidgin_blist_sort_methods = g_list_append(pidgin_blist_sort_methods, method);
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_unreg(const char *id)
{
	GList *l = pidgin_blist_sort_methods;

	g_return_if_fail(id != NULL);

	while(l) {
		struct pidgin_blist_sort_method *method = l->data;
		if(!strcmp(method->id, id)) {
			pidgin_blist_sort_methods = g_list_delete_link(pidgin_blist_sort_methods, l);
			g_free(method->id);
			g_free(method->name);
			g_free(method);
			break;
		}
		l = l->next;
	}
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_set(const char *id){
	GList *l = pidgin_blist_sort_methods;

	if(!id)
		id = "none";

	while (l && strcmp(((struct pidgin_blist_sort_method*)l->data)->id, id))
		l = l->next;

	if (l) {
		current_sort_method = l->data;
	} else if (!current_sort_method) {
		pidgin_blist_sort_method_set("none");
		return;
	}
	if (!strcmp(id, "none")) {
		redo_buddy_list(purple_get_blist(), TRUE, FALSE);
	} else {
		redo_buddy_list(purple_get_blist(), FALSE, FALSE);
	}
}

/******************************************
 ** Sort Methods
 ******************************************/

static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter parent_iter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	PurpleBlistNode *sibling = node->prev;
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

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	const char *my_name;

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		my_name = purple_contact_get_alias((PurpleContact*)node);
	} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
		my_name = purple_chat_get_name((PurpleChat*)node);
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
		PurpleBlistNode *n;
		const char *this_name;
		int cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);

		if(PURPLE_BLIST_NODE_IS_CONTACT(n)) {
			this_name = purple_contact_get_alias((PurpleContact*)n);
		} else if(PURPLE_BLIST_NODE_IS_CHAT(n)) {
			this_name = purple_chat_get_name((PurpleChat*)n);
		} else {
			this_name = NULL;
		}

		cmp = purple_utf8_strcasecmp(my_name, this_name);

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

static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	PurpleBuddy *my_buddy, *this_buddy;

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		my_buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
	} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
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
		PurpleBlistNode *n;
		gint name_cmp;
		gint presence_cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);

		if(PURPLE_BLIST_NODE_IS_CONTACT(n)) {
			this_buddy = purple_contact_get_priority_buddy((PurpleContact*)n);
		} else {
			this_buddy = NULL;
		}

		name_cmp = purple_utf8_strcasecmp(
			purple_contact_get_alias(purple_buddy_get_contact(my_buddy)),
			(this_buddy
			 ? purple_contact_get_alias(purple_buddy_get_contact(this_buddy))
			 : NULL));

		presence_cmp = purple_presence_compare(
			purple_buddy_get_presence(my_buddy),
			this_buddy ? purple_buddy_get_presence(this_buddy) : NULL);

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

static void sort_method_log_activity(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	int activity_score = 0, this_log_activity_score = 0;
	const char *buddy_name, *this_buddy_name;

	if(cur && (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter) == 1)) {
		*iter = *cur;
		return;
	}

	if(PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBlistNode *n;
		PurpleBuddy *buddy;
		for (n = node->child; n; n = n->next) {
			buddy = (PurpleBuddy*)n;
			activity_score += purple_log_get_activity_score(PURPLE_LOG_IM, buddy->name, buddy->account);
		}
		buddy_name = purple_contact_get_alias((PurpleContact*)node);
	} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
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
		PurpleBlistNode *n;
		PurpleBlistNode *n2;
		PurpleBuddy *buddy;
		int cmp;

		val.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &val);
		n = g_value_get_pointer(&val);
		this_log_activity_score = 0;

		if(PURPLE_BLIST_NODE_IS_CONTACT(n)) {
			for (n2 = n->child; n2; n2 = n2->next) {
                        	buddy = (PurpleBuddy*)n2;
				this_log_activity_score += purple_log_get_activity_score(PURPLE_LOG_IM, buddy->name, buddy->account);
			}
			this_buddy_name = purple_contact_get_alias((PurpleContact*)n);
		} else {
			this_buddy_name = NULL;
		}

		cmp = purple_utf8_strcasecmp(buddy_name, this_buddy_name);

		if (!PURPLE_BLIST_NODE_IS_CONTACT(n) || activity_score > this_log_activity_score ||
				((activity_score == this_log_activity_score) &&
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

static void
plugin_act(GtkObject *obj, PurplePluginAction *pam)
{
	if (pam && pam->callback)
		pam->callback(pam);
}

static void
build_plugin_actions(GtkWidget *menu, PurplePlugin *plugin,
		gpointer context)
{
	GtkWidget *menuitem;
	PurplePluginAction *action = NULL;
	GList *actions, *l;

	actions = PURPLE_PLUGIN_ACTIONS(plugin, context);

	for (l = actions; l != NULL; l = l->next)
	{
		if (l->data)
		{
			action = (PurplePluginAction *) l->data;
			action->plugin = plugin;
			action->context = context;

			menuitem = gtk_menu_item_new_with_label(action->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(plugin_act), action);
			g_object_set_data_full(G_OBJECT(menuitem), "plugin_action",
								   action,
								   (GDestroyNotify)purple_plugin_action_free);
			gtk_widget_show(menuitem);
		}
		else
			pidgin_separator(menu);
	}

	g_list_free(actions);
}

static void
modify_account_cb(GtkWidget *widget, gpointer data)
{
	pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, data);
}

static void
enable_account_cb(GtkCheckMenuItem *widget, gpointer data)
{
	PurpleAccount *account = data;
	const PurpleSavedStatus *saved_status;

	saved_status = purple_savedstatus_get_current();
	purple_savedstatus_activate_for_account(saved_status, account);

	purple_account_set_enabled(account, PIDGIN_UI, TRUE);
}

static void
disable_account_cb(GtkCheckMenuItem *widget, gpointer data)
{
	PurpleAccount *account = data;

	purple_account_set_enabled(account, PIDGIN_UI, FALSE);
}



void
pidgin_blist_update_accounts_menu(void)
{
	GtkWidget *menuitem = NULL, *submenu = NULL;
	GtkAccelGroup *accel_group = NULL;
	GList *l = NULL, *accounts = NULL;
	gboolean disabled_accounts = FALSE;
	gboolean enabled_accounts = FALSE;

	if (accountmenu == NULL)
		return;

	/* Clear the old Accounts menu */
	for (l = gtk_container_get_children(GTK_CONTAINER(accountmenu)); l; l = g_list_delete_link(l, l)) {
		menuitem = l->data;

		if (menuitem != gtk_item_factory_get_widget(gtkblist->ift, N_("/Accounts/Manage Accounts")))
			gtk_widget_destroy(menuitem);
	}

	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next) {
		char *buf = NULL;
		GtkWidget *image = NULL;
		PurpleAccount *account = NULL;
		GdkPixbuf *pixbuf = NULL;

		account = accounts->data;

		if(!purple_account_get_enabled(account, PIDGIN_UI)) {
			if (!disabled_accounts) {
				menuitem = gtk_menu_item_new_with_label(_("Enable Account"));
				gtk_menu_shell_append(GTK_MENU_SHELL(accountmenu), menuitem);

				submenu = gtk_menu_new();
				gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
				gtk_menu_set_accel_path(GTK_MENU(submenu), N_("<PurpleMain>/Accounts/Enable Account"));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

				disabled_accounts = TRUE;
			}

			buf = g_strconcat(purple_account_get_username(account), " (",
				purple_account_get_protocol_name(account), ")", NULL);
			menuitem = gtk_image_menu_item_new_with_label(buf);
			g_free(buf);
			pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
			if (pixbuf != NULL)
			{
				if (!purple_account_is_connected(account))
					gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);
				image = gtk_image_new_from_pixbuf(pixbuf);
				g_object_unref(G_OBJECT(pixbuf));
				gtk_widget_show(image);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
			}
			g_signal_connect(G_OBJECT(menuitem), "activate",
				G_CALLBACK(enable_account_cb), account);
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
		} else {
			enabled_accounts = TRUE;
		}
	}

	if (!enabled_accounts) {
		gtk_widget_show_all(accountmenu);
		return;
	}

	pidgin_separator(accountmenu);
	accel_group = gtk_menu_get_accel_group(GTK_MENU(accountmenu));

	for (accounts = purple_accounts_get_all(); accounts; accounts = accounts->next) {
		char *buf = NULL;
		char *accel_path_buf = NULL;
		GtkWidget *image = NULL;
		PurpleConnection *gc = NULL;
		PurpleAccount *account = NULL;
		GdkPixbuf *pixbuf = NULL;
		PurplePlugin *plugin = NULL;
		PurplePluginProtocolInfo *prpl_info;

		account = accounts->data;

		if (!purple_account_get_enabled(account, PIDGIN_UI))
			continue;

		buf = g_strconcat(purple_account_get_username(account), " (",
				purple_account_get_protocol_name(account), ")", NULL);
		menuitem = gtk_image_menu_item_new_with_label(buf);
		accel_path_buf = g_strconcat(N_("<PurpleMain>/Accounts/"), buf, NULL);
		g_free(buf);
		pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);
		if (pixbuf != NULL) {
			if (!purple_account_is_connected(account))
				gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf,
						0.0, FALSE);
			image = gtk_image_new_from_pixbuf(pixbuf);
			g_object_unref(G_OBJECT(pixbuf));
			gtk_widget_show(image);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
		}
		gtk_menu_shell_append(GTK_MENU_SHELL(accountmenu), menuitem);

		submenu = gtk_menu_new();
		gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
		gtk_menu_set_accel_path(GTK_MENU(submenu), accel_path_buf);
		g_free(accel_path_buf);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);


		menuitem = gtk_menu_item_new_with_mnemonic(_("_Edit Account"));
		g_signal_connect(G_OBJECT(menuitem), "activate",
				G_CALLBACK(modify_account_cb), account);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);

		pidgin_separator(submenu);

		gc = purple_account_get_connection(account);
		plugin = gc && PURPLE_CONNECTION_IS_CONNECTED(gc) ? gc->prpl : NULL;
		prpl_info = plugin ? PURPLE_PLUGIN_PROTOCOL_INFO(plugin) : NULL;
		
		if (prpl_info &&
		    (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, get_moods) ||
			 PURPLE_PLUGIN_HAS_ACTIONS(plugin))) {
			if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, get_moods) &&
			    gc->flags & PURPLE_CONNECTION_SUPPORT_MOODS) {

				if (purple_account_get_status(account, "mood")) {
					menuitem = gtk_menu_item_new_with_mnemonic(_("Set _Mood..."));
					g_signal_connect(G_OBJECT(menuitem), "activate",
					         	G_CALLBACK(set_mood_cb), account);
					gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
				}
			}
			if (PURPLE_PLUGIN_HAS_ACTIONS(plugin)) {
				build_plugin_actions(submenu, plugin, gc);
			}
		} else {
			menuitem = gtk_menu_item_new_with_label(_("No actions available"));
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
			gtk_widget_set_sensitive(menuitem, FALSE);
		}

		pidgin_separator(submenu);

		menuitem = gtk_menu_item_new_with_mnemonic(_("_Disable"));
		g_signal_connect(G_OBJECT(menuitem), "activate",
				G_CALLBACK(disable_account_cb), account);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
	}
	gtk_widget_show_all(accountmenu);
}

static GList *plugin_submenus = NULL;

void
pidgin_blist_update_plugin_actions(void)
{
	GtkWidget *menuitem, *submenu;
	PurplePlugin *plugin = NULL;
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
	for (l = purple_plugins_get_loaded(); l; l = l->next) {
		char *path;

		plugin = (PurplePlugin *) l->data;

		if (PURPLE_IS_PROTOCOL_PLUGIN(plugin))
			continue;

		if (!PURPLE_PLUGIN_HAS_ACTIONS(plugin))
			continue;

		menuitem = gtk_image_menu_item_new_with_label(_(plugin->info->name));
		gtk_menu_shell_append(GTK_MENU_SHELL(pluginmenu), menuitem);

		plugin_submenus = g_list_append(plugin_submenus, menuitem);

		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

		gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
		path = g_strdup_printf("%s/Tools/%s", gtkblist->ift->path, plugin->info->name);
		gtk_menu_set_accel_path(GTK_MENU(submenu), path);
		g_free(path);

		build_plugin_actions(submenu, plugin, NULL);
	}
	gtk_widget_show_all(pluginmenu);
}

static void
sortmethod_act(GtkCheckMenuItem *checkmenuitem, char *id)
{
	if (gtk_check_menu_item_get_active(checkmenuitem))
	{
		pidgin_set_cursor(gtkblist->window, GDK_WATCH);
		/* This is redundant. I think. */
		/* pidgin_blist_sort_method_set(id); */
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/blist/sort_type", id);

		pidgin_clear_cursor(gtkblist->window);
	}
}

void
pidgin_blist_update_sort_methods(void)
{
	GtkWidget *menuitem = NULL, *activeitem = NULL;
	PidginBlistSortMethod *method = NULL;
	GList *l;
	GSList *sl = NULL;
	GtkWidget *sortmenu;
	const char *m = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/sort_type");

	if ((gtkblist == NULL) || (gtkblist->ift == NULL))
		return;

	g_return_if_fail(m != NULL);

	sortmenu = gtk_item_factory_get_widget(gtkblist->ift, N_("/Buddies/Sort Buddies"));

	if (sortmenu == NULL)
		return;

	/* Clear the old menu */
	for (l = gtk_container_get_children(GTK_CONTAINER(sortmenu)); l; l = g_list_delete_link(l, l)) {
		menuitem = l->data;
		gtk_widget_destroy(GTK_WIDGET(menuitem));
	}

	for (l = pidgin_blist_sort_methods; l; l = l->next) {
		method = (PidginBlistSortMethod *) l->data;
		menuitem = gtk_radio_menu_item_new_with_label(sl, _(method->name));
		if (g_str_equal(m, method->id))
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
