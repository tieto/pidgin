/**
 * @file gntlog.c GNT Log viewer
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
#include <internal.h>
#include "finch.h"

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gnttextview.h>
#include <gnttree.h>
#include <gntwindow.h>

#include "account.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "request.h"
#include "util.h"

#include "gntlog.h"

static GHashTable *log_viewers = NULL;
static void populate_log_tree(FinchLogViewer *lv);
static FinchLogViewer *syslog_viewer = NULL;

struct log_viewer_hash_t {
	PurpleLogType type;
	char *username;
	PurpleAccount *account;
	PurpleContact *contact;
};

static guint log_viewer_hash(gconstpointer data)
{
	const struct log_viewer_hash_t *viewer = data;

	if (viewer->contact != NULL)
		return g_direct_hash(viewer->contact);

	if (viewer->account) {
		return g_str_hash(viewer->username) +
			g_str_hash(purple_account_get_username(viewer->account));
	}

	return g_direct_hash(viewer);
}

static gboolean log_viewer_equal(gconstpointer y, gconstpointer z)
{
	const struct log_viewer_hash_t *a, *b;
	int ret;
	char *normal;

	a = y;
	b = z;

	if (a->contact != NULL) {
		if (b->contact != NULL)
			return (a->contact == b->contact);
		else
			return FALSE;
	} else {
		if (b->contact != NULL)
			return FALSE;
	}

	if (a->username && b->username) {
		normal = g_strdup(purple_normalize(a->account, a->username));
		ret = (a->account == b->account) &&
			!strcmp(normal, purple_normalize(b->account, b->username));
		g_free(normal);
	} else {
		ret = (a == b);
	}

	return ret;
}

static const char *log_get_date(PurpleLog *log)
{
	if (log->tm)
		return purple_date_format_full(log->tm);
	else
		return purple_date_format_full(localtime(&log->time));
}

static void search_cb(GntWidget *button, FinchLogViewer *lv)
{
	const char *search_term = gnt_entry_get_text(GNT_ENTRY(lv->entry));
	GList *logs;

	if (!(*search_term)) {
		/* reset the tree */
		gnt_tree_remove_all(GNT_TREE(lv->tree));
		g_free(lv->search);
		lv->search = NULL;
		populate_log_tree(lv);
		return;
	}

	if (lv->search != NULL && !strcmp(lv->search, search_term)) {
		return;
	}

	g_free(lv->search);
	lv->search = g_strdup(search_term);

	gnt_tree_remove_all(GNT_TREE(lv->tree));
	gnt_text_view_clear(GNT_TEXT_VIEW(lv->text));

	for (logs = lv->logs; logs != NULL; logs = logs->next) {
		char *read = purple_log_read((PurpleLog*)logs->data, NULL);
		if (read && *read && purple_strcasestr(read, search_term)) {
			PurpleLog *log = logs->data;

			gnt_tree_add_row_last(GNT_TREE(lv->tree),
									log,
									gnt_tree_create_row(GNT_TREE(lv->tree), log_get_date(log)),
									NULL);
		}
		g_free(read);
	}

}

static void destroy_cb(GntWidget *w, struct log_viewer_hash_t *ht)
{
	FinchLogViewer *lv = syslog_viewer;

	if (ht != NULL) {
		lv = g_hash_table_lookup(log_viewers, ht);
		g_hash_table_remove(log_viewers, ht);

		g_free(ht->username);
		g_free(ht);
	} else
		syslog_viewer = NULL;

	purple_request_close_with_handle(lv);

	g_list_foreach(lv->logs, (GFunc)purple_log_free, NULL);
	g_list_free(lv->logs);

	g_free(lv->search);
	g_free(lv);

	gnt_widget_destroy(w);
}

