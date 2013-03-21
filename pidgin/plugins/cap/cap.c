/*
 * Contact Availability Prediction plugin for Purple
 *
 * Copyright (C) 2006 Geoffrey Foster.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#include "cap.h"

static void generate_prediction(CapStatistics *statistics) {
	if(statistics->buddy) {
		if(statistics->prediction == NULL)
			statistics->prediction = g_malloc(sizeof(CapPrediction));
		statistics->prediction->probability = generate_prediction_for(statistics->buddy);
		statistics->prediction->generated_at = time(NULL);
	}
}

static double generate_prediction_for(PurpleBuddy *buddy) {
	double prediction = 1.0f;
	gboolean generated = FALSE;
	gchar *buddy_name = buddy->name;
	const gchar *protocol_id = purple_account_get_protocol_id(buddy->account);
	const gchar *account_id = purple_account_get_username(buddy->account);
	const gchar *status_id = purple_status_get_id(get_status_for(buddy));
	time_t t = time(NULL);
	struct tm *current_time = localtime(&t);
	int current_minute = current_time->tm_min + current_time->tm_hour * 60;
	int threshold = purple_prefs_get_int("/plugins/gtk/cap/threshold");
	int min_minute = (current_minute - threshold) % 1440;
	int max_minute = (current_minute + threshold) % 1440;
	char *sql, *sta_id = NULL;
	sqlite3_stmt *stmt = NULL;
	const char *tail = NULL;
	int rc;


	sql = sqlite3_mprintf("select sum(success_count) as successes, sum(failed_count) as failures "
		"from cap_msg_count where "
		"buddy=%Q and account=%Q and protocol=%Q and minute_val>=%d and minute_val<=%d;",
		buddy_name,	account_id,	protocol_id, min_minute, max_minute);
	rc = sqlite3_prepare(_db, sql, -1, &stmt, &tail);
	if(rc == SQLITE_OK) {
		int successes = 0;
		int failures = 0;
		if(stmt != NULL) {
			if(sqlite3_step(stmt) == SQLITE_ROW) {
				successes = sqlite3_column_int(stmt, 0);
				failures = sqlite3_column_int(stmt, 1);
				if(failures + successes > 0) {
					prediction *= ((double)successes/((double)(successes+failures)));
					generated = TRUE;
				}
			}
			sqlite3_finalize(stmt);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("select sum(success_count) as successes, sum(failed_count) as failures "
		"from cap_status_count where "
		"buddy=%Q and account=%Q and protocol=%Q and status=%Q;",
		buddy_name,	account_id,	protocol_id, status_id);
	rc = sqlite3_prepare(_db, sql, -1, &stmt, &tail);
	if(rc == SQLITE_OK) {
		int successes = 0;
		int failures = 0;
		if(stmt != NULL) {
			if(sqlite3_step(stmt) == SQLITE_ROW) {
				successes = sqlite3_column_int(stmt, 0);
				failures = sqlite3_column_int(stmt, 1);
				if(failures + successes > 0) {
					prediction *= ((double)successes/((double)(successes+failures)));
					generated = TRUE;
				}
			}
			sqlite3_finalize(stmt);
		}
	}
	sqlite3_free(sql);


	sta_id = purple_status_get_id(get_status_for(buddy));

	if(sta_id && !strcmp(sta_id, "offline")) {
		/* This is kind of stupid, change it. */
		if(prediction == 1.0f)
			prediction = 0.0f;
	}

	if(generated)
		return prediction;
	else
		return -1;
}

static CapStatistics * get_stats_for(PurpleBuddy *buddy) {
	CapStatistics *stats;

	g_return_val_if_fail(buddy != NULL, NULL);

	stats = g_hash_table_lookup(_buddy_stats, buddy->name);
	if(!stats) {
		stats = g_malloc0(sizeof(CapStatistics));
		stats->last_message = -1;
		stats->buddy = buddy;
		stats->last_seen = -1;
		stats->last_status_id = "";

		g_hash_table_insert(_buddy_stats, g_strdup(buddy->name), stats);
	} else {
		/* This may actually be a different PurpleBuddy than what is in stats.
		 * We replace stats->buddy to make sure we're looking at a valid pointer. */
		stats->buddy = buddy;
	}
	generate_prediction(stats);
	return stats;
}

