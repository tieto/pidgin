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
 * This was taken almost exactly from X-Chat. The power of the GPL.
 * Translated from X-Chat to Gaim by Eric Warmenhoven.
 * Originally by Erik Scrafford <eriks@chilisoft.com>.
 * X-Chat Copyright (C) 1998 Peter Zelezny.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#ifdef DEBUG
#undef DEBUG
#endif
#endif
#undef PACKAGE

#ifdef USE_PERL

#include <EXTERN.h>
#ifndef _SEM_SEMUN_UNDEFINED
#define HAS_UNION_SEMUN
#endif
#include <perl.h>
#include <XSUB.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#undef PACKAGE
#include <stdio.h>
#include <dirent.h>
#include <string.h>


/* perl module support */
extern void xs_init _((void));
extern void boot_DynaLoader _((CV * cv)); /* perl is so wacky */

#undef _
#ifdef DEBUG
#undef DEBUG
#endif
#include "gaim.h"
#include "prpl.h"

struct perlscript {
	char *name;
	char *version;
	char *shutdowncallback; /* bleh */
};

struct _perl_event_handlers {
	char *event_type;
	char *handler_name;
};

struct _perl_timeout_handlers {
	char *handler_name;
	char *handler_args;
	gint iotag;
};

static GList *perl_list = NULL; /* should probably extern this at some point */
static GList *perl_timeout_handlers = NULL;
static GList *perl_event_handlers = NULL;
static PerlInterpreter *my_perl = NULL;
static void perl_init();

/* dealing with gaim */
XS(XS_GAIM_register); /* set up hooks for script */
XS(XS_GAIM_get_info); /* version, last to attempt signon, protocol */
XS(XS_GAIM_print); /* lemme figure this one out... */
XS(XS_GAIM_write_to_conv); /* write into conversation window */

/* list stuff */
XS(XS_GAIM_buddy_list); /* all buddies */
XS(XS_GAIM_online_list); /* online buddies */

/* server stuff */
XS(XS_GAIM_command); /* send command to server */
XS(XS_GAIM_user_info); /* given name, return struct buddy members */
XS(XS_GAIM_print_to_conv); /* send message to someone */
XS(XS_GAIM_print_to_chat); /* send message to chat room */
XS(XS_GAIM_serv_send_im); /* send message to someone (but do not display) */

/* handler commands */
XS(XS_GAIM_add_event_handler); /* when servers talk */
XS(XS_GAIM_add_timeout_handler); /* figure it out */