static void log_select_cb(GntWidget *w, gpointer old, gpointer new, FinchLogViewer *viewer)
{
	GntTree *tree = GNT_TREE(w);
	PurpleLog *log = NULL;
	PurpleLogReadFlags flags;
	char *read = NULL, *strip, *newline;

	if (!viewer->search && !gnt_tree_get_parent_key(tree, new))
		return;

	log = (PurpleLog *)new;

	if (log == NULL)
		return;

	if (log->type != PURPLE_LOG_SYSTEM) {
		char *title;
		if (log->type == PURPLE_LOG_CHAT)
			title = g_strdup_printf(_("Conversation in %s on %s"),
									log->name, log_get_date(log));
		else
			title = g_strdup_printf(_("Conversation with %s on %s"),
									log->name, log_get_date(log));

		gnt_label_set_text(GNT_LABEL(viewer->label), title);
		g_free(title);
	}

	read = purple_log_read(log, &flags);
	if (flags != PURPLE_LOG_READ_NO_NEWLINE) {
		newline = purple_strdup_withhtml(read);
		strip = purple_markup_strip_html(newline);
		g_free(newline);
	} else {
		strip = purple_markup_strip_html(read);
	}
	viewer->flags = flags;

	purple_signal_emit(finch_log_get_handle(), "log-displaying", viewer, log);

	gnt_text_view_clear(GNT_TEXT_VIEW(viewer->text));
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(viewer->text), strip, GNT_TEXT_FLAG_NORMAL);
	g_free(read);
	g_free(strip);
}

/* I want to make this smarter, but haven't come up with a cool algorithm to do so, yet.
 * I want the tree to be divided into groups like "Today," "Yesterday," "Last week,"
 * "August," "2002," etc. based on how many conversation took place in each subdivision.
 *
 * For now, I'll just make it a flat list.
 */
static void populate_log_tree(FinchLogViewer *lv)
     /* Logs are made from trees in real life.
        This is a tree made from logs */
{
	const char *pmonth;
	char *month = NULL;
	char prev_top_month[30] = "";
	GList *logs = lv->logs;

	while (logs != NULL) {
		PurpleLog *log = logs->data;

		pmonth = purple_utf8_strftime(_("%B %Y"),
		                           log->tm ? log->tm : localtime(&log->time));

		if (strcmp(pmonth, prev_top_month) != 0) {
			month = g_strdup(pmonth);
			/* top level */
			gnt_tree_add_row_last(GNT_TREE(lv->tree),
									month,
									gnt_tree_create_row(GNT_TREE(lv->tree), month),
									NULL);
			gnt_tree_set_expanded(GNT_TREE(lv->tree), month, FALSE);

			g_strlcpy(prev_top_month, month, sizeof(prev_top_month));
		}

		/* sub */
		gnt_tree_add_row_last(GNT_TREE(lv->tree),
								log,
								gnt_tree_create_row(GNT_TREE(lv->tree), log_get_date(log)),
								month);

		logs = logs->next;
	}
}

