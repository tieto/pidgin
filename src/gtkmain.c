/*
 * gaim
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
#include "gtkgaim.h"

#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
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
#include "gtkeventloop.h"
#include "gtkft.h"
#include "gtkidle.h"
#include "gtklog.h"
#include "gtknotify.h"
#include "gtkplugin.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkrequest.h"
#include "gtkroomlist.h"
#include "gtksavedstatuses.h"
#include "gtksession.h"
#include "gtksound.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "gtkstock.h"
#include "gtkwhiteboard.h"

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif

#include <getopt.h>

#ifdef HAVE_STARTUP_NOTIFICATION
# define SN_API_NOT_YET_FROZEN
# include <libsn/sn-launchee.h>
# include <gdk/gdkx.h>
#endif

#ifdef _WIN32
# include "wspell.h"
#endif



#ifdef HAVE_STARTUP_NOTIFICATION
static SnLauncheeContext *sn_context = NULL;
static SnDisplay *sn_display = NULL;
#endif

#ifdef HAVE_SIGNAL_H
/*
 * Lists of signals we wish to catch and those we wish to ignore.
 * Each list terminated with -1
 */
static int catch_sig_list[] = {
	SIGSEGV,
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	SIGCHLD,
	-1
};

static int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif

static int
dologin_named(const char *name)
{
	GaimAccount *account;
	char **names;
	int i;
	int ret = -1;

	if (name != NULL) { /* list of names given */
		names = g_strsplit(name, ",", 64);
		for (i = 0; names[i] != NULL; i++) {
			account = gaim_accounts_find(names[i], NULL);
			if (account != NULL) { /* found a user */
				ret = 0;
				gaim_account_connect(account);
			}
		}
		g_strfreev(names);
	} else { /* no name given, use the first account */
		GList *accounts;

		accounts = gaim_accounts_get_all();
		if (accounts != NULL)
		{
			account = (GaimAccount *)accounts->data;
			ret = 0;
			gaim_account_connect(account);
		}
	}

	return ret;
}

#ifdef HAVE_SIGNAL_H
static void
clean_pid(void)
{
	int status;
	pid_t pid;

	do {
		pid = waitpid(-1, &status, WNOHANG);
	} while (pid != 0 && pid != (pid_t)-1);

	if ((pid == (pid_t) - 1) && (errno != ECHILD)) {
		char errmsg[BUFSIZ];
		snprintf(errmsg, BUFSIZ, "Warning: waitpid() returned %d", pid);
		perror(errmsg);
	}
}

char *segfault_message;

static void
sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		gaim_debug_warning("sighandler", "Caught signal %d\n", sig);
		gaim_connections_disconnect_all();
		break;
	case SIGSEGV:
		fprintf(stderr, "%s", segfault_message);
		abort();
		break;
	case SIGCHLD:
		clean_pid();
		signal(SIGCHLD, sighandler);    /* restore signal catching on this one! */
		break;
	default:
		gaim_debug_warning("sighandler", "Caught signal %d\n", sig);
		gaim_connections_disconnect_all();

		gaim_plugins_unload_all();

		if (gtk_main_level())
			gtk_main_quit();
		exit(0);
	}
}
#endif

static int
ui_main()
{
#ifndef _WIN32
	GList *icons = NULL;
	GdkPixbuf *icon = NULL;
	char *icon_path;
#endif

	gaim_gtkthemes_init();

	gaim_gtk_blist_setup_sort_methods();

#ifndef _WIN32
	/* use the nice PNG icon for all the windows */
	icon_path = g_build_filename(DATADIR, "pixmaps", "gaim", "icons", "online.png", NULL);
	icon = gdk_pixbuf_new_from_file(icon_path, NULL);
	g_free(icon_path);
	if (icon) {
		icons = g_list_append(icons,icon);
		gtk_window_set_default_icon_list(icons);
		g_object_unref(G_OBJECT(icon));
		g_list_free(icons);
	} else {
		gaim_debug_error("ui_main",
						 "Failed to load the default window icon!\n");
	}
#endif

	return 0;
}

static void
debug_init(void)
{
	gaim_debug_set_ui_ops(gaim_gtk_debug_get_ui_ops());
	gaim_gtk_debug_init();
}

