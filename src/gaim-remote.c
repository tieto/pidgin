/*
 * gaim-remote
 *
 * Copyright (C) 2002, Sean Egan <bj91704@binghamton.edu>
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

#include "gaim.h"
#include <getopt.h>
#include <unistd.h>
#include "gaim-socket.h"

void show_remote_usage(char *name) 
{
	printf(_("Usage: %s command [OPTIONS] [URI]\n\n"
		 
	     "    COMMANDS:\n"
	     "       uri                      Handle AIM:// URI\n"
	     "       info                     Show information about connected accounts\n"
	     "       list                     Print buddy list\n"
	     "       ison                     Show presence state of your buddy\n"
	     "       convo                    Open a new conversation window\n"
	     "       send                     Send message\n"
	     "       add                      Add buddy to buddy list\n"
	     "       remove                   Remove buddy from list\n"
	     "       quit                     Close running copy of Gaim\n\n"
		 
	     "    OPTIONS:\n"
	     "       -m, --message=MESG       Message to send or show in conversation window\n"
	     "       -t, --to=SCREENNAME      Select a target for command\n"
	     "       -p, --protocol=PROTO     Specify protocol to use\n"
	     "       -f, --from=SCREENNAME    Specify screenname to use\n"
	     "       -q, --quiet              Send message without showing a conversation\n" 
	     "                                window\n"
	     "       -h, --help               Show help for command\n"), name);
	return;
}

static struct option longopts[] = {
	{"message", required_argument, NULL, 'm'},
	{"to",      required_argument, NULL, 't'},
	{"protocol",required_argument, NULL, 'p'},
	{"from",    required_argument, NULL, 'f'},
	{"quiet",   no_argument,       NULL, 'q'},
	{"help",    no_argument,       NULL, 'h'},
	{0,0,0,0}
};

struct remoteopts {
	char *command;
	char *uri;
	gboolean help, quiet;
	char *message, *to, *from;
	int protocol;
};


struct remoteopts opts;
int get_options(int argc, char *argv[])
{
	int i;
	memset(&opts, 0, sizeof(opts));
	opts.protocol = -1;
	while ((i=getopt_long(argc, argv, "m:t:p:f:qh", longopts, NULL)) != -1) {
		switch (i) {
		case 'm':
			opts.message = optarg;
			break;
		case 't':
			opts.to = optarg;
			break;
		case 'p':
			/* Do stuff here. */
			break;
		case 'f':
			opts.from = optarg;
			break;
		case 'q':
			opts.quiet = TRUE;
			break;
		case 'h':
			opts.help = TRUE;
			break;
		}
	}
	
	/* We must have non getopt'ed argument-- the command */
	if (optind < argc)
		opts.command = g_strdup(argv[optind++]);
	else
		return 1;

	/* And we can have another argument--the URI. */
	if (optind < argc) {
		/* but only if we're using the uri command. */
		if (!strcmp(opts.command, "uri"))
			opts.uri = g_strdup(argv[optind++]);
		else
			return 1;
		
		/* and we can't have any others. */
		if (optind < argc)
			return 1;
	}
	
	return 0;
}


int command_uri() {
	int fd = 0;
	struct gaim_cui_packet *p = NULL;
	fd = gaim_connect_to_session(0);
	if (!fd) {
		fprintf(stderr, "Gaim not running (on session 0)\n");
		return 1;
	}
	p = cui_packet_new(CUI_TYPE_REMOTE, CUI_REMOTE_URI);
	cui_packet_append_string(p, opts.uri);
	cui_send_packet (fd, p);
	close(fd);
	cui_packet_free(p);
	return 0;
}

int command_quit() {
	int fd = 0;
	struct gaim_cui_packet *p = NULL;
	fd = gaim_connect_to_session(0);
	if (!fd) {
		fprintf(stderr, "Gaim not running (on session 0)\n");
		return 1;
	}
	p = cui_packet_new(CUI_TYPE_META, CUI_META_QUIT);
	cui_send_packet (fd, p);
	close(fd);
	cui_packet_free(p);
	return 0;
}

int command_info(){
	fprintf(stderr, "Info not yet implemented\n");
    return 1;
}

int main (int argc, char *argv[])
{

	if (get_options(argc, argv)) {
		show_remote_usage(argv[0]);
		return 0;
	}
	
	
	if (!strcmp(opts.command, "uri")) {
		return command_uri();
	} else if (!strcmp(opts.command, "info")) {
		return command_info();
	} else if (!strcmp(opts.command, "quit")) {
		return command_quit();
	} else {
		show_remote_usage(argv[0]);
		return 1;
	}
	
	return 0;
}
