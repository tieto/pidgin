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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef USE_APPLET
#include "applet.h"
#include <gnome.h>
#else
#ifdef USE_GNOME
#include <gnome.h>
#endif /* USE_GNOME */
#endif /* USE_APPLET */
#ifdef GAIM_PLUGINS
#include <dlfcn.h>
#endif /* GAIM_PLUGINS */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "prpl.h"
#include "gaim.h"
#include "pixmaps/logo.xpm"
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#include "locale.h"
#include "gtkticker.h"
#include "gtkspell.h"
#ifndef USE_APPLET
#include <getopt.h>
#endif

static gchar *ispell_cmd[] = { "ispell", "-a", NULL };

static GtkWidget *name;
static GtkWidget *pass;

GList *log_conversations = NULL;
GList *buddy_pounces = NULL;
GSList *away_messages = NULL;
GList *conversations = NULL;
GList *chat_rooms = NULL;
GSList *message_queue = NULL;
GSList *away_time_queue = NULL;

GtkWidget *mainwindow = NULL;

int opt_away = 0;
char *opt_away_arg = NULL;
char *opt_rcfile_arg = NULL;

void BuddyTickerCreateWindow(void);

void cancel_logon(void)
{
#ifdef USE_APPLET
	applet_buddy_show = FALSE;
	if (mainwindow)
		gtk_widget_hide(mainwindow);
#else
#ifdef GAIM_PLUGINS
	/* first we tell those who have requested it we're quitting */
	plugin_event(event_quit, 0, 0, 0, 0);

	/* then we remove everyone in a mass suicide */
	remove_all_plugins();
#endif /* GAIM_PLUGINS */
#ifdef USE_PERL
	perl_end();
#endif

	gtk_main_quit();
#endif /* USE_APPLET */
}

static int snd_tmout;
int logins_not_muted = 1;
static void sound_timeout()
{
	logins_not_muted = 1;
	gtk_timeout_remove(snd_tmout);
}

/* we need to do this for Oscar because serv_login only starts the login
 * process, it doesn't end there. gaim_setup will be called later from
 * oscar.c, after the buddy list is made and serv_finish_login is called */
void gaim_setup(struct gaim_connection *gc)
{
	if ((sound_options & OPT_SOUND_LOGIN) && (sound_options & OPT_SOUND_SILENT_SIGNON)) {
		logins_not_muted = 0;
		snd_tmout = gtk_timeout_add(10000, (GtkFunction)sound_timeout, NULL);
	}
#ifdef USE_APPLET
	set_user_state(online);
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"signoff", _("Signoff"), (AppletCallbackFunc)signoff_all, NULL);
#endif /* USE_APPLET */
}


static void dologin(GtkWidget *widget, GtkWidget *w)
{
	struct aim_user *u;
	const char *username = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(name)->entry));
	const char *password = gtk_entry_get_text(GTK_ENTRY(pass));

	if (!strlen(username)) {
		do_error_dialog(_("Please enter your logon"), _("Signon Error"));
		return;
	}

	if (!strlen(password)) {
		do_error_dialog(_("Please enter your password"), _("Signon Error"));
		return;
	}

	/* if there is more than one user of the same name, then fuck them, they just have
	 * to use the account editor to sign in the second one */
	u = find_user(username, -1);
	if (!u)
		u = new_user(username, DEFAULT_PROTO, OPT_USR_REM_PASS);
	g_snprintf(u->password, sizeof u->password, "%s", password);
	save_prefs();
	serv_login(u);
}


static void doenter(GtkWidget *widget, GtkWidget *w)
{
	if (widget == name) {
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_entry_select_region(GTK_ENTRY(GTK_COMBO(name)->entry), 0, 0);
		gtk_widget_grab_focus(pass);
	} else if (widget == pass) {
		dologin(widget, w);
	}
}


static void combo_changed(GtkWidget *w, GtkWidget *combo)
{
	const char *txt = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
	struct aim_user *u;

	u = find_user(txt, -1);

	if (u && u->options & OPT_USR_REM_PASS) {
		gtk_entry_set_text(GTK_ENTRY(pass), u->password);
	} else {
		gtk_entry_set_text(GTK_ENTRY(pass), "");
	}
}


static GList *combo_user_names()
{
	GList *usr = aim_users;
	GList *tmp = NULL;
	struct aim_user *u;

	if (!usr)
		return g_list_append(NULL, "<New User>");

	while (usr) {
		u = (struct aim_user *)usr->data;
		tmp = g_list_append(tmp, g_strdup(u->username));
		usr = usr->next;
	}

	return tmp;
}