static void destroy_stats(gpointer data) {
	CapStatistics *stats = data;
	g_free(stats->prediction);
	/* g_free(stats->hourly_usage); */
	/* g_free(stats->daily_usage); */
	if (stats->timeout_source_id != 0)
		purple_timeout_remove(stats->timeout_source_id);
	g_free(stats);
}

static void
insert_cap_msg_count_success(const char *buddy_name, const char *account, const char *protocol, int minute) {
	int rc;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_select = sqlite3_mprintf("SELECT * FROM cap_msg_count WHERE "
		"buddy=%Q AND account=%Q AND protocol=%Q AND minute_val=%d;",
		buddy_name, account, protocol, minute);
	char *sql_ins_up = NULL;

	purple_debug_info("cap", "%s\n", sql_select);

	sqlite3_prepare(_db, sql_select, -1, &stmt, &tail);

	rc = sqlite3_step(stmt);

	if(rc == SQLITE_DONE) {
		sql_ins_up = sqlite3_mprintf("INSERT INTO cap_msg_count VALUES (%Q, %Q, %Q, %d, %d, %d);",
			buddy_name, account, protocol, minute, 1, 0);
	} else if(rc == SQLITE_ROW) {
		sql_ins_up = sqlite3_mprintf("UPDATE cap_msg_count SET success_count=success_count+1 WHERE "
			"buddy=%Q AND account=%Q AND protocol=%Q AND minute_val=%d;",
			buddy_name, account, protocol, minute);
	} else {
		purple_debug_info("cap", "%d\n", rc);
		sqlite3_finalize(stmt);
		sqlite3_free(sql_select);
		return;
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sql_select);

	sqlite3_exec(_db, sql_ins_up, NULL, NULL, NULL);
	sqlite3_free(sql_ins_up);
}

static void
insert_cap_status_count_success(const char *buddy_name, const char *account, const char *protocol, const char *status_id) {
	int rc;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_select = sqlite3_mprintf("SELECT * FROM cap_status_count WHERE "
		"buddy=%Q AND account=%Q AND protocol=%Q AND status=%Q;",
		buddy_name, account, protocol, status_id);
	char *sql_ins_up = NULL;

	purple_debug_info("cap", "%s\n", sql_select);

	sqlite3_prepare(_db, sql_select, -1, &stmt, &tail);

	rc = sqlite3_step(stmt);

	if(rc == SQLITE_DONE) {
		sql_ins_up = sqlite3_mprintf("INSERT INTO cap_status_count VALUES (%Q, %Q, %Q, %Q, %d, %d);",
			buddy_name, account, protocol, status_id, 1, 0);
	} else if(rc == SQLITE_ROW) {
		sql_ins_up = sqlite3_mprintf("UPDATE cap_status_count SET success_count=success_count+1 WHERE "
			"buddy=%Q AND account=%Q AND protocol=%Q AND status=%Q;",
			buddy_name, account, protocol, status_id);
	} else {
		purple_debug_info("cap", "%d\n", rc);
		sqlite3_finalize(stmt);
		sqlite3_free(sql_select);
		return;
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sql_select);

	sqlite3_exec(_db, sql_ins_up, NULL, NULL, NULL);
	sqlite3_free(sql_ins_up);
}

static void
insert_cap_msg_count_failed(const char *buddy_name, const char *account, const char *protocol, int minute) {
	int rc;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_select = sqlite3_mprintf("SELECT * FROM cap_msg_count WHERE "
		"buddy=%Q AND account=%Q AND protocol=%Q AND minute_val=%d;",
		buddy_name, account, protocol, minute);
	char *sql_ins_up = NULL;

	purple_debug_info("cap", "%s\n", sql_select);

	sqlite3_prepare(_db, sql_select, -1, &stmt, &tail);

	rc = sqlite3_step(stmt);

	if(rc == SQLITE_DONE) {
		sql_ins_up = sqlite3_mprintf("INSERT INTO cap_msg_count VALUES (%Q, %Q, %Q, %d, %d, %d);",
			buddy_name, account, protocol, minute, 0, 1);
	} else if(rc == SQLITE_ROW) {
		sql_ins_up = sqlite3_mprintf("UPDATE cap_msg_count SET failed_count=failed_count+1 WHERE "
			"buddy=%Q AND account=%Q AND protocol=%Q AND minute_val=%d;",
			buddy_name, account, protocol, minute);
	} else {
		purple_debug_info("cap", "%d\n", rc);
		sqlite3_finalize(stmt);
		sqlite3_free(sql_select);
		return;
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sql_select);

	sqlite3_exec(_db, sql_ins_up, NULL, NULL, NULL);
	sqlite3_free(sql_ins_up);
}

