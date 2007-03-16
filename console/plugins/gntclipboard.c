/**
 * @file gntclipboard.c
 *
 * Copyright (C) 2007 Richard Nelson <wabz@whatsbeef.net>
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
 */


#include "internal.h"
#include <glib.h>

#define PLUGIN_STATIC_NAME	"GntClipboard"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

#include <sys/types.h>
#include <signal.h>

#include <glib.h>

#include <plugin.h>
#include <version.h>
#include <debug.h>
#include <gntwm.h>

#include <gntplugin.h>

static pid_t child = 0;

static gulong sig_handle;

static void
set_clip(gchar *string)
{
#ifdef HAVE_X11
	Window w;
	XEvent e, respond;
	XSelectionRequestEvent *req;
	const char *ids;
	Display *dpy = XOpenDisplay(NULL);

	if (!dpy)
		return;
	ids = getenv("WINDOWID");
	if (ids == NULL)
		return;
	w = atoi(ids);
	XSetSelectionOwner(dpy, XA_PRIMARY, w, CurrentTime);
	XFlush(dpy);
	XSelectInput(dpy, w, StructureNotifyMask);
	while (TRUE) {
		XNextEvent(dpy, &e); /* this blocks. */
		req = &e.xselectionrequest;
		if (e.type == SelectionRequest) {
			XChangeProperty(dpy,
				req->requestor,
				req->property,
				XA_STRING,
				8, PropModeReplace,
				(unsigned char *)string,
				strlen(string));
			respond.xselection.property = req->property;
			respond.xselection.type = SelectionNotify;
			respond.xselection.display = req->display;
			respond.xselection.requestor = req->requestor;
			respond.xselection.selection = req->selection;
			respond.xselection.target= req->target;
			respond.xselection.time = req->time;
			XSendEvent(dpy, req->requestor, 0, 0, &respond);
			XFlush (dpy);
		} else if (e.type == SelectionClear) {
			return;
		}
	}
#endif
	return;
}

static void
clipboard_changed(GntWM *wm, gchar *string)
{
#ifdef HAVE_X11
	if (child) {
		kill(child, SIGTERM);
	}
	if ((child = fork() == 0)) {
		set_clip(string);
		_exit(0);
	}
#endif
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	if (!XOpenDisplay(NULL)) {
		gaim_debug_warning("gntclipboard", "Couldn't find X display\n");
		return FALSE;
	}
	if (!getenv("WINDOWID")) {
		gaim_debug_warning("gntclipboard", "Couldn't find window\n");
		return FALSE;
	}
	sig_handle = g_signal_connect(G_OBJECT(gnt_get_clipboard()), "clipboard_changed", G_CALLBACK(clipboard_changed), NULL);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (child) {
		kill(child, SIGTERM);
		child = 0;
	}
	g_signal_handler_disconnect(G_OBJECT(gnt_get_clipboard()), sig_handle);
	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GNT_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	"gntclipboard",
	N_("GntClipboard"),
	VERSION,
	N_("Clipboard plugin"),
	N_("When the gnt clipboard contents change, "
		"the contents are made available to X, if possible."),
	"Richard Nelson <wabz@whatsbeef.net>",
	"http://gaim.sourceforge.net",
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
