/*
 * Contact Availability Prediction plugin for Gaim
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "cap.h"

static char * quote_string(const char *str) {
	gchar *quoted_str = NULL;
	quoted_str = g_strdup(str);
	dbi_driver_quote_string(_driver, &quoted_str);
	return quoted_str;
}

static void generate_minute_stats(CapStatistics *statistics) {
	gchar *buddy_name = quote_string(statistics->buddy->name);
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(statistics->buddy->account));
	gchar *account_id = quote_string(gaim_account_get_username(statistics->buddy->account));
	dbi_result result = dbi_conn_queryf(_conn, "select minute_val, success_count, failed_count from cap_msg_count where buddy=%s account=%s and protocol=%s;", buddy_name, account_id, protocol_id);

	while(dbi_result_next_row(result)) {
		int minute_val = dbi_result_get_int(result, "minute_val");
		int success_count = dbi_result_get_int(result, "success_count");
		int failed_count = dbi_result_get_int(result, "failed_count");
		double ratio = ((double)success_count/(double)(success_count + failed_count));
		statistics->minute_stats[minute_val] = ratio;
	}
	dbi_result_free(result);
}

static void generate_prediction(CapStatistics *statistics) {
	if(statistics->buddy) {
		if(statistics->prediction == NULL)
			statistics->prediction = g_malloc(sizeof(CapPrediction));
		statistics->prediction->probability = generate_prediction_for(statistics->buddy);
		statistics->prediction->generated_at = time(NULL);
	}
}

static double generate_prediction_for(GaimBuddy *buddy) {
	double prediction = 1.0f;
	gboolean generated = FALSE;
	gchar *buddy_name = quote_string(buddy->name);
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(buddy->account));
	gchar *account_id = quote_string(gaim_account_get_username(buddy->account));
	gchar *status_id = quote_string(gaim_status_get_id(get_status_for(buddy)));
	time_t t = time(NULL);
	struct tm *current_time = localtime(&t);
	int current_minute = current_time->tm_min + current_time->tm_hour * 60;
	int threshold = gaim_prefs_get_int("/plugins/gtk/cap/threshold");
	int min_minute = (current_minute - threshold) % 1440;
	int max_minute = (current_minute + threshold) % 1440;
	dbi_result result;

	result = dbi_conn_queryf(_conn, "select success_count, failed_count from cap_msg_count where buddy=%s and account=%s and protocol=%s and minute_val>=%d and minute_val<=%d;", buddy_name, account_id, protocol_id, min_minute, max_minute);
	if(result) {
		int successes = 0;
		int failures = 0;
		while(dbi_result_next_row(result)) {
			if(!dbi_result_field_is_null(result, "success_count"))
				successes += dbi_result_get_int(result, "success_count");

			if(!dbi_result_field_is_null(result, "failed_count"))
				failures += dbi_result_get_int(result, "failed_count");
		}
		gaim_debug_info("cap", "Successes = %d; Failures = %d\n", successes, failures);
		if(failures + successes > 0) {
			prediction *= ((double)successes/((double)(successes + failures)));
			generated = TRUE;
		}
		gaim_debug_info("cap", "After message value prediction is %0.4f.\n", prediction);
		dbi_result_free(result);
	}


/*
 * Note to self: apparently when using a function like sum results in not being able to get
 * the values for fields using libdbi...why? the way I'm doing it above sucks, find a fix.
	result = dbi_conn_queryf(_conn, "select sum(success_count) as successes, sum(failed_count) as failures from cap_msg_count where buddy=%s and account=%s and protocol=%s and minute_val>=%d and minute_val<=%d;", buddy_name, account_id, protocol_id, min_minute, max_minute);
	gaim_debug_info("cap", "select sum(success_count) as successes, sum(failed_count) as failures from cap_msg_count where buddy=%s and account=%s and protocol=%s and minute_val>=%d and minute_val<=%d;\n", buddy_name, account_id, protocol_id, min_minute, max_minute);

	if(result) {
		int failures;
		int successes;
		int rc = dbi_result_get_numrows(result);
		dbi_result_next_row(result);
		gaim_debug_info("cap", "Result code: %d\n", rc);
		failures = dbi_result_get_int_idx(result, 1);//"failures");
		successes = dbi_result_get_int_idx(result, 2); //"successes");
		gaim_debug_info("cap", "Successes = %d; Failures = %d\n", successes, failures);
		dbi_result_get_fields(result, "successes.%i failures.%i", &successes, &failures);
		gaim_debug_info("cap", "Successes = %d; Failures = %d\n", successes, failures);
		if(failures + successes > 0.0)
			prediction *= ((double)successes/((double)(successes + failures)));
		gaim_debug_info("cap", "After message value prediction is %0.4f.\n", prediction);
		dbi_result_free(result);
	}
*/	
	
	result = dbi_conn_queryf(_conn, "select success_count, failed_count from cap_status_count where buddy=%s and account=%s and protocol=%s and status=%s;", buddy_name, account_id, protocol_id, status_id);
	if(result) {
		int successes = 0;
		int failures = 0;
		
		dbi_result_next_row(result);

		if(!dbi_result_field_is_null(result, "success_count"))
			successes = dbi_result_get_int(result, "success_count");

		if(!dbi_result_field_is_null(result, "failed_count"))
			failures = dbi_result_get_int(result, "failed_count");

		gaim_debug_info("cap", "Successes = %d; Failures = %d\n", successes, failures);
		if(successes + failures > 0) {
			prediction *= ((double)successes/(double)(successes + failures));
			generated = TRUE;
		}
		gaim_debug_info("cap", "After status value prediction is %0.4f.\n", prediction);
		dbi_result_free(result);
	}

	free(buddy_name);
	free(account_id);
	free(protocol_id);
	free(status_id);

	if(strcmp(gaim_status_get_id(get_status_for(buddy)), "offline") == 0) {
		//This is kind of stupid, change it.
		gaim_debug_info("cap", "Buddy is offline.\n");
		if(prediction == 1.0f)
			prediction = 0.0f;
	}

	if(generated)
		return prediction;
	else
		return -1;
}

