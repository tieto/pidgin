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

/* this file should be all that prpls need to include. therefore, by including
 * this file, they should get glib, proxy, gaim_connection, prpl, etc. */

#ifndef _PRPL_H_
#define _PRPL_H_

#include "core.h"
#include "proxy.h"
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
#define PROTO_ZEPHYR   10
#define PROTO_GADUGADU 11
#define PROTO_SAMETIME 12
#define PROTO_TLEN     13
#define PROTO_RVP      14
#define PROTO_BACKRUB  15
#define PROTO_UNTAKEN  16

/* DON'T TAKE AN UNASSIGNED NUMBER! Talk to Rob or Sean if you'd like
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
/* #define OPT_PROTO_HTML            0x00000001 this should be per-connection */
/* TOC/Oscar have signon time, and the server's time needs to be adjusted to match
 * your computer's time. We wouldn't need this if everyone used NTP. */
#define OPT_PROTO_CORRECT_TIME      0x00000002
/* Jabber lets you choose what name you want for chat. So it shouldn't be pulling
 * the alias for when you're in chat; it gets annoying. */
#define OPT_PROTO_UNIQUE_CHATNAME   0x00000004
/* IRC, Jabber let you have chat room topics */
#define OPT_PROTO_CHAT_TOPIC        0x00000008
/* Zephyr doesn't require passwords, so there's no need for a password prompt */
#define OPT_PROTO_NO_PASSWORD       0x00000010
/* MSN and Yahoo notify you when you have new mail */
#define OPT_PROTO_MAIL_CHECK        0x00000020
/* Oscar and Jabber have buddy icons */
#define OPT_PROTO_BUDDY_ICON        0x00000040
/* Oscar lets you send images in direct IMs */
#define OPT_PROTO_IM_IMAGE          0x00000080
/* Passwords in IRC are optional, and are needed for certain functionality. */
#define OPT_PROTO_PASSWORD_OPTIONAL 0x00000100

#define GAIM_AWAY_CUSTOM "Custom"

typedef void (*proto_init)(struct prpl *);

struct file_transfer;

struct prpl {
	int protocol;
	int options;
	struct gaim_plugin *plug;
	char *name;

	/* for ICQ and Yahoo, who have off/on per-conversation options */
	/* char *checkbox; this should be per-connection */

	/* returns the XPM associated with the given user class */
	char **(* list_icon)(int);
	GList *(* away_states)(struct gaim_connection *gc);
	GList *(* actions)();
	void   (* do_action)(struct gaim_connection *, char *);
	/* user_opts is a GList* of g_malloc'd struct proto_user_opts */
	GList *user_opts;
	GList *(* buddy_menu)(struct gaim_connection *, char *);
	GList *(* edit_buddy_menu)(struct gaim_connection *, char *);
	GList *(* chat_info)(struct gaim_connection *);

	/* all the server-related functions */

	/* a lot of these (like get_dir) are protocol-dependent and should be removed. ones like
	 * set_dir (which is also protocol-dependent) can stay though because there's a dialog
	 * (i.e. the prpl says you can set your dir info, the ui shows a dialog and needs to call
	 * set_dir in order to set it) */

	void (* login)		(struct aim_user *);
	void (* close)		(struct gaim_connection *);
	int  (* send_im)	(struct gaim_connection *, char *who, char *message, int len, int away);
	void (* set_info)	(struct gaim_connection *, char *info);
	int  (* send_typing)    (struct gaim_connection *, char *name, int typing);
	void (* get_info)	(struct gaim_connection *, char *who);
	void (* set_away)	(struct gaim_connection *, char *state, char *message);
	void (* get_away)       (struct gaim_connection *, char *who);
	void (* set_dir)	(struct gaim_connection *, const char *first,
							   const char *middle,
							   const char *last,
							   const char *maiden,
							   const char *city,
							   const char *state,
							   const char *country,
							   int web);
	void (* get_dir)	(struct gaim_connection *, char *who);
	void (* dir_search)	(struct gaim_connection *, const char *first,
							   const char *middle,
							   const char *last,
							   const char *maiden,
							   const char *city,
							   const char *state,
							   const char *country,
							   const char *email);
	void (* set_idle)	(struct gaim_connection *, int idletime);
	void (* change_passwd)	(struct gaim_connection *, const char *old, const char *new);
	void (* add_buddy)	(struct gaim_connection *, const char *name);
	void (* add_buddies)	(struct gaim_connection *, GList *buddies);
	void (* remove_buddy)	(struct gaim_connection *, char *name, char *group);
	void (* remove_buddies)	(struct gaim_connection *, GList *buddies, const char *group);
	void (* add_permit)	(struct gaim_connection *, char *name);
	void (* add_deny)	(struct gaim_connection *, char *name);
	void (* rem_permit)	(struct gaim_connection *, char *name);
	void (* rem_deny)	(struct gaim_connection *, char *name);
	void (* set_permit_deny)(struct gaim_connection *);
	void (* warn)		(struct gaim_connection *, char *who, int anonymous);
	void (* join_chat)	(struct gaim_connection *, GList *data);
	void (* chat_invite)	(struct gaim_connection *, int id, const char *who, const char *message);
	void (* chat_leave)	(struct gaim_connection *, int id);
	void (* chat_whisper)	(struct gaim_connection *, int id, char *who, char *message);
	int  (* chat_send)	(struct gaim_connection *, int id, char *message);
	void (* keepalive)	(struct gaim_connection *);

