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
#include "internal.h"

#ifdef USE_SCREENSAVER
# ifndef _WIN32
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/extensions/scrnsaver.h>
#  include <gdk/gdkx.h>
# else
#  include "idletrack.h"
# endif
#endif /* USE_SCREENSAVER */

#include "connection.h"
#include "debug.h"
#include "log.h"
#include "multi.h"
#include "prefs.h"
#include "prpl.h"

#include "ui.h"

#include "gtkprefs.h"

/* XXX For the away stuff */
#include "gaim.h"

#define IDLEMARK 600   	/* 10 minutes! */

gint check_idle(gpointer data)
{
	const char *report_idle;
	GaimConnection *gc = data;
	GaimAccount *account;
	time_t t;
#ifdef USE_SCREENSAVER
#ifndef _WIN32
	static XScreenSaverInfo *mit_info = NULL;
#endif
#endif
	int idle_time;

	account = gaim_connection_get_account(gc);

	gaim_event_broadcast(event_blist_update);

	time(&t);

	report_idle = gaim_prefs_get_string("/gaim/gtk/idle/reporting_method");

#ifdef USE_SCREENSAVER
	if (report_idle != NULL && !strcmp(report_idle, "system")) {
#ifndef _WIN32
		int event_base, error_base;
		if (XScreenSaverQueryExtension(GDK_DISPLAY(), &event_base, &error_base)) {
			if (mit_info == NULL) {
				mit_info = XScreenSaverAllocInfo();
			}
			XScreenSaverQueryInfo(GDK_DISPLAY(), GDK_ROOT_WINDOW(), mit_info);
			idle_time = (mit_info->idle) / 1000;
		} else
			idle_time = 0;
#else
		idle_time = (GetTickCount() - wgaim_get_lastactive()) / 1000;
#endif
	} else
#endif /* USE_SCREENSAVER */
		idle_time = t - gc->last_sent_time;

	if (gaim_prefs_get_bool("/core/away/away_when_idle") &&
		(idle_time > (60 * gaim_prefs_get_int("/core/away/mins_before_away")))
		&& (!gc->is_auto_away)) {

		if (!gc->away) {
			struct away_message *default_away = NULL;
			const char *default_name;
			GSList *l;

			default_name = gaim_prefs_get_string("/core/away/default_message");

			for(l = away_messages; l; l = l->next) {
				if(!strcmp(default_name, ((struct away_message *)l->data)->name)) {
					default_away = l->data;
					break;
				}
			}

			if(!default_away && away_messages)
				default_away = away_messages->data;

			gaim_debug(GAIM_DEBUG_INFO, "idle",
					   "Making %s away automatically\n",
					   gaim_account_get_username(account));
			if (g_list_length(gaim_connections_get_all()) == 1)
				do_away_message(NULL, default_away);
			else if (default_away)
				serv_set_away(gc, GAIM_AWAY_CUSTOM, default_away->message);
			gc->is_auto_away = 1;
		} else
			gc->is_auto_away = 2;
	} else if (gc->is_auto_away &&
			idle_time < 60 * gaim_prefs_get_int("/core/away/mins_before_away")) {
		if (gc->is_auto_away == 2) {
			gc->is_auto_away = 0;
			return TRUE;
		}
		gc->is_auto_away = 0;
		if (awaymessage == NULL) {
			gaim_debug(GAIM_DEBUG_INFO, "idle",
					   "Removing auto-away message for %s\n", gaim_account_get_username(account));
			serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
		} else {
			if (g_list_length(gaim_connections_get_all()) == 1)
				do_im_back(0, 0);
			else {
				gaim_debug(GAIM_DEBUG_INFO, "idle",
						   "Replacing auto-away with global for %s\n",
						   gaim_account_get_username(account));
				serv_set_away(gc, GAIM_AWAY_CUSTOM, awaymessage->message);
			}
		}
	}


	/* If we're not reporting idle times to the server, still use Gaim
	   usage for auto-away, but quit here so we don't report to the 
	   server */

	if (report_idle != NULL && !strcmp(report_idle, "none"))
		return TRUE;

	if (idle_time >= IDLEMARK && !gc->is_idle) {
		gaim_debug(GAIM_DEBUG_INFO, "idle", "Setting %s idle %d seconds\n",
				   gaim_account_get_username(account), idle_time);
		serv_set_idle(gc, idle_time);
		gc->is_idle = 1;
		system_log(log_idle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON);
	} else if (idle_time < IDLEMARK && gc->is_idle) {
		gaim_debug(GAIM_DEBUG_INFO, "idle", "Setting %s unidle\n",
				   gaim_account_get_username(account));
		serv_touch_idle(gc);
		system_log(log_unidle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON);
	}

	return TRUE;

}
