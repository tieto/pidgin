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
#ifdef DEBUG
# undef DEBUG
#endif

#undef PACKAGE

#define group perl_group

#ifdef _WIN32
/* This took me an age to figure out.. without this __declspec(dllimport)
 * will be ignored.
 */
# define HASATTRIBUTE
#endif

#include <EXTERN.h>

#ifndef _SEM_SEMUN_UNDEFINED
# define HAS_UNION_SEMUN
#endif

#include <perl.h>
#include <XSUB.h>

#ifndef _WIN32
# include <sys/mman.h>
#endif

#undef PACKAGE

#ifndef _WIN32
# include <dirent.h>
#else
 /* We're using perl's win32 port of this */
# define dirent direct
#endif

#undef group

/* perl module support */
#ifdef OLD_PERL
extern void boot_DynaLoader _((CV * cv));
#else
extern void boot_DynaLoader _((pTHX_ CV * cv)); /* perl is so wacky */
#endif

#undef _
#ifdef DEBUG
# undef DEBUG
#endif
#ifdef _WIN32
# undef pipe
#endif

#ifdef _WIN32
#define _WIN32DEP_H_
#endif
#include "internal.h"

#include "debug.h"
#include "prpl.h"
#include "notify.h"
#include "server.h"
#include "sound.h"

#include "ui.h"

#ifndef call_pv
# define call_pv(i,j) perl_call_pv((i), (j))
#endif

#define PERL_PLUGIN_ID "core-perl"

struct perlscript {
	char *name;
	char *version;
	char *shutdowncallback; /* bleh */
	GaimPlugin *plug;
};

struct _perl_event_handlers {
	char *event_type;
	char *handler_name;
	GaimPlugin *plug;
};

struct _perl_timeout_handlers {
	char *handler_name;
	char *handler_args;
	gint iotag;
	GaimPlugin *plug;
};

static GaimPlugin *my_plugin = NULL;
static GList *perl_list = NULL;
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
XS(XS_GAIM_play_sound); /*play a sound */

static void
#ifdef OLD_PERL
xs_init()
#else
xs_init(pTHX)
#endif
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

static char *
escape_quotes(const char *buf)
{
	static char *tmp_buf = NULL;
	const char *i;
	char *j;

	if (tmp_buf)
		g_free(tmp_buf);

	tmp_buf = g_malloc(strlen(buf) * 2 + 1);

	for (i = buf, j = tmp_buf; *i; i++, j++) {
		if (*i == '\'' || *i == '\\')
			*j++ = '\\';

		*j = *i;
	}

	*j = '\0';

	return tmp_buf;
}

/*
  2003/02/06: execute_perl modified by Mark Doliner <mark@kingant.net>
		Pass parameters by pushing them onto the stack rather than 
		passing an array of strings.  This way, perl scripts can 
		modify the parameters and we can get the changed values 
		and then shoot ourselves.  I mean, uh, use them.

  2001/06/14: execute_perl replaced by Martin Persson <mep@passagen.se>
		previous use of perl_eval leaked memory, replaced with
		a version that uses perl_call instead

  30/11/2002: execute_perl modified by Eric Timme <timothy@voidnet.com>
		args changed to char** so that we can have preparsed
  		arguments again, and many headaches ensued! This essentially 
		means we replaced one hacked method with a messier hacked 
		method out of perceived necessity. Formerly execute_perl 
		required a single char_ptr, and it would insert it into an 
		array of character pointers and NULL terminate the new array.
		Now we have to pass in pre-terminated character pointer arrays
		to accomodate functions that want to pass in multiple arguments.

		Previously arguments were preparsed because an argument list
		was constructed in the form 'arg one','arg two' and was
		executed via a call like &funcname(arglist) (see .59.x), so
		the arglist was magically pre-parsed because of the method. 
		With Martin Persson's change to perl_call we now need to
		use a null terminated list of character pointers for arguments
		if we wish them to be parsed. Lacking a better way to allow
		for both single arguments and many I created a NULL terminated
		array in every function that called execute_perl and passed
		that list into the function.  In the former version a single
		character pointer was passed in, and was placed into an array
		of character pointers with two elements, with a NULL element
		tacked onto the back, but this method no longer seemed prudent.

		Enhancements in the future might be to get rid of pre-declaring
		the array sizes?  I am not comfortable enough with this
		subject to attempt it myself and hope it to stand the test
		of time.
*/