static CapStatistics * get_stats_for(GaimBuddy *buddy) {
	gchar *buddy_name;
	gchar *q_buddy_name = quote_string(buddy->name);
	CapStatistics *stats;
	buddy_name = g_strdup(buddy->name);
	stats = g_hash_table_lookup(_buddy_stats, buddy_name);
	if(!stats) {
		dbi_result result;
		gaim_debug_info("cap", "Creating stats for %s\n", buddy->name);
		stats = g_malloc(sizeof(CapStatistics));
		stats->last_message = -1;
		stats->last_message_status_id = NULL;
		stats->last_status_id = NULL;
		stats->prediction = NULL;
		g_hash_table_insert(_buddy_stats, buddy_name, stats);
		stats->buddy = buddy;
		/* Setup the last seen online time from database or -1 if no time available. */
		result = dbi_conn_queryf(_conn,
			//"select max(event_time) as last_event_time from cap_status where buddy=\'%s\' and status!=\'offline\';",
			"select event_time from cap_status where buddy=\'%s\' and status!=\'offline\' order by event_time desc;",
			q_buddy_name);
		if(result && dbi_result_get_numrows(result) > 0) {
			dbi_result_next_row(result);
			stats->last_seen = dbi_result_get_datetime(result, "event_time");
		} else {
			stats->last_seen = -1;
		}
		dbi_result_free(result);
		/* Setup the last messaged time to be a 'useable' value for comparisons. */
		//result = dbi_conn_queryf(conn, "", );
		/* Setup the last status id to nothing */
		// --> Better approach would be to get the last status available in db and use it.
		result = dbi_conn_queryf(_conn,
			"select status, max(event_time) from cap_status where buddy=\'%s\' group by status;",
			buddy->name);
		if(result && dbi_result_get_numrows(result) > 0) {
			dbi_result_next_row(result);
			stats->last_status_id = dbi_result_get_string_copy(result, "status");
		} else {
			stats->last_status_id = "";
		}
		dbi_result_free(result);
		//TODO: populate stats from database
	} else {
		g_free(buddy_name);
	}
	free(q_buddy_name);
	generate_prediction(stats);
	return stats;
}

static void destroy_stats(gpointer data) {
	CapStatistics *stats = data;
	g_free(stats->prediction);
	//g_free(stats->hourly_usage);
	//g_free(stats->daily_usage);
	g_free(stats);
}

static gboolean remove_stats_for(GaimBuddy *buddy) {
	gboolean success = TRUE;
	//GString *buddy_name = g_string_new(buddy->name);
	gchar *buddy_name = g_strdup(buddy->name);
	success = g_hash_table_remove(_buddy_stats, buddy_name);
	g_free(buddy_name);
	return success;
}

static dbi_result insert_cap_msg_count_success(const char *buddy_name, const char *account, const char *protocol, int minute) {
	gaim_debug_info("cap", "Insert cap_msg_count success: %s %s %s %d\n", buddy_name, account, protocol, minute);
	return dbi_conn_queryf(_conn, "insert into cap_msg_count (buddy, account, protocol, minute_val, success_count, failed_count) values (%s, %s, %s, %d, %d, %d) on duplicate key update success_count=success_count+1;", buddy_name, account, protocol, minute, 1, 0);
}

static dbi_result insert_cap_status_count_success(const char *buddy_name, const char *account, const char *protocol, const char *status_id) {
	gaim_debug_info("cap", "Insert cap_status_count success: %s %s %s %s\n", buddy_name, account, protocol, status_id);
	return dbi_conn_queryf(_conn, "insert into cap_status_count (buddy, account, protocol, status, success_count, failed_count) values(%s, %s, %s, %s, %d, %d) on duplicate key update success_count=success_count+1;", buddy_name, account, protocol, status_id, 1, 0);
}

static dbi_result insert_cap_msg_count_failed(const char *buddy_name, const char *account, const char *protocol, int minute) {
	gaim_debug_info("cap", "Insert cap_msg_count failed: %s %s %s %d\n", buddy_name, account, protocol, minute);
	return dbi_conn_queryf(_conn, "insert into cap_msg_count (buddy, account, protocol, minute_val, success_count, failed_count) values (%s, %s, %s, %d, %d, %d) on duplicate key update failed_count=failed_count+1;", buddy_name, account, protocol, minute, 0, 1);
}

static dbi_result insert_cap_status_count_failed(const char *buddy_name, const char *account, const char *protocol, const char *status_id) {
	gaim_debug_info("cap", "Insert cap_status_count failed: %s %s %s %s\n", buddy_name, account, protocol, status_id);
	return dbi_conn_queryf(_conn, "insert into cap_status_count (buddy, account, protocol, status, success_count, failed_count) values(%s, %s, %s, %s, %d, %d) on duplicate key update failed_count=failed_count+1;", buddy_name, account, protocol, status_id, 0, 1);
}

