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

/* ok. now the fun begins. first we create a connection structure */
struct gaim_connection {
	int edittype;

	/* we need to do either oscar or TOC */
	/* we make this as an int in case if we want to add more protocols later */
	int protocol;
	struct prpl *prpl;

	/* all connections need an input watcher */
	int inpa;

	/* buddy list stuff. there is still a global groups for the buddy list, but
	 * we need to maintain our own set of buddies, and our own permit/deny lists */
	GSList *groups;
	GSList *permit;
	GSList *deny;
	int permdeny;

	/* all connections need a list of chats, even if they don't have chat */
	GSList *buddy_chats;

	/* each connection then can have its own protocol-specific data */
	void *proto_data;

	struct aim_user *user;

	char username[64];
	char password[32];
	int options; /* same as aim_user options */
	guint keepalive;
	/* stuff needed for per-connection idle times */
	guint idle_timer;
	time_t login_time;
	time_t lastsent;
	int is_idle;
	time_t correction_time;

	/* stuff for a signin progress meter */
	GtkWidget *meter;
	GtkWidget *progress;
	GtkWidget *status;

	char *away;
	int is_auto_away;

	int evil;
	gboolean wants_to_die; /* defaults to FALSE */

	/* email notification (MSN and Yahoo) */
	GtkWidget *email_win;
	GtkWidget *email_label;

	/* buddy icon file */
	char *iconfile;
};

struct proto_user_opt {
	char *label;
	char *def;
	int pos;
};

struct proto_buddy_menu {
	char *label;
	void (*callback)(struct gaim_connection *, char *);
	struct gaim_connection *gc;
};

struct proto_chat_entry {
	char *label;
	gboolean is_int;
	int min;
	int max;
};

/* now that we have our struct, we're going to need lots of them. Maybe even a list of them. */
extern GSList *connections;

struct aim_user *new_user(const char *, int, int);
struct gaim_connection *new_gaim_conn(struct aim_user *);
void destroy_gaim_conn(struct gaim_connection *);

struct gaim_connection *find_gaim_conn_by_name(char *);

void account_editor(GtkWidget *, GtkWidget *);
void regenerate_user_list();

void account_online(struct gaim_connection *);
void account_offline(struct gaim_connection *);

void auto_login();

void set_login_progress(struct gaim_connection *, float, char *);
void hide_login_progress(struct gaim_connection *, char *);

#endif /* _GAIMMULTI_H_ */
