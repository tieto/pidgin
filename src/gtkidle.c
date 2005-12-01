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

#include "idle.h"

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
#ifdef USE_SCREENSAVER
static time_t
gaim_gtk_get_time_idle()
{
# ifndef _WIN32
	/* Query xscreensaver */
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
# else
	/* Query windows */
	return (GetTickCount() - wgaim_get_lastactive()) / 1000;
# endif /* _WIN32 */
}
#endif /* USE_SCREENSAVER */

static GaimIdleUiOps ui_ops =
{
#ifdef USE_SCREENSAVER
	gaim_gtk_get_time_idle
#else
	NULL
#endif /* USE_SCREENSAVER */
};

GaimIdleUiOps *
gaim_gtk_idle_get_ui_ops()
{
	return &ui_ops;
}
