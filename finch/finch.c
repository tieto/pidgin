/**
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 */
#include <internal.h>
#include "finch.h"

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
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"

#include "gntdebug.h"
#include "gntprefs.h"
#include "gntui.h"
#include "gntidle.h"

#define _GNU_SOURCE
#include <getopt.h>

#include "config.h"
#include "package_revision.h"

static void
debug_init(void)
{
	finch_debug_init();
	purple_debug_set_ui_ops(finch_debug_get_ui_ops());
}

static GHashTable *ui_info = NULL;
static GHashTable *finch_ui_get_info(void)
{
	if (ui_info == NULL) {
		ui_info = g_hash_table_new(g_str_hash, g_str_equal);

		g_hash_table_insert(ui_info, "name", (char*)_("Finch"));
		g_hash_table_insert(ui_info, "version", VERSION);
		g_hash_table_insert(ui_info, "website", "https://pidgin.im");
		g_hash_table_insert(ui_info, "dev_website", "https://developer.pidgin.im");
		g_hash_table_insert(ui_info, "client_type", "console");

		/*
		 * This is the client key for "Finch."  It is owned by the AIM
		 * account "markdoliner."  Please don't use this key for other
		 * applications.  You can either not specify a client key, in
		 * which case the default "libpurple" key will be used, or you
		 * can try to register your own at the AIM or ICQ web sites
		 * (although this functionality was removed at some point, it's
		 * possible it has been re-added).  AOL's old key management
		 * page is http://developer.aim.com/manageKeys.jsp
		 */
		g_hash_table_insert(ui_info, "prpl-aim-clientkey", "ma19sqWV9ymU6UYc");

		/*
		 * This is the client key for "Pidgin."  It is owned by the AIM
		 * account "markdoliner."  Please don't use this key for other
		 * applications.  You can either not specify a client key, in
		 * which case the default "libpurple" key will be used, or you
		 * can try to register your own at the AIM or ICQ web sites
		 * (although this functionality was removed at some point, it's
		 * possible it has been re-added).  AOL's old key management
		 * page is http://developer.aim.com/manageKeys.jsp
		 *
		 * We used to have a Finch-specific devId/clientkey
		 * (ma19sqWV9ymU6UYc), but it stopped working, so we switched
		 * to this one.
		 */
		g_hash_table_insert(ui_info, "prpl-icq-clientkey", "ma1cSASNCKFtrdv9");

		/*
		 * This is the distid for Finch, given to us by AOL.  Please
		 * don't use this for other applications.  You can just not
		 * specify a distid and libpurple will use a default.
		 */
		g_hash_table_insert(ui_info, "prpl-aim-distid", GINT_TO_POINTER(1552));
		g_hash_table_insert(ui_info, "prpl-icq-distid", GINT_TO_POINTER(1552));
	}

	return ui_info;
}

static void
finch_quit(void)
{
	gnt_ui_uninit();
	if (ui_info)
		g_hash_table_destroy(ui_info);
}

static PurpleCoreUiOps core_ops =
{
	finch_prefs_init,
	debug_init,
	gnt_ui_init,
	finch_quit,
	finch_ui_get_info,

	/* padding */
	NULL,
	NULL,
	NULL
};

static PurpleCoreUiOps *
gnt_core_get_ui_ops(void)
{
	return &core_ops;
}

/* Anything IO-related is directly copied from gtkpurple's source tree */

#define FINCH_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define FINCH_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleGntIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;

} PurpleGntIOClosure;

static void purple_gnt_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean purple_gnt_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGntIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & FINCH_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & FINCH_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

#if 0
	purple_debug(PURPLE_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: callback for %d, fd is %d\n",
			   closure->result, g_io_channel_unix_get_fd(source));
#endif

