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
#include "../config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>




#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
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

static GdkAtom XA_MOZILLA_VERSION  = 0;
static GdkAtom XA_MOZILLA_LOCK     = 0;
static GdkAtom XA_MOZILLA_COMMAND  = 0;
static GdkAtom XA_MOZILLA_RESPONSE = 0;


static int netscape_lock;


static Window
VirtualRootWindowOfScreen(screen)
        Screen *screen;
{
        static Screen *save_screen = (Screen *)0;
        static Window root = (Window)0;

        if (screen != save_screen) {
                Display *dpy = DisplayOfScreen(screen);
                Atom __SWM_VROOT = None;
                unsigned int i;
                Window rootReturn, parentReturn, *children;
                unsigned int numChildren;

                root = RootWindowOfScreen(screen);

                /* go look for a virtual root */
                __SWM_VROOT = XInternAtom(dpy, "__SWM_VROOT", False);
                if (XQueryTree(dpy, root, &rootReturn, &parentReturn,
                                 &children, &numChildren)) {
                        for (i = 0; i < numChildren; i++) {
                                Atom actual_type;
                                int actual_format;
                                unsigned long nitems, bytesafter;
                                Window *newRoot = (Window *)0;

                                if (XGetWindowProperty(dpy, children[i],
                                        __SWM_VROOT, 0, 1, False, XA_WINDOW,
                                        &actual_type, &actual_format,
                                        &nitems, &bytesafter,
                                        (unsigned char **) &newRoot) == Success
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

static Window GClientWindow (dpy, win)
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
    if (type)
    {
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
Window TryChildren (dpy, win, WM_STATE)
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
                           AnyPropertyType, &type, &format, &nitems,
                           &after, &data);
        if (type)
            inf = children[i];

	XFree(data);
    }
    for (i = 0; !inf && (i < nchildren); i++)
        inf = TryChildren(dpy, children[i], WM_STATE);
    if (children) XFree((char *)children);
    return inf;
}

/* END X Consortium code */



static void mozilla_remote_init_atoms()
{
	if (!XA_MOZILLA_VERSION)
		XA_MOZILLA_VERSION = gdk_atom_intern(MOZILLA_VERSION_PROP, 0);
	if (!XA_MOZILLA_LOCK)
                XA_MOZILLA_LOCK = gdk_atom_intern(MOZILLA_LOCK_PROP, 0);
        if (! XA_MOZILLA_COMMAND)
                XA_MOZILLA_COMMAND = gdk_atom_intern(MOZILLA_COMMAND_PROP, 0);
	if (! XA_MOZILLA_RESPONSE)
		XA_MOZILLA_RESPONSE = gdk_atom_intern(MOZILLA_RESPONSE_PROP, 0);
}

static GdkWindow *mozilla_remote_find_window()
{
        int i;
        Window root = VirtualRootWindowOfScreen(DefaultScreenOfDisplay(gdk_display));
        Window root2, parent, *kids;
        unsigned int nkids;
        Window result = 0;
        Window tenative = 0;
        unsigned char *tenative_version = 0;

        if (!XQueryTree (gdk_display, root, &root2, &parent, &kids, &nkids))
        {
            sprintf (debug_buff, "%s: XQueryTree failed on display %s\n", progname,
                     DisplayString (gdk_display));
			debug_print(debug_buff);
            return NULL;
        }

        /* root != root2 is possible with virtual root WMs. */

        if (!(kids && nkids)) {
                sprintf (debug_buff, "%s: root window has no children on display %s\n",
                         progname, DisplayString (gdk_display));
		debug_print(debug_buff);
            	return NULL;
        }

        for (i = nkids-1; i >= 0; i--)
        {
                Atom type;
                int format;
                unsigned long nitems, bytesafter;
                unsigned char *version = 0;
                Window w = GClientWindow (gdk_display, kids[i]);
                int status = XGetWindowProperty (gdk_display, w, XA_MOZILLA_VERSION,
                                                 0, (65536 / sizeof (long)),
                                                 False, XA_STRING,
                                                 &type, &format, &nitems, &bytesafter,
                                                 &version);

                if (! version)
                        continue;

                if (strcmp ((char *) version, expected_mozilla_version) &&
                    !tenative)
                {
                        tenative = w;
                        tenative_version = version;
                        continue;
                }
                XFree(version);
                if (status == Success && type != None)
                {
                        result = w;
                        break;
                }
        }

	XFree(kids);

        if (result && tenative)
        {
            sprintf (debug_buff,
                         "%s: warning: both version %s (0x%x) and version\n"
                         "\t%s (0x%x) are running.  Using version %s.\n",
                         progname, tenative_version, (unsigned int) tenative,
                         expected_mozilla_version, (unsigned int) result,
                         expected_mozilla_version);
			debug_print(debug_buff);
            XFree(tenative_version);
            return gdk_window_foreign_new(result);
        }
        else if (tenative)
        {
            sprintf (debug_buff,
                     "%s: warning: expected version %s but found version\n"
                     "\t%s (0x%x) instead.\n",
                     progname, expected_mozilla_version,
                     tenative_version, (unsigned int) tenative);
			debug_print(debug_buff);
            XFree(tenative_version);
            return gdk_window_foreign_new(tenative);
        }
        else if (result)
        {
                return gdk_window_foreign_new(result);
        }
        else
        {
            sprintf (debug_buff, "%s: not running on display %s\n", progname,
                     DisplayString (gdk_display));
			debug_print(debug_buff);
            return NULL;
        }
}


static char *lock_data = 0;

static void mozilla_remote_obtain_lock (GdkWindow *window)
{
        Bool locked = False;

        if (!lock_data) {
		lock_data = (char *)g_malloc (255);
		sprintf (lock_data, "pid%d@", getpid ());
		if (gethostname (lock_data + strlen (lock_data), 100)) {
			return;
		}
	}

        do {
                int result;
                GdkAtom actual_type;
                gint actual_format;
		gint nitems;
                unsigned char *data = 0;

                result = gdk_property_get (window, XA_MOZILLA_LOCK,
					   XA_STRING, 0,
					   (65536 / sizeof (long)), 0,
					   &actual_type, &actual_format,
					   &nitems, &data);
                if (result != Success || actual_type == None)
                {
                        /* It's not now locked - lock it. */
                    sprintf (debug_buff, "%s: (writing " MOZILLA_LOCK_PROP
                             " \"%s\" to 0x%x)\n",
                             progname, lock_data, (unsigned int) window);
					debug_print(debug_buff);

					gdk_property_change(window, XA_MOZILLA_LOCK, XA_STRING,
						    8, PropModeReplace,
						    (unsigned char *) lock_data,
					   		strlen (lock_data));
                    locked = True;
                }

                if (!locked) {
                        /* Then just fuck it. */
                        if (data)
                                g_free(data);
                        return;
                }
                if (data)
                        g_free(data);
        } while (!locked);
}


static void mozilla_remote_free_lock (GdkWindow *window)
{
        int result = 0;
        GdkAtom actual_type;
        gint actual_format;
        gint nitems;
        unsigned char *data = 0;

       	sprintf (debug_buff, "%s: (deleting " MOZILLA_LOCK_PROP
           	     " \"%s\" from 0x%x)\n",
               	 progname, lock_data, (unsigned int) window);
		debug_print(debug_buff);

		result = gdk_property_get(window, XA_MOZILLA_LOCK, XA_STRING,
				  0, (65536 / sizeof (long)),
				  1, &actual_type, &actual_format,
				  &nitems, &data);
        if (result != Success)
        {
             sprintf (debug_buff, "%s: unable to read and delete " MOZILLA_LOCK_PROP
                     " property\n",
                     progname);
		   	 debug_print(debug_buff);
           	 return;
        }
        else if (!data || !*data)
        {
              sprintf (debug_buff, "%s: invalid data on " MOZILLA_LOCK_PROP
               	     " of window 0x%x.\n",
                   	 progname, (unsigned int) window);
			  debug_print(debug_buff);
              return;
        }
        else if (strcmp ((char *) data, lock_data))
        {
            sprintf (debug_buff, "%s: " MOZILLA_LOCK_PROP
                     " was stolen!  Expected \"%s\", saw \"%s\"!\n",
                     progname, lock_data, data);
			debug_print(debug_buff);
            return;
        }

        if (data)
                g_free(data);
}


static int
mozilla_remote_command (GdkWindow *window, const char *command,
                        Bool raise_p)
{
        int result = 0;
        Bool done = False;
        char *new_command = 0;

        /* The -noraise option is implemented by passing a "noraise" argument
         to each command to which it should apply.
         */
        if (!raise_p)
        {
                char *close;
                new_command = g_malloc (strlen (command) + 20);
                strcpy (new_command, command);
                close = strrchr (new_command, ')');
                if (close)
                        strcpy (close, ", noraise)");
                else
                        strcat (new_command, "(noraise)");
                command = new_command;
        }

       	sprintf (debug_buff, "%s: (writing " MOZILLA_COMMAND_PROP " \"%s\" to 0x%x)\n",
           	     progname, command, (unsigned int) window);
		debug_print(debug_buff);

	gdk_property_change(window, XA_MOZILLA_COMMAND, XA_STRING, 8,
			    GDK_PROP_MODE_REPLACE, (unsigned char *) command,
			    strlen (command));

	while (!done) {
                GdkEvent *event;
		
                event = gdk_event_get();
		
		if (!event)
			continue;
		
		if (event->any.window != window) {
			gtk_main_do_event(event);
			continue;
		}

                if (event->type == GDK_DESTROY &&
                    event->any.window == window) {

						/* Print to warn user...*/
						sprintf (debug_buff, "%s: window 0x%x was destroyed.\n",
								 progname, (unsigned int) window);
						debug_print(debug_buff);
						result = 6;
						goto DONE;
		} else if (event->type == GDK_PROPERTY_NOTIFY &&
                         event->property.state == GDK_PROPERTY_NEW_VALUE &&
                         event->property.window == window &&
                         event->property.atom == XA_MOZILLA_RESPONSE) {
			GdkAtom actual_type;
			gint actual_format, nitems;
			unsigned char *data = 0;

			result = gdk_property_get (window, XA_MOZILLA_RESPONSE,
						   XA_STRING, 0,
						   (65536 / sizeof (long)),
						   1,
						   &actual_type, &actual_format,
						   &nitems, &data);

			
			if (result == Success && data && *data) {
				sprintf (debug_buff, "%s: (server sent " MOZILLA_RESPONSE_PROP
					 " \"%s\" to 0x%x.)\n",
					 progname, data, (unsigned int) window);
				debug_print(debug_buff);
			}

			if (result != Success) {
				sprintf (debug_buff, "%s: failed reading " MOZILLA_RESPONSE_PROP
					 " from window 0x%0x.\n",
					 progname, (unsigned int) window);
				debug_print(debug_buff);
				result = 6;
				done = True;
			} else if (!data || strlen((char *) data) < 5) {
				sprintf (debug_buff, "%s: invalid data on " MOZILLA_RESPONSE_PROP
					 " property of window 0x%0x.\n",
					 progname, (unsigned int) window);
				debug_print(debug_buff);
				result = 6;
				done = True;
			} else if (*data == '1') { /* positive preliminary reply */
				sprintf (debug_buff, "%s: %s\n", progname, data + 4);
				debug_print(debug_buff);
				/* keep going */
				done = False;
			} else if (!strncmp ((char *)data, "200", 3)) {
				result = 0;
				done = True;
			} else if (*data == '2') {
				sprintf (debug_buff, "%s: %s\n", progname, data + 4);
				debug_print(debug_buff);
				result = 0;
				done = True;
			} else if (*data == '3') {
				sprintf (debug_buff, "%s: internal error: "
					 "server wants more information?  (%s)\n",
					 progname, data);
				debug_print(debug_buff);
				result = 3;
				done = True;
			} else if (*data == '4' || *data == '5') {
				sprintf (debug_buff, "%s: %s\n", progname, data + 4);
				debug_print(debug_buff);
				result = (*data - '0');
				done = True;
			} else {
				sprintf (debug_buff,
					 "%s: unrecognised " MOZILLA_RESPONSE_PROP
					 " from window 0x%x: %s\n",
					 progname, (unsigned int) window, data);
				debug_print(debug_buff);
				result = 6;
				done = True;
			}

			if (data)
				g_free(data);
		}
		else if (event->type == GDK_PROPERTY_NOTIFY &&
                         event->property.window == window &&
                         event->property.state == GDK_PROPERTY_DELETE &&
                         event->property.atom == XA_MOZILLA_COMMAND) {
                        sprintf (debug_buff, "%s: (server 0x%x has accepted "
                                 MOZILLA_COMMAND_PROP ".)\n",
                                 progname, (unsigned int) window);
						debug_print(debug_buff);
		}
		gdk_event_free(event);
	}

DONE:

	if (new_command)
		g_free (new_command);

	return result;
}


gint check_netscape(char *msg)
{
        int status;
        GdkWindow *window;

        mozilla_remote_init_atoms ();
        window = mozilla_remote_find_window();

        if (window && (((GdkWindowPrivate *)window)->destroyed == FALSE)) {

		XSelectInput(gdk_display, ((GdkWindowPrivate *)window)->xwindow,
			     (PropertyChangeMask|StructureNotifyMask));

		
                mozilla_remote_obtain_lock(window);

                status = mozilla_remote_command(window, msg, False);

                if (status != 6)
                        mozilla_remote_free_lock(window);

                gtk_timeout_add(1000, (GtkFunction)clean_pid, NULL);

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

	if (window && (((GdkWindowPrivate *)window)->destroyed == FALSE)) {

		XSelectInput(gdk_display, ((GdkWindowPrivate *)window)->xwindow,
			     (PropertyChangeMask|StructureNotifyMask));

		mozilla_remote_obtain_lock(window);

		status = mozilla_remote_command(window, command, False);

		if (status != 6)
			mozilla_remote_free_lock(window);

		netscape_lock = 0;
		
		gdk_window_destroy (window);
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
			gtk_timeout_add(200, (GtkFunction)check_netscape, tmp);
		}
	}

}

void open_url(GtkWidget *w, char *url) {

	if (web_browser == BROWSER_NETSCAPE) {
                char *command = g_malloc(1024);

		g_snprintf(command, 1024, "OpenURL(%s)", url);

		netscape_command(command);
		g_free(command);
	} else if (web_browser == BROWSER_KFM) {
		pid_t pid;

		pid = fork();

		if (pid == 0) {
			char *args[4];

			args[0] = g_strdup("kfmclient");
			args[1] = g_strdup("openURL");
                        args[2] = url;;
			args[3] = NULL;

			execvp(args[0], args);
			_exit(0);
		} else {
			gtk_timeout_add(1000, (GtkFunction)clean_pid, NULL);
		}
#ifdef USE_GNOME
	} else if (web_browser == BROWSER_GNOME) {
		gnome_url_show(url);
#endif /* USE_GNOME */
	} else if (web_browser == BROWSER_MANUAL) {
		pid_t pid;

		pid = fork();

		if (pid == 0) {
			char *args[4];

			char command[1024];

			g_snprintf(command, sizeof(command), web_command, url);

			args[0] = "sh";
			args[1] = "-c";
			args[2] = command;
			args[3] = NULL;

                        execvp(args[0], args);

			_exit(0);
		} else {
			gtk_timeout_add(1000, (GtkFunction)clean_pid, NULL);
		}
        }
}

void add_bookmark(GtkWidget *w, char *url) {
	if (web_browser == BROWSER_NETSCAPE) {
                char *command = g_malloc(1024);

		g_snprintf(command, 1024, "AddBookmark(%s)", url);

		netscape_command(command);
		g_free(command);
	}
}

void open_url_nw(GtkWidget *w, char *url) {
	if (web_browser == BROWSER_NETSCAPE) {
                char *command = g_malloc(1024);

		g_snprintf(command, 1024, "OpenURL(%s, new-window)", url);

		netscape_command(command);
		g_free(command);
	} else {
		open_url(w, url);
	}
}

#else

/* Sooner or later, I shall support Windows clicking! */

void add_bookmark(GtkWidget *w, char *url) { }
void open_url_nw(GtkWidget *w, char *url) { }
void open_url(GtkWidget *w, char *url) { }


#endif _WIN32