static int 
execute_perl(const char *function, int argc, char **args)
{
	int count = 0, i, ret_value = 1;
	SV *sv_args[argc];
	STRLEN na;

	/*
	 * Set up the perl environment, push arguments onto the 
	 * perl stack, then call the given function
	 */
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	for (i = 0; i < argc; i++) {
		if (args[i]) {
			sv_args[i] = sv_2mortal(newSVpv(args[i], 0));
			XPUSHs(sv_args[i]);
		}
	}

	PUTBACK;
	count = call_pv(function, G_EVAL | G_SCALAR);
	SPAGAIN;

	/*
	 * Check for "die," make sure we have 1 argument, and set our
	 * return value.
	 */
	if (SvTRUE(ERRSV)) {
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Perl function %s exited abnormally: %s\n",
				   function, SvPV(ERRSV, na));
		POPs;
	}
	else if (count != 1) {
		/*
		 * This should NEVER happen.  G_SCALAR ensures that we WILL
		 * have 1 parameter.
		 */
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Perl error from %s: expected 1 return value, "
				   "but got %d\n", function, count);
	}
	else
		ret_value = POPi;

	/* Check for changed arguments */
	for (i = 0; i < argc; i++) {
		if (args[i] && strcmp(args[i], SvPVX(sv_args[i]))) {
			/*
			 * Shizzel.  So the perl script changed one of the parameters,
			 * and we want this change to affect the original parameters.
			 * args[i] is just a tempory little list of pointers.  We don't
			 * want to free args[i] here because the new parameter doesn't
			 * overwrite the data that args[i] points to.  That is done by
			 * the function that called execute_perl.  I'm not explaining this
			 * very well.  See, it's aggregate...  Oh, but if 2 perl scripts
			 * both modify the data, _that's_ a memleak.  This is really kind
			 * of hackish.  I should fix it.  Look how long this comment is.
			 * Holy crap.
			 */
			args[i] = g_strdup(SvPV(sv_args[i], na));
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret_value;
}

static void
perl_unload_file(GaimPlugin *plug)
{
	char *atmp[2] = { "", NULL };
	struct perlscript *scp = NULL;
	struct _perl_timeout_handlers *thn;
	struct _perl_event_handlers *ehn;
	GList *pl;

	for (pl = perl_list; pl != NULL; pl = pl->next) {
		scp = pl->data;

		if (scp->plug == plug) {
			perl_list = g_list_remove(perl_list, scp);

			if (scp->shutdowncallback[0])
				execute_perl(scp->shutdowncallback, 1, atmp);

			g_free(scp->name);
			g_free(scp->version);
			g_free(scp->shutdowncallback);
			g_free(scp);	

			break;
		}
	}

	for (pl = perl_timeout_handlers; pl != NULL; pl = pl->next) {
		thn = pl->data;

		if (thn && thn->plug == plug) {
			perl_timeout_handlers = g_list_remove(perl_timeout_handlers, thn);

			g_source_remove(thn->iotag);
			g_free(thn->handler_args);
			g_free(thn->handler_name);
			g_free(thn);
		}
	}

	for (pl = perl_event_handlers; pl != NULL; pl = pl->next) {
		ehn = pl->data;

		if (ehn && ehn->plug == plug) {
			perl_event_handlers = g_list_remove(perl_event_handlers, ehn);

			g_free(ehn->event_type);
			g_free(ehn->handler_name);
			g_free(ehn);
		}
	}
}

static int
perl_load_file(char *script_name, GaimPlugin *plugin)
{
	char *atmp[2] = { script_name, NULL };
	GList *s;
	struct perlscript *scp;
	int ret;

	if (my_perl == NULL)
		perl_init();

	plugin->handle = plugin->path;

	ret = execute_perl("load_n_eval", 1, atmp);

	for (s = perl_list; s != NULL; s = s->next) {
		scp = s->data;

		if (!strcmp(scp->name, plugin->info->name) &&
		    !strcmp(scp->version, plugin->info->version)) {

			break;
		}
	}

	if (!s) {
		plugin->error = g_strdup(_("GAIM::register not called with "
								   "proper arguments.  Consult PERL-HOWTO."));

		return 0;
	}
	
	return ret;
}

static void
perl_init(void)
{
	/* changed the name of the variable from load_file to
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
		    "GAIM::print(\"Errors loading file $f_name:\\n\",\"$@\");"
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
#ifdef HAVE_PERL_EVAL_PV
	eval_pv(perl_definitions, TRUE);
#else
	perl_eval_pv(perl_definitions, TRUE); /* deprecated */
#endif
}

static void
perl_end(void)
{
	char *atmp[2] = { "", NULL };
	struct perlscript *scp;
	struct _perl_timeout_handlers *thn;
	struct _perl_event_handlers *ehn;

	while (perl_list) {
		scp = perl_list->data;
		perl_list = g_list_remove(perl_list, scp);

		if (scp->shutdowncallback[0])
			execute_perl(scp->shutdowncallback, 1, atmp);

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
	GaimPlugin *plug = NULL;
	GList *pl;

	dXSARGS;
	items = 0;

	name     = SvPV(ST(0), junk);
	ver      = SvPV(ST(1), junk);
	callback = SvPV(ST(2), junk);
	unused   = SvPV(ST(3), junk);

	gaim_debug(GAIM_DEBUG_INFO, "perl",
			   "GAIM::register(%s, %s)\n", name, ver);

	for (pl = gaim_plugins_get_all(); pl != NULL; pl = pl->next) {
		plug = pl->data;

		if (!strcmp(name, plug->info->name) &&
		    !strcmp(ver, plug->info->version)) {

			break;
		}

		plug = NULL;
	}

	if (plug) {
		scp = g_new0(struct perlscript, 1);
		scp->name = g_strdup(name);
		scp->version = g_strdup(ver);
		scp->shutdowncallback = g_strdup(callback);
		scp->plug = plug; 
		perl_list = g_list_append(perl_list, scp);
		XST_mPV(0, plug->path);
	}
	else
		XST_mPV(0, NULL);

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
				GList *c = gaim_connections_get_all();
				GaimConnection *gc;

				while (c) {
					gc = (GaimConnection *)c->data;
					XST_mIV(i++, (guint)gc);
					c = c->next;
				}
			}
			break;

		case 2:
			{
				GaimConnection *gc =
					(GaimConnection *)SvIV(ST(1));
				GaimAccount *account = gaim_connection_get_account(gc);

				if (g_list_find(gaim_connections_get_all(), gc))
					XST_mIV(i++, gaim_account_get_protocol(account));
				else
					XST_mIV(i++, -1);
			}
			break;

		case 3:
			{
				GaimConnection *gc =
					(GaimConnection *)SvIV(ST(1));
				GaimAccount *account = gaim_connection_get_account(gc);

				if (g_list_find(gaim_connections_get_all(), gc))
					XST_mPV(i++, gaim_account_get_username(account));
				else
					XST_mPV(i++, "");
			}
			break;

		case 4:
			{
				GaimConnection *gc =
					(GaimConnection *)SvIV(ST(1));
				GaimAccount *account = gaim_connection_get_account(gc);

				if (g_list_find(gaim_connections_get_all(), gc))
					XST_mIV(i++, g_list_index(gaim_accounts_get_all(),
											  account));
				else
					XST_mIV(i++, -1);
			}
			break;

		case 5:
			{
				GList *a = gaim_accounts_get_all();
				while (a) {
					GaimAccount *account = a->data;
					XST_mPV(i++, gaim_account_get_username(account));
					a = a->next;
				}
			}
			break;

		case 6:
			{
				GList *a = gaim_accounts_get_all();
				while (a) {
					GaimAccount *account = a->data;
					XST_mIV(i++, gaim_account_get_protocol(account));
					a = a->next;
				}
			}
			break;

		case 7:
			{
				GaimConnection *gc =
					(GaimConnection *)SvIV(ST(1));

				if (g_list_find(gaim_connections_get_all(), gc))
					XST_mPV(i++, gc->prpl->info->name);
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
	gaim_notify_info(my_plugin, NULL, title, message);
	XSRETURN(0);
}

XS (XS_GAIM_buddy_list)
{
	GaimConnection *gc;
	struct buddy *buddy;
	struct group *g;
	GaimBlistNode *gnode,*bnode;
	int i = 0;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		g = (struct group *)gnode;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			buddy = (struct buddy *)bnode;
			if(buddy->account == gc->account)
				XST_mPV(i++, buddy->name);
		}
	}
	XSRETURN(i);
}