static void
insert_cap_status_count_failed(const char *buddy_name, const char *account, const char *protocol, const char *status_id) {
	int rc;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_select = sqlite3_mprintf("SELECT * FROM cap_status_count WHERE "
		"buddy=%Q AND account=%Q AND protocol=%Q AND status=%Q;",
		buddy_name, account, protocol, status_id);
	char *sql_ins_up = NULL;

	purple_debug_info("cap", "%s\n", sql_select);

	sqlite3_prepare(_db, sql_select, -1, &stmt, &tail);

	rc = sqlite3_step(stmt);

	if(rc == SQLITE_DONE) {
		sql_ins_up = sqlite3_mprintf("INSERT INTO cap_status_count VALUES (%Q, %Q, %Q, %Q, %d, %d);",
			buddy_name, account, protocol, status_id, 0, 1);
	} else if(rc == SQLITE_ROW) {
		sql_ins_up = sqlite3_mprintf("UPDATE cap_status_count SET failed_count=failed_count+1 WHERE "
			"buddy=%Q AND account=%Q AND protocol=%Q AND status=%Q;",
			buddy_name, account, protocol, status_id);
	} else {
		purple_debug_info("cap", "%d\n", rc);
		sqlite3_finalize(stmt);
		sqlite3_free(sql_select);
		return;
	}

	sqlite3_finalize(stmt);
	sqlite3_free(sql_select);

	sqlite3_exec(_db, sql_ins_up, NULL, NULL, NULL);
	sqlite3_free(sql_ins_up);
}

static void insert_cap_success(CapStatistics *stats) {
	gchar *buddy_name = stats->buddy->name;
	const gchar *protocol_id = purple_account_get_protocol_id(stats->buddy->account);
	const gchar *account_id = purple_account_get_username(stats->buddy->account);
	const gchar *status_id = (stats->last_message_status_id) ?
		stats->last_message_status_id :
		purple_status_get_id(get_status_for(stats->buddy));
	struct tm *current_time;
	int minute;

	if(stats->last_message == -1) {
		time_t now = time(NULL);
		current_time = localtime(&now);
	} else {
		current_time = localtime(&stats->last_message);
	}
	minute = current_time->tm_min + current_time->tm_hour * 60;

	insert_cap_msg_count_success(buddy_name, account_id, protocol_id, minute);

	insert_cap_status_count_success(buddy_name, account_id, protocol_id, status_id);

	stats->last_message = -1;
	stats->last_message_status_id = NULL;
}

static void insert_cap_failure(CapStatistics *stats) {
	gchar *buddy_name = stats->buddy->name;
	const gchar *protocol_id = purple_account_get_protocol_id(stats->buddy->account);
	const gchar *account_id = purple_account_get_username(stats->buddy->account);
	const gchar *status_id = (stats->last_message_status_id) ?
		stats->last_message_status_id :
		purple_status_get_id(get_status_for(stats->buddy));
	struct tm *current_time = localtime(&stats->last_message);
	int minute = current_time->tm_min + current_time->tm_hour * 60;

	insert_cap_msg_count_failed(buddy_name, account_id, protocol_id, minute);

	insert_cap_status_count_failed(buddy_name, account_id, protocol_id, status_id);

	stats->last_message = -1;
	stats->last_message_status_id = NULL;
}

static gboolean max_message_difference_cb(gpointer data) {
	CapStatistics *stats = data;
	purple_debug_info("cap", "Max Message Difference timeout occurred\n");
	insert_cap_failure(stats);
	stats->timeout_source_id = 0;
	return FALSE;
}

/* Purple Signal Handlers */

