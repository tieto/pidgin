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

#ifndef _GAIMCONVO_H_
#define _GAIMCONVO_H_

#include <gtk/gtk.h>
#include "gaim.h"

#include "pixmaps/underline.xpm"
#include "pixmaps/bold.xpm"
#include "pixmaps/italic.xpm"
#include "pixmaps/small.xpm"
#include "pixmaps/normal.xpm"
#include "pixmaps/big.xpm"
#include "pixmaps/fontface.xpm"
#include "pixmaps/speaker.xpm"
/* #include "pixmaps/aimicon2.xpm" */
#include "pixmaps/wood.xpm"
#include "pixmaps/palette.xpm"
#include "pixmaps/link.xpm"
#include "pixmaps/strike.xpm"

#include "pixmaps/angel.xpm"
#include "pixmaps/bigsmile.xpm"
#include "pixmaps/burp.xpm"
#include "pixmaps/crossedlips.xpm"
#include "pixmaps/cry.xpm"
#include "pixmaps/embarrassed.xpm"
#include "pixmaps/kiss.xpm"
#include "pixmaps/moneymouth.xpm"
#include "pixmaps/sad.xpm"
#include "pixmaps/scream.xpm"
#include "pixmaps/smile.xpm"
#include "pixmaps/smile8.xpm"
#include "pixmaps/think.xpm"
#include "pixmaps/tongue.xpm"
#include "pixmaps/wink.xpm"
#include "pixmaps/yell.xpm"
#include "pixmaps/luke03.xpm"

#include "pixmaps/join.xpm"
#include "pixmaps/cancel.xpm"


/* we declare all of the global functions for chat and IM windows here, so
 * that it's easy to keep them merged. */

/* chat first */
extern void im_callback(GtkWidget *, struct conversation *);
extern void ignore_callback(GtkWidget *, struct conversation *);
extern void whisper_callback(GtkWidget *, struct conversation *);
extern void invite_callback(GtkWidget *, struct conversation *);

/* now IM */
extern void warn_callback(GtkWidget *, struct conversation *);
extern void block_callback(GtkWidget *, struct conversation *);
extern void add_callback(GtkWidget *, struct conversation *);

/* now both */
extern void info_callback(GtkWidget *, struct conversation *);
extern void do_bold(GtkWidget *, GtkWidget *);
extern void do_italic(GtkWidget *, GtkWidget *);
extern void do_underline(GtkWidget *, GtkWidget *);
extern void do_strike(GtkWidget *, GtkWidget *);
extern void do_small(GtkWidget *, GtkWidget *);
extern void do_normal(GtkWidget *, GtkWidget *);
extern void do_big(GtkWidget *, GtkWidget *);
extern void toggle_font(GtkWidget *, struct conversation *);
extern void do_link(GtkWidget *, GtkWidget *);
extern void toggle_color(GtkWidget *, struct conversation *);
extern void toggle_loggle(GtkWidget *, struct conversation *);
extern void insert_smiley(GtkWidget *, struct conversation *);
/* sound is handled by set_option */
extern gboolean keypress_callback(GtkWidget *, GdkEventKey *, struct conversation *);

extern GtkWidget *build_conv_toolbar(struct conversation *);

extern void send_callback(GtkWidget *, struct conversation *);
extern int close_callback(GtkWidget *, struct conversation *);

extern gboolean meify(char *);

#endif
