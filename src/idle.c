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
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#ifdef USE_SCREENSAVER
#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#include <gdk/gdkx.h>
#else
#include "idletrack.h"
#endif
#endif /* USE_SCREENSAVER */

#include "multi.h"
#include "gaim.h"
#include "prpl.h"

#define IDLEMARK 600   	/* 10 minutes! */

gint check_idle(gpointer data)
{
	struct gaim_connection *gc = data;
	time_t t;
#ifdef USE_SCREENSAVER
#ifndef _WIN32
	static XScreenSaverInfo *mit_info = NULL;
#endif
#endif
	int idle_time;

	/* Not idle, really...  :) */
	update_idle_times();

	plugin_event(event_blist_update);

	time(&t);


#ifdef USE_SCREENSAVER
	if (report_idle == IDLE_SCREENSAVER) {
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
		idle_time = t - gc->lastsent;

	if ((away_options & OPT_AWAY_AUTO) && (idle_time > (60 * auto_away)) && (!gc->is_auto_away)) {
		if (!gc->away) {
			debug_printf("making %s away automatically\n", gc->username);
			if (g_slist_length(connections) == 1)
				do_away_message(NULL, default_away);
			else if (default_away)
				serv_set_away(gc, GAIM_AWAY_CUSTOM, default_away->message);
			gc->is_auto_away = 1;
			set_default_away(NULL, (gpointer)g_slist_index(away_messages, default_away));
		} else
			gc->is_auto_away = 2;
	} else if (gc->is_auto_away && idle_time < 60 * auto_away) {
		if (gc->is_auto_away == 2) {
			gc->is_auto_away = 0;
			return TRUE;
		}
		gc->is_auto_away = 0;
		if (awaymessage == NULL) {
			debug_printf("removing auto-away message for %s\n", gc->username);
			serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
		} else {
			if (g_slist_length(connections) == 1)
				do_im_back(0, 0);
			else {
				debug_printf("replacing auto-away with global for %s\n", gc->username);
				serv_set_away(gc, GAIM_AWAY_CUSTOM, awaymessage->message);
			}
		}
	}


	/* If we're not reporting idle times to the server, still use Gaim
	   usage for auto-away, but quit here so we don't report to the 
	   server */
	if (report_idle == IDLE_NONE) {
		return TRUE;
	}

	if (idle_time > IDLEMARK && !gc->is_idle) {
		debug_printf("setting %s idle %d seconds\n", gc->username, idle_time);
		serv_set_idle(gc, idle_time);
		gc->is_idle = 1;
		system_log(log_idle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON);
	} else if (idle_time < IDLEMARK && gc->is_idle) {
		debug_printf("setting %s unidle\n", gc->username);
		serv_touch_idle(gc);
		system_log(log_unidle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON);
	}

	return TRUE;

}