/* sent-im-msg */
static void sent_im_msg(PurpleAccount *account, const char *receiver, const char *message) {
	PurpleBuddy *buddy;
	guint interval, words;
	CapStatistics *stats = NULL;

	buddy = purple_find_buddy(account, receiver);

	if (buddy == NULL)
		return;

	interval = purple_prefs_get_int("/plugins/gtk/cap/max_msg_difference") * 60;
	words = word_count(message);

	stats = get_stats_for(buddy);

	insert_word_count(purple_account_get_username(account), receiver, words);
	stats->last_message = time(NULL);
	stats->last_message_status_id = purple_status_get_id(get_status_for(buddy));
	if(stats->timeout_source_id != 0)
		purple_timeout_remove(stats->timeout_source_id);

	stats->timeout_source_id = purple_timeout_add_seconds(interval, max_message_difference_cb, stats);
}

/* received-im-msg */
static void
received_im_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags) {
	PurpleBuddy *buddy;
	CapStatistics *stats;
	/* guint words = word_count(message); */

	if (flags & PURPLE_MESSAGE_AUTO_RESP)
		return;

	buddy = purple_find_buddy(account, sender);

	if (buddy == NULL)
		return;

	stats = get_stats_for(buddy);

	/* insert_word_count(sender, buddy_name, words); */

	/* If we are waiting for a response from a prior message
	 * then cancel the timeout callback. */
	if(stats->timeout_source_id != 0) {
		purple_debug_info("cap", "Cancelling timeout callback\n");
		purple_timeout_remove(stats->timeout_source_id);
		stats->timeout_source_id = 0;
	}

	insert_cap_success(stats);

	/* Reset the last_message value */
	stats->last_message = -1;
	/* Reset the last status id value */
	stats->last_message_status_id = NULL;
}

/* buddy-status-changed */
static void buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old_status, PurpleStatus *status) {
	CapStatistics *stats = get_stats_for(buddy);
	insert_status_change_from_purple_status(stats, status);
}

/* buddy-signed-on */
static void buddy_signed_on(PurpleBuddy *buddy) {
	CapStatistics *stats = get_stats_for(buddy);

	/* If the statistic object existed but doesn't have a buddy pointer associated
	 * with it then reassociate one with it. The pointer being null is a result
	 * of a buddy with existing stats signing off and Purple sticking around. */
	if(!stats->buddy) {
		stats->buddy = buddy;
	}

	insert_status_change(stats);
}

/* buddy-signed-off */
static void buddy_signed_off(PurpleBuddy *buddy) {
	CapStatistics *stats = get_stats_for(buddy);

	/* We don't necessarily want to delete a buddies generated statistics every time they go offline.
	 * Instead we just set the buddy pointer to null so that when they come back online we can look
	 * them up again and continue using their statistics.
	 */
	insert_status_change(stats);
	/* stats->buddy = NULL; */
	stats->last_seen = time(NULL);
}

/* drawing-tooltip */
static void drawing_tooltip(PurpleBlistNode *node, GString *text, gboolean full) {
	if(node->type == PURPLE_BLIST_BUDDY_NODE) {
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		CapStatistics *stats = get_stats_for(buddy);
		/* get the probability that this buddy will respond and add to the tooltip */
		if(stats->prediction->probability >= 0.0) {
			g_string_append_printf(text, "\n<b>%s</b> %3.0f %%", _("Response Probability:"),
				100 * stats->prediction->probability);
		} else {
			g_string_append_printf(text, "\n<b>%s</b> ???", _("Response Probability:"));
		}
	}
}

/* signed-on */
static void signed_on(PurpleConnection *gc) {
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *my_purple_name = purple_account_get_username(account);
	gchar *my_name = g_strdup(my_purple_name);
	time_t *last_offline = g_hash_table_lookup(_my_offline_times, my_name);

	const gchar *account_id = purple_account_get_username(account);
	const gchar *protocol_id = purple_account_get_protocol_id(account);
	char *sql;

	sql = sqlite3_mprintf("insert into cap_my_usage values(%Q, %Q, %d, now());", account_id, protocol_id, 1);
	sqlite3_exec(_db, sql, NULL, NULL, NULL);
	sqlite3_free(sql);

	if(last_offline) {
		if(difftime(*last_offline, time(NULL)) > purple_prefs_get_int("/plugins/gtk/cap/max_seen_difference") * 60) {
			/* reset all of the last_message times to -1 */
			g_hash_table_foreach(_my_offline_times, reset_all_last_message_times, NULL);
		}
		g_hash_table_remove(_my_offline_times, my_name);
	}
	g_free(my_name);
}

