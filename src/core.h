/**
 * @file core.h Gaim Core
 * @defgroup core Gaim Core
 *
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
 */

#ifndef _CORE_H_
#define _CORE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <gmodule.h>

struct gaim_account;
struct group;
struct buddy;


#include "debug.h"
#include "multi.h"
#include "conversation.h"
#include "ft.h"
#include "privacy.h"
#include "plugin.h"
#include "event.h"
#include "notify.h"
#include "request.h"

/* Really user states are controlled by the PRPLs now. We just use this for event_away */
#define UC_UNAVAILABLE  1

/* This is far too long to be practical, but MSN users are probably used to long aliases */
#define SELF_ALIAS_LEN 400

struct gaim_account {
	char username[64];
	char alias[SELF_ALIAS_LEN]; 
	char password[32];
	char user_info[2048];
	int options;
	int protocol;
	/* prpls can use this to save information about the user,
	 * like which server to connect to, etc */
	char proto_opt[7][256];

	/* buddy icon file */
	char iconfile[256];

	struct gaim_proxy_info *gpi;

	struct gaim_connection *gc;
	gboolean connecting;

	GSList *permit;
	GSList *deny;
	int permdeny;
};

/* XXX Temporary, until we have better account-specific prefs. */
#define GAIM_ACCOUNT_CHECK_MAIL(account) \
	((account)->options & OPT_ACCT_MAIL_CHECK)

struct UI {
	GIOChannel *channel;
	guint inpa;
};

/* Globals in core.c */
extern GSList *uis;
extern int gaim_session;

/* Functions in core.c */
extern gint UI_write(struct UI *, guchar *, int);
extern void UI_build_write(struct UI *, guchar, guchar, ...);
extern void UI_broadcast(guchar *data, int);
extern void UI_build_broadcast(guchar, guchar, ...);
/* Don't ever use these; when gaim-core is done these will be
 * merged into the core's main() and won't be called directly */
extern int core_main();
extern void core_quit();

/* Functions in gaimrc.c */
extern void load_prefs();
extern void load_pounces();
extern void save_prefs();

/* Functions in server.c */
extern void serv_got_update(struct gaim_connection *, char *, int, int, time_t, time_t, int);
extern void serv_got_im(struct gaim_connection *, const char *, const char *, guint32, time_t, gint);
extern void serv_got_typing(struct gaim_connection *, char *, int, int);
extern void serv_got_typing_stopped(struct gaim_connection *, char *);
extern void serv_got_eviled(struct gaim_connection *, char *, int);
extern void serv_got_chat_invite(struct gaim_connection *, char *, char *, char *, GHashTable *);
extern struct gaim_conversation *serv_got_joined_chat(struct gaim_connection *, int, char *);
extern void serv_got_chat_left(struct gaim_connection *, int);
extern void serv_got_chat_in(struct gaim_connection *, int, char *, int, char *, time_t);
extern void serv_got_alias(struct gaim_connection *, char *, char *);
extern void serv_finish_login();

#endif /* _CORE_H_ */
