/*
 * gaim
 *
 * File: gtkwin32dep.c
 * Date: June, 2002
 * Description: Windows dependant code for Gaim
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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

#include "gaim.h"
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
#include "wspell.h"

/*
 *  GLOBALS
 */
HINSTANCE gaimexe_hInstance = 0;
HINSTANCE gtkgaimdll_hInstance = 0;
HWND messagewin_hwnd;
static int gtkwin32_handle;

typedef BOOL (CALLBACK* LPFNFLASHWINDOWEX)(PFLASHWINFO);
static LPFNFLASHWINDOWEX MyFlashWindowEx = NULL;


/*
 *  PUBLIC CODE
 */

HINSTANCE gtkwgaim_hinstance(void) {
	return gaimexe_hInstance;
}

int gtkwgaim_gz_decompress(const char* in, const char* out) {
	gzFile fin;
	FILE *fout;
	char buf[1024];
	int ret;

	if((fin = gzopen(in, "rb"))) {
		if(!(fout = g_fopen(out, "wb"))) {
			gaim_debug_error("gtkwgaim_gz_decompress", "Error opening file: %s\n", out);
			gzclose(fin);
			return 0;
		}
	}
	else {
		gaim_debug_error("gtkwgaim_gz_decompress", "gzopen failed to open: %s\n", in);
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
		gaim_debug_error("gtkwgaim_gz_decompress", "gzread failed while reading: %s\n", in);
		return 0;
	}

	return 1;
}

int gtkwgaim_gz_untar(const char* filename, const char* destdir) {
	char tmpfile[_MAX_PATH];
	char template[]="wgaimXXXXXX";

	sprintf(tmpfile, "%s%s%s", g_get_tmp_dir(), G_DIR_SEPARATOR_S, _mktemp(template));
	if(gtkwgaim_gz_decompress(filename, tmpfile)) {
		int ret;
		if(untar(tmpfile, destdir, UNTAR_FORCE | UNTAR_QUIET))
			ret = 1;
		else {
			gaim_debug_error("gtkwgaim_gz_untar", "Failure untaring %s\n", tmpfile);
			ret = 0;
		}
		g_unlink(tmpfile);
		return ret;
	}
	else {
		gaim_debug_error("gtkwgaim_gz_untar", "Failed to gz decompress %s\n", filename);
		return 0;
	}
}

void gtkwgaim_notify_uri(const char *uri) {

	/* We'll allow whatever URI schemes are supported by the
	 * default http browser.
	 */

	if (G_WIN32_HAVE_WIDECHAR_API()) {
		SHELLEXECUTEINFOW wsinfo;
		wchar_t *w_uri;

		w_uri = g_utf8_to_utf16(uri, -1, NULL, NULL, NULL);

		memset(&wsinfo, 0, sizeof(wsinfo));
		wsinfo.cbSize = sizeof(wsinfo);
		wsinfo.fMask = SEE_MASK_CLASSNAME;
		wsinfo.lpVerb = L"open";
		wsinfo.lpFile = w_uri;
		wsinfo.nShow = SW_SHOWNORMAL;
		wsinfo.lpClass = L"http";

		if(!ShellExecuteExW(&wsinfo))
			gaim_debug_error("gtkwgaim", "Error opening URI: %s error: %d\n",
				uri, (int) wsinfo.hInstApp);

		g_free(w_uri);
	} else {
		SHELLEXECUTEINFOA sinfo;
		gchar *locale_uri;

		locale_uri = g_locale_from_utf8(uri, -1, NULL, NULL, NULL);

		memset(&sinfo, 0, sizeof(sinfo));
		sinfo.cbSize = sizeof(sinfo);
		sinfo.fMask = SEE_MASK_CLASSNAME;
		sinfo.lpVerb = "open";
		sinfo.lpFile = locale_uri;
		sinfo.nShow = SW_SHOWNORMAL;
		sinfo.lpClass = "http";

		if(!ShellExecuteExA(&sinfo))
			gaim_debug_error("gtkwgaim", "Error opening URI: %s error: %d\n",
				uri, (int) sinfo.hInstApp);

		g_free(locale_uri);
	}
}

#define WM_FOCUS_REQUEST (WM_APP + 13)

