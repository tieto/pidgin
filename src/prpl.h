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

#ifndef _GAIMPRPL_H_
#define _GAIMPRPL_H_

#include "multi.h"

#define PROTO_TOC	0
#define PROTO_OSCAR	1
#define PROTO_YAHOO	2
#define PROTO_ICQ	3
#define PROTO_MSN	4
#define PROTO_IRC	5
#define PROTO_FTP	6
#define PROTO_VGATE	7
#define PROTO_JABBER	8
#define PROTO_NAPSTER	9

#define OPT_PROTO_HTML  0x00000001
/* there should be more here eventually... These should all be stuff that other
 * plugins can't do (for example, TOC and Oscar and Jabber can do HTML in messages,
 * but IRC etc can't, so TOC/Oscar/Jabber have _HTML set but not IRC. */

typedef void (*proto_init)(struct prpl *);

struct prpl {
	int protocol;
	int options;
	char *(* name)();

	/* returns the XPM associated with the given user class */
	char **(* list_icon)(int);

	/* when UI plugins come, these will have to be reconciled by returning
	 * structs indicating what kinds of information they want displayed. */
	void (* action_menu)(GtkWidget *, struct gaim_connection *, char *);
	void (* user_opts)(GtkWidget *, struct aim_user *);
	void (* draw_new_user)(GtkWidget *);
	void (* do_new_user)();

	/* all the server-related functions */
	void (* login)		(struct aim_user *);
	void (* close)		(struct gaim_connection *);
	void (* send_im)	(struct gaim_connection *, char *who, char *message, int away);
	void (* set_info)	(struct gaim_connection *, char *info);
	void (* get_info)	(struct gaim_connection *, char *who);
	void (* set_away)	(struct gaim_connection *, char *message);
	void (* get_away_msg)	(struct gaim_connection *, char *who);
	void (* set_dir)	(struct gaim_connection *, char *first,
							   char *middle,
							   char *last,
							   char *maiden,
							   char *city,
							   char *state,
							   char *country,
							   int web);
	void (* get_dir)	(struct gaim_connection *, char *who);
	void (* dir_search)	(struct gaim_connection *, char *first,
							   char *middle,
							   char *last,
							   char *maiden,
							   char *city,
							   char *state,
							   char *country,
							   char *email);
	void (* set_idle)	(struct gaim_connection *, int idletime);
	void (* change_passwd)	(struct gaim_connection *, char *old, char *new);
	void (* add_buddy)	(struct gaim_connection *, char *name);
	void (* add_buddies)	(struct gaim_connection *, GList *buddies);
	void (* remove_buddy)	(struct gaim_connection *, char *name);
	void (* add_permit)	(struct gaim_connection *, char *name);
	void (* add_deny)	(struct gaim_connection *, char *name);
	void (* rem_permit)	(struct gaim_connection *, char *name);
	void (* rem_deny)	(struct gaim_connection *, char *name);
	void (* set_permit_deny)(struct gaim_connection *);
	void (* warn)		(struct gaim_connection *, char *who, int anonymous);
	void (* accept_chat)	(struct gaim_connection *, int id);
	void (* join_chat)	(struct gaim_connection *, int id, char *name);
	void (* chat_invite)	(struct gaim_connection *, int id, char *who, char *message);
	void (* chat_leave)	(struct gaim_connection *, int id);
	void (* chat_whisper)	(struct gaim_connection *, int id, char *who, char *message);
	void (* chat_send)	(struct gaim_connection *, int id, char *message);
	void (* keepalive)	(struct gaim_connection *);
};

extern GSList *protocols;

/* this is mostly just for aim.c, when it initializes the protocols */
void static_proto_init();

/* this is what should actually load the protocol. pass it the protocol's initializer */
void load_protocol(proto_init);
void unload_protocol(struct prpl *);

struct prpl *find_prpl(int);

void register_user(gpointer, gpointer);
void prepare_regbox_for_next();

void do_ask_dialog(const char *, void *, void *, void *);
#endif
