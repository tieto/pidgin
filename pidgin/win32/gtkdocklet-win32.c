/*
 * System tray icon (aka docklet) plugin for Winpidgin
 *
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <windows.h>
#include <gdk/gdkwin32.h>
#include <gdk/gdk.h>

#include "internal.h"
#include "gtkblist.h"
#include "debug.h"

#include "resource.h"
#include "MinimizeToTray.h"
#include "gtkwin32dep.h"
#include "gtkdocklet.h"
#include "gaimstock.h"

/*
 *  DEFINES, MACROS & DATA TYPES
 */
#define WM_TRAYMESSAGE WM_USER /* User defined WM Message */

/*
 *  LOCALS
 */
static HWND systray_hwnd = NULL;
static const char *prev_icon_name = NULL;
static HICON prev_hicon = NULL;
static GtkWidget *image = NULL;
static NOTIFYICONDATA _nicon_data;

static LRESULT CALLBACK systray_mainmsg_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	static UINT taskbarRestartMsg; /* static here means value is kept across multiple calls to this func */

	switch(msg) {
	case WM_CREATE:
		gaim_debug_info("docklet", "WM_CREATE\n");
		taskbarRestartMsg = RegisterWindowMessage("TaskbarCreated");
		break;

	case WM_TIMER:
		gaim_debug_info("docklet", "WM_TIMER\n");
		break;

	case WM_DESTROY:
		gaim_debug_info("docklet", "WM_DESTROY\n");
		break;

	case WM_TRAYMESSAGE:
	{
		int type = 0;

		/* We'll use Double Click - Single click over on linux */
		if(lparam == WM_LBUTTONDBLCLK)
			type = 1;
		else if(lparam == WM_MBUTTONUP)
			type = 2;
		else if(lparam == WM_RBUTTONUP)
			type = 3;
		else
			break;

		pidgin_docklet_clicked(type);
		break;
	}
	default:
		if (msg == taskbarRestartMsg) {
			/* explorer crashed and left us hanging...
			   This will put the systray icon back in it's place, when it restarts */
			Shell_NotifyIcon(NIM_ADD, &_nicon_data);
		}
		break;
	}/* end switch */

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

/* Create hidden window to process systray messages */
static HWND systray_create_hiddenwin() {
	WNDCLASSEX wcex;
	LPCTSTR wname;

	wname = TEXT("WinpidginSystrayWinCls");

	wcex.cbSize = sizeof(wcex);
	wcex.style		= 0;
	wcex.lpfnWndProc	= systray_mainmsg_handler;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= winpidgin_hinstance();
	wcex.hIcon		= NULL;
	wcex.hCursor		= NULL,
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= wname;
	wcex.hIconSm		= NULL;

	RegisterClassEx(&wcex);

	/* Create the window */
	return (CreateWindow(wname, "", 0, 0, 0, 0, 0, GetDesktopWindow(), NULL, winpidgin_hinstance(), 0));
}

static void systray_init_icon(HWND hWnd) {
	ZeroMemory(&_nicon_data, sizeof(_nicon_data));
	_nicon_data.cbSize = sizeof(NOTIFYICONDATA);
	_nicon_data.hWnd = hWnd;
	_nicon_data.uID = 0;
	_nicon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	_nicon_data.uCallbackMessage = WM_TRAYMESSAGE;
	_nicon_data.hIcon = NULL;
	lstrcpy(_nicon_data.szTip, PIDGIN_NAME);
	Shell_NotifyIcon(NIM_ADD, &_nicon_data);
	pidgin_docklet_embedded();
}

/* This is ganked from GTK+.
 * When we can use GTK+ 2.10 and the GtkStatusIcon stuff, this will no longer be necesary */
#define WIN32_GDI_FAILED(api) printf("GDI FAILED %s\n", api)

static gboolean
_gdk_win32_pixbuf_to_hicon_supports_alpha (void)
{
  static gboolean is_win_xp=FALSE, is_win_xp_checked=FALSE;

  if (!is_win_xp_checked)
    {
      is_win_xp_checked = TRUE;

      if (!G_WIN32_IS_NT_BASED ())
	is_win_xp = FALSE;
      else
	{
	  OSVERSIONINFO version;

	  memset (&version, 0, sizeof (version));
	  version.dwOSVersionInfoSize = sizeof (version);
	  is_win_xp = GetVersionEx (&version)
	    && version.dwPlatformId == VER_PLATFORM_WIN32_NT
	    && (version.dwMajorVersion > 5
		|| (version.dwMajorVersion == 5 && version.dwMinorVersion >= 1));
	}
    }
  return is_win_xp;
}