/* signed-off */
static void signed_off(PurpleConnection *gc) {
	/* Here we record the time you (the user) sign off of an account.
	 * The account username is the key in the hashtable and the sign off time_t
	 * (equal to the sign off time) is the value. */
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *my_purple_name = purple_account_get_username(account);
	gchar *my_name = g_strdup(my_purple_name);
	time_t *offline_time = g_malloc(sizeof(time_t));
	const gchar *account_id = purple_account_get_username(account);
	const gchar *protocol_id = purple_account_get_protocol_id(account);
	char *sql;

	sql = sqlite3_mprintf("insert into cap_my_usage values(%Q, %Q, %d, now());", account_id, protocol_id, 0);
	sqlite3_exec(_db, sql, NULL, NULL, NULL);
	sqlite3_free(sql);

	time(offline_time);
	g_hash_table_insert(_my_offline_times, my_name, offline_time);
}

static void reset_all_last_message_times(gpointer key, gpointer value, gpointer user_data) {
	CapStatistics *stats = value;
	stats->last_message = -1;
}

static PurpleStatus * get_status_for(PurpleBuddy *buddy) {
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);
	return status;
}

static void create_tables() {
	int rc;
	rc = sqlite3_exec(_db,
		"CREATE TABLE IF NOT EXISTS cap_status ("
		"	buddy varchar(60) not null,"
		"	account varchar(60) not null,"
		"	protocol varchar(60) not null,"
		"	status varchar(60) not null,"
		"	event_time datetime not null,"
		"	primary key (buddy, account, protocol, event_time)"
		");",
		NULL, NULL, NULL);

	rc = sqlite3_exec(_db,
		"create table if not exists cap_message ("
		"	sender varchar(60) not null,"
		"	receiver varchar(60) not null,"
		"	account varchar(60) not null,"
		"	protocol varchar(60) not null,"
		"	word_count integer not null,"
		"	event_time datetime not null,"
		"	primary key (sender, account, protocol, receiver, event_time)"
		");",
		NULL, NULL, NULL);

	rc = sqlite3_exec(_db,
		"create table if not exists cap_msg_count ("
		"	buddy varchar(60) not null,"
		"	account varchar(60) not null,"
		"	protocol varchar(60) not null,"
		"	minute_val int not null,"
		"	success_count int not null,"
		"	failed_count int not null,"
		"	primary key (buddy, account, protocol, minute_val)"
		");",
	NULL, NULL, NULL);

	rc = sqlite3_exec(_db,
		"create table if not exists cap_status_count ("
		"	buddy varchar(60) not null,"
		"	account varchar(60) not null,"
		"	protocol varchar(60) not null,"
		"	status varchar(60) not null,"
		"	success_count int not null,"
		"	failed_count int not null,"
		"	primary key (buddy, account, protocol, status)"
		");",
	NULL, NULL, NULL);

	rc = sqlite3_exec(_db,
		"create table if not exists cap_my_usage ("
		"	account varchar(60) not null,"
		"	protocol varchar(60) not null,"
		"	online tinyint not null,"
		"	event_time datetime not null,"
		"	primary key(account, protocol, online, event_time)"
		");",
	NULL, NULL, NULL);
}

static gboolean create_database_connection() {
	gchar *path;
	int rc;

	if(_db)
		return TRUE;

	/* build the path */
	path = g_build_filename(purple_user_dir(), "cap.db", (gchar *)NULL);

	/* make database connection here */
	rc = sqlite3_open(path, &_db);
	g_free(path);
	if(rc != SQLITE_OK)
		return FALSE;

	/* Add tables here */
	create_tables();
	purple_debug_info("cap", "Database connection successfully made.\n");
	return TRUE;
}
static void destroy_database_connection() {
	if(_db)
		sqlite3_close(_db);

	_db = NULL;
}

