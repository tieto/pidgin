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
 *
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#undef PACKAGE

#ifndef USE_APPLET /* FIXME: _ conflicts */
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
#include "gaim.h"

struct perlscript {
	char *name;
	char *version;
	char *shutdowncallback; /* bleh */
};

struct _perl_timeout_handlers {
	char *handler_name;
	gint iotag;
};

static GList *perl_list = NULL;
static GList *perl_timeout_handlers = NULL;
static PerlInterpreter *my_perl = NULL;

/* dealing with gaim */
XS(XS_AIM_register);
XS(XS_AIM_get_info);
XS(XS_AIM_print); /* lemme figure this one out... */

/* list stuff */
XS(XS_AIM_buddy_list);
XS(XS_AIM_online_list);
XS(XS_AIM_deny_list); /* also returns permit list */

/* server stuff */
XS(XS_AIM_command);
XS(XS_AIM_user_info); /* given name, return struct buddy members */

/* handler commands */
XS(XS_AIM_add_message_handler);
XS(XS_AIM_add_command_handler);
XS(XS_AIM_add_timeout_handler);


/* perl module support */
extern void xs_init _((void));
extern void boot_DynaLoader _((CV * cv)); /* perl is so wacky */

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

	if (perl_cmd)
		g_free(perl_cmd);
	perl_cmd = g_malloc(strlen(function) + strlen(args) + 2 + 10);
	sprintf(perl_cmd, "&%s('%s')", function, escape_quotes(args));
#ifndef HAVE_PERL_EVAL_PV
	return (perl_eval_pv(perl_cmd, TRUE));
#else
	return (Perl_eval_pv(perl_cmd, TRUE));
#endif
}

int perl_load_file(char *script_name)
{
	SV *return_val;
	return_val = execute_perl("load_file", script_name);
	return SvNV (return_val);
}

void perl_init()
{
	char *perl_args[] = {"", "-e", "0"};
	char load_file[] =
"sub load_file()\n"
"{\n"
"	(my $file_name) = @_;\n"
"	open FH, $file_name or return 2;\n"
"	local($/) = undef;\n"
"	$file = <FH>;\n"
"	close FH;\n"
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

	newXS ("AIM::register", XS_AIM_register, "AIM");
	newXS ("AIM::get_info", XS_AIM_get_info, "AIM");
	newXS ("AIM::print", XS_AIM_print, "AIM");

	newXS ("AIM::buddy_list", XS_AIM_buddy_list, "AIM");
	newXS ("AIM::online_list", XS_AIM_online_list, "AIM");
	newXS ("AIM::deny_list", XS_AIM_deny_list, "AIM");

	newXS ("AIM::command", XS_AIM_command, "AIM");
	newXS ("AIM::user_info", XS_AIM_user_info, "AIM");

	newXS ("AIM::add_message_handler", XS_AIM_add_message_handler, "AIM");
	newXS ("AIM::add_command_handler", XS_AIM_add_command_handler, "AIM");
	newXS ("AIM::add_timeout_handler", XS_AIM_add_timeout_handler, "AIM");
}

void perl_end()
{
	struct perlscript *scp;
	struct _perl_timeout_handlers *thn;

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
		gtk_timeout_remove(thn->iotag);
		g_free(thn->handler_name);
		g_free(thn);
	}

	if (my_perl != NULL) {
		perl_destruct(my_perl);
		perl_free(my_perl);
		my_perl = NULL;
	}
}

XS (XS_AIM_register)
{
	char *name, *ver, *callback, *unused; /* exactly like X-Chat, eh? :) */
	int junk;
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

XS (XS_AIM_get_info)
{
	int junk;
	dXSARGS;
	items = 0;

	switch(atoi(SvPV(ST(0), junk))) {
	case 0:
		XST_mPV(0, VERSION);
		break;
	case 1:
		XST_mPV(0, current_user->username);
		break;
	/* FIXME */
	default:
		XST_mPV(0, "Error2");
	}

	XSRETURN(1);
}

XS (XS_AIM_print)
{
	/* FIXME */
}

XS (XS_AIM_buddy_list)
{
	struct buddy *buddy;
	struct group *g;
	GList *list = groups;
	GList *mem;
	int i = 0;
	dXSARGS;
	items = 0;

	while (list) {
		g = (struct group *)list->data;
		mem = g->members;
		while (mem) {
			buddy = (struct buddy *)mem->data;
			XST_mPV(i++, buddy->name);
			mem = mem->next;
		}
		list = list->next;
	}
	XSRETURN(i);
}

XS (XS_AIM_online_list)
{
	struct buddy *b;
	struct group *g;
	GList *list = groups;
	GList *mem;
	int i = 0;
	dXSARGS;
	items = 0;

	while (list) {
		g = (struct group *)list->data;
		mem = g->members;
		while (mem) {
			b = (struct buddy *)mem->data;
			if (b->present) XST_mPV(i++, b->name);
			mem = mem->next;
		}
		list = list->next;
	}
	XSRETURN(i);
}

XS (XS_AIM_deny_list)
{
	/* FIXME */
}

XS (XS_AIM_command)
{
	/* FIXME */
}

XS (XS_AIM_user_info)
{
	int junk;
	struct buddy *buddy;
	char *nick;
	dXSARGS;
	items = 0;

	nick = SvPV(ST(0), junk);
	if (!nick[0])
		XSRETURN(0);
	buddy = find_buddy(nick);
	if (!buddy)
		XSRETURN(0);
	XST_mPV(0, buddy->name);
	XST_mPV(1, buddy->present ? "Online" : "Offline");
	XST_mIV(2, buddy->evil);
	XST_mIV(3, buddy->signon);
	XST_mIV(4, buddy->idle);
	XST_mIV(5, buddy->uc);
	XST_mIV(6, buddy->caps);
	XSRETURN(6);
}

XS (XS_AIM_add_message_handler)
{
	/* FIXME */
}

XS (XS_AIM_add_command_handler)
{
	/* FIXME */
}

static int perl_timeout(struct _perl_timeout_handlers *handler)
{
	execute_perl(handler->handler_name, "");
	perl_timeout_handlers = g_list_remove(perl_timeout_handlers, handler);
	g_free(handler->handler_name);
	g_free(handler);

	return 0; /* returning zero removes the timeout handler */
}

XS (XS_AIM_add_timeout_handler)
{
	int junk;
	long timeout;
	struct _perl_timeout_handlers *handler;
	dXSARGS;
	items = 0;

	handler = g_new0(struct _perl_timeout_handlers, 1);
	timeout = atol(SvPV(ST(0), junk));
	handler->handler_name = g_strdup(SvPV(ST(1), junk));
	perl_timeout_handlers = g_list_append(perl_timeout_handlers, handler);
	handler->iotag = gtk_timeout_add(timeout, (GtkFunction)perl_timeout, handler);
	XSRETURN_EMPTY;
}

#endif /* USE_PERL */
#endif /* ifndef USE_APPLET */