static void
gaim_gtk_ui_init(void)
{
	/* Set the UI operation structures. */
	gaim_accounts_set_ui_ops(gaim_gtk_accounts_get_ui_ops());
	gaim_xfers_set_ui_ops(gaim_gtk_xfers_get_ui_ops());
	gaim_blist_set_ui_ops(gaim_gtk_blist_get_ui_ops());
	gaim_notify_set_ui_ops(gaim_gtk_notify_get_ui_ops());
	gaim_privacy_set_ui_ops(gaim_gtk_privacy_get_ui_ops());
	gaim_request_set_ui_ops(gaim_gtk_request_get_ui_ops());
	gaim_sound_set_ui_ops(gaim_gtk_sound_get_ui_ops());
	gaim_connections_set_ui_ops(gaim_gtk_connections_get_ui_ops());
	gaim_whiteboard_set_ui_ops(gaim_gtk_whiteboard_get_ui_ops());
#ifdef USE_SCREENSAVER
	gaim_idle_set_ui_ops(gaim_gtk_idle_get_ui_ops());
#endif

	gaim_gtk_stock_init();
	gaim_gtk_account_init();
	gaim_gtk_connection_init();
	gaim_gtk_blist_init();
	gaim_gtk_status_init();
	gaim_gtk_conversations_init();
	gaim_gtk_pounces_init();
	gaim_gtk_privacy_init();
	gaim_gtk_xfers_init();
	gaim_gtk_roomlist_init();
	gaim_gtk_log_init();
}

static void
gaim_gtk_quit(void)
{
#ifdef USE_SM
	/* unplug */
	gaim_gtk_session_end();
#endif

	/* Save the plugins we have loaded for next time. */
	gaim_gtk_plugins_save();

	/* Uninit */
	gaim_gtk_conversations_uninit();
	gaim_gtk_status_uninit();
	gaim_gtk_blist_uninit();
	gaim_gtk_connection_uninit();
	gaim_gtk_account_uninit();
	gaim_gtk_xfers_uninit();
	gaim_gtk_debug_uninit();

	/* and end it all... */
	gtk_main_quit();
}

static GaimCoreUiOps core_ops =
{
	gaim_gtk_prefs_init,
	debug_init,
	gaim_gtk_ui_init,
	gaim_gtk_quit
};

static GaimCoreUiOps *
gaim_gtk_core_get_ui_ops(void)
{
	return &core_ops;
}

static void
show_usage(const char *name, gboolean terse)
{
	char *text;

	if (terse) {
		text = g_strdup_printf(_("Gaim %s. Try `%s -h' for more information.\n"), VERSION, name);
	} else {
		text = g_strdup_printf(_("Gaim %s\n"
		       "Usage: %s [OPTION]...\n\n"
		       "  -c, --config=DIR    use DIR for config files\n"
		       "  -d, --debug         print debugging messages to stdout\n"
		       "  -h, --help          display this help and exit\n"
		       "  -n, --nologin       don't automatically login\n"
		       "  -l, --login[=NAME]  automatically login (optional argument NAME specifies\n"
		       "                      account(s) to use, separated by commas)\n"
		       "  -v, --version       display the current version and exit\n"), VERSION, name);
	}

	gaim_print_utf8_to_console(stdout, text);
	g_free(text);
}

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push(SnDisplay *display, Display *xdisplay)
{
	gdk_error_trap_push();
}

static void
sn_error_trap_pop(SnDisplay *display, Display *xdisplay)
{
	gdk_error_trap_pop();
}

static void
startup_notification_complete(void)
{
	Display *xdisplay;

	xdisplay = GDK_DISPLAY();
	sn_display = sn_display_new(xdisplay,
								sn_error_trap_push,
								sn_error_trap_pop);
	sn_context =
		sn_launchee_context_new_from_environment(sn_display,
												 DefaultScreen(xdisplay));

	if (sn_context != NULL)
	{
		sn_launchee_context_complete(sn_context);
		sn_launchee_context_unref(sn_context);

		sn_display_unref(sn_display);
	}
}
#endif /* HAVE_STARTUP_NOTIFICATION */

#ifndef _WIN32
static char *gaim_find_binary_location(void *symbol, void *data)
{
	static char *fullname = NULL;
	static gboolean first = TRUE;

	char *argv0 = data;
	struct stat st;
	char *basebuf, *linkbuf, *fullbuf;

	if (!first)
		/* We've already been through this. */
		return strdup(fullname);

	first = FALSE;

	if (!argv0)
		return NULL;


	basebuf = g_find_program_in_path(argv0);

	/* But we still need to deal with symbolic links */
	g_lstat(basebuf, &st);
	while ((st.st_mode & S_IFLNK) == S_IFLNK) {
		int written;
		linkbuf = g_malloc(1024);
		written = readlink(basebuf, linkbuf, 1024 - 1);
		if (written == -1)
		{
			/* This really shouldn't happen, but do we
			 * need something better here? */
			g_free(linkbuf);
			continue;
		}
		linkbuf[written] = '\0';
		if (linkbuf[0] == G_DIR_SEPARATOR) {
			/* an absolute path */
			fullbuf = g_strdup(linkbuf);
		} else {
			char *dirbuf = g_path_get_dirname(basebuf);
			/* a relative path */
			fullbuf = g_strdup_printf("%s%s%s",
						  dirbuf, G_DIR_SEPARATOR_S,
						  linkbuf);
			g_free(dirbuf);
		}
		/* There's no memory leak here.  Really! */
		g_free(linkbuf);
		g_free(basebuf);
		basebuf = fullbuf;
		g_lstat(basebuf, &st);
	}

	fullname = basebuf;
	return strdup(fullname);
}
#endif /* #ifndef _WIN32 */

