/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * some code: (most in this file)
 * Copyright (C) 1996 Netscape Communications Corporation, all rights reserved.
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Dec-94.
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
 * This code is mainly taken from Netscape's sample implementation of
 * their protocol.  Nifty.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef _WIN32
#include <gdk/gdkwin32.h>
#else
#include <unistd.h>
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
#include <gdk/gdkx.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include "gaim.h"

#ifndef _WIN32




#include <X11/Xlib.h>
#include <X11/Xatom.h>


static const char *progname = "gaim";
static const char *expected_mozilla_version = "1.1";

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"

static GdkAtom GDKA_MOZILLA_VERSION = 0;
static GdkAtom GDKA_MOZILLA_LOCK = 0;
static GdkAtom GDKA_MOZILLA_COMMAND = 0;
static GdkAtom GDKA_MOZILLA_RESPONSE = 0;

static char *window_check_mozilla_version(Window);
static const char *get_lock_data();
static GdkFilterReturn netscape_response_cb(XEvent *, GdkEvent *, GdkWindow *);
static gboolean netscape_command(const char *);

static int netscape_lock;


static Window VirtualRootWindowOfScreen(screen)
Screen *screen;
{
	static Screen *save_screen = (Screen *) 0;
	static Window root = (Window) 0;

	if (screen != save_screen) {
		Display *dpy = DisplayOfScreen(screen);
		Atom __SWM_VROOT = None;
		unsigned int i;
		Window rootReturn, parentReturn, *children;
		unsigned int numChildren;

		root = RootWindowOfScreen(screen);

		/* go look for a virtual root */
		__SWM_VROOT = XInternAtom(dpy, "__SWM_VROOT", False);
		if (XQueryTree(dpy, root, &rootReturn, &parentReturn, &children, &numChildren)) {
			for (i = 0; i < numChildren; i++) {
				Atom actual_type;
				int actual_format;
				unsigned long nitems, bytesafter;
				Window *newRoot = (Window *) 0;

				if (XGetWindowProperty(dpy, children[i],
						       __SWM_VROOT, 0, 1, False, XA_WINDOW,
						       &actual_type, &actual_format,
						       &nitems, &bytesafter,
						       (unsigned char **)&newRoot) == Success
				    && newRoot) {
					root = *newRoot;
					break;
				}
			}
			if (children)
				XFree((char *)children);
		}

		save_screen = screen;
	}

	return root;
}

/* The following code is Copyright (C) 1989 X Consortium */

static Window TryChildren();

/* Find a window with WM_STATE, else return win itself, as per ICCCM */

static Window GClientWindow(dpy, win)
Display *dpy;
Window win;
{
	Atom WM_STATE;
	Atom type = None;
	int format;
	unsigned long nitems, after;
	unsigned char *data;
	Window inf;

	WM_STATE = XInternAtom(dpy, "WM_STATE", True);
	if (!WM_STATE)
		return win;
	XGetWindowProperty(dpy, win, WM_STATE, 0, 0, False, AnyPropertyType,
			   &type, &format, &nitems, &after, &data);
	if (type) {
		XFree(data);
		return win;
	}

	inf = TryChildren(dpy, win, WM_STATE);
	if (!inf)
		inf = win;

	XFree(data);

	return inf;
}

static
Window TryChildren(dpy, win, WM_STATE)
Display *dpy;
Window win;
Atom WM_STATE;
{
	Window root, parent;
	Window *children;
	unsigned int nchildren;
	unsigned int i;
	Atom type = None;
	int format;
	unsigned long nitems, after;
	unsigned char *data;
	Window inf = 0;

	if (!XQueryTree(dpy, win, &root, &parent, &children, &nchildren))
		return 0;
	for (i = 0; !inf && (i < nchildren); i++) {
		XGetWindowProperty(dpy, children[i], WM_STATE, 0, 0, False,
				   AnyPropertyType, &type, &format, &nitems, &after, &data);
		if (type)
			inf = children[i];

		XFree(data);
	}
	for (i = 0; !inf && (i < nchildren); i++)
		inf = TryChildren(dpy, children[i], WM_STATE);
	if (children)
		XFree((char *)children);
	return inf;
}

/* END X Consortium code */



