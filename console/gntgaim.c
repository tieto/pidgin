/**
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
 */
#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "notify.h"
#include "prefix.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"

#include "gntdebug.h"
#include "gntgaim.h"
#include "gntprefs.h"
#include "gntui.h"

#define _GNU_SOURCE
#include <getopt.h>

#include "config.h"

static void
debug_init()
{
	gg_debug_init();
	gaim_debug_set_ui_ops(gg_debug_get_ui_ops());
}

static GaimCoreUiOps core_ops =
{
	gg_prefs_init,
	debug_init,
	gnt_ui_init,
	gnt_ui_uninit
};

static GaimCoreUiOps *
gnt_core_get_ui_ops()
{
	return &core_ops;
}

/* Anything IO-related is directly copied from gtkgaim's source tree */

#define GAIM_GNT_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_GNT_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _GaimGntIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;

} GaimGntIOClosure;

static void gaim_gnt_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_gnt_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimGntIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_GNT_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_GNT_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

#if 0
	gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: callback for %d, fd is %d\n",
			   closure->result, g_io_channel_unix_get_fd(source));
#endif

#ifdef _WIN32
	if(! gaim_cond) {
#if DEBUG
		gaim_debug_misc("gnt_eventloop",
			   "CLOSURE received GIOCondition of 0x%x, which does not"
			   " match 0x%x (READ) or 0x%x (WRITE)\n",
			   condition, GAIM_GNT_READ_COND, GAIM_GNT_WRITE_COND);
#endif /* DEBUG */

		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  gaim_cond);

	return TRUE;
}

static guint gnt_input_add(gint fd, GaimInputCondition condition, GaimInputFunction function,
							   gpointer data)
{
	GaimGntIOClosure *closure = g_new0(GaimGntIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_GNT_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_GNT_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_gnt_io_invoke, closure, gaim_gnt_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static GaimEventLoopUiOps eventloop_ops =
{
	g_timeout_add,
	g_source_remove,
	gnt_input_add,
	g_source_remove,
	NULL /* input_get_error */
};

static GaimEventLoopUiOps *
gnt_eventloop_get_ui_ops(void)
{
	return &eventloop_ops;
}

/* This is copied from gtkgaim */
static char *
gnt_find_binary_location(void *symbol, void *data)
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


/* This is mostly copied from gtkgaim's source tree */
static void
show_usage(const char *name, gboolean terse)
{
	char *text;

	if (terse) {
		text = g_strdup_printf(_("%s. Try `%s -h' for more information.\n"), VERSION, name);
	} else {
		text = g_strdup_printf(_("%s\n"
		       "Usage: %s [OPTION]...\n\n"
		       "  -c, --config=DIR    use DIR for config files\n"
		       "  -d, --debug         print debugging messages to stdout\n"
		       "  -h, --help          display this help and exit\n"
		       "  -n, --nologin       don't automatically login\n"
		       "  -v, --version       display the current version and exit\n"), VERSION, name);
	}

	gaim_print_utf8_to_console(stdout, text);
	g_free(text);
}

static int
init_libgaim(int argc, char **argv)
{
	char *path;
	int opt;
	gboolean opt_help = FALSE;
	gboolean opt_nologin = FALSE;
	gboolean opt_version = FALSE;
	char *opt_config_dir_arg = NULL;
	char *opt_session_arg = NULL;
	gboolean debug_enabled = FALSE;

	struct option long_options[] = {
		{"config",   required_argument, NULL, 'c'},
		{"debug",    no_argument,       NULL, 'd'},
		{"help",     no_argument,       NULL, 'h'},
		{"nologin",  no_argument,       NULL, 'n'},
		{"session",  required_argument, NULL, 's'},
		{"version",  no_argument,       NULL, 'v'},
		{0, 0, 0, 0}
	};

	gaim_br_set_locate_fallback_func(gnt_find_binary_location, argv[0]);

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

#ifdef HAVE_SETLOCALE
	setlocale(LC_ALL, "");
#endif

	/* scan command-line options */
	opterr = 1;
	while ((opt = getopt_long(argc, argv,
#ifndef _WIN32
				  "c:dhn::s:v",
#else
				  "c:dhn::v",
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
		printf("gaim-text %s\n", VERSION);
		return 0;
	}

	/* set a user-specified config directory */
	if (opt_config_dir_arg != NULL) {
		gaim_util_set_user_dir(opt_config_dir_arg);
		g_free(opt_config_dir_arg);
	}

	/*
	 * We're done piddling around with command line arguments.
	 * Fire up this baby.
	 */

	/* Because we don't want debug-messages to show up and corrup the display */
	gaim_debug_set_enabled(debug_enabled);

	gaim_core_set_ui_ops(gnt_core_get_ui_ops());
	gaim_eventloop_set_ui_ops(gnt_eventloop_get_ui_ops());

	path = g_build_filename(gaim_user_dir(), "plugins", NULL);
	gaim_plugins_add_search_path(path);
	g_free(path);

	gaim_plugins_add_search_path(LIBDIR);

	if (!gaim_core_init(GAIM_GNT_UI))
	{
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

	/* load plugins we had when we quit */
	gaim_plugins_load_saved("/gaim/gnt/plugins/loaded");

	/* TODO: Move pounces loading into gaim_pounces_init() */
	gaim_pounces_load();

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

	return 1;
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

	/* Initialize the libgaim stuff */
	if (!init_libgaim(argc, argv))
		return 0;

	gaim_blist_show();
	gnt_main();

#ifdef STANDALONE
	gaim_core_quit();
#endif

	return 0;
}

