/*
 * purple - WinPurple Options Plugin
 *
 * File: gtkappbar.c
 * Date: August 2, 2003
 * Description: Appbar functionality for Windows GTK+ applications
 *
 * Copyright (C) 2003, Herman Bloggs <hermanator12002@yahoo.com>
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
/*
 *  TODO:
 *  - Move 'App on top' feature from Trans plugin to here
 *  - Bug: Multiple Show/Hide Desktop calls causes client area to disappear
 */
#include <windows.h>
#include <winver.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>
#include "gtkappbar.h"
#include "debug.h"

#define APPBAR_CALLBACK 	WM_USER + 1010

typedef HMONITOR WINAPI purple_MonitorFromPoint(POINT, DWORD);
typedef HMONITOR WINAPI purple_MonitorFromWindow(HWND, DWORD);
typedef BOOL WINAPI purple_GetMonitorInfo(HMONITOR, LPMONITORINFO);

static void gtk_appbar_do_dock(GtkAppBar *ab, UINT side);

/* Retrieve the rectangular display area from the specified monitor
 * Return TRUE if successful, otherwise FALSE
 */
static gboolean
get_rect_from_monitor(HMODULE hmod, HMONITOR monitor, RECT *rect) {
	purple_GetMonitorInfo *the_GetMonitorInfo;
	MONITORINFO info;

	if (!(the_GetMonitorInfo = (purple_GetMonitorInfo*)
		GetProcAddress(hmod, "GetMonitorInfoA"))) {
		return FALSE;
	}

	info.cbSize = sizeof(info);
	if (!the_GetMonitorInfo(monitor, &info)) {
		return FALSE;
	}

	CopyRect(rect, &(info.rcMonitor));

	return TRUE;
}

/**
 * This will only work on Win98+ and Win2K+
 * Return TRUE if successful, otherwise FALSE
 */
static gboolean
get_rect_at_point_multimonitor(POINT pt, RECT *rect) {
	HMODULE hmod;
	purple_MonitorFromPoint *the_MonitorFromPoint;
	HMONITOR monitor;

	if (!(hmod = GetModuleHandle("user32"))) {
		return FALSE;
	}

	if (!(the_MonitorFromPoint = (purple_MonitorFromPoint*)
		GetProcAddress(hmod, "MonitorFromPoint"))) {
		return FALSE;
	}

	monitor =
		the_MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);

	return get_rect_from_monitor(hmod, monitor, rect);
}

/**
 * This will only work on Win98+ and Win2K+
 * Return TRUE if successful, otherwise FALSE
 */
static gboolean
get_rect_of_window_multimonitor(HWND window, RECT *rect) {
	HMODULE hmod;
	purple_MonitorFromWindow *the_MonitorFromWindow;
	HMONITOR monitor;

	if (!(hmod = GetModuleHandle("user32"))) {
		return FALSE;
	}

	if (!(the_MonitorFromWindow = (purple_MonitorFromWindow*)
		GetProcAddress(hmod, "MonitorFromWindow"))) {
		return FALSE;
	}

	monitor =
		the_MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

	return get_rect_from_monitor(hmod, monitor, rect);
}

/*
 * Fallback if cannot get the RECT from the monitor directly
 */
static void get_default_workarea(RECT *rect) {
	if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, FALSE)) {
		/* I don't think this will ever happen */
		rect->left = 0;
		rect->top = 0;
		rect->bottom = GetSystemMetrics(SM_CYSCREEN);
		rect->right = GetSystemMetrics(SM_CXSCREEN);
	}
}

/* Retrieve the rectangle of the active work area at a point */
static void get_rect_at_point(POINT pt, RECT *rc) {
	if (!get_rect_at_point_multimonitor(pt, rc)) {
		get_default_workarea(rc);
	}
}

/* Retrieve the rectangle of the active work area of a window*/
static void get_rect_of_window(HWND window, RECT *rc) {
	if (!get_rect_of_window_multimonitor(window, rc)) {
		get_default_workarea(rc);
	}
}