static void insert_cap_success(CapStatistics *stats) {
	dbi_result result;
	gchar *buddy_name = quote_string(stats->buddy->name);
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(stats->buddy->account));
	gchar *account_id = quote_string(gaim_account_get_username(stats->buddy->account));
	gchar *status_id = (stats->last_message_status_id) ?
		quote_string(stats->last_message_status_id) :
		quote_string(gaim_status_get_id(get_status_for(stats->buddy)));
	struct tm *current_time;
	int minute;
	
	if(stats->last_message == -1) {
		time_t now = time(NULL);
		current_time = localtime(&now);
	} else {
		current_time = localtime(&stats->last_message);
	}
	minute = current_time->tm_min + current_time->tm_hour * 60;

	result = insert_cap_msg_count_success(buddy_name, account_id, protocol_id, minute);
	if(result)
		dbi_result_free(result);
	
	result = insert_cap_status_count_success(buddy_name, account_id, protocol_id, status_id);
	if(result)
		dbi_result_free(result);

	stats->last_message = -1;
	stats->last_message_status_id = NULL;

	free(status_id);
	free(protocol_id);
	free(account_id);
	free(buddy_name);
}

static void insert_cap_failure(CapStatistics *stats) {
	dbi_result result;
	gchar *buddy_name = quote_string(stats->buddy->name);
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(stats->buddy->account));
	gchar *account_id = quote_string(gaim_account_get_username(stats->buddy->account));
	gchar *status_id = (stats->last_message_status_id) ?
		quote_string(stats->last_message_status_id) :
		quote_string(gaim_status_get_id(get_status_for(stats->buddy)));
	struct tm *current_time = localtime(&stats->last_message);
	int minute = current_time->tm_min + current_time->tm_hour * 60;

	result = insert_cap_msg_count_failed(buddy_name, account_id, protocol_id, minute);
	if(result)
		dbi_result_free(result);
	
	result = insert_cap_status_count_failed(buddy_name, account_id, protocol_id, status_id);
	if(result)
		dbi_result_free(result);
	
	stats->last_message = -1;
	stats->last_message_status_id = NULL;

	free(status_id);
	free(protocol_id);
	free(account_id);
	free(buddy_name);
}

static gboolean max_message_difference_cb(gpointer data) {
	CapStatistics *stats = data;
	gaim_debug_info("cap", "Max Message Difference timeout occured\n");
	insert_cap_failure(stats);
	stats->timeout_source_id = 0;
	return FALSE;
}

/* Gaim Signal Handlers */

//sent-im-msg
static void sent_im_msg(GaimAccount *account, const char *receiver, const char *message) {
	GaimBuddy *buddy = gaim_find_buddy(account, receiver);
	guint interval = gaim_prefs_get_int("/plugins/gtk/cap/max_msg_difference") * 1000 * 60;
	guint words = word_count(message);
	CapStatistics *stats = get_stats_for(buddy);

	insert_word_count(gaim_account_get_username(account), receiver, words);
	stats->last_message = time(NULL);
	stats->last_message_status_id = gaim_status_get_id(get_status_for(buddy));
	if(stats->timeout_source_id != 0)
		g_source_remove(stats->timeout_source_id);

	stats->timeout_source_id = g_timeout_add(interval, max_message_difference_cb, stats);
}

//received-im-msg
static void received_im_msg(GaimAccount *account, char *sender, char *message,
		GaimConversation *conv, GaimMessageFlags flags) {
	GaimBuddy *buddy = gaim_find_buddy(account, sender);
	guint words = word_count(message);
	CapStatistics *stats = get_stats_for(buddy);

	//insert_word_count(sender, buddy_name, words);
	
	//If we are waiting for a response from a prior message
	// then cancel the timeout callback.
	if(stats->timeout_source_id != 0) {
		gaim_debug_info("cap", "Cancelling timeout callback\n");
		g_source_remove(stats->timeout_source_id);
		stats->timeout_source_id = 0;
	}

	insert_cap_success(stats);

	stats->last_message = -1; //Reset the last_message value
	stats->last_message_status_id = NULL; //Reset the last status id value
}

//buddy-status-changed
static void buddy_status_changed(GaimBuddy *buddy, GaimStatus *old_status, GaimStatus *status) {
	CapStatistics *stats = get_stats_for(buddy);
	insert_status_change_from_gaim_status(stats, status);
}

//buddy-signed-on
static void buddy_signed_on(GaimBuddy *buddy) {
	CapStatistics *stats = get_stats_for(buddy);
	
	/* If the statistic object existed but doesn't have a buddy pointer associated
	 * with it then reassociate one with it. The pointer being null is a result
	 * of a buddy with existing stats signing off and Gaim sticking around.
	 */
	if(!stats->buddy) {
		stats->buddy = buddy;
	}

	insert_status_change(stats);
}

//buddy-signed-off
static void buddy_signed_off(GaimBuddy *buddy) {
	CapStatistics *stats = get_stats_for(buddy);

	/* We don't necessarily want to delete a buddies generated statistics every time they go offline.
	 * Instead we just set the buddy pointer to null so that when they come back online we can look
	 * them up again and continue using their statistics.
	 */
	insert_status_change(stats);
	//stats->buddy = NULL;
	stats->last_seen = time(NULL);
}

static void buddy_idle(GaimBuddy *buddy, gboolean old_idle, gboolean idle) {
}

static void blist_node_extended_menu(GaimBlistNode *node, GList **menu) {
	GaimBuddy *buddy;
	GaimMenuAction *menu_action;
	gaim_debug_info("cap", "got extended blist menu\n");
	gaim_debug_info("cap", "is buddy: %d\n", GAIM_BLIST_NODE_IS_BUDDY(node));
	gaim_debug_info("cap", "is contact: %d\n", GAIM_BLIST_NODE_IS_CONTACT(node));
	gaim_debug_info("cap", "is group: %d\n", GAIM_BLIST_NODE_IS_GROUP(node));
	/* Probably only concerned with buddy/contact types. Contacts = meta-buddies (grouped msn/jabber/etc.) */
	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));
	buddy = (GaimBuddy *)node;
	menu_action = gaim_menu_action_new(_("Display Statistics"),
			GAIM_CALLBACK(display_statistics_action_cb), NULL, NULL);
	*menu = g_list_append(*menu, menu_action);
}

