/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 * ----------------
 * The Plug-in plug
 *
 * Plugin support is currently being maintained by Mike Saraf
 * msaraf@dwc.edu
 *
 */

#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"

/* ------------------ Local Variables -------------------------*/

static GtkWidget *plugins=NULL;

/* --------------- Function Declarations -------------------- */

static void destroy_plugins();
       void do_plugins(GtkWidget *, void *);
/* ------------------ Code Below ---------------------------- */

void show_plugins()
{
 char *buf = g_malloc(BUF_LEN);
 
 if (!plugins) 
    {
     plugins = gtk_file_selection_new("Gaim - Plugin List");

     gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(plugins));

     if(getenv("PLUGIN_DIR") == NULL)
        {
         g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("HOME"));
        }
     else
        {
         g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("PLUGIN_DIR"));
        }

     gtk_file_selection_set_filename(GTK_FILE_SELECTION(plugins), buf);
     gtk_signal_connect(GTK_OBJECT(plugins), "destroy",
                        GTK_SIGNAL_FUNC(destroy_plugins), plugins);

    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugins)->ok_button),
                       "clicked", GTK_SIGNAL_FUNC(do_plugins), NULL);
    
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(plugins)->cancel_button),
                       "clicked", GTK_SIGNAL_FUNC(destroy_plugins),plugins );

   }

 gtk_widget_show(plugins);
 gdk_window_raise(plugins->window);   
}

void do_plugins(GtkWidget *w, void *dummy)
{
}

static void destroy_plugins()
{
 if (plugins)
    gtk_widget_destroy(plugins);

 plugins = NULL;
}                  
