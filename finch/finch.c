/**
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 */

#include "internal.h"
#include "core.h"

#include "finch.h"
#include "gnt.h"

int main(int argc, char *argv[])
{
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

#if !GLIB_CHECK_VERSION(2, 32, 0)
	/* GLib threading system is automaticaly initialized since 2.32.
	 * For earlier versions, it have to be initialized before calling any
	 * Glib or GTK+ functions.
	 */
	g_thread_init(NULL);
#endif

	g_set_prgname("Finch");
	g_set_application_name(_("Finch"));

	if (finch_start(&argc, &argv)) {
		gnt_main();

#ifdef STANDALONE
		purple_core_quit();
#endif
	}

	return 0;
}
