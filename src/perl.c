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

#define group perl_group

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

#undef group

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
	char *handle;
};

struct _perl_timeout_handlers {
	char *handler_name;
	char *handler_args;
	gint iotag;
	char *handle;
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
XS(XS_GAIM_remove_event_handler); /* remove a handler */
XS(XS_GAIM_add_timeout_handler); /* figure it out */

/* play sound */
XS(XS_GAIM_play_sound); /*play a sound*/

void xs_init()
{
	char *file = __FILE__;

	/* This one allows dynamic loading of perl modules in perl
           scripts by the 'use perlmod;' construction*/
	newXS ("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
	
	/* load up all the custom Gaim perl functions */
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
	newXS ("GAIM::remove_event_handler", XS_GAIM_remove_event_handler, "GAIM");
	newXS ("GAIM::add_timeout_handler", XS_GAIM_add_timeout_handler, "GAIM");

	newXS ("GAIM::play_sound", XS_GAIM_play_sound, "GAIM");
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

/*
  2001/06/14: execute_perl replaced by Martin Persson <mep@passagen.se>
              previous use of perl_eval leaked memory, replaced with
              a version that uses perl_call instead
*/

static int 
execute_perl(char *function, char *args)
{
	char *perl_args[2] = { args, NULL }, buf[512];
	int count, ret_value = 1;
	SV *sv;

	dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(sp);
        count = perl_call_argv(function, G_EVAL | G_SCALAR, perl_args);
        SPAGAIN;

        sv = GvSV(gv_fetchpv("@", TRUE, SVt_PV));
        if (SvTRUE(sv)) {
                snprintf(buf, 512, "Perl error: %s\n", SvPV(sv, count));
                debug_printf(buf);
                POPs;
        } else if (count != 1) {
                snprintf(buf, 512, "Perl error: expected 1 value from %s, "
                        "got: %d\n", function, count);
                debug_printf(buf);
        } else {
                ret_value = POPi;
        }

        PUTBACK;
        FREETMPS;
        LEAVE;

        return ret_value;

}

/* This function is so incredibly broken and should never, ever, ever
   be trusted to work */
void perl_unload_file(struct gaim_plugin *plug) {
	struct perlscript *scp = NULL;
	struct _perl_timeout_handlers *thn;
	struct _perl_event_handlers *ehn;

	GList *pl = perl_list;

	debug_printf("Unloading %s\n", plug->handle);
	while (pl) {
		scp = pl->data;
		/* This is so broken */
		if (!strcmp(scp->name, plug->desc.name) &&
		    !strcmp(scp->version, plug->desc.version))
			break;
		pl = pl->next;
		scp = NULL;
	}
	if (scp) {
		perl_list = g_list_remove(perl_list, scp);
		if (scp->shutdowncallback[0])
			execute_perl(scp->shutdowncallback, "");
		perl_list = g_list_remove(perl_list, scp);
		g_free(scp->name);
		g_free(scp->version);
		g_free(scp->shutdowncallback);
		g_free(scp);
	}

	pl = perl_timeout_handlers;
	while (pl) {
		thn = pl->data;
		if (thn && thn->handle == plug->handle) {
			g_list_remove(perl_timeout_handlers, thn);
			g_source_remove(thn->iotag);
			g_free(thn->handler_args);
			g_free(thn->handler_name);
			g_free(thn);
		}
		pl = pl->next;
	}

	pl = perl_event_handlers;
	while (pl) {
		ehn = pl->data;
		if (ehn && ehn->handle == plug->handle) {
			perl_event_handlers = g_list_remove(perl_event_handlers, ehn);
			g_free(ehn->event_type);
			g_free(ehn->handler_name);
			g_free(ehn);
		}
		pl = pl->next;
	}

	plug->handle=NULL;
}

int perl_load_file(char *script_name)
{
	struct gaim_plugin *plug;
	GList *p = probed_plugins;
	GList *e = perl_event_handlers;
	GList *t = perl_timeout_handlers;
	int num_e, num_t, ret;

	if (my_perl == NULL)
		perl_init();
	
	while (p) {
		plug = (struct gaim_plugin *)p->data;
		if (!strcmp(script_name, plug->path)) 
			break;
		p = p->next;
	}
	
	if (!plug) {
		probe_perl(script_name);
	}
	
	plug->handle = plug->path;
	
	/* This is such a terrible hack-- if I weren't tired and annoyed
	 * with perl, I'm sure I wouldn't even be considering this. */
	num_e=g_list_length(e);
	num_t=g_list_length(t);

	ret = execute_perl("load_n_eval", script_name);

	t = g_list_nth(perl_timeout_handlers, num_t++);
	while (t) {
		struct _perl_timeout_handlers *h = t->data;
		h->handle = plug->handle;
		t = t->next;
	}

	e = g_list_nth(perl_event_handlers, num_e++);
	while (e) {
		struct _perl_event_handlers *h = e->data;
		h->handle = plug->handle;
		e = e->next;
	}
	return ret;
}

struct gaim_plugin *probe_perl(const char *filename) {

	/* XXX This woulld be much faster if I didn't create a new
	 *     PerlInterpreter every time I did probed a plugin */

	PerlInterpreter *prober = perl_alloc();
	struct gaim_plugin * plug = NULL;
	char *argv[] = {"", filename};
	int count;
	perl_construct(prober);
	perl_parse(prober, NULL, 2, argv, NULL);
	
	{
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		count =perl_call_pv("description", G_NOARGS | G_ARRAY);
		SPAGAIN;
		
		if (count = sizeof(struct gaim_plugin_description) / sizeof(char*)) {
			plug = g_new0(struct gaim_plugin, 1);
			plug->type = perl_script;
			g_snprintf(plug->path, sizeof(plug->path), filename);
			plug->desc.iconfile = g_strdup(POPp);
			plug->desc.url = g_strdup(POPp);
			plug->desc.authors = g_strdup(POPp);
			plug->desc.description = g_strdup(POPp);
			plug->desc.version = g_strdup(POPp);
			plug->desc.name = g_strdup(POPp);
		}
			
		PUTBACK;
		FREETMPS;
		LEAVE;
	}
	perl_destruct(prober);
	perl_free(prober);
	return plug;
}

static void perl_init()
{        /*changed the name of the variable from load_file to
	   perl_definitions since now it does much more than defining
	   the load_file sub. Moreover, deplaced the initialisation to
	   the xs_init function. (TheHobbit)*/
        char *perl_args[] = { "", "-e", "0", "-w" };
        char perl_definitions[] =
        {
		/* We use to function one to load a file the other to
		   execute the string obtained from the first and holding
		   the file conents. This allows to have a realy local $/
		   without introducing temp variables to hold the old
		   value. Just a question of style:) */ 
		"sub load_file{"
		  "my $f_name=shift;"
		  "local $/=undef;"
		  "open FH,$f_name or return \"__FAILED__\";"
		  "$_=<FH>;"
		  "close FH;"
		  "return $_;"
		"}"
		"sub load_n_eval{"
		  "my $f_name=shift;"
		  "my $strin=load_file($f_name);"
		  "return 2 if($strin eq \"__FAILED__\");"
		  "eval $strin;"
		  "if($@){"
		    /*"  #something went wrong\n"*/
		    "GAIM::print\"Errors loading file $f_name:\\n\";"
		    "GAIM::print\"$@\\n\";"
		    "return 1;"
		  "}"
		  "return 0;"
		"}"
	};

	my_perl = perl_alloc();
	perl_construct(my_perl);
#ifdef DEBUG
	perl_parse(my_perl, xs_init, 4, perl_args, NULL);
#else
	perl_parse(my_perl, xs_init, 3, perl_args, NULL);
#endif
#ifndef HAVE_PERL_EVAL_PV
	eval_pv(perl_definitions, TRUE);
#else
	perl_eval_pv(perl_definitions, TRUE); /* deprecated */
#endif


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
		debug_printf("handler_name: %s\n", ehn->handler_name);
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
	do_error_dialog(title, message, GAIM_INFO);
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

int perl_event(enum gaim_event event, void *arg1, void *arg2, void *arg3, void *arg4, void *arg5)
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
	case event_blist_update:
		buf = g_malloc0(1);
		break;
	case event_new_conversation:
	case event_del_conversation:
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
	case event_draw_menu:
		/* we can't handle this usefully without gtk/perl bindings */
		return 0;
	default:
		debug_printf("someone forgot to handle %s in the perl binding\n", event_name(event));
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

XS (XS_GAIM_remove_event_handler)
{
	unsigned int junk;
	struct _perl_event_handlers *ehn;
	GList *cur = perl_event_handlers;
	dXSARGS;

	while (cur) {
		GList *next = cur->next;
		ehn = cur->data;

		if (!strcmp(ehn->event_type, SvPV(ST(0), junk)) &&
			!strcmp(ehn->handler_name, SvPV(ST(1), junk)))
		{
	        perl_event_handlers = g_list_remove(perl_event_handlers, ehn);
			g_free(ehn->event_type);
			g_free(ehn->handler_name);
			g_free(ehn);
		}

		cur = next;
    }
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

XS (XS_GAIM_play_sound)
{
	int id;
	dXSARGS;

	id = SvIV(ST(0));

	play_sound(id);

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

	do_error_dialog(buf, NULL, GAIM_INFO);
}

#endif /* USE_PERL */
