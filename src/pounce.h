/*
 * gaim
 *
 * Copyright (C) 1998-2003, Mark Spencer <markster@marko.net>
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

#ifndef _POUNCE_H_
#define _POUNCE_H_

struct buddy_pounce {
        char name[80];
        char message[2048];
        char command[2048];
        char sound[2048];
        
        char pouncer[80];
        int protocol;

        int options;
};

struct addbp {
	GtkWidget *window;
	GtkWidget *nameentry;
	GtkWidget *messentry;
	GtkWidget *commentry;
	GtkWidget *command;
	GtkWidget *sendim;
	GtkWidget *openwindow;
	GtkWidget *popupnotify;
	GtkWidget *p_signon;
	GtkWidget *p_unaway;
	GtkWidget *p_unidle;
	GtkWidget *p_typing;
	GtkWidget *save;
	GtkWidget *menu;
	GtkWidget *sound;
	GtkWidget *soundentry;

	struct gaim_account *account;
	struct buddy_pounce *buddy_pounce;
};

void rem_bp(GtkWidget *w, struct buddy_pounce *b);

void do_pounce(struct gaim_connection *gc, char *name, int when);

void do_bp_menu();

#endif /* _POUNCE_H_ */