static HBITMAP
create_alpha_bitmap (gint     size,
		     guchar **outdata)
{
  BITMAPV5HEADER bi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
  bi.bV5Size = sizeof (BITMAPV5HEADER);
  bi.bV5Height = bi.bV5Width = size;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  /* The following mask specification specifies a supported 32 BPP
   * alpha format for Windows XP (BGRA format).
   */
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  /* Create the DIB section with an alpha channel. */
  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static HBITMAP
create_color_bitmap (gint     size,
		     guchar **outdata,
		     gint     bits)
{
  struct {
    BITMAPV4HEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } bmi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bmi, sizeof (bmi));
  bmi.bmiHeader.bV4Size = sizeof (BITMAPV4HEADER);
  bmi.bmiHeader.bV4Height = bmi.bmiHeader.bV4Width = size;
  bmi.bmiHeader.bV4Planes = 1;
  bmi.bmiHeader.bV4BitCount = bits;
  bmi.bmiHeader.bV4V4Compression = BI_RGB;

  /* when bits is 1, these will be used.
   * bmiColors[0] already zeroed from ZeroMemory()
   */
  bmi.bmiColors[1].rgbBlue = 0xFF;
  bmi.bmiColors[1].rgbGreen = 0xFF;
  bmi.bmiColors[1].rgbRed = 0xFF;

  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static gboolean
pixbuf_to_hbitmaps_alpha_winxp (GdkPixbuf *pixbuf,
				HBITMAP   *color,
				HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  guchar *indata, *inrow;
  guchar *colordata, *colorrow, *maskdata, *maskbyte;
  gint width, height, size, i, i_offset, j, j_offset, rowstride;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_alpha_bitmap (size, &colordata);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + 4*(j+j_offset)*size + 4*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  colorrow[4*i+0] = inrow[4*i+2];
	  colorrow[4*i+1] = inrow[4*i+1];
	  colorrow[4*i+2] = inrow[4*i+0];
	  colorrow[4*i+3] = inrow[4*i+3];
	  if (inrow[4*i+3] == 0)
	    maskbyte[0] |= mask_bit;	/* turn ON bit */
	  else
	    maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

  return TRUE;
}

static gboolean
pixbuf_to_hbitmaps_normal (GdkPixbuf *pixbuf,
			   HBITMAP   *color,
			   HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  guchar *indata, *inrow;
  guchar *colordata, *colorrow, *maskdata, *maskbyte;
  gint width, height, size, i, i_offset, j, j_offset, rowstride, nc, bmstride;
  gboolean has_alpha;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_color_bitmap (size, &colordata, 24);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* rows are always aligned on 4-byte boundarys */
  bmstride = size * 3;
  if (bmstride % 4 != 0)
    bmstride += 4 - (bmstride % 4);

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  nc = gdk_pixbuf_get_n_channels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + (j+j_offset)*bmstride + 3*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  if (has_alpha && inrow[nc*i+3] < 128)
	    {
	      colorrow[3*i+0] = colorrow[3*i+1] = colorrow[3*i+2] = 0;
	      maskbyte[0] |= mask_bit;	/* turn ON bit */
	    }
	  else
	    {
	      colorrow[3*i+0] = inrow[nc*i+2];
	      colorrow[3*i+1] = inrow[nc*i+1];
	      colorrow[3*i+2] = inrow[nc*i+0];
	      maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	    }
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

  return TRUE;
}

static HICON
pixbuf_to_hicon (GdkPixbuf *pixbuf)
{
  gint x = 0, y = 0;
  gboolean is_icon = TRUE;
  ICONINFO ii;
  HICON icon;
  gboolean success;

  if (pixbuf == NULL)
    return NULL;

  if (_gdk_win32_pixbuf_to_hicon_supports_alpha() && gdk_pixbuf_get_has_alpha (pixbuf))
    success = pixbuf_to_hbitmaps_alpha_winxp (pixbuf, &ii.hbmColor, &ii.hbmMask);
  else
    success = pixbuf_to_hbitmaps_normal (pixbuf, &ii.hbmColor, &ii.hbmMask);

  if (!success)
    return NULL;

  ii.fIcon = is_icon;
  ii.xHotspot = x;
  ii.yHotspot = y;
  icon = CreateIconIndirect (&ii);
  DeleteObject (ii.hbmColor);
  DeleteObject (ii.hbmMask);
  return icon;
}

static HICON load_hicon_from_stock(const char *stock) {
	HICON hicon = NULL;
	GdkPixbuf *pixbuf = gtk_widget_render_icon(image, stock,
		gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL), NULL);

	if (pixbuf) {
		hicon = pixbuf_to_hicon(pixbuf);
		g_object_unref(pixbuf);
	} else
		gaim_debug_error("Unable to load pixbuf for %s.\n", stock);

	return hicon;
}