static guint word_count(const gchar *string) {
	/*TODO: doesn't really work, should use regex instead (#include <regex.h>)*/
	gchar **result = g_strsplit_set(string, " ", -1);
	guint count = g_strv_length(result);

	g_strfreev(result);

	return count;
}

static void insert_status_change(CapStatistics *statistics) {
	insert_status_change_from_purple_status(statistics, get_status_for(statistics->buddy));
}

static void insert_status_change_from_purple_status(CapStatistics *statistics, PurpleStatus *status) {
	char *sql;
	int rc;
	const gchar *status_id;
	const gchar *buddy_name;
	const gchar *protocol_id;
	const gchar *account_id;

	/* It would seem that some protocols receive periodic updates of the buddies status.
	 * Check to make sure the last status is not the same as current status to prevent
	 * to many duplicated useless database entries. */
	if(strcmp(statistics->last_status_id, purple_status_get_id(status)) == 0)
		return;

	status_id = purple_status_get_id(status);
	buddy_name = statistics->buddy->name;
	protocol_id = purple_account_get_protocol_id(statistics->buddy->account);
	account_id = purple_account_get_username(statistics->buddy->account);

	statistics->last_status_id = purple_status_get_id(status);

	purple_debug_info("cap", "Executing: insert into cap_status (buddy, account, protocol, status, event_time) values(%s, %s, %s, %s, now());\n", buddy_name, account_id, protocol_id, status_id);

	sql = sqlite3_mprintf("insert into cap_status values (%Q, %Q, %Q, %Q, now());", buddy_name, account_id, protocol_id, status_id);
	rc = sqlite3_exec(_db, sql, NULL, NULL, NULL);
	sqlite3_free(sql);
}

static void insert_word_count(const char *sender, const char *receiver, guint count) {
	/* TODO! */
	/* dbi_result result; */
	/* result = dbi_conn_queryf(_conn, "insert into cap_message values(\'%s\', \'%s\', %d, now());", sender, receiver, count); */
}

/* Purple plugin specific code */

static gboolean plugin_load(PurplePlugin *plugin) {
	_plugin_pointer = plugin;
	_signals_connected = FALSE;

	/* buddy_stats is a hashtable where strings are keys
	 * and the keys are a buddies account id (PurpleBuddy.name).
	 * keys/values are automatically deleted */
	_buddy_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_stats);

	/* ? - Can't remember at the moment
	 */
	_my_offline_times = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if(create_database_connection()) {
		add_plugin_functionality(plugin);
	}
	return TRUE;
}

static void add_plugin_functionality(PurplePlugin *plugin) {
	if(_signals_connected)
		return;

	purple_debug_info("cap", "Adding plugin functionality.\n");

	/* Connect all the signals */
	purple_signal_connect(purple_conversations_get_handle(), "sent-im-msg", plugin,
			PURPLE_CALLBACK(sent_im_msg), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", plugin,
			PURPLE_CALLBACK(received_im_msg), NULL);

	purple_signal_connect(purple_blist_get_handle(), "buddy-status-changed", plugin,
			PURPLE_CALLBACK(buddy_status_changed), NULL);

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on", plugin,
			PURPLE_CALLBACK(buddy_signed_on), NULL);

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off", plugin,
			PURPLE_CALLBACK(buddy_signed_off), NULL);

	purple_signal_connect(pidgin_blist_get_handle(), "drawing-tooltip", plugin,
			PURPLE_CALLBACK(drawing_tooltip), NULL);

	purple_signal_connect(purple_connections_get_handle(), "signed-on", plugin,
			PURPLE_CALLBACK(signed_on), NULL);

	purple_signal_connect(purple_connections_get_handle(), "signed-off", plugin,
			PURPLE_CALLBACK(signed_off), NULL);

	_signals_connected = TRUE;
}

static void cancel_conversation_timeouts(gpointer key, gpointer value, gpointer user_data) {
	CapStatistics *stats = value;
	if(stats->timeout_source_id != 0) {
		purple_timeout_remove(stats->timeout_source_id);
		stats->timeout_source_id = 0;
	}
}

