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

#ifndef _GAIMMULTI_H_
#define _GAIMMULTI_H_

#include <gtk/gtk.h>
#include "gaim.h"
#include "aim.h"

#define PROTO_TOC 0
#define PROTO_OSCAR 1

/* ok. now the fun begins. first we create a connection structure */
struct gaim_connection {
	/* we need to do either oscar or TOC */
	/* we make this as an int in case if we want to add more protocols later */
	int protocol;

	/* let's do the oscar-specific stuff first since i know it better */
	struct aim_session_t *oscar_sess;
	struct aim_conn_t *oscar_conn; /* we don't particularly need this since it
					  will be in oscar_sess, but it's useful to
					  still keep our own reference to it */
	int inpa; /* do we really need this? it's for the BOS conn */
	int cnpa; /* chat nav input watcher */
	int paspa; /* for changing passwords, which doesn't work yet */

	int create_exchange;
	char *create_name;

	GSList *oscar_chats;
	GSList *buddy_chats;

	/* that's all we need for oscar. now then, on to TOC.... */
	int toc_fd;
	int seqno;
	int state;
	/* int inpa; input watcher, dual-declared for oscar as well */

	/* now we'll do stuff that both of them need */
	char username[64];
	char password[32];
	char user_info[2048];
	char g_screenname[64];
	int options; /* same as aim_user options */
	int keepalive;
	/* stuff needed for per-connection idle times */
	int idle_timer;
	time_t login_time;
	time_t lastsent;
	int is_idle;
};

/* now that we have our struct, we're going to need lots of them. Maybe even a list of them. */
extern GSList *connections;

struct gaim_connection *new_gaim_conn(int, char *, char *);
void destroy_gaim_conn(struct gaim_connection *);

struct gaim_connection *find_gaim_conn_by_name(char *);

void account_editor(GtkWidget *, GtkWidget *);

void account_online(struct gaim_connection *);
void account_offline(struct gaim_connection *);

void auto_login();

#endif /* _GAIMMULTI_H_ */
