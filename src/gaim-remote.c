/*
 * gaim-remote
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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

#include "internal.h"

#include <getopt.h>

#include <gaim-remote/remote.h>

/*To be implemented:
	     "       info                     Show information about connected accounts\n"
	     "       list                     Print buddy list\n"
	     "       ison                     Show presence state of your buddy\n"
	     "       convo                    Open a new conversation window\n"
	     "       add                      Add buddy to buddy list\n"
	     "       remove                   Remove buddy from list\n"
	     "       -q, --quiet              Send message without showing a conversation\n"
	     "                                window\n"
*/

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
	char *message, *to, *from, *protocol;
	/*int protocol;*/
};
struct remoteopts opts;

/*
 * Prints a message to the terminal/shell/console.
 * We try to convert "text" from UTF-8 to the user's locale.
 * If that fails then UTF-8 is used as a fallback.
 *
 * If channel is 1, the message is printed to stdout.
 * if channel is 2, the message is printed to stderr.
 */ 
static void
message(char *text, int channel)
{
	char *text_conv = NULL,*text_output;
	GError *error = NULL;

	text_conv = g_locale_from_utf8(text, -1, NULL, NULL, &error);

	if (text_conv == NULL) {
		g_warning("%s\n", error->message);
		g_error_free(error);
	}

	text_output = (text_conv ? text_conv : text);

	switch (channel) {
		case 1:
			puts(text_output);
			break;
		case 2:
			fputs(text_output, stderr);
			break;
		default:
			break;
	}

	if (text_conv)
		g_free(text_conv);
}

static void
show_remote_usage(const char *name)
{
	char *text = NULL;

	text = g_strdup_printf(_("Usage: %s command [OPTIONS] [URI]\n\n"
		"    COMMANDS:\n"
		"       uri                      Handle AIM: URI\n"
		"       away                     Popup the away dialog with the default message\n"
		"       back                     Remove the away dialog\n"
		"       send                     Send message\n"
		"       quit                     Close running copy of Gaim\n\n"
		"    OPTIONS:\n"
		"       -m, --message=MESG       Message to send or show in conversation window\n"
		"       -t, --to=SCREENNAME      Select a target for command\n"
		"       -p, --protocol=PROTO     Specify protocol to use\n"
		"       -f, --from=SCREENNAME    Specify screen name to use\n"
		"       -h, --help [command]     Show help for command\n"), name);

	message(text, 1);
	g_free(text);

	return;
}

