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
 */

#ifndef _UI_H_
#define _UI_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#ifdef USE_APPLET
#include <applet-widget.h>
#endif /* USE_APPLET */
#ifdef USE_GNOME
#include <gnome.h>
#endif
#if USE_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#define FACE_ANGEL 0
#define FACE_BIGSMILE 1
#define FACE_BURP 2
#define FACE_CROSSEDLIPS 3
#define FACE_CRY 4
#define FACE_EMBARRASSED 5
#define FACE_KISS 6
#define FACE_MONEYMOUTH 7
#define FACE_SAD 8
#define FACE_SCREAM 9
#define FACE_SMILE 10
#define FACE_SMILE8 11
#define FACE_THINK 12
#define FACE_TONGUE 13
#define FACE_WINK 14
#define FACE_YELL 15
#define FACE_TOTAL 16

struct debug_window {
	GtkWidget *window;
	GtkWidget *entry;
};
extern struct debug_window *dw;

/* CUI: save_pos and window_size are used by gaimrc.c which is core.
 * Need to figure out options saving. Same goes for several global variables as well. */
struct save_pos {
        int x;
        int y;
        int width;
        int height;
	int xoff;
	int yoff;
};

struct window_size {
	int width;
	int height;
	int entry_height;
};

#define EDIT_GC    0
#define EDIT_GROUP 1
#define EDIT_BUDDY 2

/* Globals in applet.c */
#ifdef USE_APPLET
extern GtkWidget *applet;
#endif /* USE_APPLET */

/* Globals in buddy.c */
extern GtkWidget *buddies;
extern GtkWidget *bpmenu;
extern GtkWidget *blist;

/* Globals in buddy_chat.c */
/* it is very important that you don't use this for anything.
 * its sole purpose is to allow all group chats to be in one
 * window. use struct gaim_connection's buddy_chats instead. */
extern GList *chats;
/* these are ok to use */
extern GtkWidget *all_chats;
extern GtkWidget *chat_notebook;
extern GtkWidget *joinchat;

/* Globals in dialog.c */
extern char fontface[64];
extern int fontsize;
extern GdkColor bgcolor;
extern GdkColor fgcolor;
extern int smiley_array[FACE_TOTAL];

/* Globals in prpl.c */
extern GtkWidget *protomenu;

/* Functions in about.c */
extern void show_about(GtkWidget *, void *);
extern void gaim_help(GtkWidget *, void *);

#endif /* _UI_H_ */