static LRESULT CALLBACK message_window_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	if (msg == WM_FOCUS_REQUEST) {
		gaim_debug_info("gtkwgaim", "Got external Buddy List focus request.");
		gaim_blist_set_visible(TRUE);
		return TRUE;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND wgaim_message_window_init(void) {
	HWND win_hwnd;
	WNDCLASSEX wcx;
	LPCTSTR wname;

	wname = TEXT("WingaimMsgWinCls");

	wcx.cbSize = sizeof(wcx);
	wcx.style = 0;
	wcx.lpfnWndProc = message_window_handler;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = gtkwgaim_hinstance();
	wcx.hIcon = NULL;
	wcx.hCursor = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = wname;
	wcx.hIconSm = NULL;

	RegisterClassEx(&wcx);

	/* Create the window */
	if(!(win_hwnd = CreateWindow(wname, TEXT("WingaimMsgWin"), 0, 0, 0, 0, 0,
			NULL, NULL, gtkwgaim_hinstance(), 0))) {
		gaim_debug_error("gtkwgaim",
			"Unable to create message window.\n");
		return NULL;
	}

	return win_hwnd;
}

static int
halt_flash_filter(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
	if(MyFlashWindowEx) {
		HWND hWnd = data;
		FLASHWINFO info;

		if(!IsWindow(hWnd))
			return 0;

		memset(&info, 0, sizeof(FLASHWINFO));
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = hWnd;
		info.dwFlags = FLASHW_STOP;
		info.dwTimeout = 0;
		MyFlashWindowEx(&info);
	}
	return 0;
}

void
gtkwgaim_conv_blink(GaimConversation *conv, GaimMessageFlags flags) {
	GaimGtkWindow *win;
	GtkWidget *window;

	/* Don't flash for our own messages or system messages */
	if(flags & GAIM_MESSAGE_SEND || flags & GAIM_MESSAGE_SYSTEM)
		return;

	if(conv == NULL) {
		gaim_debug_info("gtkwgaim", "No conversation found to blink.\n");
		return;
	}

	win = gaim_gtkconv_get_window(GAIM_GTK_CONVERSATION(conv));
	if(win == NULL) {
		gaim_debug_info("gtkwgaim", "No conversation windows found to blink.\n");
		return;
	}

	window = win->window;

	if(MyFlashWindowEx) {
		FLASHWINFO info;
		/* Don't flash if we have the window focused */
		if(GetForegroundWindow() == GDK_WINDOW_HWND(window->window))
			return;

		memset(&info, 0, sizeof(FLASHWINFO));
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = GDK_WINDOW_HWND(window->window);
		info.dwFlags = FLASHW_ALL | FLASHW_TIMER;
		info.dwTimeout = 0;
		MyFlashWindowEx(&info);
		/* Stop flashing when window receives focus */
		g_signal_connect(G_OBJECT(window), "focus-in-event",
			G_CALLBACK(halt_flash_filter), info.hwnd);
	}
}

static gboolean
gtkwgaim_conv_im_blink(GaimAccount *account, const char *who, char **message,
		GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gtkwgaim_conv_blink(conv, flags);
	return FALSE;
}

void gtkwgaim_init(HINSTANCE hint) {

	gaim_debug_info("gtkwgaim", "gtkwgaim_init start\n");

	gaimexe_hInstance = hint;

	/* IdleTracker Initialization */
	if(!wgaim_set_idlehooks())
		gaim_debug_error("gtkwgaim", "Failed to initialize idle tracker\n");

	wgaim_gtkspell_init();
	gaim_debug_info("gtkwgaim", "GTK+ :%u.%u.%u\n",
		gtk_major_version, gtk_minor_version, gtk_micro_version);

	messagewin_hwnd = wgaim_message_window_init();

	MyFlashWindowEx = (LPFNFLASHWINDOWEX) wgaim_find_and_loadproc("user32.dll", "FlashWindowEx");

	gaim_debug_info("gtkwgaim", "gtkwgaim_init end\n");
}

void gtkwgaim_post_init(void) {

	gaim_signal_connect(gaim_gtk_conversations_get_handle(),
		"displaying-im-msg", &gtkwin32_handle, GAIM_CALLBACK(gtkwgaim_conv_im_blink),
		NULL);

}

/* Windows Cleanup */

void gtkwgaim_cleanup(void) {
	gaim_debug_info("gtkwgaim", "gtkwgaim_cleanup\n");

	if(messagewin_hwnd)
		DestroyWindow(messagewin_hwnd);

	/* Idle tracker cleanup */
	wgaim_remove_idlehooks();

}

/* DLL initializer */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	gtkgaimdll_hInstance = hinstDLL;
	return TRUE;
}

typedef HMONITOR WINAPI gaim_MonitorFromWindow(HWND, DWORD);
typedef BOOL WINAPI gaim_GetMonitorInfo(HMONITOR, LPMONITORINFO);

static gboolean
get_WorkingAreaRectForWindow(HWND hwnd, RECT *workingAreaRc) {
	static gaim_MonitorFromWindow *the_MonitorFromWindow;
	static gaim_GetMonitorInfo *the_GetMonitorInfo;
	static gboolean initialized = FALSE;

	HMONITOR monitor;
	MONITORINFO info;

	if(!initialized) {
		the_MonitorFromWindow = (gaim_MonitorFromWindow*)
			wgaim_find_and_loadproc("user32", "MonitorFromWindow");
		the_GetMonitorInfo = (gaim_GetMonitorInfo*)
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

void gtkwgaim_ensure_onscreen(GtkWidget *win) {
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

