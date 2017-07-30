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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "gtkidle.h"

#ifdef HAVE_IOKIT
# include <CoreFoundation/CoreFoundation.h>
# include <IOKit/IOKitLib.h>
#elif defined (_WIN32)
# include "win32/gtkwin32dep.h"
#endif

#include "idle.h"

#if !defined(HAVE_IOKIT) && !defined(_WIN32)
typedef struct {
	gchar *bus_name;
	gchar *object_path;
	gchar *iface_name;
} PidginDBusScreenSaverInfo;

static const PidginDBusScreenSaverInfo screensavers[] = {
	{
		"org.freedesktop.ScreenSaver",
		"/org/freedesktop/ScreenSaver",
		"org.freedesktop.ScreenSaver"
	}, {
		"org.gnome.ScreenSaver",
		"/org/gnome/ScreenSaver",
		"org.gnome.ScreenSaver"
	}, {
		"org.kde.ScreenSaver",
		"/org/kde/ScreenSaver",
		"org.kde.ScreenSaver"
	},
};
#endif /* !HAVE_IOKIT && !_WIN32 */

/*
 * pidgin_get_time_idle:
 *
 * Get the number of seconds the user has been idle.  In Unix-world
 * this is based on the DBus ScreenSaver interfaces.  In MS Windows this
 * is based on keyboard/mouse usage information obtained from the OS.
 * In MacOS X, this is based on keyboard/mouse usage information
 * obtained from the OS, if configure detected IOKit.  Otherwise,
 * MacOS X is handled as a case of Unix.
 *
 * Returns: The number of seconds the user has been idle.
 */
static time_t
pidgin_get_time_idle(void)
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
	static guint idx = 0;
	GApplication *app;
	GDBusConnection *conn;
	GVariant *reply = NULL;
	guint32 active_time = 0;
	GError *error = NULL;

	app = g_application_get_default();

	if (app == NULL) {
		purple_debug_error("gtkidle",
				"Unable to retrieve GApplication");
		return 0;
	}

	conn = g_application_get_dbus_connection(app);

	if (conn == NULL) {
		purple_debug_misc("gtkidle",
				"GApplication lacking DBus connection. "
				"Skip checking ScreenSaver interface");
		return 0;
	}

	for (; idx < G_N_ELEMENTS(screensavers); ++idx) {
		const PidginDBusScreenSaverInfo *info = &screensavers[idx];

		reply = g_dbus_connection_call_sync(conn,
				info->bus_name, info->object_path,
				info->iface_name, "GetActiveTime",
				NULL, G_VARIANT_TYPE("(u)"),
				G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000,
				NULL, &error);

		if (reply != NULL) {
			break;
		}

		if (g_error_matches(error, G_DBUS_ERROR,
				G_DBUS_ERROR_NOT_SUPPORTED)) {
			purple_debug_info("gtkidle",
					"Querying idle time on '%s' "
					"unsupported. Trying the next one",
					info->bus_name);
		} else if (g_error_matches(error, G_DBUS_ERROR,
				G_DBUS_ERROR_NAME_HAS_NO_OWNER)) {
			purple_debug_info("gtkidle",
					"Querying idle time on '%s' "
					"not found. Trying the next one",
					info->bus_name);
		} else {
			purple_debug_error("gtkidle",
					"Querying idle time on '%s' "
					"error: %s", info->bus_name,
					error->message);
		}

		g_clear_error(&error);
	}

	if (reply == NULL) {
		purple_debug_warning("gtkidle",
				"Failed to query ScreenSaver active time: "
				"No working ScreenSaver interfaces");
		return 0;
	}

	g_variant_get(reply, "(u)", &active_time);
	g_variant_unref(reply);

	return active_time;
#  endif /* !_WIN32 */
# endif /* !HAVE_IOKIT */
}

static PurpleIdleUiOps ui_ops =
{
	pidgin_get_time_idle,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleIdleUiOps *
pidgin_idle_get_ui_ops()
{
	return &ui_ops;
}