XS (XS_GAIM_online_list)
{
	GaimConnection *gc;
	struct buddy *b;
	struct group *g;
	GaimBlistNode *gnode,*bnode;
	int i = 0;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		g = (struct group *)gnode;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			b = (struct buddy *)bnode;
			if (b->account == gc->account && GAIM_BUDDY_IS_ONLINE(b)) XST_mPV(i++, b->name);
		}
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
	if (!strncasecmp(command, "signon", 6)) {
		int index = SvIV(ST(1));
		if (g_list_nth_data(gaim_accounts_get_all(), index))
		serv_login(g_list_nth_data(gaim_accounts_get_all(), index));
	} else if (!strncasecmp(command, "signoff", 7)) {
		GaimConnection *gc = (GaimConnection *)SvIV(ST(1));
		if (g_list_find(gaim_connections_get_all(), gc))
			gaim_connection_disconnect(gc);
		else
			gaim_connections_disconnect_all();
	} else if (!strncasecmp(command, "info", 4)) {
		GaimConnection *gc = (GaimConnection *)SvIV(ST(1));
		if (g_list_find(gaim_connections_get_all(), gc))
			serv_set_info(gc, SvPV(ST(2), junk));
	} else if (!strncasecmp(command, "away", 4)) {
		char *message = SvPV(ST(1), junk);
		static struct away_message a;
		g_snprintf(a.message, sizeof(a.message), "%s", message);
		do_away_message(NULL, &a);
	} else if (!strncasecmp(command, "back", 4)) {
		do_im_back(NULL, NULL);
	} else if (!strncasecmp(command, "idle", 4)) {
		GList *c = gaim_connections_get_all();
		GaimConnection *gc;

		while (c) {
			gc = (GaimConnection *)c->data;
			serv_set_idle(gc, SvIV(ST(1)));
			c = c->next;
		}
	} else if (!strncasecmp(command, "warn", 4)) {
		GList *c = gaim_connections_get_all();
		GaimConnection *gc;

		while (c) {
			gc = (GaimConnection *)c->data;
			serv_warn(gc, SvPV(ST(1), junk), SvIV(ST(2)));
			c = c->next;
		}
	}

	XSRETURN(0);
}

