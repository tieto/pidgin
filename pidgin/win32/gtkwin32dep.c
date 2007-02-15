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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include "debug.h"
#include "notify.h"

#include "resource.h"
#include "idletrack.h"
#include "zlib.h"
#include "untar.h"

#include <libintl.h>

#include "gtkwin32dep.h"
#include "win32dep.h"
#include "gtkconv.h"
#include "util.h"
#include "wspell.h"

/*
 *  GLOBALS
 */
HINSTANCE exe_hInstance = 0;
HINSTANCE dll_hInstance = 0;
HWND messagewin_hwnd;
static int gtkwin32_handle;

typedef BOOL (CALLBACK* LPFNFLASHWINDOWEX)(PFLASHWINFO);
static LPFNFLASHWINDOWEX MyFlashWindowEx = NULL;


/*
 *  PUBLIC CODE
 */

HINSTANCE winpidgin_hinstance(void) {
	return exe_hInstance;
}

int winpidgin_gz_decompress(const char* in, const char* out) {
	gzFile fin;
	FILE *fout;
	char buf[1024];
	int ret;

	if((fin = gzopen(in, "rb"))) {
		if(!(fout = g_fopen(out, "wb"))) {
			gaim_debug_error("winpidgin_gz_decompress", "Error opening file: %s\n", out);
			gzclose(fin);
			return 0;
		}
	}
	else {
		gaim_debug_error("winpidgin_gz_decompress", "gzopen failed to open: %s\n", in);
		return 0;
	}

	while((ret = gzread(fin, buf, 1024))) {
		if(fwrite(buf, 1, ret, fout) < ret) {
			gaim_debug_error("wgaim_gz_decompress", "Error writing %d bytes to file\n", ret);
			gzclose(fin);
			fclose(fout);
			return 0;
		}
	}
	fclose(fout);
	gzclose(fin);

	if(ret < 0) {
		gaim_debug_error("winpidgin_gz_decompress", "gzread failed while reading: %s\n", in);
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
			gaim_debug_error("winpidgin_gz_untar", "Failure untarring %s\n", tmpfile);
			ret = 0;
		}
		g_unlink(tmpfile);
		return ret;
	}
	else {
		gaim_debug_error("winpidgin_gz_untar", "Failed to gz decompress %s\n", filename);
		return 0;
	}
}

void winpidgin_shell_execute(const char *target, const char *verb, const char *clazz) {

	g_return_if_fail(target != NULL);
	g_return_if_fail(verb != NULL);

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		SHELLEXECUTEINFOW wsinfo;
		wchar_t *w_uri, *w_verb, *w_clazz = NULL;

		w_uri = g_utf8_to_utf16(target, -1, NULL, NULL, NULL);
		w_verb = g_utf8_to_utf16(verb, -1, NULL, NULL, NULL);

		memset(&wsinfo, 0, sizeof(wsinfo));
		wsinfo.cbSize = sizeof(wsinfo);
		wsinfo.lpVerb = w_verb;
		wsinfo.lpFile = w_uri;
		wsinfo.nShow = SW_SHOWNORMAL;
		if (clazz != NULL) {
			w_clazz = g_utf8_to_utf16(clazz, -1, NULL, NULL, NULL);
			wsinfo.fMask |= SEE_MASK_CLASSNAME;
			wsinfo.lpClass = w_clazz;
		}

		if(!ShellExecuteExW(&wsinfo))
			gaim_debug_error("winpidgin", "Error opening URI: %s error: %d\n",
				target, (int) wsinfo.hInstApp);

		g_free(w_uri);
		g_free(w_verb);
		g_free(w_clazz);
	} else {
		SHELLEXECUTEINFOA sinfo;
		gchar *locale_uri;

		locale_uri = g_locale_from_utf8(target, -1, NULL, NULL, NULL);

		memset(&sinfo, 0, sizeof(sinfo));
		sinfo.cbSize = sizeof(sinfo);
		sinfo.lpVerb = verb;
		sinfo.lpFile = locale_uri;
		sinfo.nShow = SW_SHOWNORMAL;
		if (clazz != NULL) {
			sinfo.fMask |= SEE_MASK_CLASSNAME;
			sinfo.lpClass = clazz;
		}

		if(!ShellExecuteExA(&sinfo))
			gaim_debug_error("winpidgin", "Error opening URI: %s error: %d\n",
				target, (int) sinfo.hInstApp);

		g_free(locale_uri);
	}

}