#ifdef _WIN32
	if(! purple_cond) {
#if DEBUG
		purple_debug_misc("gnt_eventloop",
			   "CLOSURE received GIOCondition of 0x%x, which does not"
			   " match 0x%x (READ) or 0x%x (WRITE)\n",
			   condition, FINCH_READ_COND, FINCH_WRITE_COND);
#endif /* DEBUG */

		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

static guint gnt_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							   gpointer data)
{
	PurpleGntIOClosure *closure = g_new0(PurpleGntIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= FINCH_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= FINCH_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      purple_gnt_io_invoke, closure, purple_gnt_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps eventloop_ops =
{
	g_timeout_add,
	g_source_remove,
	gnt_input_add,
	g_source_remove,
	NULL, /* input_get_error */
	g_timeout_add_seconds,

	/* padding */
	NULL,
	NULL,
	NULL
};

static PurpleEventLoopUiOps *
gnt_eventloop_get_ui_ops(void)
{
	return &eventloop_ops;
}

/* This is mostly copied from gtkpurple's source tree */
static void
show_usage(const char *name, gboolean terse)
{
	char *text;

	if (terse) {
		text = g_strdup_printf(_("%s. Try `%s -h' for more information.\n"), DISPLAY_VERSION, name);
	} else {
		text = g_strdup_printf(_("%s\n"
		       "Usage: %s [OPTION]...\n\n"
		       "  -c, --config=DIR    use DIR for config files\n"
		       "  -d, --debug         print debugging messages to stderr\n"
		       "  -h, --help          display this help and exit\n"
		       "  -n, --nologin       don't automatically login\n"
		       "  -v, --version       display the current version and exit\n"), DISPLAY_VERSION, name);
	}

	purple_print_utf8_to_console(stdout, text);
	g_free(text);
}

static int
init_libpurple(int argc, char **argv)
{
	char *path;
	int opt;
	gboolean opt_help = FALSE;
	gboolean opt_nologin = FALSE;
	gboolean opt_version = FALSE;
	char *opt_config_dir_arg = NULL;
	gboolean debug_enabled = FALSE;
	GStatBuf st;

	struct option long_options[] = {
		{"config",   required_argument, NULL, 'c'},
		{"debug",    no_argument,       NULL, 'd'},
		{"help",     no_argument,       NULL, 'h'},
		{"nologin",  no_argument,       NULL, 'n'},
		{"version",  no_argument,       NULL, 'v'},
		{0, 0, 0, 0}
	};

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
				  "c:dhn::v",
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
		/* Translators may want to transliterate the name.
		 It is not to be translated. */
		printf("%s %s (%s)\n", _("Finch"), DISPLAY_VERSION, REVISION);
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

		g_free(opt_config_dir_arg);
	}

	/*
	 * We're done piddling around with command line arguments.
	 * Fire up this baby.
	 */

	/* We don't want debug-messages to show up and corrupt the display */
	purple_debug_set_enabled(debug_enabled);

	purple_core_set_ui_ops(gnt_core_get_ui_ops());
	purple_eventloop_set_ui_ops(gnt_eventloop_get_ui_ops());
	purple_idle_set_ui_ops(finch_idle_get_ui_ops());

	path = g_build_filename(purple_user_dir(), "plugins", NULL);
	if (!g_stat(path, &st))
		g_mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
	purple_plugins_add_search_path(path);
	g_free(path);

	purple_plugins_add_search_path(LIBDIR);

	if (!purple_core_init(FINCH_UI))
	{
		fprintf(stderr,
				"Initialization of the Purple core failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}

	/* TODO: should this be moved into finch_prefs_init() ? */
	finch_prefs_update_old();

	/* load plugins we had when we quit */
	purple_plugins_load_saved("/finch/plugins/loaded");

	if (opt_nologin)
	{
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
	}
	else
	{
		/* Everything is good to go--sign on already */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status"))
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		purple_accounts_restore_current_statuses();
	}

	return 1;
}

static gboolean gnt_start(int *argc, char ***argv)
{
	/* Initialize the libpurple stuff */
	if (!init_libpurple(*argc, *argv))
		return FALSE;

	purple_blist_show();
	return TRUE;
}

int main(int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN);

#if !GLIB_CHECK_VERSION(2, 32, 0)
	/* GLib threading system is automaticaly initialized since 2.32.
	 * For earlier versions, it have to be initialized before calling any
	 * Glib or GTK+ functions.
	 */
	g_thread_init(NULL);
#endif

	g_set_prgname("Finch");
	g_set_application_name(_("Finch"));

	if (gnt_start(&argc, &argv)) {
		gnt_main();

#ifdef STANDALONE
		purple_core_quit();
#endif
	}

	return 0;
}

