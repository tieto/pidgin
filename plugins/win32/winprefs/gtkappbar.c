/*
 * gaim - WinGaim Options Plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 *  TODO:
 *  - Move 'App on top' feature from Trans plugin to here
 *  - Bug: Multiple Show/Hide Desktop calls causes client area to disapear
 */
#include <windows.h>
#include <winver.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>
#include "gtkappbar.h"
#include "debug.h"

#define APPBAR_CALLBACK 	WM_USER + 1010

static void get_window_normal_rc(HWND hwnd, RECT *rc) {
         WINDOWPLACEMENT wplc;
         GetWindowPlacement(hwnd, &wplc);
         CopyRect(rc, &wplc.rcNormalPosition);
}

static void print_rect(RECT *rc) {
        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "RECT: L:%ld R:%ld T:%ld B:%ld\n",
                   rc->left, rc->right, rc->top, rc->bottom);
}

static void set_toolbar(HWND hwnd, gboolean val) {
	LONG style=0;

        style = GetWindowLong(hwnd, GWL_EXSTYLE);
        if(val && !(style & WS_EX_TOOLWINDOW))
                style |= WS_EX_TOOLWINDOW;
        else if(!val && style & WS_EX_TOOLWINDOW)
                style &= ~WS_EX_TOOLWINDOW;
        else
                return;
        SetWindowLong(hwnd, GWL_EXSTYLE, style);
        SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static gboolean gtk_appbar_register(GtkAppBar *ab, HWND hwnd) {
	APPBARDATA abd;

	abd.cbSize = sizeof(APPBARDATA);
	abd.hWnd = hwnd;
	abd.uCallbackMessage = APPBAR_CALLBACK;

	ab->registered = SHAppBarMessage(ABM_NEW, &abd);

	return ab->registered;
}

static gboolean gtk_appbar_unregister(GtkAppBar *ab, HWND hwnd) {
	APPBARDATA abd;

        if(!ab->registered)
                return TRUE;

	abd.cbSize = sizeof(APPBARDATA);
	abd.hWnd = hwnd;

	ab->registered = !SHAppBarMessage(ABM_REMOVE, &abd);

        if(!ab->registered) {
                ab->docked = FALSE;
                ab->docking = FALSE;
        }
	return !ab->registered;
}

static void gtk_appbar_querypos(GtkAppBar *ab, HWND hwnd, RECT *rc) {
	APPBARDATA abd;
	int iWidth = 0;

        if(!ab->registered)
                gtk_appbar_register(ab, hwnd);

	abd.hWnd = hwnd;
	abd.cbSize = sizeof(APPBARDATA);
        CopyRect(&abd.rc, rc);
	abd.uEdge = ab->side;

        iWidth = abd.rc.right - abd.rc.left;
        
        abd.rc.top = 0;
        abd.rc.bottom = GetSystemMetrics(SM_CYSCREEN);
	switch (abd.uEdge)
	{
		case ABE_LEFT:
                        abd.rc.left = 0;
			abd.rc.right = iWidth;
			break;

		case ABE_RIGHT:
                        abd.rc.right = GetSystemMetrics(SM_CXSCREEN);
			abd.rc.left = abd.rc.right - iWidth;
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

	CopyRect(rc, &abd.rc);
}

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

static GdkFilterReturn wnd_moving(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;
        POINT cp;
        LONG cxScreen;
        RECT *rc = (RECT*)msg->lParam;
        int side = -1;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_moving\n");

        cxScreen = GetSystemMetrics(SM_CXSCREEN);
        GetCursorPos(&cp);

        /* Which part of the screen are we in ? */
        if( cp.x > (cxScreen - (cxScreen / 10)) )
                side = ABE_RIGHT;
        else if( cp.x < (cxScreen / 10) )
                side = ABE_LEFT;

        if(!ab->docked) {
                if( (side == ABE_RIGHT || side == ABE_LEFT) ) {
                        if( !ab->docking ) {
                                ab->side = side;
                                GetWindowRect(msg->hwnd, &(ab->docked_rect));
                                gtk_appbar_querypos(ab, msg->hwnd, &(ab->docked_rect));

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
                rc->bottom = rc->top + ab->undocked_height;
        }

        /* Switch to toolbar/regular caption*/ 
        if(ab->docking)
                set_toolbar(msg->hwnd, TRUE);
        else if(!ab->docked)
                set_toolbar(msg->hwnd, FALSE);

        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_sizing(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_sizing\n");
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

static GdkFilterReturn wnd_poschanging(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;
        WINDOWPOS *wpos = (WINDOWPOS*)msg->lParam;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_poschanging\n");

        if(ab->docked || ab->docking) {
                wpos->x = ab->docked_rect.left;
                wpos->y = ab->docked_rect.top;
                wpos->cx = ab->docked_rect.right - ab->docked_rect.left;
                wpos->cy = ab->docked_rect.bottom - ab->docked_rect.top;
                if(IsIconic(msg->hwnd))
                        set_toolbar(msg->hwnd, FALSE);
                /*return GDK_FILTER_REMOVE;*/
        }
        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_exitsizemove(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        if(ab->docking) {
                gtk_appbar_setpos(ab, msg->hwnd);
                ab->docking = FALSE;
                ab->docked = TRUE;
        }
        else if(!ab->docked) {
                gtk_appbar_unregister(ab, msg->hwnd);
        }

        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_showwindow(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_showwindow\n");
        if(msg->wParam && ab->docked) {
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "shown\n");
                ab->docked = FALSE;
                gtk_appbar_dock(ab, ab->side);
                
        }
        else if(!msg->wParam && ab->docked) {
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "hidden\n");
                gtk_appbar_unregister(ab, GDK_WINDOW_HWND(ab->win->window));
                set_toolbar(GDK_WINDOW_HWND(ab->win->window), FALSE);
                ab->docked = TRUE;
        }
        return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn wnd_size(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_size\n");

        if(msg->wParam == SIZE_MINIMIZED) {
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "Minimize\n");
                if(ab->docked) {
                        gtk_appbar_unregister(ab, GDK_WINDOW_HWND(ab->win->window));
                        ab->docked = TRUE;
                }
        }
        else if(msg->wParam == SIZE_RESTORED) {
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "Restore\n");
                if(ab->docked) {
                        gtk_appbar_dock(ab, ab->side);
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
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "wnd_initpopupmenu: docked: %d ismenu: %d\n", ab->docked, IsMenu(sysmenu));
                if(EnableMenuItem(sysmenu, SC_MAXIMIZE, MF_BYCOMMAND|MF_GRAYED)<0)
                        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "SC_MAXIMIZE Menu item does not exist\n");
                if(EnableMenuItem(sysmenu, SC_MOVE, MF_BYCOMMAND|MF_GRAYED)<0)
                        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "SC_MOVE Menu item does not exist\n");
                return GDK_FILTER_CONTINUE;
        }
        else
                GetSystemMenu(msg->hwnd, TRUE);
        return GDK_FILTER_CONTINUE;
}
#endif

static GdkFilterReturn gtk_appbar_callback(GtkAppBar *ab, GdkXEvent *xevent) {
        MSG *msg = (MSG*)xevent;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback\n");
        switch (msg->wParam) {
        case ABN_STATECHANGE:
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: ABN_STATECHANGE\n");
	        break;

        case ABN_FULLSCREENAPP:
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: ABN_FULLSCREENAPP: %d\n", (BOOL)msg->lParam);
                break;
        
    	case ABN_POSCHANGED:
                gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_callback: ABN_POSCHANGED\n");
                gtk_appbar_querypos(ab, msg->hwnd, &(ab->docked_rect));
                MoveWindow(msg->hwnd, ab->docked_rect.left, ab->docked_rect.top, 
                           ab->docked_rect.right - ab->docked_rect.left,
                           ab->docked_rect.bottom - ab->docked_rect.top, TRUE);
                gtk_appbar_setpos(ab, msg->hwnd);
        	break;
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
        default:
        }
        return GDK_FILTER_CONTINUE;
}

void gtk_appbar_dock(GtkAppBar *ab, UINT side) {
        RECT orig;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_dock\n");

        if(!ab || !IsWindow(GDK_WINDOW_HWND(ab->win->window)))
                return;

        ab->side = side;
        get_window_normal_rc(GDK_WINDOW_HWND(ab->win->window), &(ab->docked_rect));
        CopyRect(&orig, &(ab->docked_rect));
        print_rect(&(ab->docked_rect));
        gtk_appbar_querypos(ab, GDK_WINDOW_HWND(ab->win->window), &(ab->docked_rect));
        if(EqualRect(&orig, &(ab->docked_rect)) == 0)
                MoveWindow(GDK_WINDOW_HWND(ab->win->window),
                           ab->docked_rect.left,
                           ab->docked_rect.top, 
                           ab->docked_rect.right - ab->docked_rect.left,
                           ab->docked_rect.bottom - ab->docked_rect.top, TRUE);
        gtk_appbar_setpos(ab, GDK_WINDOW_HWND(ab->win->window));
        set_toolbar(GDK_WINDOW_HWND(ab->win->window), TRUE);
        ab->docked = TRUE;
}

GtkAppBar *gtk_appbar_add(GtkWidget *win) {
        GtkAppBar *ab;

        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_add\n");

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
        gaim_debug(GAIM_DEBUG_INFO, "gtkappbar", "gtk_appbar_remove\n");

        if(!ab)
                return;
        gdk_window_remove_filter(ab->win->window,
                                 gtk_appbar_event_filter,
                                 ab);
        if(ab->docked) {
                gtk_window_resize(GTK_WINDOW(ab->win),
                                  ab->docked_rect.right - ab->docked_rect.left,
                                  ab->undocked_height);
                set_toolbar(GDK_WINDOW_HWND(ab->win->window), FALSE);
        }
        gtk_appbar_unregister(ab, GDK_WINDOW_HWND(ab->win->window));

        g_free(ab);
}