void winpidgin_notify_uri(const char *uri) {
	/* We'll allow whatever URI schemes are supported by the
	 * default http browser.
	 */
	winpidgin_shell_execute(uri, "open", "http");
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static LRESULT CALLBACK message_window_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	if (msg == PIDGIN_WM_FOCUS_REQUEST) {
		gaim_debug_info("winpidgin", "Got external Buddy List focus request.");
		gaim_blist_set_visible(TRUE);
		return TRUE;
	} else if (msg == PIDGIN_WM_PROTOCOL_HANDLE) {
		char *proto_msg = (char *) lparam;
		gaim_debug_info("winpidgin", "Got protocol handler request: %s\n", proto_msg ? proto_msg : "");
		gaim_got_protocol_handler_uri(proto_msg);
		return TRUE;
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
	wcx.hInstance = winpidgin_hinstance();
	wcx.hIcon = NULL;
	wcx.hCursor = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = wname;
	wcx.hIconSm = NULL;

	RegisterClassEx(&wcx);

	/* Create the window */
	if(!(win_hwnd = CreateWindow(wname, TEXT("WinpidginMsgWin"), 0, 0, 0, 0, 0,
			NULL, NULL, winpidgin_hinstance(), 0))) {
		gaim_debug_error("winpidgin",
			"Unable to create message window.\n");
		return NULL;
	}

	return win_hwnd;
}

static gboolean stop_flashing(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
	GtkWindow *window = data;
	winpidgin_window_flash(window, FALSE);
	return FALSE;
}

void
winpidgin_window_flash(GtkWindow *window, gboolean flash) {
	GdkWindow * gdkwin;

	g_return_if_fail(window != NULL);

	gdkwin = GTK_WIDGET(window)->window;

	g_return_if_fail(GDK_IS_WINDOW(gdkwin));
	g_return_if_fail(GDK_WINDOW_TYPE(gdkwin) != GDK_WINDOW_CHILD);

	if(GDK_WINDOW_DESTROYED(gdkwin))
	    return;

	if(MyFlashWindowEx) {
		FLASHWINFO info;

		memset(&info, 0, sizeof(FLASHWINFO));
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = GDK_WINDOW_HWND(gdkwin);
		if (flash)
			info.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
		else
			info.dwFlags = FLASHW_STOP;
		info.dwTimeout = 0;
		info.dwTimeout = 0;

		MyFlashWindowEx(&info);
	} else
		FlashWindow(GDK_WINDOW_HWND(gdkwin), flash);
}

void
winpidgin_conv_blink(GaimConversation *conv, GaimMessageFlags flags) {
	PidginWindow *win;
	GtkWindow *window;

	/* Don't flash for our own messages or system messages */
	if(flags & GAIM_MESSAGE_SEND || flags & GAIM_MESSAGE_SYSTEM)
		return;

	if(conv == NULL) {
		gaim_debug_info("winpidgin", "No conversation found to blink.\n");
		return;
	}

	win = pidgin_conv_get_window(PIDGIN_CONVERSATION(conv));
	if(win == NULL) {
		gaim_debug_info("winpidgin", "No conversation windows found to blink.\n");
		return;
	}
	window = GTK_WINDOW(win->window);

	winpidgin_window_flash(window, TRUE);
	/* Stop flashing when window receives focus */
	g_signal_connect(G_OBJECT(window), "focus-in-event",
					 G_CALLBACK(stop_flashing), window);
}

static gboolean
winpidgin_conv_im_blink(GaimAccount *account, const char *who, char **message,
		GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	if (gaim_prefs_get_bool("/gaim/gtk/win32/blink_im"))
		winpidgin_conv_blink(conv, flags);
	return FALSE;
}

void winpidgin_init(HINSTANCE hint) {

	gaim_debug_info("winpidgin", "winpidgin_init start\n");

	exe_hInstance = hint;

	/* IdleTracker Initialization */
	if(!winpidgin_set_idlehooks())
		gaim_debug_error("winpidgin", "Failed to initialize idle tracker\n");

	winpidgin_spell_init();
	gaim_debug_info("winpidgin", "GTK+ :%u.%u.%u\n",
		gtk_major_version, gtk_minor_version, gtk_micro_version);

	messagewin_hwnd = winpidgin_message_window_init();

	MyFlashWindowEx = (LPFNFLASHWINDOWEX) wgaim_find_and_loadproc("user32.dll", "FlashWindowEx");

	gaim_debug_info("winpidgin", "winpidgin_init end\n");
}

void winpidgin_post_init(void) {

	gaim_prefs_add_none("/gaim/gtk/win32");
	gaim_prefs_add_bool("/gaim/gtk/win32/blink_im", TRUE);

	gaim_signal_connect(pidgin_conversations_get_handle(),
		"displaying-im-msg", &gtkwin32_handle, GAIM_CALLBACK(winpidgin_conv_im_blink),
		NULL);

}

/* Windows Cleanup */

void winpidgin_cleanup(void) {
	gaim_debug_info("winpidgin", "winpidgin_cleanup\n");

	if(messagewin_hwnd)
		DestroyWindow(messagewin_hwnd);

	/* Idle tracker cleanup */
	winpidgin_remove_idlehooks();

}

/* DLL initializer */
/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	dll_hInstance = hinstDLL;
	return TRUE;
}