void xs_init()
{
	char *file = __FILE__;
	newXS ("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

static char *escape_quotes(char *buf)
{
	static char *tmp_buf = NULL;
	char *i, *j;

	if (tmp_buf)
		g_free(tmp_buf);
	tmp_buf = g_malloc(strlen(buf) * 2 + 1);
	for (i = buf, j = tmp_buf; *i; i++, j++) {
		if (*i == '\'' || *i == '\\')
			*j++ = '\\';
		*j = *i;
	}
	*j = '\0';

	return (tmp_buf);
}

static SV *execute_perl(char *function, char *args)
{
	static char *perl_cmd = NULL;
	SV *i;

	if (perl_cmd)
		g_free(perl_cmd);
	perl_cmd = g_malloc(strlen(function) + strlen(args) + 4);
	sprintf(perl_cmd, "&%s(%s)", function, args);
#ifndef HAVE_PERL_EVAL_PV
	i = (perl_eval_pv(perl_cmd, TRUE));
#else
	i = (Perl_eval_pv(perl_cmd, TRUE));
#endif
	return i;
}

int perl_load_file(char *script_name)
{
	char *name = g_strdup_printf("'%s'", escape_quotes(script_name));
	SV *return_val;
	if (my_perl == NULL)
		perl_init();
	return_val = execute_perl("load_file", name);
	g_free(name);
	return SvNV (return_val);
}

static int is_pl_file(char *filename)
{
	int len;
	if (!filename) return 0;
	if (!filename[0]) return 0;
	len = strlen(filename);
	len -= 3;
	if (len < 0) return 0;
	return (!strncmp(filename + len, ".pl", 3));
}

void perl_autoload()
{
	DIR *dir;
	struct dirent *ent;
	char *buf;
	char *path;

	path = gaim_user_dir();
	dir = opendir(path);
	if (dir) {
		while ((ent = readdir(dir))) {
			if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
				if (is_pl_file(ent->d_name)) {
					buf = g_malloc(strlen(path) + strlen(ent->d_name) + 2);
					sprintf(buf, "%s/%s", path, ent->d_name);
					perl_load_file(buf);
					g_free(buf);
				}
			}
		}
		closedir(dir);
	}
	g_free(path);
}

static void perl_init()
{
	char *perl_args[] = {"", "-e", "0", "-w"};
	char load_file[] =
"sub load_file()\n"
"{\n"
"	(my $file_name) = @_;\n"
"	open FH, $file_name or return 2;\n"
"	my $is = $/;\n"
"	local($/) = undef;\n"
"	$file = <FH>;\n"
"	close FH;\n"
"	$/ = $is;\n"
"	$file = \"\\@ISA = qw(Exporter DynaLoader);\\n\" . $file;\n"
"	eval $file;\n"
"	eval $file if $@;\n"
"	return 1 if $@;\n"
"	return 0;\n"
"}";

	my_perl = perl_alloc();
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 4, perl_args, NULL);
#ifndef HAVE_PERL_EVAL_PV
	perl_eval_pv(load_file, TRUE);
#else
	Perl_eval_pv(load_file, TRUE);
#endif

	newXS ("GAIM::register", XS_GAIM_register, "GAIM");
	newXS ("GAIM::get_info", XS_GAIM_get_info, "GAIM");
	newXS ("GAIM::print", XS_GAIM_print, "GAIM");
	newXS ("GAIM::write_to_conv", XS_GAIM_write_to_conv, "GAIM");

	newXS ("GAIM::buddy_list", XS_GAIM_buddy_list, "GAIM");
	newXS ("GAIM::online_list", XS_GAIM_online_list, "GAIM");

	newXS ("GAIM::command", XS_GAIM_command, "GAIM");
	newXS ("GAIM::user_info", XS_GAIM_user_info, "GAIM");
	newXS ("GAIM::print_to_conv", XS_GAIM_print_to_conv, "GAIM");
	newXS ("GAIM::print_to_chat", XS_GAIM_print_to_chat, "GAIM");
	newXS ("GAIM::serv_send_im", XS_GAIM_serv_send_im, "GAIM");

	newXS ("GAIM::add_event_handler", XS_GAIM_add_event_handler, "GAIM");
	newXS ("GAIM::add_timeout_handler", XS_GAIM_add_timeout_handler, "GAIM");
}

void perl_end()
{
	struct perlscript *scp;
	struct _perl_timeout_handlers *thn;
	struct _perl_event_handlers *ehn;

	while (perl_list) {
		scp = perl_list->data;
		perl_list = g_list_remove(perl_list, scp);
		if (scp->shutdowncallback[0])
			execute_perl(scp->shutdowncallback, "");
		g_free(scp->name);
		g_free(scp->version);
		g_free(scp->shutdowncallback);
		g_free(scp);
	}

	while (perl_timeout_handlers) {
		thn = perl_timeout_handlers->data;
		perl_timeout_handlers = g_list_remove(perl_timeout_handlers, thn);
		g_source_remove(thn->iotag);
		g_free(thn->handler_args);
		g_free(thn->handler_name);
		g_free(thn);
	}

	while (perl_event_handlers) {
		ehn = perl_event_handlers->data;
		perl_event_handlers = g_list_remove(perl_event_handlers, ehn);
		g_free(ehn->event_type);
		g_free(ehn->handler_name);
		g_free(ehn);
	}

	if (my_perl != NULL) {
		perl_destruct(my_perl);
		perl_free(my_perl);
		my_perl = NULL;
	}
}

