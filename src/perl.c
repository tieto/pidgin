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
#include "../config.h"
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
#include <gtk/gtk.h>


/* perl module support */
extern void xs_init _((void));
extern void boot_DynaLoader _((CV * cv)); /* perl is so wacky */

#undef _
#ifdef DEBUG
#undef DEBUG
#endif
#include "gaim.h"

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
	gint iotag;
};

static GList *perl_list = NULL; /* should probably extern this at some point */
static GList *perl_timeout_handlers = NULL;
static GList *perl_event_handlers = NULL;
static PerlInterpreter *my_perl = NULL;
static char* last_dir = NULL;

/* dealing with gaim */
XS(XS_GAIM_register); /* set up hooks for script */
XS(XS_GAIM_get_info); /* version, last to attempt signon, protocol */
XS(XS_GAIM_print); /* lemme figure this one out... */

/* list stuff */
XS(XS_GAIM_buddy_list); /* all buddies */
XS(XS_GAIM_online_list); /* online buddies */

/* server stuff */
XS(XS_GAIM_command); /* send command to server */
XS(XS_GAIM_user_info); /* given name, return struct buddy members */
XS(XS_GAIM_print_to_conv); /* send message to someone */
XS(XS_GAIM_print_to_chat); /* send message to chat room */

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

	if (perl_cmd)
		g_free(perl_cmd);
	perl_cmd = g_malloc(strlen(function) + (strlen(args) * 2) + 2 + 10);
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

void perl_init()
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

	newXS ("GAIM::buddy_list", XS_GAIM_buddy_list, "GAIM");
	newXS ("GAIM::online_list", XS_GAIM_online_list, "GAIM");

	newXS ("GAIM::command", XS_GAIM_command, "GAIM");
	newXS ("GAIM::user_info", XS_GAIM_user_info, "GAIM");
	newXS ("GAIM::print_to_conv", XS_GAIM_print_to_conv, "GAIM");
	newXS ("GAIM::print_to_chat", XS_GAIM_print_to_chat, "GAIM");

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
		gtk_timeout_remove(thn->iotag);
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

XS (XS_GAIM_get_info)
{
	int i = 0;
	int junk;
	dXSARGS;
	items = 0;

	switch(atoi(SvPV(ST(0), junk))) {
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
				XST_mPV(i++, gc->username);
				c = c->next;
			}
		}
		break;
	case 2:
		{
			GList *u = aim_users;
			struct aim_user *a;
			char *name = g_strdup(normalize(SvPV(ST(1), junk)));

			while (u) {
				a = (struct aim_user *)u->data;
				if (!strcasecmp(normalize(a->username), name))
					XST_mIV(i++, a->protocol);
				u = u->next;
			}
			g_free(name);
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
	int junk;
	dXSARGS;
	items = 0;

	title = SvPV(ST(0), junk);
	message = SvPV(ST(1), junk);
	do_error_dialog(message, title);
	XSRETURN(0);
}

XS (XS_GAIM_buddy_list)
{
	char *acct;
	struct gaim_connection *gc;
	struct buddy *buddy;
	struct group *g;
	GSList *list = NULL;
	GSList *mem;
	int i = 0;
	int junk;
	dXSARGS;
	items = 0;

	acct = SvPV(ST(0), junk);
	gc = find_gaim_conn_by_name(acct);
	if (gc) list = gc->groups;

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
	char *acct;
	struct gaim_connection *gc;
	struct buddy *b;
	struct group *g;
	GSList *list = NULL;
	GSList *mem;
	int i = 0;
	int junk;
	dXSARGS;
	items = 0;

	acct = SvPV(ST(0), junk);
	gc = find_gaim_conn_by_name(acct);
	if (gc) list = gc->groups;

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
	int junk;
	char *command = NULL;
	dXSARGS;
	items = 0;

	command = SvPV(ST(0), junk);
	if (!command) XSRETURN(0);
	if        (!strncasecmp(command, "signon", 6)) {
		char *who = SvPV(ST(1), junk);
		struct aim_user *u = find_user(who, -1);
		if (u) serv_login(u);
	} else if (!strncasecmp(command, "signoff", 7)) {
		char *who = SvPV(ST(1), junk);
		struct gaim_connection *gc = find_gaim_conn_by_name(who);
		if (gc) signoff(gc);
		else signoff_all(NULL, NULL);
	} else if (!strncasecmp(command, "info", 4)) {
	        GSList *c = connections;
		struct gaim_connection *gc;
		while (c) {
		        gc = (struct gaim_connection *)c->data;
			serv_set_info(gc, SvPV(ST(1), junk));
			c = c->next;
		}
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
			serv_set_idle(gc, atoi(SvPV(ST(1), junk)));
			gc->is_idle = 1;
			c = c->next;
		}
	} else if (!strncasecmp(command, "warn", 4)) {
		GSList *c = connections;
		struct gaim_connection *gc;

		while (c) {
			gc = (struct gaim_connection *)c->data;
			serv_warn(gc, SvPV(ST(1), junk), atoi(SvPV(ST(2), junk)));
			c = c->next;
		}
	}

	XSRETURN(0);
}

