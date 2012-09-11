/*
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"
#include "pidgin.h"

#include "account.h"
#include "conversation.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkdocklet.h"
#include "gtkeventloop.h"
#include "gtkft.h"
#include "gtkidle.h"
#include "gtklog.h"
#include "gtkmedia.h"
#include "gtknotify.h"
#include "gtkplugin.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkrequest.h"
#include "gtkroomlist.h"
#include "gtksavedstatuses.h"
#include "gtksession.h"
#include "gtksmiley.h"
#include "gtksound.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkwhiteboard.h"

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif

#include <getopt.h>


#ifdef HAVE_SIGNAL_H

/*
 * Lists of signals we wish to catch and those we wish to ignore.
 * Each list terminated with -1
 */
static const int catch_sig_list[] = {
	SIGSEGV,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	-1
};

static const int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif

static void
dologin_named(const char *name)
{
	PurpleAccount *account;
	char **names;
	int i;

	if (name != NULL) { /* list of names given */
		names = g_strsplit(name, ",", 64);
		for (i = 0; names[i] != NULL; i++) {
			account = purple_accounts_find(names[i], NULL);
			if (account != NULL) { /* found a user */
				purple_account_set_enabled(account, PIDGIN_UI, TRUE);
			}
		}
		g_strfreev(names);
	} else { /* no name given, use the first account */
		GList *accounts;

		accounts = purple_accounts_get_all();
		if (accounts != NULL)
		{
			account = (PurpleAccount *)accounts->data;
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}
}

#ifdef HAVE_SIGNAL_H
static char *segfault_message;

static int signal_sockets[2];

static void sighandler(int sig);

static void sighandler(int sig)
{
	ssize_t written;

	/*
	 * We won't do any of the heavy lifting for the signal handling here
	 * because we have no idea what was interrupted.  Previously this signal
	 * handler could result in some calls to malloc/free, which can cause
	 * deadlock in libc when the signal handler was interrupting a previous
	 * malloc or free.  So instead we'll do an ugly hack where we write the
	 * signal number to one end of a socket pair.  The other half of the
	 * socket pair is watched by our main loop.  When the main loop sees new
	 * data on the socket it reads in the signal and performs the appropriate
	 * action without fear of interrupting stuff.
	 */
	if (sig == SIGSEGV) {
		fprintf(stderr, "%s", segfault_message);
		abort();
		return;
	}

	written = write(signal_sockets[0], &sig, sizeof(int));
	if (written < 0 || written != sizeof(int)) {
		/* This should never happen */
		purple_debug_error("sighandler", "Received signal %d but only "
				"wrote %" G_GSSIZE_FORMAT " bytes out of %"
				G_GSIZE_FORMAT ": %s\n",
				sig, written, sizeof(int), g_strerror(errno));
		exit(1);
	}
}

static gboolean
mainloop_sighandler(GIOChannel *source, GIOCondition cond, gpointer data)
{
	GIOStatus stat;
	int sig;
	gsize bytes_read;
	GError *error = NULL;

	/* read the signal number off of the io channel */
	stat = g_io_channel_read_chars(source, (gchar *)&sig, sizeof(int),
			&bytes_read, &error);
	if (stat != G_IO_STATUS_NORMAL) {
		purple_debug_error("sighandler", "Signal callback failed to read "
				"from signal socket: %s", error->message);
		purple_core_quit();
		return FALSE;
	}

	purple_debug_warning("sighandler", "Caught signal %d\n", sig);
	purple_core_quit();

	return TRUE;
}
#endif

static int
ui_main(void)
{
#ifndef _WIN32
	GList *icons = NULL;
	GdkPixbuf *icon = NULL;
	char *icon_path;
	int i;
	struct {
		const char *dir;
		const char *filename;
	} icon_sizes[] = {
		{"16x16", "pidgin.png"},
		{"24x24", "pidgin.png"},
		{"32x32", "pidgin.png"},
		{"48x48", "pidgin.png"},
		{"scalable", "pidgin.svg"}
	};

#endif

	pidgin_themes_init();

	pidgin_blist_setup_sort_methods();

#ifndef _WIN32
	/* use the nice PNG icon for all the windows */
	for(i=0; i<G_N_ELEMENTS(icon_sizes); i++) {
		icon_path = g_build_filename(DATADIR, "icons", "hicolor", icon_sizes[i].dir, "apps", icon_sizes[i].filename, NULL);
		icon = pidgin_pixbuf_new_from_file(icon_path);
		g_free(icon_path);
		if (icon) {
			icons = g_list_append(icons,icon);
		} else {
			purple_debug_error("ui_main",
					"Failed to load the default window icon (%spx version)!\n", icon_sizes[i].dir);
		}
	}
	if(NULL == icons) {
		purple_debug_error("ui_main", "Unable to load any size of default window icon!\n");
	} else {
		gtk_window_set_default_icon_list(icons);

		g_list_foreach(icons, (GFunc)g_object_unref, NULL);
		g_list_free(icons);
	}
#endif

	return 0;
}

static void
debug_init(void)
{
	purple_debug_set_ui_ops(pidgin_debug_get_ui_ops());
	pidgin_debug_init();
}

static void
pidgin_ui_init(void)
{
	pidgin_stock_init();

	/* Set the UI operation structures. */
	purple_accounts_set_ui_ops(pidgin_accounts_get_ui_ops());
	purple_xfers_set_ui_ops(pidgin_xfers_get_ui_ops());
	purple_blist_set_ui_ops(pidgin_blist_get_ui_ops());
	purple_notify_set_ui_ops(pidgin_notify_get_ui_ops());
	purple_privacy_set_ui_ops(pidgin_privacy_get_ui_ops());
	purple_request_set_ui_ops(pidgin_request_get_ui_ops());
	purple_sound_set_ui_ops(pidgin_sound_get_ui_ops());
	purple_connections_set_ui_ops(pidgin_connections_get_ui_ops());
	purple_whiteboard_set_ui_ops(pidgin_whiteboard_get_ui_ops());
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
	purple_idle_set_ui_ops(pidgin_idle_get_ui_ops());
#endif

	pidgin_account_init();
	pidgin_connection_init();
	pidgin_blist_init();
	pidgin_status_init();
	pidgin_conversations_init();
	pidgin_pounces_init();
	pidgin_privacy_init();
	pidgin_xfers_init();
	pidgin_roomlist_init();
	pidgin_log_init();
	pidgin_docklet_init();
	pidgin_smileys_init();
	pidgin_utils_init();
	pidgin_medias_init();
	pidgin_notify_init();
}

static GHashTable *ui_info = NULL;

static void
pidgin_quit(void)
{
#ifdef USE_SM
	/* unplug */
	pidgin_session_end();
#endif

	/* Uninit */
	pidgin_utils_uninit();
	pidgin_notify_uninit();
	pidgin_smileys_uninit();
	pidgin_conversations_uninit();
	pidgin_status_uninit();
	pidgin_docklet_uninit();
	pidgin_blist_uninit();
	pidgin_connection_uninit();
	pidgin_account_uninit();
	pidgin_xfers_uninit();
	pidgin_debug_uninit();

	if(NULL != ui_info)
		g_hash_table_destroy(ui_info);

	/* and end it all... */
	gtk_main_quit();
}

static GHashTable *pidgin_ui_get_info(void)
{
	if(NULL == ui_info) {
		ui_info = g_hash_table_new(g_str_hash, g_str_equal);

		g_hash_table_insert(ui_info, "name", (char*)PIDGIN_NAME);
		g_hash_table_insert(ui_info, "version", VERSION);
		g_hash_table_insert(ui_info, "website", "http://pidgin.im");
		g_hash_table_insert(ui_info, "dev_website", "http://developer.pidgin.im");
		g_hash_table_insert(ui_info, "client_type", "pc");

		/*
		 * This is the client key for "Pidgin."  It is owned by the AIM
		 * account "markdoliner."  Please don't use this key for other
		 * applications.  You can either not specify a client key, in
		 * which case the default "libpurple" key will be used, or you
		 * can try to register your own at the AIM or ICQ web sites
		 * (although this functionality was removed at some point, it's
		 * possible it has been re-added).  AOL's old key management
		 * page is http://developer.aim.com/manageKeys.jsp
		 */
		g_hash_table_insert(ui_info, "prpl-aim-clientkey", "ma1cSASNCKFtrdv9");
		g_hash_table_insert(ui_info, "prpl-icq-clientkey", "ma1cSASNCKFtrdv9");

		/*
		 * This is the distid for Pidgin, given to us by AOL.  Please
		 * don't use this for other applications.  You can just not
		 * specify a distid and libpurple will use a default.
		 */
		g_hash_table_insert(ui_info, "prpl-aim-distid", GINT_TO_POINTER(1550));
		g_hash_table_insert(ui_info, "prpl-icq-distid", GINT_TO_POINTER(1550));
	}

	return ui_info;
}

static PurpleCoreUiOps core_ops =
{
	pidgin_prefs_init,
	debug_init,
	pidgin_ui_init,
	pidgin_quit,
	pidgin_ui_get_info,
	NULL,
	NULL,
	NULL
};

static PurpleCoreUiOps *
pidgin_core_get_ui_ops(void)
{
	return &core_ops;
}

static void
show_usage(const char *name, gboolean terse)
{
	char *text;

	if (terse) {
		text = g_strdup_printf(_("%s %s. Try `%s -h' for more information.\n"), PIDGIN_NAME, DISPLAY_VERSION, name);
	} else {
		GString *str = g_string_new(NULL);
		g_string_append_printf(str, "%s %s\n", PIDGIN_NAME, DISPLAY_VERSION);
		g_string_append_printf(str, _("Usage: %s [OPTION]...\n\n"), name);
		g_string_append_printf(str, "  -c, --config=%s    %s\n",
				_("DIR"), _("use DIR for config files"));
		g_string_append_printf(str, "  -d, --debug         %s\n",
				_("print debugging messages to stdout"));
		g_string_append_printf(str, "  -f, --force-online  %s\n",
				_("force online, regardless of network status"));
		g_string_append_printf(str, "  -h, --help          %s\n",
				_("display this help and exit"));
		g_string_append_printf(str, "  -m, --multiple      %s\n",
				_("allow multiple instances"));
		g_string_append_printf(str, "  -n, --nologin       %s\n",
				_("don't automatically login"));
		g_string_append_printf(str, "  -l, --login[=%s]  %s\n",
				_("NAME"),
				_("enable specified account(s) (optional argument NAME\n"
				  "                      "
				  "specifies account(s) to use, separated by commas.\n"
				  "                      "
				  "Without this only the first account will be enabled)."));
#ifndef WIN32
		g_string_append_printf(str, "  --display=DISPLAY   %s\n",
				_("X display to use"));
#endif /* !WIN32 */
		g_string_append_printf(str, "  -v, --version       %s\n",
				_("display the current version and exit"));
		text = g_string_free(str, FALSE);
	}

	purple_print_utf8_to_console(stdout, text);
	g_free(text);
}

/* FUCKING GET ME A TOWEL! */
#ifdef _WIN32
/* suppress gcc "no previous prototype" warning */
int pidgin_main(HINSTANCE hint, int argc, char *argv[]);
int pidgin_main(HINSTANCE hint, int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	gboolean opt_force_online = FALSE;
	gboolean opt_help = FALSE;
	gboolean opt_login = FALSE;
	gboolean opt_nologin = FALSE;
	gboolean opt_version = FALSE;
	gboolean opt_si = TRUE;     /* Check for single instance? */
	char *opt_config_dir_arg = NULL;
	char *opt_login_arg = NULL;
	char *opt_session_arg = NULL;
	char *search_path;
	GList *accounts;
#ifdef HAVE_SIGNAL_H
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	RETSIGTYPE (*prev_sig_disp)(int);
	char errmsg[BUFSIZ];
	GIOChannel *signal_channel;
	GIOStatus signal_status;
	guint signal_channel_watcher;
#ifndef DEBUG
	char *segfault_message_tmp;
#endif
	GError *error;
#endif
	int opt;
	gboolean gui_check;
	gboolean debug_enabled;
	GList *active_accounts;
	struct stat st;

	struct option long_options[] = {
		{"config",       required_argument, NULL, 'c'},
		{"debug",        no_argument,       NULL, 'd'},
		{"force-online", no_argument,       NULL, 'f'},
		{"help",         no_argument,       NULL, 'h'},
		{"login",        optional_argument, NULL, 'l'},
		{"multiple",     no_argument,       NULL, 'm'},
		{"nologin",      no_argument,       NULL, 'n'},
		{"session",      required_argument, NULL, 's'},
		{"version",      no_argument,       NULL, 'v'},
		{"display",      required_argument, NULL, 'D'},
		{"sync",         no_argument,       NULL, 'S'},
		{0, 0, 0, 0}
	};

#ifdef DEBUG
	debug_enabled = TRUE;
#else
	debug_enabled = FALSE;
#endif

	/* Initialize GThread before calling any Glib or GTK+ functions. */
	g_thread_init(NULL);

	g_set_prgname("Pidgin");

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

#ifdef HAVE_SETLOCALE
	/* Locale initialization is not complete here.  See gtk_init_check() */
	setlocale(LC_ALL, "");
#endif

#ifdef HAVE_SIGNAL_H

#ifndef DEBUG
		/* We translate this here in case the crash breaks gettext. */
		segfault_message_tmp = g_strdup_printf(_(
			"%s %s has segfaulted and attempted to dump a core file.\n"
			"This is a bug in the software and has happened through\n"
			"no fault of your own.\n\n"
			"If you can reproduce the crash, please notify the developers\n"
			"by reporting a bug at:\n"
			"%ssimpleticket/\n\n"
			"Please make sure to specify what you were doing at the time\n"
			"and post the backtrace from the core file.  If you do not know\n"
			"how to get the backtrace, please read the instructions at\n"
			"%swiki/GetABacktrace\n"),
			PIDGIN_NAME, DISPLAY_VERSION, PURPLE_DEVEL_WEBSITE, PURPLE_DEVEL_WEBSITE
		);

		/* we have to convert the message (UTF-8 to console
		   charset) early because after a segmentation fault
		   it's not a good practice to allocate memory */
		error = NULL;
		segfault_message = g_locale_from_utf8(segfault_message_tmp,
						      -1, NULL, NULL, &error);
		if (segfault_message != NULL) {
			g_free(segfault_message_tmp);
		}
		else {
			/* use 'segfault_message_tmp' (UTF-8) as a fallback */
			g_warning("%s\n", error->message);
			g_error_free(error);
			segfault_message = segfault_message_tmp;
		}
#else
		/* Don't mark this for translation. */
		segfault_message = g_strdup(
			"Hi, user.  We need to talk.\n"
			"I think something's gone wrong here.  It's probably my fault.\n"
			"No, really, it's not you... it's me... no no no, I think we get along well\n"
			"it's just that.... well, I want to see other people.  I... what?!?  NO!  I \n"
			"haven't been cheating on you!!  How many times do you want me to tell you?!  And\n"
			"for the last time, it's just a rash!\n"
		);
#endif

	/*
	 * Create a socket pair for receiving unix signals from a signal
	 * handler.
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, signal_sockets) < 0) {
		perror("Failed to create sockets for GLib signal handling");
		exit(1);
	}
	signal_channel = g_io_channel_unix_new(signal_sockets[1]);

	/*
	 * Set the channel encoding to raw binary instead of the default of
	 * UTF-8, because we'll be sending integers across instead of strings.
	 */
	error = NULL;
	signal_status = g_io_channel_set_encoding(signal_channel, NULL, &error);
	if (signal_status != G_IO_STATUS_NORMAL) {
		fprintf(stderr, "Failed to set the signal channel to raw "
				"binary: %s", error->message);
		exit(1);
	}
	signal_channel_watcher = g_io_add_watch(signal_channel, G_IO_IN, mainloop_sighandler, NULL);
	g_io_channel_unref(signal_channel);

	/* Let's not violate any PLA's!!!! */
	/* jseymour: whatever the fsck that means */
	/* Robot101: for some reason things like gdm like to block     *
	 * useful signals like SIGCHLD, so we unblock all the ones we  *
	 * declare a handler for. thanks JSeymour and Vann.            */
	if (sigemptyset(&sigset)) {
		snprintf(errmsg, sizeof(errmsg), "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(catch_sig_list[sig_indx], sighandler)) == SIG_ERR) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(ignore_sig_list[sig_indx], SIG_IGN)) == SIG_ERR) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		snprintf(errmsg, sizeof(errmsg), "Warning: couldn't unblock signals");
		perror(errmsg);
	}
#endif

	/* scan command-line options */
	opterr = 1;
	while ((opt = getopt_long(argc, argv,
#ifndef _WIN32
				  "c:dfhmnl::s:v",
#else
				  "c:dfhmnl::v",
#endif
				  long_options, NULL)) != -1) {
		switch (opt) {
		case 'c':	/* config dir */
			g_free(opt_config_dir_arg);
			opt_config_dir_arg = g_strdup(optarg);
			break;
		case 'd':	/* debug */
			debug_enabled = TRUE;
			break;
		case 'f':	/* force-online */
			opt_force_online = TRUE;
			break;
		case 'h':	/* help */
			opt_help = TRUE;
			break;
		case 'n':	/* no autologin */
			opt_nologin = TRUE;
			break;
		case 'l':	/* login, option username */
			opt_login = TRUE;
			g_free(opt_login_arg);
			if (optarg != NULL)
				opt_login_arg = g_strdup(optarg);
			break;
		case 's':	/* use existing session ID */
			g_free(opt_session_arg);
			opt_session_arg = g_strdup(optarg);
			break;
		case 'v':	/* version */
			opt_version = TRUE;
			break;
		case 'm':   /* do not ensure single instance. */
			opt_si = FALSE;
			break;
		case 'D':   /* --display */
		case 'S':   /* --sync */
			/* handled by gtk_init_check below */
			break;
		case '?':	/* show terse help */
		default:
			show_usage(argv[0], TRUE);
#ifdef HAVE_SIGNAL_H
			g_free(segfault_message);
#endif
			return 0;
			break;
		}
	}

	/* show help message */
	if (opt_help) {
		show_usage(argv[0], FALSE);
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		return 0;
	}
	/* show version message */
	if (opt_version) {
		printf("%s %s (libpurple %s)\n", PIDGIN_NAME, DISPLAY_VERSION,
		                                 purple_core_get_version());
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		return 0;
	}

	/* set a user-specified config directory */
	if (opt_config_dir_arg != NULL) {
		if (g_path_is_absolute(opt_config_dir_arg)) {
			purple_util_set_user_dir(opt_config_dir_arg);
		} else {
			/* Make an absolute (if not canonical) path */
			char *cwd = g_get_current_dir();
			char *path = g_build_path(G_DIR_SEPARATOR_S, cwd, opt_config_dir_arg, NULL);
			purple_util_set_user_dir(path);
			g_free(path);
			g_free(cwd);
		}
	}

	/*
	 * We're done piddling around with command line arguments.
	 * Fire up this baby.
	 */

	purple_debug_set_enabled(debug_enabled);

	search_path = g_build_filename(purple_user_dir(), "gtkrc-2.0", NULL);
	gtk_rc_add_default_file(search_path);
	g_free(search_path);

	gui_check = gtk_init_check(&argc, &argv);
	if (!gui_check) {
		char *display = gdk_get_display();

		printf("%s %s\n", PIDGIN_NAME, DISPLAY_VERSION);

		g_warning("cannot open display: %s", display ? display : "unset");
		g_free(display);
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif

		return 1;
	}

	g_set_application_name(PIDGIN_NAME);

#ifdef _WIN32
	winpidgin_init(hint);
#endif

	purple_core_set_ui_ops(pidgin_core_get_ui_ops());
	purple_eventloop_set_ui_ops(pidgin_eventloop_get_ui_ops());

	/*
	 * Set plugin search directories. Give priority to the plugins
	 * in user's home directory.
	 */
	search_path = g_build_filename(purple_user_dir(), "plugins", NULL);
	if (!g_stat(search_path, &st))
		g_mkdir(search_path, S_IRUSR | S_IWUSR | S_IXUSR);
	purple_plugins_add_search_path(search_path);
	g_free(search_path);
	purple_plugins_add_search_path(LIBDIR);

	if (!purple_core_init(PIDGIN_UI)) {
		fprintf(stderr,
				"Initialization of the libpurple core failed. Dumping core.\n"
				"Please report this!\n");
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		abort();
	}

	if (opt_si && !purple_core_ensure_single_instance()) {
#ifdef HAVE_DBUS
		DBusConnection *conn = purple_dbus_get_connection();
		DBusMessage *message = dbus_message_new_method_call(DBUS_SERVICE_PURPLE, DBUS_PATH_PURPLE,
				DBUS_INTERFACE_PURPLE, "PurpleBlistSetVisible");
		gboolean tr = TRUE;
		dbus_message_append_args(message, DBUS_TYPE_INT32, &tr, DBUS_TYPE_INVALID);
		dbus_connection_send_with_reply_and_block(conn, message, -1, NULL);
		dbus_message_unref(message);
#endif
		gdk_notify_startup_complete();
		purple_core_quit();
		g_printerr(_("Exiting because another libpurple client is already running.\n"));
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		return 0;
	}

	/* TODO: Move blist loading into purple_blist_init() */
	purple_set_blist(purple_blist_new());
	purple_blist_load();

	/* load plugins we had when we quit */
	purple_plugins_load_saved(PIDGIN_PREFS_ROOT "/plugins/loaded");

	/* TODO: Move pounces loading into purple_pounces_init() */
	purple_pounces_load();

	ui_main();

#ifdef USE_SM
	pidgin_session_init(argv[0], opt_session_arg, opt_config_dir_arg);
#endif
	if (opt_session_arg != NULL) {
		g_free(opt_session_arg);
		opt_session_arg = NULL;
	}
	if (opt_config_dir_arg != NULL) {
		g_free(opt_config_dir_arg);
		opt_config_dir_arg = NULL;
	}

	/* This needs to be before purple_blist_show() so the
	 * statusbox gets the forced online status. */
	if (opt_force_online)
		purple_network_force_online();

	/*
	 * We want to show the blist early in the init process so the
	 * user feels warm and fuzzy (not cold and prickley).
	 */
	purple_blist_show();

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"))
		pidgin_debug_window_show();

	if (opt_login) {
		/* disable all accounts */
		for (accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {
			PurpleAccount *account = accounts->data;
			purple_account_set_enabled(account, PIDGIN_UI, FALSE);
		}
		/* honor the startup status preference */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status"))
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		/* now enable the requested ones */
		dologin_named(opt_login_arg);
		if (opt_login_arg != NULL) {
			g_free(opt_login_arg);
			opt_login_arg = NULL;
		}
	} else if (opt_nologin)	{
		/* Set all accounts to "offline" */
		PurpleSavedStatus *saved_status;

		/* If we've used this type+message before, lookup the transient status */
		saved_status = purple_savedstatus_find_transient_by_type_and_message(
							PURPLE_STATUS_OFFLINE, NULL);

		/* If this type+message is unique then create a new transient saved status */
		if (saved_status == NULL)
			saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_OFFLINE);

		/* Set the status for each account */
		purple_savedstatus_activate(saved_status);
	} else {
		/* Everything is good to go--sign on already */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status"))
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		purple_accounts_restore_current_statuses();
	}

	if ((active_accounts = purple_accounts_get_all_active()) == NULL)
	{
		pidgin_accounts_window_show();
	}
	else
	{
		g_list_free(active_accounts);
	}

	/* GTK clears the notification for us when opening the first window,
	 * but we may have launched with only a status icon, so clear the it
	 * just in case. */
	gdk_notify_startup_complete();

#ifdef _WIN32
	winpidgin_post_init();
#endif

	gtk_main();

#ifdef HAVE_SIGNAL_H
	g_free(segfault_message);
	g_source_remove(signal_channel_watcher);
	close(signal_sockets[0]);
	close(signal_sockets[1]);
#endif

#ifdef _WIN32
	winpidgin_cleanup();
#endif

	return 0;
}