static void get_window_normal_rc(HWND hwnd, RECT *rc) {
         WINDOWPLACEMENT wplc;
         GetWindowPlacement(hwnd, &wplc);
         CopyRect(rc, &wplc.rcNormalPosition);
}
#if 0
static void print_rect(RECT *rc) {
        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "RECT: L:%ld R:%ld T:%ld B:%ld\n",
                   rc->left, rc->right, rc->top, rc->bottom);
}
#endif
/** Set the window style to be the "Tool Window" style - small header, no min/max buttons */
static void set_toolbar(HWND hwnd, gboolean val) {
	LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);

        if(val && !(style & WS_EX_TOOLWINDOW))
                style |= WS_EX_TOOLWINDOW;
        else if(!val && style & WS_EX_TOOLWINDOW)
                style &= ~WS_EX_TOOLWINDOW;
        else
                return;
        SetWindowLong(hwnd, GWL_EXSTYLE, style);
	SetWindowPos(hwnd, 0, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
/*	This really should be the following, but SWP_FRAMECHANGED strangely causes initermittent problems "Show Desktop" done more than once.
 *	Not having SWP_FRAMECHANGED *should* cause the Style not to be applied, but i haven't noticed any problems
 *			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
 */
}
/** Register the window as an appbar */
static gboolean gtk_appbar_register(GtkAppBar *ab, HWND hwnd) {
	APPBARDATA abd;

	abd.cbSize = sizeof(APPBARDATA);
	abd.hWnd = hwnd;
	abd.uCallbackMessage = APPBAR_CALLBACK;

	ab->registered = SHAppBarMessage(ABM_NEW, &abd);

	return ab->registered;
}
/** Unregister the window as an appbar */
static gboolean gtk_appbar_unregister(GtkAppBar *ab, HWND hwnd) {
	APPBARDATA abd;

        if(!ab->registered)
                return TRUE;

	abd.cbSize = sizeof(APPBARDATA);
	abd.hWnd = hwnd;

	SHAppBarMessage(ABM_REMOVE, &abd); /** This always returns TRUE */

	ab->registered = FALSE;

	ab->docked = FALSE;
	ab->undocking = FALSE;
	ab->docking = FALSE;

	return TRUE;
}

static void gtk_appbar_querypos(GtkAppBar *ab, HWND hwnd, RECT rcWorkspace) {
	APPBARDATA abd;
	guint iWidth = 0;

	if(!ab->registered)
		gtk_appbar_register(ab, hwnd);

	abd.hWnd = hwnd;
	abd.cbSize = sizeof(APPBARDATA);
	abd.uEdge = ab->side;

	iWidth = ab->docked_rect.right - ab->docked_rect.left;

	abd.rc.top = rcWorkspace.top;
	abd.rc.bottom = rcWorkspace.bottom;
	switch (abd.uEdge)
	{
		case ABE_LEFT:
			abd.rc.left = rcWorkspace.left;
			abd.rc.right = rcWorkspace.left + iWidth;
			break;

		case ABE_RIGHT:
			abd.rc.right = rcWorkspace.right;
			abd.rc.left = rcWorkspace.right - iWidth;
			break;
	}

        /* Ask the system for the screen space */
	SHAppBarMessage(ABM_QUERYPOS, &abd);

	switch (abd.uEdge)
	{
		case ABE_LEFT:
                        abd.rc.right = abd.rc.left + iWidth;
			break;

		case ABE_RIGHT:
			abd.rc.left = abd.rc.right - iWidth;
			break;
	}

	CopyRect(&(ab->docked_rect), &abd.rc);
}
/* Actually set the size and screen location of the appbar */
static void gtk_appbar_setpos(GtkAppBar *ab, HWND hwnd) {
        APPBARDATA abd;

        if(!ab->registered)
                gtk_appbar_register(ab, hwnd);

	abd.hWnd = hwnd;
	abd.cbSize = sizeof(APPBARDATA);
        CopyRect(&abd.rc, &(ab->docked_rect));
	abd.uEdge = ab->side;

	SHAppBarMessage(ABM_SETPOS, &abd);
}
/** Let any callbacks know that we have docked or undocked */
static void gtk_appbar_dispatch_dock_cbs(GtkAppBar *ab, gboolean val) {
        GSList *lst = ab->dock_cbs;

        while(lst) {
                GtkAppBarDockCB dock_cb = lst->data;
                dock_cb(val);
                lst = lst->next;
        }
}