//drawing-tooltip
static void drawing_tooltip(GaimBlistNode *node, GString *text, gboolean full) {
	if(node->type == GAIM_BLIST_BUDDY_NODE) {
		GaimBuddy *buddy = (GaimBuddy *)node;
		CapStatistics *stats = get_stats_for(buddy);
		// get the probability that this buddy will respond and add to the tooltip
		if(stats->prediction->probability >= 0.0) {
			g_string_append_printf(text, "\n<b>%s</b> %0.4f", _("Response Probability:"), stats->prediction->probability);
		} else {
			g_string_append_printf(text, "\n<b>%s</b> ???", _("Response Probability:"));
		}
	}
}

//signed-on
static void signed_on(GaimConnection *gc) {
	GaimAccount *account = gaim_connection_get_account(gc);
	const char *my_gaim_name = gaim_account_get_username(account);
	gchar *my_name = g_strdup(my_gaim_name);
	time_t *last_offline = g_hash_table_lookup(_my_offline_times, my_name);

	gchar *account_id = quote_string(gaim_account_get_username(account));
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(account));
	dbi_result result;

	result = dbi_conn_queryf(_conn, "insert into cap_my_usage values(%s, %s, %d, now());", account_id, protocol_id, 1);
	if(!result)
		gaim_debug_error("cap", "Could not insert sign on into cap_my_usage\n");
	else
		dbi_result_free(result);

	if(last_offline) {
		if(difftime(*last_offline, time(NULL)) > gaim_prefs_get_int("/plugins/gtk/cap/max_seen_difference") * 60) {
			//reset all of the last_message times to -1
			g_hash_table_foreach(_my_offline_times, reset_all_last_message_times, NULL);
		}
		g_hash_table_remove(_my_offline_times, my_name);
	}
	g_free(my_name);
}

//signed-off
static void signed_off(GaimConnection *gc) {
	/* Here we record the time you (the user) sign off of an account.
	 * The account username is the key in the hashtable and the sign off time_t
	 * (equal to the sign off time) is the value. */
	GaimAccount *account = gaim_connection_get_account(gc);
	const char *my_gaim_name = gaim_account_get_username(account);
	gchar *my_name = g_strdup(my_gaim_name);
	time_t *offline_time = g_malloc(sizeof(time_t));
	gchar *account_id = quote_string(gaim_account_get_username(account));
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(account));
	dbi_result result;

	result = dbi_conn_queryf(_conn, "insert into cap_my_usage values(%s, %s, %d, now());", account_id, protocol_id, 0);
	if(!result)
		gaim_debug_error("cap", "Could not insert sign off into cap_my_usage\n");
	else
		dbi_result_free(result);

	time(offline_time);
	g_hash_table_insert(_my_offline_times, my_name, offline_time);
}

static const gchar * get_error_msg() {
	if(error_msg)
		return error_msg->str;
	else
		return NULL;
}

static void set_error_msg(const gchar *msg) {
	if(!error_msg)
		error_msg = g_string_new(msg);
	else
		g_string_assign(error_msg, msg);
}

static void append_error_msg(const gchar *msg) {
	if(!error_msg)
		set_error_msg(msg);
	else
		g_string_append(error_msg, msg);
}

static void reset_all_last_message_times(gpointer key, gpointer value, gpointer user_data) {
	CapStatistics *stats = value;
	stats->last_message = -1;
}

static GaimStatus * get_status_for(GaimBuddy *buddy) {
	GaimPresence *presence = gaim_buddy_get_presence(buddy);
	GaimStatus *status = gaim_presence_get_active_status(presence);
	return status;
}

static void create_tables() {
}