static void mozilla_remote_init_atoms()
{
	if (!GDKA_MOZILLA_VERSION)
		GDKA_MOZILLA_VERSION = gdk_atom_intern(MOZILLA_VERSION_PROP, 0);
	if (!GDKA_MOZILLA_LOCK)
		GDKA_MOZILLA_LOCK = gdk_atom_intern(MOZILLA_LOCK_PROP, 0);
	if (!GDKA_MOZILLA_COMMAND)
		GDKA_MOZILLA_COMMAND = gdk_atom_intern(MOZILLA_COMMAND_PROP, 0);
	if (!GDKA_MOZILLA_RESPONSE)
		GDKA_MOZILLA_RESPONSE = gdk_atom_intern(MOZILLA_RESPONSE_PROP, 0);
}

static char *window_check_mozilla_version(Window window) {

	Atom type;
	int format;
	unsigned long nitems, bytesafter;
	unsigned char *version = 0;
	gchar *retval = NULL;

	if (XGetWindowProperty(gdk_display, window, 
					gdk_x11_atom_to_xatom(GDKA_MOZILLA_VERSION),
					0, (65536 / sizeof(long)),
					False, XA_STRING,
					&type, &format, &nitems, &bytesafter,
					&version) != Success) {	
		return NULL;
	}

	if (!version) {
		return NULL;
	}

	retval = g_strdup(version);
	XFree(version);

	return retval;
}

static GdkWindow *mozilla_remote_find_window()
{
	int i;
	Window root = VirtualRootWindowOfScreen(DefaultScreenOfDisplay(gdk_display));
	Window root2, parent, *kids;
	unsigned int nkids;
	Window result = 0;
	Window tenative = 0;
	unsigned char *tenative_version = 0, *version = 0;
	static GdkWindow *remote_window = NULL;

	if (remote_window != NULL) {
		version = window_check_mozilla_version(GDK_WINDOW_XID(remote_window));

		if (version != NULL) {
			g_free(version);
			return remote_window;
		}
		g_free(version);

		gdk_window_destroy(remote_window);
		remote_window = NULL;
	}

	if (!XQueryTree(gdk_display, root, &root2, &parent, &kids, &nkids)) {
		debug_printf("%s: XQueryTree failed on display %s\n", progname,
			     DisplayString(gdk_display));
		return NULL;
	}

	/* root != root2 is possible with virtual root WMs. */

	if (!(kids && nkids)) {
		debug_printf("%s: root window has no children on display %s\n",
			     progname, DisplayString(gdk_display));
		return NULL;
	}

	for (i = nkids - 1; i >= 0; i--) {
		Window w = GClientWindow(gdk_display, kids[i]);

        version = window_check_mozilla_version(w);

		if (version == NULL) {
			continue;
		}

		if (strcmp((char *)version, expected_mozilla_version) && !tenative) {
			tenative = w;
			tenative_version = version;
			continue;
		}

		g_free(version);

			result = w;
			break;
		}

	XFree(kids);

	if (result && tenative) {
		debug_printf("%s: warning: both version %s (0x%x) and version\n"
			     "\t%s (0x%x) are running.  Using version %s.\n",
			     progname, tenative_version, (unsigned int)tenative,
			     expected_mozilla_version, (unsigned int)result, expected_mozilla_version);
		XFree(tenative_version);
		return gdk_window_foreign_new(result);
	} else if (tenative) {
		debug_printf("%s: warning: expected version %s but found version\n"
			     "\t%s (0x%x) instead.\n",
			     progname, expected_mozilla_version,
			     tenative_version, (unsigned int)tenative);
		XFree(tenative_version);
		return gdk_window_foreign_new(tenative);
	} else if (result) {
		return gdk_window_foreign_new(result);
	} else {
		debug_printf("%s: not running on display %s\n", progname, DisplayString(gdk_display));
		return NULL;
	}
}


static const char *get_lock_data() {
	static char *lock_data = NULL;

	if (lock_data == NULL) {
		char hostname[HOST_NAME_MAX + 1] = {0}; 

		if (gethostname(hostname, HOST_NAME_MAX + 1) == 0) {
			lock_data = g_strdup_printf("pid%d@%s", getpid(), hostname);
		} else {
			lock_data = g_strdup_printf("pid%d", getpid());
		}
	}

	return lock_data;
}

