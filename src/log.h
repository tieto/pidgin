/**
 * @file log.h Logging API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_LOG_H_
#define _GAIM_LOG_H_

#include "blist.h"
#include "connection.h"
#include "conversation.h"

/* XXX Temporary! */
#define OPT_LOG_BUDDY_SIGNON    0x00000004
#define OPT_LOG_BUDDY_IDLE		0x00000008
#define OPT_LOG_BUDDY_AWAY		0x00000010
#define OPT_LOG_MY_SIGNON		0x00000020


enum log_event {
	log_signon = 0,
	log_signoff,
	log_away,
	log_back,
	log_idle,
	log_unidle,
	log_quit
};

struct log_conversation {
	char name[80];
	char filename[512];
        struct log_conversation *next;
};

extern GList *log_conversations;

FILE *open_log_file (const char *, int);
void system_log(enum log_event, GaimConnection *, struct buddy *, int);
void rm_log(struct log_conversation *);
struct log_conversation *find_log_info(const char *);
void update_log_convs();
void save_convo(GtkWidget *save, GaimConversation *c);
char *html_logize(const char *p);

#endif /* _GAIM_LOG_H_ */