static gboolean create_database_connection() {
	int rc;
	int driver_type;
	//make database connection here
	/*
	 * conn = dbi_conn_new("mysql");
	 * dbi_conn_set_option(conn, "host", "localhost");
	 * dbi_conn_set_option(conn, "username", "root");
	 * dbi_conn_set_option(conn, "password", "b1qywm96");
	 * dbi_conn_set_option(conn, "dbname", "mysql");
	 * dbi_conn_set_option(conn, "encoding", "auto");
	 */
	_conn = dbi_conn_new(gaim_prefs_get_string("/plugins/gtk/cap/db_driver"));
	_driver = dbi_conn_get_driver(_conn);
	gaim_debug_info("cap", "Using driver: %s\n", gaim_prefs_get_string("/plugins/gtk/cap/db_driver"));
	if(strcmp(gaim_prefs_get_string("/plugins/gtk/cap/db_driver"), "mysql") == 0) {
		driver_type = MYSQL;
		dbi_conn_set_option(_conn, "host", gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_host"));
		dbi_conn_set_option(_conn, "username", gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_user"));
		dbi_conn_set_option(_conn, "password", gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_password"));
		dbi_conn_set_option(_conn, "dbname", gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_name"));
		dbi_conn_set_option(_conn, "encoding", "auto");
		dbi_conn_set_option_numeric(_conn, "port", gaim_prefs_get_int("/plugins/gtk/cap/mysql/db_port"));
	}
	if(dbi_conn_connect(_conn) < 0) {
		const char *err_msg = "";
		//rc = dbi_conn_error(_conn, &err_msg);
		rc = dbi_conn_error(_conn, NULL);
		gaim_debug_error("cap", "CAP could not create database connection. %d\n", rc);
		//set_error_msg(_("Could not create database connection. Reason: "));
		//append_error_msg(err_msg);
		return FALSE;
	} else {
		//Add tables here
		create_tables();
	}
	gaim_debug_info("cap", "Database connection successfully made.\n");
	return TRUE;
}

static guint word_count(const gchar *string) {
	//TODO: doesn't really work, should use regex instead (#include <regex.h>)
	gchar **result = g_strsplit_set(string, " ", -1);
	guint count = g_strv_length(result);

	g_strfreev(result);

	return count;
}

/* If the difference in time between the present time and the time that
 * you last sent a message to a buddy is less than some value then return
 * true, otherwise return false.
 * The difference can either be + or - the max_difference.
 * max_difference is in seconds
 */
static gboolean last_message_time_in_range(CapStatistics *statistics, gdouble max_difference) {
	time_t now = time(NULL);
	gdouble difference = 0.0;
	//If there is no last_message time then it is considered in range
	if(statistics->last_message == -1) {
		return TRUE;
	}
	//Compute the difference between now and the last_message
	difference = difftime(statistics->last_message, now);
	//Absolute value
	difference = (difference < 0.0) ? -difference : difference;
	//If the difference is less than the maximum then we are good and its in range, otherwise not
	if(difference <= max_difference)
		return TRUE;
	else
		return FALSE;
}

static gboolean last_seen_time_in_range(CapStatistics *statistics, gdouble max_difference) {
	time_t now = time(NULL);
	gdouble difference = 0.0;
	if(statistics->last_seen == -1)
		return FALSE;
	difference = difftime(statistics->last_seen, now);
	difference = (difference < 0.0) ? -difference : difference;
	if(difference < max_difference)
		return TRUE;
	else
		return FALSE;
}

static void insert_status_change(CapStatistics *statistics) {
	insert_status_change_from_gaim_status(statistics, get_status_for(statistics->buddy));
}

static void insert_status_change_from_gaim_status(CapStatistics *statistics, GaimStatus *status) {
	dbi_result result;
	gchar *status_id = quote_string(gaim_status_get_id(status));
	gchar *buddy_name = quote_string(statistics->buddy->name);
	gchar *protocol_id = quote_string(gaim_account_get_protocol_id(statistics->buddy->account));
	gchar *account_id = quote_string(gaim_account_get_username(statistics->buddy->account));

	statistics->last_status_id = gaim_status_get_id(status);
	gaim_debug_info("cap", "Executing: insert into cap_status (buddy, account, protocol, status, event_time) values(%s, %s, %s, %s, now());\n", buddy_name, account_id, protocol_id, status_id);

	result = dbi_conn_queryf(_conn, "insert into cap_status (buddy, account, protocol, status, event_time) values(%s, %s, %s, %s, now());", buddy_name, account_id, protocol_id, status_id);
	if(result)
		dbi_result_free(result);
	else {
		const char *err = "";
		dbi_conn_error(_conn, &err);
		gaim_debug_error("cap", "Could not insert status change event into database. %s\n", err);
	}

	free(status_id);
	free(buddy_name);
	free(protocol_id);
	free(account_id);
}

static void insert_word_count(const char *sender, const char *receiver, guint count) {
	//TODO!
	//dbi_result result;
	//result = dbi_conn_queryf(_conn, "insert into cap_message values(\'%s\', \'%s\', %d, now());", sender, receiver, count);
}

/* Callbacks */
void display_statistics_action_cb(GaimBlistNode *node, gpointer data) {
	GaimBuddy *buddy;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));
	buddy = (GaimBuddy *)node;
	gaim_debug_info("cap", "Statistics for %s requested.\n", buddy->name);
}

/* Gaim plugin specific code */

static gboolean plugin_load(GaimPlugin *plugin) {
	dbi_driver driver = dbi_driver_list(NULL);
	gboolean drivers_available = FALSE;
	int rc = dbi_initialize(gaim_prefs_get_string("/plugins/gtk/cap/libdbi_drivers"));

	if(rc == -1) {
		gaim_debug_error("cap", "Error initializing dbi.\n");
		set_error_msg(_("Error initializing libdbi."));
		return FALSE;
	}

	_plugin_pointer = plugin;
	_signals_connected = FALSE;

	while(driver != NULL) {
		gaim_debug_info("cap", "Located driver: %s\n", dbi_driver_get_name(driver));
		if(strcmp("mysql", dbi_driver_get_name(driver)) == 0)
			drivers_available = TRUE;
	
		driver = dbi_driver_list(driver);
	}
	if(!drivers_available)
		return FALSE;


	if(gaim_prefs_get_bool("/plugins/gtk/cap/configured")) {
		if(!add_plugin_functionality(plugin)) {	
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean add_plugin_functionality(GaimPlugin *plugin) {
	if(_signals_connected)
		return TRUE;
	
	if(!create_database_connection()) {
		/*
		GtkWidget *dialog = gtk_message_dialog_new("title",
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			get_error_msg());
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		*/
		return FALSE;
	}

	/* buddy_stats is a hashtable where strings are keys
	 * and the keys are a buddies account id (GaimBuddy.name).
	 * keys/values are automatically deleted */
	_buddy_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_stats);

	_my_offline_times = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	/* Connect all the signals */
	gaim_signal_connect(gaim_conversations_get_handle(), "sent-im-msg", plugin,
			GAIM_CALLBACK(sent_im_msg), NULL);

	gaim_signal_connect(gaim_conversations_get_handle(), "received-im-msg", plugin,
			GAIM_CALLBACK(received_im_msg), NULL);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-status-changed", plugin,
			GAIM_CALLBACK(buddy_status_changed), NULL);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on", plugin,
			GAIM_CALLBACK(buddy_signed_on), NULL);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-off", plugin,
			GAIM_CALLBACK(buddy_signed_off), NULL);

	//gaim_signal_connect(gaim_blist_get_handle(), "blist-node-extended-menu", plugin,
	//		GAIM_CALLBACK(blist_node_extended_menu), NULL);

	gaim_signal_connect(gaim_gtk_blist_get_handle(), "drawing-tooltip", plugin,
			GAIM_CALLBACK(drawing_tooltip), NULL);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on", plugin,
			GAIM_CALLBACK(signed_on), NULL);
	
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off", plugin,
			GAIM_CALLBACK(signed_off), NULL);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-idle-changed", plugin,
			GAIM_CALLBACK(buddy_idle), NULL);

	_signals_connected = TRUE;

	return TRUE;
}

