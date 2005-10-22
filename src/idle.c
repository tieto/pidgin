/*
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
#include "prefs.h"
#include "savedstatuses.h"
#include "signals.h"

#define IDLEMARK 600	/* 10 minutes! */

typedef enum
{
	GAIM_IDLE_NOT_AWAY = 0,
	GAIM_IDLE_AUTO_AWAY,
	GAIM_IDLE_AWAY_BUT_NOT_AUTO_AWAY

} GaimAutoAwayState;

#ifdef USE_SCREENSAVER
/**
 * Get the number of seconds the user has been idle.  In Unix-world
 * this is based on the X Windows usage.  In MS Windows this is based
 * on keyboard/mouse usage.
 *
 * In Debian bug #271639, jwz says:
 *
 * Gaim should simply ask xscreensaver how long the user has been idle:
 *   % xscreensaver-command -time
 *   XScreenSaver 4.18: screen blanked since Tue Sep 14 14:10:45 2004
 *
 * Or you can monitor the _SCREENSAVER_STATUS property on root window #0.
 * Element 0 is the status (0, BLANK, LOCK), element 1 is the time_t since
 * the last state change, and subsequent elements are which hack is running
 * on the various screens:
 *   % xprop -f _SCREENSAVER_STATUS 32ac -root _SCREENSAVER_STATUS
 *   _SCREENSAVER_STATUS(INTEGER) = BLANK, 1095196626, 10, 237
 *
 * See watch() in xscreensaver/driver/xscreensaver-command.c.
 *
 * @return The number of seconds the user has been idle.
 */
static int
get_idle_time_from_system()
{
#ifndef _WIN32
	static XScreenSaverInfo *mit_info = NULL;
	int event_base, error_base;
	if (XScreenSaverQueryExtension(GDK_DISPLAY(), &event_base, &error_base)) {
		if (mit_info == NULL) {
			mit_info = XScreenSaverAllocInfo();
		}
		XScreenSaverQueryInfo(GDK_DISPLAY(), GDK_ROOT_WINDOW(), mit_info);
		return (mit_info->idle) / 1000;
	} else
		return 0;
#else
	return (GetTickCount() - wgaim_get_lastactive()) / 1000;
#endif
}
#endif /* USE_SCREENSAVER */

/*
 * This function should be called when you think your idle state
 * may have changed.  Maybe you're over the 10-minute mark and
 * Gaim should start reporting idle time to the server.  Maybe
 * you've returned from being idle.  Maybe your auto-away message
 * should be set.
 *
 * There is no harm to calling this many many times, other than
 * it will be kinda slow.  This is called every 20 seconds by a
 * timer set when an account logs in.  It is also called when
 * you send an IM, a chat, etc.
 *
 * This function has 3 sections.
 * 1. Get your idle time.  It will query XScreenSaver or Windows
 *    or get the Gaim idle time.  Whatever.
 * 2. Set or unset your auto-away message.
 * 3. Report your current idle time to the IM server.
 */
/* TODO: This needs to be namespaced or moved elsewhere. */
gint
check_idle(gpointer data)
{
	GaimConnection *gc = data;
	gboolean report_idle;
	GaimAccount *account;
	time_t t;
	int idle_time;

	account = gaim_connection_get_account(gc);

	gaim_signal_emit(gaim_blist_get_handle(), "update-idle");

	time(&t);

	report_idle = gaim_prefs_get_bool("/gaim/gtk/idle/report");

#ifdef USE_SCREENSAVER
		idle_time = get_idle_time_from_system();
#else
		/*
		 * If Gaim wasn't built with xscreensaver support, then
		 * fallback to calculating our idle time based on when
		 * we last sent a message.
		 */
		idle_time = t - gc->last_sent_time;
#endif /* USE_SCREENSAVER */

	/* Should we become auto-away? */
	if (gaim_prefs_get_bool("/core/away/away_when_idle") &&
		(idle_time > (60 * gaim_prefs_get_int("/core/away/mins_before_away")))
		&& (!gc->is_auto_away))
	{
		GaimPresence *presence;

		presence = gaim_account_get_presence(account);

		if (gaim_presence_is_available(presence))
		{
			const char *idleaway_name;
			GaimSavedStatus *saved_status;

			gaim_debug_info("idle", "Making %s auto-away\n",
							gaim_account_get_username(account));

			/* Mark our accounts "away" using the idleaway status */
			idleaway_name = gaim_prefs_get_string("/core/status/idleaway");
			saved_status = gaim_savedstatus_find(idleaway_name);
			gaim_savedstatus_activate(saved_status);

			gc->is_auto_away = GAIM_IDLE_AUTO_AWAY;
		} else {
			gc->is_auto_away = GAIM_IDLE_AWAY_BUT_NOT_AUTO_AWAY;
		}

	/* Should we return from being auto-away? */
	} else if (gc->is_auto_away &&
			idle_time < 60 * gaim_prefs_get_int("/core/away/mins_before_away")) {
		if (gc->is_auto_away == GAIM_IDLE_AWAY_BUT_NOT_AUTO_AWAY) {
			gc->is_auto_away = GAIM_IDLE_NOT_AWAY;
			return TRUE;
		}
		gc->is_auto_away = GAIM_IDLE_NOT_AWAY;

		/* XXX STATUS AWAY CORE/UI */
		/* Need to set this connection to available here */
	}

	/* Deal with reporting idleness to the server, if appropriate */
	if (report_idle && idle_time >= IDLEMARK && !gc->is_idle) {
		gaim_debug_info("idle", "Setting %s idle %d seconds\n",
				   gaim_account_get_username(account), idle_time);
		serv_set_idle(gc, idle_time);
		gc->is_idle = 1;
		/* LOG	system_log(log_idle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON); */
	} else if ((!report_idle || idle_time < IDLEMARK) && gc->is_idle) {
		gaim_debug_info("idle", "Setting %s unidle\n",
				   gaim_account_get_username(account));
		serv_touch_idle(gc);
		/* LOG	system_log(log_unidle, gc, NULL, OPT_LOG_BUDDY_IDLE | OPT_LOG_MY_SIGNON); */
	}

	return TRUE;
}