	/* new user registration */
	void (* register_user)	(struct aim_user *);

	/* get "chat buddy" info and away message */
	void (* get_cb_info)	(struct gaim_connection *, int, char *who);
	void (* get_cb_away)	(struct gaim_connection *, int, char *who);

	/* save/store buddy's alias on server list/roster */
	void (* alias_buddy)	(struct gaim_connection *, const char *who, const char *alias);

	/* change a buddy's group on a server list/roster */
	void (* group_buddy)	(struct gaim_connection *, const char *who, const char *old_group, const char *new_group);

	/* rename a group on a server list/roster */
	void (* rename_group)	(struct gaim_connection *, const char *old_group, const char *new_group, GList *members);

	void (* buddy_free)	(struct buddy *);

	/* this is really bad. */
	void (* convo_closed)   (struct gaim_connection *, char *who);

	char *(* normalize)(const char *);

	/* transfer files */
	void (* file_transfer_cancel)	 (struct gaim_connection *, struct file_transfer *);
	void (* file_transfer_in)	 (struct gaim_connection *, struct file_transfer *, int);
	void (* file_transfer_out)	 (struct gaim_connection *, struct file_transfer *, const char *, int, int);
	void (* file_transfer_nextfile)	 (struct gaim_connection *, struct file_transfer *);
	void (* file_transfer_data_chunk)(struct gaim_connection *, struct file_transfer *, const char *, int);
	void (* file_transfer_done)	 (struct gaim_connection *, struct file_transfer *);
	size_t (* file_transfer_read) (struct gaim_connection *, struct file_transfer *, int fd, char **buf);
	size_t (* file_transfer_write) (struct gaim_connection *, struct file_transfer *, int fd, const char *buf, size_t size);
};

extern GSList *protocols;
extern int prpl_accounts[];

/* this is mostly just for aim.c, when it initializes the protocols */
extern void static_proto_init();

/* this is what should actually load the protocol. pass it the protocol's initializer */
extern gboolean load_prpl(struct prpl *);
extern void load_protocol(proto_init);
extern void unload_protocol(struct prpl *);
extern gint proto_compare(struct prpl *, struct prpl *);

extern struct prpl *find_prpl(int);
extern void do_proto_menu();

extern void show_got_added(struct gaim_connection *, const char *,
			   const char *, const char *, const char *);

extern void do_ask_cancel_by_handle(GModule *);
extern void do_ask_dialog(const char *, const char *, void *, char*, void *, char *, void *, GModule *, gboolean);
extern void do_prompt_dialog(const char *, const char *, void *, void *, void *);

extern void connection_has_mail(struct gaim_connection *, int, const char *, const char *, const char *);

extern void set_icon_data(struct gaim_connection *, char *, void *, int);
extern void *get_icon_data(struct gaim_connection *, char *, int *);

/* file transfer stuff */
extern struct file_transfer *transfer_in_add(struct gaim_connection *gc,
		const char *who, const char *filename, int totsize,
		int totfiles, const char *msg);
extern struct file_transfer *transfer_out_add(struct gaim_connection *gc,
		const char *who);
extern int transfer_abort(struct file_transfer *xfer, const char *why);
extern int transfer_out_do(struct file_transfer *xfer, int fd,
		int offset);
extern int transfer_in_do(struct file_transfer *xfer, int fd,
		const char *filename, int size);
int transfer_get_file_info(struct file_transfer *xfer, int *size,
		char **name);

/* stuff to load/unload PRPLs as necessary */
extern gboolean ref_protocol(struct prpl *p);
extern void unref_protocol(struct prpl *p);

#endif /* _PRPL_H_ */