static gboolean mozilla_remote_obtain_lock(GdkWindow * window)
{
	gboolean locked = False;
	const char *lock_data = get_lock_data();
		int result;
		GdkAtom actual_type;
		gint actual_format;
		gint nitems;
		unsigned char *data = 0;

	gdk_x11_grab_server();
	if (!gdk_property_get(window, GDKA_MOZILLA_LOCK,
					  gdk_x11_xatom_to_atom (XA_STRING), 0,
					  (65536 / sizeof(long)), 0,
				  &actual_type, &actual_format, &nitems, &data)) {

			/* It's not now locked - lock it. */
		debug_printf("%s: (writing " MOZILLA_LOCK_PROP " \"%s\" to 0x%x)\n", 
					 progname, lock_data, (unsigned int) window);

			gdk_property_change(window, GDKA_MOZILLA_LOCK, 
					    gdk_x11_xatom_to_atom (XA_STRING),
					    8, PropModeReplace,
					    (unsigned char *)lock_data, strlen(lock_data));
			locked = True;
		}

			if (data)
				g_free(data);
	gdk_x11_ungrab_server();
}


static void mozilla_remote_free_lock(GdkWindow * window)
{
	int result = 0;
	GdkAtom actual_type;
	gint actual_format; 
	gint nitems;
	unsigned char *data = 0;
	const char *lock_data = get_lock_data();

	debug_printf("%s: (deleting " MOZILLA_LOCK_PROP
		     " \"%s\" from 0x%x)\n", progname, lock_data, (unsigned int)window);

	result = gdk_property_get(window, GDKA_MOZILLA_LOCK, 
				  gdk_x11_xatom_to_atom (XA_STRING),
				  0, (65536 / sizeof(long)),
				  1, &actual_type, &actual_format, &nitems, &data);

	if (result != TRUE) {
		debug_printf("%s: unable to read and delete " MOZILLA_LOCK_PROP " property\n", progname);
		return;
	} else if (!data || !*data) {
		debug_printf("%s: invalid data on " MOZILLA_LOCK_PROP
			     " of window 0x%x.\n", progname, (unsigned int)window);
		return;
	} else if (strcmp((char *)data, lock_data)) {
		debug_printf("%s: " MOZILLA_LOCK_PROP
			     " was stolen!  Expected \"%s\", saw \"%s\"!\n", progname, lock_data, data);
		return;
	}

	XFree(data);
}

static GdkFilterReturn netscape_response_cb(XEvent *event, GdkEvent *translated, GdkWindow *window)
{
	Atom actual_type, mozilla_response;
	Window xid;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data = 0;
	char *error = NULL;

	if (window == NULL || GDK_WINDOW_OBJECT(window)->destroyed) {
		do_error_dialog(_("Communication with the browser failed.  Please close all "
					      "windows and try again."), NULL, GAIM_ERROR);
		debug_printf("netscape_response_cb called with NULL window.\n");
		return GDK_FILTER_CONTINUE;
	}

	mozilla_response = gdk_x11_atom_to_xatom(GDKA_MOZILLA_RESPONSE);
    xid = GDK_WINDOW_XID(window);

	/* If the event isn't what we want then let gtk handle it */
	if (event->xany.type != PropertyNotify ||
		event->xproperty.state != PropertyNewValue ||
		event->xproperty.window != xid ||
		event->xproperty.atom != mozilla_response) {
		return GDK_FILTER_CONTINUE;
	}

	if (XGetWindowProperty (gdk_display, xid, mozilla_response,
							 0, (65536 / sizeof (long)),
							 True, 
							 XA_STRING, 
							 &actual_type, &actual_format,
							 &nitems, &bytes_after,
							 &data) != Success 
		|| data == NULL || (data[0] != '1' && data[0] != '2')) {

		do_error_dialog(_("Communication with the browser failed.  Please close all "
					      "windows and try again."), NULL, GAIM_ERROR);
	} 

    if (data[0] == '1') {
		/* Netscape isn't ready yet */
		debug_printf("Remote Netscape window isn't ready yet.\n");
		return GDK_FILTER_REMOVE;
	} 
	
	if (data[0] == '2') {
		/* Yay! It worked */ 
		debug_printf("Successfully sent command to remote Netscape window.\n");
	}

	gdk_window_remove_filter(window, (GdkFilterFunc) netscape_response_cb, window); 
	mozilla_remote_free_lock(window);
	netscape_lock = 0;
	return GDK_FILTER_REMOVE;
}