XS (XS_GAIM_user_info)
{
	GaimConnection *gc;
	unsigned int junk;
	struct buddy *buddy = NULL;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));
	if (g_list_find(gaim_connections_get_all(), gc))
		buddy = gaim_find_buddy(gc->account, SvPV(ST(1), junk));

	if (!buddy)
		XSRETURN(0);
	XST_mPV(0, buddy->name);
	XST_mPV(1, gaim_get_buddy_alias(buddy));
	XST_mPV(2, GAIM_BUDDY_IS_ONLINE(buddy) ? "Online" : "Offline");
	XST_mIV(3, buddy->evil);
	XST_mIV(4, buddy->signon);
	XST_mIV(5, buddy->idle);
	XSRETURN(6);
}

XS (XS_GAIM_write_to_conv)
{
	char *nick, *who, *what;
	GaimConversation *c;
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

	c = gaim_find_conversation(nick);

	if (!c)
		c = gaim_conversation_new(GAIM_CONV_IM, NULL, nick);

	gaim_conversation_write(c, who, what, -1, wflags, time(NULL));
	XSRETURN(0);
}

XS (XS_GAIM_serv_send_im)
{
	GaimConnection *gc;
	char *nick, *what;
	int isauto;
	int junk;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));
	nick = SvPV(ST(1), junk);
	what = SvPV(ST(2), junk);
	isauto = SvIV(ST(3));

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		XSRETURN(0);
		return;
	}
	serv_send_im(gc, nick, what, -1, isauto);
	XSRETURN(0);
}

