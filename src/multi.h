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

#ifndef _MULTI_H_
#define _MULTI_H_

#include "core.h"

/* ok. now the fun begins. first we create a connection structure */
struct gaim_connection {
	int edittype; /* XXX CUI: this is ui-specific and should be removed */

	/* we need to do either oscar or TOC */
	/* we make this as an int in case if we want to add more protocols later */
	int protocol;
	struct prpl *prpl;
	guint32 flags;

	/* erg. */
	char *checkbox;

	/* all connections need an input watcher */
	int inpa;

	/* all connections need a list of chats, even if they don't have chat */
	GSList *buddy_chats;

	/* each connection then can have its own protocol-specific data */
	void *proto_data;

	struct gaim_account *account;

	char username[64];
	char displayname[128];
	char password[32];
	guint keepalive;

	/* stuff needed for per-connection idle times */
	guint idle_timer;
	time_t login_time;
	time_t login_time_official;
	time_t lastsent;
	int is_idle;

	char *away;		/* set by protos, is NULL when not away, or set *
				 * to "" or a custom message when away */
	char *away_state;	/* updated by serv_set_away, keeps the last set *
				 * away type */
	int is_auto_away;	/* used by idle.c */

	int evil;		/* warning level for AIM (why is this here?) */
	gboolean wants_to_die;	/* defaults to FALSE */
};

#define OPT_CONN_HTML		0x00000001
/* set this flag on a gc if you want serv_got_im to autoreply when away */
#define OPT_CONN_AUTO_RESP	0x00000002

struct proto_user_split {
	char sep;
	char *label;
	char *def;
};

struct proto_user_opt {
	char *label;
	char *def;
	int pos;
};

struct proto_actions_menu {
	char *label;
	void (*callback)(struct gaim_connection *);
	struct gaim_connection *gc;
};

struct proto_buddy_menu {
	char *label;
	void (*callback)(struct gaim_connection *, const char *);
	struct gaim_connection *gc;
};

struct proto_chat_entry {
	char *label;
	char *def;
	gboolean is_int;
	int min;
	int max;
};

/* now that we have our struct, we're going to need lots of them. Maybe even a list of them. */
extern GSList *connections;

/* number of accounts that are currently in the process of connecting */
extern int connecting_count;

struct gaim_account *gaim_account_new(const char *, int, int);
struct gaim_connection *new_gaim_conn(struct gaim_account *);
void destroy_gaim_conn(struct gaim_connection *);

void regenerate_user_list();

void account_online(struct gaim_connection *);
void account_offline(struct gaim_connection *);

void auto_login();

void set_login_progress(struct gaim_connection *, float, char *);
void hide_login_progress(struct gaim_connection *, char *);
void hide_login_progress_notice(struct gaim_connection *, char *);
void hide_login_progress_error(struct gaim_connection *, char *);

#endif /* _MULTI_H_ */