static void remove_plugin_functionality(PurplePlugin *plugin) {
	if(!_signals_connected)
		return;

	purple_debug_info("cap", "Removing plugin functionality.\n");

	/* If there are any timeouts waiting to be processed then cancel them */
	g_hash_table_foreach(_buddy_stats, cancel_conversation_timeouts, NULL);

	/* Connect all the signals */
	purple_signal_disconnect(purple_conversations_get_handle(), "sent-im-msg", plugin,
			PURPLE_CALLBACK(sent_im_msg));

	purple_signal_disconnect(purple_conversations_get_handle(), "received-im-msg", plugin,
			PURPLE_CALLBACK(received_im_msg));

	purple_signal_disconnect(purple_blist_get_handle(), "buddy-status-changed", plugin,
			PURPLE_CALLBACK(buddy_status_changed));

	purple_signal_disconnect(purple_blist_get_handle(), "buddy-signed-on", plugin,
			PURPLE_CALLBACK(buddy_signed_on));

	purple_signal_disconnect(purple_blist_get_handle(), "buddy-signed-off", plugin,
			PURPLE_CALLBACK(buddy_signed_off));

	purple_signal_disconnect(pidgin_blist_get_handle(), "drawing-tooltip", plugin,
			PURPLE_CALLBACK(drawing_tooltip));

	purple_signal_disconnect(purple_connections_get_handle(), "signed-on", plugin,
			PURPLE_CALLBACK(signed_on));

	purple_signal_disconnect(purple_connections_get_handle(), "signed-off", plugin,
			PURPLE_CALLBACK(signed_off));

	_signals_connected = FALSE;
}

static void write_stats_on_unload(gpointer key, gpointer value, gpointer user_data) {
	CapStatistics *stats = value;
	if(stats->last_message != -1 && stats->buddy != NULL) {
		insert_cap_failure(stats);
	}
}

static gboolean plugin_unload(PurplePlugin *plugin) {
	purple_debug_info("cap", "CAP plugin unloading\n");

	/* clean up memory allocations */
	if(_buddy_stats) {
		g_hash_table_foreach(_buddy_stats, write_stats_on_unload, NULL);
		g_hash_table_destroy(_buddy_stats);
	}

	 /* close database connection */
	 destroy_database_connection();

	return TRUE;
}