static FinchLogViewer *display_log_viewer(struct log_viewer_hash_t *ht, GList *logs,
						const char *title, int log_size)
{
	FinchLogViewer *lv;
	char *text;
	GntWidget *vbox, *hbox;
	GntWidget *size_label;

	if (logs == NULL)
	{
		/* No logs were found. */
		const char *log_preferences = NULL;

		if (ht == NULL) {
			if (!purple_prefs_get_bool("/purple/logging/log_system"))
				log_preferences = _("System events will only be logged if the \"Log all status changes to system log\" preference is enabled.");
		} else {
			if (ht->type == PURPLE_LOG_IM) {
				if (!purple_prefs_get_bool("/purple/logging/log_ims"))
					log_preferences = _("Instant messages will only be logged if the \"Log all instant messages\" preference is enabled.");
			} else if (ht->type == PURPLE_LOG_CHAT) {
				if (!purple_prefs_get_bool("/purple/logging/log_chats"))
					log_preferences = _("Chats will only be logged if the \"Log all chats\" preference is enabled.");
			}
			g_free(ht->username);
			g_free(ht);
		}

		purple_notify_info(NULL, title, _("No logs were found"), log_preferences);
		return NULL;
	}

	lv = g_new0(FinchLogViewer, 1);
	lv->logs = logs;

	if (ht != NULL)
		g_hash_table_insert(log_viewers, ht, lv);

	/* Window ***********/
	lv->window = gnt_vwindow_new(FALSE);
	gnt_box_set_title(GNT_BOX(lv->window), title);
	gnt_box_set_toplevel(GNT_BOX(lv->window), TRUE);
	gnt_box_set_pad(GNT_BOX(lv->window), 0);
	g_signal_connect(G_OBJECT(lv->window), "destroy", G_CALLBACK(destroy_cb), ht);

	vbox = gnt_vbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(lv->window), vbox);

	/* Label ************/
	text = g_strdup_printf("%s", title);
	lv->label = gnt_label_new_with_format(text, GNT_TEXT_FLAG_BOLD);
	g_free(text);
	gnt_box_add_widget(GNT_BOX(vbox), lv->label);

	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(vbox), hbox);
	/* List *************/
	lv->tree = gnt_tree_new();
	gnt_widget_set_size(lv->tree, 30, 0);
	populate_log_tree(lv);
	g_signal_connect (G_OBJECT(lv->tree), "selection-changed",
			G_CALLBACK (log_select_cb),
			lv);
	gnt_box_add_widget(GNT_BOX(hbox), lv->tree);

	/* Viewer ************/
	lv->text = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(hbox), lv->text);
	gnt_text_view_set_flag(GNT_TEXT_VIEW(lv->text), GNT_TEXT_VIEW_TOP_ALIGN);

	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(vbox), hbox);
	/* Log size ************/
	if (log_size) {
		char *sz_txt = purple_str_size_to_units(log_size);
		text = g_strdup_printf("%s %s", _("Total log size:"), sz_txt);
		size_label = gnt_label_new(text);
		gnt_box_add_widget(GNT_BOX(hbox), size_label);
		g_free(sz_txt);
		g_free(text);
	}

	/* Search box **********/
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Scroll/Search: ")));
	lv->entry = gnt_entry_new("");
	gnt_box_add_widget(GNT_BOX(hbox), lv->entry);
	g_signal_connect(GNT_ENTRY(lv->entry), "activate", G_CALLBACK(search_cb), lv);

	gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(lv->text), lv->entry);
	gnt_text_view_attach_pager_widget(GNT_TEXT_VIEW(lv->text), lv->entry);

	gnt_widget_show(lv->window);

	return lv;
}

static void
our_logging_blows(PurpleLogSet *set, PurpleLogSet *setagain, GList **list)
{
	/* The iteration happens on the first list. So we use the shorter list in front */
	if (set->type != PURPLE_LOG_IM)
		return;
	*list = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, set->name, set->account), *list);
}

void finch_log_show(PurpleLogType type, const char *username, PurpleAccount *account)
{
	struct log_viewer_hash_t *ht;
	FinchLogViewer *lv = NULL;
	const char *name = username;
	char *title;
	GList *logs = NULL;
	int size = 0;

	if (type != PURPLE_LOG_IM) {
		g_return_if_fail(account != NULL);
		g_return_if_fail(username != NULL);
	}

	ht = g_new0(struct log_viewer_hash_t, 1);

	ht->type = type;
	ht->username = g_strdup(username);
	ht->account = account;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gnt_window_present(lv->window);
		g_free(ht->username);
		g_free(ht);
		return;
	}

	if (type == PURPLE_LOG_CHAT) {
		PurpleChat *chat;

		chat = purple_blist_find_chat(account, username);
		if (chat != NULL)
			name = purple_chat_get_name(chat);

		title = g_strdup_printf(_("Conversations in %s"), name);
	} else {
		PurpleBuddy *buddy;

		if (username) {
			buddy = purple_blist_find_buddy(account, username);
			if (buddy != NULL)
				name = purple_buddy_get_contact_alias(buddy);
			title = g_strdup_printf(_("Conversations with %s"), name);
		} else {
			title = g_strdup(_("All Conversations"));
		}
	}

	if (username) {
		logs = purple_log_get_logs(type, username, account);
		size = purple_log_get_total_size(type, username, account);
	} else {
		/* This will happen only for IMs */
		GHashTable *table = purple_log_get_log_sets();
		g_hash_table_foreach(table, (GHFunc)our_logging_blows, &logs);
		g_hash_table_destroy(table);
		logs = g_list_sort(logs, purple_log_compare);
		size = 0;
	}

	display_log_viewer(ht, logs, title, size);

	g_free(title);
}