static void systray_change_icon(const char *icon_name) {
	HICON hicon;

	/* Avoid looking up the icon if it hasn't really changed.
	 * This will happen when re-displaying the icon when blinking.  */
	if (icon_name == prev_icon_name) {
		hicon = prev_hicon;
		prev_hicon = NULL;
	} else
		hicon = load_hicon_from_stock(icon_name);

	g_return_if_fail(hicon != NULL);

	_nicon_data.hIcon = hicon;
	Shell_NotifyIcon(NIM_MODIFY, &_nicon_data);

	if (prev_hicon)
		DestroyIcon(prev_hicon);
	prev_hicon = hicon;
	prev_icon_name = icon_name;

}

static void systray_remove_nid(void) {
	Shell_NotifyIcon(NIM_DELETE, &_nicon_data);
}

static void winpidgin_tray_update_icon(DockletStatus icon) {
	const gchar *icon_name = NULL;

	g_return_if_fail(image != NULL);

	switch (icon) {
		case DOCKLET_STATUS_OFFLINE:
			icon_name = PIDGIN_STOCK_TRAY_OFFLINE;
			break;
		case DOCKLET_STATUS_CONNECTING:
			icon_name = PIDGIN_STOCK_TRAY_CONNECT;
			break;
		case DOCKLET_STATUS_AVAILABLE:
			icon_name = PIDGIN_STOCK_TRAY_AVAILABLE;
			break;
		case DOCKLET_STATUS_PENDING:
			icon_name = PIDGIN_STOCK_TRAY_PENDING;
			break;
		case DOCKLET_STATUS_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_AWAY;
			break;
		case DOCKLET_STATUS_BUSY:
			icon_name = PIDGIN_STOCK_TRAY_BUSY;
			break;
		case DOCKLET_STATUS_XA:
			icon_name = PIDGIN_STOCK_TRAY_XA;
			break;
	}

	if(icon_name)
		systray_change_icon(icon_name);
}

static void winpidgin_tray_blank_icon() {
	_nicon_data.hIcon = NULL;
	Shell_NotifyIcon(NIM_MODIFY, &_nicon_data);
}

static void winpidgin_tray_set_tooltip(gchar *tooltip) {
	if (tooltip) {
		char *locenc = NULL;
		locenc = g_locale_from_utf8(tooltip, -1, NULL, NULL, NULL);
		lstrcpyn(_nicon_data.szTip, locenc, sizeof(_nicon_data.szTip) / sizeof(TCHAR));
		g_free(locenc);
	} else {
		lstrcpy(_nicon_data.szTip, PIDGIN_NAME);
	}
	Shell_NotifyIcon(NIM_MODIFY, &_nicon_data);
}

static void winpidgin_tray_minimize(PidginBuddyList *gtkblist) {
	MinimizeWndToTray(GDK_WINDOW_HWND(gtkblist->window->window));
}

static void winpidgin_tray_maximize(PidginBuddyList *gtkblist) {
	RestoreWndFromTray(GDK_WINDOW_HWND(gtkblist->window->window));
}


static void winpidgin_tray_create() {
	/* dummy window to process systray messages */
	systray_hwnd = systray_create_hiddenwin();

	image = gtk_image_new();
#if GLIB_CHECK_VERSION(2,10,0)
	g_object_ref_sink(image);
#else
	g_object_ref(image);
	gtk_object_sink(GTK_OBJECT(image));
#endif

	/* Create icon in systray */
	systray_init_icon(systray_hwnd);

	gaim_signal_connect(pidgin_blist_get_handle(), "gtkblist-hiding",
			pidgin_docklet_get_handle(), GAIM_CALLBACK(winpidgin_tray_minimize), NULL);
	gaim_signal_connect(pidgin_blist_get_handle(), "gtkblist-unhiding",
			pidgin_docklet_get_handle(), GAIM_CALLBACK(winpidgin_tray_maximize), NULL);

	gaim_debug_info("docklet", "created\n");
}

static void winpidgin_tray_destroy() {
	systray_remove_nid();
	DestroyWindow(systray_hwnd);
	pidgin_docklet_remove();

	g_object_unref(image);
	image = NULL;
}

static struct docklet_ui_ops winpidgin_tray_ops =
{
	winpidgin_tray_create,
	winpidgin_tray_destroy,
	winpidgin_tray_update_icon,
	winpidgin_tray_blank_icon,
	winpidgin_tray_set_tooltip,
	NULL
};

/* Used by docklet's plugin load func */
void docklet_ui_init() {
	pidgin_docklet_set_ui_ops(&winpidgin_tray_ops);
}
