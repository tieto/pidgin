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

#include "account.h"
#include "core.h"
#include "plugin.h"

#if 0
/* ok. now the fun begins. first we create a connection structure */
GaimConnection {
	int edittype; /* XXX CUI: this is ui-specific and should be removed */

	/* we need to do either oscar or TOC */
	/* we make this as an int in case if we want to add more protocols later */
	int protocol;
	GaimPlugin *prpl;
	guint32 flags;

	/* erg. */
	char *checkbox;

	/* all connections need an input watcher */
	int inpa;

	/* all connections need a list of chats, even if they don't have chat */
	GSList *buddy_chats;

	/* each connection then can have its own protocol-specific data */
	void *proto_data;

	GaimAccount *account;

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
#endif

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
	void (*callback)(GaimConnection *);
	GaimConnection *gc;
};

struct proto_buddy_menu {
	char *label;
	void (*callback)(GaimConnection *, const char *);
	GaimConnection *gc;
};

struct proto_chat_entry {
	char *label;
	char *identifier;
	char *def;
	gboolean is_int;
	int min;
	int max;
};

#endif /* _MULTI_H_ */