int
get_options(int argc, char *argv[])
{
	int i;

	memset(&opts, 0, sizeof(opts));
	/*opts.protocol = -1;*/

	while ((i=getopt_long(argc, argv, "m:t:p:f:qh", longopts, NULL)) != -1) {
		switch (i) {
		case 'm':
			opts.message = optarg;
			break;
		case 't':
			opts.to = optarg;
			break;
		case 'p':
			opts.protocol = optarg;
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

	if (opts.help)
		return 0;

	/* And we can have another argument--the URI. */
	/* but only if we're using the uri command. */
	if (!strcmp(opts.command, "uri")) {
		if (argc-optind == 1)
			opts.uri = g_strdup(argv[optind++]);
		else
			return 1;
	} else if (optind == argc)
		return 0;
	else
		return 1;

	return 0;			
}

static int
send_generic_command(guchar type, guchar subtype) {
	int fd = 0;
	GaimRemotePacket *p = NULL;

	fd = gaim_remote_session_connect(0);
	if (fd < 0) {
		message(_("Gaim not running (on session 0)\nIs the \"Remote Control\" plugin loaded?\n"), 2);
		return 1;
	}
	p = gaim_remote_packet_new(type, subtype);
	gaim_remote_session_send_packet(fd, p);
	close(fd);
	gaim_remote_packet_free(p);

	return 0;
}

static int
send_command_uri() {
	int fd = 0;
	GaimRemotePacket *p = NULL;

	fd = gaim_remote_session_connect(0);
	if (fd < 0) {
		message(_("Gaim not running (on session 0)\nIs the \"Remote Control\" plugin loaded?\n"), 2);
		return 1;
	}
	p = gaim_remote_packet_new(CUI_TYPE_REMOTE, CUI_REMOTE_URI);
	gaim_remote_packet_append_string(p, opts.uri);
	gaim_remote_session_send_packet(fd, p);
	close(fd);
	gaim_remote_packet_free(p);

	return 0;
}

static int
send_command_send() {
	int fd = 0;
	GaimRemotePacket *p = NULL;
	char temp[10003]; /*TODO: Future implementation should send packets instead */

	fd = gaim_remote_session_connect(0);
	if (fd < 0) {
		message(_("Gaim not running (on session 0)\nIs the \"Remote Control\" plugin loaded?\n"), 2);
		return 1;
	}
	p = gaim_remote_packet_new(CUI_TYPE_REMOTE, CUI_REMOTE_SEND);

	/*Format is as follows:
	 *Each string has a 4 character 'header' containing the length of the string
	 *The strings are: To, From, Protocol name, Message
	 *Following the message is the quiet flag, expressed in a single int (0/1)
	 *Because the header is 4 characters long, there is a 9999 char limit on any
	 *given string, though none of these strings should be exceeding this.
	 *-JBS 
	 */

	if(opts.to && *opts.to && opts.from && *opts.from && opts.protocol && *opts.protocol && opts.message && *opts.message  && (strlen(opts.to) <10000) && (strlen(opts.from) <10000) && (strlen(opts.protocol) <20) && (strlen(opts.message) <10000) ){ 
		sprintf(temp, "%04d%s", strlen(opts.to), opts.to);
		gaim_remote_packet_append_string(p, temp);
		sprintf(temp, "%04d%s", strlen(opts.from), opts.from);
		gaim_remote_packet_append_string(p, temp);
		sprintf(temp, "%04d%s", strlen(opts.protocol), opts.protocol);
		gaim_remote_packet_append_string(p, temp);
		sprintf(temp, "%04d%s", strlen(opts.message), opts.message);
		gaim_remote_packet_append_string(p, temp);
		sprintf(temp, "%d", 0);/*quiet flag - off for now*/
		gaim_remote_packet_append_string(p, temp);

		gaim_remote_session_send_packet (fd, p);
		close(fd);
		gaim_remote_packet_free(p);
		return 0;
	}else{
		message(_("Insufficient arguments (-t, -f, -p, & -m are all required) or arguments greater than 9999 chars\n"), 2);
		close(fd);
		gaim_remote_packet_free(p);
 		return 1;
 	}

}

static void
show_longhelp( char *name, char *command)
{
	if (!strcmp(command, "uri")) {
		message(_("\n"
		       "Using AIM: URIs:\n"
		       "Sending an IM to a screen name:\n"
		       "	gaim-remote uri 'aim:goim?screenname=Penguin&message=hello+world'\n"
		       "In this case, 'Penguin' is the screen name we wish to IM, and 'hello world'\n"
		       "is the message to be sent.  '+' must be used in place of spaces.\n"
		       "Please note the quoting used above - if you run this from a shell the '&'\n"
		       "needs to be escaped, or the command will stop at that point.\n"
		       "Also,the following will just open a conversation window to a screen name,\n"
		       "with no message:\n"
		       "	gaim-remote uri 'aim:goim?screenname=Penguin'\n\n"
		       "Joining a chat:\n"
		       "	gaim-remote uri 'aim:gochat?roomname=PenguinLounge'\n"
		       "...joins the 'PenguinLounge' chat room.\n\n"
		       "Adding a buddy to your buddy list:\n"
		       "	gaim-remote uri 'aim:addbuddy?screenname=Penguin'\n"
			  "...prompts you to add 'Penguin' to your buddy list.\n"), 1);
	}

	else if (!strcmp(command, "quit")) {
		message(_("\nClose running copy of Gaim\n"), 1);
	}

	else if (!strcmp(command, "away")) {
		message(_("\nMark all accounts as \"away\" with the default message.\n"), 1);
	}

	else if (!strcmp(command, "back")) {
		message(_("\nSet all accounts as not away.\n"), 1);
	}

	else if (!strcmp(command, "send")) {
		message(_("\nSend instant message\n"), 1);
	}

	else {
		show_remote_usage(name);
	}
}

int main(int argc, char *argv[])
{
#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

	if (get_options(argc, argv)) {
		show_remote_usage(argv[0]);
		return 0;
	}

	if (!strcmp(opts.command, "uri")) {
		if (opts.help)
			show_longhelp(argv[0], "uri");
		else
			return send_command_uri();
	}

	else if (!strcmp(opts.command, "send")) {
		if (opts.help)
			show_longhelp(argv[0], "send");
		else
			return send_command_send();
	}

	else if (!strcmp(opts.command, "away")) {
		if (opts.help)
			show_longhelp(argv[0], "away");
		else
			return send_generic_command(CUI_TYPE_USER, CUI_USER_AWAY);
	}

	else if (!strcmp(opts.command, "back")) {
		if (opts.help)
			show_longhelp(argv[0], "back");
		else
			return send_generic_command(CUI_TYPE_USER, CUI_USER_BACK);
	}

	else if (!strcmp(opts.command, "quit")) {
		if (opts.help)
			show_longhelp(argv[0], "quit");
		else
			return send_generic_command(CUI_TYPE_META, CUI_META_QUIT);
	}

	else {
		show_remote_usage(argv[0]);
		return 1;
	}
	
	return 0;
}