void show_login()
{
	GtkWidget *options;
#ifdef GAIM_PLUGINS
	GtkWidget *plugs;
#endif
#ifndef NO_MULTI
	GtkWidget *accts;
#endif
	GtkWidget *signon;
	GtkWidget *cancel;
	GtkWidget *reg;
	GtkWidget *bbox;
	GtkWidget *hbox;
	GtkWidget *sbox;
	GtkWidget *label;
	GtkWidget *table;

	GtkWidget *pmw;
	GdkPixmap *pm;
	GtkStyle *style;
	GdkBitmap *mask;

	if (mainwindow) {
		gtk_widget_show(mainwindow);
		return;
	}

	mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(mainwindow), "login", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(mainwindow), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(mainwindow), "delete_event",
			   GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
	gtk_window_set_title(GTK_WINDOW(mainwindow), _("Gaim - Login"));
	gtk_widget_realize(mainwindow);
	aol_icon(mainwindow->window);
	gdk_window_set_group(mainwindow->window, mainwindow->window);

	table = gtk_table_new(8, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(mainwindow), table);
	gtk_widget_show(table);

	style = gtk_widget_get_style(mainwindow);
	pm = gdk_pixmap_create_from_xpm_d(mainwindow->window, &mask,
					  &style->bg[GTK_STATE_NORMAL], (gchar **) aol_logo);
	pmw = gtk_pixmap_new(pm, mask);
	gtk_table_attach(GTK_TABLE(table), pmw, 0, 2, 0, 1, 0, 0, 5, 5);
	gtk_widget_show(pmw);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);

	label = gtk_label_new(_("Screen Name: "));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, 0, 0, 5, 5);
	gtk_widget_show(label);

	name = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(name), combo_user_names());
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "activate",
			   GTK_SIGNAL_FUNC(doenter), mainwindow);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(name)->entry), "changed",
			   GTK_SIGNAL_FUNC(combo_changed), name);
	gtk_widget_set_usize(name, 100, 0);
	gtk_table_attach(GTK_TABLE(table), name, 1, 2, 2, 3, 0, 0, 5, 5);
	gtk_widget_show(name);

	label = gtk_label_new(_("Password: "));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, 0, 0, 5, 5);
	gtk_widget_show(label);

	pass = gtk_entry_new();
	gtk_widget_set_usize(pass, 100, 0);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_signal_connect(GTK_OBJECT(pass), "activate", GTK_SIGNAL_FUNC(doenter), mainwindow);
	gtk_table_attach(GTK_TABLE(table), pass, 1, 2, 3, 4, 0, 0, 5, 5);
	gtk_widget_show(pass);

	sbox = gtk_vbox_new(TRUE, 5);
	gtk_container_border_width(GTK_CONTAINER(sbox), 10);
	gtk_table_attach(GTK_TABLE(table), sbox, 0, 2, 7, 8, 0, 0, 5, 5);
	gtk_widget_show(sbox);

	bbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(sbox), bbox, TRUE, TRUE, 0);
	gtk_widget_show(bbox);

#ifndef USE_APPLET
	cancel = gtk_button_new_with_label(_("Quit"));
#else
	cancel = gtk_button_new_with_label(_("Close"));
#endif
#ifndef NO_MULTI
	accts = gtk_button_new_with_label(_("Accounts"));
#endif
	signon = gtk_button_new_with_label(_("Signon"));

	if (display_options & OPT_DISP_COOL_LOOK) {
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
#ifndef NO_MULTI
		gtk_button_set_relief(GTK_BUTTON(accts), GTK_RELIEF_NONE);
#endif
		gtk_button_set_relief(GTK_BUTTON(signon), GTK_RELIEF_NONE);
	}

	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(cancel_logon), mainwindow);
#ifndef NO_MULTI
	gtk_signal_connect(GTK_OBJECT(accts), "clicked", GTK_SIGNAL_FUNC(account_editor), mainwindow);
#endif
	gtk_signal_connect(GTK_OBJECT(signon), "clicked", GTK_SIGNAL_FUNC(dologin), mainwindow);

	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
#ifndef NO_MULTI
	gtk_box_pack_start(GTK_BOX(bbox), accts, TRUE, TRUE, 0);
#endif
	gtk_box_pack_start(GTK_BOX(bbox), signon, TRUE, TRUE, 0);

	gtk_widget_show(cancel);
#ifndef NO_MULTI
	gtk_widget_show(accts);
#endif
	gtk_widget_show(signon);

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(sbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	reg = gtk_button_new_with_label(_("Register"));
	options = gtk_button_new_with_label(_("Options"));
#ifdef GAIM_PLUGINS
	plugs = gtk_button_new_with_label(_("Plugins"));
#endif
	if (display_options & OPT_DISP_COOL_LOOK) {
		gtk_button_set_relief(GTK_BUTTON(reg), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(options), GTK_RELIEF_NONE);
#ifdef GAIM_PLUGINS
		gtk_button_set_relief(GTK_BUTTON(plugs), GTK_RELIEF_NONE);
#endif
	}

	gtk_signal_connect(GTK_OBJECT(reg), "clicked", GTK_SIGNAL_FUNC(register_user), NULL);
	gtk_signal_connect(GTK_OBJECT(options), "clicked", GTK_SIGNAL_FUNC(show_prefs), NULL);
#ifdef GAIM_PLUGINS
	gtk_signal_connect(GTK_OBJECT(plugs), "clicked", GTK_SIGNAL_FUNC(show_plugins), NULL);
#endif

	gtk_box_pack_start(GTK_BOX(hbox), reg, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), options, TRUE, TRUE, 0);
