/**
 * @file away.h Away API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#if 0

#ifndef _GAIM_AWAY_H_
#define _GAIM_AWAY_H_

#include "gtkgaim.h"

/* XXX CUI: away messages aren't really anything more than char* but we need two char*'s
 * for the UI so that people can name their away messages when they save them. So these
 * are really a UI function and struct away_message should be removed from the core. */
/* WTF?  How does having a title for something mean that it is part of the UI? */
//if 0 /* XXX CUI */
struct away_message {
	char name[80];
	char message[2048];
};

extern GSList *away_messages;
extern struct away_message *awaymessage;
extern GtkWidget *awaymenu;
extern GtkWidget *awayqueue;
extern GtkListStore *awayqueuestore;

extern void rem_away_mess(GtkWidget *, struct away_message *);
extern void do_away_message(GtkWidget *, struct away_message *);
extern void do_away_menu();
extern void toggle_away_queue();
extern void purge_away_queue(GSList **);
extern void do_im_back(GtkWidget *, GtkWidget *);
void create_away_mess(GtkWidget *, void *);
#endif

#endif /* _GAIM_AWAY_H_ */