typedef HMONITOR WINAPI _MonitorFromWindow(HWND, DWORD);
typedef BOOL WINAPI _GetMonitorInfo(HMONITOR, LPMONITORINFO);

static gboolean
get_WorkingAreaRectForWindow(HWND hwnd, RECT *workingAreaRc) {
	static _MonitorFromWindow *the_MonitorFromWindow;
	static _GetMonitorInfo *the_GetMonitorInfo;
	static gboolean initialized = FALSE;

	HMONITOR monitor;
	MONITORINFO info;

	if(!initialized) {
		the_MonitorFromWindow = (_MonitorFromWindow*)
			wgaim_find_and_loadproc("user32", "MonitorFromWindow");
		the_GetMonitorInfo = (_GetMonitorInfo*)
			wgaim_find_and_loadproc("user32", "GetMonitorInfoA");
		initialized = TRUE;
	}

	if(!the_MonitorFromWindow)
		return FALSE;

	if(!the_GetMonitorInfo)
		return FALSE;

	monitor = the_MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

	info.cbSize = sizeof(info);
	if(!the_GetMonitorInfo(monitor, &info))
		return FALSE;

	CopyRect(workingAreaRc, &(info.rcWork));
	return TRUE;
}

void winpidgin_ensure_onscreen(GtkWidget *win) {
	RECT windowRect, workingAreaRect, intersectionRect;
	HWND hwnd = GDK_WINDOW_HWND(win->window);

	g_return_if_fail(hwnd != NULL);
	GetWindowRect(hwnd, &windowRect);

	gaim_debug_info("win32placement",
			"Window RECT: L:%ld R:%ld T:%ld B:%ld\n",
			windowRect.left, windowRect.right,
			windowRect.top, windowRect.bottom);

	if(!get_WorkingAreaRectForWindow(hwnd, &workingAreaRect)) {
		gaim_debug_info("win32placement",
				"Couldn't get multimonitor working area\n");
		if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &workingAreaRect, FALSE)) {
			/* I don't think this will ever happen */
			workingAreaRect.left = 0;
			workingAreaRect.top = 0;
			workingAreaRect.bottom = GetSystemMetrics(SM_CYSCREEN);
			workingAreaRect.right = GetSystemMetrics(SM_CXSCREEN);
		}
	}

	gaim_debug_info("win32placement",
			"Working Area RECT: L:%ld R:%ld T:%ld B:%ld\n",
			workingAreaRect.left, workingAreaRect.right,
			workingAreaRect.top, workingAreaRect.bottom);

	/** If the conversation window doesn't intersect perfectly with the working area,
	 *  move it to the top left corner of the working area */
	if(!(IntersectRect(&intersectionRect, &windowRect, &workingAreaRect)
				&& EqualRect(&intersectionRect, &windowRect))) {
		gaim_debug_info("win32placement",
				"conversation window out of working area, relocating\n");
		MoveWindow(hwnd, workingAreaRect.left, workingAreaRect.top,
				(windowRect.right - windowRect.left),
				(windowRect.bottom - windowRect.top), TRUE);
	}
}