static CapPrefsUI * create_cap_prefs_ui() {
	CapPrefsUI *ui = g_malloc(sizeof(CapPrefsUI));

	ui->ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ui->ret), 10);
	ui->cap_vbox = pidgin_make_frame(ui->ret, _("Statistics Configuration"));

	/* msg_difference spinner */
	ui->msg_difference_label = gtk_label_new(_("Maximum response timeout:"));
	gtk_misc_set_alignment(GTK_MISC(ui->msg_difference_label), 0, 0.5);
	ui->msg_difference_input = gtk_spin_button_new_with_range(1, 1440, 1);
	ui->msg_difference_minutes_label = gtk_label_new(_("minutes"));
	gtk_misc_set_alignment(GTK_MISC(ui->msg_difference_minutes_label), 0, 0.5);

	/* last_seen spinner */
	ui->last_seen_label = gtk_label_new(_("Maximum last-seen difference:"));
	gtk_misc_set_alignment(GTK_MISC(ui->last_seen_label), 0, 0.5);
	ui->last_seen_input = gtk_spin_button_new_with_range(1, 1440, 1);
	ui->last_seen_minutes_label = gtk_label_new(_("minutes"));
	gtk_misc_set_alignment(GTK_MISC(ui->last_seen_minutes_label), 0, 0.5);

	/* threshold spinner */
	ui->threshold_label = gtk_label_new(_("Threshold:"));
	gtk_misc_set_alignment(GTK_MISC(ui->threshold_label), 0, 0.5);
	ui->threshold_input = gtk_spin_button_new_with_range(1, 1440, 1);
	ui->threshold_minutes_label = gtk_label_new(_("minutes"));
	gtk_misc_set_alignment(GTK_MISC(ui->threshold_minutes_label), 0, 0.5);

	/* Layout threshold/last-seen/response-timeout input items */
	ui->table_layout = gtk_table_new(3, 3, FALSE);
	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->threshold_label, 0, 1, 0, 1,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->threshold_input, 1, 2, 0, 1,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->threshold_minutes_label, 2, 3, 0, 1,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->msg_difference_label, 0, 1, 1, 2,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->msg_difference_input, 1, 2, 1, 2,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->msg_difference_minutes_label, 2, 3, 1, 2,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->last_seen_label, 0, 1, 2,3,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->last_seen_input, 1, 2, 2, 3,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(ui->table_layout), ui->last_seen_minutes_label, 2, 3, 2, 3,
		(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		(GtkAttachOptions) (0), 0, 0);


	/* Config window - lay it out */
	gtk_box_pack_start(GTK_BOX(ui->cap_vbox), ui->table_layout, FALSE, FALSE, 0);

	/* Set the input areas to contain the configuration values from
	 * purple prefs.
	 */
	if(purple_prefs_exists("/plugins/gtk/cap/max_msg_difference")) {
		int max_msg_diff = purple_prefs_get_int("/plugins/gtk/cap/max_msg_difference");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->msg_difference_input),  max_msg_diff);
	}
	if(purple_prefs_exists("/plugins/gtk/cap/max_seen_difference")) {
		int max_seen_diff = purple_prefs_get_int("/plugins/gtk/cap/max_seen_difference");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->last_seen_input), max_seen_diff);
	}
	if(purple_prefs_exists("/plugins/gtk/cap/threshold")) {
		int threshold = purple_prefs_get_int("/plugins/gtk/cap/threshold");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->threshold_input), threshold);
	}

	/* Add the signals */
	g_signal_connect(G_OBJECT(ui->ret), "destroy",
		G_CALLBACK(cap_prefs_ui_destroy_cb), ui);

	g_signal_connect(G_OBJECT(ui->msg_difference_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/max_msg_difference");

	g_signal_connect(G_OBJECT(ui->last_seen_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/max_seen_difference");

	g_signal_connect(G_OBJECT(ui->threshold_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/threshold");

	return ui;
}

static void cap_prefs_ui_destroy_cb(GtkObject *object, gpointer user_data) {
	CapPrefsUI *ui = user_data;
	if(_db) {
		add_plugin_functionality(_plugin_pointer);
	}
	g_free(ui);
}

static void numeric_spinner_prefs_cb(GtkSpinButton *spinbutton, gpointer user_data) {
	purple_prefs_set_int(user_data, gtk_spin_button_get_value_as_int(spinbutton));
}

static PidginPluginUiInfo ui_info = {
	get_config_frame,
	0 /* page_num (reserved) */,
	NULL,NULL,NULL,NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,							/**< type		*/
	PIDGIN_PLUGIN_TYPE,							/**< ui_requirement */
	0,												/**< flags		*/
	NULL,											/**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,							/**< priority		*/
	CAP_PLUGIN_ID,									/**< id			*/
	N_("Contact Availability Prediction"),				/**< name		*/
	DISPLAY_VERSION,									/**< version		*/
	N_("Contact Availability Prediction plugin."),	/**  summary		*/
	N_("Displays statistical information about your buddies' availability"),
	/**  description	*/
	"Geoffrey Foster <geoffrey.foster@gmail.com>",	/**< author		*/
	PURPLE_WEBSITE,									/**< homepage		*/
	plugin_load,									/**< load		*/
	plugin_unload,									/**< unload		*/
	NULL,											/**< destroy		*/
	&ui_info,										/**< ui_info		*/
	NULL,											/**< extra_info	 */
	NULL,											/**< prefs_info		*/
	NULL,
	NULL,NULL,NULL,NULL
};

static GtkWidget * get_config_frame(PurplePlugin *plugin) {
	CapPrefsUI *ui = create_cap_prefs_ui();

	/*
	 * Prevent database stuff from occuring since we are editing values
	 */
	remove_plugin_functionality(_plugin_pointer);

	return ui->ret;
}

static void init_plugin(PurplePlugin *plugin) {
	purple_prefs_add_none("/plugins/gtk/cap");
	purple_prefs_add_int("/plugins/gtk/cap/max_seen_difference", 1);
	purple_prefs_add_int("/plugins/gtk/cap/max_msg_difference", 10);
	purple_prefs_add_int("/plugins/gtk/cap/threshold", 5);
}

PURPLE_INIT_PLUGIN(cap, init_plugin, info);