/* FUCKING GET ME A TOWEL! */
#ifdef _WIN32
int gaim_main(HINSTANCE hint, int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	gboolean opt_help = FALSE;
	gboolean opt_login = FALSE;
	gboolean opt_nologin = FALSE;
	gboolean opt_version = FALSE;
	char *opt_config_dir_arg = NULL;
	char *opt_login_arg = NULL;
	char *opt_session_arg = NULL;
	int dologin_ret = -1;
	char *search_path;
	GList *accounts;
#ifdef HAVE_SIGNAL_H
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	RETSIGTYPE (*prev_sig_disp)(int);
#endif
	int opt;
	gboolean gui_check;
	gboolean debug_enabled;
#ifdef HAVE_SIGNAL_H
	char errmsg[BUFSIZ];
#ifndef DEBUG
	char *segfault_message_tmp;
	GError *error = NULL;
#endif
#endif

	struct option long_options[] = {
		{"config",   required_argument, NULL, 'c'},
		{"debug",    no_argument,       NULL, 'd'},
		{"help",     no_argument,       NULL, 'h'},
		{"login",    optional_argument, NULL, 'l'},
		{"nologin",  no_argument,       NULL, 'n'},
		{"session",  required_argument, NULL, 's'},
		{"version",  no_argument,       NULL, 'v'},
		{0, 0, 0, 0}
	};

#ifdef DEBUG
	debug_enabled = TRUE;
#else
	debug_enabled = FALSE;
#endif

#ifdef GAIM_FATAL_ASSERTS
	/* Make g_return_... functions fatal. */
	g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
#endif

#ifndef _WIN32
	br_set_locate_fallback_func(gaim_find_binary_location, argv[0]);
#endif
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
			"Gaim has segfaulted and attempted to dump a core file.\n"
			"This is a bug in the software and has happened through\n"
			"no fault of your own.\n\n"
			"If you can reproduce the crash, please notify the gaim\n"
			"developers by reporting a bug at\n"
			"%sbug.php\n\n"
			"Please make sure to specify what you were doing at the time\n"
			"and post the backtrace from the core file.  If you do not know\n"
			"how to get the backtrace, please read the instructions at\n"
			"%sgdb.php\n\n"
			"If you need further assistance, please IM either SeanEgn or \n"
			"LSchiere (via AIM).  Contact information for Sean and Luke \n"
			"on other protocols is at\n"
			"%scontactinfo.php\n"),
			GAIM_WEBSITE, GAIM_WEBSITE, GAIM_WEBSITE
		);

		/* we have to convert the message (UTF-8 to console
		   charset) early because after a segmentation fault
		   it's not a good practice to allocate memory */
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

	/* Let's not violate any PLA's!!!! */
	/* jseymour: whatever the fsck that means */
	/* Robot101: for some reason things like gdm like to block     *
	 * useful signals like SIGCHLD, so we unblock all the ones we  *
	 * declare a handler for. thanks JSeymour and Vann.            */
	if (sigemptyset(&sigset)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(catch_sig_list[sig_indx], sighandler)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(ignore_sig_list[sig_indx], SIG_IGN)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't unblock signals");
		perror(errmsg);
	}