void finch_log_show_contact(PurpleContact *contact)
{
	struct log_viewer_hash_t *ht;
	PurpleBlistNode *child;
	FinchLogViewer *lv = NULL;
	GList *logs = NULL;
	const char *name = NULL;
	char *title;
	int total_log_size = 0;

	g_return_if_fail(contact != NULL);

	ht = g_new0(struct log_viewer_hash_t, 1);
	ht->type = PURPLE_LOG_IM;
	ht->contact = contact;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gnt_window_present(lv->window);
		g_free(ht);
		return;
	}

	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact); child;
			child = purple_blist_node_get_sibling_next(child)) {
		const char *name;
		PurpleAccount *account;
		if (!PURPLE_IS_BUDDY(child))
			continue;

		name = purple_buddy_get_name((PurpleBuddy *)child);
		account = purple_buddy_get_account((PurpleBuddy *)child);
		logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, name,
						account), logs);
		total_log_size += purple_log_get_total_size(PURPLE_LOG_IM, name, account);
	}
	logs = g_list_sort(logs, purple_log_compare);

	name = purple_contact_get_alias(contact);
	if (!name)
		name = purple_buddy_get_contact_alias(purple_contact_get_priority_buddy(contact));

	/* This will happen if the contact doesn't have an alias,
	 * and none of the contact's buddies are online.
	 * There is probably a better way to deal with this. */
	if (name == NULL) {
		child = purple_blist_node_get_first_child((PurpleBlistNode*)contact);
		if (child != NULL && PURPLE_IS_BUDDY(child))
			name = purple_buddy_get_contact_alias((PurpleBuddy *)child);
		if (name == NULL)
			name = "";
	}

	title = g_strdup_printf(_("Conversations with %s"), name);
	display_log_viewer(ht, logs, title, total_log_size);
	g_free(title);
}

void finch_syslog_show()
{
	GList *accounts = NULL;
	GList *logs = NULL;

	if (syslog_viewer != NULL) {
		gnt_window_present(syslog_viewer->window);
		return;
	}

	for(accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {

		PurpleAccount *account = (PurpleAccount *)accounts->data;
		if(purple_find_prpl(purple_account_get_protocol_id(account)) == NULL)
			continue;

		logs = g_list_concat(purple_log_get_system_logs(account), logs);
	}
	logs = g_list_sort(logs, purple_log_compare);

	syslog_viewer = display_log_viewer(NULL, logs, _("System Log"), 0);
}

/****************************************************************************
 * GNT LOG SUBSYSTEM *******************************************************
 ****************************************************************************/

void *
finch_log_get_handle(void)
{
	static int handle;

	return &handle;
}

void finch_log_init(void)
{
	void *handle = finch_log_get_handle();

	purple_signal_register(handle, "log-displaying",
	                     purple_marshal_VOID__POINTER_POINTER,
	                     G_TYPE_NONE, 2,
	                     G_TYPE_POINTER, /* (FinchLogViewer *) */
	                     PURPLE_TYPE_LOG);
}

void
finch_log_uninit(void)
{
	purple_signals_unregister_by_instance(finch_log_get_handle());
}
