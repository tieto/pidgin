/**
 * @file switchboard.h MSN switchboard functions
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _MSN_SWITCHBOARD_H_
#define _MSN_SWITCHBOARD_H_

struct msn_switchboard {
	struct gaim_connection *gc;
	struct gaim_conversation *chat;
	int fd;
	int inpa;

	char *rxqueue;
	int rxlen;
	gboolean msg;
	char *msguser;
	int msglen;

	char *sessid;
	char *auth;
	guint32 trId;
	int total;
	char *user;
	GSList *txqueue;
};

/**
 * Finds a switch with the given username.
 *
 * @param gc       The gaim connection.
 * @param username The username to search for.
 *
 * @return The switchboard, if found.
 */
struct msn_switchboard *msn_find_switch(struct gaim_connection *gc,
										const char *username);

/**
 * Finds a switchboard with the given chat ID.
 *
 * @param gc      The gaim connection.
 * @param chat_id The chat ID to search for.
 *
 * @return The switchboard, if found.
 */
struct msn_switchboard *msn_find_switch_by_id(struct gaim_connection *gc,
											  int chat_id);

/**
 * Finds the first writable switchboard.
 *
 * @param gc The gaim connection.
 *
 * @return The first writable switchboard, if found.
 */
struct msn_switchboard *msn_find_writable_switch(struct gaim_connection *gc);

/**
 * Connects to a switchboard.
 *
 * @param gc   The gaim connection.
 * @param host The hostname.
 * @param port The port.
 *
 * @return The new switchboard.
 */
struct msn_switchboard *msn_switchboard_connect(struct gaim_connection *gc,
												const char *host, int port);

/**
 * Kills a switchboard.
 *
 * @param ms The switchboard to kill.
 */
void msn_kill_switch(struct msn_switchboard *ms);

void msn_rng_connect(gpointer data, gint source, GaimInputCondition cond);

#endif /* _MSN_SWITCHBOARD_H_ */
