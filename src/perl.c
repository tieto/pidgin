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
#undef PACKAGE /* no idea why, just following X-Chat */

/* #ifdef USE_PERL */
#if 0 /* this isn't ready for prime-time yet. not even the 3-am shows. */

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

static PerlInterpreter *my_perl = NULL;

XS (XS_AIM_register); /* so far so good */
XS (XS_AIM_add_message_handler); /* um... */
XS (XS_AIM_add_command_handler); /* once again, um... */
XS (XS_AIM_add_print_handler); /* can i really do this? */
XS (XS_AIM_add_timeout_handler); /* ok, this i can do */
XS (XS_AIM_print); /* how am i going to do this */
XS (XS_AIM_print_with_channel); /* FIXME! this needs to be renamed */
XS (XS_AIM_send_raw); /* this i can do for toc, but for oscar... ? */
XS (XS_AIM_command); /* this should be easier */
XS (XS_AIM_command_with_server); /* FIXME: this should probably be removed */
XS (XS_AIM_channel_list); /* probably return conversation list */
XS (XS_AIM_server_list); /* huh? does this apply? */
XS (XS_AIM_user_list); /* return the buddy list */
XS (XS_AIM_user_info); /* we'll see.... */
XS (XS_AIM_ignore_list); /* deny list? */
XS (XS_AIM_dcc_list); /* wha? */
XS (XS_AIM_get_info); /* this i can do too */

void perl_init(int autoload)
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
	perl_parse(my_perl, NULL, 3, perl_args, NULL);
#ifndef HAVE_PERL_EVAL_PV
	perl_eval_pv(load_file, TRUE);
#else
	Perl_eval_pv(load_file, TRUE);
#endif

	newXS("AIM::register", XS_AIM_register, "AIM");
	newXS("AIM::add_message_handler", XS_AIM_add_message_handler, "AIM");
	newXS("AIM::add_command_handler", XS_AIM_add_command_handler, "AIM");
	newXS("AIM::add_print_handler", XS_AIM_add_print_handler, "AIM");
	newXS("AIM::add_timeout_handler", XS_AIM_add_timeout_handler, "AIM");
	newXS("AIM::print", XS_AIM_print, "AIM");
	newXS("AIM::print_with_channel", XS_AIM_print_with_channel, "AIM");
	newXS("AIM::send_raw", XS_AIM_send_raw, "AIM");
	newXS("AIM::command", XS_AIM_command, "AIM");
	newXS("AIM::command_with_server", XS_AIM_command_with_server, "AIM");
	newXS("AIM::channel_list", XS_AIM_channel_list, "AIM");
	newXS("AIM::server_list", XS_AIM_server_list, "AIM");
	newXS("AIM::user_list", XS_AIM_user_list, "AIM");
	newXS("AIM::user_info", XS_AIM_user_info, "AIM");
	newXS("AIM::ignore_list", XS_AIM_ignore_list, "AIM");
	newXS("AIM::dcc_list", XS_AIM_dcc_list, "AIM");
	newXS("AIM::get_info", XS_AIM_get_info, "AIM");

	/* FIXME */
	if (autoload) {
		/* for each *.pl in ~/.gaim/scripts (whatever), autoload file */
	}
}

void perl_end()
{
	if (my_perl != NULL) {
		perl_destruct(my_perl);
		perl_free(my_perl);
		my_perl = NULL;
	}
}

#endif /* USE_PERL */