#ifdef GAIM_PLUGINS
	gtk_box_pack_start(GTK_BOX(hbox), plugs, TRUE, TRUE, 0);
#endif

	gtk_widget_show(reg);
	gtk_widget_show(options);
#ifdef GAIM_PLUGINS
	gtk_widget_show(plugs);
#endif

	if (aim_users) {
		struct aim_user *c = (struct aim_user *)aim_users->data;
		if (c->options & OPT_USR_REM_PASS) {
			combo_changed(NULL, name);
			gtk_widget_grab_focus(signon);
		} else {
			gtk_widget_grab_focus(pass);
		}
	} else {
		gtk_widget_grab_focus(name);
	}

	gtk_widget_show(mainwindow);
}

extern void show_debug(GtkObject *);

#if HAVE_SIGNAL_H
void sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		debug_printf("caught signal %d\n", sig);
		signoff_all(NULL, NULL);
		break;
	case SIGSEGV:
		fprintf(stderr, "Gaim has segfaulted and attempted to dump a core file.\n"
				"Please notify the gaim maintainers by reporting a bug at\n"
				WEBSITE "bug.php3\n\n"
				"Please make sure to specify what you were doing at the time,\n"
				"and post the backtrace from the core file. If you do not know\n"
				"how to get the backtrace, please get instructions at\n"
				WEBSITE "gdb.shtml. If you need further\n"
				"assistance, please IM either EWarmenhoven or RobFlynn and\n"
				"they can help you.\n");
		abort();
		break;
	default:
		debug_printf("caught signal %d\n", sig);
		gtkspell_stop();
#ifdef GAIM_PLUGINS
		remove_all_plugins();
#endif
		if (gtk_main_level())
			gtk_main_quit();
		exit(0);
	}
}
#endif


