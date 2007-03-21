/*
 * pidgin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "gtkidle.h"

#ifdef HAVE_IOKIT
# include <CoreFoundation/CoreFoundation.h>
# include <IOKit/IOKitLib.h>
#else
# ifdef USE_SCREENSAVER
#  ifdef _WIN32
#   include "idletrack.h"
#  else
    /* We're on X11 and not MacOS X with IOKit. */
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <X11/extensions/scrnsaver.h>
#   include <gdk/gdkx.h>
#  endif /* !_WIN32 */
# endif /* USE_SCREENSAVER */
#endif /* !HAVE_IOKIT */

#include "idle.h"

/**
 * Get the number of seconds the user has been idle.  In Unix-world
 * this is based on the X Windows usage.  In MS Windows this is
 * based on keyboard/mouse usage information obtained from the OS.
 * In MacOS X, this is based on keyboard/mouse usage information
 * obtained from the OS, if configure detected IOKit.  Otherwise,
 * MacOS X is handled as a case of X Windows.
 *
 * In Debian bug #271639, jwz says:
 *
 * Purple should simply ask xscreensaver how long the user has been idle:
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
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
static time_t
pidgin_get_time_idle()
{
# ifdef HAVE_IOKIT
	/* Query the IOKit API */

	static io_service_t macIOsrvc = NULL;
	CFTypeRef property;
	uint64_t idle_time = 0; /* nanoseconds */

	if (macIOsrvc == NULL)
	{
		mach_port_t master;
		IOMasterPort(MACH_PORT_NULL, &master);
		macIOsrvc = IOServiceGetMatchingService(master,
		                                        IOServiceMatching("IOHIDSystem"));
	}

	property = IORegistryEntryCreateCFProperty(macIOsrvc, CFSTR("HIDIdleTime"),
	                                           kCFAllocatorDefault, 0);
	CFNumberGetValue((CFNumberRef)property,
	                 kCFNumberSInt64Type, &idle_time);
	CFRelease(property);

	/* convert nanoseconds to seconds */
	return idle_time / 1000000000;
# else
#  ifdef _WIN32
	/* Query Windows */
	return (GetTickCount() - winpidgin_get_lastactive()) / 1000;
#  else
	/* We're on X11 and not MacOS X with IOKit. */

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
#  endif /* !_WIN32 */
# endif /* !HAVE_IOKIT */
}
#endif /* USE_SCREENSAVER || HAVE_IOKIT */

static PurpleIdleUiOps ui_ops =
{
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
	pidgin_get_time_idle
#else
	NULL
#endif /* USE_SCREENSAVER || HAVE_IOKIT */
};

PurpleIdleUiOps *
pidgin_idle_get_ui_ops()
{
	return &ui_ops;
}