XS (XS_GAIM_user_info)
{
	GSList *c = connections;
	struct gaim_connection *gc;
	int junk;
	struct buddy *buddy = NULL;
	char *nick;
	dXSARGS;
	items = 0;

	nick = SvPV(ST(0), junk);
	if (!nick[0])
		XSRETURN(0);
	while (c) {
		gc = (struct gaim_connection *)c->data;
		buddy = find_buddy(gc, nick);
		if (buddy) c = NULL;
		else c = c->next;
	}
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

XS (XS_GAIM_print_to_conv)
{
	char *nick, *what, *isauto;
	struct conversation *c;
	int junk;
	dXSARGS;
	items = 0;

	nick = SvPV(ST(0), junk);
	what = SvPV(ST(1), junk);
	isauto = SvPV(ST(2), junk);
	c = find_conversation(nick);
	if (!c)
		c = new_conversation(nick);
	write_to_conv(c, what, WFLAG_SEND, NULL, time((time_t)NULL));
	serv_send_im(c->gc, nick, what, atoi(isauto));
}

XS (XS_GAIM_print_to_chat)
{
	char *nick, *what, *tmp;
	GSList *c = connections;
	struct gaim_connection *gc;
	struct conversation *b = NULL;
	GSList *bcs;
	int junk;
	dXSARGS;
	items = 0;

	nick = SvPV(ST(0), junk);
	what = SvPV(ST(1), junk);
	tmp = g_strdup(normalize(nick));
	while (c) {
		gc = (struct gaim_connection *)c->data;
		bcs = gc->buddy_chats;
		while (bcs) {
			b = (struct conversation *)bcs->data;
			if (!strcmp(normalize(b->name), tmp))
				break;
			bcs = bcs->next;
			b = NULL;
		}
		serv_chat_send(b->gc, b->id, what);
		c = c->next;
	}
	XSRETURN(0);
}

int perl_event(char *event, char *args)
{
	GList *handler;
	struct _perl_event_handlers *data;
	SV *handler_return;

	for (handler = perl_event_handlers; handler != NULL; handler = handler->next) {
		data = handler->data;
		if (!strcmp(event, data->event_type)) {
			handler_return = execute_perl(data->handler_name, args);
			if (SvIV(handler_return))
				return SvIV(handler_return);
		}
	}

	return 0;
}

XS (XS_GAIM_add_event_handler)
{
	int junk;
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

static int perl_timeout(struct _perl_timeout_handlers *handler)
{
	execute_perl(handler->handler_name, "");
	perl_timeout_handlers = g_list_remove(perl_timeout_handlers, handler);
	g_free(handler->handler_name);
	g_free(handler);

	return 0; /* returning zero removes the timeout handler */
}

XS (XS_GAIM_add_timeout_handler)
{
	int junk;
	long timeout;
	struct _perl_timeout_handlers *handler;
	dXSARGS;
	items = 0;

	handler = g_new0(struct _perl_timeout_handlers, 1);
	timeout = 1000 * atol(SvPV(ST(0), junk));
	handler->handler_name = g_strdup(SvPV(ST(1), junk));
	perl_timeout_handlers = g_list_append(perl_timeout_handlers, handler);
	handler->iotag = gtk_timeout_add(timeout, (GtkFunction)perl_timeout, handler);
	XSRETURN_EMPTY;
}

static GtkWidget *config = NULL; 

static void cfdes(GtkWidget *m, gpointer n) {
	if (config) gtk_widget_destroy(config);
	config = NULL;
}

static void do_load(GtkWidget *m, gpointer n) {
	gchar* file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(config));
	if (!file || !strlen(file)) {
		perl_end();
		perl_init();
		return;
	}
	
	if (file_is_dir(file, config)) {
		return;
	}
	
	if (last_dir) {
		g_free(last_dir);
	}
	last_dir = g_dirname(file);

	debug_printf("Loading perl script: %s\n", file);
	
	perl_load_file(file);
	cfdes(config, NULL);
}

void load_perl_script(GtkWidget *w, gpointer d)
{
	char *buf, *temp;

	if (config) {
		gtk_widget_show(config);
		gdk_window_raise(config->window);
		return;
	}

	/* Below is basically stolen from plugins.c */
	config = gtk_file_selection_new(_("Gaim - Select Perl Script"));

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(config));

	if (!last_dir) {
		temp = gaim_user_dir();
		buf = g_strconcat(temp, G_DIR_SEPARATOR_S, NULL);
		g_free(temp);
	} else {
		buf = g_strconcat(last_dir, G_DIR_SEPARATOR_S, NULL);
	}

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(config), buf);
	gtk_file_selection_complete(GTK_FILE_SELECTION(config), "*.pl"); 
	gtk_signal_connect(GTK_OBJECT(config), "destroy",  GTK_SIGNAL_FUNC(cfdes),
			config);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(config)->ok_button),
			"clicked", GTK_SIGNAL_FUNC(do_load), NULL);
    
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(config)->cancel_button),
			"clicked", GTK_SIGNAL_FUNC(cfdes), NULL);

	g_free(buf);
	gtk_widget_show(config);
	gdk_window_raise(config->window);
}

extern void unload_perl_scripts(GtkWidget *w, gpointer d)
{
	perl_end();
	perl_init();
}

extern void list_perl_scripts(GtkWidget *w, gpointer d)
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