XS (XS_GAIM_print_to_conv)
{
	GaimConnection *gc;
	char *nick, *what;
	int isauto;
	GaimConversation *c;
	unsigned int junk;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));
	nick = SvPV(ST(1), junk);
	what = SvPV(ST(2), junk);
	isauto = SvIV(ST(3));
	if (!g_list_find(gaim_connections_get_all(), gc)) {
		XSRETURN(0);
		return;
	}

	c = gaim_find_conversation(nick);

	if (!c)
		c = gaim_conversation_new(GAIM_CONV_IM, gc->account, nick);
	else
		gaim_conversation_set_account(c, gc->account);

	gaim_conversation_write(c, NULL, what, -1, 
				(WFLAG_SEND | (isauto ? WFLAG_AUTO : 0)), time(NULL));
	serv_send_im(gc, nick, what, -1, isauto ? IM_FLAG_AWAY : 0);
	XSRETURN(0);
}


	
XS (XS_GAIM_print_to_chat)
{
	GaimConnection *gc;
	int id;
	char *what;
	GaimConversation *b = NULL;
	GSList *bcs;
	unsigned int junk;
	dXSARGS;
	items = 0;

	gc = (GaimConnection *)SvIV(ST(0));
	id = SvIV(ST(1));
	what = SvPV(ST(2), junk);

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		XSRETURN(0);
		return;
	}
	bcs = gc->buddy_chats;
	while (bcs) {
		b = (GaimConversation *)bcs->data;

		if (gaim_chat_get_id(gaim_conversation_get_chat_data(b)) == id)
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (b)
		serv_chat_send(gc, id, what);
	XSRETURN(0);
}

static int
perl_event(GaimEvent event, void *unused, va_list args)
{
	char *buf[5] = { NULL, NULL, NULL, NULL, NULL }; /* Maximum of 5 args */
	void *arg1 = NULL, *arg2 = NULL, *arg3 = NULL, *arg4 = NULL, *arg5 = NULL;
	char tmpbuf1[16], tmpbuf2[16], tmpbuf3[1];
	GList *handler;
	struct _perl_event_handlers *data;
	int handler_return;

	arg1 = va_arg(args, void *);
	arg2 = va_arg(args, void *);
	arg3 = va_arg(args, void *);
	arg4 = va_arg(args, void *);
	arg5 = va_arg(args, void *);

	tmpbuf1[0] = '\0';
	tmpbuf2[0] = '\0';
	tmpbuf3[0] = '\0';

	/* Make a pretty array of char*'s with which to call perl functions */
	switch (event) {
	case event_connecting:
	case event_signon:
	case event_signoff:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		break;
	case event_away:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = ((GaimConnection *)arg1)->away ?
				((GaimConnection *)arg1)->away : tmpbuf2;
		break;
	case event_im_recv:
		if (!*(char**)arg2 || !*(char**)arg3) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = *(char **)arg2;
		buf[2] = *(char **)arg3;
		break;
	case event_im_send:
		if (!*(char**)arg3) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2 ? arg2 : tmpbuf3;
		buf[2] = *(char **)arg3;
		break;
	case event_buddy_signon:
	case event_buddy_signoff:
	case event_set_info:
	case event_buddy_away:
	case event_buddy_back:
	case event_buddy_idle:
	case event_buddy_unidle:
	case event_got_typing:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2;
		break;
	case event_chat_invited:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2;
		buf[2] = arg3;
		buf[3] = arg4;
		break;
	case event_chat_join:
	case event_chat_buddy_join:
	case event_chat_buddy_leave:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg2);
		buf[1] = tmpbuf2;
		buf[2] = arg3;
		break;
	case event_chat_leave:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg2);
		buf[1] = tmpbuf2;
		break;
	case event_chat_recv:
		if (!*(char**)arg3 || !*(char**)arg4) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg2);
		buf[1] = tmpbuf2;
		buf[2] = *(char **)arg3;
		buf[3] = *(char **)arg4;
		break;
	case event_chat_send_invite:
		if (!*(char**)arg4) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg2);
		buf[1] = tmpbuf2;
		buf[2] = arg3;
		buf[3] = *(char **)arg4;
		break;
	case event_chat_send:
		if (!*(char**)arg3) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg2);
		buf[1] = tmpbuf2;
		buf[2] = *(char **)arg3;
		break;
	case event_warned:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2 ? arg2 : tmpbuf3;
		g_snprintf(tmpbuf2, 16, "%d", (int)arg3);
		buf[2] = tmpbuf2;
		break;
	case event_quit:
	case event_blist_update:
		buf[0] = tmpbuf3;
		break;
	case event_new_conversation:
	case event_del_conversation:
		buf[0] = arg1;
		break;
	case event_im_displayed_sent:
		if (!*(char**)arg3) return 1;
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2;
		buf[2] = *(char **)arg3;
		break;
	case event_im_displayed_rcvd:
		g_snprintf(tmpbuf1, 16, "%lu", (unsigned long)arg1);
		buf[0] = tmpbuf1;
		buf[1] = arg2;
		buf[2] = arg3 ? arg3 : tmpbuf3;
		break;
	case event_draw_menu:
		/* we can't handle this usefully without gtk/perl bindings */
		return 0;
	default:
		gaim_debug(GAIM_DEBUG_WARNING, "perl",
				   "Someone forgot to handle %s in the perl binding\n",
				   gaim_event_get_name(event));
		return 0;
	}

	/* Call any applicable functions */
	for (handler = perl_event_handlers;
		 handler != NULL;
		 handler = handler->next) {

		data = handler->data;

		if (!strcmp(gaim_event_get_name(event), data->event_type)) {

			handler_return = execute_perl(data->handler_name, 5, buf);

			if (handler_return)
				return handler_return;
		}
	}

	/* Now make changes from perl scripts affect the real data */
	switch (event) {
	case event_im_recv:
		if (buf[1] != *(char **)arg2) {
			free(*(char **)arg2);
			*(char **)arg2 = buf[1];
		}
		if (buf[2] != *(char **)arg3) {
			free(*(char **)arg3);
			*(char **)arg3 = buf[2];
		}
		break;
	case event_im_send:
		if (buf[2] != *(char **)arg3) {
			free(*(char **)arg3);
			*(char **)arg3 = buf[2];
		}
		break;
	case event_chat_recv:
		if (buf[2] != *(char **)arg3) {
			free(*(char **)arg3);
			*(char **)arg3 = buf[2];
		}
		if (buf[3] != *(char **)arg4) {
			free(*(char **)arg4);
			*(char **)arg4 = buf[3];
		}
		break;
	case event_chat_send_invite:
		if (buf[3] != *(char **)arg4) {
			free(*(char **)arg4);
			*(char **)arg4 = buf[3];
		}
		break;
	case event_chat_send:
		if (buf[2] != *(char **)arg3) {
			free(*(char **)arg3);
			*(char **)arg3 = buf[2];
		}
		break;
	case event_im_displayed_sent:
		if (buf[2] != *(char **)arg3) {
			free(*(char **)arg3);
			*(char **)arg3 = buf[2];
		}
		break;
	default:
		break;
	}

	return 0;
}