XS (XS_GAIM_register)
{
	char *name, *ver, *callback, *unused; /* exactly like X-Chat, eh? :) */
	unsigned int junk;
	struct perlscript *scp;
	dXSARGS;
	items = 0;

	name = SvPV (ST (0), junk);
	ver = SvPV (ST (1), junk);
	callback = SvPV (ST (2), junk);
	unused = SvPV (ST (3), junk);

	scp = g_new0(struct perlscript, 1);
	scp->name = g_strdup(name);
	scp->version = g_strdup(ver);
	scp->shutdowncallback = g_strdup(callback);
	perl_list = g_list_append(perl_list, scp);

	XST_mPV (0, VERSION);
	XSRETURN (1);
}

XS (XS_GAIM_get_info)
{
	int i = 0;
	dXSARGS;
	items = 0;

	switch(SvIV(ST(0))) {
	case 0:
		XST_mPV(0, VERSION);
		i = 1;
		break;
	case 1:
		{
			GSList *c = connections;
			struct gaim_connection *gc;

			while (c) {
				gc = (struct gaim_connection *)c->data;
				XST_mIV(i++, (guint)gc);
				c = c->next;
			}
		}
		break;
	case 2:
		{
			struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
			if (g_slist_find(connections, gc))
				XST_mIV(i++, gc->protocol);
			else
				XST_mIV(i++, -1);
		}
		break;
	case 3:
		{
			struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
			if (g_slist_find(connections, gc))
				XST_mPV(i++, gc->username);
			else
				XST_mPV(i++, "");
		}
		break;
	case 4:
		{
			struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
			if (g_slist_find(connections, gc))
				XST_mIV(i++, g_slist_index(aim_users, gc->user));
			else
				XST_mIV(i++, -1);
		}
		break;
	case 5:
		{
			GSList *a = aim_users;
			while (a) {
				struct aim_user *u = a->data;
				XST_mPV(i++, u->username);
				a = a->next;
			}
		}
		break;
	case 6:
		{
			GSList *a = aim_users;
			while (a) {
				struct aim_user *u = a->data;
				XST_mIV(i++, u->protocol);
				a = a->next;
			}
		}
		break;
	case 7:
		{
			struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
			if (g_slist_find(connections, gc))
				XST_mPV(i++, gc->prpl->name());
			else
				XST_mPV(i++, "Unknown");
		}
		break;
	default:
		XST_mPV(0, "Error2");
		i = 1;
	}

	XSRETURN(i);
}

XS (XS_GAIM_print)
{
	char *title;
	char *message;
	unsigned int junk;
	dXSARGS;
	items = 0;

	title = SvPV(ST(0), junk);
	message = SvPV(ST(1), junk);
	do_error_dialog(message, title);
	XSRETURN(0);
}

XS (XS_GAIM_buddy_list)
{
	struct gaim_connection *gc;
	struct buddy *buddy;
	struct group *g;
	GSList *list = NULL;
	GSList *mem;
	int i = 0;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	if (g_slist_find(connections, gc))
		list = gc->groups;

	while (list) {
		g = (struct group *)list->data;
		mem = g->members;
		while (mem) {
			buddy = (struct buddy *)mem->data;
			XST_mPV(i++, buddy->name);
			mem = mem->next;
		}
		list = g_slist_next(list);
	}
	XSRETURN(i);
}

XS (XS_GAIM_online_list)
{
	struct gaim_connection *gc;
	struct buddy *b;
	struct group *g;
	GSList *list = NULL;
	GSList *mem;
	int i = 0;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	if (g_slist_find(connections, gc))
		list = gc->groups;

	while (list) {
		g = (struct group *)list->data;
		mem = g->members;
		while (mem) {
			b = (struct buddy *)mem->data;
			if (b->present) XST_mPV(i++, b->name);
			mem = mem->next;
		}
		list = g_slist_next(list);
	}
	XSRETURN(i);
}

