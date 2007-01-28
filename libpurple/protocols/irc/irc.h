/**
 * @file irc.h
 *
 * gaim
 *
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
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

#ifndef _GAIM_IRC_H
#define _GAIM_IRC_H

#include <glib.h>

#include "circbuffer.h"
#include "ft.h"
#include "roomlist.h"
#include "sslconn.h"

#define IRC_DEFAULT_SERVER "irc.freenode.net"
#define IRC_DEFAULT_PORT 6667
#define IRC_DEFAULT_SSL_PORT 994

#define IRC_DEFAULT_CHARSET "UTF-8"
#define IRC_DEFAULT_ALIAS "gaim"

#define IRC_DEFAULT_QUIT "Leaving."

#define IRC_INITIAL_BUFSIZE 1024


enum { IRC_USEROPT_SERVER, IRC_USEROPT_PORT, IRC_USEROPT_CHARSET };
enum irc_state { IRC_STATE_NEW, IRC_STATE_ESTABLISHED };

struct irc_conn {
	GaimAccount *account;
	GHashTable *msgs;
	GHashTable *cmds;
	char *server;
	int fd;
	guint timer;
	GHashTable *buddies;

	char *inbuf;
	int inbuflen;
	int inbufused;

	GString *motd;
	GString *names;
	char *nameconv;
	struct _whois {
		char *nick;
		char *away;
		char *userhost;
		char *name;
		char *server;
		char *serverinfo;
		char *channels;
		int ircop;
		int identified;
		int idle;
		time_t signon;
	} whois;
	GaimRoomlist *roomlist;
	GaimSslConnection *gsc;

	gboolean quitting;

	GaimCircBuffer *outbuf;
	guint writeh;

	time_t recv_time;
};

struct irc_buddy {
	char *name;
	gboolean online;
	gboolean flag;
};

typedef int (*IRCCmdCallback) (struct irc_conn *irc, const char *cmd, const char *target, const char **args);

int irc_send(struct irc_conn *irc, const char *buf);
gboolean irc_blist_timeout(struct irc_conn *irc);

char *irc_mirc2html(const char *string);
char *irc_mirc2txt(const char *string);

gboolean irc_ischannel(const char *string);

void irc_register_commands(void);
void irc_msg_table_build(struct irc_conn *irc);
void irc_parse_msg(struct irc_conn *irc, char *input);
char *irc_parse_ctcp(struct irc_conn *irc, const char *from, const char *to, const char *msg, int notice);
char *irc_format(struct irc_conn *irc, const char *format, ...);

void irc_msg_default(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_away(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_badmode(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_badnick(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_banfull(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_banned(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_chanmode(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_endwhois(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_endmotd(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_features(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_invite(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_inviteonly(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_ison(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_join(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_kick(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_list(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_mode(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_motd(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_names(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nick(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nickused(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nochan(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nonick(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nochangenick(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_nosend(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_notice(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_notinchan(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_notop(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_part(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_ping(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_pong(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_privmsg(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_regonly(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_quit(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_time(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_topic(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_unavailable(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_unknown(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_wallops(struct irc_conn *irc, const char *name, const char *from, char **args);
void irc_msg_whois(struct irc_conn *irc, const char *name, const char *from, char **args);

void irc_msg_ignore(struct irc_conn *irc, const char *name, const char *from, char **args);

void irc_cmd_table_build(struct irc_conn *irc);

int irc_cmd_default(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_away(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_ctcp_action(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_ctcp_version(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_invite(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_join(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_kick(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_list(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_mode(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_names(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_nick(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_op(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_privmsg(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_part(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_ping(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_quit(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_quote(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_query(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_remove(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_service(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_time(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_topic(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_wallops(struct irc_conn *irc, const char *cmd, const char *target, const char **args);
int irc_cmd_whois(struct irc_conn *irc, const char *cmd, const char *target, const char **args);

GaimXfer *irc_dccsend_new_xfer(GaimConnection *gc, const char *who);
void irc_dccsend_send_file(GaimConnection *gc, const char *who, const char *file);
void irc_dccsend_recv(struct irc_conn *irc, const char *from, const char *msg);
#endif /* _GAIM_IRC_H */
