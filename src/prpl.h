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
#include <stdio.h>

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
#define PROTO_ZEPHYR   10

#define DEFAULT_PROTO   PROTO_OSCAR

/* These should all be stuff that some plugins can do and others can't */
/* TOC/Oscar send HTML-encoded messages; most other protocols don't */
#define OPT_PROTO_HTML            0x00000001
/* TOC/Oscar have signon time, and the server's time needs to be adjusted to match
 * your computer's time. We wouldn't need this if everyone used NTP. */
#define OPT_PROTO_CORRECT_TIME    0x00000002
/* Jabber lets you choose what name you want for chat. So it shouldn't be pulling
 * the alias for when you're in chat; it gets annoying. */
#define OPT_PROTO_UNIQUE_CHATNAME 0x00000004
/* IRC, Jabber let you have chat room topics */
#define OPT_PROTO_CHAT_TOPIC      0x00000008
/* IRC and Zephyr don't require passwords, so there's no need for a password prompt */
#define OPT_PROTO_NO_PASSWORD     0x00000010
/* ICQ, Yahoo, others? let you send offline messages */
#define OPT_PROTO_OFFLINE         0x00000020

#define GAIM_AWAY_CUSTOM "Custom"

typedef void (*proto_init)(struct prpl *);

struct prpl {
	int protocol;
	int options;
	char *(* name)();

	/* returns the XPM associated with the given user class */
	char **(* list_icon)(int);
	GList *(* away_states)();
	GList *(* actions)();
	void   (* do_action)(struct gaim_connection *, char *);

	/* when UI plugins come, these will have to be reconciled by returning
	 * structs indicating what kinds of information they want displayed. */
	/* new thought though. instead of UI plugins, just do like X-Chat does;
	 * have different src- dirs in src: src-common, src-gtk, src-cli, etc.
	 * then have a prpl-base and prpl-UI stuff. people don't need to change
	 * their UIs all that often anyway. */
	void (* buddy_menu)(GtkWidget *, struct gaim_connection *, char *);
	void (* user_opts)(GtkWidget *, struct aim_user *);
	void (* draw_new_user)(GtkWidget *);
	void (* do_new_user)();
	void (* draw_join_chat)(struct gaim_connection *, GtkWidget *);
	void (* insert_convo)(struct gaim_connection *, struct conversation *);
	void (* remove_convo)(struct gaim_connection *, struct conversation *);

	/* all the server-related functions */
	void (* login)		(struct aim_user *);
	void (* close)		(struct gaim_connection *);
	void (* send_im)	(struct gaim_connection *, char *who, char *message, int away);
	void (* set_info)	(struct gaim_connection *, char *info);
	void (* get_info)	(struct gaim_connection *, char *who);
	void (* set_away)	(struct gaim_connection *, char *state, char *message);
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
	void (* chat_set_topic) (struct gaim_connection *, int id, char *topic);

	char *(* normalize)(const char *);
};

extern GSList *protocols;

/* this is mostly just for aim.c, when it initializes the protocols */
void static_proto_init();

/* this is what should actually load the protocol. pass it the protocol's initializer */
void load_protocol(proto_init, int);
void unload_protocol(struct prpl *);

struct prpl *find_prpl(int);
void do_proto_menu();

void register_user(gpointer, gpointer);
void prepare_regbox_for_next();

void do_ask_dialog(const char *, void *, void *, void *);
void do_prompt_dialog(const char *, void *, void *, void *);

/* UI for file transfer */
#define FT_EXIST_DNE       0
#define FT_EXIST_OVERWRITE 1
#define FT_EXIST_RESUME    2
typedef void (*ft_callback)(struct gaim_connection *, const char *, gint, gpointer);

void ft_receive_request(struct gaim_connection *, const char *, gboolean, gboolean,
		char *, guint size, ft_callback, gpointer);
void ft_send_request(struct gaim_connection *, const char *, gboolean, char *, ft_callback, gpointer);
gpointer ft_meter(gpointer, const char *, gfloat);

#endif