static void write_stats_on_unload(gpointer key, gpointer value, gpointer user_data) {
	CapStatistics *stats = value;
	gaim_debug_info("cap", "Unloading, last message time %d\n", stats->last_message);
	if(stats->last_message != -1 && stats->buddy != NULL) {
		insert_cap_failure(stats);
	}
}

static gboolean plugin_unload(GaimPlugin *plugin) {
	gaim_debug_info("cap", "CAP plugin unloading\n");
	//TODO: foreach stats object in hashtable:
	//			if stats->last_message != -1 then update for that time the failed counter...maybe?
	g_hash_table_foreach(_buddy_stats, write_stats_on_unload, NULL);
	//clean up memory allocations
	if(_buddy_stats)
		g_hash_table_destroy(_buddy_stats);
	/*while(g_list_length(plugin_actions) > 0) {
	  GaimPluginAction *action = g_list_first(plugin_actions)->data;
	  gaim_plugin_action_free(action);
	  }
	  _plugin_pointer = NULL;*/
	 
	 //close database connection
	 dbi_conn_close(_conn);
	 dbi_shutdown();

	return TRUE;
}

static GtkWidget * get_config_frame(GaimPlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *vbox;

	GtkWidget *driver_vbox;
	GtkWidget *driver_select_hbox;
	GtkWidget *driver_choice;
	GtkWidget *driver_label;
	GtkWidget *driver_config_hbox;
	GtkWidget *driver_config;

	GtkWidget *threshold_label;
	GtkWidget *threshold_input;
	GtkWidget *threshold_hbox;
	GtkWidget *threshold_minutes_label;

	GtkWidget *msg_difference_label;
	GtkWidget *msg_difference_input;
	GtkWidget *msg_difference_hbox;
	GtkWidget *msg_difference_minutes_label;

	GtkWidget *last_seen_label;
	GtkWidget *last_seen_input;
	GtkWidget *last_seen_hbox;
	GtkWidget *last_seen_minutes_label;

	GtkWidget *dbd_label;
	GtkWidget *dbd_input;
	GtkWidget *dbd_hbox;
	
	ret = gtk_vbox_new(FALSE, 18);
	g_signal_connect(G_OBJECT(ret), "destroy",
		G_CALLBACK(prefs_closed_cb), "/plugins/gtk/cap/configured");
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
	vbox = gaim_gtk_make_frame(ret, _("Contact Availability Prediction Configuration"));

	driver_vbox = gtk_vbox_new(FALSE, 18);
	//Driver selection and configuration
	driver_choice = gtk_combo_box_new_text();
	g_signal_connect(G_OBJECT(driver_choice), "changed",
		G_CALLBACK(combobox_prefs_cb), "/plugins/gtk/cap/db_driver");
	gtk_combo_box_append_text(GTK_COMBO_BOX(driver_choice), _("mysql"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(driver_choice), 0);
	//gtk_combo_box_append_text(GTK_COMBO_BOX(driver_choice), _("pgsql"));
	//gtk_combo_box_append_text(GTK_COMBO_BOX(driver_choice), _("sqlite"));
	driver_label = gtk_label_new(_("Driver:"));
	driver_select_hbox = gtk_hbox_new(FALSE, 18);
	if(gaim_prefs_exists("/plugins/gtk/cap/db_driver")) {
		const char *driver_string = gaim_prefs_get_string("/plugins/gtk/cap/db_driver");
		if(g_ascii_strcasecmp(driver_string, "mysql") == 0) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(driver_choice), 0);
		}
	}
	gtk_box_pack_start(GTK_BOX(driver_select_hbox), driver_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(driver_select_hbox), driver_choice, FALSE, FALSE, 0);

	driver_config = gtk_expander_new_with_mnemonic(_("_Configure"));
	g_signal_connect(driver_config, "notify::expanded",
		G_CALLBACK(driver_config_expanded), gtk_combo_box_get_active_text(GTK_COMBO_BOX(driver_choice)));
	driver_config_hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(driver_config_hbox), driver_config, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(driver_vbox), driver_select_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(driver_vbox), driver_config_hbox, FALSE, FALSE, 0);

	//msg_difference
	msg_difference_label = gtk_label_new(_("Maximum response timeout:"));
	//FIXME: better maximum value
	msg_difference_input = gtk_spin_button_new_with_range(1, 1440, 1);
	msg_difference_hbox = gtk_hbox_new(FALSE, 18);
	msg_difference_minutes_label = gtk_label_new(_("minutes"));
	if(gaim_prefs_exists("/plugins/gtk/cap/max_msg_difference")) {
		int max_msg_diff = gaim_prefs_get_int("/plugins/gtk/cap/max_msg_difference");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(msg_difference_input),  max_msg_diff);
	}
	g_signal_connect(G_OBJECT(msg_difference_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/max_msg_difference");
	gtk_box_pack_start(GTK_BOX(msg_difference_hbox), msg_difference_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(msg_difference_hbox), msg_difference_input, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(msg_difference_hbox), msg_difference_minutes_label, FALSE, FALSE, 0);

	//last_seen
	last_seen_label = gtk_label_new(_("Maximum last-seen difference:"));
	last_seen_input = gtk_spin_button_new_with_range(1, 1440, 1);
	last_seen_hbox = gtk_hbox_new(FALSE, 18);
	last_seen_minutes_label = gtk_label_new(_("minutes"));
	if(gaim_prefs_exists("/plugins/gtk/cap/max_seen_difference")) {
		int max_seen_diff = gaim_prefs_get_int("/plugins/gtk/cap/max_seen_difference");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(last_seen_input), max_seen_diff);
	}
	g_signal_connect(G_OBJECT(last_seen_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/max_seen_difference");
	gtk_box_pack_start(GTK_BOX(last_seen_hbox), last_seen_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(last_seen_hbox), last_seen_input, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(last_seen_hbox), last_seen_minutes_label, FALSE, FALSE, 0);

	//threshold
	threshold_label = gtk_label_new(_("Threshold:"));
	threshold_input = gtk_spin_button_new_with_range(1, 1440, 1);
	threshold_hbox = gtk_hbox_new(FALSE, 18);
	threshold_minutes_label = gtk_label_new(_("minutes"));
	if(gaim_prefs_exists("/plugins/gtk/cap/threshold")) {
		int threshold = gaim_prefs_get_int("/plugins/gtk/cap/threshold");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(threshold_input), threshold);
	}
	g_signal_connect(G_OBJECT(threshold_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/threshold");
	gtk_box_pack_start(GTK_BOX(threshold_hbox), threshold_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(threshold_hbox), threshold_input, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(threshold_hbox), threshold_minutes_label, FALSE, FALSE, 0);

	//dbd path input
	dbd_label = gtk_label_new(_("libdbi driver path:"));
	dbd_input = gtk_entry_new();
	if(gaim_prefs_exists("/plugins/gtk/cap/libdbi_drivers")) {
		gtk_entry_set_text(GTK_ENTRY(dbd_input), gaim_prefs_get_string("/plugins/gtk/cap/libdbi_drivers"));
	}
	dbd_hbox = gtk_hbox_new(FALSE, 18);
	g_signal_connect(G_OBJECT(dbd_input), "focus-out-event",
		G_CALLBACK(text_entry_prefs_cb), "/plugins/gtk/cap/libdbi_drivers");
	gtk_box_pack_start(GTK_BOX(dbd_hbox), dbd_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dbd_hbox), dbd_input, FALSE, FALSE, 0);

	//Config window
	gtk_box_pack_start(GTK_BOX(vbox), driver_vbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), msg_difference_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), last_seen_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), threshold_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), dbd_hbox, FALSE, FALSE, 0);

	return ret;
}

