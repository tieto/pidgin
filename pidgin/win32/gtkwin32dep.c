/**
 * @file gtkwin32dep.c UI Win32 Specific Functionality
 * @ingroup win32
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
#define _WIN32_IE 0x500
#ifndef WINVER
#define WINVER 0x0500 /* W2K */
#endif
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <winuser.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#include "internal.h"

#include "debug.h"
#include "notify.h"
#include "network.h"

#include "resource.h"
#include "zlib.h"
#include "untar.h"

#include "gtkwin32dep.h"
#include "win32dep.h"
#include "gtkconv.h"
#include "gtkconn.h"
#include "util.h"
#ifdef USE_GTKSPELL
#include "wspell.h"
#endif

/*
 *  GLOBALS
 */
HINSTANCE exe_hInstance = 0;
HINSTANCE dll_hInstance = 0;
HWND messagewin_hwnd;
static int gtkwin32_handle;

static gboolean pwm_handles_connections = TRUE;


/*
 *  PUBLIC CODE
 */

HINSTANCE winpidgin_exe_hinstance(void) {
	return exe_hInstance;
}

HINSTANCE winpidgin_dll_hinstance(void) {
	return dll_hInstance;
}

int winpidgin_gz_decompress(const char* in, const char* out) {
	gzFile fin;
	FILE *fout;
	char buf[1024];
	int ret;

	if((fin = gzopen(in, "rb"))) {
		if(!(fout = g_fopen(out, "wb"))) {
			purple_debug_error("winpidgin_gz_decompress", "Error opening file: %s\n", out);
			gzclose(fin);
			return 0;
		}
	}
	else {
		purple_debug_error("winpidgin_gz_decompress", "gzopen failed to open: %s\n", in);
		return 0;
	}

	while((ret = gzread(fin, buf, 1024))) {
		if(fwrite(buf, 1, ret, fout) < ret) {
			purple_debug_error("wpurple_gz_decompress", "Error writing %d bytes to file\n", ret);
			gzclose(fin);
			fclose(fout);
			return 0;
		}
	}
	fclose(fout);
	gzclose(fin);

	if(ret < 0) {
		purple_debug_error("winpidgin_gz_decompress", "gzread failed while reading: %s\n", in);
		return 0;
	}

	return 1;
}

int winpidgin_gz_untar(const char* filename, const char* destdir) {
	char tmpfile[_MAX_PATH];
	char template[]="wpidginXXXXXX";

	sprintf(tmpfile, "%s%s%s", g_get_tmp_dir(), G_DIR_SEPARATOR_S, _mktemp(template));
	if(winpidgin_gz_decompress(filename, tmpfile)) {
		int ret;
		if(untar(tmpfile, destdir, UNTAR_FORCE | UNTAR_QUIET))
			ret = 1;
		else {
			purple_debug_error("winpidgin_gz_untar", "Failure untarring %s\n", tmpfile);
			ret = 0;
		}
		g_unlink(tmpfile);
		return ret;
	}
	else {
		purple_debug_error("winpidgin_gz_untar", "Failed to gz decompress %s\n", filename);
		return 0;
	}
}

void winpidgin_shell_execute(const char *target, const char *verb, const char *clazz) {

	SHELLEXECUTEINFOW wsinfo;
	wchar_t *w_uri, *w_verb, *w_clazz = NULL;

	g_return_if_fail(target != NULL);
	g_return_if_fail(verb != NULL);

	w_uri = g_utf8_to_utf16(target, -1, NULL, NULL, NULL);
	w_verb = g_utf8_to_utf16(verb, -1, NULL, NULL, NULL);

	memset(&wsinfo, 0, sizeof(wsinfo));
	wsinfo.cbSize = sizeof(wsinfo);
	wsinfo.lpVerb = w_verb;
	wsinfo.lpFile = w_uri;
	wsinfo.nShow = SW_SHOWNORMAL;
	wsinfo.fMask |= SEE_MASK_FLAG_NO_UI;
	if (clazz != NULL) {
		w_clazz = g_utf8_to_utf16(clazz, -1, NULL, NULL, NULL);
		wsinfo.fMask |= SEE_MASK_CLASSNAME;
		wsinfo.lpClass = w_clazz;
	}

	if(!ShellExecuteExW(&wsinfo))
		purple_debug_error("winpidgin", "Error opening URI: %s error: %d\n",
			target, (int) wsinfo.hInstApp);

	g_free(w_uri);
	g_free(w_verb);
	g_free(w_clazz);

}