XS (XS_GAIM_command)
{
	unsigned int junk;
	char *command = NULL;
	dXSARGS;
	items = 0;

	command = SvPV(ST(0), junk);
	if (!command) XSRETURN(0);
	if        (!strncasecmp(command, "signon", 6)) {
		int index = SvIV(ST(1));
		if (g_slist_nth_data(aim_users, index))
		serv_login(g_slist_nth_data(aim_users, index));
	} else if (!strncasecmp(command, "signoff", 7)) {
		struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
		if (g_slist_find(connections, gc)) signoff(gc);
		else signoff_all(NULL, NULL);
	} else if (!strncasecmp(command, "info", 4)) {
		struct gaim_connection *gc = (struct gaim_connection *)SvIV(ST(1));
		if (g_slist_find(connections, gc))
			serv_set_info(gc, SvPV(ST(2), junk));
	} else if (!strncasecmp(command, "away", 4)) {
		char *message = SvPV(ST(1), junk);
		static struct away_message a;
		g_snprintf(a.message, sizeof(a.message), "%s", message);
		do_away_message(NULL, &a);
	} else if (!strncasecmp(command, "back", 4)) {
		do_im_back();
	} else if (!strncasecmp(command, "idle", 4)) {
		GSList *c = connections;
		struct gaim_connection *gc;

		while (c) {
			gc = (struct gaim_connection *)c->data;
			serv_set_idle(gc, SvIV(ST(1)));
			c = c->next;
		}
	} else if (!strncasecmp(command, "warn", 4)) {
		GSList *c = connections;
		struct gaim_connection *gc;

		while (c) {
			gc = (struct gaim_connection *)c->data;
			serv_warn(gc, SvPV(ST(1), junk), SvIV(ST(2)));
			c = c->next;
		}
	}

	XSRETURN(0);
}

XS (XS_GAIM_user_info)
{
	struct gaim_connection *gc;
	unsigned int junk;
	struct buddy *buddy = NULL;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	if (g_slist_find(connections, gc))
		buddy = find_buddy(gc, SvPV(ST(1), junk));

	if (!buddy)
		XSRETURN(0);
	XST_mPV(0, buddy->name);
	XST_mPV(1, buddy->show);
	XST_mPV(2, buddy->present ? "Online" : "Offline");
	XST_mIV(3, buddy->evil);
	XST_mIV(4, buddy->signon);
	XST_mIV(5, buddy->idle);
	XST_mIV(6, buddy->uc);
	XST_mIV(7, buddy->caps);
	XSRETURN(8);
}

XS (XS_GAIM_write_to_conv)
{
	char *nick, *who, *what;
	struct conversation *c;
	int junk;
	int send, wflags;
	dXSARGS;
	items = 0;

	nick = SvPV(ST(0), junk);
	send = SvIV(ST(1));
	what = SvPV(ST(2), junk);
	who = SvPV(ST(3), junk);
	
	if (!*who) who=NULL;
	
	switch (send) {
		case 0: wflags=WFLAG_SEND; break;
		case 1: wflags=WFLAG_RECV; break;
		case 2: wflags=WFLAG_SYSTEM; break;
		default: wflags=WFLAG_RECV;
	}	
		
	c = find_conversation(nick);
	if (!c)
		c = new_conversation(nick);
		
	write_to_conv(c, what, wflags, who, time(NULL), -1);
	XSRETURN(0);
}

XS (XS_GAIM_serv_send_im)
{
	struct gaim_connection *gc;
	char *nick, *what;
	int isauto;
	int junk;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	nick = SvPV(ST(1), junk);
	what = SvPV(ST(2), junk);
	isauto = SvIV(ST(3));

	if (!g_slist_find(connections, gc)) {
		XSRETURN(0);
		return;
	}
	serv_send_im(gc, nick, what, -1, isauto);
	XSRETURN(0);
}

XS (XS_GAIM_print_to_conv)
{
	struct gaim_connection *gc;
	char *nick, *what;
	int isauto;
	struct conversation *c;
	unsigned int junk;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	nick = SvPV(ST(1), junk);
	what = SvPV(ST(2), junk);
	isauto = SvIV(ST(3));
	if (!g_slist_find(connections, gc)) {
		XSRETURN(0);
		return;
	}
	c = find_conversation(nick);
	if (!c)
		c = new_conversation(nick);
	set_convo_gc(c, gc);
	write_to_conv(c, what, WFLAG_SEND | (isauto ? WFLAG_AUTO : 0), NULL, time(NULL), -1);
	serv_send_im(c->gc, nick, what, -1, isauto ? IM_FLAG_AWAY : 0);
	XSRETURN(0);
}