static void numeric_spinner_prefs_cb(GtkSpinButton *spinbutton, gpointer user_data) {
	gaim_prefs_set_int(user_data, gtk_spin_button_get_value_as_int(spinbutton));
}

static gboolean text_entry_prefs_cb(GtkWidget *widget, GdkEventFocus *event, gpointer user_data) {
	gaim_prefs_set_string(user_data, gtk_entry_get_text(GTK_ENTRY(widget)));
	return FALSE;
}

static void combobox_prefs_cb(GtkComboBox *widget, gpointer user_data) {
	gaim_prefs_set_string(user_data, gtk_combo_box_get_active_text(widget));
}

static void prefs_closed_cb(GtkObject *widget, gpointer user_data) {
	gboolean successfully_configured = FALSE;
	successfully_configured = add_plugin_functionality(_plugin_pointer);
	gaim_debug_info("cap", "Configured? %s\n", ((successfully_configured) ? "yes" : "no"));
	if(!successfully_configured) {
		if(error_msg) {
			GtkWidget *error_dialog = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"%s\n%s",
				_("Contact Availability Prediction configuration error."), get_error_msg());
			gtk_dialog_run(GTK_DIALOG(error_dialog));
			gtk_widget_destroy(error_dialog);
		}
	}
			
	gaim_prefs_set_bool(user_data, successfully_configured);
}

