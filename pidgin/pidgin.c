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
#include "pidgin.h"

#ifdef _WIN32
/* suppress gcc "no previous prototype" warning */
int __cdecl pidgin_main(HINSTANCE hint, int argc, char *argv[]);
int __cdecl pidgin_main(HINSTANCE hint, int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	const gchar *test_prgname;

#ifdef _WIN32
	SetConsoleOutputCP(65001); /* UTF-8 */
#endif

#if !GLIB_CHECK_VERSION(2, 32, 0)
	/* GLib threading system is automaticaly initialized since 2.32.
	 * For earlier versions, it have to be initialized before calling any
	 * Glib or GTK+ functions.
	 */
	g_thread_init(NULL);
#endif

	/* This is for UI testing purposes only, don't use it! */
	test_prgname = g_getenv("PIDGIN_TEST_PRGNAME");
	g_set_prgname(test_prgname ? test_prgname : "Pidgin");
	g_set_application_name(PIDGIN_NAME);

#ifdef _WIN32
	winpidgin_set_exe_hinstance(hint);
#endif

	return pidgin_start(argc, argv);
}