static void mozilla_remote_command(GdkWindow * window, const char *command, Bool raise_p)
{
	int result = 0;
	Bool done = False;
	char *new_command = 0;

	/* The -noraise option is implemented by passing a "noraise" argument
	   to each command to which it should apply.
	 */
	if (!raise_p) {
		char *close;
		new_command = g_malloc(strlen(command) + 20);
		strcpy(new_command, command);
		close = strrchr(new_command, ')');
		if (close)
			strcpy(close, ", noraise)");
		else
			strcat(new_command, "(noraise)");
		command = new_command;
	}

	debug_printf("%s: (writing " MOZILLA_COMMAND_PROP " \"%s\" to 0x%x)\n",
		     progname, command, (unsigned int)window);

	gdk_property_change(window, GDKA_MOZILLA_COMMAND, 
			    gdk_x11_xatom_to_atom (XA_STRING), 
			    8, GDK_PROP_MODE_REPLACE, (unsigned char *)command, strlen(command));

	while (!done) {
		GdkEvent *event;

		event = gdk_event_get();

		if (!event)
			continue;

		if (event->any.window != window) {
			gtk_main_do_event(event);
			continue;
		}

		if (event->type == GDK_DESTROY && event->any.window == window) {

			/* Print to warn user... */
			debug_printf("%s: window 0x%x was destroyed.\n", progname, (unsigned int)window);
			result = 6;
			done = True;
		} else if (event->type == GDK_PROPERTY_NOTIFY &&
			   event->property.state == GDK_PROPERTY_NEW_VALUE &&
			   event->property.window == window &&
			   event->property.atom == GDKA_MOZILLA_RESPONSE) {
			GdkAtom actual_type;
			gint actual_format, nitems;
			unsigned char *data = 0;

			result = gdk_property_get(window, GDKA_MOZILLA_RESPONSE,
						  gdk_x11_xatom_to_atom (XA_STRING), 0,
						  (65536 / sizeof(long)),
						  1, &actual_type, &actual_format, &nitems, &data);


			if (result == Success && data && *data) {
				debug_printf("%s: (server sent " MOZILLA_RESPONSE_PROP
					     " \"%s\" to 0x%x.)\n",
					     progname, data, (unsigned int)window);
			}

			if (result != Success) {
				debug_printf("%s: failed reading " MOZILLA_RESPONSE_PROP
					     " from window 0x%0x.\n", progname, (unsigned int)window);
				result = 6;
				done = True;
			} else if (!data || strlen((char *)data) < 5) {
				debug_printf("%s: invalid data on " MOZILLA_RESPONSE_PROP
					     " property of window 0x%0x.\n",
					     progname, (unsigned int)window);
				result = 6;
				done = True;
			} else if (*data == '1') {	/* positive preliminary reply */
				debug_printf("%s: %s\n", progname, data + 4);
				/* keep going */
				done = False;
			} else if (!strncmp((char *)data, "200", 3)) {
				result = 0;
				done = True;
			} else if (*data == '2') {
				debug_printf("%s: %s\n", progname, data + 4);
				result = 0;
				done = True;
			} else if (*data == '3') {
				debug_printf("%s: internal error: "
					     "server wants more information?  (%s)\n", progname, data);
				result = 3;
				done = True;
			} else if (*data == '4' || *data == '5') {
				debug_printf("%s: %s\n", progname, data + 4);
				result = (*data - '0');
				done = True;
			} else {
				debug_printf("%s: unrecognised " MOZILLA_RESPONSE_PROP
					     " from window 0x%x: %s\n",
					     progname, (unsigned int)window, data);
				result = 6;
				done = True;
			}

			if (data)
				g_free(data);
		} else if (event->type == GDK_PROPERTY_NOTIFY &&
			   event->property.window == window &&
			   event->property.state == GDK_PROPERTY_DELETE &&
			   event->property.atom == GDKA_MOZILLA_COMMAND) {
			debug_printf("%s: (server 0x%x has accepted "
				     MOZILLA_COMMAND_PROP ".)\n", progname, (unsigned int)window);
		}
		gdk_event_free(event);
	}

	if (new_command)
		g_free(new_command);

	return result;
}