XS (XS_GAIM_add_event_handler)
{
	unsigned int junk;
	struct _perl_event_handlers *handler;
	char *handle;
	GaimPlugin *plug;
	GList *p;
	dXSARGS;
	items = 0;
	
	handle = SvPV(ST(0), junk);

	for (p = gaim_plugins_get_all(); p != NULL; p = p->next) {
		plug = p->data;

		if (!strcmp(handle, plug->path))
			break;
	}

	if (p) {
		handler = g_new0(struct _perl_event_handlers, 1);
		handler->event_type = g_strdup(SvPV(ST(1), junk));
		handler->handler_name = g_strdup(SvPV(ST(2), junk));
		handler->plug = plug;
		perl_event_handlers = g_list_append(perl_event_handlers, handler);
		gaim_debug(GAIM_DEBUG_INFO, "perl",
				   "Registered perl event handler for %s\n",
				   handler->event_type);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Invalid handle (%s) registering perl event handler\n",
				   handle);
	}
	
	XSRETURN_EMPTY;
}

XS (XS_GAIM_remove_event_handler)
{
	unsigned int junk;
	struct _perl_event_handlers *ehn;
	GList *cur = perl_event_handlers;
	dXSARGS;
	items = 0;

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

static int
perl_timeout(gpointer data)
{
	char *atmp[2] = { NULL, NULL };
	struct _perl_timeout_handlers *handler = data;

	atmp[0] = escape_quotes(handler->handler_args);
	execute_perl(handler->handler_name, 1, atmp);

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
	char *handle;
	GaimPlugin *plug;
	GList *p;
	
	dXSARGS;
	items = 0;
	
	handle = SvPV(ST(0), junk);

	for (p = gaim_plugins_get_all(); p != NULL; p = p->next) {
		plug = p->data;

		if (!strcmp(handle, plug->path))
			break;
	}

	if (p) {
		handler = g_new0(struct _perl_timeout_handlers, 1);
		timeout = 1000 * SvIV(ST(1));
		gaim_debug(GAIM_DEBUG_INFO, "perl",
				   "Adding timeout for %ld seconds.\n", timeout/1000);
		handler->plug = plug;
		handler->handler_name = g_strdup(SvPV(ST(2), junk));
		handler->handler_args = g_strdup(SvPV(ST(3), junk));
		perl_timeout_handlers = g_list_append(perl_timeout_handlers, handler);
		handler->iotag = g_timeout_add(timeout, perl_timeout, handler);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Invalid handle (%s) in adding perl timeout handler.",
				   handle);
	}
	XSRETURN_EMPTY;
}