#endif

	/* scan command-line options */
	opterr = 1;
	while ((opt = getopt_long(argc, argv,
#ifndef _WIN32
				  "c:dhnl::s:v",
#else
				  "c:dhnl::v",
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
		case '?':	/* show terse help */
		default:
			show_usage(argv[0], TRUE);
			return 0;
			break;
		}
	}

	/* show help message */
	if (opt_help) {
		show_usage(argv[0], FALSE);
		return 0;
	}
	/* show version message */
	if (opt_version) {
		printf("Gaim %s\n", VERSION);
		return 0;
	}

	/* set a user-specified config directory */
	if (opt_config_dir_arg != NULL) {
		gaim_util_set_user_dir(opt_config_dir_arg);
	}

	/*
	 * We're done piddling around with command line arguments.
	 * Fire up this baby.
	 */

	gaim_debug_set_enabled(debug_enabled);

#ifdef _WIN32
	wgaim_init(hint);
#endif

	search_path = g_build_filename(gaim_user_dir(), "gtkrc-2.0", NULL);
	gtk_rc_add_default_file(search_path);
	g_free(search_path);

#if (defined(G_THREADS_ENABLED) && !defined(G_THREADS_IMPL_NONE))
	/* Since threads can be yanked in all unawares by other libraries,
	 * and some libraries aren't smart enough to initialize the thread
	 * subsystem when they need it, we need to do this here.  We also
	 * threadify dbus when that gets initialized.  Ugh. */
	if (!g_thread_supported())
		g_thread_init(NULL);
#ifndef _WIN32
	gdk_threads_init();
#endif
#endif /* Glib has threads */

	gui_check = gtk_init_check(&argc, &argv);
	if (!gui_check) {
		char *display = gdk_get_display();

		printf("Gaim %s\n", VERSION);

		g_warning("cannot open display: %s", display ? display : "unset");
		g_free(display);

		return 1;
	}

#ifdef _WIN32
	/** TODO: Move this to a wgaim_gtk_init() if we need such a thing */
	wgaim_gtkspell_init();
	gaim_debug_info("wgaim", "GTK+ :%u.%u.%u\n",
		gtk_major_version, gtk_minor_version, gtk_micro_version);

#endif

	gaim_core_set_ui_ops(gaim_gtk_core_get_ui_ops());
	gaim_eventloop_set_ui_ops(gaim_gtk_eventloop_get_ui_ops());

	/*
	 * Set plugin search directories. Give priority to the plugins
	 * in user's home directory.
	 */
	search_path = g_build_filename(gaim_user_dir(), "plugins", NULL);
	gaim_plugins_add_search_path(search_path);
	g_free(search_path);
	gaim_plugins_add_search_path(LIBDIR);

	if (!gaim_core_init(GAIM_GTK_UI)) {
		fprintf(stderr,
				"Initialization of the Gaim core failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}

	/* TODO: Move blist loading into gaim_blist_init() */
	gaim_set_blist(gaim_blist_new());
	gaim_blist_load();

	/* TODO: Move prefs loading into gaim_prefs_init() */
	gaim_prefs_load();
	gaim_prefs_update_old();
	gaim_gtk_prefs_update_old();

	/* load plugins we had when we quit */
	gaim_plugins_load_saved("/gaim/gtk/plugins/loaded");

	/* TODO: Move pounces loading into gaim_pounces_init() */
	gaim_pounces_load();

	ui_main();

#ifdef USE_SM
	gaim_gtk_session_init(argv[0], opt_session_arg, opt_config_dir_arg);
#endif
	if (opt_session_arg != NULL) {
		g_free(opt_session_arg);
		opt_session_arg = NULL;
	}
	if (opt_config_dir_arg != NULL) {
		g_free(opt_config_dir_arg);
		opt_config_dir_arg = NULL;
	}

	/*
	 * We want to show the blist early in the init process so the
	 * user feels warm and fuzzy (not cold and prickley).
	 */
	gaim_blist_show();

	if (gaim_prefs_get_bool("/gaim/gtk/debug/enabled"))
		gaim_gtk_debug_window_show();

	if (opt_login) {
		dologin_ret = dologin_named(opt_login_arg);
		if (opt_login_arg != NULL) {
			g_free(opt_login_arg);
			opt_login_arg = NULL;
		}
	}

	if (opt_nologin)
	{
		/* Set all accounts to "offline" */
		GaimSavedStatus *saved_status;

		/* If we've used this type+message before, lookup the transient status */
		saved_status = gaim_savedstatus_find_transient_by_type_and_message(
							GAIM_STATUS_OFFLINE, NULL);

		/* If this type+message is unique then create a new transient saved status */
		if (saved_status == NULL)
			saved_status = gaim_savedstatus_new(NULL, GAIM_STATUS_OFFLINE);

		/* Set the status for each account */
		gaim_savedstatus_activate(saved_status);
	}
	else
	{
		/* Everything is good to go--sign on already */
		if (!gaim_prefs_get_bool("/core/savedstatus/startup_current_status"))
			gaim_savedstatus_activate(gaim_savedstatus_get_startup());
		gaim_accounts_restore_current_statuses();
	}

	if ((accounts = gaim_accounts_get_all_active()) == NULL)
	{
		gaim_gtk_accounts_window_show();
	}
	else
	{
		g_list_free(accounts);
	}

#ifdef HAVE_STARTUP_NOTIFICATION
	startup_notification_complete();
#endif

	gtk_main();

#ifdef HAVE_SIGNAL_H
	g_free(segfault_message);
#endif

#ifdef _WIN32
	wgaim_cleanup();
#endif

	return 0;
}