int main(int argc, char *argv[])
{
	int opt;
	int opt_acct = 0, opt_help = 0, opt_version = 0, opt_user = 0, opt_login = 0, do_login_ret = -1;
	char *opt_user_arg = NULL, *opt_login_arg = NULL;
	int i;

#ifdef USE_GNOME
	struct poptOption popt_options[] = {
		{"acct", 'a', POPT_ARG_NONE, &opt_acct, 'a',
		 "Display account editor window", NULL},
		{"away", 'w', POPT_ARG_STRING, NULL, 'w',
		 "Make away on signon (optional argument MESG specifies name of away message to use)", "[MESG]"},
		{"login", 'l', POPT_ARG_STRING, NULL, 'l',
		 "Automatically login (optional argument NAME specifies account(s) to use)", "[NAME]"},
		{"user", 'u', POPT_ARG_STRING, &opt_user_arg, 'u',
		 "Use account NAME", "NAME"},
		{"file", 'f', POPT_ARG_STRING, &opt_rcfile_arg, 'f',
		 "Use FILE as config", "FILE"},
		{0, 0, 0, 0, 0, 0, 0}
	};
#endif /* USE_GNOME */
	struct option long_options[] = {
		{"acct", no_argument, NULL, 'a'},
		/*{"away", optional_argument, NULL, 'w'},*/
		{"help", no_argument, NULL, 'h'},
		/*{"login", optional_argument, NULL, 'l'},*/
		{"user", required_argument, NULL, 'u'},
		{"file", required_argument, NULL, 'f'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};


#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

#if HAVE_SIGNAL_H
	/* Let's not violate any PLA's!!!! */
#ifndef DEBUG
	signal(SIGSEGV, sighandler);
#endif
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGPIPE, SIG_IGN);
#endif


#ifdef USE_APPLET
	init_applet_mgr(argc, argv);
#else
	for (i = 0; i < argc; i++) {
		/* --login option */
		if (strstr(argv[i], "--l") == argv[i]) {
			char *equals;
			opt_login = 1;
			if ((equals = strchr(argv[i], '=')) != NULL) {
				/* --login=NAME */
				opt_login_arg = g_strdup(equals + 1);
				if (strlen (opt_login_arg) == 0) {
					g_free (opt_login_arg);
					opt_login_arg = NULL;
				}
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* --login NAME */
				opt_login_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
		/* -l option */
		else if (strstr(argv[i], "-l") == argv[i]) {
			opt_login = 1;
			if (strlen(argv[i]) > 2) {
				/* -lNAME */
				opt_login_arg = g_strdup(argv[i] + 2);
			} else if (i + 1 < argc && argv[i + 1][0] != '-') {
				/* -l NAME */
				opt_login_arg = g_strdup(argv[i + 1]);
				strcpy(argv[i + 1], " ");
			}
			strcpy(argv[i], " ");
		}
		/* --away option */
		else if (strstr (argv[i], "--aw") == argv[i]) {
			char *equals;
			opt_away = 1;
			if ((equals = strchr(argv[i], '=')) != NULL) {
				/* --away=MESG */
				opt_away_arg = g_strdup (equals+1);
				if (strlen (opt_away_arg) == 0) {
					g_free (opt_away_arg);
					opt_away_arg = NULL;
				}
			} else if (i+1 < argc && argv[i+1][0] != '-') {
				/* --away MESG */
				opt_away_arg = g_strdup (argv[i+1]);
				strcpy (argv[i+1], " ");
			}
			strcpy (argv[i], " ");
		}
		/* -w option */
		else if (strstr (argv[i], "-w") == argv[i]) {
			opt_away = 1;
			if (strlen (argv[i]) > 2) {
				/* -wMESG */
				opt_away_arg = g_strdup (argv[i]+2);
			} else if (i+1 < argc && argv[i+1][0] != '-') {
				/* -w MESG */
				opt_away_arg = g_strdup (argv[i+1]);
				strcpy (argv[i+1], " ");
			}
			strcpy(argv[i], " ");
		}
	}
	/*
	if (opt_login) {
		printf ("--login given with arg %s\n",
			opt_login_arg ? opt_login_arg : "NULL");
		exit(0);
	}
	*/

	gtk_set_locale();
#ifdef USE_GNOME
	gnome_init_with_popt_table(PACKAGE, VERSION, argc, argv, popt_options, 0, NULL);
#else
	gtk_init(&argc, &argv);
#endif

	/* scan command-line options */
#ifdef USE_GNOME
	opterr = 0;
#else
	opterr = 1;
#endif
	while ((opt = getopt_long(argc, argv, "ahu:f:v",
				  long_options, NULL)) != -1) {
		switch (opt) {
		case 'u':	/* set user */
			opt_user = 1;
			opt_user_arg = g_strdup(optarg);
			break;
		case 'a':	/* account editor */
			opt_acct = 1;
			break;
		case 'f':
			opt_rcfile_arg = g_strdup (optarg);
			break;
		case 'v':	/* version */
			opt_version = 1;
			break;
		case 'h':	/* help */
			opt_help = 1;
			break;
#ifndef USE_GNOME
		case '?':
		default:
			show_usage(1, argv[0]);
			return 0;
			break;
#endif
		}
	}

#endif /* USE_APPLET */

	/* show help message */
	if (opt_help) {
		show_usage(0, argv[0]);
		return 0;
	}
	/* show version window */
	if (opt_version) {
		gtk_init(&argc, &argv);
		set_defaults(FALSE);	/* needed for open_url_nw */
		load_prefs();
		show_about(0, (void *)2);
		gtk_main();
		return 0;
	}


	set_defaults(FALSE);
	load_prefs();

	/* set the default username */
	if (opt_user_arg != NULL) {
		set_first_user(opt_user_arg);
#ifndef USE_GNOME
		g_free(opt_user_arg);
		opt_user_arg = NULL;
#endif /* USE_GNOME */
	}

	if (general_options & OPT_GEN_DEBUG)
		show_debug(NULL);

	gtkspell_start(NULL, ispell_cmd);
#ifdef USE_PERL
	perl_init();
	perl_autoload();
#endif
	static_proto_init();

	/* deal with --login */
	if (opt_login) {
		do_login_ret = do_auto_login(opt_login_arg);
		if (opt_login_arg != NULL) {
			g_free(opt_login_arg);
			opt_login_arg = NULL;
		}
	}

#ifdef USE_APPLET
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"prefs", _("Preferences"), show_prefs, NULL);
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"accounts",
					_("Accounts"), (AppletCallbackFunc)account_editor, (void *)1);
#ifdef GAIM_PLUGINS
	applet_widget_register_callback(APPLET_WIDGET(applet),
					"plugins", _("Plugins"), GTK_SIGNAL_FUNC(show_plugins), NULL);
#endif /* GAIM_PLUGINS */

	update_pixmaps();

	if (!opt_acct)
		auto_login();

	applet_widget_gtk_main();
#else

	if (!opt_acct)
		auto_login();

	if (opt_acct) {
		account_editor(NULL, NULL);
	} else if ((do_login_ret == -1) && !connections)
		show_login();

	gtk_main();

#endif /* USE_APPLET */

	gtkspell_stop();

	return 0;

}
