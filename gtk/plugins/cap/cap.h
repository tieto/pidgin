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

#ifndef _CAP_H_
#define _CAP_H_

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"

#include "gtkconv.h"
#include "gtkblist.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "blist.h"
#include "notify.h"
#include "version.h"
#include "debug.h"

#include "util.h"

#include <dbi/dbi.h>
#include <glib.h>
#include <time.h>
#include "cap_statistics.h"

#define CAP_PLUGIN_ID "gtk-g-off_-cap"

/* Variables used throughout lifetime of the plugin */
GaimPlugin *_plugin_pointer;
dbi_conn _conn; /**< The database connection */
dbi_driver _driver; /**< The database driver */
GHashTable *_buddy_stats = NULL;
GHashTable *_my_offline_times = NULL;
GString *error_msg = NULL;
gboolean _signals_connected;

enum driver_types {MYSQL};

/* Function definitions */
static char * quote_string(const char *str);
static gboolean plugin_load(GaimPlugin *plugin);
static gboolean add_plugin_functionality(GaimPlugin *plugin);
static gboolean plugin_unload(GaimPlugin *plugin);
static GtkWidget * get_config_frame(GaimPlugin *plugin);
static void numeric_spinner_prefs_cb(GtkSpinButton *spinbutton, gpointer user_data);
static gboolean text_entry_prefs_cb(GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
static void combobox_prefs_cb(GtkComboBox *widget, gpointer user_data);
static void prefs_closed_cb(GtkObject *widget, gpointer user_data);
static GtkWidget * get_mysql_config();
static void driver_config_expanded(GObject *object, GParamSpec *param_spec, gpointer user_data);
static void init_plugin(GaimPlugin *plugin);
static void generate_prediction(CapStatistics *statistics);
static double generate_prediction_for(GaimBuddy *buddy);
static CapStatistics * get_stats_for(GaimBuddy *buddy);
static void destroy_stats(gpointer data);
static gboolean remove_stats_for(GaimBuddy *buddy);
static dbi_result insert_cap_msg_count_success(const char *buddy_name, const char *account, const char *protocol, int minute);
static dbi_result insert_cap_status_count_success(const char *buddy_name, const char *account, const char *protocol, const char *status_id);
static dbi_result insert_cap_msg_count_failed(const char *buddy_name, const char *account, const char *protocol, int minute);
static dbi_result insert_cap_status_count_failed(const char *buddy_name, const char *account, const char *protocol, const char *status_id);
static void insert_cap_success(CapStatistics *stats);
static void insert_cap_failure(CapStatistics *stats);
static gboolean max_message_difference_cb(gpointer data);

/* Various CAP helper functions */
static const gchar * get_error_msg();
static void set_error_msg(const gchar *msg);
static void reset_all_last_message_times(gpointer key, gpointer value, gpointer user_data);
static GaimStatus * get_status_for(GaimBuddy *buddy);
static void create_tables();
static gboolean create_database_connection();
static guint word_count(const gchar *string);
static gboolean last_message_time_in_range(CapStatistics *statistics, gdouble max_difference);
static gboolean last_seen_time_in_range(CapStatistics *statistics, gdouble max_difference);
static void insert_status_change(CapStatistics *statistics);
static void insert_status_change_from_gaim_status(CapStatistics *statistics, GaimStatus *status);
static void insert_word_count(const char *sender, const char *receiver, guint count);

/* Gaim Signal Handlers */
static void sent_im_msg(GaimAccount *account, const char *receiver, const char *message);
static void received_im_msg(GaimAccount *account, char *sender, char *message,
		GaimConversation *conv, GaimMessageFlags flags);
static void buddy_status_changed(GaimBuddy *buddy, GaimStatus *old_status, GaimStatus *status);
static void buddy_signed_on(GaimBuddy *buddy);
static void buddy_signed_off(GaimBuddy *buddy);
static void buddy_idle(GaimBuddy *buddy, gboolean old_idle, gboolean idle);
static void blist_node_extended_menu(GaimBlistNode *node, GList **menu);
static void drawing_tooltip(GaimBlistNode *node, GString *text, gboolean full);
static void signed_on(GaimConnection *gc);
static void signed_off(GaimConnection *gc);

/* Call backs */
void display_statistics_action_cb(GaimBlistNode *node, gpointer data);

#endif