XS (XS_GAIM_print_to_chat)
{
	struct gaim_connection *gc;
	int id;
	char *what;
	struct conversation *b = NULL;
	GSList *bcs;
	unsigned int junk;
	dXSARGS;
	items = 0;

	gc = (struct gaim_connection *)SvIV(ST(0));
	id = SvIV(ST(1));
	what = SvPV(ST(2), junk);

	if (!g_slist_find(connections, gc)) {
		XSRETURN(0);
		return;
	}
	bcs = gc->buddy_chats;
	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (b->id == id)
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (b)
		serv_chat_send(gc, id, what);
	XSRETURN(0);
}

int perl_event(enum gaim_event event, void *arg1, void *arg2, void *arg3, void *arg4)
{
	char *buf = NULL;
	GList *handler;
	struct _perl_event_handlers *data;
	SV *handler_return;

	switch (event) {
	case event_signon:
	case event_signoff:
		buf = g_strdup_printf("'%lu'", (unsigned long)arg1);
		break;
	case event_away:
		buf = g_strdup_printf("'%lu','%s'", (unsigned long)arg1,
				((struct gaim_connection *)arg1)->away ?
					escape_quotes(((struct gaim_connection *)arg1)->away) : "");
		break;
	case event_im_recv:
		{
		char *tmp = *(char **)arg2 ? g_strdup(escape_quotes(*(char **)arg2)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%s','%s'", (unsigned long)arg1, tmp,
			   *(char **)arg3 ? escape_quotes(*(char **)arg3) : "");
		g_free(tmp);
		}
		break;
	case event_im_send:
		{
		char *tmp = arg2 ? g_strdup(escape_quotes(arg2)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%s','%s'", (unsigned long)arg1, tmp,
			   *(char **)arg3 ? escape_quotes(*(char **)arg3) : "");
		g_free(tmp);
		}
		break;
	case event_buddy_signon:
	case event_buddy_signoff:
	case event_set_info:
	case event_buddy_away:
	case event_buddy_back:
	case event_buddy_idle:
	case event_buddy_unidle:
		buf = g_strdup_printf("'%lu','%s'", (unsigned long)arg1, escape_quotes(arg2));
		break;
	case event_chat_invited:
		{
		char *tmp2, *tmp3, *tmp4;
		tmp2 = g_strdup(escape_quotes(arg2));
		tmp3 = g_strdup(escape_quotes(arg3));
		tmp4 = arg4 ? g_strdup(escape_quotes(arg4)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%s','%s','%s'", (unsigned long)arg1, tmp2, tmp3, tmp4);
		g_free(tmp2);
		g_free(tmp3);
		g_free(tmp4);
		}
		break;
	case event_chat_join:
	case event_chat_buddy_join:
	case event_chat_buddy_leave:
		buf = g_strdup_printf("'%lu','%d','%s'", (unsigned long)arg1, (int)arg2,
				escape_quotes(arg3));
		break;
	case event_chat_leave:
		buf = g_strdup_printf("'%lu','%d'", (unsigned long)arg1, (int)arg2);
		break;
	case event_chat_recv:
		{
		char *t3, *t4;
		t3 = g_strdup(escape_quotes(*(char **)arg3));
		t4 = *(char **)arg4 ? g_strdup(escape_quotes(*(char **)arg4)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%d','%s','%s'", (unsigned long)arg1, (int)arg2, t3, t4);
		g_free(t3);
		g_free(t4);
		}
		break;
	case event_chat_send_invite:
		{
		char *t3, *t4;
		t3 = g_strdup(escape_quotes(arg3));
		t4 = *(char **)arg4 ? g_strdup(escape_quotes(*(char **)arg4)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%d','%s','%s'", (unsigned long)arg1, (int)arg2, t3, t4);
		g_free(t3);
		g_free(t4);
		}
		break;
	case event_chat_send:
		buf = g_strdup_printf("'%lu','%d','%s'", (unsigned long)arg1, (int)arg2,
				*(char **)arg3 ? escape_quotes(*(char **)arg3) : "");
		break;
	case event_warned:
		buf = g_strdup_printf("'%lu','%s','%d'", (unsigned long)arg1,
				arg2 ? escape_quotes(arg2) : "", (int)arg3);
		break;
	case event_quit:
		buf = g_malloc0(1);
		break;
	case event_new_conversation:
		buf = g_strdup_printf("'%s'", escape_quotes(arg1));
		break;
	case event_im_displayed_sent:
		{
		char *tmp2, *tmp3;
		tmp2 = g_strdup(escape_quotes(arg2));
		tmp3 = *(char **)arg3 ? g_strdup(escape_quotes(*(char **)arg3)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%s','%s'", (unsigned long)arg1, tmp2, tmp3);
		g_free(tmp2);
		g_free(tmp3);
		}
		break;
	case event_im_displayed_rcvd:
		{
		char *tmp2, *tmp3;
		tmp2 = g_strdup(escape_quotes(arg2));
		tmp3 = arg3 ? g_strdup(escape_quotes(arg3)) : g_malloc0(1);
		buf = g_strdup_printf("'%lu','%s','%s'", (unsigned long)arg1, tmp2, tmp3);
		g_free(tmp2);
		g_free(tmp3);
		}
		break;
	default:
		return 0;
	}

	for (handler = perl_event_handlers; handler != NULL; handler = handler->next) {
		data = handler->data;
		if (!strcmp(event_name(event), data->event_type)) {
			handler_return = execute_perl(data->handler_name, buf);
			if (SvIV(handler_return)) {
				if (buf)
					g_free(buf);
				return SvIV(handler_return);
			}
		}
	}

	if (buf)
		g_free(buf);

	return 0;
}

XS (XS_GAIM_add_event_handler)
{
	unsigned int junk;
	struct _perl_event_handlers *handler;
	dXSARGS;
	items = 0;

	handler = g_new0(struct _perl_event_handlers, 1);
	handler->event_type = g_strdup(SvPV(ST(0), junk));
	handler->handler_name = g_strdup(SvPV(ST(1), junk));
	perl_event_handlers = g_list_append(perl_event_handlers, handler);
	debug_printf("registered perl event handler for %s\n", handler->event_type);
	XSRETURN_EMPTY;
}

static int perl_timeout(gpointer data)
{
	struct _perl_timeout_handlers *handler = data;
	execute_perl(handler->handler_name, escape_quotes(handler->handler_args));
	perl_timeout_handlers = g_list_remove(perl_timeout_handlers, handler);
	g_free(handler->handler_args);
	g_free(handler->handler_name);
	g_free(handler);

	return 0; /* returning zero removes the timeout handler */
}

XS (XS_GAIM_add_timeout_handler)
{
	unsigned int junk;
	long timeout;
	struct _perl_timeout_handlers *handler;
	dXSARGS;
	items = 0;

	handler = g_new0(struct _perl_timeout_handlers, 1);
	timeout = 1000 * SvIV(ST(0));
	debug_printf("Adding timeout for %d seconds.\n", timeout/1000);
	handler->handler_name = g_strdup(SvPV(ST(1), junk));
	handler->handler_args = g_strdup(SvPV(ST(2), junk));
	perl_timeout_handlers = g_list_append(perl_timeout_handlers, handler);
	handler->iotag = g_timeout_add(timeout, perl_timeout, handler);
	XSRETURN_EMPTY;
}

extern void unload_perl_scripts()
{
	perl_end();
	perl_init();
}

extern void list_perl_scripts()
{
	GList *s = perl_list;
	struct perlscript *p;
	char buf[BUF_LONG * 4];
	int at = 0;

	at += g_snprintf(buf + at, sizeof(buf) - at, "Loaded scripts:\n");
	while (s) {
		p = (struct perlscript *)s->data;
		at += g_snprintf(buf + at, sizeof(buf) - at, "%s\n", p->name);
		s = s->next;
	}

	do_error_dialog(buf, _("Perl Scripts"));
}

#endif /* USE_PERL */
