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
#define PROTO_GADUGADU 11
/* DON'T TAKE AN UNASSIGNED NUMBER! Talk to Eric or Rob if you'd like
 * to create a new PRPL. */

#define PRPL_DESC(x)	"Allows gaim to use the " x " protocol.\n\n" \
			"Now that you have loaded this protocol, use the " \
			"Account Editor to add an account that uses this " \
			"protocol. You can access the Account Editor from " \
			"the \"Accounts\" button on the login window or " \
			"in the \"Tools\" menu in the buddy list window."

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
/* MSN and Yahoo notify you when you have new mail */
#define OPT_PROTO_MAIL_CHECK      0x00000020

#define GAIM_AWAY_CUSTOM "Custom"

typedef void (*proto_init)(struct prpl *);

struct prpl {
	int protocol;
	int options;
	char *(* name)();

	/* for ICQ and Yahoo, who have off/on per-conversation options */
	char *checkbox;

	/* returns the XPM associated with the given user class */
	char **(* list_icon)(int);
	GList *(* away_states)();
	GList *(* actions)();
	void   (* do_action)(struct gaim_connection *, char *);
	/* user_opts returns a GList* of g_malloc'd struct proto_user_opts */
	GList *(* user_opts)();
	GList *(* buddy_menu)(struct gaim_connection *, char *);
	GList *(* chat_info)(struct gaim_connection *);

	/* all the server-related functions */
	void (* login)		(struct aim_user *);
	void (* close)		(struct gaim_connection *);
	int  (* send_im)	(struct gaim_connection *, char *who, char *message, int away);
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
	void (* join_chat)	(struct gaim_connection *, GList *data);
	void (* chat_invite)	(struct gaim_connection *, int id, char *who, char *message);
	void (* chat_leave)	(struct gaim_connection *, int id);
	void (* chat_whisper)	(struct gaim_connection *, int id, char *who, char *message);
	int  (* chat_send)	(struct gaim_connection *, int id, char *message);
	void (* keepalive)	(struct gaim_connection *);

	void (* convo_closed)   (struct gaim_connection *, char *who);

	char *(* normalize)(const char *);
};

extern GSList *protocols;

/* this is mostly just for aim.c, when it initializes the protocols */
extern void static_proto_init();

/* this is what should actually load the protocol. pass it the protocol's initializer */
extern void load_protocol(proto_init, int);
extern void unload_protocol(struct prpl *);

extern struct prpl *find_prpl(int);
extern void do_proto_menu();

extern void do_ask_dialog(const char *, void *, void *, void *);
extern void do_prompt_dialog(const char *, void *, void *, void *);

extern void connection_has_mail(struct gaim_connection *, int, const char *, const char *);

extern void set_icon_data(struct gaim_connection *, char *, void *, int);
extern void *get_icon_data(struct gaim_connection *, char *, int *);

#endif