void winpidgin_notify_uri(const char *uri) {
	/* We'll allow whatever URI schemes are supported by the
	 * default http browser.
	 */
	winpidgin_shell_execute(uri, "open", "http");
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static void*
winpidgin_netconfig_changed_cb(void *data)
{
	pwm_handles_connections = FALSE;

	return NULL;
}

static void*
winpidgin_get_handle(void)
{
	static int handle;

	return &handle;
}

static gboolean
winpidgin_pwm_reconnect()
{
	purple_signal_disconnect(purple_network_get_handle(), "network-configuration-changed",
		winpidgin_get_handle(), PURPLE_CALLBACK(winpidgin_netconfig_changed_cb));

	if (pwm_handles_connections == TRUE) {
		PurpleConnectionUiOps *ui_ops = pidgin_connections_get_ui_ops();

		purple_debug_info("winpidgin", "Resumed from standby, reconnecting accounts.\n");

		if (ui_ops != NULL && ui_ops->network_connected != NULL)
			ui_ops->network_connected();
	} else {
		purple_debug_info("winpidgin", "Resumed from standby, gtkconn will handle reconnecting.\n");
		pwm_handles_connections = TRUE;
	}

	return FALSE;
}

static LRESULT CALLBACK message_window_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	if (msg == PIDGIN_WM_FOCUS_REQUEST) {
		purple_debug_info("winpidgin", "Got external Buddy List focus request.");
		purple_blist_set_visible(TRUE);
		return TRUE;
	} else if (msg == PIDGIN_WM_PROTOCOL_HANDLE) {
		char *proto_msg = (char *) lparam;
		purple_debug_info("winpidgin", "Got protocol handler request: %s\n", proto_msg ? proto_msg : "");
		purple_got_protocol_handler_uri(proto_msg);
		return TRUE;
	} else if (msg == WM_POWERBROADCAST) {
		if (wparam == PBT_APMQUERYSUSPEND) {
			purple_debug_info("winpidgin", "Windows requesting permission to suspend.\n");
			return TRUE;
		} else if (wparam == PBT_APMSUSPEND) {
			PurpleConnectionUiOps *ui_ops = pidgin_connections_get_ui_ops();

			purple_debug_info("winpidgin", "Entering system standby, disconnecting accounts.\n");

			if (ui_ops != NULL && ui_ops->network_disconnected != NULL)
				ui_ops->network_disconnected();

			purple_signal_connect(purple_network_get_handle(), "network-configuration-changed", winpidgin_get_handle(),
				PURPLE_CALLBACK(winpidgin_netconfig_changed_cb), NULL);

			return TRUE;
		} else if (wparam == PBT_APMRESUMESUSPEND) {
			purple_debug_info("winpidgin", "Resuming from system standby.\n");
			/* TODO: It seems like it'd be wise to use the NLA message, if possible, instead of this. */
			purple_timeout_add_seconds(1, winpidgin_pwm_reconnect, NULL);
			return TRUE;
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND winpidgin_message_window_init(void) {
	HWND win_hwnd;
	WNDCLASSEX wcx;
	LPCTSTR wname;

	wname = TEXT("WinpidginMsgWinCls");

	wcx.cbSize = sizeof(wcx);
	wcx.style = 0;
	wcx.lpfnWndProc = message_window_handler;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = winpidgin_exe_hinstance();
	wcx.hIcon = NULL;
	wcx.hCursor = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = wname;
	wcx.hIconSm = NULL;

	RegisterClassEx(&wcx);

	/* Create the window */
	if(!(win_hwnd = CreateWindow(wname, TEXT("WinpidginMsgWin"), 0, 0, 0, 0, 0,
			NULL, NULL, winpidgin_exe_hinstance(), 0))) {
		purple_debug_error("winpidgin",
			"Unable to create message window.\n");
		return NULL;
	}

	return win_hwnd;
}

static gboolean stop_flashing(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
	GtkWindow *window = data;
	gpointer handler_id;

	winpidgin_window_flash(window, FALSE);

	if ((handler_id = g_object_get_data(G_OBJECT(window), "flash_stop_handler_id"))) {
		g_signal_handler_disconnect(G_OBJECT(window), (gulong) GPOINTER_TO_UINT(handler_id));
		g_object_steal_data(G_OBJECT(window), "flash_stop_handler_id");
	}

	return FALSE;
}

void
winpidgin_window_flash(GtkWindow *window, gboolean flash) {
	GdkWindow * gdkwin;
	FLASHWINFO info;

	g_return_if_fail(window != NULL);

	gdkwin = GTK_WIDGET(window)->window;

	g_return_if_fail(GDK_IS_WINDOW(gdkwin));
	g_return_if_fail(GDK_WINDOW_TYPE(gdkwin) != GDK_WINDOW_CHILD);

	if(GDK_WINDOW_DESTROYED(gdkwin))
		return;

	memset(&info, 0, sizeof(FLASHWINFO));
	info.cbSize = sizeof(FLASHWINFO);
	info.hwnd = GDK_WINDOW_HWND(gdkwin);
	if (flash) {
		DWORD flashCount;
		info.uCount = 3;
		if (SystemParametersInfo(SPI_GETFOREGROUNDFLASHCOUNT, 0, &flashCount, 0))
			info.uCount = flashCount;
		info.dwFlags = FLASHW_ALL | FLASHW_TIMER;
	} else
		info.dwFlags = FLASHW_STOP;
	FlashWindowEx(&info);
	info.dwTimeout = 0;

}

void
winpidgin_conv_blink(PurpleConversation *conv, PurpleMessageFlags flags) {
	PidginWindow *win;
	GtkWindow *window;

	/* Don't flash for our own messages or system messages */
	if(flags & PURPLE_MESSAGE_SEND || flags & PURPLE_MESSAGE_SYSTEM)
		return;

	if(conv == NULL) {
		purple_debug_info("winpidgin", "No conversation found to blink.\n");
		return;
	}

	win = pidgin_conv_get_window(PIDGIN_CONVERSATION(conv));
	if(win == NULL) {
		purple_debug_info("winpidgin", "No conversation windows found to blink.\n");
		return;
	}
	window = GTK_WINDOW(win->window);

	/* Don't flash if the window is in the foreground */
	if (GetForegroundWindow() == GDK_WINDOW_HWND(GTK_WIDGET(window)->window))
		return;

	winpidgin_window_flash(window, TRUE);
	/* Stop flashing when window receives focus */
	if (g_object_get_data(G_OBJECT(window), "flash_stop_handler_id") == NULL) {
		gulong handler_id = g_signal_connect(G_OBJECT(window), "focus-in-event",
						     G_CALLBACK(stop_flashing), window);
		g_object_set_data(G_OBJECT(window), "flash_stop_handler_id", GUINT_TO_POINTER(handler_id));
	}
}

static gboolean
winpidgin_conv_im_blink(PurpleAccount *account, const char *who, char **message,
		PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/win32/blink_im"))
		winpidgin_conv_blink(conv, flags);
	return FALSE;
}

void winpidgin_init(HINSTANCE hint) {
	typedef void (__cdecl* LPFNSETLOGFILE)(const LPCSTR);
	LPFNSETLOGFILE MySetLogFile;
	gchar *exchndl_dll_path;

	purple_debug_info("winpidgin", "winpidgin_init start\n");

	exe_hInstance = hint;

	exchndl_dll_path = g_build_filename(wpurple_install_dir(), "exchndl.dll", NULL);
	MySetLogFile = (LPFNSETLOGFILE) wpurple_find_and_loadproc(exchndl_dll_path, "SetLogFile");
	g_free(exchndl_dll_path);
	exchndl_dll_path = NULL;
	if (MySetLogFile) {
		gchar *debug_dir, *locale_debug_dir;

		debug_dir = g_build_filename(purple_user_dir(), "pidgin.RPT", NULL);
		locale_debug_dir = g_locale_from_utf8(debug_dir, -1, NULL, NULL, NULL);

		purple_debug_info("winpidgin", "Setting exchndl.dll LogFile to %s\n", debug_dir);

		MySetLogFile(locale_debug_dir);

		g_free(debug_dir);
		g_free(locale_debug_dir);
	}

#ifdef USE_GTKSPELL
	winpidgin_spell_init();
#endif
	purple_debug_info("winpidgin", "GTK+ :%u.%u.%u\n",
		gtk_major_version, gtk_minor_version, gtk_micro_version);

	messagewin_hwnd = winpidgin_message_window_init();

	purple_debug_info("winpidgin", "winpidgin_init end\n");
}

void winpidgin_post_init(void) {

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/win32");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/win32/blink_im", TRUE);

	purple_signal_connect(pidgin_conversations_get_handle(),
		"displaying-im-msg", &gtkwin32_handle, PURPLE_CALLBACK(winpidgin_conv_im_blink),
		NULL);

}

/* Windows Cleanup */

void winpidgin_cleanup(void) {
	purple_debug_info("winpidgin", "winpidgin_cleanup\n");

	if(messagewin_hwnd)
		DestroyWindow(messagewin_hwnd);

}

/* DLL initializer */
/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	dll_hInstance = hinstDLL;
	return TRUE;
}

static gboolean
get_WorkingAreaRectForWindow(HWND hwnd, RECT *workingAreaRc) {

	HMONITOR monitor;
	MONITORINFO info;

	monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

	info.cbSize = sizeof(info);
	if(!GetMonitorInfo(monitor, &info))
		return FALSE;

	CopyRect(workingAreaRc, &(info.rcWork));
	return TRUE;
}

void winpidgin_ensure_onscreen(GtkWidget *win) {
	RECT winR, wAR, intR;
	HWND hwnd = GDK_WINDOW_HWND(win->window);

	g_return_if_fail(hwnd != NULL);
	GetWindowRect(hwnd, &winR);

	purple_debug_info("win32placement",
			"Window RECT: L:%ld R:%ld T:%ld B:%ld\n",
			winR.left, winR.right,
			winR.top, winR.bottom);

	if(!get_WorkingAreaRectForWindow(hwnd, &wAR)) {
		purple_debug_info("win32placement",
				"Couldn't get multimonitor working area\n");
		if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &wAR, FALSE)) {
			/* I don't think this will ever happen */
			wAR.left = 0;
			wAR.top = 0;
			wAR.bottom = GetSystemMetrics(SM_CYSCREEN);
			wAR.right = GetSystemMetrics(SM_CXSCREEN);
		}
	}

	purple_debug_info("win32placement",
			"Working Area RECT: L:%ld R:%ld T:%ld B:%ld\n",
			wAR.left, wAR.right,
			wAR.top, wAR.bottom);

	/** If the conversation window doesn't intersect perfectly, move it to do so */
	if(!(IntersectRect(&intR, &winR, &wAR)
				&& EqualRect(&intR, &winR))) {
		purple_debug_info("win32placement",
				"conversation window out of working area, relocating\n");

		/* Make sure the working area is big enough. */
		if ((winR.right - winR.left) <= (wAR.right - wAR.left)
				&& (winR.bottom - winR.top) <= (wAR.bottom - wAR.top)) {
			/* Is it off the bottom? */
			if (winR.bottom > wAR.bottom) {
				winR.top = wAR.bottom - (winR.bottom - winR.top);
				winR.bottom = wAR.bottom;
			}
			/* Is it off the top? */
			else if (winR.top < wAR.top) {
				winR.bottom = wAR.top + (winR.bottom - winR.top);
				winR.top = wAR.top;
			}

			/* Is it off the left? */
			if (winR.left < wAR.left) {
				winR.right = wAR.left + (winR.right - winR.left);
				winR.left = wAR.left;
			}
			/* Is it off the right? */
			else  if (winR.right > wAR.right) {
				winR.left = wAR.right - (winR.right - winR.left);
				winR.right = wAR.right;
			}

		} else {
 			/* We couldn't salvage it; move it to the top left corner of the working area */
 			winR.right = wAR.left + (winR.right - winR.left);
 			winR.bottom = wAR.top + (winR.bottom - winR.top);
 			winR.left = wAR.left;
 			winR.top = wAR.top;
		}

		purple_debug_info("win32placement",
			"Relocation RECT: L:%ld R:%ld T:%ld B:%ld\n",
			winR.left, winR.right,
			winR.top, winR.bottom);

		MoveWindow(hwnd, winR.left, winR.top,
				   (winR.right - winR.left),
				   (winR.bottom - winR.top), TRUE);
	}

}

DWORD winpidgin_get_lastactive() {
	DWORD result = 0;

	LASTINPUTINFO lii;
	memset(&lii, 0, sizeof(lii));
	lii.cbSize = sizeof(lii);
	if (GetLastInputInfo(&lii))
		result = lii.dwTime;

	return result;
}