static GtkWidget * get_mysql_config() {
	GtkWidget *config_area = gtk_table_new(5, 2, TRUE);
	GtkWidget *username_label = gtk_label_new(_("Username:"));
	GtkWidget *username_input = gtk_entry_new();
	GtkWidget *password_label = gtk_label_new(_("Password:"));
	GtkWidget *password_input = gtk_entry_new();
	GtkWidget *host_label = gtk_label_new(_("Host:"));
	GtkWidget *host_input = gtk_entry_new();
	GtkWidget *db_label = gtk_label_new(_("Database:"));
	GtkWidget *db_input = gtk_entry_new();
	GtkWidget *port_label = gtk_label_new(_("Port:"));
	GtkWidget *port_input = gtk_spin_button_new_with_range(0, 10000, 1);

	gtk_entry_set_visibility(GTK_ENTRY(password_input), FALSE);

	gtk_table_attach_defaults(GTK_TABLE(config_area), host_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(config_area), host_input, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(config_area), port_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(config_area), port_input, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(config_area), db_label, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(config_area), db_input, 1, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(config_area), username_label, 0, 1, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(config_area), username_input, 1, 2, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(config_area), password_label, 0, 1, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(config_area), password_input, 1, 2, 4, 5);

	//Initialize with data
	if(gaim_prefs_exists("/plugins/gtk/cap/mysql/db_host")) {
		gtk_entry_set_text(GTK_ENTRY(host_input), gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_host"));
	} else {
		gtk_entry_set_text(GTK_ENTRY(host_input), "localhost");
	}
	if(gaim_prefs_exists("/plugins/gtk/cap/mysql/db_port")) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_input), gaim_prefs_get_int("/plugins/gtk/cap/mysql/db_port"));
	} else {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_input), 3306);
	}
	if(gaim_prefs_exists("/plugins/gtk/cap/mysql/db_user")) {
		gtk_entry_set_text(GTK_ENTRY(username_input), gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_user"));
	} else {
		gtk_entry_set_text(GTK_ENTRY(username_input), "root");
	}
	if(gaim_prefs_exists("/plugins/gtk/cap/mysql/db_password")) {
		gtk_entry_set_text(GTK_ENTRY(password_input), gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_password"));
	} else {
		gtk_entry_set_text(GTK_ENTRY(password_input), "");
	}
	if(gaim_prefs_exists("/plugins/gtk/cap/mysql/db_name")) {
		gtk_entry_set_text(GTK_ENTRY(db_input), gaim_prefs_get_string("/plugins/gtk/cap/mysql/db_name"));
	} else {
		gtk_entry_set_text(GTK_ENTRY(db_input), "cap");
	}

	//Add callbacks
	g_signal_connect(G_OBJECT(host_input), "focus-out-event",
		G_CALLBACK(text_entry_prefs_cb), "/plugins/gtk/cap/mysql/db_host");
	g_signal_connect(G_OBJECT(port_input), "value-changed",
		G_CALLBACK(numeric_spinner_prefs_cb), "/plugins/gtk/cap/mysql/db_port");
	g_signal_connect(G_OBJECT(username_input), "focus-out-event",
		G_CALLBACK(text_entry_prefs_cb), "/plugins/gtk/cap/mysql/db_user");
	g_signal_connect(G_OBJECT(password_input), "focus-out-event",
		G_CALLBACK(text_entry_prefs_cb), "/plugins/gtk/cap/mysql/db_password");
	g_signal_connect(G_OBJECT(db_input), "focus-out-event",
		G_CALLBACK(text_entry_prefs_cb), "/plugins/gtk/cap/mysql/db_name");

	gtk_widget_show_all(config_area);

	return config_area;
}

static void driver_config_expanded(GObject *object, GParamSpec *param_spec, gpointer user_data) {
	GtkExpander *expander;
	gchar *driver = user_data;
	expander = GTK_EXPANDER(object);
	if(gtk_expander_get_expanded(expander)) {
		if(g_ascii_strcasecmp(driver, "mysql") == 0) {
			gtk_container_add(GTK_CONTAINER(expander), get_mysql_config());
		}
	} else {
		gtk_container_remove(GTK_CONTAINER(expander), gtk_bin_get_child(GTK_BIN(expander)));
	}
}

static GaimGtkPluginUiInfo ui_info = {
	get_config_frame,
	0 /* page_num (reserved) */
};

static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,							/**< type		*/
	GAIM_GTK_PLUGIN_TYPE,							/**< ui_requirement */
	0,												/**< flags		*/
	NULL,											/**< dependencies   */
	GAIM_PRIORITY_DEFAULT,							/**< priority		*/
	CAP_PLUGIN_ID,									/**< id			*/
	"Contact Availability Prediction",				/**< name		*/
	VERSION,										/**< version		*/
	N_("Contact Availability Prediction plugin."),	/**  summary		*/
	N_("The contact availability plugin (cap) is used to display statistical information about buddies in a users contact list."),
	/**  description	*/
	"Geoffrey Foster <geoffrey.foster@gmail.com>",	/**< author		*/
	GAIM_WEBSITE,									/**< homepage		*/
	plugin_load,									/**< load		*/
	plugin_unload,									/**< unload		*/
	NULL,											/**< destroy		*/
	&ui_info,										/**< ui_info		*/
	NULL,											/**< extra_info	 */
	NULL,											/**< prefs_info		*/
	NULL
};

static void init_plugin(GaimPlugin *plugin) {
	/* TODO: change this before distributing */
	gaim_prefs_add_none("/plugins/gtk/cap");
	gaim_prefs_add_int("/plugins/gtk/cap/max_seen_difference", 1);
	gaim_prefs_add_int("/plugins/gtk/cap/max_msg_difference", 10);
	gaim_prefs_add_int("/plugins/gtk/cap/threshold", 5);
	gaim_prefs_add_string("/plugins/gtk/cap/libdbi_drivers", "/usr/lib/dbd");
	gaim_prefs_add_bool("/plugins/gtk/cap/configured", FALSE);
	gaim_prefs_add_string("/plugins/gtk/cap/db_driver", "mysql");
	gaim_prefs_add_none("/plugins/gtk/cap/mysql");
	gaim_prefs_add_string("/plugins/gtk/cap/mysql/db_host", "localhost");
	gaim_prefs_add_int("/plugins/gtk/cap/mysql/db_port", 3306);
	gaim_prefs_add_string("/plugins/gtk/cap/mysql/db_user", "root");
	gaim_prefs_add_string("/plugins/gtk/cap/mysql/db_password", "");
	gaim_prefs_add_string("/plugins/gtk/cap/mysql/db_name", "cap");
}

GAIM_INIT_PLUGIN(cap, init_plugin, info);
