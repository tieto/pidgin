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
#include "../config.h"
#endif
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <aim.h>

#ifdef USE_SCREENSAVER
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#endif /* USE_SCREENSAVER */

#include "multi.h"
#include "gaim.h"


gint check_idle(struct gaim_connection *gc)
{
	time_t t;
#ifdef USE_SCREENSAVER
	static XScreenSaverInfo *mit_info = NULL;
	static Display *d = NULL;
	time_t idle_time;
#endif

        /* Not idle, really...  :) */
	update_all_buddies();

	plugin_event(event_blist_update, 0, 0, 0, 0);
        
	time(&t);

	if (report_idle == 0)
                return TRUE;

#ifdef USE_SCREENSAVER
	if (report_idle == IDLE_SCREENSAVER) {
		if (!d)
			d = XOpenDisplay((char *)NULL);
		if (mit_info == NULL) {
			mit_info = XScreenSaverAllocInfo ();
		}
		XScreenSaverQueryInfo (d, DefaultRootWindow(d), mit_info);
		idle_time = (mit_info->idle)/1000;
	} else
#endif /* USE_SCREENSAVER */
		idle_time = t - gc->lastsent;

	if (idle_time > 600 && !gc->is_idle) { /* 10 minutes! */
		debug_printf("setting %s idle %d seconds\n", gc->username, idle_time);
		serv_set_idle(gc, idle_time);
		gc->is_idle = 1;
        } else if (idle_time < 600 && gc->is_idle) {
		debug_printf("setting %s unidle\n", gc->username);
		serv_touch_idle(gc);
	}

	return TRUE;

}