static GdkFilterReturn wnd_moving(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;
        POINT cp;
        RECT *rc = (RECT*)msg->lParam;
	RECT monRect;
        int side = -1;
	long dockAreaWidth = 0;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_moving\n");

        GetCursorPos(&cp);
	get_rect_at_point(cp, &monRect);

	dockAreaWidth = (monRect.right - monRect.left) / 10;
        /* Which part of the screen are we in ? */
	if (cp.x > (monRect.right - dockAreaWidth)) {
                side = ABE_RIGHT;
	} else if (cp.x < (monRect.left + dockAreaWidth)) {
                side = ABE_LEFT;
	}

        if(!ab->docked) {
                if( (side == ABE_RIGHT || side == ABE_LEFT) ) {
                        if( !ab->docking ) {
                                ab->side = side;
                                GetWindowRect(msg->hwnd, &(ab->docked_rect));
				gtk_appbar_querypos(ab, msg->hwnd, monRect);

                                /* save pre-docking height */
                                ab->undocked_height = rc->bottom - rc->top;
                                ab->docking = TRUE;
                        }
                }
                else
                        ab->docking = FALSE;
        }
        else if(side < 0) {
                gtk_appbar_unregister(ab, msg->hwnd);
		ab->undocking = TRUE;
                rc->bottom = rc->top + ab->undocked_height;
        }

        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_sizing(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_sizing\n");
        if(ab->docked) {
                RECT *rc = (RECT*)msg->lParam;
                if(ab->side == ABE_LEFT && msg->wParam == WMSZ_RIGHT) {
                        ab->docked_rect.right = rc->right;
                        gtk_appbar_setpos(ab, msg->hwnd);
                }
                else if(ab->side == ABE_RIGHT && msg->wParam == WMSZ_LEFT) {
                        ab->docked_rect.left = rc->left;
                        gtk_appbar_setpos(ab, msg->hwnd);
                }
                return GDK_FILTER_REMOVE;
        }
        return GDK_FILTER_CONTINUE;
}
/** Notify the system that the appbar has been activated */
static GdkFilterReturn wnd_activate(GtkAppBar *ab, GdkXEvent *xevent) {
	if (ab->registered) {
		APPBARDATA abd;
		MSG *msg = (MSG*)xevent;
		purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_activate\n");

		abd.hWnd = msg->hwnd;
		abd.cbSize = sizeof(APPBARDATA);

		SHAppBarMessage(ABM_ACTIVATE, &abd);
	}
	return GDK_FILTER_CONTINUE;
}

static void show_hide(GtkAppBar *ab, gboolean hide) {
	purple_debug_info("gtkappbar", "show_hide(%d)\n", hide);

	if (hide) {
		purple_debug_info("gtkappbar", "hidden\n");
		gtk_appbar_unregister(ab, GDK_WINDOW_HWND(ab->win->window));
		ab->docked = TRUE;
		ab->iconized = TRUE;
	} else {
		ab->iconized = FALSE;
		purple_debug_info("gtkappbar", "shown\n");
		ab->docked = FALSE;
		gtk_appbar_do_dock(ab, ab->side);
	}

}

/** Notify the system that the appbar's position has changed */
static GdkFilterReturn wnd_poschanged(GtkAppBar *ab, GdkXEvent *xevent) {
	if (ab->registered) {
		APPBARDATA abd;
		MSG *msg = (MSG*)xevent;

		purple_debug(PURPLE_DEBUG_MISC, "gtkappbar", "wnd_poschanged\n");

		abd.hWnd = msg->hwnd;
		abd.cbSize = sizeof(APPBARDATA);

		SHAppBarMessage(ABM_WINDOWPOSCHANGED, &abd);

	}
	return GDK_FILTER_CONTINUE;
}
/** The window is about to change */
static GdkFilterReturn wnd_poschanging(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;
        WINDOWPOS *wpos = (WINDOWPOS*)msg->lParam;

        purple_debug(PURPLE_DEBUG_MISC, "gtkappbar", "wnd_poschanging\n");

        if(ab->docked || ab->docking) {
                wpos->x = ab->docked_rect.left;
                wpos->y = ab->docked_rect.top;
                wpos->cx = ab->docked_rect.right - ab->docked_rect.left;
                wpos->cy = ab->docked_rect.bottom - ab->docked_rect.top;
                if(IsIconic(msg->hwnd))
                        set_toolbar(msg->hwnd, FALSE);
                /*return GDK_FILTER_REMOVE;*/
        }

	if (ab->docked) {
		if (ab->iconized && wpos->flags & SWP_SHOWWINDOW)
			show_hide(ab, FALSE);
		else if (!ab->iconized && wpos->flags & SWP_HIDEWINDOW)
			show_hide(ab, TRUE);
	}

        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_exitsizemove(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_exitsizemove\n");
        if(ab->docking) {
		gtk_appbar_setpos(ab, msg->hwnd);
		ab->docking = FALSE;
		ab->docked = TRUE;
		ShowWindow(msg->hwnd, SW_HIDE);
		set_toolbar(msg->hwnd, TRUE);
		ShowWindow(msg->hwnd, SW_SHOW);
		gtk_appbar_dispatch_dock_cbs(ab, TRUE);
	} else if(ab->undocking) {
		ShowWindow(msg->hwnd, SW_HIDE);
		set_toolbar(msg->hwnd, FALSE);
		ShowWindow(msg->hwnd, SW_SHOW);
		gtk_appbar_dispatch_dock_cbs(ab, FALSE);
		ab->undocking = FALSE;
        }

        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_showwindow(GtkAppBar *ab, GdkXEvent *xevent) {
	MSG *msg = (MSG*)xevent;

	purple_debug_info("gtkappbar", "wnd_showwindow\n");
	if(msg->wParam && ab->docked) {
		show_hide(ab, FALSE);
	} else if(!msg->wParam && ab->docked) {
		show_hide(ab, TRUE);
	}
	return GDK_FILTER_CONTINUE;
}

/** The window's size has changed */
static GdkFilterReturn wnd_size(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_size\n");

        if(msg->wParam == SIZE_MINIMIZED) {
                purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "Minimize\n");
                if(ab->docked) {
                        gtk_appbar_unregister(ab, GDK_WINDOW_HWND(ab->win->window));
                        ab->docked = TRUE;
                }
        }
        else if(msg->wParam == SIZE_RESTORED) {
                purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "Restore\n");
		if (!ab->iconized && ab->docked) {
                        gtk_appbar_do_dock(ab, ab->side);
                }
        }
        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_nchittest(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        if(ab->docked) {
                UINT ret = DefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);

                switch(ret) {
                case HTBOTTOM:
                case HTBOTTOMLEFT:
                case HTBOTTOMRIGHT:
                case HTTOP:
                case HTTOPLEFT:
                case HTTOPRIGHT:
                        return GDK_FILTER_REMOVE;
                case HTLEFT:
                        if(ab->side == ABE_LEFT)
                                return GDK_FILTER_REMOVE;
                        break;
                case HTRIGHT:
                        if(ab->side == ABE_RIGHT)
                                return GDK_FILTER_REMOVE;
                        break;
                }
        }
        return GDK_FILTER_CONTINUE;
}

#if 0
static GdkFilterReturn wnd_initmenupopup(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        if(ab->docked && HIWORD(msg->lParam)) {
                HMENU sysmenu = GetSystemMenu(msg->hwnd, FALSE);
                purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "wnd_initpopupmenu: docked: %d ismenu: %d\n", ab->docked, IsMenu(sysmenu));
                if(EnableMenuItem(sysmenu, SC_MAXIMIZE, MF_BYCOMMAND|MF_GRAYED)<0)
                        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "SC_MAXIMIZE Menu item does not exist\n");
                if(EnableMenuItem(sysmenu, SC_MOVE, MF_BYCOMMAND|MF_GRAYED)<0)
                        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "SC_MOVE Menu item does not exist\n");
                return GDK_FILTER_CONTINUE;
        }
        else
                GetSystemMenu(msg->hwnd, TRUE);
        return GDK_FILTER_CONTINUE;
}
#endif