gboolean check_netscape(gpointer data)
{
	char *msg = data;
	int status;
	GdkWindow *window;

	mozilla_remote_init_atoms();
	window = mozilla_remote_find_window();

	if (window && (GDK_WINDOW_OBJECT(window)->destroyed == FALSE)) {

		XSelectInput(gdk_display, GDK_WINDOW_XWINDOW(window),
			     (PropertyChangeMask | StructureNotifyMask));


		mozilla_remote_obtain_lock(window);

		status = mozilla_remote_command(window, msg, False);

		if (status != 6)
			mozilla_remote_free_lock(window);

		netscape_lock = 0;

		g_free(msg);
		return FALSE;
	} else
		return TRUE;
}


static void netscape_command(char *command)
{
	int status;
	pid_t pid;
	GdkWindow *window;

	if (netscape_lock)
		return;

	netscape_lock = 1;



	mozilla_remote_init_atoms();
	window = mozilla_remote_find_window();

	if (window && (GDK_WINDOW_OBJECT(window)->destroyed == FALSE)) {

		XSelectInput(gdk_display, GDK_WINDOW_XWINDOW(window),
			     (PropertyChangeMask | StructureNotifyMask));

		mozilla_remote_obtain_lock(window);

		status = mozilla_remote_command(window, command, False);

		if (status != 6)
			mozilla_remote_free_lock(window);

		netscape_lock = 0;

		gdk_window_destroy(window);
	} else {
		pid = fork();
		if (pid == 0) {
			char *args[2];
			int e;

			args[0] = g_strdup("netscape");
			args[1] = NULL;
			e = execvp(args[0], args);
			printf("Hello%d\n", getppid());

			_exit(0);
		} else {
			char *tmp = g_strdup(command);
			g_timeout_add(200, check_netscape, tmp);
		}
	}

}

void open_url(GtkWidget *w, char *url)
{
	gchar *space_free_url;

	if (web_browser == BROWSER_NETSCAPE) {
		char *command;

		if (misc_options & OPT_MISC_BROWSER_POPUP)
			command = g_strdup_printf("OpenURL(%s, new-window)", url);
		else
			command = g_strdup_printf("OpenURL(%s)", url);

		netscape_command(command);
		g_free(command);
/* fixme: GNOME helper					*
 *	} else if (web_browser == BROWSER_GNOME) {	*
 *		gnome_url_show(url);			*/
	} else {
		pid_t pid;

		pid = fork();

		if (pid == 0) {
			/* args will be allocated below but we don't bother
			 * freeing it since we're just going to exec and
			 * exit */
			char **args=NULL;
			char command[1024];

			if (web_browser == BROWSER_OPERA) {
				args = g_new(char *, 4);
				args[0] = "opera";
				args[1] = "-newwindow";
				args[2] = url;
				args[3] = NULL;
			} else if (web_browser == BROWSER_KONQ) {
				args = g_new(char *, 4);
				args[0] = "kfmclient";
				args[1] = "openURL";
				args[2] = url;
				args[3] = NULL;
			} else if (web_browser == BROWSER_GALEON) {
				args = g_new(char *, 4);
				args[0] = "galeon";
				if (misc_options & OPT_MISC_BROWSER_POPUP) {
					args[1] = "-w";
					args[2] = url;
					args[3] = NULL;
				} else {
					args[1] = url;
					args[2] = NULL;
				}
			} else if (web_browser == BROWSER_MOZILLA) {
				args = g_new(char *, 4);
				args[0] = "mozilla";
				args[1] = url;
				args[2] = NULL;
			} else if (web_browser == BROWSER_MANUAL) {
				if(strcmp(web_command,"") == 0)
					_exit(0);
				space_free_url = g_strdelimit(url, " ", '+');
				g_snprintf(command, sizeof(command), web_command, space_free_url);
				g_free(space_free_url);
				args = g_strsplit(command, " ", 0);
			}

			execvp(args[0], args);
			_exit(0);
		}
	}
}

void add_bookmark(GtkWidget *w, char *url)
{
	if (web_browser == BROWSER_NETSCAPE) {
		char *command = g_malloc(1024);

		g_snprintf(command, 1024, "AddBookmark(%s)", url);

		netscape_command(command);
		g_free(command);
	}
}

#else

/* Sooner or later, I shall support Windows clicking! */

void add_bookmark(GtkWidget *w, char *url)
{
}
void open_url(GtkWidget *w, char *url)
{
	ShellExecute(NULL, NULL, url, NULL, ".\\", 0);
}


#endif /* _WIN32 */
