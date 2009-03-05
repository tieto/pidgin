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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */


#include "internal.h"
#include <glib.h>

#define PLUGIN_STATIC_NAME	GntClipboard

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
#include <notify.h>
#include <gntwm.h>

#include <gntplugin.h>

#ifdef HAVE_X11
static pid_t child = 0;

static gulong sig_handle;

static void
set_clip(gchar *string)
{
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
	return;
}

static void
clipboard_changed(GntWM *wm, gchar *string)
{
	if (child) {
		kill(child, SIGTERM);
	}
	if ((child = fork() == 0)) {
		set_clip(string);
		_exit(0);
	}
}
#endif

static gboolean
plugin_load(PurplePlugin *plugin)
{
#ifdef HAVE_X11
	if (!XOpenDisplay(NULL)) {
		purple_debug_warning("gntclipboard", "Couldn't find X display\n");
		purple_notify_error(NULL, _("Error"), _("Error loading the plugin."),
				_("Couldn't find X display"));
		return FALSE;
	}
	if (!getenv("WINDOWID")) {
		purple_debug_warning("gntclipboard", "Couldn't find window\n");
		purple_notify_error(NULL, _("Error"), _("Error loading the plugin."),
				_("Couldn't find window"));
		return FALSE;
	}
	sig_handle = g_signal_connect(G_OBJECT(gnt_get_clipboard()), "clipboard_changed", G_CALLBACK(clipboard_changed), NULL);
	return TRUE;
#else
	purple_notify_error(NULL, _("Error"), _("Error loading the plugin."),
			_("This plugin cannot be loaded because it was not built with X11 support."));
	return FALSE;
#endif
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
#ifdef HAVE_X11
	if (child) {
		kill(child, SIGTERM);
		child = 0;
	}
	g_signal_handler_disconnect(G_OBJECT(gnt_get_clipboard()), sig_handle);
#endif
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	FINCH_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gntclipboard",
	N_("GntClipboard"),
	DISPLAY_VERSION,
	N_("Clipboard plugin"),
	N_("When the gnt clipboard contents change, "
		"the contents are made available to X, if possible."),
	"Richard Nelson <wabz@whatsbeef.net>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