static GdkFilterReturn gtk_appbar_callback(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;
	RECT orig, windowRect;

        switch (msg->wParam) {
        case ABN_STATECHANGE:
                purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: ABN_STATECHANGE\n");
	        break;

        case ABN_FULLSCREENAPP:
                purple_debug(PURPLE_DEBUG_MISC, "gtkappbar", "gtk_appbar_callback: ABN_FULLSCREENAPP: %d\n", (BOOL)msg->lParam);
		if (!ab->iconized && ab->docked) {
		if ((BOOL)msg->lParam) {
			SetWindowPos(msg->hwnd, HWND_BOTTOM, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		} else {
			SetWindowPos(msg->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
		}

                break;
	case ABN_POSCHANGED:
		purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: ABN_POSCHANGED\n");
		CopyRect(&orig, &(ab->docked_rect));
		get_rect_of_window(msg->hwnd, &windowRect);
		gtk_appbar_querypos(ab, msg->hwnd, windowRect);
		if (EqualRect(&orig, &(ab->docked_rect)) == 0) {
			MoveWindow(msg->hwnd, ab->docked_rect.left, ab->docked_rect.top,
				ab->docked_rect.right - ab->docked_rect.left,
				ab->docked_rect.bottom - ab->docked_rect.top, TRUE);
		}
                gtk_appbar_setpos(ab, msg->hwnd);
		break;
#if 0
	default:
		purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: %d\n", msg->wParam);
#endif
        }
        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn gtk_appbar_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
	MSG *msg = (MSG*)xevent;

        /*printf("MSG: %s\n", message_to_string (msg->message));*/
        switch(msg->message) {
        case WM_EXITSIZEMOVE:
                return wnd_exitsizemove(data, xevent);
        case WM_WINDOWPOSCHANGING:
                return wnd_poschanging(data, xevent);
	case WM_WINDOWPOSCHANGED:
		return wnd_poschanged(data, xevent);
	case WM_ACTIVATE:
		return wnd_activate(data, xevent);
        case WM_SIZING:
                return wnd_sizing(data, xevent);
        case WM_MOVING:
                return wnd_moving(data, xevent);
        case WM_SHOWWINDOW:
                return wnd_showwindow(data, xevent);
        case WM_NCHITTEST:
                return wnd_nchittest(data, xevent);
#if 0
        case WM_INITMENUPOPUP:
                return wnd_initmenupopup(data, xevent);
#endif
        case WM_SIZE:
                return wnd_size(data, xevent);
        case APPBAR_CALLBACK:
                return gtk_appbar_callback(data, xevent);
#if 0
	default:
		purple_debug_info("gtkappbar", "gtk_appbar_event_filter %d\n", msg->message);
#endif
        }
        return GDK_FILTER_CONTINUE;
}

static void gtk_appbar_do_dock(GtkAppBar *ab, UINT side) {
	RECT orig, windowRect;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_do_dock\n");

        if(!ab || !IsWindow(GDK_WINDOW_HWND(ab->win->window)))
                return;

        ab->side = side;
        get_window_normal_rc(GDK_WINDOW_HWND(ab->win->window), &(ab->docked_rect));
        CopyRect(&orig, &(ab->docked_rect));
	get_rect_of_window(GDK_WINDOW_HWND(ab->win->window), &windowRect);
	gtk_appbar_querypos(ab, GDK_WINDOW_HWND(ab->win->window), windowRect);
        if(EqualRect(&orig, &(ab->docked_rect)) == 0)
                MoveWindow(GDK_WINDOW_HWND(ab->win->window),
                           ab->docked_rect.left,
                           ab->docked_rect.top,
                           ab->docked_rect.right - ab->docked_rect.left,
                           ab->docked_rect.bottom - ab->docked_rect.top, TRUE);
        gtk_appbar_setpos(ab, GDK_WINDOW_HWND(ab->win->window));
        ab->docked = TRUE;
}

void gtk_appbar_dock(GtkAppBar *ab, UINT side) {
	HWND hwnd;

	g_return_if_fail(ab != NULL);

	hwnd = GDK_WINDOW_HWND(ab->win->window);

	g_return_if_fail(IsWindow(hwnd));

	ab->iconized = IsIconic(hwnd);

	if (!ab->docked && !ab->iconized)
		ShowWindow(hwnd, SW_HIDE);
	gtk_appbar_do_dock(ab, side);
	set_toolbar(hwnd, TRUE);
	if (!ab->iconized)
		ShowWindow(hwnd, SW_SHOW);
}

void gtk_appbar_add_dock_cb(GtkAppBar *ab, GtkAppBarDockCB dock_cb) {
        if(!ab)
                return;
        ab->dock_cbs = g_slist_prepend(ab->dock_cbs, dock_cb);
}

GtkAppBar *gtk_appbar_add(GtkWidget *win) {
        GtkAppBar *ab;

        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_add\n");

        if(!win)
                return NULL;
        ab = g_new0(GtkAppBar, 1);
        ab->win = win;

        /* init docking coords */
        get_window_normal_rc(GDK_WINDOW_HWND(win->window), &(ab->docked_rect));

        /* Add main window filter */
	gdk_window_add_filter(win->window,
                              gtk_appbar_event_filter,
                              ab);
        return ab;
}

void gtk_appbar_remove(GtkAppBar *ab) {
	HWND hwnd;
        purple_debug(PURPLE_DEBUG_INFO, "gtkappbar", "gtk_appbar_remove\n");

        if(!ab)
                return;

	hwnd = GDK_WINDOW_HWND(ab->win->window);
        gdk_window_remove_filter(ab->win->window,
                                 gtk_appbar_event_filter,
                                 ab);
        if(ab->docked) {
                gtk_window_resize(GTK_WINDOW(ab->win),
                                  ab->docked_rect.right - ab->docked_rect.left,
                                  ab->undocked_height);
		if (!ab->iconized)
			ShowWindow(hwnd, SW_HIDE);
		set_toolbar(hwnd, FALSE);
		if (!ab->iconized)
			ShowWindow(hwnd, SW_SHOW);
        }
        gtk_appbar_unregister(ab, hwnd);

	while (ab->dock_cbs)
		ab->dock_cbs = g_slist_remove(ab->dock_cbs, ab->dock_cbs->data);

	g_free(ab);
}