XS (XS_GAIM_play_sound)
{
	int id;
	dXSARGS;

	items = 0;

	id = SvIV(ST(0));

	gaim_sound_play_event(id);

	XSRETURN_EMPTY;
}

static gboolean
probe_perl_plugin(GaimPlugin *plugin)
{
	/* XXX This would be much faster if I didn't create a new
	 *     PerlInterpreter every time I probed a plugin */

	GaimPluginInfo *info;
	PerlInterpreter *prober = perl_alloc();
	char *argv[] = {"", plugin->path };
	int count;
	gboolean status = TRUE;

	perl_construct(prober);
	perl_parse(prober, xs_init, 2, argv, NULL);
	
	{
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);

		count = perl_call_pv("description", G_NOARGS | G_ARRAY | G_EVAL);
		SPAGAIN;

		if (count == 6) {
			info = g_new0(GaimPluginInfo, 1);

			info->api_version  = 2;
			info->type         = GAIM_PLUGIN_STANDARD;

			info->dependencies = g_list_append(info->dependencies,
											   PERL_PLUGIN_ID);

			POPp; /* iconfile */

			info->homepage    = g_strdup(POPp);
			info->author      = g_strdup(POPp);
			info->description = g_strdup(POPp);
			info->version     = g_strdup(POPp);
			info->name        = g_strdup(POPp);

			plugin->info = info;

			if (!gaim_plugin_register(plugin))
				status = FALSE;
		}
		else
			status = FALSE;

		PUTBACK;
		FREETMPS;
		LEAVE;
	}

	perl_destruct(prober);
	perl_free(prober);

	return status;
}

static gboolean
load_perl_plugin(GaimPlugin *plugin)
{
	perl_load_file(plugin->path, plugin);

	return TRUE;
}

static gboolean
unload_perl_plugin(GaimPlugin *plugin)
{
	perl_unload_file(plugin);

	return TRUE;
}

static void
destroy_perl_plugin(GaimPlugin *plugin)
{
	if (plugin->info != NULL) {
		g_free(plugin->info->name);
		g_free(plugin->info->version);
		g_free(plugin->info->description);
		g_free(plugin->info->author);
		g_free(plugin->info->homepage);
	}
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	perl_end();

	return TRUE;
}

static GaimPluginLoaderInfo loader_info =
{
	NULL,                                             /**< exts           */

	probe_perl_plugin,                                /**< probe          */
	load_perl_plugin,                                 /**< load           */
	unload_perl_plugin,                               /**< unload         */
	destroy_perl_plugin,                              /**< destroy        */
	perl_event                                        /**< broadcast      */
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_LOADER,                               /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	PERL_PLUGIN_ID,                                   /**< id             */
	N_("Perl Plugin Loader"),                         /**< name           */
	VERSION,                                          /**< version        */
	N_("Provides support for loading perl plugins."), /**< summary        */
	N_("Provides support for loading perl plugins."), /**< description    */
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&loader_info                                      /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	my_plugin = plugin;

	loader_info.exts = g_list_append(loader_info.exts, "pl");
}

GAIM_INIT_PLUGIN(perl, init_plugin, info);
